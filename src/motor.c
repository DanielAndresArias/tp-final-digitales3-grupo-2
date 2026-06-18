/*
 * motor.c
 * Generador de pasos con rampa trapezoidal para NEMA17 + A4988.
 *
 * Rampa 100% ENTERA (sin float en la ISR): la velocidad se lleva en punto fijo
 * Q8 (vel_fp = pasos/s * 256). Asi el incremento por paso ACCEL/vel, que en
 * enteros "puros" se haria 0 cuando vel > ACCEL, queda representable:
 *
 *     vel_fp += (ACCEL * 256 * 256) / vel_fp
 *
 * Se mantiene un contador de posicion absoluta (pasos, con signo) que la ISR
 * actualiza en cada paso, y un modo de "jog" a velocidad constante para homing.
 *
 * Las conversiones mm<->pasos de la API publica tambien son enteras: se trabaja
 * en centesimas de milimetro (cmm = mm*100), asi el firmware no usa float en
 * ningun lado.
 */

#include "motor.h"
#include "limit_switches.h"

#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_systick.h"

/* ---------- Conexiones del A4988 ---------- */
#define STEP_PORT   PORT_3
#define STEP_PIN    PIN_26          /* pin STEP */
#define DIR_PORT    PORT_0
#define DIR_PIN     PIN_0           /* pin DIR  */

/* ---------- Parametros de la rampa (pasos/s y pasos/s^2) ---------- */
#define TICK_HZ     10000u          /* 1 tick = 100 us (ver SysTick_Config) */
#define V_START     150             /* velocidad de arranque (pasos/s) */
#define V_MAX       2000            /* velocidad de crucero maxima (pasos/s) */
#define ACCEL       1000            /* aceleracion (pasos/s^2)         */
#define V_HOME      800             /* velocidad de homing (pasos/s) ~4 mm/s, suave */

/* ---------- Punto fijo Q8 para la velocidad ---------- */
#define VSCALE      256u                                  /* factor de escala (2^8) */
#define V_START_FP  ((uint32_t)V_START * VSCALE)
#define V_HOME_FP   ((uint32_t)V_HOME  * VSCALE)
#define ACCEL_NUM   ((uint32_t)ACCEL * VSCALE * VSCALE)   /* numerador de ACCEL/vel */
#define TICKS_NUM   ((uint32_t)TICK_HZ * VSCALE)          /* numerador de TICK_HZ/vel */

/* "Infinito" de pasos para el movimiento continuo de homing */
#define JOG_STEPS   ((int32_t)0x7FFFFFFF)

/* ---------- Estado del generador de pasos (lo toca la ISR de SysTick) ---------- */
static volatile int32_t  stepsToGo  = 0;   /* pasos que faltan del movimiento  */
static volatile int32_t  stepsDone  = 0;   /* pasos ya dados en el movimiento   */
static volatile int32_t  accelSteps = 0;   /* pasos que tardo en llegar a vmax  */
static volatile int32_t  position   = 0;   /* posicion absoluta en pasos (signo) */
static volatile uint32_t vel_fp     = 0;   /* velocidad actual en Q8 (pasos/s*256) */
static volatile uint32_t interval   = 0;   /* ticks entre pasos                 */
static volatile uint32_t tickCnt    = 0;
static volatile uint32_t vmax       = V_MAX; /* velocidad de crucero (la maneja el pot) */
static volatile uint8_t  pulseHigh  = 0;
static volatile uint8_t  moveDir    = DIR_TO_MAX;  /* sentido del movimiento actual */
static volatile uint8_t  decel      = 0;
static volatile uint8_t  jogMode    = 0;   /* 1 = jog (rampa de arranque, luego constante) */

/* ---------- Prototipos privados ---------- */
static void motor_configPins(void);

/* ====================================================================== */

