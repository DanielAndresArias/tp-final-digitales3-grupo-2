#include "lpc17xx_adc.h"
#include "lpc17xx_pinsel.h"

#define ADC_CHANNEL     ADC_CHANNEL_0
#define ADC_PIN         23
#define ADC_PORT        0

void configADC(void);



int main(void){

    configADC();
while(1){



}



return 0;
}

void configADC(void) {
    // configurar P0.23 como AD0.0
    PINSEL_CFG_Type pin;
    pin.Portnum   = ADC_PORT;
    pin.Pinnum    = ADC_PIN;
    pin.Funcnum   = PINSEL_FUNC_1;   // función 1 = AD0.0
    pin.Pinmode   = PINSEL_PINMODE_TRISTATE;  // sin pull-up ni pull-down
    pin.OpenDrain = PINSEL_PINMODE_NORMAL;
    PINSEL_ConfigPin(&pin);

    // inicializar el ADC a 200kHz, modo burst desactivado
    ADC_Init(LPC_ADC, 200000);
    ADC_IntConfig(LPC_ADC, ADC_CHANNEL, DISABLE);
    ADC_ChannelCmd(LPC_ADC, ADC_CHANNEL, ENABLE);
}


static const uint32_t tabla_velocidades[4] = {
    300,    // velocidad 1
    700,    // velocidad 2
    1400,   // velocidad 3
    2000    // velocidad 4 - irian a las variables de velocidad y de esta forma elegimos
            // velocidad_max, pero hay que revisar estos datos
};

uint32_t leer_velocidad_pot(void) {
    // iniciar conversion y esperar resultado
    ADC_StartCmd(LPC_ADC, ADC_START_NOW);
    while (!ADC_ChannelGetStatus(LPC_ADC, ADC_CHANNEL, ADC_DATA_DONE));

    uint32_t valor = ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL); // 0 a 4095
    uint8_t  indice = valor / 1024;  // 0 a 3

    // proteccion por si valor es exactamente 4095
    if (indice > 3) indice = 3;

    return tabla_velocidades[indice];
}



