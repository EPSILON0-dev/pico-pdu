#include "pico_pdu.h"

static enum ERelayState relays_state[NUM_RELAYS] = {0};

void relay_init(void)
{
    (void)0;  // Do nothing
}

void relay_on(int channel)
{
    if (channel < 0 || channel >= NUM_RELAYS) return;
    relays_state[channel] = RELAY_ON;
}

void relay_off(int channel)
{
    if (channel < 0 || channel >= NUM_RELAYS) return;
    relays_state[channel] = RELAY_OFF;
}

enum ERelayState relay_get_state(int channel)
{
    if (channel < 0 || channel >= NUM_RELAYS) return RELAY_ERROR;
    return relays_state[channel];
}
