/*
 * comms.h
 * Interfaz de comandos por UART0 (P0.2 TXD0 / P0.3 RXD0).
 *
 * Recibe lineas de texto desde la PC y las ejecuta:
 *   <numero>  -> ir a esa posicion en mm (ej: "40", "12.5")
 *   p / P     -> reportar posicion (motor y encoder)
 *   h / H     -> rehacer el homing
 *
 * La recepcion es por interrupcion; el parseo/ejecucion se hace en comms_task(),
 * que hay que llamar desde el lazo principal.
 */

#ifndef COMMS_H_
#define COMMS_H_

/* Configura UART0, los pines y la interrupcion de recepcion. */
void comms_init(void);

/* Procesa un comando si llego una linea completa. Llamar desde el lazo. */
void comms_task(void);

#endif /* COMMS_H_ */
