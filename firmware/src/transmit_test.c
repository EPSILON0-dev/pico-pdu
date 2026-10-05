#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pio_uart.h"
#include "semphr.h"
#include "task.h"

#define TX_COUNT 8
#define TX_PIN_BASE 0
#define TX_BAUDRATE 115200
#define LED_PIN 25

#define TASK_PRIORITY_BASIC tskIDLE_PRIORITY + 1
#define TASK_STACK_SIZE_BASIC configMINIMAL_STACK_SIZE

#if TX_COUNT > NUM_PIOS * NUM_PIO_STATE_MACHINES
#error "TX_COUNT can't be larger than state machine count in all PIOs"
#endif

static const char* boot_logs[] = {
    "[    0.000000] firmware: starting boot sequence\n",
    "[    0.004812] cpu0: detected ARMv7 processor, revision r1p0\n",
    "[    0.007193] memory: 32 MiB available\n",
    "[    0.009442] clocksource: arch_sys_counter initialized\n",
    "[    0.013801] uart0: 115200 8N1 console enabled\n",
    "[    0.018277] gpio: initializing 24 pins\n",
    "[    0.021904] regulator-vcore: 900mV enabled\n",
    "[    0.025612] regulator-vio: 1800mV enabled\n",
    "[    0.029031] regulator-vdd: 3300mV enabled\n",
    "[    0.033719] storage: probing eMMC controller\n",
    "[    0.038004] mmc0: new high speed MMC card at address 0001\n",
    "[    0.041882] mmcblk0: 7.28 GiB detected\n",
    "[    0.045101] mmcblk0p1: boot partition\n",
    "[    0.049672] mmcblk0p2: root filesystem\n",
    "[    0.053884] fs: mounting /dev/mmcblk0p2 as ext4\n",
    "[    0.059121] fs: mounted root filesystem read-write\n",
    "[    0.062901] entropy: collecting initial seed\n",
    "[    0.067331] watchdog0: timeout set to 30 seconds\n",
    "[    0.071842] net: initializing ethernet controller\n",
    "[    0.076005] eth0: PHY detected at address 1\n",
    "[    0.081772] eth0: link negotiation started\n",
    "[    0.104228] eth0: link up, 1000Mbps full-duplex\n",
    "[    0.109473] dhcp: requesting address on eth0\n",
    "[    0.182991] dhcp: lease acquired, address=192.168.1.215\n",
    "[    0.187301] ntp: waiting for network time\n",
    "[    0.192882] time: synchronized to 2026-09-16T19:23:01Z\n",
    "[    0.198771] rng: hardware random generator ready\n",
    "[    0.204115] usb: controller initialized\n",
    "[    0.209771] usb0: virtual serial device connected\n",
    "[    0.214331] logd: ring buffer initialized, size=65536\n",
    "[    0.219004] logd: aggregator started\n",
    "[    0.224871] scheduler: starting worker pool (4 threads)\n",
    "[    0.230441] storage-monitor: service started\n",
    "[    0.235912] thermal: sensor cpu0 registered\n",
    "[    0.241103] thermal: sensor board registered\n",
    "[    0.247821] i2c0: bus initialized at 400kHz\n",
    "[    0.253774] i2c0: found device 0x48\n",
    "[    0.258992] sensor: temperature sensor initialized\n",
    "[    0.264113] spi0: mode=0, max-frequency=10000000\n",
    "[    0.269881] spi0: peripheral attached at CS0\n",
    "[    0.275001] pwm: initializing fan controller\n",
    "[    0.280472] fan0: controller online\n",
    "[    0.285119] led0: status LED registered\n",
    "[    0.290004] led0: trigger=heartbeat\n",
    "[    0.295772] config: loading /etc/device.conf\n",
    "[    0.301221] config: 17 settings loaded\n",
    "[    0.307912] auth: local authentication service started\n",
    "[    0.313551] sshd: listening on 0.0.0.0:22\n",
    "[    0.318772] rpc: control socket created at /run/control.sock\n",
    "[    0.324901] telemetry: collector started\n",
    "[    0.330114] telemetry: interval=1000ms destination=local\n",
    "[    0.336821] scheduler: idle policy=balanced\n",
    "[    0.341772] service-manager: dependency graph resolved\n",
    "[    0.347101] service-manager: starting remaining services\n",
    "[    0.352991] syslog: accepting messages from /dev/log\n",
    "[    0.358441] logger: console backend started\n",
    "[    0.364812] logger: persistent backend unavailable, continuing\n",
    "[    0.370115] backup: no previous checkpoint found\n",
    "[    0.375882] update: firmware version 2.7.14\n",
    "[    0.381119] update: no pending update\n",
    "[    0.386771] health: all mandatory checks passed\n",
    "[    0.392002] health: cpu=ok memory=ok storage=ok network=ok\n",
    "[    0.398551] rpc: accepting connections\n",
    "[    0.403882] shell: interactive console ready\n",
    "[    0.409114] system: entering normal operation\n",
    "[    0.414772] system: boot completed in 414 ms\n",
    "[    0.421003] service-manager: service started: logger\n",
    "[    0.426771] service-manager: service started: telemetry\n",
    "[    0.432112] service-manager: service started: network\n",
    "[    0.438901] service-manager: service started: storage\n",
    "[    0.444117] service-manager: service started: watchdog\n",
    "[    0.450882] service-manager: service started: shell\n",
    "[    0.456003] status: READY\n",
    "[    0.512114] warning: received truncated packet; expected=128 actual=17\n",
    "[    0.518772] retry: operation failed, errno=11, attempt=1/3\n",
    "[    0.524991] retry: operation failed, errno=11, attempt=2/3\n",
    "[    0.531114] retry: operation succeeded on attempt=3\n",
    "[    0.536882] buffer: message length=0\n",
    "[    0.542113] buffer: next message begins immediately after empty record\n",
    // NOLINTNEXTLINE
    "[    0.548771] diagnostic: "
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAA\n",
    "[    0.554112] diagnostic: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n",
    "[    0.559881] diagnostic: !!! ??? ::: [[[ ]]] {{{ }}} <<< >>> !!!\n",
    "[    0.604772] network: rx packet accepted src=00:11:22:33:44:55 len=1514\n",
    "[    0.610113] network: rx packet rejected reason=bad_checksum\n",
    "[    0.615881] network: tx queue depth=7 dropped=0\n",
    "[    0.621114] watchdog: heartbeat received\n",
    "[    0.626772] watchdog: heartbeat received\n",
    "[    0.632113] watchdog: heartbeat received\n",
    "[    0.637881] scheduler: worker=0 jobs=17 runtime=231ms\n",
    "[    0.643114] scheduler: worker=1 jobs=21 runtime=198ms\n",
    "[    0.648772] scheduler: worker=2 jobs=19 runtime=205ms\n",
    "[    0.654113] scheduler: worker=3 jobs=23 runtime=219ms\n",
    "[    0.659881] memory: heap=18432/32768 bytes\n",
    "[    0.665114] memory: largest-free-block=8192 bytes\n",
    "[    0.670772] storage: queue=0 pending=0 errors=0\n",
    "[    0.676113] telemetry: sample cpu=17.2% temp=43.1C voltage=3.301V\n",
    "[    0.681881] telemetry: sample cpu=19.8% temp=43.4C voltage=3.299V\n",
    "[    0.687114] telemetry: sample cpu=15.1% temp=43.0C voltage=3.300V\n",
    "[    0.692772] rpc: client connected fd=7 peer=192.168.1.100:48211\n",
    "[    0.698113] rpc: request method='get_status' id=42\n",
    "[    0.703881] rpc: response id=42 status=200\n",
    "[    0.709114] rpc: client disconnected fd=7\n",
    "[    0.714772] logd: dropped=0 queued=3 flushed=3\n",
    "[    0.720113] logd: rotation not required\n",
    "[    0.725881] service-manager: no pending jobs\n",
    "[    0.731114] system: uptime=731ms\n",
    "[    0.736772] system: idle\n",
    "[    0.742113] heartbeat: alive\n",
    "[    0.747881] heartbeat: alive\n",
    "[    0.753114] heartbeat: alive\n",
    "[    0.758772] shutdown-test: this is intentionally the final record\n",
    "[    0.592772] diagnostic: deliberately-long-record-begins: "
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
    "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
    "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris "
    "nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in "
    "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
    "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
    "culpa qui officia deserunt mollit anim id est laborum. "
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
    "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
    "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris "
    "nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in "
    "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
    "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
    "culpa qui officia deserunt mollit anim id est laborum. "
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
    "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
    "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris "
    "nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in "
    "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
    "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
    "culpa qui officia deserunt mollit anim id est laborum. "
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
    "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
    "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris "
    "nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in "
    "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
    "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
    "culpa qui officia deserunt mollit anim id est laborum. "
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
    "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
    "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris "
    "nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in "
    "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
    "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
    "culpa qui officia deserunt mollit anim id est laborum.\n",
};

