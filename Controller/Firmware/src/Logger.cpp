#include "Logger.h"
#include <Arduino.h>

size_t Logger::write(uint8_t v){
    if(Serial){
        return Serial.write(v);
    }
    return 0;
}

size_t Logger::write(const uint8_t *buffer, size_t size){
    if(Serial){
        return Serial.write(buffer, size);
    }
    return 0;
}

int Logger::availableForWrite(){
    if(Serial) {
        return Serial.availableForWrite();
    }
    return 0;
}

void Logger::flush(){
    if(Serial){
        Serial.flush();
    }
}