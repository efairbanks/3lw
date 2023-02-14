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
  Metronome* trig;
  Arp() {
    trig = new Metronome(150);
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
  fp_signed semitone = FP_UNITY/12;
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
    lp = new OnePoleLP(FLOAT2FP(0.001));
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
  Saw* oscs[NUM_WORDS];
  Metronome cvMetro;
  int xformTriggers[NUM_WORDS];
  char xforms[8] = {'<','>','v','^','-','+','o','?'};
  int numXforms;
  Trigger* analogTriggers[NUM_WORDS];
  int voiceIndex[NUM_WORDS];
  int selectedVoice;
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
    this->selectedVoice = 0;
    this->numXforms = 8;
    for(int i=0;i<NUM_WORDS;i++) {
      oscs[i] = new Saw(1);
      analogTriggers[i] = new Trigger((FP_UNITY*3)/5);
      voiceIndex[i] = 0;
    }
    cvMetro.SetFreq(1000);
    UpdateInternals();
  }

  void UpdateInternals() {
    this->invEdo = FP_UNITY/this->edo;
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
    int index = i + voiceIndex[i];
    int octave = index/tones;
    int tone = (root + getInterval(index - octave*tones));
    while(tone>=edo) tone-=edo;
    hw.voctOut[i]->SetCVFP(invEdo*tone+octave*FP_UNITY);
    oscs[i]->SetFreq((freqs[tone]<<octave)>>(i+2));
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
      if(analogTriggers[i]->Process(hw.analogIn[i])) {
        switch(i) {
          case 0:
            if(++selectedVoice>=NUM_WORDS) selectedVoice = 0;
            break;
          case 1:
            if(++voiceIndex[selectedVoice]>tones) voiceIndex[selectedVoice] = 0;
            break;
          case 2:
            if(--voiceIndex[selectedVoice]<0) voiceIndex[selectedVoice] = tones;
            break;
        }
      }
    }
    if(cvMetro.Process()) {
      recalculateOutputs(outputToRecalculate);
      if(++outputToRecalculate > NUM_WORDS-1) outputToRecalculate = 0;
    }
    processAudioOutputs();
  }
};

class LFO : public App {
public:
  Tri* oscs[3];
  int rate;
  int coef;
  int maxRate;
  int maxCoef;
  LFO() {
    for(int i=0;i<3;i++) oscs[i] = new Tri(1);
    rate = 30;  AddParam("rate", &rate, 0, 127);
    coef = 30;  AddParam("coef", &coef, 0, 127);
    maxRate = FP_UNITY*5;
    maxCoef = FP_UNITY*5;
  }
  ~LFO() {
    for(int i=0;i<3;i++) delete oscs[i];
  }
  void UpdateInternals() {
    int delta = FP_MUL(SAMPLEDELTA, (maxRate*rate)>>7);
    for(int i=0;i<3;i++) {
      oscs[i]->phasor->delta = delta;
      delta = FP_MUL(delta, (maxCoef*coef)>>7);
    }
  }
  void UpdateDisplay() {
    char buffer[128];
    hw.display->setFont(u8g2_font_missingplanet_tf);
    sprintf(buffer, " %1.3f * ( %1.3f ^ N )", FP2FLOAT((maxRate*rate)>>7), FP2FLOAT((maxCoef*coef)>>7));
    hw.display->drawStr(0, 0, buffer);
  }
  void Process() {
    for(int i=0;i<3;i++) {
      hw.cvOut[i]->SetAudioFP(this->oscs[i]->Process());
    }
  }
};

class InputCalibrator : public App {
public:
  int samplesToAverage;
  double offsets[3];
  double coefs[3];
  double zeroVals[3];
  double neg3Vals[3];
  bool initialized;
  InputCalibrator() {
    samplesToAverage = 1000;
    initialized = false;
  }
  ~InputCalibrator() {}
  void UpdateInternals() {

  }
  void UpdateDisplay() {
    char buffer[64];
    hw.display->setFont(u8g2_font_missingplanet_tf);

    if(!initialized) {
      for(int i=0;i<NUM_WORDS;i++) {
        // record value of 0v signal
        hw.voctOut[i]->Set(0.0);
        hw.cvOut[i]->SetOffset(0.0);
        zeroVals[i] = 0.0;
        for(int j=0;j<samplesToAverage;j++) {
          sleep_ms(1);
          zeroVals[i] = zeroVals[i] + FP2FLOAT(hw.analogIn[i])/((double)samplesToAverage);
        }
        // record value of -3.3v signal
        hw.voctOut[i]->Set(1.0);
        hw.cvOut[i]->SetOffset(0.0);
        neg3Vals[i] = 0.0;
        for(int j=0;j<samplesToAverage;j++) {
          sleep_ms(1);
          neg3Vals[i] = neg3Vals[i] + FP2FLOAT(hw.analogIn[i])/((double)samplesToAverage);
        }
      }
      initialized = true;
    }

    for(int i=0;i<NUM_WORDS;i++) {
      sprintf(buffer, "%1.4f", FP2FLOAT(hw.analogIn[i]));
      hw.display->drawStr((128*i)/3, 0, buffer);
      sprintf(buffer, "%1.4f", neg3Vals[i]);
      hw.display->drawStr((128*i)/3, 16, buffer);
      sprintf(buffer, "%1.4f", zeroVals[i]);
      hw.display->drawStr((128*i)/3, 32, buffer);
      double avgVal = 0.0;
      for(int j=0;j<samplesToAverage;j++) {
        sleep_ms(1);
        avgVal = (FP2FLOAT(hw.analogIn[i])-zeroVals[i])*(VIN_3V3/abs(zeroVals[i]-neg3Vals[i]));
      }
      sprintf(buffer, "%1.4f", avgVal);
      hw.display->drawStr((128*i)/3, 48, buffer);
    }
  }
};

