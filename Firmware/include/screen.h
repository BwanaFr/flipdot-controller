#ifndef _SCREEN_H__
#define _SCREEN_H__

#include <Arduino.h>
#include <HardwareSerial.h>

class Screen {
public:
    /**
     * Constructor
     * @param serial Reference to the RS485 serial port
     * @param address Display address, default is 0x6
     * @param width Number of horizontal pixels
     * @param height Number of vertial pixels
    */
    Screen(HardwareSerial& serial, uint8_t address=0x06, uint8_t width=112, uint8_t height=19);
    virtual ~Screen() = default;

    /**
     * Begin
    */
    void begin(int8_t rxPin, int8_t txPin);

    /**
     * Display a text
     * @param text Text to be displayed
     * @param x Horizontal offset in pixels
     * @param y Vertical offset in pixels
     * @param font Font selection
    */
    void sendText(const char* text, uint8_t x=0, uint8_t y=0, uint8_t font=0x73);

    /**
     * Send raw values (debugging)
     * Raw values doesn't contains checksum, address and first 0xff
    */
    void sendRaw(const uint8_t* data, int dataLen);

private:
    HardwareSerial& serial_;
    uint8_t address_;
    uint8_t width_;
    uint8_t height_;
    int checksum_;
    void startTransaction(uint8_t mode=0xa2);
    void endTransaction();
    void sendSerial(uint8_t byte, bool addChk=true);
    void sendScreenSize();
};


#endif
