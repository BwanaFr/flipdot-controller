#ifndef __CONFIG_H__
#define __CONFIG_H__

// PIN
#define PIN_5V_EN 16

#define CAN_TX_PIN 26
#define CAN_RX_PIN 27
#define CAN_SE_PIN 23

#define RS485_EN_PIN 17 // 17 /RE
#define RS485_TX_PIN 22 // 21
#define RS485_RX_PIN 21 // 22
#define RS485_SE_PIN 19 // 22 /SHDN

#define SD_MISO_PIN 2
#define SD_MOSI_PIN 15
#define SD_SCLK_PIN 14
#define SD_CS_PIN 13

//Buildin RGB led
#define LEDS_COUNT 1
#define LEDS_PIN 4
#define CHANNEL 0

//Relays to control power supplies
#define SCREEN_PWR_PIN 18
#define BACKLIGHT_PWR_PIN 5

#endif
