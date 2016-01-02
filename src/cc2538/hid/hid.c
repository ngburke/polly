//*****************************************************************************
//! @file       app_usb_descriptor.c
//! @brief      USB descriptor for HID class.
//!
//! Revised     $Date: 2013-04-08 13:14:23 +0200 (Mon, 08 Apr 2013) $
//! Revision    $Revision: 9658 $
//
//  Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com/
//
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//****************************************************************************/

#include <stdio.h>
#include <hid.h>
#include <usb_firmware_library_headers.h>
#include <core.h>
#include <assert.h>

//
// Constants specifying HID Class requests (bRequest)
//
#define GET_REPORT              0x01
#define GET_IDLE                0x02
#define GET_PROTOCOL            0x03
#define SET_REPORT              0x09
#define SET_IDLE                0x0A
#define SET_PROTOCOL            0x0B

//
// Report types for use with the GET_/SET_REPORT request
//
#define HID_REP_TYPE_INPUT      1
#define HID_REP_TYPE_OUTPUT     2
#define HID_REP_TYPE_FEATURE    3

//
/// Generic report descriptor
//
static const uint8_t pHid0ReportDesc[] = 
{
    0x06, 0xFF, 0xFF,   // 04|2   , Usage Page (vendor defined?)
    0x09, 0x01,         // 08|1   , Usage      (vendor defined
    0xA1, 0x01,         // A0|1   , Collection (Application)

    // IN report
    0x09, 0x02,         // 08|1   , Usage      (vendor defined)
    0x09, 0x03,         // 08|1   , Usage      (vendor defined)
    0x15, 0x00,         // 14|1   , Logical Minimum(0 for signed byte?)
    0x26, 0xFF, 0x00,   // 24|1   , Logical Maximum(255 for signed byte?)
    0x75, 0x08,         // 74|1   , Report Size(8) = field size in bits = 1 byte
    0x95, 0x40,         // 94|1   , ReportCount(size) = repeat count of previous item
    0x81, 0x02,         // 80|1   , IN report (Data,Variable, Absolute)

    // OUT report
    0x09, 0x04,         // 08|1   , Usage      (vendor defined)
    0x09, 0x05,         // 08|1   , Usage      (vendor defined)
    0x15, 0x00,         // 14|1   , Logical Minimum(0 for signed byte?)
    0x26, 0xFF, 0x00,   // 24|1   , Logical Maximum(255 for signed byte?)
    0x75, 0x08,         // 74|1   , Report Size(8) = field size in bits = 1 byte
    0x95, 0x40,         // 94|1   , ReportCount(size) = repeat count of previous item
    0x91, 0x02,         // 90|1   , OUT report (Data,Variable, Absolute)

    // Feature report
    0x09, 0x06,         // 08|1   , Usage      (vendor defined)
    0x09, 0x07,         // 08|1   , Usage      (vendor defined)
    0x15, 0x00,         // 14|1   , LogicalMinimum(0 for signed byte)
    0x26, 0xFF, 0x00,   // 24|1   , Logical Maximum(255 for signed byte)
    0x75, 0x08,         // 74|1   , Report Size(8) =field size in bits = 1 byte
    0x95, 0x40,         // 94|x   , ReportCount in byte
    0xB1, 0x02,         // B0|1   , Feature report
    0xC0                // C0|0   , End Collection
};


