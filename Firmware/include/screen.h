#ifndef _SCREEN_H__
#define _SCREEN_H__

#include <Arduino.h>
#include <HardwareSerial.h>
#include "config.h"

#define DATA_BUFFER_SIZE 1024
#define DATA_BUFFER_COUNT 2

//Ring buffer code from https://landenlabs.com/code/ring/ring.html
template <class T, size_t RingSize>
class RingBuffer
{
public:
    RingBuffer(size_t size = 100)
        : m_size(size), m_buffer(new T[size]), m_rIndex(0), m_wIndex(0)
        { assert(size > 1 && m_buffer != NULL); }

    ~RingBuffer()
        { delete [] m_buffer; };

    size_t Next(size_t n) const
        { return (n+1)%m_size; }
    bool Empty() const
        { return (m_rIndex == m_wIndex); }
    bool Full() const
        { return (Next(m_wIndex) == m_rIndex); }

    bool Put(const T& value)
    {
        if (Full())
            return false;
        m_buffer[m_wIndex] = value;
        m_wIndex = Next(m_wIndex);
        return true;
    }

    bool Get(T& value)
    {
        if (Empty())
            return false;
        value = m_buffer[m_rIndex];
        m_rIndex = Next(m_rIndex);
        return true;
    }

private:
    T*              m_buffer;
    size_t          m_size;

    // volatile is only used to keep compiler from placing values in registers.
    // volatile does NOT make the index thread safe.
    volatile size_t m_rIndex;
    volatile size_t m_wIndex;
};


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
     * @return false On error
    */
    bool sendText(const char* text, uint8_t x=0, uint8_t y=0, uint8_t font=0x73);

    /**
     * Send raw values (debugging)
     * Raw values doesn't contains checksum, address and first 0xff
    * @return false On error
    */
    bool sendRaw(const uint8_t* data, int dataLen);

    /**
     * Sets the screen will all dots
     * @return false On error
    */
    bool allWhite();

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
    SemaphoreHandle_t semaphore_;
    bool dataAvailable_;
    DataPacket buffer_;

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