class MathTest : public App {
  void UpdateDisplay() {
    char buffer[64];
    hw.display->setFont(u8g2_font_missingplanet_tf);
    fp_signed a, b;

    a = FLOAT2FP(1.35);
    b = FLOAT2FP(8.65);
    sprintf(
      buffer,
      "%2.2f + %2.2f = %2.2f",
      FP2FLOAT(a),
      FP2FLOAT(b),
      FP2FLOAT(a+b)
    );
    hw.display->drawStr(0, 0, buffer);

    a = FLOAT2FP(3.333333);
    b = FLOAT2FP(3);
    sprintf(
      buffer,
      "%2.2f * %2.2f = %2.2f",
      FP2FLOAT(a),
      FP2FLOAT(b),
      FP2FLOAT(FP_MUL(a,b))
    );
    hw.display->drawStr(0, 16, buffer);
  }
};

class OutputCalibrator : public App {
public:
  int voctNegVoltage = 0;
  int voctPosVoltage = 1;
  int cvNegVoltage = 0;
  int cvPosVoltage = 1;

  fp_t<int,14> voctOffset = 0.0;
  fp_t<int,14> voctNegCoef = 1.0/VOCT_NOUT_MAX;
  fp_t<int,14> voctPosCoef = 1.0/VOCT_POUT_MAX;
  fp_t<int,14> cvOffset = 0.0;
  fp_t<int,14> cvNegCoef = 1.0/CV_NOUT_MAX;
  fp_t<int,14> cvPosCoef = 1.0/CV_POUT_MAX;
  
  OutputCalibrator() {
    AddParam("octNV", &voctNegVoltage, 0, 10);
    AddParam("octPV", &voctPosVoltage, 0, 10);
    AddParam("cvNV", &cvNegVoltage, 0, 10);
    AddParam("cvPV", &cvPosVoltage, 0, 10);

    AddParam("vo", (int*)&voctOffset, 0, 1<<14, (1<<14)/100);
    AddParam("vNc", (int*)&voctNegCoef, 0, 1<<14, (1<<14)/100);
    AddParam("vPc", (int*)&voctPosCoef, 0, 1<<14, (1<<14)/100);

    AddParam("co", (int*)&cvOffset, 0, 1<<14, (1<<14)/100);
    AddParam("cNc", (int*)&cvNegCoef, 0, 1<<14, (1<<14)/100);
    AddParam("cPc", (int*)&cvPosCoef, 0, 1<<14, (1<<14)/100);
    
  }
  void UpdateDisplay() {
    char buffer[64];
    hw.display->setFont(u8g2_font_threepix_tr);
    for(int i=0;i<min((64/6),params.size());i++) {
      sprintf(buffer, "%s %d", params[i].GetName(), params[i].Get());
      hw.display->drawStr(0, i*6, buffer);
    }
    //sprintf(buffer, "%s %d", "voctOutCycles", int(fp_t<int,0>(voctNegVoltage*hw.voctOut[0]->res)*voctNegCoef));
    //hw.display->drawStr(0,48, buffer);
  }
  void Process() {
    for(int i=0;i<NUM_WORDS;i++) {
      hw.voctOut[i]->SetCycles(int(fp_t<int,0>(voctNegVoltage*hw.voctOut[i]->res)*voctNegCoef));

      // - this works!! - //
      fp_t<int64_t,14> temp = (fp_t<int64_t,14>(voctPosVoltage)+voctOffset)*voctPosCoef;
      temp = temp*fp_t<int64_t,14>(hw.voctOut[i]->res);
      hw.voctOut[i]->SetCyclesOffset(int(temp));
      // ---- //

      hw.cvOut[i]->SetCycles(int(fp_t<int,0>(cvNegVoltage*hw.cvOut[i]->res)*cvNegCoef));
      hw.cvOut[i]->SetCyclesOffset(int(fp_t<int,0>(cvPosVoltage*hw.cvOut[i]->res)*cvPosCoef));
    }

  }
};

