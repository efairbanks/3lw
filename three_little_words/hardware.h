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

class Button {
public:
  // this class assumes a pulled-up pin that
  // is pulled down by a grounded button
  bool lastHeld;
  bool lastPressed;
  int pin;
  Button(int pin) {
    this->lastHeld = false;
    this->lastPressed = false;
    this->pin = pin;
    gpio_pull_up(pin);
  }
  void Update() {
    bool held = !gpio_get(pin);
    bool pressed = held && !this->lastHeld;
    this->lastHeld = held;
    this->lastPressed = pressed;
  }
  bool Pressed() { return this->lastPressed; }
  bool Held() { return this->lastHeld; }
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

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 5, /* data=*/ 4);   // ESP32 Thing, HW I2C with pin remapping

Button btn_a(21);
Button btn_b(16);
Button btn_c(0);

#endif