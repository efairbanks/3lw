#ifndef APPS_H
#define APPS_H

#include <Arduino.h>
#include <vector>

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "constants.h"
#include "hardware.h"
#include "apps.h"
#include "dsp.h"

TLWHardware hw;

int wclip(int x) { return max(0, min(127, x)); }
int hclip(int x) { return max(0, min(63, x)); }
int wrap(int x, int w) { while(x<0) x+=w; while(x>=w) x-=w; return x; }
int clip(int x, int low, int high) { return max(low, min(high, x)); };

void drawFilledBox(int x, int y, int w, int h, int padding=0) {
  hw.display->drawBox(
    wclip(x + padding),
    hclip(y + padding),
    wclip(w - padding * 2),
    hclip(h - padding * 2));
}

void drawHollowBox(int x, int y, int w, int h, int padding=0) {
  hw.display->drawFrame(
    wclip(x + padding),
    hclip(y + padding),
    wclip(w - padding * 2),
    hclip(h - padding * 2));
}

void drawMeter(int x, int y, int w, int h, int padding, bool border, fp_t<int, 8> v) {
  int borderPadding = border ? 2 : 0;
  int maxFilledHeight = h - (padding + borderPadding);
  int filledHeight = int(fp_t<int, 12>(maxFilledHeight) * v) + (padding*2 + borderPadding);
  drawFilledBox(x, y + h - filledHeight, w, filledHeight, padding + borderPadding);
  if(border) drawHollowBox(x, y, w, h, padding);
}

void drawMeters(int x, int y, int w, int h, int padding, bool border, fp_t<int, 12> *v, int n) {
  x = x + padding;
  y = y + padding;
  w = w - padding * 2;
  h = h - padding * 2;
  if(border) drawHollowBox(x, y, w, h, 0);
  int meterWidth = w / n;
  x++; y++; w-=2; h-=2;
  int curMeterPos = 0;
  for(int i=0; i<n; i++) {
    int nextMeterPos = ((i + 1) * w) / n;
    if(i == n-1) nextMeterPos = w-1;
    drawMeter(
      x + curMeterPos + 1,
      y + 1,
      (nextMeterPos - curMeterPos) - 1,
      h - 2,
      0,
      false,
      v[i]
    );
    curMeterPos = nextMeterPos;
  }
}

void drawSteps(int x, int y, int w, int h, int padding, bool border, bool *v, int n) {
  x = x + padding;
  y = y + padding;
  w = w - padding * 2;
  h = h - padding * 2;
  if(border) drawHollowBox(x, y, w, h, 0);
  int meterWidth = w / n;
  x++; y++; w-=2; h-=2;
  int curMeterPos = 0;
  for(int i=0; i<n; i++) {
    int nextMeterPos = ((i + 1) * w) / n;
    if(i == n-1) nextMeterPos = w-1;
    drawMeter(
      x + curMeterPos + 1,
      y + 1,
      (nextMeterPos - curMeterPos) - 1,
      h - 2,
      0,
      false,
      fp_t<int, 12>(v[i] ? 1 : 0)
    );
    curMeterPos = nextMeterPos;
  }
}

void drawStepCounter(int x, int y, int w, int h, int padding, bool border, int index, int n) {
  x = x + padding;
  y = y + padding;
  w = w - padding * 2;
  h = h - padding * 2;
  if(border) drawHollowBox(x, y, w, h, 0);
  int meterWidth = w / n;
  x++; y++; w-=2; h-=2;
  int curMeterPos = 0;
  for(int i=0; i<n; i++) {
    int nextMeterPos = ((i + 1) * w) / n;
    if(i == n-1) nextMeterPos = w-1;
    drawMeter(
      x + curMeterPos + 1,
      y + 1,
      (nextMeterPos - curMeterPos) - 1,
      h - 2,
      0,
      false,
      fp_t<int, 12>(i == index ? 1 : 0)
    );
    curMeterPos = nextMeterPos;
  }
}

