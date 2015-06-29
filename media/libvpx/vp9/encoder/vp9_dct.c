/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <math.h>

#include "./vpx_config.h"
#include "./vp9_rtcd.h"

#include "vpx_ports/mem.h"
#include "vp9/common/vp9_blockd.h"
#include "vp9/common/vp9_idct.h"
#include "vp9/common/vp9_systemdependent.h"
#include "vp9/encoder/vp9_dct.h"

static INLINE tran_high_t fdct_round_shift(tran_high_t input) {
  tran_high_t rv = ROUND_POWER_OF_TWO(input, DCT_CONST_BITS);
  // TODO(debargha, peter.derivaz): Find new bounds for this assert
  // and make the bounds consts.
  // assert(INT16_MIN <= rv && rv <= INT16_MAX);
  return rv;
}

void vp9_fdct4(const tran_low_t *input, tran_low_t *output) {
  tran_high_t step[4];
  tran_high_t temp1, temp2;

  step[0] = input[0] + input[3];
  step[1] = input[1] + input[2];
  step[2] = input[1] - input[2];
  step[3] = input[0] - input[3];

  temp1 = (step[0] + step[1]) * cospi_16_64;
  temp2 = (step[0] - step[1]) * cospi_16_64;
  output[0] = (tran_low_t)fdct_round_shift(temp1);
  output[2] = (tran_low_t)fdct_round_shift(temp2);
  temp1 = step[2] * cospi_24_64 + step[3] * cospi_8_64;
  temp2 = -step[2] * cospi_8_64 + step[3] * cospi_24_64;
  output[1] = (tran_low_t)fdct_round_shift(temp1);
  output[3] = (tran_low_t)fdct_round_shift(temp2);
}

void vp9_fdct4x4_1_c(const int16_t *input, tran_low_t *output, int stride) {
  int r, c;
  tran_low_t sum = 0;
  for (r = 0; r < 4; ++r)
    for (c = 0; c < 4; ++c)
      sum += input[r * stride + c];

  output[0] = sum << 1;
  output[1] = 0;
}

void vp9_fdct4x4_c(const int16_t *input, tran_low_t *output, int stride) {
  // The 2D transform is done with two passes which are actually pretty
  // similar. In the first one, we transform the columns and transpose
  // the results. In the second one, we transform the rows. To achieve that,
  // as the first pass results are transposed, we transpose the columns (that
  // is the transposed rows) and transpose the results (so that it goes back
  // in normal/row positions).
  int pass;
  // We need an intermediate buffer between passes.
  tran_low_t intermediate[4 * 4];
  const int16_t *in_pass0 = input;
  const tran_low_t *in = NULL;
  tran_low_t *out = intermediate;
  // Do the two transform/transpose passes
  for (pass = 0; pass < 2; ++pass) {
    tran_high_t input[4];      // canbe16
    tran_high_t step[4];       // canbe16
    tran_high_t temp1, temp2;  // needs32
    int i;
    for (i = 0; i < 4; ++i) {
      // Load inputs.
      if (0 == pass) {
        input[0] = in_pass0[0 * stride] * 16;
        input[1] = in_pass0[1 * stride] * 16;
        input[2] = in_pass0[2 * stride] * 16;
        input[3] = in_pass0[3 * stride] * 16;
        if (i == 0 && input[0]) {
          input[0] += 1;
        }
      } else {
        input[0] = in[0 * 4];
        input[1] = in[1 * 4];
        input[2] = in[2 * 4];
        input[3] = in[3 * 4];
      }
      // Transform.
      step[0] = input[0] + input[3];
      step[1] = input[1] + input[2];
      step[2] = input[1] - input[2];
      step[3] = input[0] - input[3];
      temp1 = (step[0] + step[1]) * cospi_16_64;
      temp2 = (step[0] - step[1]) * cospi_16_64;
      out[0] = (tran_low_t)fdct_round_shift(temp1);
      out[2] = (tran_low_t)fdct_round_shift(temp2);
      temp1 = step[2] * cospi_24_64 + step[3] * cospi_8_64;
      temp2 = -step[2] * cospi_8_64 + step[3] * cospi_24_64;
      out[1] = (tran_low_t)fdct_round_shift(temp1);
      out[3] = (tran_low_t)fdct_round_shift(temp2);
      // Do next column (which is a transposed row in second/horizontal pass)
      in_pass0++;
      in++;
      out += 4;
    }
    // Setup in/out for next pass.
    in = intermediate;
    out = output;
  }

  {
    int i, j;
    for (i = 0; i < 4; ++i) {
      for (j = 0; j < 4; ++j)
        output[j + i * 4] = (output[j + i * 4] + 1) >> 2;
    }
  }
}

void vp9_fadst4(const tran_low_t *input, tran_low_t *output) {
  tran_high_t x0, x1, x2, x3;
  tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;

  x0 = input[0];
  x1 = input[1];
  x2 = input[2];
  x3 = input[3];

  if (!(x0 | x1 | x2 | x3)) {
    output[0] = output[1] = output[2] = output[3] = 0;
    return;
  }

  s0 = sinpi_1_9 * x0;
  s1 = sinpi_4_9 * x0;
  s2 = sinpi_2_9 * x1;
  s3 = sinpi_1_9 * x1;
  s4 = sinpi_3_9 * x2;
  s5 = sinpi_4_9 * x3;
  s6 = sinpi_2_9 * x3;
  s7 = x0 + x1 - x3;

  x0 = s0 + s2 + s5;
  x1 = sinpi_3_9 * s7;
  x2 = s1 - s3 + s6;
  x3 = s4;

  s0 = x0 + x3;
  s1 = x1;
  s2 = x2 - x3;
  s3 = x2 - x0 + x3;

  // 1-D transform scaling factor is sqrt(2).
  output[0] = (tran_low_t)fdct_round_shift(s0);
  output[1] = (tran_low_t)fdct_round_shift(s1);
  output[2] = (tran_low_t)fdct_round_shift(s2);
  output[3] = (tran_low_t)fdct_round_shift(s3);
}

void vp9_fht4x4_c(const int16_t *input, tran_low_t *output,
                  int stride, int tx_type) {
  if (tx_type == DCT_DCT) {
    vp9_fdct4x4_c(input, output, stride);
  } else {
    tran_low_t out[4 * 4];
    int i, j;
    tran_low_t temp_in[4], temp_out[4];
    const transform_2d ht = FHT_4[tx_type];

    // Columns
    for (i = 0; i < 4; ++i) {
      for (j = 0; j < 4; ++j)
        temp_in[j] = input[j * stride + i] * 16;
      if (i == 0 && temp_in[0])
        temp_in[0] += 1;
      ht.cols(temp_in, temp_out);
      for (j = 0; j < 4; ++j)
        out[j * 4 + i] = temp_out[j];
    }

    // Rows
    for (i = 0; i < 4; ++i) {
      for (j = 0; j < 4; ++j)
        temp_in[j] = out[j + i * 4];
      ht.rows(temp_in, temp_out);
      for (j = 0; j < 4; ++j)
        output[j + i * 4] = (temp_out[j] + 1) >> 2;
    }
  }
}

void vp9_fdct8(const tran_low_t *input, tran_low_t *output) {
  tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;  // canbe16
  tran_high_t t0, t1, t2, t3;                  // needs32
  tran_high_t x0, x1, x2, x3;                  // canbe16

  // stage 1
  s0 = input[0] + input[7];
  s1 = input[1] + input[6];
  s2 = input[2] + input[5];
  s3 = input[3] + input[4];
  s4 = input[3] - input[4];
  s5 = input[2] - input[5];
  s6 = input[1] - input[6];
  s7 = input[0] - input[7];

  // fdct4(step, step);
  x0 = s0 + s3;
  x1 = s1 + s2;
  x2 = s1 - s2;
  x3 = s0 - s3;
  t0 = (x0 + x1) * cospi_16_64;
  t1 = (x0 - x1) * cospi_16_64;
  t2 =  x2 * cospi_24_64 + x3 *  cospi_8_64;
  t3 = -x2 * cospi_8_64  + x3 * cospi_24_64;
  output[0] = (tran_low_t)fdct_round_shift(t0);
  output[2] = (tran_low_t)fdct_round_shift(t2);
  output[4] = (tran_low_t)fdct_round_shift(t1);
  output[6] = (tran_low_t)fdct_round_shift(t3);

  // Stage 2
  t0 = (s6 - s5) * cospi_16_64;
  t1 = (s6 + s5) * cospi_16_64;
  t2 = (tran_low_t)fdct_round_shift(t0);
  t3 = (tran_low_t)fdct_round_shift(t1);

  // Stage 3
  x0 = s4 + t2;
  x1 = s4 - t2;
  x2 = s7 - t3;
  x3 = s7 + t3;

  // Stage 4
  t0 = x0 * cospi_28_64 + x3 *   cospi_4_64;
  t1 = x1 * cospi_12_64 + x2 *  cospi_20_64;
  t2 = x2 * cospi_12_64 + x1 * -cospi_20_64;
  t3 = x3 * cospi_28_64 + x0 *  -cospi_4_64;
  output[1] = (tran_low_t)fdct_round_shift(t0);
  output[3] = (tran_low_t)fdct_round_shift(t2);
  output[5] = (tran_low_t)fdct_round_shift(t1);
  output[7] = (tran_low_t)fdct_round_shift(t3);
}

