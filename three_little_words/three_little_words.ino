#include <Arduino.h>
#include <U8g2lib.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "RPi_Pico_TimerInterrupt.h"

#include "utils.h"
#include "settings.h"
#include "analogout.h"
#include "apps.h"
#include "controls.h"

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

typedef enum 
{ 
  SEQ
} AppPage;

class App {
public:
  App() {}
  void UpdateDisplay() {}
};

struct repeating_timer timer;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 5, /* data=*/ 4);   // ESP32 Thing, HW I2C with pin remapping
AnalogOut* analogOut;

typedef enum 
{ 
  PAGE_SELECT,
  PARAM_SELECT,
  PARAM_MODIFY
} InputMode;

class Seq : public App {
public:
  int len;
  int selectedParam;
  int playingParam;
  float* values;
  float phase;
  int xOffset;
  int yOffset;
  int appWidth;
  int appHeight;
  int width;
  int height;
  Seq(int x, int y, int width, int height, int len) {
    this->len = len;
    this->selectedParam = 0;
    this->values = (float*)malloc(sizeof(float)*this->len);
    this->xOffset = x; this->yOffset = y; this->appWidth = width; this->appHeight = height;
    for(int i=0;i<this->len;i++) values[i]=0.5;
  }
  ~Seq() {
    free(this->values);
  }
  void NextParam() {this->selectedParam = (this->selectedParam+1)%this->len;}
  void PrevParam() {this->selectedParam = (this->selectedParam-1); if(this->selectedParam<0) this->selectedParam += this->len;}
  void DecParam() {
    values[this->selectedParam] -= 0.05;
  }
  void IncParam() {
    values[this->selectedParam] += 0.05;
  }
  void UpdateDisplay() {
    float colWidth = 128.0/len;
    for(int i=0;i<len;i++) {
      float height = 56*values[i];
      float padding = 1.0;
      u8g2.drawRBox(i*colWidth, 56-height, max(1,colWidth-1), height, 2);
    }
    u8g2.drawRBox(this->selectedParam*colWidth, 57, colWidth, 3, 1);
    u8g2.drawRBox(this->playingParam*colWidth, 61, colWidth, 3, 1);
  }
  void Process() {
    phase+=10.0/SAMPLE_RATE;
    if(phase>1.0) {
      phase=fmod(phase,1.0);
      analogOut->SetOutputVoltage(0, values[playingParam]);
      analogOut->SetOffsetVoltage(1, OUTPUT_VMAX);
      playingParam=(playingParam+1)%len;
    }
  }
};

class AudioFxTest : public App {
public:
  int len;
  float* values;
  float phase;
  float lastVal;
  int xOffset;
  int yOffset;
  int appWidth;
  int appHeight;
  int width;
  int height;
  #define CAPTURE_CHANNEL 0
  #define CAPTURE_DEPTH 1000
  uint8_t capture_buf[CAPTURE_DEPTH];
  AudioFxTest(int x, int y, int width, int height, int len) {
    this->len = len;
    this->xOffset = x; this->yOffset = y; this->appWidth = width; this->appHeight = height;
    adc_gpio_init(26 + CAPTURE_CHANNEL);
    adc_init();
    adc_select_input(CAPTURE_CHANNEL);
    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        false,    // Enable DMA data request (DREQ)
        1,       // DREQ (and IRQ) asserted when at least 1 sample present
        false,   // We won't see the ERR bit because of 8 bit reads; disable.
        false     // Shift each sample to 8 bits when pushing to FIFO
    );
    adc_set_clkdiv(CPU_SPEED/SAMPLE_RATE);
    adc_run(true);
  }
  ~AudioFxTest() {
    //free(this->values);
  }
  void NextParam() {}
  void PrevParam() {}
  void DecParam() {}
  void IncParam() {}
  void UpdateDisplay() {
    char buffer[32];
    sprintf(buffer, "input: %f", lastVal);
    u8g2.drawStr(0, 0, buffer);
  }
  void Process() {
    while(!adc_fifo_is_empty()) {
      lastVal = ((adc_fifo_get()*1.0)/(1<<12)) - 0.5;
    }
    analogOut->SetOutputVoltage(0, ((lastVal*abs(phase*2.0-1.0))+0.5)*OUTPUT_VMAX*2);
    analogOut->SetOffsetVoltage(1, 0);
    phase+=5.0/SAMPLE_RATE;
    if(phase>1.0) phase=fmod(phase,1.0);
  }
};

Seq mainApp(0,0,128,64,16);
InputMode mode = PARAM_MODIFY;

Button btn_a(7);
Button btn_b(8);
Button btn_c(9);

bool repeating_timer_callback(struct repeating_timer *t) {
  mainApp.Process();
  return true;
}

void setup() {
  u8g2.setBusClock(400000);
  u8g2.begin();
  analogOut = new AnalogOut(2, 1024);
  add_repeating_timer_us(-TIMER_INTERVAL, repeating_timer_callback, NULL, &timer);
}

void drawPoly(int sides, float angle) {
  int lastX=(int)(cos(angle)*30.0+64.0);
  int lastY=(int)(sin(angle)*30.0+32.0);
  for(int i=1;i<sides+1;i++) {
    int x=(int)(cos(i*M_PI*2.0/sides + angle)*30.0+64.0);
    int y=(int)(sin(i*M_PI*2.0/sides + angle)*30.0+32.0);
    u8g2.drawLine(lastX,lastY,x,y);
    lastX=x;
    lastY=y;
  }
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
  }

  switch(mode) {
    case PAGE_SELECT:
    break;
    case PARAM_SELECT:
      if(btn_a.Pressed()) mainApp.PrevParam();
      if(btn_b.Pressed()) mainApp.NextParam();
    break;
    case PARAM_MODIFY:
      if(btn_a.Held()) mainApp.DecParam();
      if(btn_b.Held()) mainApp.IncParam();
    break;
  }

  mainApp.UpdateDisplay();
  
  u8g2.sendBuffer();
  //delay(1000/30);
}