static struct
{
    char* text;
    uint len;
} spicy_boot_logs[] = {
    {"[    0.581881] diagnostic: quotes=\"single='double=\" backslash=\\ pipe=|\n", 71},
    {"[    0.587114] diagnostic: brackets=()[]{}<> angle=<0x20> control=\x1f\n", 68},
    {"[    0.404771] status: temperatura=42°C, tensão=3.30V, état=normal\n", 70},
    {"[    0.461772] status: temperatura=42°C, tensão=3.30V, état=normal\n", 70},
    {"[    0.467114] unicode: Zażółć gęślą jaźń / Ελληνικά / 日本語 / 中文 / 🚀\n", 98},
    {"[    0.472881] serial: RX frame \x01\x02\x03\x04\x05\x06\x07\x08\n", 41},
    {"[    0.478221] serial: unexpected byte \x7f received from uart0\n", 61},
    {"[    0.483992] parser: field separator='|' tab=\x09 cr=\x0d\n", 54},
    {"[    0.489114] parser: malformed record: \xff\xfe\xfa\x80\x81\x82\n", 48},
    {"[    0.494771] console: escape sequence detected: \x1b[2J\x1b[H\n", 58},
    {"[    0.500221] console: raw control bytes: \x00\x01\x02\x03\x1b\x1f\x7f\n", 51},
    {"[    0.506881] warning: device name contains unusual character: 'sensor-\xc3\xa4-01'\n", 79},
    {"[    0.598114] diagnostic: \n"
     "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
     "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
     " normal-text-after-controls"
     "\x7f\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f\n"
     " end\n",
        110},
    {"[    0.565114] diagnostic: embedded-nul=\x00-after-nul-is-still-part-of-record\n", 76},
    {"[    0.570772] diagnostic: DEL=\x7f; control=\x01\x02\x03\x04\x05\x06\x07\n", 50},
    {"[    0.576113] diagnostic: utf8=Łukasz_żółć_東京_Москва_Αθήνα_😀_🦀\n", 90},
};

