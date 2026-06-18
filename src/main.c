/*
 * main.c
 * TP Final - Electronica Digital 3 - FCEFyN, UNC.
 *
 * Posicionador lineal: NEMA17 + A4988 sobre varilla roscada (1 mm/vuelta),
 * encoder ISC3806-1000 por QEI, finales de carrera con logica negativa,
 * potenciometro de velocidad por ADC y comandos por UART.
 *
 * Estructura de modulos:
 *   motor.{c,h}            -> generador de pasos con rampa + ir a posicion + velocidad
 *   limit_switches.{c,h}   -> topes por EINT, frenan el motor
 *   encoder.{c,h}          -> lectura de posicion/velocidad por QEI
 *   homing.{c,h}           -> busqueda del cero del recorrido
 *   pot.{c,h}              -> potenciometro de velocidad (ADC)
 *   comms.{c,h}            -> comandos por UART (ir a X mm desde la PC)
 */

#include "motor.h"
#include "limit_switches.h"
#include "encoder.h"
#include "homing.h"
#include "pot.h"
#include "comms.h"

int main(void) {
    encoder_init();
    limits_init();
    pot_init();
    comms_init();
    motor_init();        /* arranca el SysTick: dejarlo ultimo */

    /* Buscar el cero del recorrido antes de aceptar ordenes. */
    homing_run();

    /* Lazo principal: la velocidad la fija el pot, los destinos llegan por UART. */
    while (1) {
        motor_set_max_speed(pot_get_speed());   /* velocidad segun el potenciometro */
        comms_task();                           /* procesa comandos UART (ir a X mm) */
    }
    return 0;
}
