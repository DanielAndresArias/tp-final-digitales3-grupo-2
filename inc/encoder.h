/*
 * encoder.h
 * Lectura del encoder incremental ISC3806-1000 mediante el periferico QEI.
 *
 * Encoder de 1000 PPR en modo 4X => 4000 cuentas por vuelta.
 * Montado 1:1 sobre la varilla (1 mm/vuelta) => 4000 cuentas por mm.
 */

#ifndef ENCODER_H_
#define ENCODER_H_

#include <stdint.h>

/* Configura los pines PHA/PHB/IDX y arranca el QEI. */
void encoder_init(void);

/* Pone la cuenta de posicion del QEI en 0 (se usa en el home). */
void encoder_zero(void);

/* Posicion actual de la tuerca en centesimas de milimetro (mm x 100). */
int32_t encoder_posicion_centimm(void);

/* Velocidad lineal actual en centesimas de mm/s (mm/s x 100). */
int32_t encoder_velocidad_centimm_s(void);

/* DEBUG: refresca las variables que copian los registros del QEI/PINSEL. */
void encoder_debug_update(void);

#endif /* ENCODER_H_ */
