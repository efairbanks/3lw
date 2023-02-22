#include <Arduino.h>
#include <math.h>
#include "hardware/gpio.h"

#include <vector>

#include "constants.h"
#include "hardware.h"
#include "apps.h"
#include "dsp.h"

App* app;
int appIndex = 0;

void audio_callback() {
  app->Process();
}

App* getAppByIndex(int index) {
  switch(index) {
    case 0:
      return new MultiSeq();
    case 1:
      return new ThreeLittleWords();
    default:
      return getAppByIndex(index%2);
  }
}

void setup() {
  char buffer[64];

  set_sys_clock_khz(250000, true);

  app = getAppByIndex(0);
  hw.Init(audio_callback);

  hw.display->setFont(u8g2_font_pixzillav1_tf);
  hw.display->setFontRefHeightExtendedText();
  hw.display->setDrawColor(1);
  hw.display->setFontPosTop();
  hw.display->setFontDirection(0);
  hw.display->clearBuffer();
  sprintf(buffer, "i love you");
  hw.display->drawStr(20, 28, buffer);
  hw.display->sendBuffer();
  sleep_ms(2000);
}

void loop() {
  hw.Update();

  if(hw.control[0]->topButtonHeld && hw.control[1]->topButtonHeld && hw.control[2]->topButtonPressed()) {
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
  app->UpdateParams();
  if(app->ParamsHaveChanged()) {
    app->UpdateInternals();
  }
  app->UpdateDisplay();
  app->DrawParams();
  hw.display->sendBuffer();
}
