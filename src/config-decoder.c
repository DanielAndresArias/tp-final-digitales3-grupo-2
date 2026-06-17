#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"


//========================= includes para el encoder

#include "LPC17xx.h"          // definiciones de registros del LPC1769
#include "lpc17xx_qei.h"      // driver del QEI
#include "lpc17xx_clkpwr.h"   // para encender el periférico

/* configuracion  y definiciones para el encoder:  */

#define CUENTAS_POR_MM   4000.0f
#define PASO_ROSCA_MM   1.0f
#define ENCODER_PPR     1000

//prototipos

void configQEI(void);
void configQEI_reload(void);


int main(void) {
    
    configQEI();
    configQEI_reload();
    

    while (1) {
        
    }
    return 0;
}

//================================================== configuraciones del encoder

void configQEI(void){
    PINSEL_CFG_Type PinCfg;

    // Phase A → P1.20
    PinCfg.Portnum   = PINSEL_PORT_1;
    PinCfg.Pinnum    = PINSEL_PIN_20;
    PinCfg.Funcnum   = PINSEL_FUNC_1;  // función 1 = MCI0 (QEI Phase A)
    PinCfg.Pinmode   = PINSEL_PINMODE_TRISTATE;
    PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
    PINSEL_ConfigPin(&PinCfg);

    // Phase B → P1.23
    PinCfg.Pinnum  = PINSEL_PIN_23;
    PinCfg.Funcnum = PINSEL_FUNC_1;  // función 1 = MCI1 (QEI Phase B)
    PINSEL_ConfigPin(&PinCfg);

    // Index → P1.24
    PinCfg.Pinnum  = PINSEL_PIN_24;
    PinCfg.Funcnum = PINSEL_FUNC_1;  // función 1 = MCI2 (QEI Index)
    PINSEL_ConfigPin(&PinCfg);

    QEI_CFG_Type QEIConfig;

    QEIConfig.DirectionInvert = DISABLE;       // no invertir dirección
    QEIConfig.SignalMode      = QEI_SIGNALMODE_QUAD;    // modo cuadratura A/B
    QEIConfig.CaptureMode     = QEI_CAPMODE_4X;         // contar 4 flancos por ciclo
    QEIConfig.InvertIndex     = DISABLE;       // no invertir señal de index

    QEI_Init(LPC_QEI, &QEIConfig);

}




void configQEI_reload(void){
    QEI_RELOADCFG_Type ReloadConfig;

    ReloadConfig.ReloadOption = QEI_TIMERRELOAD_USVAL;
    ReloadConfig.ReloadValue  = 10000;  // capturar cada 10000 µs = cada 10 ms

    QEI_SetTimerReload(LPC_QEI, &ReloadConfig);

}
//==================================================
/* funciones para el encoder   */


float calcular_velocidad(void) {
    uint32_t vel_cap = QEI_GetVelocityCap(LPC_QEI);

    if (vel_cap == 0) {
        return 0.0f;
    }

    uint32_t rpm = QEI_CalculateRPM(LPC_QEI, vel_cap, ENCODER_PPR);

    float vel_mm_por_seg = ((float)rpm * PASO_ROSCA_MM) / 60.0f;

    return vel_mm_por_seg;
}


float calcular_posicion_mm(void) {
    uint32_t cuentas = QEI_GetPosition(LPC_QEI);
    return (float)cuentas / CUENTAS_POR_MM;
}

//======================================================
