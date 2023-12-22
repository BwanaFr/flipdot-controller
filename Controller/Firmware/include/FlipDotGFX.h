#ifndef _FLIP_DOT_GFX_H__
#define _FLIP_DOT_GFX_H__

#include <Adafruit_GFX.h>
#include <esp_task.h>
#include "pin_config.h"
#include <SPI.h>
#include <queue.h>

template<size_t ROWS=19, size_t COLS=112>
class FlipdotGFX : public Adafruit_GFX {

public:
    /**
     * Constructor
     * @param serPin SPI MOSI pin for shift register
     * @param srClkPin SPI CLK pin for shift register
     * @param rClkPin Shift register latch pin
     * @param srClrPin Shift register clear pin
     * @param coilPwrPin Coil DC/DC power control pin
    */
    FlipdotGFX(uint8_t serPin = MOSI,
        uint8_t srClkPin = SCK, uint8_t rClkPin = SS,
        uint8_t srClrPin = 3, uint8_t coilPwrPin = 47) :
        Adafruit_GFX(COLS, ROWS), serPin_(serPin), srClkPin_(srClkPin),
        rClkPin_(rClkPin), srClrPin_(srClrPin), coilPwrPin_(coilPwrPin),
        fspi_(nullptr), fBuffer_{}, updateQueue_(nullptr), inverted_(false)
    {
        updateQueue_ = xQueueCreate(queueSize_, sizeof(PixelData));
    }

    virtual ~FlipdotGFX() = default;

    void pushPixel(int16_t x, int16_t y, bool on, bool force)
    {
        if ((x >= 0) && (x < width()) && (y >= 0) && (y < height())) {
            if((getPixel(x, y) != on) || force){
                setPixel(x, y, on);
                if(updateQueue_ != NULL){
                    PixelData d(x, y, on);
                    xQueueSendToBack(updateQueue_, (void*)&d, portMAX_DELAY);
                }
            }
        }
    }

    void drawPixel(int16_t x, int16_t y, uint16_t color) override
    {
        pushPixel(x, y, (inverted_ ? (color == 0) : (color != 0)), false);
    }

    void invertDisplay(bool i) override
    {
        inverted_ = i;
        for(int x=0; x<width(); ++x){
            for(int y=0; y<height(); ++y){
                bool pixel = getPixel(x, y);
                pushPixel(x, y, !pixel, true);
            }
        }
    }

    /**
     * Display initialization
    */
    void begin()
    {
        //Configure I/O and SPI
        pinMode(srClrPin_, OUTPUT);
        pinMode(coilPwrPin_, OUTPUT);
        digitalWrite(coilPwrPin_, LOW);
        digitalWrite(srClrPin_, HIGH);
        fspi_ = new SPIClass(FSPI);
        fspi_->begin(srClkPin_, -1, serPin_, rClkPin_);
        fspi_->setHwCs(true);
        clearOutputs();
        //Create FreeRTOS task to update the screen
        xTaskCreate(updateTask, "FlipDotUpdate", 8192, this, 1, NULL);
        //Clears the screen
        for(int y=0; y<height(); ++y){
            for(int x=0; x<width(); ++x){
                pushPixel(x, y, false, true);
            }
        }
    }

    /**
     * Is the screen inverted?
    */
    inline bool isInverted(){ return inverted_; }

private:
    /**
     * Structure to update one pixel
    */
    struct PixelData{
        int16_t x;
        int16_t y;
        bool on;

        PixelData(int16_t x, int16_t y, bool value) : x(x), y(y), on(value) {}
        PixelData() : x(0), y(0), on(false) {}
    };

    static constexpr int queueSize_ = 50;   //!< Maximum size of the queue
    static constexpr unsigned long coilTimeout_ = 1000;//!< DC/DC switch off time
    static constexpr uint32_t spiSpeed_ = 10000000;    //!< SPI clock speed (Hz)
    static constexpr int colsPerChip_ = 7;                                              //!< One panel column chip drives 7 columns
    static constexpr int colsChipsPerPanel_ = 4;                                        //!< One panel have 4 chips
    static constexpr int colsPerPanel_ = colsPerChip_ * colsChipsPerPanel_;             //!< Number of columns per flipdot panel
    static constexpr int rowsPerChip_ = 7;                                              //!< One chip drives 7 rows
    static constexpr uint32_t flipTime_ = 200;                                          //!< Dot flip time (us)

    /**
     * Gets a pixel from the frame buffer (without any checks)
    */
    inline bool getPixel(int16_t x, int16_t y) { return fBuffer_[x][y]; }

