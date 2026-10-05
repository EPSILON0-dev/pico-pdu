#include "pico_pdu.h"

#include <hardware/dma.h>
#include <hardware/pio.h>
#include <pico/error.h>

#include "hardware/clocks.h"
#include "uart_rx.pio.h"
#include "uart_tx.pio.h"

static struct
{
    bool valid;
    uint offset;
    uint users;
} pio_tx_program[NUM_PIOS] = {0};

static struct
{
    bool valid;
    uint offset;
    uint users;
} pio_rx_program[NUM_PIOS] = {0};

static inline void uart_tx_program_init(PIO pio, uint sm, uint offset, uint pin_tx, uint baud)
{
    // Tell PIO to initially drive output-high on the selected pin, then map PIO
    // onto that pin with the IO muxes.
    pio_sm_set_pins_with_mask64(pio, sm, 1ull << pin_tx, 1ull << pin_tx);
    pio_sm_set_pindirs_with_mask64(pio, sm, 1ull << pin_tx, 1ull << pin_tx);
    pio_gpio_init(pio, pin_tx);

    pio_sm_config c = uart_tx_program_get_default_config(offset);

    // OUT shifts to right, no autopull
    sm_config_set_out_shift(&c, true, false, 32);

    // We are mapping both OUT and side-set to the same pin, because sometimes
    // we need to assert user data onto the pin (with OUT) and sometimes
    // assert constant values (start/stop bit)
    sm_config_set_out_pins(&c, pin_tx, 1);
    sm_config_set_sideset_pins(&c, pin_tx);

    // We only need TX, so get an 8-deep FIFO!
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // SM transmits 1 bit per 8 execution cycles.
    float div = (float)clock_get_hz(clk_sys) / (8 * baud);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

int pio_uart_tx_create(pio_uart_tx_t* inst, uint tx_pin, uint baudrate)
{
    inst->tx_pin = tx_pin;
    inst->baudrate = baudrate;

    static pio_program_t empty_program = {0};

    // Try to get a SM for the uart
    bool result = pio_claim_free_sm_and_add_program(&empty_program, &inst->pio, &inst->sm, NULL);
    if (!result) return PICO_ERROR_GENERIC;

    // Load the program if not already loaded to the PIO
    if (!pio_tx_program[PIO_NUM(inst->pio)].valid)
    {
        pio_tx_program[PIO_NUM(inst->pio)].offset = pio_add_program(inst->pio, &uart_tx_program);
        pio_tx_program[PIO_NUM(inst->pio)].valid = true;
    }

    // Get the offset and setup GPIO
    inst->offset = pio_tx_program[PIO_NUM(inst->pio)].offset;
    pio_tx_program[PIO_NUM(inst->pio)].users++;
    pio_set_gpio_base(inst->pio, inst->tx_pin);

    // Claim a DMA channel
    int channel = dma_claim_unused_channel(true);
    if (channel < 0) return PICO_ERROR_GENERIC;
    inst->dma_channel = channel;

    // Setup the DMA channel for the SM
    const uint dreq_index = pio_get_dreq(inst->pio, inst->sm, true);
    dma_channel_config_t config = dma_channel_get_default_config(inst->dma_channel);
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
    channel_config_set_dreq(&config, dreq_index);
    dma_channel_set_config(inst->dma_channel, &config, false);
    dma_channel_set_write_addr(inst->dma_channel, &inst->pio->txf[inst->sm], false);

    // Initialize the SM
    uart_tx_program_init(inst->pio, inst->sm, inst->offset, inst->tx_pin, inst->baudrate);
    return PICO_OK;
}

int pio_uart_tx_destroy(pio_uart_tx_t* inst)
{
    // Unclaim the SM
    pio_sm_unclaim(inst->pio, inst->sm);

    // Decrease the users and remove the program if needed
    pio_tx_program[PIO_NUM(inst->pio)].users--;
    if (pio_tx_program[PIO_NUM(inst->pio)].users == 0)
    {
        pio_remove_program(inst->pio, &uart_tx_program, inst->offset);
        pio_tx_program[PIO_NUM(inst->pio)].valid = false;
    }
    return PICO_OK;
}

void pio_uart_tx_putc(pio_uart_tx_t* inst, char c)
{
    pio_sm_put_blocking(inst->pio, inst->sm, (uint32_t)c);
};

void pio_uart_tx_dma_puts(pio_uart_tx_t* inst, const char* str, size_t len)
{
    dma_channel_set_read_addr(inst->dma_channel, str, false);
    dma_channel_set_transfer_count(inst->dma_channel, len, true);
};

static inline void uart_rx_program_init(PIO pio, uint sm, uint offset, uint pin, uint baud)
{
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
    pio_gpio_init(pio, pin);
    gpio_pull_up(pin);

    pio_sm_config c = uart_rx_program_get_default_config(offset);
    sm_config_set_in_pins(&c, pin);  // for WAIT, IN
    sm_config_set_jmp_pin(&c, pin);  // for JMP
    // Shift to right, autopush disabled
    sm_config_set_in_shift(&c, true, false, 32);
    // Deeper FIFO as we're not doing any TX
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    // SM transmits 1 bit per 8 execution cycles.
    float div = (float)clock_get_hz(clk_sys) / (8 * baud);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

int pio_uart_rx_create(pio_uart_rx_t* inst, uint rx_pin, uint baudrate)
{
    inst->rx_pin = rx_pin;
    inst->baudrate = baudrate;

    static pio_program_t empty_program = {0};

    // Try to get a SM for the uart
    bool result = pio_claim_free_sm_and_add_program(&empty_program, &inst->pio, &inst->sm, NULL);
    if (!result) return PICO_ERROR_GENERIC;

    // Load the program if not already loaded to the PIO
    if (!pio_rx_program[PIO_NUM(inst->pio)].valid)
    {
        pio_rx_program[PIO_NUM(inst->pio)].offset = pio_add_program(inst->pio, &uart_rx_program);
        pio_rx_program[PIO_NUM(inst->pio)].valid = true;
    }

    // Get the offset and setup GPIO
    inst->offset = pio_rx_program[PIO_NUM(inst->pio)].offset;
    pio_rx_program[PIO_NUM(inst->pio)].users++;
    pio_set_gpio_base(inst->pio, inst->rx_pin);

    // Claim a DMA channel
    int channel = dma_claim_unused_channel(true);
    if (channel < 0) return PICO_ERROR_GENERIC;
    inst->dma_channel = channel;

    // Setup the DMA channel for the SM
    const uint dreq_index = pio_get_dreq(inst->pio, inst->sm, false);
    dma_channel_config_t config = dma_channel_get_default_config(inst->dma_channel);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
    channel_config_set_dreq(&config, dreq_index);
    dma_channel_set_config(inst->dma_channel, &config, false);
    dma_channel_set_read_addr(inst->dma_channel, ((io_rw_8*)&inst->pio->rxf[inst->sm]) + 3, false);

    // Initialize the SM
    uart_rx_program_init(inst->pio, inst->sm, inst->offset, inst->rx_pin, inst->baudrate);
    return PICO_OK;
}

int pio_uart_rx_destroy(pio_uart_rx_t* inst)
{
    // Unclaim the SM
    pio_sm_unclaim(inst->pio, inst->sm);

    // Decrease the users and remove the program if needed
    pio_rx_program[PIO_NUM(inst->pio)].users--;
    if (pio_rx_program[PIO_NUM(inst->pio)].users == 0)
    {
        pio_remove_program(inst->pio, &uart_rx_program, inst->offset);
        pio_rx_program[PIO_NUM(inst->pio)].valid = false;
    }
    return PICO_OK;
}

char pio_uart_rx_getc(pio_uart_rx_t* inst)
{
    io_rw_8* rxfifo_shift = (io_rw_8*)&inst->pio->rxf[inst->sm] + 3;
    while (pio_sm_is_rx_fifo_empty(inst->pio, inst->sm)) tight_loop_contents();
    return (char)*rxfifo_shift;
}

void pio_uart_rx_dma_set_buffer(pio_uart_rx_t* inst, void* buff_ptr, size_t buff_size)
{
    inst->buff_base = buff_ptr;
    inst->buff_size = buff_size;
    dma_channel_set_write_addr(inst->dma_channel, buff_ptr, false);
    dma_channel_set_transfer_count(inst->dma_channel, buff_size, true);
};
