/*
 * homing.c
 * Busqueda del cero del recorrido.
 *
 * Secuencia:
 *   1. Si la tuerca no esta ya sobre el switch del 0, la mandamos despacio
 *      hacia el (motor_jog a velocidad de homing).
 *   2. El final de carrera del 0 (EINT1) frena el motor al pisarse.
 *   3. Verificamos que efectivamente quedamos sobre ese switch.
 *   4. Fijamos el cero (contador de pasos y encoder a 0).
 *   5. Nos despegamos del switch unos mm: no conviene operar pisando el final
 *      de carrera, porque el rebote del contacto dispara la interrupcion una y
 *      otra vez y le roba el CPU al lazo principal. El 0 sigue en el switch;
 *      la tuerca solo descansa en +BACKOFF_MM.
 */

#include "homing.h"

#include "motor.h"
#include "limit_switches.h"
#include "encoder.h"

#define BACKOFF_MM   2          /* cuanto despegarse del switch tras tocar el 0 (mm enteros) */

uint8_t homing_run(void) {
    /* Si no estamos ya en el cero, avanzar despacio hacia el switch del 0. */
    if (!limit_pressed(LS_MIN_MASK)) {
        motor_jog(DIR_TO_MIN);
        while (motor_busy()) {
            /* esperar: el final de carrera del 0 frena el motor al pisarse */
        }
    }

    /* Verificar que paramos por el switch del 0 (y no por otra cosa). */
    if (!limit_pressed(LS_MIN_MASK)) {
        return 0;                       /* homing fallido */
    }

    /* Cero alcanzado: fijar la referencia en ambos contadores. */
    motor_set_position(0);
    encoder_zero();

    /* Despegarse del switch para no quedar pisandolo (evita el chatter del
       contacto disparando interrupciones sin parar). Queda en +BACKOFF_MM. */
    motor_move(MM_TO_STEPS(BACKOFF_MM), DIR_TO_MAX);
    while (motor_busy()) {
        /* el SysTick (mas prioritario) genera los pasos aunque haya rebotes */
    }

    return 1;
}
