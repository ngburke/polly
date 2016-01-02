#ifndef MEMLCD_H_
#define MEMLCD_H_

#include <stdint.h>
#include <stdbool.h>

void memlcd_init     (void);
void memlcd_refresh  (void);
void memlcd_clear    (void);
void memlcd_sendline (const uint32_t linenum, const uint8_t *data);

#endif
