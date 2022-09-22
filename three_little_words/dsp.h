#ifndef FPSYNTH_H
#define FPSYNTH_H

#include "fpmath.h"
#include "constants.h"
#include "fp.hpp"

#define SAMPLERATE  ((int)SAMPLE_RATE)
#define SAMPLEDELTA (0xFFFFFFFF/SAMPLERATE)

using namespace fp;
using namespace fp::constants;

class Phasor {
public:
  uint32_t phase;
  uint32_t delta;
  Phasor() {
    this->phase = 0;
    this->delta = SAMPLEDELTA;
  }
  Phasor(fp_signed freq) {
    this->phase = 0;
    this->SetFreq(freq);
  }
  void SetFreq(fp_signed freq) {
    this->delta = SAMPLEDELTA*freq;
  }
  void SetFreqFractional(fp_signed fracFreq) {
    this->delta = FP_MUL(SAMPLEDELTA, fracFreq);
  }
  void SetDuration(uint32_t ms) {
    this->delta = (SAMPLEDELTA*1000)/ms;
  }
  uint32_t Process() {
    uint32_t out = phase;
    phase += delta;
    return out;
  }
};

class Metronome : public Phasor {
public:
  Metronome(uint32_t ms) : Phasor() {
    phase = 0xFFFFFFFF;
    SetDuration(ms);
  }
  Metronome() : Phasor() {
    phase = 0xFFFFFFFF;
    SetFreq(0);
  }
  uint32_t Process() {
    uint32_t lastPhase = phase;
    phase += delta;
    return phase < lastPhase ? FP_UNITY : 0;
  }
};

class Trigger {
public:
  fp_signed lastVal;
  fp_signed threshold;
  bool triggered;
  Trigger(fp_signed t) {
    threshold = t;
    lastVal = threshold-1;
    triggered = false;
  }
  bool Triggered() {
    bool out = triggered;
    triggered = false;
    return out;
  }
  fp_signed Process(fp_signed curVal) {
    fp_signed out = 0;
    if(curVal > threshold && lastVal < threshold) {
      out = FP_UNITY;
      triggered = true;
    }
    lastVal = curVal;
    return out;
  }
};

class Line {
public:
  uint32_t phase;
  uint32_t delta;

  Line(uint32_t ms) {
    delta = SAMPLEDELTA*1000/ms;
  }

  void Reset() { phase = 0; }

  fp_signed Process() {
    if(phase < (0xFFFFFFFF-delta)) {
      uint32_t out = phase >> (32-FP_BITS);
      phase += delta;
      return FP_UNITY-out;
    } else {
      return 0;
    }
  }
};

class Osc {
public:
  Phasor* phasor;
  Osc(fp_signed freq) {
    phasor = new Phasor(freq);
  }
  ~Osc() {
    delete phasor;
  }
  void SetFreq(fp_signed freq) {
    phasor->SetFreq(freq);
  }
  void SetDuration(uint32_t ms) {
    phasor->SetDuration(ms);
  }
  virtual fp_signed Process() = 0;
};

class Saw : public Osc {
public:
  Saw(fp_signed freq) : Osc(freq) {}
  fp_signed Process() {
    return (phasor->Process() >> (31-FP_BITS)) - FP_UNITY;
  }
};

class Pulse : public Osc {
public:
  Pulse(fp_signed freq) : Osc(freq) {}
  fp_signed Process() {
    return phasor->Process() < (0x7FFFFFFF) ? FP_UNITY : -FP_UNITY;
  }
};

class Tri : public Osc {
public:
  Tri(fp_signed freq) : Osc(freq) {}
  fp_signed Process() {
    return abs((fp_signed)((phasor->Process() >> (30-FP_BITS)) - (FP_UNITY<<1))) - FP_UNITY;
  }
};

class OnePoleLP {
public:
  fp_signed coef;
  fp_signed lastVal;
  OnePoleLP(fp_signed coef) {
    this->lastVal = 0;
    this->SetCoef(coef);
  }
  void SetCoef(fp_signed coef) {
    this->coef = coef;
  }
  fp_signed Process(fp_signed input) {
    input = input<<11;
    lastVal = ((input*1)>>11) + ((lastVal*((1<<11)-1))>>11);
    return lastVal>>11;
  }
};

