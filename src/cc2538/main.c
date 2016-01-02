#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pka.h>
#include <blue.h>
#include <hid.h>
#include <memlcd.h>

#include <core.h>
#include <crypto.h>
#include <pixel.h>
#include <screen.h>
#include <touch.h>
#include <fw_header.h>

// For board init routine
#include <pins.h>
#include <hw_ioc.h>
#include <ioc.h>
#include <sys_ctrl.h>
#include <gpio.h>
#include <ssi.h>
#include <interrupt.h>

void __assert_func(const char *_file, int _line, const char *_func, const char *_expr)
{
    while(1);
}


void board_init(void)
{
    uint32_t dummy;

    //
    // Clocking
    //

    // Set system clock (no ext 32k osc, no internal osc)
    SysCtrlClockSet(false, false, SYS_CTRL_SYSDIV_32MHZ);

    // Set IO clock to the same as system clock
    SysCtrlIOClockSet(SYS_CTRL_SYSDIV_32MHZ);

    //
    // SPI bus
    //

    // Enable SSI peripheral module
    SysCtrlPeripheralEnable(SYS_CTRL_PERIPH_SSI0);

    // Disable SSI function before configuring module
    SSIDisable(SSI0_BASE);

    // Set IO clock as SSI clock source
    SSIClockSourceSet(SSI0_BASE, SSI_CLOCK_PIOSC);

    // Map SSI signals to the correct GPIO pins and configure them as HW ctrl'd
    IOCPinConfigPeriphOutput (SPI_BUS_BASE, SPI_SCK,  IOC_MUX_OUT_SEL_SSI0_CLKOUT);
    IOCPinConfigPeriphOutput (SPI_BUS_BASE, SPI_MOSI, IOC_MUX_OUT_SEL_SSI0_TXD);
    IOCPinConfigPeriphInput  (SPI_BUS_BASE, SPI_MISO, IOC_SSIRXD_SSI0);
    GPIOPinTypeSSI           (SPI_BUS_BASE, (SPI_MOSI | SPI_MISO | SPI_SCK));

    // Set SPI mode and speed - 1MHz max clock rate for Sharp Memory LCD
    SSIConfigSetExpClk(SSI0_BASE, SysCtrlIOClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 1000000UL, 8);

    // Enable the SSI function
    SSIEnable(SSI0_BASE);

    // Flush the RX FIFO
    while(SSIDataGetNonBlocking(SSI0_BASE, &dummy));

}

#pragma DATA_SECTION(hdr, ".header")
const fw_header_t hdr = { .f.id = STANDARD_ID };


#pragma CODE_SECTION(main, ".entry")
int main(void)
{
    // Simple access to ensure the header placeholder is not stripped away
    assert(hdr.f.id == STANDARD_ID);

    // Disable global interrupts
    IntMasterDisable();

    // Turn on internal peripherals
    SysCtrlPeripheralReset  (SYS_CTRL_PERIPH_GPT0);
    SysCtrlPeripheralEnable (SYS_CTRL_PERIPH_GPT0);

    SysCtrlPeripheralReset  (SYS_CTRL_PERIPH_PKA);
    SysCtrlPeripheralEnable (SYS_CTRL_PERIPH_PKA);

    SysCtrlPeripheralReset  (SYS_CTRL_PERIPH_AES);
    SysCtrlPeripheralEnable (SYS_CTRL_PERIPH_AES);

    // Clock and shared IO initialization, loosely based on bspInit()
    board_init();

    // Module initialization
    memlcd_init ();
    pixel_init  (memlcd_sendline, memlcd_clear);
    crypto_init ();
    core_init   ();
    //blue_init   ();
    hid_init    ();
    touch_init  ();

    // Re-enable global interrupts
    IntMasterEnable();

    #if TOUCH_CAPACITIVE
    touch_cap_calibrate();
    #endif

    core_load_ctx();
    core_ready();

    while(1)
    {
        //SysCtrlSleep();

        blue_process_events();
        hid_process_events();

        core_process();

        // while(1) screen_captouch_debug();

        // SysCtrlPowerModeSet(SYS_CTRL_PM_1);
    }
}
