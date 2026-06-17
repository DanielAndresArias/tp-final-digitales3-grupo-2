/*
 * Control de motor paso a paso NEMA17 con driver A4988 en LPC1769.
 *
 * - Genera los pasos desde el SysTick (base de tiempo de 100 us).
 * - Usa una rampa trapezoidal: arranca lento, acelera hasta una
 *   velocidad de crucero, la mantiene y desacelera antes de frenar.
 *   Esto evita que el motor pierda pasos al arrancar/frenar de golpe.
 *
 * Mecanica: varilla roscada de 1 mm/vuelta + NEMA17 de 200 pasos/vuelta
 * en PASO COMPLETO (MS1/MS2/MS3 a masa)  =>  200 pasos = 1 mm.
 */

#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_systick.h"

//========================= includes para el encoder

#include "LPC17xx.h"          // definiciones de registros del LPC1769
#include "lpc17xx_qei.h"      // driver del QEI
#include "lpc17xx_clkpwr.h"   // para encender el periférico

/* configuracion  y definiciones para el encoder:  */

#define CUENTAS_POR_MM   4000.0f
#define PASO_ROSCA_MM   1.0f
#define ENCODER_PPR     1000



/* ---------- Conexiones ---------- */
#define STEP_PORT   PORT_3
#define STEP_PIN    PIN_26          /* pin STEP del A4988 */
#define DIR_PORT    PORT_0
#define DIR_PIN     PIN_0           /* pin DIR del A4988 (elegi uno libre) */

/* ---------- Parametros de la rampa ---------- */
#define TICK_HZ     10000u          /* 1 tick = 100 us (ver SysTick_Config) */
#define V_START     150          /* velocidad de arranque (pasos/s) ~45 rpm  */
#define V_MAX       2000          /* velocidad de crucero  (pasos/s) ~150 rpm */
#define ACCEL       1000         /* aceleracion (pasos/s^2)                   */

/* ---------- Conversion distancia <-> pasos ---------- */
#define STEPS_PER_MM   200          /* 200 pasos/vuelta * 1 vuelta/mm */
#define MM_TO_STEPS(mm)  ((int32_t)((mm) * STEPS_PER_MM))

/* ---------- Estado del generador de pasos (lo toca la ISR) ---------- */
static volatile int32_t  stepsToGo  = 0;   /* pasos que faltan del movimiento  */
static volatile int32_t  stepsDone  = 0;   /* pasos ya dados en el movimiento   */
static volatile int32_t  accelSteps = 0;   /* pasos que tardo en llegar a V_MAX */
static volatile float    vel        = 0;   /* velocidad actual (pasos/s)        */
static volatile uint32_t interval   = 0;   /* ticks entre pasos                 */
static volatile uint32_t tickCnt    = 0;
static volatile uint8_t  pulseHigh  = 0;
static volatile uint8_t  decel      = 0;

/* ---------- Prototipos ---------- */
void configGPIO(void);
void moveSteps(int32_t n, uint8_t dir);
void configQEI(void);
void configQEI_reload(void);

int main(void) {
    NVIC_SetPriority(SysTick_IRQn, 0);
    configGPIO();

    configQEI();

    configQEI_reload();
    /* Tick de 100 us, independiente del reloj real (CCLK = 100 MHz aca).
       SystemCoreClock/10000 = 10000 -> SysTick interrumpe cada 100 us. */
    SysTick_Config(SystemCoreClock / 10000);

    /* --- Demo: va y vuelve 10 mm para probar --- */
    uint8_t dir = 1;
    while (1) {
        if (stepsToGo == 0) {                       /* termino el movimiento anterior */
            for (volatile uint32_t d = 0; d < 2000000; d++) { }   /* pausa simple */
            moveSteps(MM_TO_STEPS(150), dir);        /* mover 10 mm */
            dir ^= 1;                               /* alternar el sentido */
        }
    }
    return 0;
}



//================================================== configuraciones del encoder

