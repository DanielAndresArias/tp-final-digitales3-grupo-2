/*
 * pot.c
 * Potenciometro de velocidad leido por el ADC (canal 0, P0.23 = AD0.0),
 * con transferencia por GPDMA: la CPU NO lee ni espera el ADC.
 *
 * Esquema:
 *   - ADC en modo RAFAGA (burst): convierte el canal 0 de forma continua por hardware.
 *   - Cada conversion genera un request de DMA (habilitado via ADINTEN del canal 0;
 *     NO se habilita la IRQ del ADC en el NVIC, asi que la CPU nunca se interrumpe).
 *   - El GPDMA copia ADGDR -> adc_dma_buffer (1 palabra), periferico a memoria.
 *   - Una lista enlazada (LLI) que apunta a si misma hace la transferencia CIRCULAR:
 *     al terminar recarga el mismo LLI y vuelve a empezar, para siempre.
 *
 * Resultado: adc_dma_buffer siempre tiene el ultimo valor del ADC sin que el
 * procesador haga nada. pot_get_speed() solo lo lee.
 */

#include "pot.h"

#include "LPC17xx.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_gpdma.h"

#include <cr_section_macros.h>   /* macros para ubicar variables en un banco de RAM */

#define ADC_RATE   200000u          /* 200 kHz de muestreo */

/* 4 velocidades seleccionables con el pot (pasos/s). 2000 = V_MAX del motor. */
static const uint32_t tabla_velocidades[3] = {
    500,    /* velocidad 1 (lenta)  */
    1000,    /* velocidad 2          */
    1500,   /* velocidad 3          */

};

/* IMPORTANTE: estos dos van en AHB SRAM (banco 0x2007C000), NO en el RAM
 * principal (RamLoc32). Como el DMA corre continuamente, si el buffer estuviera
 * en RamLoc32 podria pisar el area donde el debugger carga su driver de flash
 * (y romper la grabacion). En un banco separado eso no puede pasar, y ademas el
 * AHB SRAM es el bus ideal para buffers de DMA. */

/* Aca el DMA deja, solo, la palabra completa de ADGDR (resultado + flags). */
__BSS(RamAHB32) static volatile uint32_t adc_dma_buffer;

/* Lista enlazada que apunta a si misma -> transferencia circular infinita. */
__BSS(RamAHB32) static GPDMA_LLI_T adc_lli;

/* ---------- DEBUG (mirar en la ventana de variables del IDE) ---------- */
static volatile uint32_t dbg_adcr;        /* ADCR: BURST=b16, PDN=b21, SEL ch0=b0, START(b24..)=0 */
static volatile uint32_t dbg_adstat;      /* ADSTAT (NO borra): DONE0=b0, OVERRUN0=b8, ADINT=b16   */
static volatile uint32_t dbg_adinten;     /* ADINTEN: b0=1 (request ch0), b8=0 (global off)        */
static volatile uint32_t dbg_adc_buf;     /* adc_dma_buffer: lo que el DMA deposito (b31=DONE)      */
static volatile uint32_t dbg_pconp_dma;   /* PCONP b29: GPDMA alimentado                            */
static volatile uint32_t dbg_dma_global;  /* DMACConfig b0=E: GPDMA global habilitado               */
static volatile uint32_t dbg_dma_enbld;   /* DMACEnbldChns b0: canal 0 corriendo                    */
static volatile uint32_t dbg_ch0_src;     /* DMACCSrcAddr: debe ser &ADGDR (~0x40034004)            */
static volatile uint32_t dbg_ch0_dst;     /* DMACCDestAddr: debe ser &adc_dma_buffer                */
static volatile uint32_t dbg_ch0_ctrl;    /* DMACCControl: tamanio/anchos                           */

void pot_debug_update(void) {
    dbg_adcr       = LPC_ADC->ADCR;
    dbg_adstat     = LPC_ADC->ADSTAT;        /* solo lectura, no roba el dato al DMA */
    dbg_adinten    = LPC_ADC->ADINTEN;
    dbg_adc_buf    = adc_dma_buffer;
    dbg_pconp_dma  = (LPC_SC->PCONP >> 29) & 0x1u;
    dbg_dma_global = LPC_GPDMA->DMACConfig;
    dbg_dma_enbld  = LPC_GPDMA->DMACEnbldChns;
    dbg_ch0_src    = LPC_GPDMACH0->DMACCSrcAddr;
    dbg_ch0_dst    = LPC_GPDMACH0->DMACCDestAddr;
    dbg_ch0_ctrl   = LPC_GPDMACH0->DMACCControl;
}