class Drums : public App {
public:
  Kick kick;
  Snare snare;
  HighHat hat;
  bool lastVals[NUM_WORDS];
  Drums() {
    for(int i=0;i<NUM_WORDS;i++) lastVals[i] = false;
  }
  void UpdateDisplay() {
    char buffer[64];
    hw.display->setFont(u8g2_font_missingplanet_tf);
    sprintf(buffer, "%d  %d  %d", lastVals[0], lastVals[1], lastVals[2]);
    hw.display->drawStr(0, 0, buffer);
  }
  void Process() {
    for(int i=0;i<NUM_WORDS;i++) {
      bool curVal = hw.analogIn[i]>(FP_UNITY>>1)+(FP_UNITY/5);
      hw.trigIn[i]->Update();
      if(hw.trigIn[i]->RisingEdge()) {
        switch(i) {
          case 0:
            kick.env->Reset();
            break;
          case 1:
            snare.env->Reset();
            break;
          case 2:
            hat.env->Reset();
            break;
        }
      }
      lastVals[i] = curVal;
    }
    hw.cvOut[0]->SetAudioFP(kick.Process());
    hw.cvOut[1]->SetAudioFP(snare.Process());
    hw.cvOut[2]->SetAudioFP(hat.Process());
  }
};

class MiniMaths : public App {
public:
  typedef fp_t<int64_t,22> hpreal_t;
  typedef fp_t<int32_t,22> real_t;
  typedef fp_t<int32_t,FP_BITS> out_t;
  class ADEnv {
    public:
      real_t attackDelta;
      real_t decayDelta;
      real_t attackPhase;
      real_t decayPhase;
      real_t deltaScale;
      real_t CalculateDelta(double seconds) {
        return real_t(1.0/(SAMPLE_RATE*seconds));
      }
      ADEnv(double attack, double decay) {
        attackDelta = CalculateDelta(attack);
        decayDelta = CalculateDelta(decay);
        attackPhase = real_t(1);
        decayPhase = real_t(1);
        deltaScale = real_t(1);
      }
      void Reset() {
        decayPhase = real_t(0);
        attackPhase = real_t(0);
      }
      bool IsComplete() {
        return (decayPhase > real_t(1)) && (attackPhase > real_t(1));
      }
      out_t Process() {
        out_t out = out_t(0);
        if(attackPhase <= real_t(1)) {
          out = out_t(attackPhase);
          auto scaledDelta = real_t(attackDelta) * fp_t<int32_t,8>(deltaScale);
          attackPhase += real_t(scaledDelta);
        } else if (decayPhase <= real_t(1)) {
          out = out_t(1) - out_t(decayPhase);
          auto scaledDelta = real_t(decayDelta) * fp_t<int32_t,8>(deltaScale);
          decayPhase += real_t(scaledDelta);
        }
        return out;
      }
  };
  ADEnv* adEnvs[NUM_WORDS];
  ADEnv* clockEnvs[NUM_WORDS];
  bool clockTriggered[3];
  MiniMaths() {
    for(int i=0;i<NUM_WORDS;i++) {
      adEnvs[i] = new ADEnv(0.01, 0.01);
      clockEnvs[i] = new ADEnv(0.001, 0.001);
      clockTriggered[i] = true;
    }
    int oneHz = (1<<22)/SAMPLERATE;
    AddParam("ATK 1", (int*)&adEnvs[0]->attackDelta,  oneHz, oneHz*2000, oneHz);
    AddParam("ATK 2", (int*)&adEnvs[1]->attackDelta,  oneHz, oneHz*2000, oneHz);
    AddParam("ATK 3", (int*)&adEnvs[2]->attackDelta,  oneHz, oneHz*2000, oneHz);
    AddParam("DCY 1", (int*)&adEnvs[0]->decayDelta,   oneHz, oneHz*2000, oneHz);
    AddParam("DCY 2", (int*)&adEnvs[1]->decayDelta,   oneHz, oneHz*2000, oneHz);
    AddParam("DCY 3", (int*)&adEnvs[2]->decayDelta,   oneHz, oneHz*2000, oneHz);
    paramIndices[0] = 3;
    paramIndices[1] = 4;
    paramIndices[2] = 5;
  }
  ~MiniMaths() {
    for(int i=0;i<NUM_WORDS;i++) {
      delete adEnvs[i];
      delete clockEnvs[i];
    }
  }
  void UpdateDisplay() {
    char buffer[64];
    hw.display->setFont(u8g2_font_missingplanet_tf);
    for(int i=0;i<3;i++) {
      sprintf(buffer, "%1.2f %1.5f %1.5f",
        float(adEnvs[i]->deltaScale),
        float(adEnvs[i]->attackDelta),
        float(adEnvs[i]->decayDelta));
      hw.display->drawStr(0, i*16, buffer);
    }
  }
  void Process() {
    for(int i=0;i<NUM_WORDS;i++) {
      hw.trigIn[i]->Update();
      auto ain = fp_t<int32_t,14>(hw.analogIn[i])>>13;
      ain = ain * ain;
      adEnvs[i]->deltaScale = real_t(ain);
      if(hw.trigIn[i]->RisingEdge() || hw.control[i]->encButtonHeld) {
        adEnvs[i]->Reset();
        clockTriggered[i] = false;
      }
      if(adEnvs[i]->IsComplete() && (clockTriggered[i] == false)) {
        clockEnvs[i]->Reset();
        clockTriggered[i] = true;
      }
      hw.cvOut[i]->SetAudioFP((fp_signed)(adEnvs[i]->Process()*fp_t<int,0>(FP_UNITY)));
      hw.voctOut[i]->SetAudioFP(clockEnvs[i]->Process() > real_t(0) ? FP_UNITY : 0);
     }
  }
};

