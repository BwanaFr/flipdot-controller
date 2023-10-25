#ifndef _SCREEN_H__
#define _SCREEN_H__

#include <Arduino.h>
#include <HardwareSerial.h>
#include "config.h"

#define DATA_BUFFER_SIZE 1024
#define DATA_BUFFER_COUNT 2

class Screen {
public:
    /**
     * Constructor
     * @param serial Reference to the RS485 serial port
     * @param address Display address, default is 0x6
     * @param width Number of horizontal pixels
     * @param height Number of vertial pixels
     * @param powerPin Pin to use to control the relay
     * @param pwrSaveTime Seconds to switch off screen power supply (-1 to disable)
     * @param backlightPin Pin to control the screen backlight
    */
    Screen(HardwareSerial& serial, uint8_t address=0x06, uint8_t width=112,
            uint8_t height=19, uint8_t powerPin=SCREEN_PWR_PIN,
            int pwrSaveTime=-1, uint8_t backlightPin=BACKLIGHT_PWR_PIN);
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

    /**
     * Sets the screen will all dots
    */
    void allWhite();

    /**
     * Sets the number of seconds before switching the screen off
    */
    inline void setPowerTimeout(int seconds){ pwrSaveTime_ = seconds; }

    /**
     * Loop
    */
    void loop();

    /**
     * Sets backlight on/off
     **/
    void setBackLight(bool on);



private:

    //Data packet
    struct DataPacket {
        uint8_t data[DATA_BUFFER_SIZE];
        size_t count;
        DataPacket() : count(0){}
        inline void append(uint8_t d){
            data[count++] = d;
        }
        inline void operator<<(uint8_t d){
            append(d);
        }
    };

    HardwareSerial& serial_;
    uint8_t address_;
    uint8_t width_;
    uint8_t height_;
    uint8_t powerPin_;
    int pwrSaveTime_;
    uint8_t backlightPin_;
    unsigned long lastUsed_;
    //Buffer for storing data
    SemaphoreHandle_t readSemaphore_;
    SemaphoreHandle_t writeSemaphore_;
    DataPacket buffers_[DATA_BUFFER_COUNT];
    int readPos_;
    int writePos_;

    /**
     * Append mode to packet
    */
    void appendMode(DataPacket& packet, uint8_t mode=0xa2);

    /**
     * Append screen size to buffer
    */
    void appendScreenSize(DataPacket& packet, uint8_t w, uint8_t h);

    /**
     * Sets coordinates
    */
    void appendCoordinates(DataPacket& packet, uint8_t x, uint8_t y);

    /**
     * Sets font
    */
    void appendFont(DataPacket& packet, uint8_t font);

    /**
     * Append text to packet
    */
    void appendText(DataPacket& packet, const char* text);

    /**
     * Sends packet to serial line
    */
    void sendToSerial(DataPacket& packet);

    /**
     * Sends byte to serial
    */
    void sendSerial(uint8_t byte);

};


#endif
