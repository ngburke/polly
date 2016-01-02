#ifndef SCREEN_H_
#define SCREEN_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    LIGHT_OFF,
    LIGHT_ON,
    LIGHT_ERR,
} light_e;

void screen_init             (void);
void screen_banner           (char str[]);
void screen_idle             (void);
bool screen_send             (const uint8_t *output_addr, uint64_t btc, uint64_t fee);
void screen_signing_progress (unsigned done, unsigned total);
void screen_signing          (uint64_t btc, uint64_t fee);
void screen_tx               (light_e state);
void screen_rx               (light_e state);
void screen_enter_pin        (bool failed);
void screen_generate_seed    (void);
void screen_generate_pin     (void);

void screen_captouch_debug   (void);

#endif