void vp9_fdct8x8_1_c(const int16_t *input, tran_low_t *output, int stride) {
  int r, c;
  tran_low_t sum = 0;
  for (r = 0; r < 8; ++r)
    for (c = 0; c < 8; ++c)
      sum += input[r * stride + c];

  output[0] = sum;
  output[1] = 0;
}

void vp9_fdct8x8_c(const int16_t *input, tran_low_t *final_output, int stride) {
  int i, j;
  tran_low_t intermediate[64];

  // Transform columns
  {
    tran_low_t *output = intermediate;
    tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;  // canbe16
    tran_high_t t0, t1, t2, t3;                  // needs32
    tran_high_t x0, x1, x2, x3;                  // canbe16

    int i;
    for (i = 0; i < 8; i++) {
      // stage 1
      s0 = (input[0 * stride] + input[7 * stride]) * 4;
      s1 = (input[1 * stride] + input[6 * stride]) * 4;
      s2 = (input[2 * stride] + input[5 * stride]) * 4;
      s3 = (input[3 * stride] + input[4 * stride]) * 4;
      s4 = (input[3 * stride] - input[4 * stride]) * 4;
      s5 = (input[2 * stride] - input[5 * stride]) * 4;
      s6 = (input[1 * stride] - input[6 * stride]) * 4;
      s7 = (input[0 * stride] - input[7 * stride]) * 4;

      // fdct4(step, step);
      x0 = s0 + s3;
      x1 = s1 + s2;
      x2 = s1 - s2;
      x3 = s0 - s3;
      t0 = (x0 + x1) * cospi_16_64;
      t1 = (x0 - x1) * cospi_16_64;
      t2 =  x2 * cospi_24_64 + x3 *  cospi_8_64;
      t3 = -x2 * cospi_8_64  + x3 * cospi_24_64;
      output[0 * 8] = (tran_low_t)fdct_round_shift(t0);
      output[2 * 8] = (tran_low_t)fdct_round_shift(t2);
      output[4 * 8] = (tran_low_t)fdct_round_shift(t1);
      output[6 * 8] = (tran_low_t)fdct_round_shift(t3);

      // Stage 2
      t0 = (s6 - s5) * cospi_16_64;
      t1 = (s6 + s5) * cospi_16_64;
      t2 = fdct_round_shift(t0);
      t3 = fdct_round_shift(t1);

      // Stage 3
      x0 = s4 + t2;
      x1 = s4 - t2;
      x2 = s7 - t3;
      x3 = s7 + t3;

      // Stage 4
      t0 = x0 * cospi_28_64 + x3 *   cospi_4_64;
      t1 = x1 * cospi_12_64 + x2 *  cospi_20_64;
      t2 = x2 * cospi_12_64 + x1 * -cospi_20_64;
      t3 = x3 * cospi_28_64 + x0 *  -cospi_4_64;
      output[1 * 8] = (tran_low_t)fdct_round_shift(t0);
      output[3 * 8] = (tran_low_t)fdct_round_shift(t2);
      output[5 * 8] = (tran_low_t)fdct_round_shift(t1);
      output[7 * 8] = (tran_low_t)fdct_round_shift(t3);
      input++;
      output++;
    }
  }

  // Rows
  for (i = 0; i < 8; ++i) {
    vp9_fdct8(&intermediate[i * 8], &final_output[i * 8]);
    for (j = 0; j < 8; ++j)
      final_output[j + i * 8] /= 2;
  }
}

void vp9_fdct8x8_quant_c(const int16_t *input, int stride,
                         tran_low_t *coeff_ptr, intptr_t n_coeffs,
                         int skip_block,
                         const int16_t *zbin_ptr, const int16_t *round_ptr,
                         const int16_t *quant_ptr,
                         const int16_t *quant_shift_ptr,
                         tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                         const int16_t *dequant_ptr,
                         uint16_t *eob_ptr,
                         const int16_t *scan, const int16_t *iscan) {
  int eob = -1;

  int i, j;
  tran_low_t intermediate[64];

  // Transform columns
  {
    tran_low_t *output = intermediate;
    tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;  // canbe16
    tran_high_t t0, t1, t2, t3;                  // needs32
    tran_high_t x0, x1, x2, x3;                  // canbe16

    int i;
    for (i = 0; i < 8; i++) {
      // stage 1
      s0 = (input[0 * stride] + input[7 * stride]) * 4;
      s1 = (input[1 * stride] + input[6 * stride]) * 4;
      s2 = (input[2 * stride] + input[5 * stride]) * 4;
      s3 = (input[3 * stride] + input[4 * stride]) * 4;
      s4 = (input[3 * stride] - input[4 * stride]) * 4;
      s5 = (input[2 * stride] - input[5 * stride]) * 4;
      s6 = (input[1 * stride] - input[6 * stride]) * 4;
      s7 = (input[0 * stride] - input[7 * stride]) * 4;

      // fdct4(step, step);
      x0 = s0 + s3;
      x1 = s1 + s2;
      x2 = s1 - s2;
      x3 = s0 - s3;
      t0 = (x0 + x1) * cospi_16_64;
      t1 = (x0 - x1) * cospi_16_64;
      t2 =  x2 * cospi_24_64 + x3 *  cospi_8_64;
      t3 = -x2 * cospi_8_64  + x3 * cospi_24_64;
      output[0 * 8] = (tran_low_t)fdct_round_shift(t0);
      output[2 * 8] = (tran_low_t)fdct_round_shift(t2);
      output[4 * 8] = (tran_low_t)fdct_round_shift(t1);
      output[6 * 8] = (tran_low_t)fdct_round_shift(t3);

      // Stage 2
      t0 = (s6 - s5) * cospi_16_64;
      t1 = (s6 + s5) * cospi_16_64;
      t2 = fdct_round_shift(t0);
      t3 = fdct_round_shift(t1);

      // Stage 3
      x0 = s4 + t2;
      x1 = s4 - t2;
      x2 = s7 - t3;
      x3 = s7 + t3;

      // Stage 4
      t0 = x0 * cospi_28_64 + x3 *   cospi_4_64;
      t1 = x1 * cospi_12_64 + x2 *  cospi_20_64;
      t2 = x2 * cospi_12_64 + x1 * -cospi_20_64;
      t3 = x3 * cospi_28_64 + x0 *  -cospi_4_64;
      output[1 * 8] = (tran_low_t)fdct_round_shift(t0);
      output[3 * 8] = (tran_low_t)fdct_round_shift(t2);
      output[5 * 8] = (tran_low_t)fdct_round_shift(t1);
      output[7 * 8] = (tran_low_t)fdct_round_shift(t3);
      input++;
      output++;
    }
  }

  // Rows
  for (i = 0; i < 8; ++i) {
    vp9_fdct8(&intermediate[i * 8], &coeff_ptr[i * 8]);
    for (j = 0; j < 8; ++j)
      coeff_ptr[j + i * 8] /= 2;
  }

  // TODO(jingning) Decide the need of these arguments after the
  // quantization process is completed.
  (void)zbin_ptr;
  (void)quant_shift_ptr;
  (void)iscan;

  memset(qcoeff_ptr, 0, n_coeffs * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr, 0, n_coeffs * sizeof(*dqcoeff_ptr));

  if (!skip_block) {
    // Quantization pass: All coefficients with index >= zero_flag are
    // skippable. Note: zero_flag can be zero.
    for (i = 0; i < n_coeffs; i++) {
      const int rc = scan[i];
      const int coeff = coeff_ptr[rc];
      const int coeff_sign = (coeff >> 31);
      const int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;

      int tmp = clamp(abs_coeff + round_ptr[rc != 0], INT16_MIN, INT16_MAX);
      tmp = (tmp * quant_ptr[rc != 0]) >> 16;

      qcoeff_ptr[rc] = (tmp ^ coeff_sign) - coeff_sign;
      dqcoeff_ptr[rc] = qcoeff_ptr[rc] * dequant_ptr[rc != 0];

      if (tmp)
        eob = i;
    }
  }
  *eob_ptr = eob + 1;
}

void vp9_fdct16x16_1_c(const int16_t *input, tran_low_t *output, int stride) {
  int r, c;
  tran_low_t sum = 0;
  for (r = 0; r < 16; ++r)
    for (c = 0; c < 16; ++c)
      sum += input[r * stride + c];

  output[0] = sum >> 1;
  output[1] = 0;
}