class ADEnv {
public:
  typedef fp_t<int32_t, 24> phase_t;
  typedef fp_t<int32_t, 0> param_t;
  typedef fp_t<int32_t, 14> audio_t;
  typedef enum { RISING, FALLING, WAITING } state_t;
  phase_t phase;
  phase_t deltaConst;
  param_t attackSpeed;
  param_t decaySpeed;
  state_t state;
  bool hold;
  ADEnv() {
    phase = phase_t(0);
    deltaConst = phase_t(0.2/SAMPLE_RATE);
    attackSpeed = param_t(10);
    decaySpeed = param_t(6);
    state = WAITING;
    hold = false;
  }

  void Start() {
    state = RISING;
  }

  void Stop() {
    if(state == RISING && hold) state = FALLING;
  }

  void SetAttackSpeed(int speed) { attackSpeed = param_t(speed); }
  void SetDecaySpeed(int speed) { decaySpeed = param_t(speed); }
  audio_t Process() {
    audio_t out = audio_t(0);
    switch(state) {
      case RISING:
        out = audio_t(phase);
        phase += deltaConst * attackSpeed;
        if(phase > phase_t(1)) {
          phase = phase_t(1);
          if(!hold) {
            state = FALLING;
          }
        }
        break;
      case FALLING:
        out = audio_t(phase);
        phase -= deltaConst * decaySpeed;
        if(phase <= phase_t(0)) {
          phase = 0;
          state = WAITING;
        }
        break;
      case WAITING:
        out = audio_t(phase);
        break;
    }
    return out;
  }
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
  typedef fp_t<int32_t, 14> audio_t;
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
    int speedScaler = hw.analogIn[wordIndex]>>(FP_BITS-8);
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

class LittleSeq : public LittleApp {  
public:
  typedef enum { PARAM_WRITEINDEX, PARAM_VALUE, PARAM_GATE, PARAM_LENGTH, PARAM_LAST } SelectedParam;
  SelectedParam selectedParam;
  typedef fp_t<int32_t, 20> phase_t;
  typedef fp_t<int32_t, 0> param_t;
  typedef fp_t<int32_t, 14> audio_t;
  audio_t voctCoef;
  audio_t cvCoef;
  audio_t steps[32];
  bool gates[32];
  int len;
  int readIndex;
  int writeIndex;
  int clockPulseLength;
  int blinkTime;
  char editMode;
  LittleSeq(int wordIndex) : LittleApp(wordIndex) {
    len = 8;
    readIndex = 0;
    writeIndex = 0;
    clockPulseLength = 0;
    blinkTime = 0;
    selectedParam = PARAM_LENGTH;
    editMode = 'L';
    this->voctCoef = audio_t(5.0/6.49);
    this->cvCoef = audio_t(5.0/8.72);
    for(int i=0;i<32;i++) {
      steps[i] = audio_t(rand()%11 + 1) * audio_t(1.0/12.0);
      gates[i] = i == 0 ? true : false;
    }
  }
  void UpdateDisplay() {
    // handle controls
    bool encPressed = hw.control[wordIndex]->encButtonPressed();
    if(encPressed) selectedParam = (SelectedParam)(((int)selectedParam + 1) % PARAM_LAST);
    int encDelta = hw.control[wordIndex]->GetDelta();
    if(encDelta != 0 || encPressed) {
      switch(selectedParam) {
        case PARAM_WRITEINDEX:
          editMode = 'I';
          writeIndex += encDelta;
          if(writeIndex >= len) writeIndex -= len;
          if(writeIndex < 0) writeIndex += len;
          break;
        case PARAM_VALUE:
          editMode = 'V';
          steps[writeIndex] += audio_t(1.0/(20.0))*fp_t<int32_t, 0>(encDelta);
          if(steps[writeIndex] > audio_t(1.0)) steps[writeIndex] = audio_t(1.0);
          if(steps[writeIndex] < audio_t(0.0)) steps[writeIndex] = audio_t(0.0);
          break;
        case PARAM_GATE:
          editMode = 'G';
          if(encDelta != 0) gates[writeIndex] = gates[writeIndex] ? 0 : 1;
          break;
        case PARAM_LENGTH:
          editMode = 'L';
          len += encDelta;
          len = max(1, min(32, len));
          break;
      }
    }

    // draw ui
    char buffer[64];
    int appOffset = (wordIndex*hw.display->getDisplayWidth())/NUM_WORDS;
    int appWidth = hw.display->getDisplayWidth()/3 - 1;
    int barHeight = hw.display->getDisplayHeight()/len - 1;
    blinkTime = (blinkTime+1) & 0xf;
    for(int i=0;i<len;i++) {
      if(i==readIndex || (i==writeIndex && blinkTime < 8)) {
        hw.display->drawVLine(
          appOffset,
          (i*hw.display->getDisplayHeight())/len,
          barHeight
        );
      }
      if(gates[i]) {
        hw.display->drawBox(
          appOffset + 2,
          (i*hw.display->getDisplayHeight())/len,
          min(max(0, int(fp_t<int32_t, 0>(appWidth-3) * steps[i])), appWidth),
          barHeight
        );
      } else {
        hw.display->drawFrame(
          appOffset + 2,
          (i*hw.display->getDisplayHeight())/len,
          min(max(0, int(fp_t<int32_t, 0>(appWidth-3) * steps[i])), appWidth),
          barHeight
        );
      }
    }
    sprintf(buffer, "%c", editMode);
    hw.display->setDrawColor(2);
    hw.display->setFontMode(true);
    hw.display->drawStr(appOffset + appWidth - 10, hw.display->getDisplayHeight() - 15, buffer);
    hw.display->setDrawColor(1);
  }
  void Process() {
    hw.trigIn[wordIndex]->Update();
    if(hw.trigIn[wordIndex]->RisingEdge()) {
      readIndex = readIndex + 1;
      if(readIndex >= len) {
        readIndex = 0;
      }
      if(gates[readIndex]) clockPulseLength = SAMPLERATE>>9;
    }
    if(hw.analogIn[wordIndex] > ((1<<FP_BITS)*3)/4) {
      readIndex = 0;
    }
    hw.voctOut[wordIndex]->SetCycles(0);
    hw.voctOut[wordIndex]->SetCyclesOffset(clockPulseLength-- > 0 && clockPulseLength < SAMPLERATE>>10 ? hw.voctOut[wordIndex]->res : 0);
    audio_t out = audio_t(steps[readIndex] * cvCoef);
    hw.cvOut[wordIndex]->SetCycles(0);
    hw.cvOut[wordIndex]->SetCyclesOffset((int)(fp_t<int,0>(hw.cvOut[wordIndex]->res) * out));
  }
};

class LittleCount : public LittleApp {
public:
  typedef enum { PARAM_DIVS, PARAM_STEP, PARAM_MAX, PARAM_MIN, PARAM_LAST } SelectedParam;
  SelectedParam selectedParam;
  int divs;
  int step;
  int stepSize;
  fp_t<int32_t, 10> maxVal;
  fp_t<int32_t, 10> minVal;
  LittleCount(int wordIndex) : LittleApp(wordIndex) {
    divs = 5;
    step = 0;
    stepSize = 1;
    maxVal = fp_t<int32_t, 10>(5);
    minVal = fp_t<int32_t, 10>(-5);
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
          maxVal += fp_t<int32_t, 10>(0.25) * fp_t<int32_t, 0>(encDelta);
          if(maxVal > fp_t<int32_t, 10>(5.0)) maxVal = fp_t<int32_t, 10>(5.0);
          if(maxVal < fp_t<int32_t, 10>(-5.0)) maxVal = fp_t<int32_t, 10>(-5.0);
          break;
        case PARAM_MIN:
          minVal += fp_t<int32_t, 10>(0.25) * fp_t<int32_t, 0>(encDelta);
          if(minVal > fp_t<int32_t, 10>(5.0)) minVal = fp_t<int32_t, 10>(5.0);
          if(minVal < fp_t<int32_t, 10>(-5.0)) minVal = fp_t<int32_t, 10>(-5.0);
          break;
      }
    }

