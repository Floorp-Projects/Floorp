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

#ifndef AOM_AOM_DSP_POSTPROC_H_
#define AOM_AOM_DSP_POSTPROC_H_

#ifdef __cplusplus
extern "C" {
#endif

// Fills a noise buffer with gaussian noise strength determined by sigma.
int aom_setup_noise(double sigma, int size, char *noise);

#ifdef __cplusplus
}
#endif

#endif  // AOM_AOM_DSP_POSTPROC_H_