void motor_init(void) {
    NVIC_SetPriority(SysTick_IRQn, 0);      /* el generador de pasos es lo mas prioritario */
    motor_configPins();

    /* Tick de 100 us, independiente del reloj real (CCLK = 100 MHz aca).
       SystemCoreClock/10000 = 10000 -> SysTick interrumpe cada 100 us. */
    SysTick_Config(SystemCoreClock / 10000);
}

/* Configura STEP y DIR como salidas GPIO. */
static void motor_configPins(void) {
    PINSEL_CFG_T pinCfg = {0};
    pinCfg.func = PINSEL_FUNC_00;

    /* STEP */
    pinCfg.port = STEP_PORT;
    pinCfg.pin  = STEP_PIN;
    PINSEL_ConfigPin(&pinCfg);
    GPIO_SetDir(STEP_PORT, (1 << STEP_PIN), GPIO_OUTPUT);
    GPIO_ClearPins(STEP_PORT, (1 << STEP_PIN));

    /* DIR */
    pinCfg.port = DIR_PORT;
    pinCfg.pin  = DIR_PIN;
    PINSEL_ConfigPin(&pinCfg);
    GPIO_SetDir(DIR_PORT, (1 << DIR_PIN), GPIO_OUTPUT);
    GPIO_ClearPins(DIR_PORT, (1 << DIR_PIN));
}

/* Setea el pin DIR segun el sentido pedido. */
static void applyDir(uint8_t dir) {
    if (dir) GPIO_SetPins(DIR_PORT, (1 << DIR_PIN));
    else     GPIO_ClearPins(DIR_PORT, (1 << DIR_PIN));
}

void motor_move(int32_t steps, uint8_t dir) {
    /* Antitrabado: no arrancar contra un tope que ya esta pisado.
       Como solo se bloquea el sentido del tope pisado, el retroceso siempre se permite. */
    if (dir == DIR_TO_MIN && limit_pressed(LS_MIN_MASK)) return;
    if (dir == DIR_TO_MAX && limit_pressed(LS_MAX_MASK)) return;

    moveDir = dir;                  /* que la ISR del final sepa hacia donde voy */
    applyDir(dir);

    jogMode    = 0;
    stepsDone  = 0;
    accelSteps = 0;
    decel      = 0;
    vel_fp     = V_START_FP;
    interval   = TICKS_NUM / vel_fp;
    tickCnt    = 0;
    stepsToGo  = steps;             /* esto "dispara" el movimiento */
}

void motor_jog(uint8_t dir) {
    if (dir == DIR_TO_MIN && limit_pressed(LS_MIN_MASK)) return;
    if (dir == DIR_TO_MAX && limit_pressed(LS_MAX_MASK)) return;

    moveDir = dir;
    applyDir(dir);

    jogMode   = 1;                  /* jog con rampa de arranque, luego constante */
    vel_fp    = V_START_FP;         /* arranca lento y acelera hasta V_HOME (no saltar) */
    interval  = TICKS_NUM / vel_fp;
    tickCnt   = 0;
    stepsToGo = JOG_STEPS;          /* "infinito": solo lo frena un tope o motor_stop() */
}

void motor_stop(void) {
    stepsToGo = 0;                  /* store atomico de 32 bits -> frena en el acto */
}

uint8_t motor_busy(void) {
    return (stepsToGo > 0) ? 1u : 0u;
}

uint8_t motor_dir(void) {
    return moveDir;
}

int32_t motor_position(void) {
    return position;
}

int32_t motor_position_centimm(void) {
    /* cmm = position * 100 / STEPS_PER_MM, redondeado al 0.01 mm mas cercano */
    int32_t num = position * 100;
    return (num >= 0) ? (num + STEPS_PER_MM / 2) / STEPS_PER_MM
                      : (num - STEPS_PER_MM / 2) / STEPS_PER_MM;
}

void motor_set_position(int32_t steps) {
    position = steps;
}