class OnePoleHP : public OnePoleLP {
public:
  OnePoleHP(fp_signed coef) : OnePoleLP(coef) {}
  fp_signed Process(fp_signed input) {
    lastVal = FP_MUL(input,coef) + FP_MUL(lastVal, FP_UNITY-coef);
    return input-lastVal;
  }
};

class Delay {
public:
  uint32_t bufferLength;
  uint32_t delay;
  uint32_t writeHead;
  fp_signed* buffer;

  Delay(uint32_t maxDelay) {
    this->bufferLength = maxDelay;
    this->buffer = (fp_signed*)malloc(sizeof(fp_signed)*this->bufferLength);
    memset(this->buffer, 0, sizeof(fp_signed)*this->bufferLength);
    this->delay = bufferLength-1;
    this->writeHead = 0;
  }

  ~Delay() {
    free(this->buffer);
  }

  void SetDelay(uint32_t delaySamples) {
    this->delay = delaySamples;
  }

  fp_signed Process(fp_signed input) {
    writeHead++;
    if(writeHead >= bufferLength) writeHead -= bufferLength;
    int32_t readHead = writeHead - delay;
    if(readHead < 0) readHead += bufferLength;
    if(readHead >= bufferLength) readHead -= bufferLength;
    buffer[writeHead] = input;
    return buffer[readHead];
  }
};

class Comb {
public:
  Delay* delay;
  fp_signed lastVal;
  fp_signed feedback;
  Comb(uint32_t maxDelay, fp_signed feedback) {
    this->delay = new Delay(maxDelay);
    this->feedback = feedback;
  }
  ~Comb() {
    delete delay;
  }
  fp_signed Process(fp_signed input) {
    lastVal = delay->Process(input + FP_MUL(lastVal, feedback));
    return lastVal;
  }
};

class HighHat {
public:
  Pulse* osca;
  Pulse* oscb;
  Line* env;
  HighHat() {
    osca = new Pulse(4235);
    oscb = new Pulse(2655);
    env = new Line(1400);
  }
  ~HighHat() {
    delete osca;
    delete oscb;
  }
  fp_signed Process() {
    fp_signed out = env->Process();
    for(int i=0;i<3;i++) out = FP_MUL(out,out);
    return FP_MUL(out, FP_MUL(osca->Process(), oscb->Process()));
  }
};

class Kick {
public:
  Tri* osc;
  Line* env;
  int upperFreq;
  int lowerFreq;
  Kick() {
    osc = new Tri(400);
    env = new Line(250);
    upperFreq = 400;
    lowerFreq = 70;
  }
  ~Kick() {
    delete osc;
    delete env;
  }
  void Reset() {
    osc->phasor->phase = 0;
    env->Reset();
  }
  fp_signed Process() {
    fp_signed out = env->Process();
    fp_signed lenv = out;
    for(int i=0;i<4;i++) {
      out=FP_MUL(out,out);
    }
    osc->SetFreq(FP_MUL(upperFreq,out)+lowerFreq);
    return FP_MUL(osc->Process(), lenv);
  }
};

class Snare {
public:
  Tri* osc;
  Line* env;
  Snare() {
    osc = new Tri(400);
    env = new Line(200);
  }
  ~Snare() {
    delete osc;
    delete env;
  }
  fp_signed Process() {
    fp_signed out = env->Process();
    fp_signed lenv = out;
    for(int i=0;i<2;i++) {
      out=FP_MUL(out,out);
    }
    fp_signed noise = FP_MUL(rand(),FP_UNITY-1) + FP_MUL(rand(),FP_UNITY-1);
    fp_signed freq = FP_MUL(FP_MUL(650,noise),FP_UNITY-out);
    freq += FP_MUL(300,out);
    osc->SetFreq(freq+180);
    return FP_MUL(osc->Process(), lenv);
  }
};

#endif