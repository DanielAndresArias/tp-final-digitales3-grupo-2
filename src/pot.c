/*
 * pot.c
 * Potenciometro de velocidad leido por el ADC (canal 0, P0.23 = AD0.0).
 *
 * Traduccion a la API mejorada del driver ADC:
 *   - ADC_Init(rate) ya enciende el periferico (PCONP + PDN).
 *   - ADC_PinConfig(ADC_CHANNEL_0) mapea P0.23 como AD0.0 (no hace falta PINSEL manual).
 *   - Lectura por polling con ADC_StartCmd / ADC_ChannelGetStatus / ADC_ChannelGetData.
 */

#include "pot.h"

#include "lpc17xx_adc.h"

#define ADC_RATE   200000u          /* 200 kHz de muestreo */

/* 4 velocidades seleccionables con el pot (pasos/s).
 * 2000 pasos/s = V_MAX del motor. Ajustar segun tu mecanica. */
static const uint32_t tabla_velocidades[4] = {
    300,    /* velocidad 1 (lenta)  */
    700,    /* velocidad 2          */
    1400,   /* velocidad 3          */
    2000    /* velocidad 4 (rapida) */
};

void pot_init(void) {
    ADC_Init(ADC_RATE);
    ADC_PinConfig(ADC_CHANNEL_0);       /* P0.23 -> AD0.0 */
    ADC_IntDisable(ADC_INT_CH0);        /* sin interrupcion: lo leemos por polling */
    ADC_ChannelEnable(ADC_CHANNEL_0);
}

uint32_t pot_get_speed(void) {
    ADC_StartCmd(ADC_START_NOW);
    while (!ADC_ChannelGetStatus(ADC_CHANNEL_0, ADC_DATA_DONE)) {
        /* esperar fin de conversion */
    }
    uint16_t valor  = ADC_ChannelGetData(ADC_CHANNEL_0);   /* 0..4095 */
    uint8_t  indice = (uint8_t)(valor >> 10);              /* /1024 -> 0..3 */
    if (indice > 3) indice = 3;                            /* por las dudas */
    return tabla_velocidades[indice];
}