void vp9_fdct16x16_c(const int16_t *input, tran_low_t *output, int stride) {
  // The 2D transform is done with two passes which are actually pretty
  // similar. In the first one, we transform the columns and transpose
  // the results. In the second one, we transform the rows. To achieve that,
  // as the first pass results are transposed, we transpose the columns (that
  // is the transposed rows) and transpose the results (so that it goes back
  // in normal/row positions).
  int pass;
  // We need an intermediate buffer between passes.
  tran_low_t intermediate[256];
  const int16_t *in_pass0 = input;
  const tran_low_t *in = NULL;
  tran_low_t *out = intermediate;
  // Do the two transform/transpose passes
  for (pass = 0; pass < 2; ++pass) {
    tran_high_t step1[8];      // canbe16
    tran_high_t step2[8];      // canbe16
    tran_high_t step3[8];      // canbe16
    tran_high_t input[8];      // canbe16
    tran_high_t temp1, temp2;  // needs32
    int i;
    for (i = 0; i < 16; i++) {
      if (0 == pass) {
        // Calculate input for the first 8 results.
        input[0] = (in_pass0[0 * stride] + in_pass0[15 * stride]) * 4;
        input[1] = (in_pass0[1 * stride] + in_pass0[14 * stride]) * 4;
        input[2] = (in_pass0[2 * stride] + in_pass0[13 * stride]) * 4;
        input[3] = (in_pass0[3 * stride] + in_pass0[12 * stride]) * 4;
        input[4] = (in_pass0[4 * stride] + in_pass0[11 * stride]) * 4;
        input[5] = (in_pass0[5 * stride] + in_pass0[10 * stride]) * 4;
        input[6] = (in_pass0[6 * stride] + in_pass0[ 9 * stride]) * 4;
        input[7] = (in_pass0[7 * stride] + in_pass0[ 8 * stride]) * 4;
        // Calculate input for the next 8 results.
        step1[0] = (in_pass0[7 * stride] - in_pass0[ 8 * stride]) * 4;
        step1[1] = (in_pass0[6 * stride] - in_pass0[ 9 * stride]) * 4;
        step1[2] = (in_pass0[5 * stride] - in_pass0[10 * stride]) * 4;
        step1[3] = (in_pass0[4 * stride] - in_pass0[11 * stride]) * 4;
        step1[4] = (in_pass0[3 * stride] - in_pass0[12 * stride]) * 4;
        step1[5] = (in_pass0[2 * stride] - in_pass0[13 * stride]) * 4;
        step1[6] = (in_pass0[1 * stride] - in_pass0[14 * stride]) * 4;
        step1[7] = (in_pass0[0 * stride] - in_pass0[15 * stride]) * 4;
      } else {
        // Calculate input for the first 8 results.
        input[0] = ((in[0 * 16] + 1) >> 2) + ((in[15 * 16] + 1) >> 2);
        input[1] = ((in[1 * 16] + 1) >> 2) + ((in[14 * 16] + 1) >> 2);
        input[2] = ((in[2 * 16] + 1) >> 2) + ((in[13 * 16] + 1) >> 2);
        input[3] = ((in[3 * 16] + 1) >> 2) + ((in[12 * 16] + 1) >> 2);
        input[4] = ((in[4 * 16] + 1) >> 2) + ((in[11 * 16] + 1) >> 2);
        input[5] = ((in[5 * 16] + 1) >> 2) + ((in[10 * 16] + 1) >> 2);
        input[6] = ((in[6 * 16] + 1) >> 2) + ((in[ 9 * 16] + 1) >> 2);
        input[7] = ((in[7 * 16] + 1) >> 2) + ((in[ 8 * 16] + 1) >> 2);
        // Calculate input for the next 8 results.
        step1[0] = ((in[7 * 16] + 1) >> 2) - ((in[ 8 * 16] + 1) >> 2);
        step1[1] = ((in[6 * 16] + 1) >> 2) - ((in[ 9 * 16] + 1) >> 2);
        step1[2] = ((in[5 * 16] + 1) >> 2) - ((in[10 * 16] + 1) >> 2);
        step1[3] = ((in[4 * 16] + 1) >> 2) - ((in[11 * 16] + 1) >> 2);
        step1[4] = ((in[3 * 16] + 1) >> 2) - ((in[12 * 16] + 1) >> 2);
        step1[5] = ((in[2 * 16] + 1) >> 2) - ((in[13 * 16] + 1) >> 2);
        step1[6] = ((in[1 * 16] + 1) >> 2) - ((in[14 * 16] + 1) >> 2);
        step1[7] = ((in[0 * 16] + 1) >> 2) - ((in[15 * 16] + 1) >> 2);
      }
      // Work on the first eight values; fdct8(input, even_results);
      {
        tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;  // canbe16
        tran_high_t t0, t1, t2, t3;                  // needs32
        tran_high_t x0, x1, x2, x3;                  // canbe16

        // stage 1
        s0 = input[0] + input[7];
        s1 = input[1] + input[6];
        s2 = input[2] + input[5];
        s3 = input[3] + input[4];
        s4 = input[3] - input[4];
        s5 = input[2] - input[5];
        s6 = input[1] - input[6];
        s7 = input[0] - input[7];

        // fdct4(step, step);
        x0 = s0 + s3;
        x1 = s1 + s2;
        x2 = s1 - s2;
        x3 = s0 - s3;
        t0 = (x0 + x1) * cospi_16_64;
        t1 = (x0 - x1) * cospi_16_64;
        t2 = x3 * cospi_8_64  + x2 * cospi_24_64;
        t3 = x3 * cospi_24_64 - x2 * cospi_8_64;
        out[0] = (tran_low_t)fdct_round_shift(t0);
        out[4] = (tran_low_t)fdct_round_shift(t2);
        out[8] = (tran_low_t)fdct_round_shift(t1);
        out[12] = (tran_low_t)fdct_round_shift(t3);

        // Stage 2
        t0 = (s6 - s5) * cospi_16_64;
        t1 = (s6 + s5) * cospi_16_64;
        t2 = fdct_round_shift(t0);
        t3 = fdct_round_shift(t1);

        // Stage 3
        x0 = s4 + t2;
        x1 = s4 - t2;
        x2 = s7 - t3;
        x3 = s7 + t3;

        // Stage 4
        t0 = x0 * cospi_28_64 + x3 *   cospi_4_64;
        t1 = x1 * cospi_12_64 + x2 *  cospi_20_64;
        t2 = x2 * cospi_12_64 + x1 * -cospi_20_64;
        t3 = x3 * cospi_28_64 + x0 *  -cospi_4_64;
        out[2] = (tran_low_t)fdct_round_shift(t0);
        out[6] = (tran_low_t)fdct_round_shift(t2);
        out[10] = (tran_low_t)fdct_round_shift(t1);
        out[14] = (tran_low_t)fdct_round_shift(t3);
      }
      // Work on the next eight values; step1 -> odd_results
      {
        // step 2
        temp1 = (step1[5] - step1[2]) * cospi_16_64;
        temp2 = (step1[4] - step1[3]) * cospi_16_64;
        step2[2] = fdct_round_shift(temp1);
        step2[3] = fdct_round_shift(temp2);
        temp1 = (step1[4] + step1[3]) * cospi_16_64;
        temp2 = (step1[5] + step1[2]) * cospi_16_64;
        step2[4] = fdct_round_shift(temp1);
        step2[5] = fdct_round_shift(temp2);
        // step 3
        step3[0] = step1[0] + step2[3];
        step3[1] = step1[1] + step2[2];
        step3[2] = step1[1] - step2[2];
        step3[3] = step1[0] - step2[3];
        step3[4] = step1[7] - step2[4];
        step3[5] = step1[6] - step2[5];
        step3[6] = step1[6] + step2[5];
        step3[7] = step1[7] + step2[4];
        // step 4
        temp1 = step3[1] *  -cospi_8_64 + step3[6] * cospi_24_64;
        temp2 = step3[2] * cospi_24_64 + step3[5] *  cospi_8_64;
        step2[1] = fdct_round_shift(temp1);
        step2[2] = fdct_round_shift(temp2);
        temp1 = step3[2] * cospi_8_64 - step3[5] * cospi_24_64;
        temp2 = step3[1] * cospi_24_64 + step3[6] *  cospi_8_64;
        step2[5] = fdct_round_shift(temp1);
        step2[6] = fdct_round_shift(temp2);
        // step 5
        step1[0] = step3[0] + step2[1];
        step1[1] = step3[0] - step2[1];
        step1[2] = step3[3] + step2[2];
        step1[3] = step3[3] - step2[2];
        step1[4] = step3[4] - step2[5];
        step1[5] = step3[4] + step2[5];
        step1[6] = step3[7] - step2[6];
        step1[7] = step3[7] + step2[6];
        // step 6
        temp1 = step1[0] * cospi_30_64 + step1[7] *  cospi_2_64;
        temp2 = step1[1] * cospi_14_64 + step1[6] * cospi_18_64;
        out[1] = (tran_low_t)fdct_round_shift(temp1);
        out[9] = (tran_low_t)fdct_round_shift(temp2);
        temp1 = step1[2] * cospi_22_64 + step1[5] * cospi_10_64;
        temp2 = step1[3] *  cospi_6_64 + step1[4] * cospi_26_64;
        out[5] = (tran_low_t)fdct_round_shift(temp1);
        out[13] = (tran_low_t)fdct_round_shift(temp2);
        temp1 = step1[3] * -cospi_26_64 + step1[4] *  cospi_6_64;
        temp2 = step1[2] * -cospi_10_64 + step1[5] * cospi_22_64;
        out[3] = (tran_low_t)fdct_round_shift(temp1);
        out[11] = (tran_low_t)fdct_round_shift(temp2);
        temp1 = step1[1] * -cospi_18_64 + step1[6] * cospi_14_64;
        temp2 = step1[0] *  -cospi_2_64 + step1[7] * cospi_30_64;
        out[7] = (tran_low_t)fdct_round_shift(temp1);
        out[15] = (tran_low_t)fdct_round_shift(temp2);
      }
      // Do next column (which is a transposed row in second/horizontal pass)
      in++;
      in_pass0++;
      out += 16;
    }
    // Setup in/out for next pass.
    in = intermediate;
    out = output;
  }
}

