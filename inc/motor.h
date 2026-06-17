/*
 * motor.h
 * Control del motor paso a paso NEMA17 (driver A4988) con rampa trapezoidal.
 *
 * El generador de pasos vive en la ISR del SysTick (base de 100 us).
 * Mecanica: 200 pasos/vuelta (paso completo) * 1 vuelta/mm => 200 pasos = 1 mm.
 */

#ifndef MOTOR_H_
#define MOTOR_H_

#include <stdint.h>

/* ---------- Conversion distancia <-> pasos ---------- */
#define STEPS_PER_MM     200                               /* 200 pasos/vuelta * 1 vuelta/mm */
#define MM_TO_STEPS(mm)  ((int32_t)((mm) * STEPS_PER_MM))

/* ---------- Convencion de sentido  (¡VERIFICAR EN EL BANCO!) ----------
 * Si el motor gira al reves de lo esperado, intercambia estos dos valores.
 * Los usan tanto el motor como los finales de carrera, por eso viven aca. */
#define DIR_TO_MAX   1u   /* valor de 'dir' que ALEJA la tuerca del 0 (+) */
#define DIR_TO_MIN   0u   /* valor de 'dir' que ACERCA la tuerca al 0 (-) */

/* ---------- Interfaz publica ---------- */

/* Configura los pines STEP/DIR y arranca la base de tiempo (SysTick). */
void motor_init(void);

/* Arranca un movimiento de 'steps' pasos en el sentido 'dir' (con rampa trapezoidal).
 * No arranca si el tope hacia el que iria ya esta pisado (antitrabado). */
void motor_move(int32_t steps, uint8_t dir);

/* Movimiento continuo a velocidad baja y constante (sin rampa), pensado para homing.
 * Avanza en 'dir' hasta que algo lo frene (un final de carrera o motor_stop()). */
void motor_jog(uint8_t dir);

/* Frena el movimiento en curso de inmediato. La llama la ISR de un final de carrera. */
void motor_stop(void);

/* Devuelve 1 mientras haya un movimiento en curso, 0 si esta detenido. */
uint8_t motor_busy(void);

/* Devuelve el sentido del movimiento actual (DIR_TO_MAX o DIR_TO_MIN). */
uint8_t motor_dir(void);

/* ---------- Posicion absoluta ---------- */

/* Posicion actual en pasos (con signo). Se pone en 0 en el home. */
int32_t motor_position(void);

/* Posicion actual en milimetros. */
float motor_position_mm(void);

/* Fija el contador de posicion (p.ej. motor_set_position(0) al llegar al home). */
void motor_set_position(int32_t steps);

/* ---------- Velocidad ---------- */

/* Fija la velocidad de crucero (pasos/s). La usa el potenciometro.
 * Queda acotada a [V_START, V_MAX]; el cambio se aplica en vivo (incluso a
 * mitad de un movimiento). */
void motor_set_max_speed(uint32_t v);


/* ---------- Ir a posicion absoluta ---------- */

/* Va a una posicion absoluta (NO bloqueante: dispara el movimiento y vuelve).
 * El sentido se calcula solo segun la posicion actual. Para saber cuando
 * termino, consultar motor_busy(). */
void motor_goto_steps(int32_t target);
void motor_goto_mm(float mm);

#endif /* MOTOR_H_ */
