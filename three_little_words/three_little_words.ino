#include <Arduino.h>
#include <math.h>
#include "hardware/gpio.h"

#include <vector>

#include "utils.h"
#include "constants.h"
#include "hardware.h"
#include "apps.h"
#include "dsp.h"

typedef enum { 
  PAGE_SELECT,
  PARAM_SELECT,
  PARAM_MODIFY
} InputMode;

InputMode mode = PARAM_MODIFY;
#define NUM_APPS 3
App* apps[NUM_APPS];
int currentApp = 0;

void audio_callback() {
  App* app = apps[currentApp];
  app->Process();
}

void setup() {
  apps[0] = new Info(0,0,128,64,16);
  hw.Init(audio_callback);
}

void loop() {
  hw.Update();

  hw.display->setFont(u8g_font_6x10);
  hw.display->setFontRefHeightExtendedText();
  hw.display->setDrawColor(1);
  hw.display->setFontPosTop();
  hw.display->setFontDirection(0);
  hw.display->clearBuffer();
  apps[currentApp]->UpdateDisplay();
  hw.display->sendBuffer();
}
