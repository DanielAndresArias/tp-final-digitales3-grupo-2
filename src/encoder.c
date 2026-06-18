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

#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_qei.h"

/* ---------- Parametros del encoder ---------- */
#define CUENTAS_POR_MM   4000       /* 4X * 1000 PPR, encoder 1:1 con la varilla */
#define PASO_ROSCA_CMM   100        /* avance de la varilla por vuelta (1 mm = 100 cmm) */
#define ENCODER_PPR      1000       /* pulsos por revolucion del encoder          */

/* ---------- Variables de DEBUG (mirar en la ventana de variables del IDE) ---------- */
/* Copian registros del hardware para diagnosticar por que el QEI no cuenta.        */
static volatile uint32_t dbg_pconp_qei;   /* 1 = QEI alimentado (PCONP bit 18)        */
static volatile uint32_t dbg_pinsel3;     /* PINSEL3 crudo                            */
static volatile uint32_t dbg_pin_pha;     /* funcion P1.20 -> esperado 1 (QEI)        */
static volatile uint32_t dbg_pin_phb;     /* funcion P1.23 -> esperado 1 (QEI)        */
static volatile uint32_t dbg_pin_idx;     /* funcion P1.24 -> esperado 1 (QEI)        */
static volatile uint32_t dbg_qeiconf;     /* QEICONF -> esperado 0x4 (cuadratura,4X)  */
static volatile uint32_t dbg_qeimaxpos;   /* QEIMAXPOS -> debe dar 0xFFFFFFFF         */
static volatile int32_t  dbg_qeipos;      /* QEIPOS crudo: GIRA LA VARILLA y mira esto*/
static volatile uint32_t dbg_pha_level;   /* nivel FISICO de P1.20 (PHA): 0 o 1       */
static volatile uint32_t dbg_phb_level;   /* nivel FISICO de P1.23 (PHB): 0 o 1       */

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

    QEI_CFG_Type qeiCfg = {0};                         /* IMPORTANTE: limpiar bits reservados */
    qeiCfg.DirectionInvert = QEI_DIRINV_NONE;          /* no invertir direccion   */
    qeiCfg.SignalMode      = QEI_SIGNALMODE_QUAD;      /* modo cuadratura A/B     */
    qeiCfg.CaptureMode     = QEI_CAPMODE_4X;           /* contar los 4 flancos    */
    qeiCfg.InvertIndex     = QEI_INVINX_NONE;          /* no invertir el index    */
    QEI_Init(LPC_QEI, &qeiCfg);

    /* CRITICO: QEI_Init deja QEIMAXPOS = 0, y el contador de posicion se reinicia
       a 0 al llegar a QEIMAXPOS -> si queda en 0, la posicion SIEMPRE lee 0.
       Lo ponemos al maximo para que cuente libre (32 bits). */
    QEI_SetMaxPosition(LPC_QEI, 0xFFFFFFFFu);

    /* Timer de velocidad: recargar cada 10 ms */
    QEI_RELOADCFG_Type reloadCfg;
    reloadCfg.ReloadOption = QEI_TIMERRELOAD_USVAL;
    reloadCfg.ReloadValue  = 10000;                    /* 10000 us = 10 ms */
    QEI_SetTimerReload(LPC_QEI, &reloadCfg);

    encoder_debug_update();   /* foto inicial de los registros de config */
}

/* Refresca las variables de debug. Llamar en el lazo principal para ver QEIPOS vivo. */
void encoder_debug_update(void) {
    dbg_pconp_qei = (LPC_SC->PCONP >> 18) & 0x1u;   /* QEI encendido?            */
    dbg_pinsel3   = LPC_PINCON->PINSEL3;            /* ruteo de los pines        */
    dbg_pin_pha   = (dbg_pinsel3 >> 8)  & 0x3u;     /* P1.20: debe dar 1         */
    dbg_pin_phb   = (dbg_pinsel3 >> 14) & 0x3u;     /* P1.23: debe dar 1         */
    dbg_pin_idx   = (dbg_pinsel3 >> 16) & 0x3u;     /* P1.24: debe dar 1         */
    dbg_qeiconf   = LPC_QEI->QEICONF;               /* debe dar 0x4              */
    dbg_qeimaxpos = LPC_QEI->QEIMAXPOS;             /* debe dar 0xFFFFFFFF        */
    dbg_qeipos    = (int32_t)LPC_QEI->QEIPOS;       /* cambia al girar la varilla*/
    /* Nivel FISICO de los pines (FIOPIN lee el pin aunque este en funcion QEI).
       Gira la varilla a mano y mira si estos cambian entre 0 y 1. */
    dbg_pha_level = (LPC_GPIO1->FIOPIN >> 20) & 0x1u;   /* P1.20 = PHA */
    dbg_phb_level = (LPC_GPIO1->FIOPIN >> 23) & 0x1u;   /* P1.23 = PHB */
}

void encoder_zero(void) {
    QEI_Reset(LPC_QEI, QEI_RESET_POS);                 /* contador de posicion a 0 */
}

int32_t encoder_posicion_centimm(void) {
    /* Se lee como int32 con signo: si el encoder cuenta hacia atras y el contador
       del QEI "da la vuelta", queda como negativo chico en vez de un numero gigante. */
    int32_t cuentas = (int32_t)QEI_GetPosition(LPC_QEI);
    /* cmm = cuentas * 100 / CUENTAS_POR_MM, redondeado */
    int32_t num = cuentas * 100;
    return (num >= 0) ? (num + CUENTAS_POR_MM / 2) / CUENTAS_POR_MM
                      : (num - CUENTAS_POR_MM / 2) / CUENTAS_POR_MM;
}

int32_t encoder_velocidad_centimm_s(void) {
    uint32_t velCap = QEI_GetVelocityCap(LPC_QEI);
    if (velCap == 0) {
        return 0;
    }
    uint32_t rpm = QEI_CalculateRPM(LPC_QEI, velCap, ENCODER_PPR);
    /* mm/s = rpm * paso_mm / 60 ; en centimm/s = rpm * paso_cmm / 60 */
    return (int32_t)((rpm * PASO_ROSCA_CMM) / 60u);
}
