#include <Arduino.h>
#include "config.h"
#include <HardwareSerial.h>
#include "Freenove_WS2812_Lib_for_ESP32.h"

//For OTA update
#include <Update.h>
#include <ESPEasyCfg.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>

//Screen
#include "screen.h"

//Buildin RGB led
#define LEDS_COUNT 1
#define LEDS_PIN 4
#define CHANNEL 0

Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);

HardwareSerial serial485(2);
Screen screen(serial485);

AsyncWebServer server(80);
ESPEasyCfg captivePortal(&server, "FlipDot");


void WS2812Task(void *params);

size_t otaContentLenght;                    // Size of the OTA update file

/**
 * Sets the screen backlight on
*/
inline void backLightOn(){
  digitalWrite(BACKLIGHT_PWR_PIN, LOW);
}

/**
 * Sets the screen backlight off
*/
inline void backLightOff(){
  digitalWrite(BACKLIGHT_PWR_PIN, HIGH);
}

/**
 * Sets the screen power supply on
*/
inline void screenOn(){
  digitalWrite(SCREEN_PWR_PIN, LOW);
}

/**
 * Sets the screen power supply off
*/
inline void screenOff(){
  digitalWrite(SCREEN_PWR_PIN, HIGH);
}

/**
 * Callback to get captive-portal messages
*/
void captive_portal_message(const char* msg, ESPEasyCfgMessageType type) {
  Serial.println(msg);
}

/**
 * Callback from captive portal when state has changed
*/
void captive_portal_state(ESPEasyCfgState state) {
//   if(state == ESPEasyCfgState::Reconfigured){

//   }
}

/**
 * Setups the captive portal
 *
*/
void captive_portal_setup() {
  //Configure captive portal
  captivePortal.setMessageHandler(captive_portal_message);
  captivePortal.setStateHandler(captive_portal_state);
  //Start our captive portal (if not configured)
  //At first usage, you will find a new WiFi network named "Heater"
  captivePortal.begin();
  //Serve web pages
  server.begin();
}

/**
 * Setups OTA update
*/
void setup_ota_update() {
  server.on("/www/update", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<html><body><form method='POST' action='/www/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form></body></html>");
  });
  server.on("/www/update", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
                    if (!index){
                      Serial.println("Update");
                      otaContentLenght = request->contentLength();
                      // if filename includes spiffs, update the spiffs partition
                      int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
                      if (!Update.begin(otaContentLenght, cmd)) {
                        Update.printError(Serial);
                      }
                    }

                    if (Update.write(data, len) != len) {
                      Update.printError(Serial);
                    } else {
                      Serial.printf("Progress: %d%%\n", (Update.progress()*100)/Update.size());
                    }

                    if (final) {
                      AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
                      response->addHeader("Refresh", "20");
                      response->addHeader("Location", "/");
                      request->send(response);
                      if (!Update.end(true)){
                        Update.printError(Serial);
                      } else {
                        Serial.println("Update complete");
                        Serial.flush();
                        ESP.restart();
                      }
                    }
                  }
  );
}

void setup_screen_endpoints(){
  //Raw text
  server.on("/text", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String text;
    String x= "0";
    String y = "0";
    if(request->hasParam("X")){
      x = request->getParam("X")->value().c_str();
    }
    if(request->hasParam("Y")){
      y = request->getParam("Y")->value().c_str();
    }
    if(request->hasParam("text")){
      text = request->getParam("text")->value();
    }
    screen.sendText(text.c_str(), std::atoi(x.c_str()), std::atoi(y.c_str()));
    request->send(200, "text/plain", "X: " + x + " Y: " + y + " Text : " + text);
  });
  //Raw binary data (for tests)
  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/raw", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonArrayConst jsonObj = json.as<JsonArrayConst>();
    for(JsonVariantConst v : jsonObj) {
        Serial.println(v.as<int>());
    }
    request->send(200, "text/plain", "ok");
  });
  handler->setMethod(HTTP_POST);
  server.addHandler(handler);

  server.on("/backlight", HTTP_GET, [] (AsyncWebServerRequest *request) {
    bool blOn = false;
    if(request->hasParam("on")){
      blOn = true;
    }
    if(blOn){
      backLightOn();
    }else{
      backLightOff();
    }
    String blOnText = (blOn ? "on" : "off");
    request->send(200, "text/plain", "Backlight is " + blOnText);
  });

  server.on("/screen", HTTP_GET, [] (AsyncWebServerRequest *request) {
    bool scrOn = false;
    if(request->hasParam("on")){
      scrOn = true;
    }
    if(scrOn){
      screenOn();
    }else{
      screenOff();
    }
    String srcOnText = (scrOn ? "on" : "off");
    request->send(200, "text/plain", "Screen is " + srcOnText);
  });
}

void setup()
{
    pinMode(RS485_EN_PIN, OUTPUT);
    digitalWrite(RS485_EN_PIN, HIGH);

    pinMode(RS485_SE_PIN, OUTPUT);
    digitalWrite(RS485_SE_PIN, HIGH);

    pinMode(PIN_5V_EN, OUTPUT);
    digitalWrite(PIN_5V_EN, HIGH);

    pinMode(SCREEN_PWR_PIN, OUTPUT);
    pinMode(BACKLIGHT_PWR_PIN, OUTPUT);
    backLightOff();
    screenOn();

    Serial.begin(115200);

    setup_ota_update();
    setup_screen_endpoints();
    captive_portal_setup();

    screen.begin(RS485_RX_PIN, RS485_TX_PIN);
    xTaskCreatePinnedToCore(WS2812Task, "WS2812Task", 1024, NULL, 2, NULL, 1);
}

static int i=0;
void loop()
{
  delay(10);
}

void WS2812Task(void *params)
{
    strip.begin();
    strip.setBrightness(0x20);
    while (1) {
        for (int j = 0; j < 255; j += 2) {
            for (int i = 0; i < LEDS_COUNT; i++) {
                strip.setLedColorData(i, strip.Wheel((i * 256 / LEDS_COUNT + j) & 255));
            }
            strip.show();
            delay(50);
        }
    }
}
