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

class ADEnv {
public:
  typedef fp_t<int, 24> phase_t;
  typedef fp_t<int, 0> param_t;
  typedef fp_t<int, 14> audio_t;
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

#endif