void vp9_fadst8(const tran_low_t *input, tran_low_t *output) {
  tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;

  tran_high_t x0 = input[7];
  tran_high_t x1 = input[0];
  tran_high_t x2 = input[5];
  tran_high_t x3 = input[2];
  tran_high_t x4 = input[3];
  tran_high_t x5 = input[4];
  tran_high_t x6 = input[1];
  tran_high_t x7 = input[6];

  // stage 1
  s0 = cospi_2_64  * x0 + cospi_30_64 * x1;
  s1 = cospi_30_64 * x0 - cospi_2_64  * x1;
  s2 = cospi_10_64 * x2 + cospi_22_64 * x3;
  s3 = cospi_22_64 * x2 - cospi_10_64 * x3;
  s4 = cospi_18_64 * x4 + cospi_14_64 * x5;
  s5 = cospi_14_64 * x4 - cospi_18_64 * x5;
  s6 = cospi_26_64 * x6 + cospi_6_64  * x7;
  s7 = cospi_6_64  * x6 - cospi_26_64 * x7;

  x0 = fdct_round_shift(s0 + s4);
  x1 = fdct_round_shift(s1 + s5);
  x2 = fdct_round_shift(s2 + s6);
  x3 = fdct_round_shift(s3 + s7);
  x4 = fdct_round_shift(s0 - s4);
  x5 = fdct_round_shift(s1 - s5);
  x6 = fdct_round_shift(s2 - s6);
  x7 = fdct_round_shift(s3 - s7);

  // stage 2
  s0 = x0;
  s1 = x1;
  s2 = x2;
  s3 = x3;
  s4 = cospi_8_64  * x4 + cospi_24_64 * x5;
  s5 = cospi_24_64 * x4 - cospi_8_64  * x5;
  s6 = - cospi_24_64 * x6 + cospi_8_64  * x7;
  s7 =   cospi_8_64  * x6 + cospi_24_64 * x7;

  x0 = s0 + s2;
  x1 = s1 + s3;
  x2 = s0 - s2;
  x3 = s1 - s3;
  x4 = fdct_round_shift(s4 + s6);
  x5 = fdct_round_shift(s5 + s7);
  x6 = fdct_round_shift(s4 - s6);
  x7 = fdct_round_shift(s5 - s7);

  // stage 3
  s2 = cospi_16_64 * (x2 + x3);
  s3 = cospi_16_64 * (x2 - x3);
  s6 = cospi_16_64 * (x6 + x7);
  s7 = cospi_16_64 * (x6 - x7);

  x2 = fdct_round_shift(s2);
  x3 = fdct_round_shift(s3);
  x6 = fdct_round_shift(s6);
  x7 = fdct_round_shift(s7);

  output[0] = (tran_low_t)x0;
  output[1] = (tran_low_t)-x4;
  output[2] = (tran_low_t)x6;
  output[3] = (tran_low_t)-x2;
  output[4] = (tran_low_t)x3;
  output[5] = (tran_low_t)-x7;
  output[6] = (tran_low_t)x5;
  output[7] = (tran_low_t)-x1;
}

void vp9_fht8x8_c(const int16_t *input, tran_low_t *output,
                  int stride, int tx_type) {
  if (tx_type == DCT_DCT) {
    vp9_fdct8x8_c(input, output, stride);
  } else {
    tran_low_t out[64];
    int i, j;
    tran_low_t temp_in[8], temp_out[8];
    const transform_2d ht = FHT_8[tx_type];

    // Columns
    for (i = 0; i < 8; ++i) {
      for (j = 0; j < 8; ++j)
        temp_in[j] = input[j * stride + i] * 4;
      ht.cols(temp_in, temp_out);
      for (j = 0; j < 8; ++j)
        out[j * 8 + i] = temp_out[j];
    }

    // Rows
    for (i = 0; i < 8; ++i) {
      for (j = 0; j < 8; ++j)
        temp_in[j] = out[j + i * 8];
      ht.rows(temp_in, temp_out);
      for (j = 0; j < 8; ++j)
        output[j + i * 8] = (temp_out[j] + (temp_out[j] < 0)) >> 1;
    }
  }
}

/* 4-point reversible, orthonormal Walsh-Hadamard in 3.5 adds, 0.5 shifts per
   pixel. */
void vp9_fwht4x4_c(const int16_t *input, tran_low_t *output, int stride) {
  int i;
  tran_high_t a1, b1, c1, d1, e1;
  const int16_t *ip_pass0 = input;
  const tran_low_t *ip = NULL;
  tran_low_t *op = output;

  for (i = 0; i < 4; i++) {
    a1 = ip_pass0[0 * stride];
    b1 = ip_pass0[1 * stride];
    c1 = ip_pass0[2 * stride];
    d1 = ip_pass0[3 * stride];

    a1 += b1;
    d1 = d1 - c1;
    e1 = (a1 - d1) >> 1;
    b1 = e1 - b1;
    c1 = e1 - c1;
    a1 -= c1;
    d1 += b1;
    op[0] = (tran_low_t)a1;
    op[4] = (tran_low_t)c1;
    op[8] = (tran_low_t)d1;
    op[12] = (tran_low_t)b1;

    ip_pass0++;
    op++;
  }
  ip = output;
  op = output;

  for (i = 0; i < 4; i++) {
    a1 = ip[0];
    b1 = ip[1];
    c1 = ip[2];
    d1 = ip[3];

    a1 += b1;
    d1 -= c1;
    e1 = (a1 - d1) >> 1;
    b1 = e1 - b1;
    c1 = e1 - c1;
    a1 -= c1;
    d1 += b1;
    op[0] = (tran_low_t)(a1 * UNIT_QUANT_FACTOR);
    op[1] = (tran_low_t)(c1 * UNIT_QUANT_FACTOR);
    op[2] = (tran_low_t)(d1 * UNIT_QUANT_FACTOR);
    op[3] = (tran_low_t)(b1 * UNIT_QUANT_FACTOR);

    ip += 4;
    op += 4;
  }
}

