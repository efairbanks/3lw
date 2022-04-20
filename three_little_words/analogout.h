#ifndef ANALOGOUT_H
#define ANALOGOUT_H

#include "hardware/pwm.h"
#include "settings.h"

class AnalogOut {
public:
  uint16_t res;
  AnalogOut(int numPins, int resolution = 255) {
    this->res = resolution;
    for(uint16_t i=0;i<numPins;i++) {
      uint slice_num = pwm_gpio_to_slice_num(i);
      pwm_config cfg = pwm_get_default_config();
      pwm_config_set_clkdiv_int(&cfg, 1);
      pwm_config_set_wrap(&cfg, this->res);
      pwm_init(slice_num, &cfg, true);
      gpio_set_function(i, GPIO_FUNC_PWM);
      pwm_set_gpio_level(i, 0);
    }
  }
  void Set(uint16_t pin, float level) {
    pwm_set_gpio_level(pin, (uint16_t)(level*this->res));
  }
  void SetOutputVoltage(uint16_t pin, float v) {
    Set(pin, ((OUTPUT_VMAX-v))/OUTPUT_VMAX);
  }

  void SetOffsetVoltage(uint16_t pin, float v) {
    Set(pin, (v)/OFFSET_VMAX);
  }
};

#endif