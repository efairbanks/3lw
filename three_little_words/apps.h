#ifndef APPS_H
#define APPS_H

#include <Arduino.h>
#include <vector>

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "utils.h"
#include "constants.h"
#include "hardware.h"
#include "apps.h"
#include "dsp.h"

TLWHardware hw;

class Parameter {
private:
  char* name;
  int* value;
  int lastValue;
  int min;
  int max;
public:
  Parameter() = delete;
  Parameter(char* paramName, int* val, int minimum = 0, int maximum = 100) {
    name = paramName;
    value = val;
    lastValue = value[0];
    min = minimum;
    max = maximum;
  }
  void Increase(int val) {
    val += value[0];
    if(val>max) val = min;
    if(val<min) val = max;
    value[0] = val;
  }
  void Set(int val) {
    if(val>max) val = min;
    if(val<min) val = max;
    value[0] = val;
  }
  int Get() {
    return value[0];
  }
  char* GetName() {
    return name;
  }
  bool HasChanged() {
    bool result = lastValue == value[0];
    lastValue = value[0];
    return result;
  }
};

class App {
public:
  enum ParameterState { Modify, Select };
  std::vector<Parameter> params;
  int paramIndices[NUM_WORDS];
  ParameterState paramStates [NUM_WORDS];
  App() {
    for(int i=0;i<NUM_WORDS;i++) {
      paramIndices[i] = 0;
      paramStates[i] = Modify;
    }
  }

  void AddParam(char* paramName, int* param, int min = 0, int max = 100) {
    params.push_back(Parameter(paramName, param, min, max));
  }

  void UpdateParams() {
    if(params.size() > 0) {
      for(int i=0;i<NUM_WORDS;i++) {
        int controlDelta = hw.control[i]->GetDelta();
        if(hw.control[i]->encButtonPressed()) {
          paramStates[i] = paramStates[i] == Modify ? Select : Modify;
        }
        if(controlDelta != 0) {
          switch(paramStates[i]) {
            case Modify:
              params[paramIndices[i]].Increase(controlDelta);
              break;
            case Select:
              paramIndices[i] += controlDelta;
              while(paramIndices[i] < 0) paramIndices[i] += params.size();
              while(paramIndices[i] >= params.size()) paramIndices[i] -= params.size();
              break;
            default:
              paramStates[i] = Modify;
          }
        }
      }
    }
  }

  bool ParamsHaveChanged() {
    bool changed = false;
    for(int i=0;i<params.size();i++) {
      changed |= params[i].HasChanged();
    }
    return changed;
  }

  virtual void UpdateInternals() {

  }

  virtual void DrawParams() {
    if(params.size() > 0) {
      hw.display->setFont(u8g2_font_threepix_tr);
      hw.display->drawBox(0,63-5,127,63);
      hw.display->setDrawColor(0);
      hw.display->drawVLine(127/3,63-5,6);
      hw.display->drawVLine(127*2/3,63-5,6);
      for(int i=0;i<NUM_WORDS;i++) {
        if(paramStates[i] == Select) hw.display->drawStr(i*128/3 + 2, 63-7, "+");
        hw.display->drawStr(i*128/3 + 7, 63-6, params[paramIndices[i]].GetName());
      }
      hw.display->setDrawColor(1);
    }
  }

  virtual void NextParam() {}
  virtual void PrevParam() {}
  virtual void DecParam() {}
  virtual void IncParam() {}
  virtual void UpdateDisplay() {}
  virtual void Process() {}
};

class Info : public App {
public:
  Info(int x, int y, int width, int height, int len) {
  }
  void UpdateDisplay() {
    char buffer[32];
  }

  void Process() {
  }
};

class Arp : public App {
public:
  int pitches[3];
  Trigger* trig;
  Arp() {
    trig = new Trigger(150);
    for(int i=0;i<3;i++) {
      pitches[i] = 12;
    }
  }
  void UpdateDisplay() {
    char buffer[32];
    for(int i=0;i<3;i++) {
      pitches[i] += hw.control[i]->GetDelta();
      sprintf(buffer, "pitch%d: %d", i, pitches[i]);
      hw.display->drawStr(0, 12*i, buffer);
    }
  }
  int fifths = 0;
  int leadSeqIndex = 0;
  int bassSeqIndex = 0;
  int leadSeqLen = 14;
  int bassSeqLen = 2;
  int leadSeq[14] = {7,12,3,0,3,5,12,7,12,7,5,12,7,12};
  int bassSeq[2] = {0,3};
  lfp_signed semitone = FP_UNITY/12;
  void Process() {
    if(trig->Process()) {
      hw.voctOut[0]->SetCVFP(semitone*(leadSeq[leadSeqIndex]+fifths));
      hw.voctOut[1]->SetCVFP(semitone*(bassSeq[bassSeqIndex]+fifths));
      leadSeqIndex++;
      if(leadSeqIndex>=leadSeqLen) {
        leadSeqIndex = 0;
        bassSeqIndex = (bassSeqIndex+1)%bassSeqLen;
        fifths = (fifths+5)%12;
      }
    }
  }
};

