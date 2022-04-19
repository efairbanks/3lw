#ifndef CONTROLS_H
#define CONTROLS_H

#include <Arduino.h>
#include "pico/stdlib.h"

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

#endif