/*
 * comms.c
 * Comandos por UART0 a 9600 8N1. Recepcion por interrupcion (UART0_IRQHandler):
 * cada byte se acumula en una linea; al recibir Enter (\r o \n) se marca lista,
 * y comms_task() la parsea y ejecuta.
 *
 * Driver mejorado: UART_PinConfig(UART_TXx/RXx), UART_Init(U0, &cfg),
 * UART_FIFOConfig, UART_TxEnable, UART_IntConfig(.., UART_INT_RBR, ..),
 * UART_Receive(.., NONE_BLOCKING) y UART_Send(.., BLOCKING).
 */

#include "comms.h"

#include "motor.h"
#include "encoder.h"
#include "homing.h"

#include "lpc17xx_uart.h"
#include <string.h>

#define UART_BAUD   9600u
#define LINE_MAX    32

/* UART0 tiene su propio tipo (LPC_UART0_TypeDef) en este LPC17xx.h, pero el
 * driver espera el tipo generico LPC_UART_TypeDef. Los registros comunes son
 * identicos, asi que casteamos una sola vez. */
#define U0  ((LPC_UART_TypeDef *) LPC_UART0)

/* ---------- Buffer de recepcion (lo toca la ISR) ---------- */
static volatile char    line[LINE_MAX];
static volatile uint8_t idx       = 0;
static volatile uint8_t lineReady = 0;

/* ====================================================================== */

void comms_init(void) {
    UART_PinConfig(UART_TX0_P0_2);     /* P0.2 -> TXD0 */
    UART_PinConfig(UART_RX0_P0_3);     /* P0.3 -> RXD0 */

    UART_CFG_T cfg;
    cfg.baudRate = UART_BAUD;
    cfg.parity   = UART_PARITY_NONE;
    cfg.dataBits = UART_DBITS_8;
    cfg.stopBits = UART_STOPBIT_1;
    UART_Init(U0, &cfg);

    UART_FIFO_CFG_T fifo;
    fifo.resetRxBuf = ENABLE;
    fifo.resetTxBuf = ENABLE;
    fifo.dmaMode    = DISABLE;
    fifo.level      = UART_FIFO_TRGLEV0;   /* interrumpe con 1 caracter */
    UART_FIFOConfig(U0, &fifo);

    UART_TxEnable(U0);

    UART_IntConfig(U0, UART_INT_RBR, ENABLE);  /* RX data available */
    UART_IntConfig(U0, UART_INT_RLS, ENABLE);  /* errores de linea  */

    NVIC_SetPriority(UART0_IRQn, 2);       /* por debajo de motor y topes */
    NVIC_EnableIRQ(UART0_IRQn);
}

/* ---------- Helpers de envio ---------- */

static void send_str(const char *s) {
    UART_Send(U0, (const uint8_t *)s, (uint32_t)strlen(s), BLOCKING);
}

/* Envia un float como "ddd.dd" formateando a mano (sin stdio/sprintf, que
 * arrastra semihosting). */
static void send_mm(float v) {
    char buf[24];
    int  i = 0;

    if (v < 0.0f) { buf[i++] = '-'; v = -v; }

    int32_t whole = (int32_t)v;
    int32_t frac  = (int32_t)((v - (float)whole) * 100.0f + 0.5f);  /* 2 decimales */
    if (frac >= 100) { whole++; frac -= 100; }

    /* parte entera (se arma al reves y se vuelca) */
    char tmp[12];
    int  t = 0;
    if (whole == 0) {
        tmp[t++] = '0';
    } else {
        while (whole > 0) { tmp[t++] = (char)('0' + (whole % 10)); whole /= 10; }
    }
    while (t > 0) buf[i++] = tmp[--t];

    /* parte decimal, siempre 2 digitos */
    buf[i++] = '.';
    buf[i++] = (char)('0' + (frac / 10));
    buf[i++] = (char)('0' + (frac % 10));
    buf[i]   = '\0';

    send_str(buf);
}

/* ---------- Parseo de numero (acepta '.' o ',' como decimal) ---------- */

static float parse_float(const volatile char *s) {
    while (*s == ' ' || *s == '\t') s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;

    float val = 0.0f;
    while (*s >= '0' && *s <= '9') { val = val * 10.0f + (*s - '0'); s++; }
    if (*s == '.' || *s == ',') {
        s++;
        float scale = 0.1f;
        while (*s >= '0' && *s <= '9') { val += (*s - '0') * scale; scale *= 0.1f; s++; }
    }
    return neg ? -val : val;
}

/* ---------- Procesa una linea recibida ---------- */

void comms_task(void) {
    if (!lineReady) return;

    char c = line[0];
    if (c == 'h' || c == 'H') {
        send_str("Homing...\r\n");
        homing_run();
        send_str("Listo. Posicion = 0\r\n");
    }
    else if (c == 'p' || c == 'P') {
        send_str("Motor: ");   send_mm(motor_position_mm());    send_str(" mm   ");
        send_str("Encoder: "); send_mm(encoder_posicion_mm()); send_str(" mm\r\n");
    }
    else {
        float mm = parse_float(line);
        motor_goto_mm(mm);
        send_str("OK -> "); send_mm(mm); send_str(" mm\r\n");
    }

    lineReady = 0;
    idx       = 0;
}

/* ---------------------------- ISR de UART0 ---------------------------- */

void UART0_IRQHandler(void) {
    uint32_t id = UART_GetIntId(U0) & UART_IIR_INTID_MASK;

    if (id == UART_IIR_INTID_RLS) {        /* error de linea: leer LSR lo limpia */
        UART_GetLineStatus(U0);
        return;
    }

    if (id == UART_IIR_INTID_RDA || id == UART_IIR_INTID_CTI) {
        uint8_t ch;
        while (UART_Receive(U0, &ch, 1, NONE_BLOCKING) == 1) {
            if (lineReady) continue;        /* hay una linea sin procesar: descartar */
            if (ch == '\r' || ch == '\n') {
                if (idx > 0) { line[idx] = '\0'; lineReady = 1; }
            } else if (idx < LINE_MAX - 1) {
                line[idx++] = (char)ch;
            }
        }
    }
}