const USB_DESCRIPTOR usbDescriptor = 
{
    { // device
        sizeof(USB_DEVICE_DESCRIPTOR),
        USB_DESC_TYPE_DEVICE,           // bDescriptorType
        0x0200,                         // bcdUSB (USB 2.0)
        0x00,                           // bDeviceClass (given by interface)
        0x00,                           // bDeviceSubClass
        0x00,                           // bDeviceProtocol
        USB_EP0_PACKET_SIZE,            // bMaxPacketSize0
        0x0451,                         // idVendor (Texas Instruments)
        0x16C9,                         // idProduct (CC2538 HID)
        0x0100,                         // bcdDevice (v1.0)
        0x01,                           // iManufacturer
        0x02,                           // iProduct
        0x00,                           // iSerialNumber
        0x01,                           // bNumConfigurations
    },
        { // configuration0
            sizeof(USB_CONFIGURATION_DESCRIPTOR),
            USB_DESC_TYPE_CONFIG,           // bDescriptorType
            SIZEOF_CONFIGURATION0_DESC,     // wTotalLength
            0x01,                           // bNumInterfaces
            0x01,                           // bConfigurationValue
            0x00,                           // iConfiguration
            0xA0,                           // bmAttributes (7,4-0: res, 6: self-powered, 5: remote wakeup)
            25                              // bMaxPower (max 2 * 25 = 50 mA)
        },
            { // interface0
                sizeof(USB_INTERFACE_DESCRIPTOR),
                USB_DESC_TYPE_INTERFACE,        // bDescriptorType
                0x00,                           // bInterfaceNumber
                0x00,                           // bAlternateSetting (none)
                0x02,                           // bNumEndpoints
                0x03,                           // bInterfaceClass (HID)
                0x00,                           // bInterfaceSubClass (No boot)
                0x00,                           // bInterfaceProcotol (Generic)
                0x00                            // iInterface
            },
                { // hid0
                    sizeof(USBHID_DESCRIPTOR),
                    USB_DESC_TYPE_HID,              // bDescriptorType
                    0x0111,                         // bcdHID (HID v1.11)
                    0x00,                           // bCountryCode (not localized)
                    0x01,                           // bNumDescriptors
                    USB_DESC_TYPE_HIDREPORT,        // bRDescriptorType
                    sizeof(pHid0ReportDesc)         // wDescriptorLength
                },
                { // endpoint1 in
                    sizeof(USB_ENDPOINT_DESCRIPTOR),
                    USB_DESC_TYPE_ENDPOINT,         // bDescriptorType
                    0x83,                           // bEndpointAddress
                    USB_EP_ATTR_INT,                // bmAttributes (INT)
                    0x0040,                         // wMaxPacketSize
                    0x02                            // bInterval (10 full-speed frames = 10 ms)
                },
                { // endpoint2 out
                    sizeof(USB_ENDPOINT_DESCRIPTOR),
                    USB_DESC_TYPE_ENDPOINT,         // bDescriptorType
                    0x02,                           // bEndpointAddress
                    USB_EP_ATTR_INT,                // bmAttributes (INT)
                    0x0040,                         // wMaxPacketSize
                    0x02                            // bInterval (10 full-speed frames = 10 ms)
                },
    { // strings
        { // langIds
            sizeof(USB_STRING_0_DESCRIPTOR),
            USB_DESC_TYPE_STRING,
            0x0409 // English US
        },
        { // manufacturer
            sizeof(USB_STRING_1_DESCRIPTOR),
            USB_DESC_TYPE_STRING,
            'T', 'e', 'x', 'a', 's', ' ', 'I', 'n', 's', 't', 'r', 'u', 'm', 'e', 'n', 't', 's'
        },
        { // product
            sizeof(USB_STRING_2_DESCRIPTOR),
            USB_DESC_TYPE_STRING,
            'C', 'C', '2', '5', '3', '8', ' ', 'U', 'S', 'B', ' ', 'H', 'I', 'D'
        }
    }
};

//
// Serial number (application-initialized, depends on chip serial number, IEEE address etc.)
//
USB_STRING_3_DESCRIPTOR usbSerialNumberStringDesc;


//
// Look-up table for descriptors other than device and configuration (table is NULL-terminated)
//
const USB_DESCRIPTOR_LUT pUsbDescriptorLut[] = 
{
//    value   index   length                           pDesc
    { 0x0300, 0x0000, sizeof(USB_STRING_0_DESCRIPTOR), &usbDescriptor.strings.langIds },
    { 0x0301, 0x0409, sizeof(USB_STRING_1_DESCRIPTOR), &usbDescriptor.strings.manufacturer },
    { 0x0302, 0x0409, sizeof(USB_STRING_2_DESCRIPTOR), &usbDescriptor.strings.product },
    { 0x0303, 0x0409, sizeof(USB_STRING_3_DESCRIPTOR), &usbSerialNumberStringDesc },
    { 0x2100, 0x0000, sizeof(USBHID_DESCRIPTOR),       &usbDescriptor.hid0 },
    { 0x2200, 0x0000, sizeof(pHid0ReportDesc),         &pHid0ReportDesc },
    { 0x0000, 0x0000, 0,                               NULL }
};


