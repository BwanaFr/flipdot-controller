/**
 *  Flipdot controller firmware
**/


#include <Arduino.h>
#include "pin_config.h"
#include "FlipDotGFX.h"
#include "Logger.h"

//ESPEasyCfg includes
#include <ESPEasyCfg.h>
#include <Update.h>


//Flipdot object
static FlipdotGFX<19, 112> flipDot;

//ESPEasyCfg objects
AsyncWebServer server(80);
ESPEasyCfg captivePortal(&server, "Flipdot");

//OTA
static size_t otaContentLenght=0;                    // Size of the OTA update file

//Remove this and use the native logging https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html
Logger logger;

/**
 * Setups OTA update
*/
void setup_ota_update() {
  server.on("/www/update", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<html><body><div>OTA version 1</div><form method='POST' action='/www/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form></body></html>");
  });
  server.on("/www/update", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
                    if (!index){
                      otaContentLenght = request->contentLength();
                      logger.printf("Update with lenght of %d bytes\n", otaContentLenght);
                      // if filename includes spiffs, update the spiffs partition
                      int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
                      if (!Update.begin(otaContentLenght, cmd)) {
                        Update.printError(Serial);
                      }
                    }

                    if (Update.write(data, len) != len) {
                      Update.printError(Serial);
                    } else {
                      logger.printf("Progress: %d%%\n", (Update.progress()*100)/Update.size());
                    }

                    if (final) {
                      AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
                      response->addHeader("Refresh", "20");
                      response->addHeader("Location", "/");
                      request->send(response);
                      if (!Update.end(true)){
                        Update.printError(Serial);
                      } else {
                        logger.println("Update complete");
                        // logger.flush();
                        ESP.restart();
                      }
                    }
                  }
  );
}


/**
 * Performs setup of captive portal
*/
void captive_portal_setup() {
  //Start our captive portal (if not configured)
  //At first usage, you will find a new WiFi network named "Flipdot"
  captivePortal.begin();
  //Serve web pages
  server.begin();
}

void setup_screen_endpoints(){
  //I/O controls
  server.on("/io", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(request->hasParam("BL")){
      //Switch on/off the back light
      digitalWrite(BACKLIGHT, !digitalRead(BACKLIGHT));
    }
      if(request->hasParam("COIL")){
      digitalWrite(COIL, !digitalRead(COIL));
    }
    if(request->hasParam("LED")){
      digitalWrite(LED, !digitalRead(LED));
    }
    int blStatus = digitalRead(BACKLIGHT);
    int coilStatus = digitalRead(COIL);
    int ledStatus = digitalRead(LED);
    String statusStr = "BL: ";
    statusStr = statusStr + blStatus + " COIL: " + coilStatus + " LED : " + ledStatus;
    request->send(200, "text/plain", statusStr);
  });

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
    if(request->hasParam("clear")){
      flipDot.fillScreen(0);
    }
    flipDot.setTextColor(1, 0);
    flipDot.setCursor(std::atoi(x.c_str()), std::atoi(y.c_str()));
    flipDot.print(text);
    request->send(200, "text/plain", "X: " + x + " Y: " + y + " Text : " + text);
  });

  server.on("/reverse", HTTP_GET, [] (AsyncWebServerRequest *request) {
    flipDot.invertDisplay(!flipDot.isInverted());
    if(flipDot.isInverted()){
      request->send(200, "text/plain", "Inverted!");
    }else{
      request->send(200, "text/plain", "Normal!");
    }
  });
}

/**
 * Arduino setup
*/
void setup(){
  //Basic HW init
  Serial.begin(115200);
  Serial.setDebugOutput(true);  //Configure ESP logging (for the future)
  //LED
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  //Backlight DC/DC control
  pinMode(BACKLIGHT, OUTPUT);
  digitalWrite(BACKLIGHT, LOW);
  //OTA
  setup_ota_update();
  //Screens endpoints
  setup_screen_endpoints();
  //Initialize captive portal
  captive_portal_setup();
  //Starts flipdot
  flipDot.begin();
}

unsigned long lastSend = 0;
int i=0;

/**
 * Arduino loop
*/
void loop(){
#ifdef SPI_DEBUG
  unsigned long now = millis();
  if(lastSend == 0){
    lastSend = now;
  }
  if((now - lastSend) > 1000){
    flipDot.setCursor(0,0);
    flipDot.setTextColor(1, 0);
    flipDot.print(i++);
    lastSend = now;
  }
#endif
  delay(20);
}