void motor_goto_steps(int32_t target) {
    int32_t delta = target - position;          /* cuanto falta, con signo */
    if (delta > 0)      motor_move(delta, DIR_TO_MAX);
    else if (delta < 0) motor_move(-delta, DIR_TO_MIN);
    /* delta == 0: ya estamos en el objetivo, no hacer nada */
}

void motor_goto_centimm(int32_t cmm) {
    /* target en pasos = cmm * STEPS_PER_MM / 100, redondeado al paso mas cercano.
       cmm*STEPS_PER_MM entra de sobra en int32 (250 mm -> 25000*200 = 5e6). */
    int32_t num = cmm * STEPS_PER_MM;
    int32_t target = (num >= 0) ? (num + 50) / 100 : (num - 50) / 100;
    motor_goto_steps(target);
}

void motor_set_max_speed(uint32_t v) {
    if (v < V_START) v = V_START;       /* no por debajo del arranque */
    if (v > V_MAX)   v = V_MAX;          /* tope de seguridad */
    vmax = v;
}

/* ---------------------------- ISR del SysTick ---------------------------- */
void SysTick_Handler(void) {
    /* Bajar STEP un tick despues de subirlo => pulso de 100 us
       (de sobra para el A4988, que pide minimo ~1 us). */
    if (pulseHigh) {
        GPIO_ClearPins(STEP_PORT, (1 << STEP_PIN));
        pulseHigh = 0;
    }

    if (stepsToGo <= 0)        return;   /* no hay movimiento en curso */
    if (++tickCnt < interval)  return;   /* todavia no toca dar el paso */
    tickCnt = 0;

    /* Flanco de subida en STEP = un paso */
    GPIO_SetPins(STEP_PORT, (1 << STEP_PIN));
    pulseHigh = 1;
    stepsDone++;
    stepsToGo--;

    /* Posicion absoluta: + hacia MAX, - hacia MIN */
    position += (moveDir == DIR_TO_MAX) ? 1 : -1;

    if (jogMode) {
        /* Rampa de arranque tambien en jog/homing: acelera desde V_START hasta
           V_HOME y se queda ahi, asi no se traba si V_HOME es alto. Sin decel:
           al tocar el tope, motor_stop() lo frena en seco (es lo que queremos). */
        if (vel_fp < V_HOME_FP) {
            vel_fp += ACCEL_NUM / vel_fp;
            if (vel_fp > V_HOME_FP) vel_fp = V_HOME_FP;
            interval = TICKS_NUM / vel_fp;
            if (interval < 2) interval = 2;
        }
        return;                          /* en jog no hay deceleracion ni pot */
    }

    /* --- Rampa trapezoidal, todo entero en punto fijo Q8 --- */
    volatile uint32_t vmax_fp = vmax * VSCALE;    /* velocidad de crucero del pot, escalada */

    if (decel) {                                 /* deceleracion */
        vel_fp -= ACCEL_NUM / vel_fp;
        if (vel_fp < V_START_FP) vel_fp = V_START_FP;
    }
    else if (vel_fp < vmax_fp) {                 /* aceleracion */
        if (stepsToGo <= stepsDone) {            /* movimiento corto: freno en el medio */
            decel = 1;
        } else {
            vel_fp += ACCEL_NUM / vel_fp;
            if (vel_fp >= vmax_fp) { vel_fp = vmax_fp; accelSteps = stepsDone; }
        }
    }
    else {                                       /* crucero */
        if (vel_fp > vmax_fp) {                  /* el pot bajo la velocidad: seguirlo */
            vel_fp -= ACCEL_NUM / vel_fp;
            if (vel_fp < vmax_fp) vel_fp = vmax_fp;
        }
        if (stepsToGo <= accelSteps) decel = 1;  /* empezar a frenar (simetrico) */
    }

    interval = TICKS_NUM / vel_fp;
    if (interval < 2) interval = 2;              /* garantiza que el pulso baje */
}