    // draw UI
    char buffer[64];
    int appOffset = (wordIndex*hw.display->getDisplayWidth())/NUM_WORDS;
    int appWidth = hw.display->getDisplayWidth()/3 - 1;
    hw.display->setFont(u8g2_font_missingplanet_tf);
    sprintf(buffer, "% 1.2f", float(maxVal));
    hw.display->drawStr(appOffset+16, 0, buffer);
    sprintf(buffer, "%2dU", stepSize);
    hw.display->drawStr(appOffset+17, 27, buffer);
    sprintf(buffer, "% 1.2f", float(minVal));
    hw.display->drawStr(appOffset+15, 54, buffer);
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
      fp_t<int32_t, 10> val = fp_t<int32_t, 10>( ((maxVal - minVal) * fp_t<int32_t, 0>(step)) / fp_t<int32_t, 0>(divs-1) );
      val = fp_t<int32_t, 10>((val + minVal) * fp_t<int32_t, 10>(0.2));
      hw.cvOut[wordIndex]->SetAudioFP((fp_signed)(val*fp_t<int,0>(FP_UNITY)));
    }
    if(hw.analogIn[wordIndex] > ((1<<FP_BITS)*3)/4) {
      step = 0;
      fp_t<int32_t, 10> val = fp_t<int32_t, 10>( ((maxVal - minVal) * fp_t<int32_t, 0>(step)) / fp_t<int32_t, 0>(divs-1) );
      val = fp_t<int32_t, 10>((val + minVal) * fp_t<int32_t, 10>(0.2));
      hw.cvOut[wordIndex]->SetAudioFP((fp_signed)(val*fp_t<int,0>(FP_UNITY)));
    }
  }
};

