#ifndef STORE_H_
#define STORE_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    STORE_OK,
    STORE_ERR_INVALID_PIN,
} store_status_e;

#define STORE_KEYSLOT 0

bool           store_present (void);
store_status_e store_load    (const unsigned keyslot);
void           store_create  (const unsigned keyslot);
void           store_wipe    (void);

#endif // STORE_H_
