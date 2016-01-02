#ifndef PINS_H_
#define PINS_H_

#include <hw_memmap.h>

#define SPI_BUS_BASE  GPIO_A_BASE
#define SPI_SCK       GPIO_PIN_2      // PA2
#define SPI_MOSI      GPIO_PIN_4      // PA4
#define SPI_MISO      GPIO_PIN_5      // PA5

#define LCD_BASE      GPIO_A_BASE
#define LCD_CS        GPIO_PIN_6      // PA6
#define LCD_TOGGLE    GPIO_PIN_7      // PA7

#define UART_BT_BASE  GPIO_D_BASE
#define UART_BT_TXD   GPIO_PIN_0      // PB2 -> PD0
#define UART_BT_RXD   GPIO_PIN_1      // PB3 -> PD1
#define UART_BT_RTS   GPIO_PIN_2      // PB4 -> PD2

#define USB_PIN_BASE  GPIO_C_BASE
#define USB_PIN_DP    GPIO_PIN_0      // PC0

#define TOUCH_BASE    GPIO_C_BASE
#define TOUCH_KEY_1   GPIO_PIN_4      // PC7
#define TOUCH_KEY_2   GPIO_PIN_5      // PC6
#define TOUCH_KEY_3   GPIO_PIN_6      // PC5
#define TOUCH_KEY_4   GPIO_PIN_7      // PC4

#endif