class LittleQuant : public LittleApp {
public:
  Saw* saw;
  typedef fp_t<int32_t, 20> phase_t;
  typedef fp_t<int32_t, 14> audio_t;
  typedef fp_t<int32_t, 8> voct_t;
  std::vector<int> scale;
  std::vector<int> scaleFreqs;
  int degree;
  int octave;
  int divs;
  int lastDegree;
  int lastOctave;
  int hitCount;
  voct_t dist;
  LittleQuant(int wordIndex) : LittleApp(wordIndex) {
    saw = new Saw(220);
    scale.push_back(0);
    scale.push_back(2);
    scale.push_back(3);
    scale.push_back(5);
    scale.push_back(7);
    scale.push_back(8);
    scale.push_back(10);
    for(int i=0;i<scale.size();i++) scaleFreqs[i] = (int)(pow(2.0, (float)scale[i])*440.0);
    degree = 0;
    octave = 4;
    divs = 14;
    lastDegree = degree;
    lastOctave = octave;
    hitCount = 0;
  }
  ~LittleQuant() {
    delete saw;
  }
  void UpdateDisplay() {
    char buffer[64];

    int encDelta = hw.control[wordIndex]->GetDelta();
    if(encDelta != 0) {
      divs += encDelta;
      divs = max(1, min(100, divs));
    }

    int appOffset = (wordIndex*hw.display->getDisplayWidth())/NUM_WORDS;
    int appWidth = hw.display->getDisplayWidth()/3 - 1;
    hw.display->setFont(u8g2_font_missingplanet_tf);
    sprintf(buffer, "D: %d", degree);
    hw.display->drawStr(appOffset+2, 0, buffer);
    sprintf(buffer, "O: %d", octave);
    hw.display->drawStr(appOffset+2, 16, buffer);
    sprintf(buffer, "/: %d", divs);
    hw.display->drawStr(appOffset+2, 32, buffer);
  }
  void Process() {
    hw.trigIn[wordIndex]->Update();

    voct_t frac = voct_t(hw.analogIn[wordIndex]*divs)>>14;
    int rounded = int(frac + voct_t(0.5));
    dist = frac - voct_t(rounded);
    if(dist < voct_t(0)) dist = -dist;

    if(hw.trigIn[wordIndex]->RisingEdge()) {
      int oct = 0;
      int deg = rounded;
      while(deg >= 7) {
        deg -= 7;
        oct++;
      }

      octave = oct;
      degree = deg;

      saw->SetFreq(scaleFreqs[degree]<<(oct-2));

      lastOctave = oct;
      lastDegree = deg;
    }

    audio_t out = audio_t(audio_t(scale[degree]) * audio_t(1.0/12.0));
    out = out + audio_t(max(octave-1, 0));

    hw.voctOut[wordIndex]->SetCVFP((fp_signed)(out*fp_t<int,0>(FP_UNITY)));
    hw.cvOut[wordIndex]->SetAudioFP(saw->Process());
  }
};

class LittleKick : public LittleApp {
public:
  Kick kick;
  ADEnv env;
  typedef enum { PARAM_NULL, PARAM_LAST } SelectedParam;
  SelectedParam selectedParam;

