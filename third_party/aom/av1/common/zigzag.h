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

#if !defined(_zigzag_H)
# define _zigzag_H (1)

extern const unsigned char OD_ZIGZAG4_DCT_DCT[15][2];
extern const unsigned char OD_ZIGZAG4_ADST_DCT[15][2];
extern const unsigned char OD_ZIGZAG4_DCT_ADST[15][2];
#define OD_ZIGZAG4_ADST_ADST OD_ZIGZAG4_DCT_DCT

extern const unsigned char OD_ZIGZAG8_DCT_DCT[48][2];
extern const unsigned char OD_ZIGZAG8_ADST_DCT[48][2];
extern const unsigned char OD_ZIGZAG8_DCT_ADST[48][2];
#define OD_ZIGZAG8_ADST_ADST OD_ZIGZAG8_DCT_DCT

extern const unsigned char OD_ZIGZAG16_DCT_DCT[192][2];
extern const unsigned char OD_ZIGZAG16_ADST_DCT[192][2];
extern const unsigned char OD_ZIGZAG16_DCT_ADST[192][2];
#define OD_ZIGZAG16_ADST_ADST OD_ZIGZAG16_DCT_DCT

extern const unsigned char OD_ZIGZAG32_DCT_DCT[768][2];
#endif
