#include <FreeRTOS.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/regs/intctrl.h>
#include <hardware/structs/dma.h>
#include <pico/stdlib.h>
#include <pico_pdu.h>
#include <stdio.h>
#include <string.h>
#include <task.h>

void blink_task_func(void* _unused)
{
    gpio_init(LED_GPIO);
    gpio_set_dir(LED_GPIO, GPIO_OUT);

    for (;;)
    {
        gpio_put(LED_GPIO, 1);
        vTaskDelay(500);
        gpio_put(LED_GPIO, 0);
        vTaskDelay(500);
    }
}

static pio_uart_rx_t piouart0;
static char uart0_buff[1024];
static char send_buff[1024];

char index_circular_buffer(const char* buffer, size_t buffsz, size_t index)
{
    return buffer[index % buffsz];
}

void send_circular_buffer(const char* buffer, size_t buffsz, uint send_offset, uint send_length)
{
    bool wraps = (send_offset % buffsz) + send_length > buffsz;

    if (wraps)
    {
        size_t chunk1_len = buffsz - send_length;
        usb_logger_send_buff(buffer + (send_offset % buffsz), chunk1_len);
        size_t chunk2_len = (send_offset + send_length) % buffsz;
        usb_logger_send_buff(buffer, chunk2_len);
        usb_logger_flush();
    }
    else
    {
        usb_logger_send_buff(buffer + (send_offset % buffsz), send_length);
        usb_logger_flush();
    }
}

void send_task(void* ptr_uart)
{
    pio_uart_rx_t* uart = ptr_uart;
    const uint dma_channel = uart->dma_channel;
    char* buffer = uart->buff_base;
    size_t buffsz = uart->buff_size;

    uint line_start = 0;
    uint last_checked = 0;
    for (;;)
    {
        uint current_index = dma_hw->ch[dma_channel].write_addr - (uintptr_t)buffer;
        uint previous_index = (last_checked % buffsz);
        uint new_bytes = (current_index - previous_index) % buffsz;
        uint send_length = 0;

        // Check if there's a line feed in the new bytes
        for (uint i = 0; i < new_bytes; i++)
        {
            uint index = last_checked + i;
            char c = index_circular_buffer(buffer, buffsz, index);
            if (c != '\n') continue;

            send_length = index - line_start;
        }

        // Check if we exceeded the max line length
        if (!send_length && last_checked + new_bytes - line_start > MAX_LOG_LEN)
        {
            send_length = MAX_LOG_LEN;
        }

        // Send if needed
        if (send_length)
        {
            send_circular_buffer(buffer, buffsz, line_start, send_length);
        }

        // Increment the counters
        line_start += send_length;
        last_checked += new_bytes;

        // Delay if we didn't send anything (we might have gotten multiple lines, in that case we should not wait but
        // send them immediately)
        if (send_length == 0)
        {
            vTaskDelay(1);
        }
        else
        {
            // TODO : yield
        }
    }
}

void send_task_simple(void* ptr_uart)
{
    for (;;)
    {
        pio_uart_rx_t* uart = ptr_uart;
        usb_logger_send_buff(uart->buff_base, uart->buff_size);
        usb_logger_flush();
        vTaskDelay(1000);
    }
}

void dma_handler(void)
{
    pio_uart_rx_dma_set_buffer(&piouart0, piouart0.buff_base, piouart0.buff_size);
    dma_irqn_acknowledge_channel(0, piouart0.dma_channel);
}

int main()
{
    console_usb_init();

    static TaskHandle_t blink_task_handle;
    xTaskCreate(blink_task_func, "BlinkThread", 1024, NULL, 1, &blink_task_handle);

    static TaskHandle_t usb_task_handle;
    xTaskCreate(usb_task_func, "USBThread", 4096, NULL, 1, &usb_task_handle);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    hard_assert(pio_uart_rx_create(&piouart0, 1, 115200) == PICO_OK);
    pio_uart_rx_dma_set_buffer(&piouart0, uart0_buff, sizeof(uart0_buff) / sizeof(*uart0_buff));
    dma_channel_set_irq0_enabled(piouart0.dma_channel, true);

    static TaskHandle_t send_task_handle;
    xTaskCreate(send_task, "SendThread", 1024, (void*)(&piouart0), 1, &send_task_handle);

    vTaskStartScheduler();

    return 0;
}
