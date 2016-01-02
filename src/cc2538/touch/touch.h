#ifndef TOUCH_H_
#define TOUCH_H_

#include <stdint.h>
#include <stdbool.h>

#define TOUCH_CAPACITIVE 0

typedef enum
{
    TOUCH_POINT,
    TOUCH_SLIDE_RIGHT,
    TOUCH_SLIDE_LEFT,
} touch_type_e;

void          touch_init          (void);
touch_type_e  touch_get_slide     (void);
touch_type_e  touch_get_any       (void);
touch_type_e  touch_get           (unsigned *down, unsigned *up);


#if TOUCH_CAPACITIVE
void          touch_cap_calibrate (void);
uint32_t      touch_cap_read      (bool init);
uint32_t*     touch_cap_ticks     (unsigned *count);
#endif

#endif
