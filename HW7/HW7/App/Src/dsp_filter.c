#include "dsp_filter.h"
#include <string.h>
#include "arm_math.h"

/* CMSIS-DSP FIR instances, one per axis */
static arm_fir_instance_f32 fir[DSP_NUM_AXES];
static float fir_state[DSP_NUM_AXES][FIR_NUM_TAPS + DSP_BLOCK_SIZE - 1];

/* arm_fir_f32 expects the coefficients in time-reversed order.
 * Our filter is symmetric (linear phase) so it makes no difference,
 * but we reverse anyway so the code is correct for any coefficient set. */
static float coeffs_rev[FIR_NUM_TAPS];

/* Delay lines for the reference implementation */
static double ref_hist[DSP_NUM_AXES][FIR_NUM_TAPS];

void DSP_Filter_Reset(void)
{
  for (uint32_t i = 0; i < FIR_NUM_TAPS; i++)
  {
    coeffs_rev[i] = fir_coeffs[FIR_NUM_TAPS - 1U - i];
  }
  for (uint32_t a = 0; a < DSP_NUM_AXES; a++)
  {
    arm_fir_init_f32(&fir[a], FIR_NUM_TAPS, coeffs_rev, fir_state[a], DSP_BLOCK_SIZE);
  }
  memset(ref_hist, 0, sizeof(ref_hist));
}

void DSP_Filter_Process(uint32_t axis, const float *in, float *out)
{
  arm_fir_f32(&fir[axis], (float32_t *)in, out, DSP_BLOCK_SIZE);
}

void DSP_Reference_Process(uint32_t axis, const float *in, double *out)
{
  double *h = ref_hist[axis];   /* h[0] = newest sample */
  for (uint32_t n = 0; n < DSP_BLOCK_SIZE; n++)
  {
    memmove(&h[1], &h[0], (FIR_NUM_TAPS - 1U) * sizeof(double));
    h[0] = (double)in[n];
    double acc = 0.0;
    for (uint32_t k = 0; k < FIR_NUM_TAPS; k++)
    {
      acc += (double)fir_coeffs[k] * h[k];      /* y[n] = sum b[k] x[n-k] */
    }
    out[n] = acc;
  }
}