// Rewrote to use same algorithm as others.
void vp9_fdct16(const tran_low_t in[16], tran_low_t out[16]) {
  tran_high_t step1[8];      // canbe16
  tran_high_t step2[8];      // canbe16
  tran_high_t step3[8];      // canbe16
  tran_high_t input[8];      // canbe16
  tran_high_t temp1, temp2;  // needs32

  // step 1
  input[0] = in[0] + in[15];
  input[1] = in[1] + in[14];
  input[2] = in[2] + in[13];
  input[3] = in[3] + in[12];
  input[4] = in[4] + in[11];
  input[5] = in[5] + in[10];
  input[6] = in[6] + in[ 9];
  input[7] = in[7] + in[ 8];

  step1[0] = in[7] - in[ 8];
  step1[1] = in[6] - in[ 9];
  step1[2] = in[5] - in[10];
  step1[3] = in[4] - in[11];
  step1[4] = in[3] - in[12];
  step1[5] = in[2] - in[13];
  step1[6] = in[1] - in[14];
  step1[7] = in[0] - in[15];

  // fdct8(step, step);
  {
    tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;  // canbe16
    tran_high_t t0, t1, t2, t3;                  // needs32
    tran_high_t x0, x1, x2, x3;                  // canbe16

    // stage 1
    s0 = input[0] + input[7];
    s1 = input[1] + input[6];
    s2 = input[2] + input[5];
    s3 = input[3] + input[4];
    s4 = input[3] - input[4];
    s5 = input[2] - input[5];
    s6 = input[1] - input[6];
    s7 = input[0] - input[7];

    // fdct4(step, step);
    x0 = s0 + s3;
    x1 = s1 + s2;
    x2 = s1 - s2;
    x3 = s0 - s3;
    t0 = (x0 + x1) * cospi_16_64;
    t1 = (x0 - x1) * cospi_16_64;
    t2 = x3 * cospi_8_64  + x2 * cospi_24_64;
    t3 = x3 * cospi_24_64 - x2 * cospi_8_64;
    out[0] = (tran_low_t)fdct_round_shift(t0);
    out[4] = (tran_low_t)fdct_round_shift(t2);
    out[8] = (tran_low_t)fdct_round_shift(t1);
    out[12] = (tran_low_t)fdct_round_shift(t3);

    // Stage 2
    t0 = (s6 - s5) * cospi_16_64;
    t1 = (s6 + s5) * cospi_16_64;
    t2 = fdct_round_shift(t0);
    t3 = fdct_round_shift(t1);

    // Stage 3
    x0 = s4 + t2;
    x1 = s4 - t2;
    x2 = s7 - t3;
    x3 = s7 + t3;

    // Stage 4
    t0 = x0 * cospi_28_64 + x3 *   cospi_4_64;
    t1 = x1 * cospi_12_64 + x2 *  cospi_20_64;
    t2 = x2 * cospi_12_64 + x1 * -cospi_20_64;
    t3 = x3 * cospi_28_64 + x0 *  -cospi_4_64;
    out[2] = (tran_low_t)fdct_round_shift(t0);
    out[6] = (tran_low_t)fdct_round_shift(t2);
    out[10] = (tran_low_t)fdct_round_shift(t1);
    out[14] = (tran_low_t)fdct_round_shift(t3);
  }

  // step 2
  temp1 = (step1[5] - step1[2]) * cospi_16_64;
  temp2 = (step1[4] - step1[3]) * cospi_16_64;
  step2[2] = fdct_round_shift(temp1);
  step2[3] = fdct_round_shift(temp2);
  temp1 = (step1[4] + step1[3]) * cospi_16_64;
  temp2 = (step1[5] + step1[2]) * cospi_16_64;
  step2[4] = fdct_round_shift(temp1);
  step2[5] = fdct_round_shift(temp2);

  // step 3
  step3[0] = step1[0] + step2[3];
  step3[1] = step1[1] + step2[2];
  step3[2] = step1[1] - step2[2];
  step3[3] = step1[0] - step2[3];
  step3[4] = step1[7] - step2[4];
  step3[5] = step1[6] - step2[5];
  step3[6] = step1[6] + step2[5];
  step3[7] = step1[7] + step2[4];

  // step 4
  temp1 = step3[1] *  -cospi_8_64 + step3[6] * cospi_24_64;
  temp2 = step3[2] * cospi_24_64 + step3[5] *  cospi_8_64;
  step2[1] = fdct_round_shift(temp1);
  step2[2] = fdct_round_shift(temp2);
  temp1 = step3[2] * cospi_8_64 - step3[5] * cospi_24_64;
  temp2 = step3[1] * cospi_24_64 + step3[6] *  cospi_8_64;
  step2[5] = fdct_round_shift(temp1);
  step2[6] = fdct_round_shift(temp2);

  // step 5
  step1[0] = step3[0] + step2[1];
  step1[1] = step3[0] - step2[1];
  step1[2] = step3[3] + step2[2];
  step1[3] = step3[3] - step2[2];
  step1[4] = step3[4] - step2[5];
  step1[5] = step3[4] + step2[5];
  step1[6] = step3[7] - step2[6];
  step1[7] = step3[7] + step2[6];

  // step 6
  temp1 = step1[0] * cospi_30_64 + step1[7] *  cospi_2_64;
  temp2 = step1[1] * cospi_14_64 + step1[6] * cospi_18_64;
  out[1] = (tran_low_t)fdct_round_shift(temp1);
  out[9] = (tran_low_t)fdct_round_shift(temp2);

  temp1 = step1[2] * cospi_22_64 + step1[5] * cospi_10_64;
  temp2 = step1[3] *  cospi_6_64 + step1[4] * cospi_26_64;
  out[5] = (tran_low_t)fdct_round_shift(temp1);
  out[13] = (tran_low_t)fdct_round_shift(temp2);

  temp1 = step1[3] * -cospi_26_64 + step1[4] *  cospi_6_64;
  temp2 = step1[2] * -cospi_10_64 + step1[5] * cospi_22_64;
  out[3] = (tran_low_t)fdct_round_shift(temp1);
  out[11] = (tran_low_t)fdct_round_shift(temp2);

  temp1 = step1[1] * -cospi_18_64 + step1[6] * cospi_14_64;
  temp2 = step1[0] *  -cospi_2_64 + step1[7] * cospi_30_64;
  out[7] = (tran_low_t)fdct_round_shift(temp1);
  out[15] = (tran_low_t)fdct_round_shift(temp2);
}

