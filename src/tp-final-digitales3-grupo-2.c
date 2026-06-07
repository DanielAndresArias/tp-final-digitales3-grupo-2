#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_systick.h"
#include "lpc17xx_adc.h"

static uint32_t time_ms=0;
static uint16_t adcValue=0;
static uint16_t valor=0;

void configGPIO();
void configADC();

int main(void) {
	NVIC_SetPriority(SysTick_IRQn, 0);
	configGPIO();
	configADC();
	SysTick_Config(99999);
    while(1) {
    }
    return 0 ;
}

void configGPIO() {
	PINSEL_CFG_T pinCfg = {0};
	pinCfg.func = PINSEL_FUNC_00;
	pinCfg.pin = PIN_26;
	pinCfg.port = PORT_3;

	PINSEL_ConfigPin(&pinCfg);

	GPIO_SetDir(PORT_3, (1<<PIN_26), GPIO_OUTPUT);
	GPIO_ClearPins(PORT_3, (1<<PIN_26));
}

void configADC(){
	ADC_Init(10000);
	ADC_PowerUp();
	ADC_PinConfig(ADC_CHANNEL_0);
	ADC_ChannelEnable(ADC_CHANNEL_0);
	ADC_BurstEnable();
	ADC_StartCmd(ADC_START_CONTINUOUS);
	ADC_IntEnable(ADC_INT_CH0);
	NVIC_EnableIRQ(ADC_IRQn);
}

void SysTick_Handler() {
	time_ms++;

	if (time_ms/500%2) {
		GPIO_ClearPins(PORT_3, (1<<PIN_26));
	} else {
		GPIO_SetPins(PORT_3, (1<<PIN_26));
	}
}

void ADC_IRQHandler(void) {
	adcValue=ADC_ChannelGetData(ADC_CHANNEL_0);

	if(adcValue<=1024){
		valor = 500;
	}
	else if(adcValue<=2048){
		valor = 250;
	}
	else if(adcValue<=3072){
		valor = 100;
	}
	else{
		valor = 5;
	}
}