  LittleKick(int wordIndex) : LittleApp(wordIndex) {
    this->selectedParam = PARAM_NULL;
    env.attackSpeed = 1000;
    env.decaySpeed = 40;
  }
  void UpdateDisplay() {
    // handle controls
    if(hw.control[wordIndex]->encButtonPressed()) selectedParam = (SelectedParam)(((int)selectedParam + 1) % PARAM_LAST);
    int encDelta = hw.control[wordIndex]->GetDelta();
    if(encDelta != 0) {
      switch(selectedParam) {
        case PARAM_NULL:
          break;
      }
    }

    // draw UI
    char buffer[64];
    int appOffset = (wordIndex*hw.display->getDisplayWidth())/NUM_WORDS;
    int appWidth = hw.display->getDisplayWidth()/3 - 1;
    hw.display->setFont(u8g2_font_missingplanet_tf);
    sprintf(buffer, "KICK");
    hw.display->drawStr(appOffset+2, 0, buffer);
    sprintf(buffer, "L:%d", 40 + (hw.analogIn[wordIndex] >> (FP_BITS - 6)));
    hw.display->drawStr(appOffset+2, 15, buffer);
    sprintf(buffer, "H:%d", 150 + (hw.analogIn[wordIndex] >> (FP_BITS - 9)));
    hw.display->drawStr(appOffset+2, 30, buffer);
  }
  void Process() {
    hw.trigIn[wordIndex]->Update();
    if(hw.trigIn[wordIndex]->RisingEdge()) {
      kick.lowerFreq = 30 + (hw.analogIn[wordIndex] >> (FP_BITS - 7));
      kick.upperFreq = 150 + (hw.analogIn[wordIndex] >> (FP_BITS - 9));
      kick.Reset();
      env.Start();
    }
    fp_signed out = kick.Process();
    hw.voctOut[wordIndex]->SetCycles(0);
    hw.voctOut[wordIndex]->SetCyclesOffset(hw.voctOut[wordIndex]->res - ((int)(fp_t<int,0>(hw.voctOut[wordIndex]->res) * env.Process())));
    hw.cvOut[wordIndex]->SetAudioFP(out);
  }
};

class LittleFollower : public LittleApp {
  public:
  typedef fp_t<int32_t, 14> audio_t;
  audio_t lastVal;
  audio_t gain;
  audio_t voctCoef;
  audio_t cvCoef;
  LittleFollower(int wordIndex) : LittleApp(wordIndex) {
    lastVal = 0;
    gain = audio_t(1);
    this->voctCoef = audio_t(5.0/6.49);
    this->cvCoef = audio_t(5.0/8.72);
  }
  void UpdateDisplay() {
    char buffer[64];

    int encDelta = hw.control[wordIndex]->GetDelta();
    if(encDelta != 0) {
      gain = gain + (audio_t(encDelta)>>4);
      gain = max(audio_t(0), min(audio_t(20), gain));
    }

    int appOffset = (wordIndex*hw.display->getDisplayWidth())/NUM_WORDS;
    int appWidth = hw.display->getDisplayWidth()/3 - 1;
    hw.display->setFont(u8g2_font_missingplanet_tf);
    sprintf(buffer, " FLW");
    hw.display->drawStr(appOffset+2, 0, buffer);
    sprintf(buffer, " %1.2fv", float(lastVal>>13)*5.0);
    hw.display->drawStr(appOffset+2, 15, buffer);
    sprintf(buffer, " %1.2fx", float(gain));
    hw.display->drawStr(appOffset+2, 30, buffer);
  }
  void Process() {
    audio_t curVal = audio_t(abs(hw.analogIn[wordIndex]-(1<<(FP_BITS-1))));
    audio_t diff = curVal - lastVal;
    lastVal += diff>>10;
    audio_t clippedVal = max(audio_t(0), min(audio_t(1), audio_t((lastVal>>13)*gain)));

    audio_t out = audio_t(clippedVal * voctCoef);
    hw.voctOut[wordIndex]->SetCycles(0);
    hw.voctOut[wordIndex]->SetCyclesOffset(((int)(fp_t<int,0>(hw.voctOut[wordIndex]->res)*voctCoef)) - ((int)(fp_t<int,0>(hw.voctOut[wordIndex]->res) * out)));
    out = audio_t(clippedVal * cvCoef);
    hw.cvOut[wordIndex]->SetCycles(0);
    hw.cvOut[wordIndex]->SetCyclesOffset((int)(fp_t<int,0>(hw.cvOut[wordIndex]->res) * out));
  }
};

class LFSR {
public:
  int bits;
  unsigned int mask;
  unsigned int val;
  unsigned int GetBit(int i) { return (this->val&(1<<i)) > 0 ? 1 : 0; }
  void SetBit(int i, unsigned int v) { val = val | (v<<i); }
  LFSR(int bits=16, unsigned int mask=1) {
    this->bits = bits;
    this->val = 0;
    this->mask = mask;
  }
  int Process() {
    unsigned int retVal = 0;
    for(int i=0;i<bits;i++) {
      if(mask&(1<<i)) retVal = retVal^GetBit(i);
    }
    retVal = !retVal;
    this->val = this->val>>1;
    SetBit(bits-1, retVal);
    return retVal;
  }
};

