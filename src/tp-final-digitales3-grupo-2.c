#include "lpc17xx_gpio.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_systick.h"

static int pos_secuencia = 0;
static uint8_t secuencia[8] = {
	1,
	3,
	2,
	6,
	4,
	12,
	8,
	9
};

void configStepMotorPins();

int main(void) {
	configStepMotorPins();
	// SysTick Timer 1 [ms]
	SysTick_Config(99999);
	while(1){}
    return 0 ;
}

void configStepMotorPins() {
	PINSEL_CFG_T pinselCfg = {
			PORT_0,
			0,
			PINSEL_FUNC_00,
			0,
			0,
	};

	PINSEL_ConfigMultiplePins(&pinselCfg, (15<<6));
	GPIO_SetDir(PORT_0, (15<<6), GPIO_OUTPUT);
	GPIO_ClearPins(PORT_0, (15<<6));
}

void SysTick_Handler() {
	GPIO_WriteValue(PORT_0, (secuencia[pos_secuencia]<<6));
	pos_secuencia=(pos_secuencia+1)%8;
}
