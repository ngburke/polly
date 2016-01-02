#ifndef CORE_H_
#define CORE_H_

#include <stdint.h>
#include <stdbool.h>

#define PACKET_BYTES  64

// Transfer functions

void     core_init           (void);
bool     core_unlocked       (void);
bool     core_inpacket       (uint8_t *inpacket);
bool     core_outpacket_ready(void);
uint8_t* core_outpacket      (void);
void     core_process        (void);

// State transition functions

void     core_load_ctx       (void);
void     core_enter_pin      (bool failed);
void     core_generate_pin   (void);
void     core_generate_seed  (void);
void     core_ready          (void);
void     core_gather_prevtx  (void);
void     core_ready_to_sign  (void);
bool     core_sign           (void);
void     core_sign_progress  (void);


#endif
