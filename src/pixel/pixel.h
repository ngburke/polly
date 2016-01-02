#ifndef PIXEL_H_
#define PIXEL_H_

#include <stdint.h>
#include <stdbool.h>

#define SCREEN_WIDTH_BYTES  16
#define SCREEN_WIDTH_BITS   (SCREEN_WIDTH_BYTES * 8)
#define SCREEN_HEIGHT_BITS  128
#define CENTER              0xFD
#define CENTER_LEFT         0xFE
#define CENTER_RIGHT        0xFF

#define ASCII_CODE_START    32

void    pixel_init        (void (*snd)(const uint32_t, const uint8_t*), void (*clr)(void));

// Returns the new bit offset in the screen buffer after writing the string. bit_offset can be 'center'
uint8_t  pixel_str         (const char *str, const uint8_t line, const uint8_t bit);
unsigned pixel_str_width   (const char *str);
void     pixel_rect        (const uint8_t line, const uint8_t lines, const uint8_t bit, const uint8_t bits);
void     pixel_clear_lines (const unsigned start, const unsigned lines);
void     pixel_clear       (void);
void     pixel_font        (unsigned size);

typedef unsigned         (*fn_glyph_height_t) (void);
typedef uint32_t const * (*fn_glyph_data_t)   (char);
typedef uint8_t          (*fn_glyph_width_t)  (char);

#endif
