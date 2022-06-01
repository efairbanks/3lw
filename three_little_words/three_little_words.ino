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
App* app;
int appIndex = 0;

void audio_callback() {
  app->Process();
}

void setup() {
  INIT_FPMATH();
  app = new Scope();
  hw.Init(audio_callback);
}

App* getAppByIndex(int index) {
  switch(index) {
    case 0:
      return new Scope();
    case 1:
      return new Harnomia();
    default:
      return getAppByIndex(index%2);
  }
}

void loop() {
  hw.Update();

  if(hw.control[0]->topButtonPressed()) {
    hw.SetAudioCallback(NULL);
    sleep_ms(10);
    delete app;
    app = getAppByIndex(++appIndex);
    hw.SetAudioCallback(audio_callback);
  }

  hw.display->setFont(u8g2_font_pixzillav1_tf);
  hw.display->setFontRefHeightExtendedText();
  hw.display->setDrawColor(1);
  hw.display->setFontPosTop();
  hw.display->setFontDirection(0);
  hw.display->clearBuffer();
  app->UpdateDisplay();
  hw.display->sendBuffer();
}
