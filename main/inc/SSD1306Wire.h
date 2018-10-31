/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 by Daniel Eichhorn
 * Copyright (c) 2016 by Fabrice Weinberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Credits for parts of this code go to Mike Rankin. Thank you so much for sharing!
 */

#ifndef SSD1306Wire_h
#define SSD1306Wire_h

#include <stdint.h>
#include "stdio.h"
#include "OLEDDisplay.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "I2C.h"

#define _min(X, Y) (((X) < (Y)) ? (X) : (Y))
#define _max(X, Y) (((X) > (Y)) ? (X) : (Y))

class SSD1306Wire : public OLEDDisplay {
  private:
      uint8_t             _address;
      I2C*                puerto;
  public:
    SSD1306Wire(uint8_t _address, I2C* i2cp) {
      this->_address = _address;
      this->puerto = i2cp;
    }

    bool connect() {

        //Presumed that channel was already setup
    //  Wire.begin(this->_sda, this->_scl);
      // Let's use ~700khz if ESP8266 is in 160Mhz mode
      // this will be limited to ~400khz if the ESP8266 in 80Mhz mode.
    //  Wire.setClock(700000);
   // 	i2c_config_t conf;
    //	conf.master.clk_speed = 700000;
//   	ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
      return true;
    }

    void display(void) {
      #ifdef OLEDDISPLAY_DOUBLE_BUFFER

        uint8_t minBoundY = ~0;
        uint8_t maxBoundY = 0;

        uint8_t minBoundX = ~0;
        uint8_t maxBoundX = 0;
        uint8_t x, y;

        // Calculate the Y bounding box of changes
        // and copy buffer[pos] to buffer_back[pos];
        for (y = 0; y < (DISPLAY_HEIGHT / 8); y++) {
          for (x = 0; x < DISPLAY_WIDTH; x++) {
           uint16_t pos = x + y * DISPLAY_WIDTH;
           if (buffer[pos] != buffer_back[pos]) {
             minBoundY = _min(minBoundY, y);
             maxBoundY = _max(maxBoundY, y);
             minBoundX = _min(minBoundX, x);
             maxBoundX = _max(maxBoundX, x);
           }
           buffer_back[pos] = buffer[pos];
         }
    //     yield();
        }

        // If the minBoundY wasn't updated
        // we can savely assume that buffer_back[pos] == buffer[pos]
        // holdes true for all values of pos
    //    if (minBoundY == ~0) return;
        if (minBoundY == 255) return;

        sendCommand(COLUMNADDR);
        sendCommand(minBoundX);
        sendCommand(maxBoundX);

        sendCommand(PAGEADDR);
        sendCommand(minBoundY);
        sendCommand(maxBoundY);

        uint8_t k = 0;
        for (y = minBoundY; y <= maxBoundY; y++) {
          for (x = minBoundX; x <= maxBoundX; x++) {
            if (k == 0) {
                puerto->address=_address;
                puerto->beginTransaction();
                puerto->write(0x40,1);
         //     Wire.beginTransmission(_address);
         //     Wire.write(0x40);
            }
              puerto->write((uint8_t)buffer[x + y * DISPLAY_WIDTH],1);
           // Wire.write(buffer[x + y * DISPLAY_WIDTH]);
            k++;
            if (k == 16)  {
                puerto->endTransaction();
            //  Wire.endTransmission();
              k = 0;
            }
          }
        //  yield();
        }

        if (k != 0) {
             puerto->endTransaction();
       //   Wire.endTransmission();
        }
      #else
    //	ESP_LOGI("RSN","DisplayNormalBuf");

        sendCommand(COLUMNADDR);
        sendCommand(0x0);
        sendCommand(0x7F);

        sendCommand(PAGEADDR);
        sendCommand(0x0);
        sendCommand(0x7);

        for (uint16_t i=0; i < DISPLAY_BUFFER_SIZE; i++) {
            puerto->address=_address;
            puerto->beginTransaction();
            puerto->write(0x40,1);
       //   Wire.beginTransmission(this->_address);
       //   Wire.write(0x40);
          for (uint8_t x = 0; x < 16; x++) {
              puerto->write(buffer[i],1);
          //  Wire.write(buffer[i]);
            i++;
          }
          i--;
            puerto->endTransaction();

       //   Wire.endTransmission();
        }
      #endif
    }

 // private:
    inline void sendCommand(uint8_t command) __attribute__((always_inline)){
    //	ESP_LOGI("RSN", "send Command %x Address %x",command,puerto->address);
        puerto->address=_address;
        puerto->beginTransaction();
        puerto->write(0x80,1);
        puerto->write(command,1);
        puerto->endTransaction();
        /*
      Wire.beginTransmission(_address);
      Wire.write(0x80);
      Wire.write(command);
      Wire.endTransmission();
         */
    }


};

#endif
