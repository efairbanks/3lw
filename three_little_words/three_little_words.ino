#include <Arduino.h>
#include <math.h>
#include "hardware/gpio.h"

#include <vector>

#include "utils.h"
#include "constants.h"
#include "hardware.h"
#include "apps.h"
#include "dsp.h"

struct repeating_timer timer;

typedef enum 
{ 
  PAGE_SELECT,
  PARAM_SELECT,
  PARAM_MODIFY
} InputMode;

InputMode mode = PARAM_MODIFY;
#define NUM_APPS 3
App* apps[NUM_APPS];
int currentApp = 0;

bool audio_callback(struct repeating_timer *t) {
  App* app = apps[currentApp];
  app->Process();
  return true;
}

void setup() {
  hw.Init();
  apps[0] = new Info(0,0,128,64,16);
  add_repeating_timer_us(-TIMER_INTERVAL, audio_callback, NULL, &timer);
}

void loop() {
  hw.display->setFont(u8g_font_6x10);
  hw.display->setFontRefHeightExtendedText();
  hw.display->setDrawColor(1);
  hw.display->setFontPosTop();
  hw.display->setFontDirection(0);
  hw.display->clearBuffer();
  
  hw.Update();

  apps[currentApp]->UpdateDisplay();
  hw.display->sendBuffer();
}
