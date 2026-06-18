/*
 * encoder.c
 * Encoder incremental ISC3806-1000 leido por el QEI del LPC1769.
 *
 * Pines (funcion 01 = entradas del QEI, confirmado en el manual / PINSEL3):
 *   PHA -> P1.20 (MCI0)
 *   PHB -> P1.23 (MCI1)
 *   IDX -> P1.24 (MCI2)
 *
 * QEI_Init() ya enciende el periferico (PCONP) y fija su clock, no hace falta
 * tocar clkpwr a mano.
 */

#include "encoder.h"

#include "lpc17xx_pinsel.h"
#include "lpc17xx_qei.h"

/* ---------- Parametros del encoder ---------- */
#define CUENTAS_POR_MM   4000.0f    /* 4X * 1000 PPR, encoder 1:1 con la varilla */
#define PASO_ROSCA_MM    1.0f       /* avance de la varilla por vuelta            */
#define ENCODER_PPR      1000       /* pulsos por revolucion del encoder          */

/* ====================================================================== */

void encoder_init(void) {
    PINSEL_CFG_T pinCfg = {0};
    pinCfg.func      = PINSEL_FUNC_01;   /* 01 = MCIx (entradas del QEI) */
    pinCfg.mode      = PINSEL_PULLUP;    /* pull-up interno: necesario si el encoder es colector abierto */
    pinCfg.openDrain = DISABLE;
    pinCfg.port      = PORT_1;

    pinCfg.pin = PIN_20; PINSEL_ConfigPin(&pinCfg);   /* PHA -> MCI0 */
    pinCfg.pin = PIN_23; PINSEL_ConfigPin(&pinCfg);   /* PHB -> MCI1 */
    pinCfg.pin = PIN_24; PINSEL_ConfigPin(&pinCfg);   /* IDX -> MCI2 */

    QEI_CFG_Type qeiCfg;
    qeiCfg.DirectionInvert = QEI_DIRINV_NONE;          /* no invertir direccion   */
    qeiCfg.SignalMode      = QEI_SIGNALMODE_QUAD;      /* modo cuadratura A/B     */
    qeiCfg.CaptureMode     = QEI_CAPMODE_4X;           /* contar los 4 flancos    */
    qeiCfg.InvertIndex     = QEI_INVINX_NONE;          /* no invertir el index    */
    QEI_Init(LPC_QEI, &qeiCfg);

    /* Timer de velocidad: recargar cada 10 ms */
    QEI_RELOADCFG_Type reloadCfg;
    reloadCfg.ReloadOption = QEI_TIMERRELOAD_USVAL;
    reloadCfg.ReloadValue  = 10000;                    /* 10000 us = 10 ms */
    QEI_SetTimerReload(LPC_QEI, &reloadCfg);
}

void encoder_zero(void) {
    QEI_Reset(LPC_QEI, QEI_RESET_POS);                 /* contador de posicion a 0 */
}

float encoder_posicion_mm(void) {
    uint32_t cuentas = QEI_GetPosition(LPC_QEI);
    return (float)cuentas / CUENTAS_POR_MM;
}

float encoder_velocidad_mm_s(void) {
    uint32_t velCap = QEI_GetVelocityCap(LPC_QEI);
    if (velCap == 0) {
        return 0.0f;
    }
    uint32_t rpm = QEI_CalculateRPM(LPC_QEI, velCap, ENCODER_PPR);
    return ((float)rpm * PASO_ROSCA_MM) / 60.0f;
}
