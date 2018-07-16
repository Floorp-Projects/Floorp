/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*!\file
 * \brief Describes film grain parameters and film grain synthesis
 *
 */
#ifndef AOM_AOM_GRAIN_SYNTHESIS_H_
#define AOM_AOM_GRAIN_SYNTHESIS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "aom_dsp/aom_dsp_common.h"
#include "aom/aom_image.h"

/*!\brief Structure containing film grain synthesis parameters for a frame
 *
 * This structure contains input parameters for film grain synthesis
 */
typedef struct {
  int apply_grain;

  int update_parameters;

  // 8 bit values
  int scaling_points_y[14][2];
  int num_y_points;  // value: 0..14

  // 8 bit values
  int scaling_points_cb[10][2];
  int num_cb_points;  // value: 0..10

  // 8 bit values
  int scaling_points_cr[10][2];
  int num_cr_points;  // value: 0..10

  int scaling_shift;  // values : 8..11

  int ar_coeff_lag;  // values:  0..3

  // 8 bit values
  int ar_coeffs_y[24];
  int ar_coeffs_cb[25];
  int ar_coeffs_cr[25];

  // Shift value: AR coeffs range
  // 6: [-2, 2)
  // 7: [-1, 1)
  // 8: [-0.5, 0.5)
  // 9: [-0.25, 0.25)
  int ar_coeff_shift;  // values : 6..9

  int cb_mult;       // 8 bits
  int cb_luma_mult;  // 8 bits
  int cb_offset;     // 9 bits

  int cr_mult;       // 8 bits
  int cr_luma_mult;  // 8 bits
  int cr_offset;     // 9 bits

  int overlap_flag;

  int clip_to_restricted_range;

  int bit_depth;  // video bit depth

  int chroma_scaling_from_luma;

  int grain_scale_shift;

  uint16_t random_seed;
} aom_film_grain_t;

/*!\brief Add film grain
 *
 * Add film grain to an image
 *
 * \param[in]    grain_params     Grain parameters
 * \param[in]    luma             luma plane
 * \param[in]    cb               cb plane
 * \param[in]    cr               cr plane
 * \param[in]    height           luma plane height
 * \param[in]    width            luma plane width
 * \param[in]    luma_stride      luma plane stride
 * \param[in]    chroma_stride    chroma plane stride
 */
void av1_add_film_grain_run(aom_film_grain_t *grain_params, uint8_t *luma,
                            uint8_t *cb, uint8_t *cr, int height, int width,
                            int luma_stride, int chroma_stride,
                            int use_high_bit_depth, int chroma_subsamp_y,
                            int chroma_subsamp_x, int mc_identity);

/*!\brief Add film grain
 *
 * Add film grain to an image
 *
 * \param[in]    grain_params     Grain parameters
 * \param[in]    src              Source image
 * \param[in]    dst              Resulting image with grain
 */
void av1_add_film_grain(aom_film_grain_t *grain_params, aom_image_t *src,
                        aom_image_t *dst);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_GRAIN_SYNTHESIS_H_
