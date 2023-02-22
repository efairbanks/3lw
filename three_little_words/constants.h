#ifndef CONSTANTS_H
#define CONSTANTS_H

#define CPU_SPEED 125000000.0
#define TIMER_INTERVAL ((int)(1000000.0/40000.0))
#define SAMPLE_RATE (1000000.0/TIMER_INTERVAL)
#define LFO_OUT_PIN 0
#define OFFSET_OUT_PIN 1

#define VIN_5V0 5.11
#define VIN_5V0_FP fp_t<int, 12>(VIN_5V0)
#define VIN_3V3 3.33
#define VIN_3V3_FP fp_t<int, 12>(VIN_3V3)
#define VOCT_R1 1000.0
#define VOCT_R2 1000.0
#define CV_R1   4700.0
#define CV_R2   7500.0

#define VOCT_NOUT_MAX   (VIN_3V3*(VOCT_R2/VOCT_R1))
#define VOCT_POUT_MAX   (VIN_3V3*((VOCT_R2/VOCT_R1)+1.0))
#define CV_NOUT_MAX     (VIN_3V3*(CV_R2/CV_R1))
#define CV_POUT_MAX     (VIN_3V3*((CV_R2/CV_R1)+1.0))

#define CV_IN_2_V(x) ((SIG*3)-VIN_5V0)

#endif