void vp9_fadst16(const tran_low_t *input, tran_low_t *output) {
  tran_high_t s0, s1, s2, s3, s4, s5, s6, s7, s8;
  tran_high_t s9, s10, s11, s12, s13, s14, s15;

  tran_high_t x0 = input[15];
  tran_high_t x1 = input[0];
  tran_high_t x2 = input[13];
  tran_high_t x3 = input[2];
  tran_high_t x4 = input[11];
  tran_high_t x5 = input[4];
  tran_high_t x6 = input[9];
  tran_high_t x7 = input[6];
  tran_high_t x8 = input[7];
  tran_high_t x9 = input[8];
  tran_high_t x10 = input[5];
  tran_high_t x11 = input[10];
  tran_high_t x12 = input[3];
  tran_high_t x13 = input[12];
  tran_high_t x14 = input[1];
  tran_high_t x15 = input[14];

  // stage 1
  s0 = x0 * cospi_1_64  + x1 * cospi_31_64;
  s1 = x0 * cospi_31_64 - x1 * cospi_1_64;
  s2 = x2 * cospi_5_64  + x3 * cospi_27_64;
  s3 = x2 * cospi_27_64 - x3 * cospi_5_64;
  s4 = x4 * cospi_9_64  + x5 * cospi_23_64;
  s5 = x4 * cospi_23_64 - x5 * cospi_9_64;
  s6 = x6 * cospi_13_64 + x7 * cospi_19_64;
  s7 = x6 * cospi_19_64 - x7 * cospi_13_64;
  s8 = x8 * cospi_17_64 + x9 * cospi_15_64;
  s9 = x8 * cospi_15_64 - x9 * cospi_17_64;
  s10 = x10 * cospi_21_64 + x11 * cospi_11_64;
  s11 = x10 * cospi_11_64 - x11 * cospi_21_64;
  s12 = x12 * cospi_25_64 + x13 * cospi_7_64;
  s13 = x12 * cospi_7_64  - x13 * cospi_25_64;
  s14 = x14 * cospi_29_64 + x15 * cospi_3_64;
  s15 = x14 * cospi_3_64  - x15 * cospi_29_64;

  x0 = fdct_round_shift(s0 + s8);
  x1 = fdct_round_shift(s1 + s9);
  x2 = fdct_round_shift(s2 + s10);
  x3 = fdct_round_shift(s3 + s11);
  x4 = fdct_round_shift(s4 + s12);
  x5 = fdct_round_shift(s5 + s13);
  x6 = fdct_round_shift(s6 + s14);
  x7 = fdct_round_shift(s7 + s15);
  x8  = fdct_round_shift(s0 - s8);
  x9  = fdct_round_shift(s1 - s9);
  x10 = fdct_round_shift(s2 - s10);
  x11 = fdct_round_shift(s3 - s11);
  x12 = fdct_round_shift(s4 - s12);
  x13 = fdct_round_shift(s5 - s13);
  x14 = fdct_round_shift(s6 - s14);
  x15 = fdct_round_shift(s7 - s15);

  // stage 2
  s0 = x0;
  s1 = x1;
  s2 = x2;
  s3 = x3;
  s4 = x4;
  s5 = x5;
  s6 = x6;
  s7 = x7;
  s8 =    x8 * cospi_4_64   + x9 * cospi_28_64;
  s9 =    x8 * cospi_28_64  - x9 * cospi_4_64;
  s10 =   x10 * cospi_20_64 + x11 * cospi_12_64;
  s11 =   x10 * cospi_12_64 - x11 * cospi_20_64;
  s12 = - x12 * cospi_28_64 + x13 * cospi_4_64;
  s13 =   x12 * cospi_4_64  + x13 * cospi_28_64;
  s14 = - x14 * cospi_12_64 + x15 * cospi_20_64;
  s15 =   x14 * cospi_20_64 + x15 * cospi_12_64;

  x0 = s0 + s4;
  x1 = s1 + s5;
  x2 = s2 + s6;
  x3 = s3 + s7;
  x4 = s0 - s4;
  x5 = s1 - s5;
  x6 = s2 - s6;
  x7 = s3 - s7;
  x8 = fdct_round_shift(s8 + s12);
  x9 = fdct_round_shift(s9 + s13);
  x10 = fdct_round_shift(s10 + s14);
  x11 = fdct_round_shift(s11 + s15);
  x12 = fdct_round_shift(s8 - s12);
  x13 = fdct_round_shift(s9 - s13);
  x14 = fdct_round_shift(s10 - s14);
  x15 = fdct_round_shift(s11 - s15);

  // stage 3
  s0 = x0;
  s1 = x1;
  s2 = x2;
  s3 = x3;
  s4 = x4 * cospi_8_64  + x5 * cospi_24_64;
  s5 = x4 * cospi_24_64 - x5 * cospi_8_64;
  s6 = - x6 * cospi_24_64 + x7 * cospi_8_64;
  s7 =   x6 * cospi_8_64  + x7 * cospi_24_64;
  s8 = x8;
  s9 = x9;
  s10 = x10;
  s11 = x11;
  s12 = x12 * cospi_8_64  + x13 * cospi_24_64;
  s13 = x12 * cospi_24_64 - x13 * cospi_8_64;
  s14 = - x14 * cospi_24_64 + x15 * cospi_8_64;
  s15 =   x14 * cospi_8_64  + x15 * cospi_24_64;

  x0 = s0 + s2;
  x1 = s1 + s3;
  x2 = s0 - s2;
  x3 = s1 - s3;
  x4 = fdct_round_shift(s4 + s6);
  x5 = fdct_round_shift(s5 + s7);
  x6 = fdct_round_shift(s4 - s6);
  x7 = fdct_round_shift(s5 - s7);
  x8 = s8 + s10;
  x9 = s9 + s11;
  x10 = s8 - s10;
  x11 = s9 - s11;
  x12 = fdct_round_shift(s12 + s14);
  x13 = fdct_round_shift(s13 + s15);
  x14 = fdct_round_shift(s12 - s14);
  x15 = fdct_round_shift(s13 - s15);

  // stage 4
  s2 = (- cospi_16_64) * (x2 + x3);
  s3 = cospi_16_64 * (x2 - x3);
  s6 = cospi_16_64 * (x6 + x7);
  s7 = cospi_16_64 * (- x6 + x7);
  s10 = cospi_16_64 * (x10 + x11);
  s11 = cospi_16_64 * (- x10 + x11);
  s14 = (- cospi_16_64) * (x14 + x15);
  s15 = cospi_16_64 * (x14 - x15);

  x2 = fdct_round_shift(s2);
  x3 = fdct_round_shift(s3);
  x6 = fdct_round_shift(s6);
  x7 = fdct_round_shift(s7);
  x10 = fdct_round_shift(s10);
  x11 = fdct_round_shift(s11);
  x14 = fdct_round_shift(s14);
  x15 = fdct_round_shift(s15);

  output[0] = (tran_low_t)x0;
  output[1] = (tran_low_t)-x8;
  output[2] = (tran_low_t)x12;
  output[3] = (tran_low_t)-x4;
  output[4] = (tran_low_t)x6;
  output[5] = (tran_low_t)x14;
  output[6] = (tran_low_t)x10;
  output[7] = (tran_low_t)x2;
  output[8] = (tran_low_t)x3;
  output[9] = (tran_low_t)x11;
  output[10] = (tran_low_t)x15;
  output[11] = (tran_low_t)x7;
  output[12] = (tran_low_t)x5;
  output[13] = (tran_low_t)-x13;
  output[14] = (tran_low_t)x9;
  output[15] = (tran_low_t)-x1;
}

void vp9_fht16x16_c(const int16_t *input, tran_low_t *output,
                    int stride, int tx_type) {
  if (tx_type == DCT_DCT) {
    vp9_fdct16x16_c(input, output, stride);
  } else {
    tran_low_t out[256];
    int i, j;
    tran_low_t temp_in[16], temp_out[16];
    const transform_2d ht = FHT_16[tx_type];

    // Columns
    for (i = 0; i < 16; ++i) {
      for (j = 0; j < 16; ++j)
        temp_in[j] = input[j * stride + i] * 4;
      ht.cols(temp_in, temp_out);
      for (j = 0; j < 16; ++j)
        out[j * 16 + i] = (temp_out[j] + 1 + (temp_out[j] < 0)) >> 2;
    }

    // Rows
    for (i = 0; i < 16; ++i) {
      for (j = 0; j < 16; ++j)
        temp_in[j] = out[j + i * 16];
      ht.rows(temp_in, temp_out);
      for (j = 0; j < 16; ++j)
        output[j + i * 16] = temp_out[j];
    }
  }
}

static INLINE tran_high_t dct_32_round(tran_high_t input) {
  tran_high_t rv = ROUND_POWER_OF_TWO(input, DCT_CONST_BITS);
  // TODO(debargha, peter.derivaz): Find new bounds for this assert,
  // and make the bounds consts.
  // assert(-131072 <= rv && rv <= 131071);
  return rv;
}

static INLINE tran_high_t half_round_shift(tran_high_t input) {
  tran_high_t rv = (input + 1 + (input < 0)) >> 2;
  return rv;
}

