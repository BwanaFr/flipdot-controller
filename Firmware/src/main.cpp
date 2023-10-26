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

Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);

HardwareSerial serial485(2);
Screen screen(serial485);

AsyncWebServer server(80);
ESPEasyCfg captivePortal(&server, "FlipDot");


void WS2812Task(void *params);

static size_t otaContentLenght=0;                    // Size of the OTA update file

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
                      otaContentLenght = request->contentLength();
                      Serial.printf("Update with lenght of %d bytes\n", otaContentLenght);
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
    if(screen.sendText(text.c_str(), std::atoi(x.c_str()), std::atoi(y.c_str()))){
      request->send(200, "text/plain", "X: " + x + " Y: " + y + " Text : " + text);
    }else{
      request->send(429, "text/plain", "System is busy");
    }
  });

  //Raw binary data (for tests)
  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/raw", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonArrayConst jsonObj = json.as<JsonArrayConst>();
    uint8_t* data = new uint8_t[jsonObj.size()];
    int i=0;
    for(JsonVariantConst v : jsonObj) {
      data[i++] = v.as<uint8_t>();
    }
    screen.sendRaw(data, jsonObj.size());
    delete[] data;
    request->send(200, "text/plain", "ok");
  }, 4096);
  handler->setMethod(HTTP_POST);
  server.addHandler(handler);

  //Sets backlight on/off
  server.on("/backlight", HTTP_GET, [] (AsyncWebServerRequest *request) {
    bool blOn = false;
    if(request->hasParam("on")){
      blOn = true;
    }
    screen.setBackLight(blOn);
    String blOnText = (blOn ? "on" : "off");
    request->send(200, "text/plain", "Backlight is " + blOnText);
  });

  //Put all pixels on
  server.on("/white", HTTP_GET, [] (AsyncWebServerRequest *request) {
    screen.allWhite();
    request->send(200, "text/plain", "ok");
  });
}

void setup()
{
  Serial.begin(115200);

  //Setup screen
  screen.setPowerTimeout(600);
  screen.begin(RS485_RX_PIN, RS485_TX_PIN);

  //Setup HTTP endpoints
  setup_ota_update();
  setup_screen_endpoints();
  captive_portal_setup();
  //RGB led fading, just because we can
  xTaskCreatePinnedToCore(WS2812Task, "WS2812Task", 1024, NULL, 2, NULL, 1);
}

void loop()
{
  delay(10);
  screen.loop();
  /*Screen::TransactionStart tr;
  tr.fields.address = 0x6;
  tr.fields.mode = 0xab;
  Serial.printf("Size of transaction : %d\n", sizeof(Screen::TransactionStart));
  for(int i=0;i<sizeof(Screen::TransactionStart);++i){
    Serial.printf("Data[%d] = 0x%02x\n", i, tr.data[i]);
  }*/
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
