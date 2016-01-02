#include <pins.h>
#include <memlcd.h>
#include <ssi.h>
#include <gpio.h>
#include <pixel.h>
#include <gpio.h>
#include <gptimer.h>
#include <ioc.h>

#define LCD_SSI_BASE     SSI0_BASE
#define LCD_SPI_END()    GPIOPinWrite(LCD_BASE, LCD_CS, 0);
#define LCD_SPI_BEGIN()  GPIOPinWrite(LCD_BASE, LCD_CS, LCD_CS);

#define VCOM_HI 0x40
#define VCOM_LO 0x00

unsigned vflag;

uint8_t swap_bits(uint8_t byte)
{
    #define CHAR_BIT 8

    uint8_t r = byte; // r will be reversed bits of v; first get LSB of v
    int s = sizeof(byte) * CHAR_BIT - 1; // extra shift needed at end

    for (byte >>= 1; byte; byte >>= 1)
    {
        r <<= 1;
        r |= byte & 1;
        s--;
    }

    r <<= s; // shift when v's highest bits are zero

    return r;
}

void memlcd_init(void)
{
    // Chip select
    GPIOPinTypeGPIOOutput (LCD_BASE, LCD_CS);
    GPIOPinWrite          (LCD_BASE, LCD_CS, 0);

    // Set up the EXTCOM toggling signal

    // Configure GPTimer0A as a 16-bit PWM Timer
    TimerConfigure(GPTIMER0_BASE, GPTIMER_CFG_SPLIT_PAIR | GPTIMER_CFG_A_PWM | GPTIMER_CFG_B_PWM);

    // Set timer as active high, ~2Hz frequency, 50% duty cycle
    TimerControlLevel     (GPTIMER0_BASE, GPTIMER_A, false);
    TimerPrescaleSet      (GPTIMER0_BASE, GPTIMER_A, 0x7F);    // 2Hz
    TimerLoadSet          (GPTIMER0_BASE, GPTIMER_A, 0xFFFF);
    TimerPrescaleMatchSet (GPTIMER0_BASE, GPTIMER_A, 0x3F);    // 50% duty cycle
    TimerMatchSet         (GPTIMER0_BASE, GPTIMER_A, 0xFFFF);

    // Configure the Timer0_InputCapturePin.1
    IOCPinConfigPeriphOutput (LCD_BASE, LCD_TOGGLE, IOC_MUX_OUT_SEL_GPT0_ICP1);
    GPIODirModeSet           (LCD_BASE, LCD_TOGGLE, GPIO_DIR_MODE_HW);
    IOCPadConfigSet          (LCD_BASE, LCD_TOGGLE, IOC_OVERRIDE_OE);

    // Enable GPTimer0A.
    TimerEnable(GPTIMER0_BASE, GPTIMER_A);
}

// Only needed if external toggle is not present
void memlcd_refresh(void)
{
    vflag ^= 1;

    LCD_SPI_BEGIN();

    // Send 'toggle vcom' command
    SSIDataPut(LCD_SSI_BASE, vflag ? VCOM_HI : VCOM_LO);

    // The trailer
    SSIDataPut(LCD_SSI_BASE, 0x00);

    // Wait for the final transfer
    while(SSIBusy(LCD_SSI_BASE));

    LCD_SPI_END();
}

void memlcd_clear(void)
{
    vflag ^= 1;

    LCD_SPI_BEGIN();

    // Send 'clear screen' command
    SSIDataPut(LCD_SSI_BASE, vflag ? VCOM_HI | 0x20 : VCOM_LO | 0x20);

    // The trailer
    SSIDataPut(LCD_SSI_BASE, 0x00);

    // Wait for the final transfer
    while(SSIBusy(LCD_SSI_BASE));

    LCD_SPI_END();

}

void memlcd_sendline(const uint32_t linenum, const uint8_t *data)
{
    unsigned i;
    uint32_t out_linenum;

    out_linenum = swap_bits(linenum);

    vflag ^= 1;

    LCD_SPI_BEGIN();

    // Send everything a byte at a time

    // Send 'write line' command
    SSIDataPut(LCD_SSI_BASE, vflag ? VCOM_HI | 0x80 : VCOM_LO | 0x80);

    // The line number
    SSIDataPut(LCD_SSI_BASE, out_linenum);

    // The data
    for (i = 0; i < SCREEN_WIDTH_BYTES; i++)
    {
        SSIDataPut(LCD_SSI_BASE, data[i]);
    }

    // The trailer
    SSIDataPut(LCD_SSI_BASE, 0x00);

    SSIDataPut(LCD_SSI_BASE, 0x00);

    // Wait for the final transfer
    while(SSIBusy(LCD_SSI_BASE));

    LCD_SPI_END();
}