void vp9_fdct32(const tran_high_t *input, tran_high_t *output, int round) {
  tran_high_t step[32];
  // Stage 1
  step[0] = input[0] + input[(32 - 1)];
  step[1] = input[1] + input[(32 - 2)];
  step[2] = input[2] + input[(32 - 3)];
  step[3] = input[3] + input[(32 - 4)];
  step[4] = input[4] + input[(32 - 5)];
  step[5] = input[5] + input[(32 - 6)];
  step[6] = input[6] + input[(32 - 7)];
  step[7] = input[7] + input[(32 - 8)];
  step[8] = input[8] + input[(32 - 9)];
  step[9] = input[9] + input[(32 - 10)];
  step[10] = input[10] + input[(32 - 11)];
  step[11] = input[11] + input[(32 - 12)];
  step[12] = input[12] + input[(32 - 13)];
  step[13] = input[13] + input[(32 - 14)];
  step[14] = input[14] + input[(32 - 15)];
  step[15] = input[15] + input[(32 - 16)];
  step[16] = -input[16] + input[(32 - 17)];
  step[17] = -input[17] + input[(32 - 18)];
  step[18] = -input[18] + input[(32 - 19)];
  step[19] = -input[19] + input[(32 - 20)];
  step[20] = -input[20] + input[(32 - 21)];
  step[21] = -input[21] + input[(32 - 22)];
  step[22] = -input[22] + input[(32 - 23)];
  step[23] = -input[23] + input[(32 - 24)];
  step[24] = -input[24] + input[(32 - 25)];
  step[25] = -input[25] + input[(32 - 26)];
  step[26] = -input[26] + input[(32 - 27)];
  step[27] = -input[27] + input[(32 - 28)];
  step[28] = -input[28] + input[(32 - 29)];
  step[29] = -input[29] + input[(32 - 30)];
  step[30] = -input[30] + input[(32 - 31)];
  step[31] = -input[31] + input[(32 - 32)];

  // Stage 2
  output[0] = step[0] + step[16 - 1];
  output[1] = step[1] + step[16 - 2];
  output[2] = step[2] + step[16 - 3];
  output[3] = step[3] + step[16 - 4];
  output[4] = step[4] + step[16 - 5];
  output[5] = step[5] + step[16 - 6];
  output[6] = step[6] + step[16 - 7];
  output[7] = step[7] + step[16 - 8];
  output[8] = -step[8] + step[16 - 9];
  output[9] = -step[9] + step[16 - 10];
  output[10] = -step[10] + step[16 - 11];
  output[11] = -step[11] + step[16 - 12];
  output[12] = -step[12] + step[16 - 13];
  output[13] = -step[13] + step[16 - 14];
  output[14] = -step[14] + step[16 - 15];
  output[15] = -step[15] + step[16 - 16];

  output[16] = step[16];
  output[17] = step[17];
  output[18] = step[18];
  output[19] = step[19];

  output[20] = dct_32_round((-step[20] + step[27]) * cospi_16_64);
  output[21] = dct_32_round((-step[21] + step[26]) * cospi_16_64);
  output[22] = dct_32_round((-step[22] + step[25]) * cospi_16_64);
  output[23] = dct_32_round((-step[23] + step[24]) * cospi_16_64);

  output[24] = dct_32_round((step[24] + step[23]) * cospi_16_64);
  output[25] = dct_32_round((step[25] + step[22]) * cospi_16_64);
  output[26] = dct_32_round((step[26] + step[21]) * cospi_16_64);
  output[27] = dct_32_round((step[27] + step[20]) * cospi_16_64);

  output[28] = step[28];
  output[29] = step[29];
  output[30] = step[30];
  output[31] = step[31];

  // dump the magnitude by 4, hence the intermediate values are within
  // the range of 16 bits.
  if (round) {
    output[0] = half_round_shift(output[0]);
    output[1] = half_round_shift(output[1]);
    output[2] = half_round_shift(output[2]);
    output[3] = half_round_shift(output[3]);
    output[4] = half_round_shift(output[4]);
    output[5] = half_round_shift(output[5]);
    output[6] = half_round_shift(output[6]);
    output[7] = half_round_shift(output[7]);
    output[8] = half_round_shift(output[8]);
    output[9] = half_round_shift(output[9]);
    output[10] = half_round_shift(output[10]);
    output[11] = half_round_shift(output[11]);
    output[12] = half_round_shift(output[12]);
    output[13] = half_round_shift(output[13]);
    output[14] = half_round_shift(output[14]);
    output[15] = half_round_shift(output[15]);

    output[16] = half_round_shift(output[16]);
    output[17] = half_round_shift(output[17]);
    output[18] = half_round_shift(output[18]);
    output[19] = half_round_shift(output[19]);
    output[20] = half_round_shift(output[20]);
    output[21] = half_round_shift(output[21]);
    output[22] = half_round_shift(output[22]);
    output[23] = half_round_shift(output[23]);
    output[24] = half_round_shift(output[24]);
    output[25] = half_round_shift(output[25]);
    output[26] = half_round_shift(output[26]);
    output[27] = half_round_shift(output[27]);
    output[28] = half_round_shift(output[28]);
    output[29] = half_round_shift(output[29]);
    output[30] = half_round_shift(output[30]);
    output[31] = half_round_shift(output[31]);
  }

  // Stage 3
  step[0] = output[0] + output[(8 - 1)];
  step[1] = output[1] + output[(8 - 2)];
  step[2] = output[2] + output[(8 - 3)];
  step[3] = output[3] + output[(8 - 4)];
  step[4] = -output[4] + output[(8 - 5)];
  step[5] = -output[5] + output[(8 - 6)];
  step[6] = -output[6] + output[(8 - 7)];
  step[7] = -output[7] + output[(8 - 8)];
  step[8] = output[8];
  step[9] = output[9];
  step[10] = dct_32_round((-output[10] + output[13]) * cospi_16_64);
  step[11] = dct_32_round((-output[11] + output[12]) * cospi_16_64);
  step[12] = dct_32_round((output[12] + output[11]) * cospi_16_64);
  step[13] = dct_32_round((output[13] + output[10]) * cospi_16_64);
  step[14] = output[14];
  step[15] = output[15];

  step[16] = output[16] + output[23];
  step[17] = output[17] + output[22];
  step[18] = output[18] + output[21];
  step[19] = output[19] + output[20];
  step[20] = -output[20] + output[19];
  step[21] = -output[21] + output[18];
  step[22] = -output[22] + output[17];
  step[23] = -output[23] + output[16];
  step[24] = -output[24] + output[31];
  step[25] = -output[25] + output[30];
  step[26] = -output[26] + output[29];
  step[27] = -output[27] + output[28];
  step[28] = output[28] + output[27];
  step[29] = output[29] + output[26];
  step[30] = output[30] + output[25];
  step[31] = output[31] + output[24];

  // Stage 4
  output[0] = step[0] + step[3];
  output[1] = step[1] + step[2];
  output[2] = -step[2] + step[1];
  output[3] = -step[3] + step[0];
  output[4] = step[4];
  output[5] = dct_32_round((-step[5] + step[6]) * cospi_16_64);
  output[6] = dct_32_round((step[6] + step[5]) * cospi_16_64);
  output[7] = step[7];
  output[8] = step[8] + step[11];
  output[9] = step[9] + step[10];
  output[10] = -step[10] + step[9];
  output[11] = -step[11] + step[8];
  output[12] = -step[12] + step[15];
  output[13] = -step[13] + step[14];
  output[14] = step[14] + step[13];
  output[15] = step[15] + step[12];

  output[16] = step[16];
  output[17] = step[17];
  output[18] = dct_32_round(step[18] * -cospi_8_64 + step[29] * cospi_24_64);
  output[19] = dct_32_round(step[19] * -cospi_8_64 + step[28] * cospi_24_64);
  output[20] = dct_32_round(step[20] * -cospi_24_64 + step[27] * -cospi_8_64);
  output[21] = dct_32_round(step[21] * -cospi_24_64 + step[26] * -cospi_8_64);
  output[22] = step[22];
  output[23] = step[23];
  output[24] = step[24];
  output[25] = step[25];
  output[26] = dct_32_round(step[26] * cospi_24_64 + step[21] * -cospi_8_64);
  output[27] = dct_32_round(step[27] * cospi_24_64 + step[20] * -cospi_8_64);
  output[28] = dct_32_round(step[28] * cospi_8_64 + step[19] * cospi_24_64);
  output[29] = dct_32_round(step[29] * cospi_8_64 + step[18] * cospi_24_64);
  output[30] = step[30];
  output[31] = step[31];

  // Stage 5
  step[0] = dct_32_round((output[0] + output[1]) * cospi_16_64);
  step[1] = dct_32_round((-output[1] + output[0]) * cospi_16_64);
  step[2] = dct_32_round(output[2] * cospi_24_64 + output[3] * cospi_8_64);
  step[3] = dct_32_round(output[3] * cospi_24_64 - output[2] * cospi_8_64);
  step[4] = output[4] + output[5];
  step[5] = -output[5] + output[4];
  step[6] = -output[6] + output[7];
  step[7] = output[7] + output[6];
  step[8] = output[8];
  step[9] = dct_32_round(output[9] * -cospi_8_64 + output[14] * cospi_24_64);
  step[10] = dct_32_round(output[10] * -cospi_24_64 + output[13] * -cospi_8_64);
  step[11] = output[11];
  step[12] = output[12];
  step[13] = dct_32_round(output[13] * cospi_24_64 + output[10] * -cospi_8_64);
  step[14] = dct_32_round(output[14] * cospi_8_64 + output[9] * cospi_24_64);
  step[15] = output[15];

  step[16] = output[16] + output[19];
  step[17] = output[17] + output[18];
  step[18] = -output[18] + output[17];
  step[19] = -output[19] + output[16];
  step[20] = -output[20] + output[23];
  step[21] = -output[21] + output[22];
  step[22] = output[22] + output[21];
  step[23] = output[23] + output[20];
  step[24] = output[24] + output[27];
  step[25] = output[25] + output[26];
  step[26] = -output[26] + output[25];
  step[27] = -output[27] + output[24];
  step[28] = -output[28] + output[31];
  step[29] = -output[29] + output[30];
  step[30] = output[30] + output[29];
  step[31] = output[31] + output[28];

  // Stage 6
  output[0] = step[0];
  output[1] = step[1];
  output[2] = step[2];
  output[3] = step[3];
  output[4] = dct_32_round(step[4] * cospi_28_64 + step[7] * cospi_4_64);
  output[5] = dct_32_round(step[5] * cospi_12_64 + step[6] * cospi_20_64);
  output[6] = dct_32_round(step[6] * cospi_12_64 + step[5] * -cospi_20_64);
  output[7] = dct_32_round(step[7] * cospi_28_64 + step[4] * -cospi_4_64);
  output[8] = step[8] + step[9];
  output[9] = -step[9] + step[8];
  output[10] = -step[10] + step[11];
  output[11] = step[11] + step[10];
  output[12] = step[12] + step[13];
  output[13] = -step[13] + step[12];
  output[14] = -step[14] + step[15];
  output[15] = step[15] + step[14];

  output[16] = step[16];
  output[17] = dct_32_round(step[17] * -cospi_4_64 + step[30] * cospi_28_64);
  output[18] = dct_32_round(step[18] * -cospi_28_64 + step[29] * -cospi_4_64);
  output[19] = step[19];
  output[20] = step[20];
  output[21] = dct_32_round(step[21] * -cospi_20_64 + step[26] * cospi_12_64);
  output[22] = dct_32_round(step[22] * -cospi_12_64 + step[25] * -cospi_20_64);
  output[23] = step[23];
  output[24] = step[24];
  output[25] = dct_32_round(step[25] * cospi_12_64 + step[22] * -cospi_20_64);
  output[26] = dct_32_round(step[26] * cospi_20_64 + step[21] * cospi_12_64);
  output[27] = step[27];
  output[28] = step[28];
  output[29] = dct_32_round(step[29] * cospi_28_64 + step[18] * -cospi_4_64);
  output[30] = dct_32_round(step[30] * cospi_4_64 + step[17] * cospi_28_64);
  output[31] = step[31];

  // Stage 7
  step[0] = output[0];
  step[1] = output[1];
  step[2] = output[2];
  step[3] = output[3];
  step[4] = output[4];
  step[5] = output[5];
  step[6] = output[6];
  step[7] = output[7];
  step[8] = dct_32_round(output[8] * cospi_30_64 + output[15] * cospi_2_64);
  step[9] = dct_32_round(output[9] * cospi_14_64 + output[14] * cospi_18_64);
  step[10] = dct_32_round(output[10] * cospi_22_64 + output[13] * cospi_10_64);
  step[11] = dct_32_round(output[11] * cospi_6_64 + output[12] * cospi_26_64);
  step[12] = dct_32_round(output[12] * cospi_6_64 + output[11] * -cospi_26_64);
  step[13] = dct_32_round(output[13] * cospi_22_64 + output[10] * -cospi_10_64);
  step[14] = dct_32_round(output[14] * cospi_14_64 + output[9] * -cospi_18_64);
  step[15] = dct_32_round(output[15] * cospi_30_64 + output[8] * -cospi_2_64);

  step[16] = output[16] + output[17];
  step[17] = -output[17] + output[16];
  step[18] = -output[18] + output[19];
  step[19] = output[19] + output[18];
  step[20] = output[20] + output[21];
  step[21] = -output[21] + output[20];
  step[22] = -output[22] + output[23];
  step[23] = output[23] + output[22];
  step[24] = output[24] + output[25];
  step[25] = -output[25] + output[24];
  step[26] = -output[26] + output[27];
  step[27] = output[27] + output[26];
  step[28] = output[28] + output[29];
  step[29] = -output[29] + output[28];
  step[30] = -output[30] + output[31];
  step[31] = output[31] + output[30];

  // Final stage --- outputs indices are bit-reversed.
  output[0]  = step[0];
  output[16] = step[1];
  output[8]  = step[2];
  output[24] = step[3];
  output[4]  = step[4];
  output[20] = step[5];
  output[12] = step[6];
  output[28] = step[7];
  output[2]  = step[8];
  output[18] = step[9];
  output[10] = step[10];
  output[26] = step[11];
  output[6]  = step[12];
  output[22] = step[13];
  output[14] = step[14];
  output[30] = step[15];

  output[1]  = dct_32_round(step[16] * cospi_31_64 + step[31] * cospi_1_64);
  output[17] = dct_32_round(step[17] * cospi_15_64 + step[30] * cospi_17_64);
  output[9]  = dct_32_round(step[18] * cospi_23_64 + step[29] * cospi_9_64);
  output[25] = dct_32_round(step[19] * cospi_7_64 + step[28] * cospi_25_64);
  output[5]  = dct_32_round(step[20] * cospi_27_64 + step[27] * cospi_5_64);
  output[21] = dct_32_round(step[21] * cospi_11_64 + step[26] * cospi_21_64);
  output[13] = dct_32_round(step[22] * cospi_19_64 + step[25] * cospi_13_64);
  output[29] = dct_32_round(step[23] * cospi_3_64 + step[24] * cospi_29_64);
  output[3]  = dct_32_round(step[24] * cospi_3_64 + step[23] * -cospi_29_64);
  output[19] = dct_32_round(step[25] * cospi_19_64 + step[22] * -cospi_13_64);
  output[11] = dct_32_round(step[26] * cospi_11_64 + step[21] * -cospi_21_64);
  output[27] = dct_32_round(step[27] * cospi_27_64 + step[20] * -cospi_5_64);
  output[7]  = dct_32_round(step[28] * cospi_7_64 + step[19] * -cospi_25_64);
  output[23] = dct_32_round(step[29] * cospi_23_64 + step[18] * -cospi_9_64);
  output[15] = dct_32_round(step[30] * cospi_15_64 + step[17] * -cospi_17_64);
  output[31] = dct_32_round(step[31] * cospi_31_64 + step[16] * -cospi_1_64);
}

