/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/* clang-format off */

#if !defined(_state_H)
# define _state_H (1)

typedef struct od_state     od_state;
typedef struct od_adapt_ctx od_adapt_ctx;

# include "generic_code.h"
# include "odintrin.h"
# include "pvq.h"

/*Adaptation speed of scalar Laplace encoding.*/
# define OD_SCALAR_ADAPT_SPEED (4)

struct od_adapt_ctx {
  /* Support for PVQ encode/decode */
  od_pvq_adapt_ctx pvq;

  generic_encoder model_dc[OD_NPLANES_MAX];

  int ex_dc[OD_NPLANES_MAX][OD_TXSIZES][3];
  int ex_g[OD_NPLANES_MAX][OD_TXSIZES];

  /* Joint skip flag for DC and AC */
  uint16_t skip_cdf[OD_TXSIZES*2][CDF_SIZE(4)];
};

struct od_state {
  od_adapt_ctx *adapt;
  unsigned char pvq_qm_q4[OD_NPLANES_MAX][OD_QM_SIZE];
  /* Quantization matrices and their inverses. */
  int16_t qm[OD_QM_BUFFER_SIZE];
  int16_t qm_inv[OD_QM_BUFFER_SIZE];
};

void od_adapt_ctx_reset(od_adapt_ctx *state, int is_keyframe);
void od_init_skipped_coeffs(int16_t *d, int16_t *pred, int is_keyframe,
 int bo, int n, int w);

#endif
