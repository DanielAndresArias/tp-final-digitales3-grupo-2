/*
 * main.c
 * TP Final - Electronica Digital 3 - FCEFyN, UNC.
 *
 * Posicionador lineal: NEMA17 + A4988 sobre varilla roscada (1 mm/vuelta),
 * encoder ISC3806-1000 por QEI, finales de carrera con logica negativa y
 * potenciometro de velocidad por ADC.
 *
 * Estructura de modulos:
 *   motor.{c,h}            -> generador de pasos con rampa + ir a posicion + velocidad
 *   limit_switches.{c,h}   -> topes por EINT, frenan el motor
 *   encoder.{c,h}          -> lectura de posicion/velocidad por QEI
 *   homing.{c,h}           -> busqueda del cero del recorrido
 *   pot.{c,h}              -> potenciometro de velocidad (ADC)
 */

#include "motor.h"
#include "limit_switches.h"
#include "encoder.h"
#include "homing.h"
#include "pot.h"

int main(void) {
    encoder_init();
    limits_init();
    pot_init();
    motor_init();        /* arranca el SysTick: dejarlo ultimo */

    /* Buscar el cero del recorrido antes de moverse a cualquier lado. */
    homing_run();

    /* --- Demo: recorre posiciones absolutas; el pot fija la velocidad en vivo --- */
    const float targets[] = { 10.0f, 40.0f, 20.0f, 60.0f };
    const int   n = 4;
    int i = 0;

    while (1) {
        motor_set_max_speed(pot_get_speed());     /* velocidad segun el potenciometro */

        if (!motor_busy()) {
            for (volatile uint32_t d = 0; d < 4000000; d++) { }   /* pausa entre destinos */
            motor_goto_mm(targets[i]);
            i = (i + 1) % n;
        }
    }
    return 0;
}
