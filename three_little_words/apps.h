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
  SelectedParam selectedParam;
  ADEnv env;
  int attackSpeed;
  int decaySpeed;
  typedef fp_t<int, 12> audio_t;
  audio_t voctCoef;
  audio_t cvCoef;
  bool hold;
  LittleEnv(int wordIndex) : LittleApp(wordIndex) {
    this->selectedParam = PARAM_ATTACK;
    this->attackSpeed = 12;
    this->decaySpeed = 4;
    this->hold = true;
    this->voctCoef = audio_t(5.0/6.49);
    this->cvCoef = audio_t(5.0/8.72);
  }
  void UpdateDisplay() {
    // handle controls
    if(hw.control[wordIndex]->encButtonPressed()) selectedParam = (SelectedParam)(((int)selectedParam + 1) % PARAM_LAST);
    int encDelta = hw.control[wordIndex]->GetDelta();
    if(encDelta != 0) {
      switch(selectedParam) {
        case PARAM_ATTACK:
          attackSpeed += encDelta;
          break;
        case PARAM_DECAY:
          decaySpeed += encDelta;
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
    hw.display->setFont(u8g2_font_missingplanet_tf);
    sprintf(buffer, "A: %d %s", this->attackSpeed, selectedParam == PARAM_ATTACK ? "*" : "");
    hw.display->drawStr(appOffset+2, 0, buffer);
    sprintf(buffer, "%s: %d %s", this->hold ? "R" : "D", this->decaySpeed, selectedParam == PARAM_DECAY ? "*" : "");
    hw.display->drawStr(appOffset+2, 15, buffer);
    sprintf(buffer, "H: %s %s", this->hold ? "T" : "F", selectedParam == PARAM_MODE ? "*" : "");
    hw.display->drawStr(appOffset+2, 30, buffer);
  }
  void Process() {
    hw.trigIn[wordIndex]->Update();
    env.hold = hold;
    if(hw.trigIn[wordIndex]->RisingEdge()) env.Start();
    if(hw.trigIn[wordIndex]->FallingEdge()) env.Stop();
    int speedScaler = int(hw.analogIn[wordIndex] * fp_t<int, 0>(128)); // FP_BITS
    env.SetAttackSpeed((attackSpeed*attackSpeed*speedScaler)>>7);
    env.SetDecaySpeed((decaySpeed*decaySpeed*speedScaler)>>7);
    audio_t envVal = env.Process();
    audio_t out = audio_t(envVal * voctCoef);
    hw.voctOut[wordIndex]->SetCycles(0);
    hw.voctOut[wordIndex]->SetCyclesOffset(((int)(fp_t<int,0>(hw.voctOut[wordIndex]->res)*voctCoef)) - ((int)(fp_t<int,0>(hw.voctOut[wordIndex]->res) * out)));
    out = audio_t(envVal * cvCoef);
    hw.cvOut[wordIndex]->SetCycles(0);
    hw.cvOut[wordIndex]->SetCyclesOffset((int)(fp_t<int,0>(hw.cvOut[wordIndex]->res) * out));
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
          if(maxVal < fp_t<int, 10>(-5.0)) maxVal = fp_t<int, 10>(-5.0);
          break;
      }
    }

    // draw UI
    char buffer[64];
    int appOffset = (wordIndex*hw.display->getDisplayWidth())/NUM_WORDS;
    int appWidth = hw.display->getDisplayWidth()/3 - 1;
    hw.display->setFont(u8g2_font_missingplanet_tf);
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
      val = fp_t<int, 10>((val + minVal) * fp_t<int, 10>(0.2));
      hw.cvOut[wordIndex]->SetBipolar(val);
    }
    if(hw.analogIn[wordIndex] > fp_t<int, 12>(3.0 / 4.0)) {
      step = 0;
      fp_t<int, 10> val = fp_t<int, 10>( ((maxVal - minVal) * fp_t<int, 0>(step)) / fp_t<int, 0>(divs-1) );
      val = fp_t<int, 10>((val + minVal) * fp_t<int, 10>(0.2));
      hw.cvOut[wordIndex]->SetBipolar(val);
    }
  }
};

class ThreeLittleWords : public App {
public:
  typedef enum { ENV, COUNT, NUM_WORDTYPES } WordType;
  App* words[NUM_WORDS] = {NULL, NULL, NULL};
  WordType littleWords[NUM_WORDS] = {ENV, COUNT, ENV};
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

#endif