//
// Look-up table entry specifying double-buffering for each interface descriptor
//
const USB_INTERFACE_EP_DBLBUF_LUT pUsbInterfaceEpDblbufLut[] = 
{
//    pInterface                 inMask  outMask
    { &usbDescriptor.interface0, 0x0000, 0x0000 },
};

static uint8_t  inpacket[PACKET_BYTES];
static uint8_t *outpacket;

//
// USB library hooks
//

void usbcrHookProcessOut(void)
{
    usbfwData.ep0Status = EP_STALL;
}

void usbcrHookProcessIn(void)
{
    usbfwData.ep0Status = EP_STALL;
}

void usbvrHookProcessOut(void)
{
    usbfwData.ep0Status = EP_STALL;
}

void usbvrHookProcessIn(void)
{
    usbfwData.ep0Status = EP_STALL;
}

void usbsrHookSetDescriptor(void)
{
    usbfwData.ep0Status = EP_STALL;
}

void usbsrHookSynchFrame(void)
{
    usbfwData.ep0Status = EP_STALL;
}

void usbsrHookClearFeature(void)
{
    usbfwData.ep0Status = EP_STALL;
}

void usbsrHookSetFeature(void)
{
    usbfwData.ep0Status = EP_STALL;
}

uint8_t usbsrHookModifyGetStatus(USB_SETUP_HEADER* pSetupHeader, uint8_t ep0Status, uint16_t* pStatus)
{
    return ep0Status;
}

void usbsrHookProcessEvent(uint8_t event, uint8_t index)
{
    //
    // Process relevant events, one at a time.
    //
    switch(event)
    {
    case USBSR_EVENT_CONFIGURATION_CHANGING:
        //
        // The device configuration is about to change
        //
        break;
    case USBSR_EVENT_CONFIGURATION_CHANGED:
        //
        // The device configuration has changed
        //
        break;
    case USBSR_EVENT_INTERFACE_CHANGING:
        //
        // The alternate setting of the given interface is about to change
        //
        break;
    case USBSR_EVENT_INTERFACE_CHANGED:
        //
        // The alternate setting of the given interface has changed
        //
        break;
    case USBSR_EVENT_REMOTE_WAKEUP_ENABLED:
        //
        // Remote wakeup has been enabled by the host
        //
        break;
    case USBSR_EVENT_REMOTE_WAKEUP_DISABLED:
        //
        // Remote wakeup has been disabled by the host
        //
        break;
    case USBSR_EVENT_EPIN_STALL_CLEARED:
        //
        // The given IN endpoint's stall condition has been cleared the host
        //
        break;
    case USBSR_EVENT_EPIN_STALL_SET:
        //
        // The given IN endpoint has been stalled by the host
        //
        break;
    case USBSR_EVENT_EPOUT_STALL_CLEARED:
        //
        // The given OUT endpoint's stall condition has been cleared the host
        //
        break;
    case USBSR_EVENT_EPOUT_STALL_SET:
        //
        // The given OUT endpoint has been stalled by the PC
        //
        break;
    }
}

