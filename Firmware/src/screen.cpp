#include "screen.h"
#include "config.h"

Screen::Screen(HardwareSerial& serial, uint8_t address, uint8_t width, uint8_t height) :
    serial_(serial), address_(address), width_(width), height_(height)
{
}

void Screen::begin(int8_t rxPin, int8_t txPin)
{
    serial_.begin(4800, SERIAL_8N1, rxPin, txPin);
}

void Screen::sendText(const char* text, uint8_t x, uint8_t y, uint8_t font)
{
    //TODO: Dynamic allocation here?
    uint8_t buffer[1024];
    int pos = 0;
    addHeader(buffer, pos);
    buffer[pos++] = 0xd2;       //Horizontal offset
    buffer[pos++] = x;
    buffer[pos++] = 0xd3;       //Vertical offset
    buffer[pos++] = y + 0xf;
    buffer[pos++] = 0xd4;       //Font
    buffer[pos++] = font;
    int len = strlen(text);
    strcpy((char*)&buffer[pos], text);
    pos += len;
    addCheckSum(buffer, pos);
    for(int i=0;i<pos;++i){
        serial_.write(buffer[i]);
        while(serial_.available()){
            serial_.flush();
        }
    }
}

void Screen::sendRaw(const uint8_t* data, int dataLen)
{
    uint8_t buffer[1024];
    int pos = 0;
    buffer[pos++] = 0xff;
    buffer[pos++] = address_;
    for(int i=0;i<dataLen;++i){
        buffer[pos++] = data[i];
    }
    addCheckSum(buffer, pos);
    for(int i=0;i<pos;++i){
        serial_.write(buffer[i]);
        while(serial_.available()){
            serial_.flush();
        }
    }
}

void Screen::addHeader(uint8_t* buffer, int& pos, uint8_t mode)
{
    buffer[pos++] = 0xff;
    buffer[pos++] = address_;
    buffer[pos++] = mode;
    buffer[pos++] = 0xd0;   //Width
    buffer[pos++] = width_;
    buffer[pos++] = 0xd1;   //Height
    buffer[pos++] = height_;
}

void Screen::addCheckSum(uint8_t* buffer, int& pos)
{
    int checkSum = 0x0;
    int i=0;
    for(i=1;i<pos;++i){
        checkSum += buffer[i];
    }
    buffer[pos] = (checkSum & 0xff);
    if(buffer[pos] == 0xfe){
        buffer[++pos] = 0x00;
    }else if(buffer[pos] == 0xff){
        buffer[pos] = 0xfe;
        buffer[++pos] = 0x01;
    }
    buffer[++pos] = 0xff;
    ++pos;
}
