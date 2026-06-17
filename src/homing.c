/*
 * homing.c
 * Busqueda del cero del recorrido.
 *
 * Secuencia:
 *   1. Si la tuerca no esta ya sobre el switch del 0, la mandamos despacio
 *      hacia el (motor_jog a velocidad de homing).
 *   2. El final de carrera del 0 (EINT1) frena el motor al pisarse.
 *   3. Verificamos que efectivamente quedamos sobre ese switch.
 *   4. Fijamos el cero: contador de pasos y encoder a 0.
 */

#include "homing.h"

#include "motor.h"
#include "limit_switches.h"
#include "encoder.h"

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
    return 1;
}
