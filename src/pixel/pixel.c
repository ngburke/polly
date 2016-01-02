#include <pixel.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <font/pix_5.h>
#include <font/pix_6.h>
#include <font/pix_8.h>

// Prototypes
unsigned write_char(uint8_t (*dst)[SCREEN_WIDTH_BYTES], const unsigned bit_offset, const uint32_t *char_data, const unsigned char_bits);

// Globals
uint8_t screen_data[SCREEN_HEIGHT_BITS][SCREEN_WIDTH_BYTES];
void (*sendline)(const uint32_t, const uint8_t*);
void (*clear)   (void);

fn_glyph_height_t glyph_height;
fn_glyph_data_t   glyph_data;
fn_glyph_width_t  glyph_width;

unsigned write_char(uint8_t (*dst)[SCREEN_WIDTH_BYTES], const unsigned bit_offset, uint32_t const *char_data, const unsigned char_bits)
{
    unsigned byte_offset     = bit_offset >> 3;
    unsigned char_bit_offset = bit_offset & 7;
    unsigned i, byte, char_bytes, char_remain;

    uint8_t  chunk;

    char_bytes  = char_bits >> 3;
    char_remain = char_bits & 7;

    // Set the destination memory with the expanded character
    for (i = 0; i < glyph_height(); i++)
    {
        // Write each byte of the multiplied character, except any remainder bytes.
        for (byte = 0; byte < char_bytes; byte++)
        {
            // Write in the data from MSb to LSb
            chunk = char_data[i] >> ((char_bytes - 1 - byte) * 8) + char_remain;

            dst[i][byte_offset + byte]     ^= chunk >> char_bit_offset;
            dst[i][byte_offset + byte + 1] ^= chunk << (8 - char_bit_offset);
        }

        if (char_remain > 0)
        {
            // Write in the data from MSb to LSb
            chunk = char_data[i] << (8 - char_remain);

            dst[i][byte_offset + byte]     ^= chunk >> char_bit_offset;
            dst[i][byte_offset + byte + 1] ^= chunk << (8 - char_bit_offset);
        }
    }

    return char_bits;
}

void pixel_init(void (*snd)(const uint32_t, const uint8_t*), void (*clr)(void))
{
    sendline = snd;
    clear    = clr;

    pixel_font(8);

    clear();
}

void pixel_font(unsigned size)
{
    switch (size)
    {
    case 8:
        font_pix_8(&glyph_height, &glyph_data, &glyph_width);
        break;

    case 6:
        font_pix_6(&glyph_height, &glyph_data, &glyph_width);
        break;

    case 5:
        font_pix_5(&glyph_height, &glyph_data, &glyph_width);
        break;
    }
}

void pixel_clear_lines(const unsigned start, const unsigned lines)
{
    unsigned line, data;

    for (line = start; line < start + lines; line++)
    {
        for (data = 0; data < SCREEN_WIDTH_BYTES; data++)
        {
            screen_data[line][data] = 0xFF;
        }

        sendline(line, &screen_data[line][0]);
    }
}

void pixel_clear(void)
{
    clear();
    memset(screen_data, 0xFF, sizeof(screen_data));
}

void pixel_rect(const uint8_t line_offset, const uint8_t lines, const uint8_t bit_offset, const uint8_t bits)
{
    uint8_t  first_byte;
    uint8_t  last_byte;
    unsigned first_byte_offset;
    unsigned last_byte_offset;
    unsigned i, j;

    first_byte_offset = bit_offset / 8;
    last_byte_offset  = (bit_offset + bits - 1) / 8;

    first_byte  = bit_offset & 0x7;  // Mask off only the lower 3 bits (0-7)
    first_byte  = 7 - first_byte;    // Flip the polarity
    first_byte  = first_byte + 1;    // Add one to allow for subtraction later
    first_byte  = 1 << first_byte;   // Set the bit position (power of 2)
    first_byte  = ~(first_byte - 1); // Subtract to get the full bit coverage (power of 2 - 1)

    last_byte  = (bit_offset + bits) & 0x7;  // Mask off only the lower 3 bits (0-7)

    if (last_byte != 0)
    {
        last_byte  = 7 - last_byte;    // Flip the polarity
        last_byte  = last_byte + 1;    // Add one to allow for subtraction later
        last_byte  = 1 << last_byte;   // Set the bit position (power of 2)
        last_byte  = (last_byte - 1);  // Subtract to get the full bit coverage (power of 2 - 1)
    }

    for (i = line_offset; i < line_offset + lines; i++)
    {
        // First byte
        screen_data[i][first_byte_offset] &= first_byte;

        // Middle bytes
        for (j = first_byte_offset + 1; j < last_byte_offset; j++)
        {
            screen_data[i][j] = 0x00;
        }

        // Last byte
        if (first_byte_offset == last_byte_offset)
        {
            screen_data[i][last_byte_offset] &= first_byte | last_byte;
        }
        else
        {
            screen_data[i][last_byte_offset] &= last_byte;
        }

        sendline(i + 1, &screen_data[i][0]);
    }
}

unsigned pixel_str_width(const char *str)
{
    unsigned i, width = 0;

    for (i = 0; i < strlen(str); i++)
    {
        width += glyph_width(str[i]);
    }

    return width;
}

uint8_t pixel_str(const char *str, const uint8_t line_offset, const uint8_t bit_offset)
{
    unsigned i, width;
    uint8_t new_bit_offset = bit_offset;

    if (bit_offset >= CENTER)
    {
        // Get the width of the string
        width = pixel_str_width(str);

        if (CENTER == bit_offset)
        {
            new_bit_offset = (SCREEN_WIDTH_BYTES * 8 - width) / 2;
        }
        else if (CENTER_LEFT == bit_offset)
        {
            new_bit_offset = ((SCREEN_WIDTH_BYTES / 2) * 8 - width) / 2;
        }
        else // (CENTER_RIGHT == bit_offset)
        {
            new_bit_offset = (((SCREEN_WIDTH_BYTES / 2) * 8 - width) / 2) + (SCREEN_WIDTH_BYTES / 2 * 8);
        }
    }

    // Set characters
    for (i = 0; i < strlen(str); i++)
    {
        new_bit_offset += write_char(&screen_data[line_offset],
                                     new_bit_offset,
                                     glyph_data(str[i]),
                                     glyph_width(str[i]));
    }

    // Write out the lines
    for (i = 0; i < glyph_height(); i++)
    {
        sendline(i + line_offset + 1, &screen_data[i + line_offset][0]);
    }

    return new_bit_offset;
}

