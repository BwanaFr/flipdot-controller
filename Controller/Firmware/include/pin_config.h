#ifndef _PIN_CONFIG__
#define _PIN_CONFIG__

#include <SPI.h>

//SPI pins for shift registers
#define SER MOSI
#define SRCLK SCK
#define RCLK SS
#define SRCLR 3
//Coil DC/DC control
#define COIL 47
//Backlight DC/DC control
#define BACKLIGHT 48
//Onboard LED
#define LED 14

#endif