void usbirqHookProcessEvents(void)
{
    //
    // Handle events that require immediate processing here
    //

    unsigned bytes;

    //
    // Handle USB reset
    //
    if (USBIRQ_GET_EVENT_MASK() & USBIRQ_EVENT_RESET)
    {
        USBIRQ_CLEAR_EVENTS(USBIRQ_EVENT_RESET);
        usbfwResetHandler();
    }

    //
    // Handle USB packets on EP0
    //
    if (USBIRQ_GET_EVENT_MASK() & USBIRQ_EVENT_SETUP)
    {
        USBIRQ_CLEAR_EVENTS(USBIRQ_EVENT_SETUP);
        usbfwSetupHandler();
    }

    //
    // Handle USB IN events on EP1
    //
    if (USBIRQ_GET_EVENT_MASK() & USBIRQ_EVENT_EP1IN)
    {
        USBIRQ_CLEAR_EVENTS(USBIRQ_EVENT_EP1IN);
    }

    //
    // Handle USB OUT data on EP2
    //
    if (USBIRQ_GET_EVENT_MASK() & USBIRQ_EVENT_EP2OUT)
    {
        USBIRQ_CLEAR_EVENTS(USBIRQ_EVENT_EP2OUT);
        USBFW_SELECT_ENDPOINT(2);

        if (USBFW_OUT_ENDPOINT_DISARMED())
        {
            bytes = USBFW_GET_OUT_ENDPOINT_COUNT_LOW();

            if (bytes > 0)
            {
                assert(PACKET_BYTES == bytes);

                usbfwReadFifo(USB_F2, bytes, inpacket);
                core_inpacket(inpacket);

            }

            USBFW_ARM_OUT_ENDPOINT();
        }
    }

    //
    // Send back responses from the core via USB IN on EP3
    //
    while (core_outpacket_ready())
    {
        USBFW_SELECT_ENDPOINT(3);

        if (USBFW_IN_ENDPOINT_DISARMED())
        {
            outpacket = core_outpacket();
            usbfwWriteFifo(USB_F3, PACKET_BYTES, outpacket);
            USBFW_ARM_IN_ENDPOINT();
        }
    }
}

void usbsuspHookEnteringSuspend(bool remoteWakeupAllowed)
{
    /*
    if (remoteWakeupAllowed) {
        GPIOPowIntClear(BSP_KEY_SEL_BASE, BSP_KEY_SELECT);
        GPIOPowIntEnable(BSP_KEY_SEL_BASE, BSP_KEY_SELECT);
        IntPendClear(INT_GPIOA);
        IntEnable(INT_GPIOA);

        GPIOPowIntClear(BSP_KEY_DIR_BASE, BSP_KEY_DIR_ALL);
        GPIOPowIntEnable(BSP_KEY_DIR_BASE, BSP_KEY_DIR_ALL);
        IntPendClear(INT_GPIOC);
        IntEnable(INT_GPIOC);
    }
    */
}

void usbsuspHookExitingSuspend(void)
{
    /*
    IntDisable(INT_GPIOA);
    GPIOPowIntDisable(BSP_KEY_SEL_BASE, BSP_KEY_SELECT);

    IntDisable(INT_GPIOC);
    GPIOPowIntDisable(BSP_KEY_DIR_BASE, BSP_KEY_DIR_ALL);
    */
}


//
// Normal functions
//

void hid_init(void)
{
    // Disable the USB D+ pull-up resistor
    UsbDplusPullUpDisable();

    // Initialize the USB library
    usbfwInit();

    // Initialize the USB interrupt handler with bit mask containing all processed USBIRQ events
    usbirqInit(USBIRQ_EVENT_RESET | USBIRQ_EVENT_SETUP | USBIRQ_EVENT_SUSPEND | USBIRQ_EVENT_RESUME | USBIRQ_EVENT_EP1IN | USBIRQ_EVENT_EP2OUT);

    // Activate the USB D+ pull-up resistor
    UsbDplusPullUpEnable();
}

void hid_process_events(void)
{
    //
    // Handle USB resume
    //
    if (USBIRQ_GET_EVENT_MASK() & USBIRQ_EVENT_RESUME)
    {
        USBIRQ_CLEAR_EVENTS(USBIRQ_EVENT_RESUME);
    }

    //
    // Handle USB suspend
    //
    if (USBIRQ_GET_EVENT_MASK() & USBIRQ_EVENT_SUSPEND)
    {
        USBIRQ_CLEAR_EVENTS(USBIRQ_EVENT_SUSPEND);
        //usbsuspEnter();
    }
}
