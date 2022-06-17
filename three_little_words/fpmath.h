#ifndef FPMATH_H
#define FPMATH_H

typedef int32_t fp_signed;
typedef int64_t lfp_signed;
#define FP_BITS 14
#define FP_UNITY (1 << FP_BITS)
#define FP_MUL(x, y) (((x) * (y)) >> FP_BITS)
#define FP_DIV(n, d) (((n)<<FP_BITS) / (d))
#define FP2FLOAT(x) ((x) / ((double)(FP_UNITY)))
#define FLOAT2FP(x) ((fp_signed)((x) * FP_UNITY))
#define FLOAT2LFP(x) ((lfp_signed)((x) * FP_UNITY))

#define SIN_LEN 1024
fp_signed SIN_LUT[SIN_LEN];
void INIT_SIN_LUT() {
  for(int i=0;i<SIN_LEN;i++) {
    SIN_LUT[i] = (fp_signed)(((double)FP_UNITY) * sin((i*M_PI*2.0)/((float)SIN_LEN)));
  }
}

#define LUT_BITS 15
#define LUT_UNITY (1<<LUT_BITS)
#define TWOEXP_LEN 4096
uint32_t TWOEXP_LUT[TWOEXP_LEN];
void INIT_TWOEXP_LUT() {
  for(int i=0;i<TWOEXP_LEN;i++) {
    TWOEXP_LUT[i] = (uint32_t)(((double)LUT_UNITY) * (pow(2.0, ((float)i)/((float)TWOEXP_LEN)) - 1.0));
  }
}

lfp_signed twoexp(lfp_signed x) {
  if(x<0) {
    return 0;
  } else if(x==0) {
    return LUT_UNITY;
  } else {
    lfp_signed whole = x>>FP_BITS;
    lfp_signed frac = x - (whole<<FP_BITS);
    return (LUT_UNITY + TWOEXP_LUT[frac>>(FP_BITS-12)]) << whole;
  }
}

lfp_signed voct2freq(lfp_signed x) {
  return (33 * twoexp(x))>>LUT_BITS;
}

void INIT_FPMATH() {
  INIT_SIN_LUT();
  INIT_TWOEXP_LUT();
}



#endif