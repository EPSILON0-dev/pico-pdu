#include <FreeRTOS.h>
#include <bsp/board_api.h>
#include <stdarg.h>
#include <task.h>
#include <tusb.h>

#include "pico_pdu.h"

void usb_task_func(void* _unused)
{
    (void)_unused;

    board_init();
    tusb_init();

    if (board_init_after_tusb)
    {
        board_init_after_tusb();
    }

    for (;;)
    {
        tud_task();
        vTaskDelay(1);
    }
}

static void usb_console_send_buff(const char* buff, size_t len)
{
    tud_cdc_n_write(USB_CDC_CONSOLE, buff, len);
    tud_cdc_n_write_flush(USB_CDC_CONSOLE);
}

void usb_console_putchar(void* data, char ch, bool is_last)
{
    (void)data;
    static char buff[128];
    static size_t index = 0;

    buff[index++] = ch;
    if (is_last || index >= sizeof(buff) - 1)
    {
        usb_console_send_buff(buff, index);
        index = 0;
    }
}

void usb_console_printf(const char* fmt, ...)
{
    char buff[256];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff) - 1, fmt, args);
    va_end(args);

    usb_console_send_buff(buff, strlen(buff));
}

void tud_cdc_rx_cb(uint8_t itf)
{
    uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE];
    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

    if (itf == USB_CDC_CONSOLE)
    {
        console_usb_cb((const char*)buf, count);
    }
}

void usb_logger_send_buff(const char* buff, size_t len)
{
    tud_cdc_n_write(USB_CDC_LOG_COLLECTOR, buff, len);
}

void usb_logger_flush(void)
{
    tud_cdc_n_write_flush(USB_CDC_LOG_COLLECTOR);
}
