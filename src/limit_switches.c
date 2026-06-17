/*
 * limit_switches.c
 * Finales de carrera por interrupcion externa (EINT0/EINT1).
 *
 * - Logica negativa: pull-up interno -> reposo en alto; pisado -> nivel bajo.
 * - Se interrumpe por flanco descendente (el momento del pisado).
 * - Al pisar, se frena el motor SOLO si se estaba yendo hacia ese tope,
 *   de modo que el retroceso nunca se interrumpe.
 * - El nivel real del switch se puede leer con FIOPIN aunque el pin este en
 *   modo EINT (lo confirma el manual, Tabla 73), y eso lo usa motor_move()
 *   para no arrancar contra un tope ya pisado.
 */

#include "limit_switches.h"
#include "motor.h"

#include "lpc17xx_exti.h"
#include "lpc17xx_gpio.h"

/* ====================================================================== */

void limits_init(void) {
    EXTI_Init();

    /* Mux a EINT + pull-up interno (reposo en alto) */
    EXTI_PinConfig(EXTI_EINT0, EXTI_PULLUP);   /* P2.10 */
    EXTI_PinConfig(EXTI_EINT1, EXTI_PULLUP);   /* P2.11 */

    EXTI_CFG_T cfg;
    cfg.mode     = EXTI_EDGE_SENSITIVE;
    cfg.polarity = EXTI_FALLING_EDGE;          /* el "pisado" es un flanco de bajada */

    NVIC_SetPriority(EINT0_IRQn, 1);           /* justo debajo del SysTick (prioridad 0) */
    NVIC_SetPriority(EINT1_IRQn, 1);

    cfg.line = EXTI_EINT0; EXTI_ConfigEnable(&cfg);
    cfg.line = EXTI_EINT1; EXTI_ConfigEnable(&cfg);
}

uint8_t limit_pressed(uint32_t mask) {
    /* logica negativa: bit en 0 => pisado */
    return (GPIO_ReadValue(PORT_2) & mask) ? 0u : 1u;
}

/* ------------------------------- ISRs ------------------------------- */

void EINT0_IRQHandler(void) {                  /* tope MAX (+) */
    EXTI_ClearFlag(EXTI_EINT0);
    if (motor_dir() == DIR_TO_MAX) motor_stop();
}

void EINT1_IRQHandler(void) {                  /* tope MIN / cero (-) */
    EXTI_ClearFlag(EXTI_EINT1);
    if (motor_dir() == DIR_TO_MIN) motor_stop();
    /* Mas adelante, durante el homing, aca pondremos la posicion en 0. */
}
