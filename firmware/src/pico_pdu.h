#ifndef PIO_UART_H
#define PIO_UART_H

#include <hardware/pio.h>

#define LED_GPIO 25
#define MAX_LOG_LEN 256
#define DMA_IRQ_INDEX 0
#define NUM_MONITORS 8
#define NUM_LOGGERS 12
#define NUM_RELAYS 16
#define USB_CDC_CONSOLE 0
#define USB_CDC_LOG_COLLECTOR 1

enum ERelayState
{
    RELAY_OFF = 0,
    RELAY_ON = 1,
    RELAY_ERROR = 2
};

typedef struct
{
    PIO pio;
    uint sm;
    uint offset;
    uint tx_pin;
    uint baudrate;
    uint dma_channel;
} pio_uart_tx_t;

typedef struct
{
    PIO pio;
    uint sm;
    uint offset;
    uint rx_pin;
    uint baudrate;
    uint dma_channel;
    void* buff_base;
    uint buff_size;
} pio_uart_rx_t;

int pio_uart_tx_create(pio_uart_tx_t* inst, uint tx_pin, uint baudrate);
int pio_uart_tx_destroy(pio_uart_tx_t* inst);
void pio_uart_tx_putc(pio_uart_tx_t* inst, char c);
void pio_uart_tx_dma_puts(pio_uart_tx_t* inst, const char* str, size_t len);

int pio_uart_rx_create(pio_uart_rx_t* inst, uint rx_pin, uint baudrate);
int pio_uart_rx_destroy(pio_uart_rx_t* inst);
char pio_uart_rx_getc(pio_uart_rx_t* inst);
void pio_uart_rx_dma_set_buffer(pio_uart_rx_t* inst, void* buff_ptr, size_t buff_size);

void usb_task_func(void* _unused);
void usb_console_putchar(void* data, char ch, bool is_last);
void usb_console_printf(const char* fmt, ...);
void usb_logger_send_buff(const char* buff, size_t len);
void usb_logger_flush(void);

void console_usb_cb(const char* buff, size_t len);
void console_usb_init(void);

void relay_init(void);
void relay_on(int channel);
void relay_off(int channel);
enum ERelayState relay_get_state(int channel);

int monitor_read_channel_microamps(int channel);

#endif