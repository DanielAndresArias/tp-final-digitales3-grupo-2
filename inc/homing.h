/*
 * homing.h
 * Rutina de búsqueda del cero (homing).
 *
 * Manda la tuerca despacio contra el final de carrera del 0, deja que el tope
 * la frene y fija ahi la referencia (posicion del motor y del encoder en 0).
 */

#ifndef HOMING_H_
#define HOMING_H_

#include <stdint.h>

/* Ejecuta el homing (bloqueante). Devuelve 1 si encontro el cero, 0 si fallo. */
uint8_t homing_run(void);

#endif /* HOMING_H_ */
