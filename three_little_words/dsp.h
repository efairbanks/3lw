#ifndef FPSYNTH_H
#define FPSYNTH_H

#include "constants.h"
#include "fp.hpp"

#define SAMPLERATE  ((int)SAMPLE_RATE)
#define SAMPLEDELTA (0xFFFFFFFF/SAMPLERATE)

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

class ClockRateDetector {
public:
  uint32_t samplesSinceLastClock;
  uint32_t lastIntervalInSamples;
  ClockRateDetector() {
    samplesSinceLastClock = 0;
    lastIntervalInSamples = SAMPLE_RATE;
  }
  void Process(bool triggered) {
    if(triggered) {
      lastIntervalInSamples = samplesSinceLastClock;
      samplesSinceLastClock = 0;
    } else {
      samplesSinceLastClock++;
    }
  }
};

class Phasor {
public:
  typedef fp_t<uint32_t, 30> phase_t;
  phase_t phase;
  phase_t delta;
  Phasor() {
    phase = 0;
    delta = 0;
  }
  void SetPeriodInSamples(int samples, int multiplier) {
    delta = (phase_t(1.0) / fp_t<uint32_t, 0>(samples)) * fp_t<uint32_t, 0>(multiplier);
  }
  void SetBPM(int bpm) {
    delta = (fp_t<uint32_t, 0>(bpm) * fp_t<uint32_t, 26>(1.0 / SAMPLE_RATE)) * fp_t<uint32_t, 14>(1.0 / 60.0);
  }
  void Reset() {
    phase = 0;
  }
  fp_t<int, 12> Process() {
    fp_t<int, 12> out = phase;
    phase += delta;
    while(phase > phase_t(1.0)) phase -= phase_t(1.0);
    return out;
  }
};

class ADEnv {
public:
  typedef fp_t<int, 20> phase_t;
  typedef fp_t<int, 10> param_t;
  typedef fp_t<int, 12> audio_t;
  typedef enum { RISING, FALLING, WAITING } state_t;
  phase_t phase;
  phase_t fastestDelta;
  phase_t oneSecondDelta;
  phase_t attackDelta;
  phase_t decayDelta;
  state_t state;
  bool hold;

  static phase_t mix(phase_t a, phase_t b, param_t x) {
    //if(x > param_t(1)) x = param_t(1);
    //if(x < param_t(0)) x = param_t(0);
    return phase_t(a * (param_t(1) - x)) + phase_t(b * x);
  }

  /*
  static param_t clip(param_t min, param_t max, param_t x) {
    if(x < min) x = min;
    if(x > max) x = max;
    return x;
  }
  */

  void SetAttackSpeed(param_t x) {
    x = param_t(1) - x;
    x = x * x * x;
    x = param_t(1) - x;
    attackDelta = fastestDelta * (param_t(1.0) - x) + oneSecondDelta * x;
  }

  void SetDecaySpeed(param_t x) {
    x = param_t(1) - x;
    x = x * x * x;
    x = param_t(1) - x;
    decayDelta = fastestDelta * (param_t(1.0) - x) + oneSecondDelta * x;
  }

  ADEnv() {
    phase = phase_t(0);
    fastestDelta = phase_t(200.0 / SAMPLE_RATE);
    oneSecondDelta = phase_t(1.0 / SAMPLE_RATE);
    SetAttackSpeed(0.25);
    SetDecaySpeed(0.25);
    state = WAITING;
    hold = false;
  }

  void Start() {
    state = RISING;
  }

  void Stop() {
    if(state == RISING && hold) state = FALLING;
  }

  audio_t Process() {
    audio_t out = audio_t(0);
    switch(state) {
      case RISING:
        out = audio_t(phase);
        phase += attackDelta;
        if(phase > phase_t(1)) {
          phase = phase_t(1);
          if(!hold) {
            state = FALLING;
          }
        }
        break;
      case FALLING:
        out = audio_t(phase);
        phase -= decayDelta;
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

class TrigGen {
public:
  int samplesSinceFired;
  int delay;
  int width;
  fp_t<int, 12> amp;
  TrigGen(int delayInSamples = 20, int widthInSamples = 100, fp_t<int, 12> amplitude = fp_t<int, 12>(1)) {
    samplesSinceFired = delay + width + 1;
    delay = delayInSamples;
    width = widthInSamples;
    amp = amplitude;
  }
  void Reset() { samplesSinceFired = -1; }
  fp_t<int, 12> Process() {
    samplesSinceFired++;    
    return (samplesSinceFired > delay && samplesSinceFired < (delay + width)) ? amp : fp_t<int, 12>(0);
  }
};

class Clock : public Phasor {
public:
  typedef fp_t<uint32_t, 30> phase_t;
  TrigGen trigGen;
  Clock() : trigGen(0), Phasor() {
  }
  fp_t<int, 12> Process() {
    phase += delta;
    while(phase > phase_t(1.0)) {
      phase -= phase_t(1.0);
      trigGen.Reset();
    }
    return trigGen.Process();
  }
};

class Schmidt {
public:
  fp_t<int, 12> lo;
  fp_t<int, 12> hi;
  fp_t<int, 12> last;
  fp_t<int, 12> state;
  Schmidt(fp_t<int, 12> lowThresh = fp_t<int, 12>(1.0), fp_t<int, 12> highThresh = fp_t<int, 12>(2.0)) {
    lo = lowThresh;
    hi = highThresh;
    last = fp_t<int, 12>(0);
    state = fp_t<int, 12>(0);
  }
  fp_t<int, 12> Process(fp_t<int, 12> in) {
    fp_t<int, 0> polarity = fp_t<int, 0>(in > fp_t<int, 12>(0) ? 1 : -1);
    fp_t<int, 0> magnitude = in * polarity;
    if(last < hi && in > hi) state = polarity;
    if(last > lo && in < lo) state = fp_t<int, 12>(0);
    last = in;
    return state;
  }
};

class TrigDetector {
public:
  Schmidt schmidt;
  fp_t<int, 12> lastVal;
  TrigDetector() {
    lastVal = fp_t<int, 12>(0);
  }
  bool Process(fp_t<int, 12> in) {
    bool out = false;
    fp_t<int, 12> curVal = schmidt.Process(in);
    if(curVal > lastVal) out = true;
    lastVal = curVal;
    return out;
  }
};

#endif