#define NUM_BOOT_LOGS (sizeof(boot_logs) / sizeof(boot_logs[0]))
#define NUM_SPICY_LOGS (sizeof(spicy_boot_logs) / sizeof(spicy_boot_logs[0]))

static struct
{
    uint messages_sent;
    uint bytes_sent;
    pio_uart_tx_t pio_uart_tx;
    bool restart_pending;
    SemaphoreHandle_t sem;
    TaskHandle_t restart_task;
} transmitters[TX_COUNT] = {0};

#define EVENT_RESTART_PENDING_BIT 0

void dma_handler(void)
{
    uint irq_flags = dma_hw->ints0;
    uint irq_index = 0;
    while (irq_index < NUM_DMA_CHANNELS && !(irq_flags & 1))
    {
        irq_index++;
        irq_flags >>= 1;
    }

    for (int i = 0; i < TX_COUNT; i++)
    {
        if (transmitters[i].pio_uart_tx.dma_channel == irq_index)
        {
            xSemaphoreGive(transmitters[i].sem);
        }
    }

    dma_hw->ints0 = 1 << irq_index;
}

void restart_transmitter(uint index)
{
    if (rand() % 7 == 0)
    {
        const uint log_index = rand() % NUM_SPICY_LOGS;
        const uint log_len = spicy_boot_logs[log_index].len;
        const char* log_text = spicy_boot_logs[log_index].text;
        transmitters[index].bytes_sent += log_len;
        pio_uart_tx_dma_puts(&transmitters[index].pio_uart_tx, log_text, log_len);
    }
    else
    {
        const uint log_index = rand() % NUM_BOOT_LOGS;
        const uint log_len = strlen(boot_logs[log_index]);
        transmitters[index].bytes_sent += log_len;
        pio_uart_tx_dma_puts(&transmitters[index].pio_uart_tx, boot_logs[log_index], log_len);
    }
    transmitters[index].messages_sent++;
    transmitters[index].restart_pending = false;
}

void restart_task(void* transmitter_index)
{
    uintptr_t index = (int)transmitter_index;

    for (;;)
    {
        xSemaphoreTake(transmitters[index].sem, 1000);
        restart_transmitter(index);
    }
}

void stats_task(void* _unused)
{
    (void)_unused;

    for (;;)
    {
        for (int i = 0; i < TX_COUNT; i++)
        {
            printf("TX%d: %8d msgs, %8d bytes\n", i, transmitters[i].messages_sent, transmitters[i].bytes_sent);
        }
        printf("\n");
        vTaskDelay(5000);
    }
}

void blink_task_func(void* _unused)
{
    (void)_unused;

    bool state = true;
    for (;;)
    {
        gpio_put(LED_PIN, 1);
        vTaskDelay(500);
        gpio_put(LED_PIN, 0);
        vTaskDelay(500);
    }
}

int main()
{
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    for (int i = 0; i < TX_COUNT; i++)
    {
        hard_assert(pio_uart_tx_create(&transmitters[i].pio_uart_tx, TX_PIN_BASE + i, TX_BAUDRATE) == PICO_OK);
        dma_channel_set_irq0_enabled(transmitters[i].pio_uart_tx.dma_channel, true);
        transmitters[i].sem = xSemaphoreCreateBinary();
        hard_assert(transmitters[i].sem != NULL);
        xTaskCreate(restart_task, "RestartThread", TASK_STACK_SIZE_BASIC, (void*)i, TASK_PRIORITY_BASIC,
            &transmitters[i].restart_task);
    }

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    for (int i = 0; i < TX_COUNT; i++)
    {
        xSemaphoreGive(transmitters[i].sem);
    }

    static TaskHandle_t stats_task_handle;
    xTaskCreate(stats_task, "StatsThread", TASK_STACK_SIZE_BASIC, NULL, TASK_PRIORITY_BASIC, &stats_task_handle);
    static TaskHandle_t blink_task_handle;
    xTaskCreate(blink_task_func, "BlinkThread", TASK_STACK_SIZE_BASIC, NULL, TASK_PRIORITY_BASIC, &blink_task_handle);

    vTaskStartScheduler();
}
