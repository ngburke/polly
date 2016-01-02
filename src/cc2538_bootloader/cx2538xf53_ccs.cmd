/*
  CCS linker configuration file for CC2538SF53, CC2538NF53 and CM2538SF53
 */

--retain=g_pfnVectors

/*
   The following command line options are set as part of the CCS project.
   If you are building using the command line, or for some reason want to
   define them here, you can uncomment and modify these lines as needed.
   If you are using CCS for building, it is probably better to make any such
   modifications in your CCS project and leave this file alone.

   --heap_size=0
   --stack_size=256
   --library=rtsv7M3_T_le_eabi.lib

   The starting address of the application.  Normally the interrupt vectors
   must be located at the beginning of the application.
 */

#define BOOTLOADER_BASE         0x00200000
#define MAIN_HEADER_BASE        0x00201000
#define MAIN_BASE               0x00201800
#define DOWNLOAD_HEADER_BASE    0x00230000
#define DOWNLOAD_BASE           0x00230800
#define STORAGE_BASE            0x0027F000
#define FLASH_CCA_BASE          0x0027FFD4
#define RAM_BASE                0x20000000
#define RAM_NON_RETENTION_BASE  RAM_BASE
#define RAM_RETENTION_BASE      0x20004000

#define BOOTLOADER_BYTES        0x1000     // 4k
#define MAIN_HEADER_BYTES       0x800      // 2k (one page)
#define MAIN_BYTES              0x2E800    // 186k
#define STORAGE_BYTES           0x800      // 2k (one page)

//
// System memory map
//

MEMORY
{
    // Bootloader stored in and executes from internal flash
    BOOTLOADER (RX) : origin = BOOTLOADER_BASE, length = BOOTLOADER_BYTES

    // Main FW header stored in internal flash
    MAIN_HEADER (RW) : origin = MAIN_HEADER_BASE, length = MAIN_HEADER_BYTES

    // Main FW stored in and executes from internal flash
    MAIN (RWX) : origin = MAIN_BASE, length = MAIN_BYTES

    // FW updates are temporarily stored flash for verification
    // before being moved to Main FW for execution
    DOWNLOAD (RW) : origin = DOWNLOAD_HEADER_BASE, length = MAIN_HEADER_BYTES + MAIN_BYTES

    // Internal storage area for things like seeds and device info
    STORAGE (RW) : origin = STORAGE_BASE, length = STORAGE_BYTES

    // Customer Configuration Area and Bootloader Backdoor configuration in flash, 12 bytes
    FLASH_CCA (RX) : origin = FLASH_CCA_BASE, length = 12

    // Application uses internal RAM for data
    // RAM Size 32 KB
    // 16 KB of RAM is non-retention
    SRAM_NON_RETENTION (RWX) : origin = RAM_NON_RETENTION_BASE, length = 0x00004000

    // 16 KB of RAM is retention. The stack, and all variables that need
    // retention through PM2/3 must be in SRAM_RETENTION
    SRAM_RETENTION (RWX) : origin = RAM_RETENTION_BASE, length = 0x00004000
}

//
// Section allocation in memory
//

SECTIONS
{
    .intvecs    :    > BOOTLOADER_BASE
    .text       :    > BOOTLOADER
    .const      :    > BOOTLOADER
    .rodata     :    > BOOTLOADER
    .rodata.str1.4 : > BOOTLOADER
    .cinit      :    > BOOTLOADER
    .pinit      :    > BOOTLOADER
    .init_array :    > BOOTLOADER
    .flashcca   :    > FLASH_CCA

    .vtable     :    > RAM_RETENTION_BASE
    .data       :    > SRAM_RETENTION
    .bss        :    > SRAM_RETENTION
    .sysmem     :    > SRAM_RETENTION
    .stack      :    > SRAM_RETENTION (HIGH)
    .nonretenvar:    > SRAM_NON_RETENTION
}

// Create global constant that points to top of stack
// CCS: Change stack size under Project Properties
__STACK_TOP = __stack + __STACK_SIZE;

__BOOTLOADER_BASE      = BOOTLOADER_BASE;

__MAIN_HEADER_BASE     = MAIN_HEADER_BASE;
__MAIN_HEADER_BYTES    = MAIN_HEADER_BYTES;

__MAIN_BASE            = MAIN_BASE;
__MAIN_BYTES           = MAIN_BYTES;

__DOWNLOAD_HEADER_BASE = DOWNLOAD_HEADER_BASE;
__DOWNLOAD_BASE        = DOWNLOAD_BASE;
__STORAGE_BASE         = STORAGE_BASE;
