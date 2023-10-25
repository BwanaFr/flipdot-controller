#include "screen.h"
#include "config.h"

Screen::Screen(HardwareSerial& serial, uint8_t address, uint8_t width, uint8_t height,
                uint8_t powerPin, int pwrSaveTime, uint8_t backlightPin) :
    serial_(serial), address_(address), width_(width), height_(height),
    powerPin_(powerPin), pwrSaveTime_(pwrSaveTime), backlightPin_(backlightPin),
    readSemaphore_(nullptr), writeSemaphore_(nullptr), buffers_(), readPos_(-1), writePos_(0)
{
    readSemaphore_ = xSemaphoreCreateMutex();
    writeSemaphore_ = xSemaphoreCreateMutex();
}

void Screen::begin(int8_t rxPin, int8_t txPin)
{
    serial_.begin(4800, SERIAL_8N1, rxPin, txPin);
    //Disable relays
    pinMode(powerPin_, OUTPUT);
    digitalWrite(powerPin_, HIGH);
    pinMode(backlightPin_, OUTPUT);
    digitalWrite(backlightPin_, HIGH);

    //Enable RS485 transceiver
    pinMode(RS485_EN_PIN, OUTPUT);
    digitalWrite(RS485_EN_PIN, HIGH);

    pinMode(RS485_SE_PIN, OUTPUT);
    digitalWrite(RS485_SE_PIN, HIGH);

    pinMode(PIN_5V_EN, OUTPUT);
    digitalWrite(PIN_5V_EN, LOW);
}

void Screen::sendSerial(uint8_t byte)
{
    serial_.write(byte);
    //Discard received bytes
    while(serial_.available()){
        serial_.flush();
    }
}

void Screen::sendText(const char* text, uint8_t x, uint8_t y, uint8_t font)
{
    if(xSemaphoreTake(writeSemaphore_, portMAX_DELAY) == pdTRUE)
    {
        DataPacket& writePacket = buffers_[writePos_];
        //Set text mode
        appendMode(writePacket, 0xa2);
        //Sets screen size (Not needed?)
        appendScreenSize(writePacket, width_, height_);
        //Sets text coordinates
        appendCoordinates(writePacket, x, y);
        //Sets font
        appendFont(writePacket, font);
        //Set text
        appendText(writePacket, text);

        //Sets the read index
        xSemaphoreTake(readSemaphore_, portMAX_DELAY);
            readPos_ = writePos_;
        xSemaphoreGive(readSemaphore_);

        if(++writePos_ > DATA_BUFFER_COUNT){
            //Move to first
            writePos_ = 0;
        }
        xSemaphoreGive(writeSemaphore_);
    }
}

void Screen::sendRaw(const uint8_t* data, int dataLen)
{
    if(xSemaphoreTake(writeSemaphore_, portMAX_DELAY) == pdTRUE)
    {
        DataPacket& writePacket = buffers_[writePos_];
        //Copy raw packet
        writePacket.count = 0;
        for(int i=0;i<dataLen;++i){
            writePacket << data[i];
        }
        //Sets the read index
        xSemaphoreTake(readSemaphore_, portMAX_DELAY);
            readPos_ = writePos_;
        xSemaphoreGive(readSemaphore_);

        if(++writePos_ > DATA_BUFFER_COUNT){
            //Move to first
            writePos_ = 0;
        }
        xSemaphoreGive(writeSemaphore_);
    }
}

void Screen::allWhite()
{
    if(xSemaphoreTake(writeSemaphore_, portMAX_DELAY) == pdTRUE)
    {
        DataPacket& writePacket = buffers_[writePos_];
        //Set text mode
        appendMode(writePacket, 0xa2);
        //Sets screen size (Not needed?)
        appendScreenSize(writePacket, width_, height_);
        //Sets graphics font
        appendFont(writePacket, 0x77);

        //Fill binary buffer
        int rowCount = height_/5;
        if((rowCount*5) != rowCount){
            ++rowCount;
        }
        for(int row = 0;row<(rowCount+1);++row){
            appendCoordinates(writePacket, 0, row > 0 ? ((row * 5) - 1) : 0);
            for(int x=0;x<width_;++x){
                writePacket << (0x20 | 0x1F);
            }
        }

        //Sets the read index
        xSemaphoreTake(readSemaphore_, portMAX_DELAY);
            readPos_ = writePos_;
        xSemaphoreGive(readSemaphore_);

        if(++writePos_ > DATA_BUFFER_COUNT){
            //Move to first
            writePos_ = 0;
        }
        xSemaphoreGive(writeSemaphore_);
    }
}

void Screen::loop()
{
    if(xSemaphoreTake(readSemaphore_, (TickType_t)10) == pdTRUE)
    {
        if(readPos_ > -1){
            if(digitalRead(powerPin_)){
                //Screen is off, power it
                digitalWrite(powerPin_, LOW);
                digitalWrite(PIN_5V_EN, HIGH);
                delay(500);
            }
            lastUsed_ = millis();
            sendToSerial(buffers_[readPos_]);
            readPos_ = -1;
        }
        xSemaphoreGive(readSemaphore_);
    }
    //Power off screen if timeout
    if(!digitalRead(powerPin_) && (pwrSaveTime_ > 0)){
        //Screen is on
        unsigned long now = millis();
        if((now - lastUsed_) > (pwrSaveTime_*1000)){
            //Not used since long time, power screen off
            digitalWrite(powerPin_, HIGH);
            digitalWrite(PIN_5V_EN, LOW);
        }
    }
}

void Screen::setBackLight(bool on)
{
    digitalWrite(backlightPin_, on ? LOW : HIGH);
}

void Screen::appendMode(DataPacket& packet, uint8_t mode)
{
    packet.count = 0;
    packet << mode;
}

void Screen::appendScreenSize(DataPacket& packet, uint8_t w, uint8_t h)
{
    packet << 0xd0;
    packet << w;
    packet << 0xd1;
    packet << h;
}

void Screen::appendCoordinates(DataPacket& packet, uint8_t x, uint8_t y)
{
    packet << 0xd2;
    packet << x;
    packet << 0xd3;
    packet << y;
}

void Screen::appendFont(DataPacket& packet, uint8_t font)
{
    packet << 0xd4;
    packet << font;
}

void Screen::appendText(DataPacket& packet, const char* text)
{
    if(text){
        while(*text != 0x0){
            packet << (*text);
            ++text;
        }
    }
}

void Screen::sendToSerial(DataPacket& packet)
{
    int chkSum = address_;
    Serial.println();
    //Start of data
    sendSerial(0xff);
    sendSerial(address_);

    //Send all bytes to serial line
    for(int i=0;i<packet.count;++i){
        sendSerial(packet.data[i]);
        chkSum += packet.data[i];
    }

    //End of transation, compute checksum
    uint8_t chk = (chkSum & 0xff);
    if(chk == 0xfe){
        sendSerial(0xfe);
        sendSerial(0x0);
    }else if(chk == 0xff){
        sendSerial(0xfe);
        sendSerial(0x01);
    }else{
        sendSerial(chk);
    }
    sendSerial(0xff);
    Serial.println();
}