class Scope : public App {
public:
  uint bufIndex;
  uint writtenSamples;
  uint phase;
  int activeChannel;
  fp_signed buf[32];
  Scope() {
    activeChannel = 0;
    AddParam("channel", &activeChannel);
    AddParam("test", &activeChannel);
    AddParam("another", &activeChannel);
    bufIndex = 0;
    writtenSamples = 0;
    phase = 0;
    for(int i=0;i<32;i++) buf[i] = 0;
  }
  void UpdateDisplay() {
    for(int i=0;i<31;i++) {
      uint bufi = (bufIndex+i)&0x1F;
      hw.display->drawLine(
        i<<2,
        min(63, 63 - FP_MUL(63, buf[bufi])),
        (i+1)<<2,
        min(63, 63 - FP_MUL(63, buf[(bufi+1)&0x1F]))
      );
    }
    writtenSamples = 0;
  }
  void Process() {
    if(writtenSamples < 32) {
      if(phase<1) {
        buf[bufIndex] = hw.analogIn[0];
        bufIndex = (bufIndex+1)&0x1F;
        writtenSamples++;
        phase = 100;
      } else {
        phase--;
      }
    }
  }
};

class NoteDetector : public App {
public:
  fp_signed voltage;
  OnePoleLP* lp;
  NoteDetector() {
    voltage = 0;
    lp = new OnePoleLP(FLOAT2LFP(0.001));
  }
  ~NoteDetector() {
    delete lp;
  }
  void UpdateDisplay() {
    char buffer[32];
    sprintf(buffer, "%f", round((FP2FLOAT(voltage)*10.68-5.29)*12));
    hw.display->drawStr(24, 24, buffer);
  }
  void Process() {
    voltage = lp->Process(hw.analogIn[0]);
  }
};

class Harnomia : public App {
public:
  int edo;
  int tones;
  int harmonic;
  int color;
  int root;
  int inverted;
  fp_signed invEdo;
  fp_signed freqs[99];
  Tri* oscs[NUM_WORDS];
  int xformTriggers[NUM_WORDS];
  char xforms[8] = {'<','>','v','^','-','+','o','?'};
  int numXforms;
  Harnomia() {
    this->edo       = 12;         AddParam("edo", &edo, 2, 99);
    this->tones     = 3;          AddParam("tones", &tones, 1, 99);
    this->harmonic  = 7;          AddParam("harmonic", &harmonic, 1, 99);
    this->color     = 4;          AddParam("color", &color, 1, 99);
    this->root      = 0;          AddParam("root", &root, 0, 99);
    this->inverted  = false;      AddParam("inverted", &inverted, 0, 1);
    this->xformTriggers[0] = 0;   AddParam("xformI", &xformTriggers[0], 0, 7);
    this->xformTriggers[1] = 3;   AddParam("xformII", &xformTriggers[1], 0, 7);
    this->xformTriggers[2] = 6;   AddParam("xformIII", &xformTriggers[2], 0 ,7);
    // --- //
    this->numXforms = 8;
    for(int i=0;i<NUM_WORDS;i++) {
      oscs[i] = new Tri(1);
    }
    UpdateInternals();
  }

  void UpdateInternals() {
    this->invEdo    = FP_UNITY/this->edo;
    for(int i=0;i<this->edo;i++) {
      freqs[i] = noteToFreq(i);
    }
    for(int i=0;i<NUM_WORDS;i++) {
      recalculateOutputs(i);
    }
  }

  ~Harnomia() {
    for(int i=0;i<NUM_WORDS;i++) {
      delete oscs[i];
    }
  }

  int wrapVal(int x, int max) {
    while(x<0) x+=max;
    while(x>=max) x-=max;
    return x;
  }

