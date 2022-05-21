#ifndef APPS_H
#define APPS_H

#include <Arduino.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "utils.h"
#include "constants.h"
#include "hardware.h"
#include "apps.h"
#include "dsp.h"

TLWHardware hw;

class App {
public:
  App() {}
  virtual void NextParam() {}
  virtual void PrevParam() {}
  virtual void DecParam() {}
  virtual void IncParam() {}
  virtual void UpdateDisplay() {}
  virtual void Process() {}
};

class Info : public App {
public:
  double pitch[3];
  Info(int x, int y, int width, int height, int len) {
    for(int i=0;i<3;i++) pitch[i] = 1.0;
  }
  void UpdateDisplay() {
    char buffer[32];

    for(int i=0;i<3;i++) pitch[i] += (1.0/12.0) * hw.control[i]->GetDelta();
    for(int i=0;i<3;i++) {
      sprintf(buffer, "ENC_%d: v%d p%d h%d",
              i,
              hw.control[i]->encValue,
              hw.control[i]->encButtonPressed(),
              hw.control[i]->encButtonHeld);
      hw.display->drawStr(0, 0 + 16*i, buffer);
      sprintf(buffer, "BTN_%d: p%d h%d",
        i,
        hw.control[i]->topButtonPressed(),
        hw.control[i]->topButtonHeld);
      hw.display->drawStr(0, 8 + 16*i, buffer);
    }
    sprintf(buffer, "pitch: %f", pitch);
    hw.display->drawStr(0, 8 + 16*3, buffer);
  }
  void Process() {
    for(int i=0;i<3;i++) {
      hw.voctOut[0]->SetOutputVoltage(pitch[i]);
      hw.voctOut[0]->SetOffsetVoltage(VOCT_NOUT_MAX);
    }
  }
};