class Parameter {
private:
  char* name;
  int* value;
  int lastValue;
  int min;
  int max;
  int inc;
public:
  Parameter() = delete;
  Parameter(char* paramName, int* val, int minimum = 0, int maximum = 100, int incAmount = 1) {
    name = paramName;
    value = val;
    lastValue = value[0];
    min = minimum;
    max = maximum;
    inc = incAmount;
  }
  void Increase(int val) {
    val = val * inc;
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

  void AddParam(char* paramName, int* param, int min = 0, int max = 100, int incAmount = 1) {
    params.push_back(Parameter(paramName, param, min, max, incAmount));
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

class LittleApp : public App {
private:
  LittleApp() {}
public:
  int wordIndex;
  LittleApp(int i) {
    wordIndex = i;
  }
};

class LittleEnv : public LittleApp {
public:
  typedef enum { PARAM_ATTACK, PARAM_DECAY, PARAM_MODE, PARAM_LAST } SelectedParam;
  typedef fp_t<int, 10> param_t;
  typedef fp_t<int, 12> audio_t;
  SelectedParam selectedParam;
  ADEnv env;
  param_t attackSpeed;
  param_t decaySpeed;
  audio_t lastOut;
  bool hold;
  LittleEnv(int wordIndex) : LittleApp(wordIndex) {
    this->selectedParam = PARAM_ATTACK;
    this->attackSpeed = param_t(0);
    this->decaySpeed = param_t(0.30);
    this->hold = false;
    this->lastOut = 0;
  }

  param_t pclip(param_t x, param_t min = param_t(0), param_t max = param_t(1)) {
    if(x > max) x = max;
    if(x < min) x = min;
    return x;
  }

  void UpdateDisplay() {
    // handle controls
    if(hw.control[wordIndex]->encButtonPressed()) selectedParam = (SelectedParam)(((int)selectedParam + 1) % PARAM_LAST);
    int encDelta = hw.control[wordIndex]->GetDelta();
    if(encDelta != 0) {
      switch(selectedParam) {
        case PARAM_ATTACK:
          attackSpeed = pclip(attackSpeed + param_t(encDelta) * param_t(0.02));
          break;
        case PARAM_DECAY:
          decaySpeed = pclip(decaySpeed + param_t(encDelta) * param_t(0.02));
          break;
        case PARAM_MODE:
          hold = hold ? 0 : 1;
          break;
      }
    }
    // draw UI
    char buffer[64];
    int appOffset = (wordIndex*hw.display->getDisplayWidth())/NUM_WORDS;
    int appWidth = hw.display->getDisplayWidth()/3 - 1;
    hw.display->setFont(u8g2_font_baby_tf);
    sprintf(buffer, "%1sA:%d", selectedParam == PARAM_ATTACK ? "|" : "", int(this->attackSpeed * fp_t<int, 0>(100)));
    hw.display->drawStr(appOffset, 0, buffer);
    sprintf(buffer, "%1s%s:%d", selectedParam == PARAM_DECAY ? "|" : "", this->hold ? "R" : "D", int(this->decaySpeed * fp_t<int, 0>(100)));
    hw.display->drawStr(appOffset, 8, buffer);
    sprintf(buffer, "%1sH:%s ", selectedParam == PARAM_MODE ? "|" : "", this->hold ? "T" : "F");
    hw.display->drawStr(appOffset, 16, buffer);

    drawMeter(appOffset + appWidth/3, 16, appWidth/3, hw.display->getDisplayHeight() - 16, 2, true, lastOut);
  }
  void Process() {
    hw.trigIn[wordIndex]->Update();
    env.hold = hold;
    if(hw.trigIn[wordIndex]->RisingEdge()) env.Start();
    if(hw.trigIn[wordIndex]->FallingEdge()) env.Stop();
    param_t mAttackSpeed = pclip(attackSpeed + param_t(hw.analogIn[wordIndex] * audio_t(0.3 / 5.0)));
    param_t mDecaySpeed = pclip(decaySpeed + param_t(hw.analogIn[wordIndex] * audio_t(0.3 / 5.0)));
    env.SetAttackSpeed(mAttackSpeed);
    env.SetDecaySpeed(mDecaySpeed);
    audio_t out = env.Process();
    out = out * out;
    lastOut = out;
    audio_t envVal = lastOut * fp_t<int, 0>(5);
    hw.voctOut[wordIndex]->SetUnipolar(envVal);
    hw.cvOut[wordIndex]->SetUnipolar(envVal);
  }
};

class LittleCount : public LittleApp {
public:
  typedef enum { PARAM_DIVS, PARAM_STEP, PARAM_MAX, PARAM_LAST } SelectedParam;
  SelectedParam selectedParam;
  int divs;
  int step;
  int stepSize;
  fp_t<int, 10> maxVal;
  fp_t<int, 10> minVal;
  LittleCount(int wordIndex) : LittleApp(wordIndex) {
    selectedParam = PARAM_DIVS;
    divs = 5;
    step = divs-1;
    stepSize = 1;
    maxVal = fp_t<int, 10>(5);
    minVal = fp_t<int, 10>(0);
  }
  void UpdateDisplay() {
    // handle controls
    if(hw.control[wordIndex]->encButtonPressed()) selectedParam = (SelectedParam)(((int)selectedParam + 1) % PARAM_LAST);
    int encDelta = hw.control[wordIndex]->GetDelta();
    if(encDelta != 0) {
      switch(selectedParam) {
        case PARAM_DIVS:
          divs = max(2,min(100, divs + encDelta));
          break;
        case PARAM_STEP:
          stepSize = max(1,min(divs-1, stepSize + encDelta));
          break;
        case PARAM_MAX:
          maxVal += fp_t<int, 10>(0.05) * fp_t<int, 0>(encDelta);
          if(maxVal > fp_t<int, 10>(5.0)) maxVal = fp_t<int, 10>(5.0);
          if(maxVal < fp_t<int, 10>(0.0)) maxVal = fp_t<int, 10>(0.0);
          break;
      }
    }

    // draw UI
    char buffer[64];
    int appOffset = (wordIndex*hw.display->getDisplayWidth())/NUM_WORDS;
    int appWidth = hw.display->getDisplayWidth()/3 - 1;
    hw.display->setFont(u8g2_font_baby_tf);
    sprintf(buffer, "% 1.2f", float(maxVal));
    hw.display->drawStr(appOffset+15, 0, buffer);
    sprintf(buffer, "%2dU", stepSize);
    hw.display->drawStr(appOffset+17, 27, buffer);
    hw.display->drawVLine(appOffset+7, 2, hw.display->getDisplayHeight() - 4);
    for(int i=0; i<divs; i++) {
      int y = 2 + ((divs-1-i)*(hw.display->getDisplayHeight()-4))/(divs-1);
      hw.display->drawHLine(appOffset+3, y, 11);
      if(i==step) hw.display->drawCircle(appOffset+7, y, 3);
    }
  }
  void Process() {
    hw.trigIn[wordIndex]->Update();
    if(hw.trigIn[wordIndex]->RisingEdge()) {
      step = (step + stepSize) % divs;
      fp_t<int, 10> val = fp_t<int, 10>( ((maxVal - minVal) * fp_t<int, 0>(step)) / fp_t<int, 0>(divs-1) );
      val = fp_t<int, 10>(val + minVal);
      hw.cvOut[wordIndex]->SetUnipolar(val);
    }
    if(hw.analogIn[wordIndex] > fp_t<int, 12>(3.0 / 4.0)) {
      step = 0;
      fp_t<int, 10> val = fp_t<int, 10>( ((maxVal - minVal) * fp_t<int, 0>(step)) / fp_t<int, 0>(divs-1) );
      val = fp_t<int, 10>(val + minVal);
      hw.cvOut[wordIndex]->SetUnipolar(val);
    }
  }
};

class LittleLFO : public LittleApp {
public:
  typedef enum { PARAM_MULT, PARAM_LAST } SelectedParam;

  SelectedParam selectedParam;
  ClockRateDetector clockDetector;
  Phasor lfo;
  int multiplier;
  fp_t<int, 12> phase;

  LittleLFO(int wordIndex) : LittleApp(wordIndex) {
    selectedParam = PARAM_MULT;
    multiplier = 1;
    phase = 0;
  }

  fp_t<int, 12> Shape(fp_t<int, 12> x) {
    fp_t<int, 12> out = (x * fp_t<int, 0>(2)) - fp_t<int, 0>(1);
    if(out < fp_t<int, 12>(0)) out = -out;
    return out;
  }

  void DrawWaveform(int x, int y, int w, int h) {
    int lastHeight = int(Shape(0) * fp_t<int, 0>(h - 1));
    int index = int(phase * fp_t<int, 0>(w - 2)) + 1;
    fp_t<int, 12> invWidth = fp_t<int, 12>(1.0 / (w - 1));
    for(int i=1; i<w; i++) {
      int height = int(Shape(fp_t<int, 0>(i) * invWidth) * fp_t<int, 0>(h - 1));
      if(i == index) {
        hw.display->drawCircle(
          x + i - 1,
          y + (h - 1) - lastHeight,
          2
        );
      }
      hw.display->drawLine(
        x + i - 1,
        y + (h - 1) - lastHeight,
        x + i,
        y + (h - 1) - height
      );
      lastHeight = height;
    }
  }

  void UpdateDisplay() {
    // handle controls
    if(hw.control[wordIndex]->encButtonPressed()) selectedParam = (SelectedParam)(((int)selectedParam + 1) % PARAM_LAST);
    int encDelta = hw.control[wordIndex]->GetDelta();
    if(encDelta != 0) {
      switch(selectedParam) {
        case PARAM_MULT:
          multiplier = max(1,min(32, multiplier + encDelta));
          break;
      }
    }

    // prep UI
    char buffer[64];
    int appOffset = (wordIndex*hw.display->getDisplayWidth())/NUM_WORDS;
    int appWidth = hw.display->getDisplayWidth()/3 - 1;
    hw.display->setFont(u8g2_font_baby_tf);

    // draw UI text
    sprintf(buffer, "LFO");
    hw.display->drawStr(appOffset+8, 0, buffer);
    sprintf(buffer, "x%d", multiplier);
    hw.display->drawStr(appOffset+8, 8, buffer);

    drawHollowBox(appOffset + 1, 33, appWidth - 2, 30, 0);
    DrawWaveform(appOffset + 3, 35, appWidth - 6, 26);
  }

  void Process() {
      // update phase locked loop/LFO on clock input
      hw.trigIn[wordIndex]->Update();
      bool isRisingEdge = hw.trigIn[wordIndex]->RisingEdge();
      clockDetector.Process(isRisingEdge);
      if(isRisingEdge) {
        lfo.SetPeriodInSamples(clockDetector.lastIntervalInSamples, multiplier);
        lfo.phase = 0;
      }
      
      // output the LFO and inverse LFO values
      phase = lfo.Process();
      fp_t<int, 12> out = Shape(phase);
      hw.voctOut[wordIndex]->SetUnipolar((fp_t<int, 12>(1.0) - out) * fp_t<int, 0>(5.0));
      hw.cvOut[wordIndex]->SetUnipolar(out * fp_t<int, 0>(5.0));
  }
};

/*
class LittleMeter : public LittleApp {
public:
  typedef enum { PARAM_NONE, PARAM_LAST } SelectedParam;

  SelectedParam selectedParam;
  ClockRateDetector clockDetector;
  Phasor lfo;
  int none;
  fp_t<int, 26> lastInput;

  LittleMeter(int wordIndex) : LittleApp(wordIndex) {
    selectedParam = PARAM_NONE;
    none = 0;
  }

  void UpdateDisplay() {
    // handle controls
    if(hw.control[wordIndex]->encButtonPressed()) selectedParam = (SelectedParam)(((int)selectedParam + 1) % PARAM_LAST);
    int encDelta = hw.control[wordIndex]->GetDelta();
    if(encDelta != 0) {
      switch(selectedParam) {
        case PARAM_NONE:
          none = max(1,min(32, none + encDelta));
          break;
      }
    }

    // prep UI
    char buffer[64];
    int appOffset = (wordIndex*hw.display->getDisplayWidth())/NUM_WORDS;
    int appWidth = hw.display->getDisplayWidth()/3 - 1;
    hw.display->setFont(u8g2_font_baby_tf);

    fp_t<int, 12> out = lastInput;

    // draw UI text
    sprintf(buffer, "METER");
    hw.display->drawStr(appOffset+8, 0, buffer);
    sprintf(buffer, "%d", int(out*fp_t<int, 0>(1000)));
    hw.display->drawStr(appOffset+8, 8, buffer);

    // draw VU meter
    drawMeter(appOffset + appWidth/3, 16, appWidth/3, hw.display->getDisplayHeight() - 16, 2, true, (out + fp_t<int, 12>(5)) * fp_t<int, 12>(1.0 / 10.0));
  }

  void Process() {
    fp_t<int, 26> delta = hw.analogIn[wordIndex] - lastInput;
    lastInput = lastInput + (delta>>15);
  }
};
*/

class LittleClock : public LittleApp {
public:
  typedef enum { PARAM_BPM, PARAM_MULT_A, PARAM_MULT_B, PARAM_LAST } SelectedParam;

  SelectedParam selectedParam;
  ClockRateDetector clockDetector;
  TrigDetector trigDetectorA;
  TrigDetector trigDetectorB;
  TrigGen trigA;
  TrigGen trigB;
  Phasor clock;
  int multiplierA;
  int multiplierB;
  int bpm;

  LittleClock(int wordIndex) : trigA(0, 120), trigB(0, 120), LittleApp(wordIndex) {
    bpm = 120;
    selectedParam = PARAM_BPM;
    multiplierA = 1;
    multiplierB = 4;
    clock.SetBPM(bpm);
  }

  void UpdateDisplay() {
    // handle controls
    if(hw.control[wordIndex]->encButtonPressed()) selectedParam = (SelectedParam)(((int)selectedParam + 1) % PARAM_LAST);
    int encDelta = hw.control[wordIndex]->GetDelta();
    if(encDelta != 0) {
      switch(selectedParam) {
        case PARAM_BPM:
          bpm = max(30,min(300, bpm + encDelta));
          clock.SetBPM(bpm);
          break;
        case PARAM_MULT_A:
          multiplierA = max(1,min(32, multiplierA + encDelta));
          break;
        case PARAM_MULT_B:
          multiplierB = max(1,min(32, multiplierB + encDelta));
          break;
      }
    }

    // prep UI
    char buffer[64];
    int appOffset = (wordIndex*hw.display->getDisplayWidth())/NUM_WORDS;
    int appWidth = hw.display->getDisplayWidth()/3 - 1;
    hw.display->setFont(u8g2_font_baby_tf);

    // draw UI text
    if(selectedParam == PARAM_BPM) drawHollowBox(-1 + appOffset, -1 + 0, appWidth - 1, 11, 1);
    sprintf(buffer, "BPM:  %d", bpm);
    hw.display->drawStr(2 + appOffset, 0, buffer);
    if(selectedParam == PARAM_MULT_A) drawHollowBox(-1 + appOffset, -1 + 10, appWidth - 1, 11, 1);
    sprintf(buffer, "A:    x%d", multiplierA);
    hw.display->drawStr(2 + appOffset, 10, buffer);
    if(selectedParam == PARAM_MULT_B) drawHollowBox(-1 + appOffset, -1 + 20, appWidth - 1, 11, 1);
    sprintf(buffer, "B:    x%d", multiplierB);
    hw.display->drawStr(2 + appOffset, 20, buffer);
  }

  const fp_t<int, 12> multPhase(fp_t<int, 12> x, int m)
  {
    int temp = int(x<<12);
    temp = (temp * m) & 0x0FFF;
    return fp_t<int, 12>(temp)>>12;
  }

  void Process() {
    if(trigDetectorA.Process(fp_t<int, 12>(5) - multPhase(fp_t<int, 12>(clock.phase), multiplierA) * fp_t<int, 0>(5))) trigA.Reset();
    if(trigDetectorB.Process(fp_t<int, 12>(5) - multPhase(fp_t<int, 12>(clock.phase), multiplierB) * fp_t<int, 0>(5))) trigB.Reset();
    clock.Process();

    hw.voctOut[wordIndex]->SetUnipolar(trigA.Process() * fp_t<int, 0>(5.0));
    hw.cvOut[wordIndex]->SetUnipolar(trigB.Process() * fp_t<int, 0>(5.0));
  }
};

class ThreeLittleWords : public App {
public:
  typedef enum { ENV, COUNT, LFO, CLOCK, NUM_WORDTYPES } WordType;
  App* words[NUM_WORDS] = {NULL, NULL, NULL};
  WordType littleWords[NUM_WORDS] = {ENV, COUNT, LFO};
  ThreeLittleWords() {
    for(int i=0;i<NUM_WORDS;i++) {
      loadWord(i); 
    }
  }
  void loadWord(int word) {
    WordType type = littleWords[word];
    App* oldWord = words[word];
    App* newWord = NULL;
    switch(type) {
      case ENV:   newWord = new LittleEnv(word);    break;
      case COUNT: newWord = new LittleCount(word);  break;
      case LFO:   newWord = new LittleLFO(word);    break;
      case CLOCK: newWord = new LittleClock(word);  break;
      default: return; break;
    }
    words[word] = newWord;
    if(oldWord != NULL) delete oldWord;
  }
  void UpdateDisplay() {
    for(int i=0;i<NUM_WORDS;i++) {
      if(hw.control[i]->encButtonHeldFor > 20) {
        hw.control[i]->encButtonHeldFor = 0;
        littleWords[i] = (WordType)(((int)littleWords[i]) + 1);
        if(littleWords[i] >= NUM_WORDTYPES) littleWords[i] = (WordType)0;
        loadWord(i);
      }
      words[i]->UpdateDisplay();
      if(i>0) {
        hw.display->drawVLine((i*hw.display->getDisplayWidth())/3-2, 0, hw.display->getDisplayHeight());
      }
    }
  }
  void Process() {
    for(int i=0;i<NUM_WORDS;i++) words[i]->Process();
  }
};

class MultiSeq : public App {
public:
  class Track {
  public:
    int index;
    int editIndex;
    int len;
    fp_t<int, 12> cv[32];
    bool gate[32];
    Track() {
      index = 0;
      editIndex = 0;
      len = 8;
      for(int i=0; i<32; i++) {
        cv[i] = fp_t<int, 12>(1.0 / 5.0);
        gate[i] = false;
      }
    }
  };

  typedef enum { PARAM_TRACK, PARAM_LENGTH, PARAM_INDEX, PARAM_LAST } SelectedParam;

  SelectedParam selectedParam;
  Track tracks[NUM_WORDS];
  TrigGen trigGens[NUM_WORDS];
  TrigDetector trigDetectors[NUM_WORDS];
  int selectedTrack;

  void SetOutputs() {
    for(int i=0; i<NUM_WORDS;i++) hw.voctOut[i]->SetUnipolar(tracks[i].cv[tracks[i].index] * fp_t<int, 0>(5));
    for(int i=0; i<NUM_WORDS;i++) hw.cvOut[i]->SetUnipolar(trigGens[i].Process() * fp_t<int, 0>(5));
  }

  MultiSeq() {
    selectedParam = PARAM_TRACK;
    selectedTrack = 0;
    for(int i=0; i<NUM_WORDS; i++) tracks[i].len = 4<<i;
  }

  void UpdateDisplay() {
    char buffer[64];

    // handle controls
    if(hw.control[0]->encButtonPressed()) selectedParam = (SelectedParam)(((int)selectedParam + 1) % PARAM_LAST);
    int encDelta = hw.control[0]->GetDelta();
    if(encDelta != 0) {
      switch(selectedParam) {
        case PARAM_TRACK:
          selectedTrack = wrap(selectedTrack + encDelta, NUM_WORDS);
          break;
        case PARAM_LENGTH:
          tracks[selectedTrack].len = clip(tracks[selectedTrack].len + encDelta, 1, 32);
          break;
        case PARAM_INDEX:
          tracks[selectedTrack].index = wrap(tracks[selectedTrack].index + encDelta, tracks[selectedTrack].len);
          break;
      }
    }
    encDelta = hw.control[1]->GetDelta();
    if(encDelta != 0) {
      tracks[selectedTrack].editIndex = wrap(tracks[selectedTrack].editIndex + encDelta, tracks[selectedTrack].len);
    }
    encDelta = hw.control[2]->GetDelta();
    if(encDelta != 0) {
      fp_t<int, 12> currentVal = tracks[selectedTrack].cv[tracks[selectedTrack].editIndex];
      currentVal += fp_t<int, 12>(1.0 / (12.0 * 5.0)) * fp_t<int, 0>(encDelta);
      if(currentVal < fp_t<int, 12>(0)) currentVal = fp_t<int, 12>(0);
      if(currentVal > fp_t<int, 12>(1)) currentVal = fp_t<int, 12>(1);
      tracks[selectedTrack].cv[tracks[selectedTrack].editIndex] = currentVal;
    }
    if(hw.control[2]->encButtonPressed()) {
      tracks[selectedTrack].gate[tracks[selectedTrack].editIndex] = !tracks[selectedTrack].gate[tracks[selectedTrack].editIndex];
    }

    hw.display->setFont(u8g2_font_baby_tf);
    
    sprintf(buffer, "SEQ", 0);
    hw.display->drawStr(3, 0, buffer);
    if(selectedParam == PARAM_TRACK) drawHollowBox(-1, -1 + 20, 22, 11, 1);
    sprintf(buffer, "T:%d", selectedTrack);
    hw.display->drawStr(3, 20, buffer);
    if(selectedParam == PARAM_LENGTH) drawHollowBox(-1, -1 + 30, 22, 11, 1);
    sprintf(buffer, "L:%d", tracks[selectedTrack].len);
    hw.display->drawStr(3, 30, buffer);
    if(selectedParam == PARAM_INDEX) drawHollowBox(-1, -1 + 40, 22, 11, 1);
    sprintf(buffer, "S:%d", tracks[selectedTrack].index);
    hw.display->drawStr(3, 40, buffer);

    for(int line=1; line<5; line++) {
      int y = ((hw.display->getHeight() - 8) * line) / 5;
      for(int x=23; x<hw.display->getWidth() - 4; x+=3) {
        hw.display->drawPixel(x, y);
      }
    }
    drawMeters(
      20,
      -1,
      hw.display->getWidth() - 20,
      hw.display->getHeight() - 6,
      1,
      true,
      tracks[selectedTrack].cv,
      tracks[selectedTrack].len
    );
    drawStepCounter(20, hw.display->getHeight() - 10, hw.display->getWidth() - 20, 7, 1, false, tracks[selectedTrack].index, tracks[selectedTrack].len);
    drawStepCounter(20, hw.display->getHeight() - 10, hw.display->getWidth() - 20, 7, 1, false, tracks[selectedTrack].editIndex, tracks[selectedTrack].len);
    drawSteps(
      20,
      hw.display->getHeight() - 6,
      hw.display->getWidth() - 20,
      7,
      1,
      true,
      tracks[selectedTrack].gate,
      tracks[selectedTrack].len
    );
  }

  void Process() {
    for(int i=0; i<NUM_WORDS; i++) {
      if(trigDetectors[i].Process(hw.analogIn[i])) {
        tracks[i].index = wrap(tracks[i].index + 1, tracks[i].len);
        if(tracks[i].gate[tracks[i].index]) trigGens[i].Reset();
      }
      hw.trigIn[i]->Update();
      if(hw.trigIn[i]->RisingEdge()) {
        tracks[i].index = 0;
      }
    }
    SetOutputs();
  }
};

#endif