    /**
     * Sets a pixel to the frame buffer
    */
    inline void setPixel(int16_t x, int16_t y, bool value) { fBuffer_[x][y] = value; }

    /**
     * FreeRTOS update task
    */
    static void updateTask(void* instance)
    {
        FlipdotGFX* obj = reinterpret_cast<FlipdotGFX*>(instance);
        obj->updateScreen();
        //updateScreen should be an endless loop, but...
        vTaskDelete(NULL);
    }

    /**
     * Screen update function
    */
    void updateScreen()
    {
        unsigned long lastCoilOn = 0;   //Last time the coil is switched on
        if(updateQueue_ == NULL) {
            //kill task, it's not expected here...
            return;
        }
        while(true) {
            PixelData d;
            //Waits for an update within 1 ms
            if(xQueueReceive(updateQueue_, &(d), pdMS_TO_TICKS(10)) == pdPASS){
                if(digitalRead(coilPwrPin_) == LOW){
                    //Needs to switch on the DC/DC converter
                    digitalWrite(coilPwrPin_, HIGH);
                    digitalWrite(LED, HIGH);
                    lastCoilOn = millis();
                    delay(5);
                }
                outputPixel(d);
            }else{
                //Queue receive timeout, check if we need to switch off the DC/DC converter
                if((digitalRead(coilPwrPin_) == HIGH) && ((millis() - lastCoilOn) >= coilTimeout_)){
                    digitalWrite(coilPwrPin_, LOW);
                    digitalWrite(LED, LOW);
                }
            }
        }
    }

    /**
     * Ouput the pixel to hardware
    */
    void outputPixel(const PixelData& d)
    {
        //Shift register data
        uint8_t data[] = {0x0, 0x0, 0x0};
        //Prepare byte for the last shift register
        //This shift register controls the panel selection
        int panel = (d.x / colsPerPanel_);
        data[0] = (0x1 << panel);

        //Prepare byte for the middle shift register
        //This shift registers controls the column address
        int panelCol = d.x - (panel * colsPerPanel_);
        int colChip = (panelCol / colsPerChip_);            //Chip selection (74HC4514)
        int col = panelCol - (colChip * colsPerChip_) + 1;  //Column selection on chip
        data[1] = (colChip & 0x3) << 3;
        data[1] |= col & 0x7;
        if(d.on == 0x0){
            data[1] |= 0x20;    //Select high/low side of column
        }

        //Last shift register
        //This chip drives the row selection
        int rowChip = (d.y / rowsPerChip_);             //Chip select
        int row =  d.y - (rowChip * rowsPerChip_) + 1;  //Row chip selection
        data[2] = (rowChip & 0x3) << 3;
        data[2] |= row & 0x7;
        if(d.on != 0x0){
            data[2] |= 0x40;    //Select high/low side of row
        }else{
            data[2] |= 0x20;    //Select high/low side of row
        }

    #ifdef SPI_DEBUG
        Serial.printf("%02x\t%02x\t%02x\n", data[0], data[1], data[2]);
    #endif
        //Send pixels to shift registers
        //Latch is done automatically uising the chip select of SPI
        fspi_->beginTransaction(SPISettings(spiSpeed_, SPI_MSBFIRST, SPI_MODE0));
        fspi_->transfer(data, 3);
        fspi_->endTransaction();
        //Energize magnet
        delayMicroseconds(flipTime_);
        //Resets outputs
        clearOutputs();
    }

    /**
     * Resets outputs
    */
    void clearOutputs()
    {
        //Clear shift registers
        digitalWrite(srClrPin_, LOW);
        //Assert latch, we may do better here
        fspi_->setHwCs(false);
        pinMode(rClkPin_, OUTPUT);
        digitalWrite(rClkPin_, LOW);
        digitalWrite(rClkPin_, HIGH);
        fspi_->setHwCs(true);
        digitalWrite(srClrPin_, HIGH);
    }

    uint8_t serPin_;                        //!< Serial data pin
    uint8_t srClkPin_;                      //!< Serial clock pin
    uint8_t rClkPin_;                       //!< Latch pin
    uint8_t srClrPin_;                      //!< Shift register clear pin
    uint8_t coilPwrPin_;                    //!< Coil power pin
    SPIClass* fspi_;                        //!< Fast SPI reference
    bool fBuffer_[COLS][ROWS];              //!< Frame buffer object
    QueueHandle_t updateQueue_;             //!< Queue for sending pixel update
    bool inverted_;                         //!< Is display inverted?
};


#endif