class LittleShift : public LittleApp {
public:
  typedef enum { PARAM_BITS, /*PARAM_MASK, PARAM_RATE,*/ PARAM_LAST } SelectedParam;
  SelectedParam selectedParam;
  typedef fp_t<int32_t, 14> audio_t;
  audio_t voctCoef;
  audio_t cvCoef;
  LFSR shift;
  LittleShift(int wordIndex) : LittleApp(wordIndex) {

  }
  void UpdateDisplay() {
    // handle controls
    if(hw.control[wordIndex]->encButtonPressed()) selectedParam = (SelectedParam)(((int)selectedParam + 1) % PARAM_LAST);
    int encDelta = hw.control[wordIndex]->GetDelta();
    if(encDelta != 0) {
      switch(selectedParam) {
        case PARAM_BITS:
          shift.bits += encDelta;
          break;
        /*
        case PARAM_MASK:
          shift.mask += encDelta;
          break;
        case PARAM_RATE:
          break;
        */
      }
    }

    // draw UI
    char buffer[64];
    int appOffset = (wordIndex*hw.display->getDisplayWidth())/NUM_WORDS;
    int appWidth = hw.display->getDisplayWidth()/12 - 1;
    int barHeight = hw.display->getDisplayHeight()/shift.bits - 1;
    for(int i=0;i<shift.bits;i++) {
      if(shift.GetBit((shift.bits-1)-i)) {
        hw.display->drawBox(
          appOffset + 2,
          (i*hw.display->getDisplayHeight())/shift.bits,
          appWidth-3,
          barHeight
        );
      } else {
        hw.display->drawFrame(
          appOffset + 2,
          (i*hw.display->getDisplayHeight())/shift.bits,
          appWidth-3,
          barHeight
        );
      }
    }

    //hw.display->setFont(u8g2_font_missingplanet_tf);
    //sprintf(buffer, "A: %d %s", this->attackSpeed, selectedParam == PARAM_ATTACK ? "*" : "");
  }
  void Process() {
    uint32_t mask = abs((hw.analogIn[wordIndex]>>(FP_BITS-9))-(1<<8));
    mask = mask | (mask<<8) | (mask<<16) | (mask<<24);
    shift.mask = mask;
    hw.trigIn[wordIndex]->Update();
    if(hw.trigIn[wordIndex]->RisingEdge()) {
      shift.Process();
      hw.voctOut[wordIndex]->SetCycles(0);
      hw.voctOut[wordIndex]->SetCyclesOffset((int)(fp_t<int,0>(hw.voctOut[wordIndex]->res) * fp_t<int,0>(shift.GetBit(0))));
      hw.cvOut[wordIndex]->SetCycles(0);
      hw.cvOut[wordIndex]->SetCyclesOffset((int)(fp_t<int,0>(hw.cvOut[wordIndex]->res) * fp_t<int,0>(shift.GetBit(0))));
    }
    else
    {
      if(hw.trigIn[wordIndex]->FallingEdge()) {
      hw.cvOut[wordIndex]->SetCyclesOffset(0);
      }
    }
    /*
    hw.trigIn[wordIndex]->Update();
    if(hw.trigIn[wordIndex]->RisingEdge()) env.Start();
    if(hw.trigIn[wordIndex]->FallingEdge()) env.Stop();
    hw.voctOut[wordIndex]->SetCycles(0);
    hw.voctOut[wordIndex]->SetCyclesOffset(((int)(fp_t<int,0>(hw.voctOut[wordIndex]->res)*voctCoef)) - ((int)(fp_t<int,0>(hw.voctOut[wordIndex]->res) * out)));
    hw.cvOut[wordIndex]->SetCycles(0);
    hw.cvOut[wordIndex]->SetCyclesOffset((int)(fp_t<int,0>(hw.cvOut[wordIndex]->res) * out));
    */
  }
};

class ThreeLittleWords : public App {
public:
  typedef enum { SEQ, ENV, QUANT, COUNT, DRUM, FOLLOWER, SHIFT, NUM_WORDTYPES } WordType;
  App* words[NUM_WORDS] = {NULL, NULL, NULL};
  WordType littleWords[NUM_WORDS] = {SEQ, ENV, QUANT};
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
      case SEQ: newWord = new LittleSeq(word); break;
      case ENV: newWord = new LittleEnv(word); break;
      case QUANT: newWord = new LittleQuant(word); break;
      case COUNT: newWord = new LittleCount(word); break;
      case DRUM: newWord = new LittleKick(word); break;
      case FOLLOWER: newWord = new LittleFollower(word); break;
      case SHIFT: newWord = new LittleShift(word); break;
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