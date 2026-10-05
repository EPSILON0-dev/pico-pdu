#include <stdlib.h>

#include "pico_pdu.h"

int monitor_read_channel_microamps(int channel)
{
    if (channel < 0 || channel >= NUM_MONITORS) return 0;
    return rand() % 5000 * 1000;
}