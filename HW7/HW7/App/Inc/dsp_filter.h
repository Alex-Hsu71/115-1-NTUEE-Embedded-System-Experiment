#ifndef DSP_FILTER_H
#define DSP_FILTER_H

#include <stdint.h>
#include "fir_coeffs.h"

#define DSP_NUM_AXES    3     /* X, Y, Z */
#define DSP_BLOCK_SIZE  10    /* samples per block (100 ms at 100 Hz) */

/* Reset the CMSIS-DSP FIR state of every axis (and the reference filter). */
void DSP_Filter_Reset(void);

/* Filter one block of one axis with arm_fir_f32(). */
void DSP_Filter_Process(uint32_t axis, const float *in, float *out);

/* Plain-C double-precision direct-form FIR, used only to cross-check
 * the CMSIS-DSP result while the known test signal is running. */
void DSP_Reference_Process(uint32_t axis, const float *in, double *out);

#endif /* DSP_FILTER_H */