void configQEI(void){
    PINSEL_CFG_Type PinCfg;

    // Phase A → P1.20
    PinCfg.Portnum   = PINSEL_PORT_1;
    PinCfg.Pinnum    = PINSEL_PIN_20;
    PinCfg.Funcnum   = PINSEL_FUNC_1;  // función 1 = MCI0 (QEI Phase A)
    PinCfg.Pinmode   = PINSEL_PINMODE_TRISTATE;
    PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
    PINSEL_ConfigPin(&PinCfg);

    // Phase B → P1.23
    PinCfg.Pinnum  = PINSEL_PIN_23;
    PinCfg.Funcnum = PINSEL_FUNC_1;  // función 1 = MCI1 (QEI Phase B)
    PINSEL_ConfigPin(&PinCfg);

    // Index → P1.24
    PinCfg.Pinnum  = PINSEL_PIN_24;
    PinCfg.Funcnum = PINSEL_FUNC_1;  // función 1 = MCI2 (QEI Index)
    PINSEL_ConfigPin(&PinCfg);

    QEI_CFG_Type QEIConfig;

    QEIConfig.DirectionInvert = DISABLE;       // no invertir dirección
    QEIConfig.SignalMode      = QEI_SIGNALMODE_QUAD;    // modo cuadratura A/B
    QEIConfig.CaptureMode     = QEI_CAPMODE_4X;         // contar 4 flancos por ciclo
    QEIConfig.InvertIndex     = DISABLE;       // no invertir señal de index

    QEI_Init(LPC_QEI, &QEIConfig);

}




void configQEI_reload(void){
    QEI_RELOADCFG_Type ReloadConfig;

    ReloadConfig.ReloadOption = QEI_TIMERRELOAD_USVAL;
    ReloadConfig.ReloadValue  = 10000;  // capturar cada 10000 µs = cada 10 ms

    QEI_SetTimerReload(LPC_QEI, &ReloadConfig);

}
//==================================================
/* funciones para el encoder   */


float calcular_velocidad(void) {
    uint32_t vel_cap = QEI_GetVelocityCap(LPC_QEI);

    if (vel_cap == 0) {
        return 0.0f;
    }

    uint32_t rpm = QEI_CalculateRPM(LPC_QEI, vel_cap, ENCODER_PPR);

    float vel_mm_por_seg = ((float)rpm * PASO_ROSCA_MM) / 60.0f;

    return vel_mm_por_seg;
}


float calcular_posicion_mm(void) {
    uint32_t cuentas = QEI_GetPosition(LPC_QEI);
    return (float)cuentas / CUENTAS_POR_MM;
}

//======================================================



/* Configura STEP y DIR como salidas GPIO. */
void configGPIO(void) {
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

/* Arranca un movimiento de n pasos. dir = 1 o 0 segun el sentido de giro. */
void moveSteps(int32_t n, uint8_t dir) {
    if (dir) GPIO_SetPins(DIR_PORT, (1 << DIR_PIN));
    else     GPIO_ClearPins(DIR_PORT, (1 << DIR_PIN));

    stepsDone  = 0;
    accelSteps = 0;
    decel      = 0;
    vel        = V_START;
    interval   = (uint32_t)(TICK_HZ / V_START);
    tickCnt    = 0;
    stepsToGo  = n;                 /* esto "dispara" el movimiento */
}

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

    /* Actualizar la velocidad segun la fase de la rampa */
    if (decel) {                                /* deceleracion */
        vel -= ACCEL / vel;
        if (vel < V_START) vel = V_START;
    }
    else if (vel < V_MAX) {                     /* aceleracion */
        if (stepsToGo <= stepsDone) {           /* movimiento corto: freno en el medio */
            decel = 1;
        } else {
            vel += ACCEL / vel;
            if (vel >= V_MAX) { vel = V_MAX; accelSteps = stepsDone; }
        }
    }
    else {                                      /* crucero */
        if (stepsToGo <= accelSteps) decel = 1; /* empezar a frenar (simetrico) */
    }

    interval = (uint32_t)(TICK_HZ / vel);
    if (interval < 2) interval = 2;             /* garantiza que el pulso baje */
}
