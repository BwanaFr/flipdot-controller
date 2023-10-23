#include "screen.h"
#include "config.h"

Screen::Screen(HardwareSerial& serial, uint8_t address, uint8_t width, uint8_t height) :
    serial_(serial), address_(address), width_(width), height_(height), checksum_(0)
{
}

void Screen::begin(int8_t rxPin, int8_t txPin)
{
    serial_.begin(4800, SERIAL_8N1, rxPin, txPin);
}

void Screen::startTransaction(uint8_t mode)
{
    checksum_ = 0;
    sendSerial(0xff, false);
    sendSerial(address_);
    sendSerial(mode);
}

void Screen::sendScreenSize()
{
    sendSerial(0xd0);   //Width
    sendSerial(width_);
    sendSerial(0xd1);   //Height
    sendSerial(height_);
}

void Screen::endTransaction()
{
    uint8_t chk = (checksum_ & 0xff);
    if(chk == 0xfe){
        sendSerial(0xfe, false);
        sendSerial(0x0, false);
    }else if(chk == 0xff){
        sendSerial(0xfe, false);
        sendSerial(0x01, false);
    }else{
        sendSerial(chk, false);
    }
    sendSerial(0xff, false);
    checksum_ = 0;
}

void Screen::sendSerial(uint8_t byte, bool addChk)
{
    serial_.write(byte);
    //Discard received bytes
    while(serial_.available()){
        serial_.flush();
    }
    if(addChk){
        checksum_ += byte;
    }
}

void Screen::sendText(const char* text, uint8_t x, uint8_t y, uint8_t font)
{
    startTransaction();
    sendScreenSize();
    sendSerial(0xd2);       //Horizontal offset
    sendSerial(x);
    sendSerial(0xd3);       //Vertical offset
    sendSerial(y + 0xf);
    sendSerial(0xd4);       //Font
    sendSerial(font);
    while(*text != 0x0){
        sendSerial(*text);
        ++text;
    }
    endTransaction();
}

void Screen::sendRaw(const uint8_t* data, int dataLen)
{
    startTransaction(data[0]);
    for(int i=1;i<dataLen;++i){
        sendSerial(data[i]);
    }
    endTransaction();
}
