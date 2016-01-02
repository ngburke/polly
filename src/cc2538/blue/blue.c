#include <string.h>
#include <blue.h>
#include <pins.h>
#include <uart.h>
#include <assert.h>
#include <core.h>
#include <gpio.h>
#include <ioc.h>
#include <hw_ioc.h>
#include <sys_ctrl.h>

#define UART_INT 0xF0  // Interrupts handled by uart

static uint8_t  inpacket[PACKET_BYTES];
static unsigned inbytes;
static uint8_t *outpacket;

static void     blue_isr  (void);
static void     send      (uint8_t *buff, unsigned bytes);

static void     rx_pause  (void);
static void     rx_resume (void);


static void blue_isr(void)
{
    uint32_t int_stat;

    // Get status of enabled interrupts
    int_stat = UARTIntStatus(UART0_BASE, 1);

    // Clear flags handled by this handler
    UARTIntClear(UART0_BASE, (int_stat & UART_INT));

    //
    // RX or RX timeout
    //
    if (int_stat & (UART_INT_RX | UART_INT_RT))
    {
        // Put received bytes into buffer

        while (UARTCharsAvail(UART0_BASE))
        {
            inpacket[inbytes++] = UARTCharGetNonBlocking(UART0_BASE);

            if (inbytes == PACKET_BYTES)
            {
                rx_pause();

                //
                // Assumes every time rx data is available, it is packetized appropriately
                // with a flow control and byte count, thus no byte count is necessary.
                //
                core_inpacket(inpacket);

                inbytes = 0;

                break;
            }
        }
    }

    //
    // TX
    //
    if (int_stat & UART_INT_TX)
    {
        // Do nothing for now.
    }

    //
    // Send back responses from the core
    //
    while (core_outpacket_ready())
    {
        outpacket = core_outpacket();
        send(outpacket, PACKET_BYTES);
    }

    rx_resume();
}

static void rx_pause(void)
{
    GPIOPinWrite(UART_BT_BASE, UART_BT_RTS, UART_BT_RTS);
}

static void rx_resume(void)
{
    GPIOPinWrite(UART_BT_BASE, UART_BT_RTS, 0);
}


static void send(uint8_t *buff, unsigned bytes)
{
    unsigned i;

    for (i = 0; i < bytes; i++)
    {
        UARTCharPut(UART0_BASE, buff[i]);
    }
}


void blue_process_events(void)
{
    // Nothing for now
}

void blue_init(void)
{
    //const int BAUD_RATE = 115200;
    const int BAUD_RATE = 460800;

    inbytes = 0;

    // Enable UART peripheral module
    SysCtrlPeripheralEnable(SYS_CTRL_PERIPH_UART0);

    // Disable UART function
    UARTDisable(UART0_BASE);

    // Disable all UART module interrupts
    UARTIntDisable(UART0_BASE, 0x17FF);

    // Set IO clock as UART clock source
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);

    //
    // Map Bluetooth UART signals to the correct GPIO pins and configure them as
    // hardware controlled.
    //
    IOCPinConfigPeriphOutput (UART_BT_BASE, UART_BT_TXD, IOC_MUX_OUT_SEL_UART0_TXD);
    GPIOPinTypeUARTOutput    (UART_BT_BASE, UART_BT_TXD);
    IOCPinConfigPeriphInput  (UART_BT_BASE, UART_BT_RXD, IOC_UARTRXD_UART0);
    GPIOPinTypeUARTInput     (UART_BT_BASE, UART_BT_RXD);

    // Manually controlling the flow control signal
    GPIOPinTypeGPIOOutput    (UART_BT_BASE, UART_BT_RTS);
    GPIOPinWrite             (UART_BT_BASE, UART_BT_RTS, 0);

    // Register UART interrupt handler
    UARTIntRegister(UART0_BASE, &blue_isr);

    // Configure and enable UART module
    UARTConfigSetExpClk(UART0_BASE, SysCtrlClockGet(), BAUD_RATE,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    // TX interrupts occur on FIFO threshold
    UARTTxIntModeSet(UART0_BASE, UART_TXINT_MODE_FIFO);

    // Configure FIFO interrupt threshold (half full)
    UARTFIFOLevelSet(UART0_BASE, UART_FIFO_TX4_8, UART_FIFO_RX4_8);

    // Clear all UART module interrupt flags
    UARTIntClear(UART0_BASE, 0x17FF);

    // Enable UART module interrupts (RX, TX, RX timeout)
    UARTIntEnable(UART0_BASE, (UART_INT_RX | UART_INT_TX | UART_INT_RT));

    // Enable UART function
    UARTEnable(UART0_BASE);

}
