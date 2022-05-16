#include <Arduino.h>
#include <math.h>

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
  u8g2.setBusClock(400000);
  u8g2.begin();
  apps[0] = new SelfPatchPlayground(0,0,128,64,16);
  apps[1] = new Seq(0,0,128,64,16);
  apps[2] = new AudioInMonitor(0,0,128,64,16);
  add_repeating_timer_us(-TIMER_INTERVAL, audio_callback, NULL, &timer);
}

void loop() {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
  u8g2.clearBuffer();
  
  btn_a.Update();
  btn_b.Update();
  btn_c.Update();

  if(btn_c.Pressed()) {
    switch(mode) {
      case PARAM_SELECT:
        mode=PARAM_MODIFY;
      break;
      case PARAM_MODIFY:
        mode=PARAM_SELECT;
      break;
      default:
        mode=PARAM_SELECT;
    }
  } else if(btn_c.Held()) {
      if(btn_a.Pressed()) currentApp--;
      if(btn_b.Pressed()) currentApp++;
      while(currentApp<0) currentApp += NUM_APPS;
      while(currentApp>=NUM_APPS) currentApp -= NUM_APPS;
  }

  switch(mode) {
    case PARAM_SELECT:
      if(btn_a.Pressed()) apps[currentApp]->PrevParam();
      if(btn_b.Pressed()) apps[currentApp]->NextParam();
    break;
    case PARAM_MODIFY:
      if(btn_a.Held()) apps[currentApp]->DecParam();
      if(btn_b.Held()) apps[currentApp]->IncParam();
    break;
  }

  apps[currentApp]->UpdateDisplay();
  u8g2.sendBuffer();
}