void vp9_fdct32x32_1_c(const int16_t *input, tran_low_t *output, int stride) {
  int r, c;
  tran_low_t sum = 0;
  for (r = 0; r < 32; ++r)
    for (c = 0; c < 32; ++c)
      sum += input[r * stride + c];

  output[0] = sum >> 3;
  output[1] = 0;
}

void vp9_fdct32x32_c(const int16_t *input, tran_low_t *out, int stride) {
  int i, j;
  tran_high_t output[32 * 32];

  // Columns
  for (i = 0; i < 32; ++i) {
    tran_high_t temp_in[32], temp_out[32];
    for (j = 0; j < 32; ++j)
      temp_in[j] = input[j * stride + i] * 4;
    vp9_fdct32(temp_in, temp_out, 0);
    for (j = 0; j < 32; ++j)
      output[j * 32 + i] = (temp_out[j] + 1 + (temp_out[j] > 0)) >> 2;
  }

  // Rows
  for (i = 0; i < 32; ++i) {
    tran_high_t temp_in[32], temp_out[32];
    for (j = 0; j < 32; ++j)
      temp_in[j] = output[j + i * 32];
    vp9_fdct32(temp_in, temp_out, 0);
    for (j = 0; j < 32; ++j)
      out[j + i * 32] =
          (tran_low_t)((temp_out[j] + 1 + (temp_out[j] < 0)) >> 2);
  }
}

// Note that although we use dct_32_round in dct32 computation flow,
// this 2d fdct32x32 for rate-distortion optimization loop is operating
// within 16 bits precision.
void vp9_fdct32x32_rd_c(const int16_t *input, tran_low_t *out, int stride) {
  int i, j;
  tran_high_t output[32 * 32];

  // Columns
  for (i = 0; i < 32; ++i) {
    tran_high_t temp_in[32], temp_out[32];
    for (j = 0; j < 32; ++j)
      temp_in[j] = input[j * stride + i] * 4;
    vp9_fdct32(temp_in, temp_out, 0);
    for (j = 0; j < 32; ++j)
      // TODO(cd): see quality impact of only doing
      //           output[j * 32 + i] = (temp_out[j] + 1) >> 2;
      //           PS: also change code in vp9/encoder/x86/vp9_dct_sse2.c
      output[j * 32 + i] = (temp_out[j] + 1 + (temp_out[j] > 0)) >> 2;
  }

  // Rows
  for (i = 0; i < 32; ++i) {
    tran_high_t temp_in[32], temp_out[32];
    for (j = 0; j < 32; ++j)
      temp_in[j] = output[j + i * 32];
    vp9_fdct32(temp_in, temp_out, 1);
    for (j = 0; j < 32; ++j)
      out[j + i * 32] = (tran_low_t)temp_out[j];
  }
}

#if CONFIG_VP9_HIGHBITDEPTH
void vp9_highbd_fdct4x4_c(const int16_t *input, tran_low_t *output,
                          int stride) {
  vp9_fdct4x4_c(input, output, stride);
}

void vp9_highbd_fht4x4_c(const int16_t *input, tran_low_t *output,
                         int stride, int tx_type) {
  vp9_fht4x4_c(input, output, stride, tx_type);
}

void vp9_highbd_fdct8x8_1_c(const int16_t *input, tran_low_t *final_output,
                            int stride) {
  vp9_fdct8x8_1_c(input, final_output, stride);
}

void vp9_highbd_fdct8x8_c(const int16_t *input, tran_low_t *final_output,
                          int stride) {
  vp9_fdct8x8_c(input, final_output, stride);
}

void vp9_highbd_fdct16x16_1_c(const int16_t *input, tran_low_t *output,
                              int stride) {
  vp9_fdct16x16_1_c(input, output, stride);
}

void vp9_highbd_fdct16x16_c(const int16_t *input, tran_low_t *output,
                            int stride) {
  vp9_fdct16x16_c(input, output, stride);
}

void vp9_highbd_fht8x8_c(const int16_t *input, tran_low_t *output,
                         int stride, int tx_type) {
  vp9_fht8x8_c(input, output, stride, tx_type);
}

void vp9_highbd_fwht4x4_c(const int16_t *input, tran_low_t *output,
                          int stride) {
  vp9_fwht4x4_c(input, output, stride);
}

void vp9_highbd_fht16x16_c(const int16_t *input, tran_low_t *output,
                           int stride, int tx_type) {
  vp9_fht16x16_c(input, output, stride, tx_type);
}

void vp9_highbd_fdct32x32_1_c(const int16_t *input, tran_low_t *out,
                              int stride) {
  vp9_fdct32x32_1_c(input, out, stride);
}

void vp9_highbd_fdct32x32_c(const int16_t *input, tran_low_t *out, int stride) {
  vp9_fdct32x32_c(input, out, stride);
}

void vp9_highbd_fdct32x32_rd_c(const int16_t *input, tran_low_t *out,
                               int stride) {
  vp9_fdct32x32_rd_c(input, out, stride);
}
#endif  // CONFIG_VP9_HIGHBITDEPTH
