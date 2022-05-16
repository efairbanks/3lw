#ifndef FPMATH_H
#define FPMATH_H

typedef int16_t fp_signed;
#define FP_BITS 14
#define FP_UNITY (1 << FP_BITS)
#define FP_MUL(x, y) (((x) * (y)) >> FP_BITS)
#define FP_DIV(n, d) (((n)<<FP_BITS) / (d))
#define FP2FLOAT(x) ((x) / ((float)(FP_UNITY)))
#define FLOAT2FP(x) ((fp_signed)((x) * FP_UNITY))

#endif