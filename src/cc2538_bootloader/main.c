//*****************************************************************************
//! @file       main.c
//! @brief      Empty template main source file
//!
//! Revised     $Date: 2013-02-15 10:39:12 +0100 (fr, 15 feb 2013) $
//! Revision    $Revision: 25225 $
//
//  Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com/
//
//****************************************************************************/

#include <stdint.h>
#include <linker.h>
#include <fw_header.h>
#include <flash.h>

static bool valid_header(uint32_t addr);

static uint32_t temp[0x800 / 4];

int main(void)
{
    // Check the main firmware header & image integrity
    if (valid_header(MAIN_HEADER_BASE))
    {
        //
        // Jump to Main FW
        // The jump call will be a BLX assembly instruction, lowest order bit
        // of the destination address must be a 1.
        //
        void (*mainfw)(void) = (void (*)(void))(MAIN_BASE + 1);

        (*mainfw)();
    }

    // Check the downloaded firmware header & image integrity
    if (valid_header(DOWNLOAD_HEADER_BASE))
    {
        // A new firmware was downloaded and authenticated, hence the main firmware header is not valid.
        // Erase the main firmware, then copy the downloaded firmware over to the main location now.
        uint32_t off;

        for (off = 0; off < MAIN_HEADER_BYTES + MAIN_BYTES; off += 0x800)
        {
            FlashMainPageErase(MAIN_HEADER_BASE + off);
        }

        // Program the firmware before the header so a half download will be restarted again
        for (off = 0; off < MAIN_BYTES; off += 0x800)
        {
            memcpy(temp, (void*)(DOWNLOAD_BASE + off), 0x800);
            FlashMainPageProgram(temp, MAIN_BASE + off, 0x800);
        }

        // Finally, program the header
        memcpy(temp, (void*)(DOWNLOAD_HEADER_BASE), 0x800);
        FlashMainPageProgram(temp, MAIN_HEADER_BASE, 0x800);

        //
        // Jump to Main FW
        // The jump call will be a BLX assembly instruction, lowest order bit
        // of the destination address must be a 1.
        //
        void (*mainfw)(void) = (void (*)(void))(MAIN_BASE + 1);

        (*mainfw)();
    }

    // Neither main FW or the downloaded firmware is valid, we're sunk
    while(1);
}

bool valid_header(uint32_t addr)
{
    fw_header_t *hdr = (fw_header_t*)addr;

    if (hdr->f.id == STANDARD_ID)
    {
        return true;
    }

    return false;
}