/*
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
  AnalogOut* analogOut;
  Seq(int x, int y, int width, int height, int len) {
    this->phase = 0;
    this->len = len;
    this->selectedParam = 0;
    this->playingParam = 0;
    this->values = (float*)malloc(sizeof(float)*this->len);
    this->xOffset = x; this->yOffset = y; this->appWidth = width; this->appHeight = height;
    for(int i=0;i<this->len;i++) values[i]=0.5;
    this->analogOut = new AnalogOut(2);
  }
  ~Seq() {
    free(this->values);
    delete analogOut;
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
      hw.display->drawRBox(i*colWidth, 56-height, max(1,colWidth-1), height, 2);
    }
    hw.display->drawRBox(this->selectedParam*colWidth, 57, colWidth, 3, 1);
    hw.display->drawRBox(this->playingParam*colWidth, 61, colWidth, 3, 1);
  }
  void Process() {
    phase+=10.0/SAMPLE_RATE;
    if(phase>1.0) {
      phase=fmod(phase,1.0);
      analogOut->SetOutputVoltage(0, values[this->playingParam]);
      analogOut->SetOffsetVoltage(1, OUTPUT_VMAX);
      playingParam=(playingParam+1)%len;
    }
  }
};

class AudioFxTest : public App {
public:
  float phase;
  float lastVal;
  #define CAPTURE_CHANNEL 0
  AudioFxTest(int x, int y, int width, int height, int len) {
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
  }
  void NextParam() {}
  void PrevParam() {}
  void DecParam() {}
  void IncParam() {}
  void UpdateDisplay() {
    char buffer[32];
    sprintf(buffer, "input: %f", lastVal);
    hw.display->drawStr(0, 0, buffer);
  }
  void Process() {
    while(!adc_fifo_is_empty()) {
      lastVal = ((adc_fifo_get()*1.0)/(1<<12)) - 0.5;
    }
    //analogOut->SetOutputVoltage(0, ((lastVal*abs(phase*2.0-1.0))+0.5)*OUTPUT_VMAX*2);
    //analogOut->SetOffsetVoltage(1, 0);
    phase+=5.0/SAMPLE_RATE;
    if(phase>1.0) phase=fmod(phase,1.0);
  }
};

class AudioInMonitor : public App {
public:
  float lastVal;
  float maxVal;
  float minVal;
  float avgVal;
  int xOffset;
  int yOffset;
  int appWidth;
  int appHeight;
  uint32_t samples;
  #define CAPTURE_CHANNEL 0
  AudioInMonitor(int x, int y, int width, int height, int len) {
    lastVal = 0;
    maxVal = 0;
    minVal = 0;
    avgVal = 0;
    samples=0;
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
    adc_set_clkdiv(CPU_SPEED/(SAMPLE_RATE/8.0));
    adc_run(true);
  }
  ~AudioInMonitor() {
  }
  void NextParam() {}
  void PrevParam() {}
  void DecParam() {}
  void IncParam() {}
  void UpdateDisplay() {
    char buffer[32];
    sprintf(buffer, "input: %4fv", (lastVal-1.666)*3.0);
    hw.display->drawStr(0, 0, buffer);
    sprintf(buffer, "max:   %4fv", (maxVal-1.666)*3.0);
    hw.display->drawStr(0, 12, buffer);
    sprintf(buffer, "min:   %4fv", (minVal-1.666)*3.0);
    hw.display->drawStr(0, 24, buffer);
    sprintf(buffer, "avg:   %4fv", (avgVal-1.666)*3.0);
    hw.display->drawStr(0, 36, buffer);
  }
  void Process() {
    while(!adc_fifo_is_empty()) {
      lastVal = (lastVal+((adc_fifo_get()*3.333)/(1<<12)))*0.5;
    }
    if(samples>44100) {
      maxVal = max(lastVal,maxVal);
      minVal = min(lastVal,minVal);
      float avgCoef = 1.0/1000.0;
      avgVal = lastVal*avgCoef+avgVal*(1.0-avgCoef);
    } else {
      minVal=lastVal;
      maxVal=lastVal;
      avgVal=lastVal;
    }
    samples++;
  }
};

class SelfPatchPlayground : public App {
public:
  AnalogOut* triOut;
  AnalogOut* sawOut;
  AnalogOut* pulseOut;
  Tri* tri;
  Saw* saw;
  Pulse* pulse;
  SelfPatchPlayground(int x, int y, int width, int height, int len) {
    this->triOut = new AnalogOut(2);
    this->sawOut = new AnalogOut(2, 255, 6);
    this->pulseOut = new AnalogOut(2, 255, 8);
    tri = new Tri(220);
    saw = new Saw(275);
    pulse = new Pulse(330);
  }
  void UpdateDisplay() {
    char buffer[32];
    sprintf(buffer, "tri: %dHz", 220);
    hw.display->drawStr(0, 5, buffer);
    sprintf(buffer, "saw: %dHz", 275);
    hw.display->drawStr(0, 15, buffer);
    sprintf(buffer, "pulse: %dHz", 330);
    hw.display->drawStr(0, 25, buffer);
  }
  void Process() {
    triOut->RawSet(2, FP_MUL(tri->Process(), 127) + 127);
    triOut->SetOffsetVoltage(1, OUTPUT_VMAX/2);
    sawOut->RawSet(6, FP_MUL(saw->Process(), 127) + 127);
    sawOut->SetOffsetVoltage(1, OUTPUT_VMAX/2);
    pulseOut->RawSet(8, FP_MUL(pulse->Process(), 127) + 127);
    pulseOut->SetOffsetVoltage(1, OUTPUT_VMAX/2);
  }
};

class EncoderTest : public App {
public:
  int ccwPresses;
  int cwPresses;
  int state;
  uint32_t samples;
  uint32_t lastSampleTriggered;
  EncoderTest(int x, int y, int width, int height, int len) {
    ccwPresses = 0;
    cwPresses = 0;
    state = 0;
    samples = 0;
    lastSampleTriggered = 0;
  }
  void UpdateDisplay() {
    char buffer[32];
    sprintf(buffer, "ccws: %d", ccwPresses);
    hw.display->drawStr(0, 0, buffer);
    sprintf(buffer, "cws: %d", cwPresses);
    hw.display->drawStr(0, 8, buffer);
    sprintf(buffer, "sampSince: %d", (samples - lastSampleTriggered));
    hw.display->drawStr(0, 16, buffer);
  }
  void DecParam() {
    if(state == 0 && (samples - lastSampleTriggered) > 1000) {
      state = -1;
    }
    if(state == 1) {
      cwPresses++;
      state = 0;
      lastSampleTriggered = samples;
    }
  }
  void IncParam() {
    if(state == 0 && (samples - lastSampleTriggered) > 1000) {
      state = 1;
    }
    if(state == -1) {
      state = 0;
      ccwPresses++;
      lastSampleTriggered = samples;
    }
  }
  void Process() {
    samples++;
  }
};
*/

#endif