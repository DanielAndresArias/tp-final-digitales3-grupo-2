/*
 * limit_switches.h
 * Finales de carrera con logica negativa (reposo en alto, pisado = nivel bajo).
 *
 * Cableado: dos switches en P2.10 (EINT0) y P2.11 (EINT1).
 * Interrupcion por flanco descendente (= el "pisado") con pull-up interno.
 */

#ifndef LIMIT_SWITCHES_H_
#define LIMIT_SWITCHES_H_

#include <stdint.h>

/* Mascaras de los pines dentro del PORT_2 (para leer el nivel real del switch) */
#define LS_MAX_MASK   (1u << 10)   /* EINT0 -> P2.10 : tope lejano (+)        */
#define LS_MIN_MASK   (1u << 11)   /* EINT1 -> P2.11 : tope cero / home (-)   */

/* Configura los pines EINT0/EINT1, los pull-ups y habilita las interrupciones. */
void limits_init(void);

/* Devuelve 1 si el final de carrera indicado por 'mask' esta pisado, 0 si no.
 * 'mask' debe ser LS_MAX_MASK o LS_MIN_MASK. */
uint8_t limit_pressed(uint32_t mask);

#endif /* LIMIT_SWITCHES_H_ */