void pot_init(void) {
    /* ---------- ADC en modo rafaga ---------- */
    ADC_Init(ADC_RATE);
    ADC_PinConfig(ADC_CHANNEL_0);        /* P0.23 -> AD0.0                    */
    ADC_ChannelEnable(ADC_CHANNEL_0);    /* selecciona el canal 0             */
    ADC_IntDisable(ADC_INT_GLOBAL);      /* en burst, el DONE global NO debe interrumpir */
    ADC_IntEnable(ADC_INT_CH0);          /* habilita el request del canal 0 (lo usa el DMA) */
    /* OJO: NO habilitamos ADC_IRQn en el NVIC. El request lo consume el GPDMA,
       la CPU nunca entra a una ISR del ADC.
       El burst (que arranca las conversiones) se enciende AL FINAL, recien
       cuando el canal de DMA ya esta escuchando. */

    /* ---------- GPDMA: ADGDR -> adc_dma_buffer, circular ---------- */
    GPDMA_Init();

    GPDMA_Channel_CFG_T dma = {0};
    dma.channelNum    = GPDMA_CH_0;
    dma.transferSize  = 1;                         /* 1 palabra por pasada      */
    dma.type          = GPDMA_P2M;                 /* periferico (ADC) -> memoria */
    dma.srcConn       = GPDMA_ADC;                 /* la LUT del driver pone ADGDR */
    dma.dstConn       = GPDMA_ADC;                 /* destino es memoria; se ignora en P2M */
    dma.dstMemAddr    = (uint32_t)&adc_dma_buffer;
    dma.src.width     = GPDMA_WORD;
    dma.src.burst     = GPDMA_BSIZE_1;
    dma.src.increment = DISABLE;                   /* siempre lee la misma ADGDR */
    dma.dst.width     = GPDMA_WORD;
    dma.dst.burst     = GPDMA_BSIZE_1;
    dma.dst.increment = DISABLE;                   /* siempre escribe el mismo word */
    dma.intTC         = DISABLE;                   /* sin interrupciones de DMA  */
    dma.intErr        = DISABLE;
    dma.linkedList    = (uint32_t)&adc_lli;        /* al terminar, recarga este LLI */

    GPDMA_SetupChannel(&dma);

    /* Armamos el LLI EXPLICITAMENTE (no leido del registro, que era fragil y
       podia quedar con tamanio 0): ADGDR -> adc_dma_buffer, 1 palabra, sin
       incremento, y apuntando a si mismo para que se recargue para siempre. */
    adc_lli.srcAddr = (uint32_t)&LPC_ADC->ADGDR;          /* fuente: registro del ADC */
    adc_lli.dstAddr = (uint32_t)&adc_dma_buffer;          /* destino: el buffer       */
    adc_lli.control = GPDMA_DMACCxControl_TransferSize(1)        /* 1 transferencia */
                    | GPDMA_DMACCxControl_SWidth(GPDMA_WORD)     /* lee 32 bits     */
                    | GPDMA_DMACCxControl_DWidth(GPDMA_WORD);    /* escribe 32 bits */
                    /* SI=0, DI=0 (sin incremento), I=0 (sin interrupcion) */
    adc_lli.nextLLI = (uint32_t)&adc_lli;                 /* circular: se recarga solo */

    GPDMA_ChannelStart(GPDMA_CH_0);

    /* Recien ahora, con el canal de DMA escuchando, arrancamos la conversion en
       rafaga. Si el burst se enciende antes, el primer flanco de DONE ocurre sin
       que el DMA este listo, se pierde, DONE queda en 1 (overrun) y ya no hay mas
       flancos -> el DMA nunca dispara. Por eso el burst va ULTIMO. */
    ADC_BurstEnable();
}

uint32_t pot_get_speed(void) {
    /* El DMA mantiene adc_dma_buffer siempre fresco: solo lo leemos. */
    uint16_t valor  = (uint16_t)ADC_GDR_RESULT(adc_dma_buffer);  /* 0..4095 */
    if (valor>= 0 && valor <=1365){
    	return tabla_velocidades[0];
    }else if(  valor <=2730){
    	return tabla_velocidades[1];
    }else{
    	return tabla_velocidades[2];

    }

    return tabla_velocidades[0];
}
