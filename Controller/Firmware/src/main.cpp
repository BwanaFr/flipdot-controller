/**
 *  Flipdot controller firmware
**/


#include <Arduino.h>
#include <SPI.h>
#include "pin_config.h"

SPIClass * fspi = NULL;

void setup(){
  Serial.begin(115200);
  fspi = new SPIClass(FSPI);
  fspi->begin(SRCLK, -1, SER, RCLK);
  fspi->setHwCs(true);
  pinMode(LED, OUTPUT);
  pinMode(SRCLR, OUTPUT);
  pinMode(COIL, OUTPUT);
  pinMode(BACKLIGHT, OUTPUT);
}

unsigned long lastSend = 0;
void loop(){
  unsigned long now = millis();
  if((now - lastSend) > 1000){
    uint8_t data[] = {0x55, 0x55, 0x55};
    digitalWrite(SRCLR, LOW);
    fspi->beginTransaction(SPISettings(10000000, SPI_MSBFIRST, SPI_MODE0));
    fspi->transfer(data, 3);
    fspi->endTransaction();
    delayMicroseconds(100);
    digitalWrite(SRCLR, HIGH);
    lastSend = now;
    digitalWrite(LED, !digitalRead(LED));
    digitalWrite(COIL, !digitalRead(LED));
    digitalWrite(BACKLIGHT, !digitalRead(COIL));
  }
  delay(20);
}