#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <U8g2lib.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "fpmath.h"

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define NUM_WORDS 3

uint TOP_BTN_CCW[] = {0, 16, 21};
uint ENC_BTN_CW[]  = {1,17,22};
uint TRIG_IN[]     = {18, 19, 20};
uint CV_IN[]       = {26, 27, 28};
uint VOCT_OFFSET[] = {2, 6, 8};
uint CV_OFFSET[]   = {10, 12, 14};

class ButtonAndEncoder {
private:
  bool _topButtonPressed;
  bool _encButtonPressed;
public:
  enum State {
    NONE,
    TOP_CCW_TRIG,
    ENC_CW_TRIG
  };
  uint topButtonCCW;
  uint encButtonCW;
  int encValue;
  State state;
  uint64_t nextCanTrigger;
  uint64_t delayTime;
  bool topButtonHeld;
  bool encButtonHeld;
  bool topButtonPressed() {
    bool out = _topButtonPressed;
    _topButtonPressed = false;
    return out;
  }
    bool encButtonPressed() {
    bool out = _encButtonPressed;
    _encButtonPressed = false;
    return out;
  }
  ButtonAndEncoder(uint topButtonCcw, uint encButtonCw) {
    this->topButtonCCW = topButtonCcw;
    this->encButtonCW = encButtonCw;
    this->encValue = 0;
    this->state = NONE;
    this->nextCanTrigger = time_us_64();
    this->delayTime = 1000000/40;
    this->topButtonHeld = false;
    this->_topButtonPressed = false;
    this->encButtonHeld = false;
    this->_encButtonPressed = false;
    gpio_pull_up(topButtonCCW);
    gpio_pull_up(encButtonCW);
  }

  void Update() {
    if(time_us_64() > (nextCanTrigger + delayTime)) {
      bool topButtonState = !gpio_get(topButtonCCW);
      bool encButtonState = !gpio_get(encButtonCW);
      if(state != NONE) {
        _topButtonPressed |= !topButtonHeld && topButtonState;
        _encButtonPressed |= !encButtonHeld && encButtonState;
      }
      topButtonHeld = topButtonState;
      encButtonHeld = encButtonState;
      state = NONE;
    }
  }

  void Update(uint pin, uint32_t events) {
    switch(state) {
      case NONE:
        if(time_us_64() > nextCanTrigger) {
          if(pin == topButtonCCW && !gpio_get(pin)) {
            state = TOP_CCW_TRIG;
            nextCanTrigger = time_us_64() + delayTime;
          }
          if(pin == encButtonCW && !gpio_get(pin)) {
            state = ENC_CW_TRIG;
            nextCanTrigger = time_us_64() + delayTime;
          }
        }
        break;
      case TOP_CCW_TRIG:
        if(pin == encButtonCW && !gpio_get(pin)) {
          encValue++;
          state = NONE;
          nextCanTrigger = time_us_64() + delayTime;
        }
        break;
      case ENC_CW_TRIG:
        if(pin == topButtonCCW && !gpio_get(pin)) {
          encValue--;
          state = NONE;
          nextCanTrigger = time_us_64() + delayTime;
        }
        break;
    }
  }

};

class AnalogOut {
public:
  uint16_t res;
  uint8_t offset;
  AnalogOut(int numPins, int resolution = 255, int offset=2) {
    this->res = resolution;
    this->offset = offset;
    for(uint16_t i=0;i<numPins;i++) {
      uint slice_num = pwm_gpio_to_slice_num(i + offset);
      pwm_config cfg = pwm_get_default_config();
      pwm_config_set_clkdiv_int(&cfg, 1);
      pwm_config_set_wrap(&cfg, this->res);
      pwm_init(slice_num, &cfg, true);
      gpio_set_function(i + offset, GPIO_FUNC_PWM);
      pwm_set_gpio_level(i + offset, 0);
    }
  }
  void RawSet(uint pin, uint16_t level) {
    pwm_set_gpio_level(pin, level);
  }
  void Set(uint pin, float level) {
    pwm_set_gpio_level(pin + offset, (uint16_t)(level*this->res));
  }
  void SetOutputVoltage(uint pin, float v) {
    Set(pin, ((OUTPUT_VMAX-v))/OUTPUT_VMAX);
  }
  void SetOffsetVoltage(uint pin, float v) {
    Set(pin, (v)/OFFSET_VMAX);
  }
};

class TLWHardware {
public:
  static TLWHardware* _tlwhw_;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C* display;
  ButtonAndEncoder* control[NUM_WORDS];
  AnalogOut* voctOut[NUM_WORDS];
  AnalogOut* cvOut[NUM_WORDS];

  static void controlCallback(uint gpio, uint32_t events) {
    for(int i=0; i<NUM_WORDS; i++) {
      _tlwhw_->control[i]->Update(gpio, events);
    }
  }

  void Init() {
    if(_tlwhw_ == NULL) {
      display = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE, 5, 4);
      display->setBusClock(400000);
      display->begin();
      for(int i=0; i<NUM_WORDS; i++) {
        control[i] = new ButtonAndEncoder(TOP_BTN_CCW[i], ENC_BTN_CW[i]);
        gpio_set_irq_enabled_with_callback(TOP_BTN_CCW[i], GPIO_IRQ_EDGE_FALL, true, &controlCallback);
        gpio_set_irq_enabled_with_callback(ENC_BTN_CW[i], GPIO_IRQ_EDGE_FALL, true, &controlCallback);
        voctOut[i] = new AnalogOut(2, 255, VOCT_OFFSET[i]);
        cvOut[i]   = new AnalogOut(2, 255, CV_OFFSET[i]);
      }
      _tlwhw_ = this;
    }
  }

  void Update() {
    for(int i=0; i<NUM_WORDS; i++) {
      control[i]->Update();
    }
  }
};
TLWHardware* TLWHardware::_tlwhw_ = NULL;

#endif