  int getColor() {
    return inverted ? harmonic-color : color;
  }

  fp_signed noteToFreq(int note) {
    return (fp_signed)(pow(2.0, ((float)note)/((float)edo))*261.63*4);
  }

  void UpdateDisplay() {
    char buffer[32];
    int radius = 23;
    int xoffset = radius+6;
    int yoffset = radius+3;

    sprintf(buffer, "   %d / %d", tones, edo);
    hw.display->drawStr(64, 0, buffer);

    sprintf(buffer, "%2d   %2d", color, harmonic);
    hw.display->drawStr(78, 20, buffer);
    hw.display->drawDisc(98, 27, 4);

    for(int i=0;i<3;i++) {
      sprintf(buffer, "%c", xforms[xformTriggers[i]]);
      hw.display->drawStr(
        64 + i*64/3 + 64/6,
        38,
        buffer
      );
    }

    sprintf(buffer, "%d", root);
    hw.display->drawStr(xoffset-3, yoffset-6, buffer);

    for(int i=0;i<edo;i++) {
      fp_signed xCoef = SIN_LUT[(FP_MUL(SIN_LEN,i*invEdo)+(SIN_LEN/4))%1024];
      fp_signed yCoef = SIN_LUT[FP_MUL(SIN_LEN,i*invEdo)];
      if(i==root) {
          hw.display->drawDisc(
          xoffset+FP_MUL(xCoef, radius),
          yoffset+FP_MUL(yCoef, radius),
          3
        );
      } else if(i==root || i==(root+getColor())%edo || i==(root+harmonic)%edo) {
        hw.display->drawCircle(
          xoffset+FP_MUL(xCoef, radius),
          yoffset+FP_MUL(yCoef, radius),
          3
        );
      } else {
        hw.display->drawLine(
          xoffset+FP_MUL(xCoef, radius-2),
          yoffset+FP_MUL(yCoef, radius-2),
          xoffset+FP_MUL(xCoef, radius+2),
          yoffset+FP_MUL(yCoef, radius+2)
        );
      }
    }
  }

  void Transform(char xform) {
    switch(xform) {
      case '<':
        inverted = inverted ? 0 : 1;
        root -= getColor();
        break;
      case '>':
        root += getColor();
        inverted = inverted ? 0 : 1;
        break;
      case 'v':
        root -= 1;
        break;
      case '^':
        root += 1;
        break;
      case '-':
        color -= 1;
        break;
      case '+':
        color += 1;
        break;
      case 'o':
        root = 0;
        color = 4;
        break;
      case '?':
        Transform(xforms[rand()%(numXforms-2)]);
        break;
    }
    edo       = max(wrapVal(edo, 99), 2);
    tones     = max(wrapVal(tones, edo), 1);
    root      = wrapVal(root, edo);
    harmonic  = max(wrapVal(harmonic, edo), 2);
    color     = max(wrapVal(color, harmonic), 1);
  }

  void recalculateOutputs(int i) {
    int index = i + FP_MUL(11, hw.analogIn[i]);
    int octave = index/tones;
    int tone = (root + getInterval(index - octave*tones));
    while(tone>=edo) tone-=edo;
    hw.voctOut[i]->SetCVFP((invEdo*tone)<<octave);
    oscs[i]->SetFreq((freqs[tone]<<octave)>>3);
  }

  void processAudioOutputs() {
    for(int i=0;i<3;i++) {
      hw.cvOut[i]->SetAudioFP(this->oscs[i]->Process());
    }
  }

  int getInterval(int index) {
    return harmonic*(index>>1) + (index&0x1 ? getColor() : 0);
  }

  fp_signed getFreq(fp_signed index) {
    int octave = index/tones;
    return freqs[getInterval(index - octave*tones) % edo]<<octave;
  }

  fp_signed getVoct(fp_signed index) {
    int octave = index/tones;
    return invEdo*(getInterval(index - octave*tones) % edo)<<octave;
  }

  int outputToRecalculate = 0;
  void Process() {
    for(int i=0;i<NUM_WORDS;i++) {
      hw.trigIn[i]->Update();
      if(hw.trigIn[i]->RisingEdge()) {
        Transform(xforms[this->xformTriggers[i]]);
      }
    }
    recalculateOutputs(outputToRecalculate);
    if(++outputToRecalculate > NUM_WORDS-1) outputToRecalculate = 0;
    processAudioOutputs();
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