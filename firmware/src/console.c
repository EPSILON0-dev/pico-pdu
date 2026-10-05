#include <FreeRTOS.h>
#include <pico/bootrom.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>

#include "lib/embedded_cli.h"
#include "pico_pdu.h"

static struct embedded_cli usb_cli;

typedef void (*printf_func_t)(const char* fmt, ...);

void console_command(printf_func_t printf, int argc, char** argv);
void cmd_help(printf_func_t printf, int argc, char** argv);
void cmd_dfu(printf_func_t printf, int argc, char** argv);
void cmd_relay(printf_func_t printf, int argc, char** argv);
void cmd_state_raw(printf_func_t printf, int argc, char** argv);
void cmd_state(printf_func_t printf, int argc, char** argv);
void cmd_logger(printf_func_t printf, int argc, char** argv);

static struct
{
    char* cmd;
    char* help;
    void (*handler)(printf_func_t printf, int argc, char** argv);
} commands[] = {
    {"help", "Print help", cmd_help},
    {"dfu", "Enter the bootloader", cmd_dfu},
    {"relay", "Turn on/off relay channel", cmd_relay},
    {"state_raw", "Report the device state in an easily parsable format", cmd_state_raw},
    {"state", "Report the device state in a human-readable format", cmd_state},
    {"log", "Change logger settings", cmd_logger},
};

void console_usb_cb(const char* buff, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        int c = buff[i];
        if (embedded_cli_insert_char(&usb_cli, c))
        {
            int cli_argc;
            char** cli_argv;
            cli_argc = embedded_cli_argc(&usb_cli, &cli_argv);
            console_command(usb_console_printf, cli_argc, cli_argv);
            embedded_cli_prompt(&usb_cli);
        }
    }
}

void console_usb_init(void)
{
    embedded_cli_init(&usb_cli, "pico-pdu> ", usb_console_putchar, NULL);
    embedded_cli_prompt(&usb_cli);
}

void console_command(printf_func_t printf, int argc, char** argv)
{
    if (argc < 1)
    {
        printf("Unknown error\r\n");
        return;
    }

    for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        if (strcmp(commands[i].cmd, argv[0]) == 0)
        {
            commands[i].handler(printf, argc, argv);
            printf("\r\n");
            return;
        }
    }

    printf("Unknown command '%s', use 'help' for help\r\n", argv[0]);
}

void cmd_help(printf_func_t printf, int argc, char** argv)
{
    for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        printf("  %-10s - %s\r\n", commands[i].cmd, commands[i].help);
    }

    return;
}

void cmd_dfu(printf_func_t printf, int argc, char** argv)
{
    (void)argc;
    (void)argv;
    printf("Entering bootloader...\r\n");
    vTaskDelay(1000);
    reset_usb_boot(0, 0);
}

void cmd_relay(printf_func_t printf, int argc, char** argv)
{
    if (argc < 3)
    {
        printf("Usage: relay <on|off> <channel>\r\n");
        return;
    }

    int channel = atoi(argv[2]);
    if (channel <= 0 || channel > NUM_RELAYS)
    {
        printf("Invalid channel: %s (expected: 0-%d)\r\n", argv[2], NUM_RELAYS - 1);
        return;
    }

    if (strcmp(argv[1], "on") == 0)
    {
        relay_on(channel);
    }
    else if (strcmp(argv[1], "off") == 0)
    {
        relay_off(channel);
    }
    else
    {
        printf("Usage: relay <on|off> <channel>\r\n");
    }
}

void cmd_state(printf_func_t printf, int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("Channel states:\r\n");
    for (int i = 0; i < NUM_RELAYS; i++)
    {
        const char* state = relay_get_state(i) == RELAY_ON ? "ON" : "OFF";

        if (i < NUM_MONITORS)
        {
            float power = monitor_read_channel_microamps(i) / 1000.0f;
            if (power < 1.0f)
            {
                printf("  %2d: %s %.2fmA\r\n", i, state, power);
            }
            else
            {
                printf("  %2d: %s %.2fA\r\n", i, state, power / 1000.0f);
            }
        }
        else
        {
            printf("  %2d: %s (not monitored)\r\n", i, state);
        }
    }
}

void cmd_state_raw(printf_func_t printf, int argc, char** argv)
{
    (void)argc;
    (void)argv;

    for (int i = 0; i < NUM_RELAYS; i++)
    {
        const char* state = relay_get_state(i) == RELAY_ON ? "ON" : "OFF";
        printf("RELAY %d %s\r\n", i, state);
    }

    for (int i = 0; i < NUM_MONITORS; i++)
    {
        int power = monitor_read_channel_microamps(i);
        printf("MONITOR %d %d\r\n", i, power);
    }

    for (int i = 0; i < NUM_LOGGERS; i++)
    {
        int power = monitor_read_channel_microamps(i);
        printf("LOGGER (not implemented)\r\n");
    }
}

void cmd_logger(printf_func_t printf, int argc, char** argv)
{
    (void)argc;
    (void)argv;
    printf("Not implemented\r\n");
}
