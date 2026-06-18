/*
 * pot.h
 * Potenciometro en AD0.0 (P0.23) que selecciona la velocidad de crucero del motor.
 *
 * El ADC corre en modo rafaga y el GPDMA deposita el dato en memoria solo
 * (sin polling ni CPU). 0..4095 -> una de 4 velocidades de la tabla.
 */

#ifndef POT_H_
#define POT_H_

#include <stdint.h>

/* Configura el ADC y el pin del potenciometro. */
void pot_init(void);

/* Lee el potenciometro y devuelve la velocidad correspondiente (pasos/s). */
uint32_t pot_get_speed(void);

/* DEBUG: refresca variables que copian los registros del ADC y del GPDMA. */
void pot_debug_update(void);

#endif /* POT_H_ */
