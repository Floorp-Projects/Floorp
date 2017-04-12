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

#include <stdio.h>
#include "test/av1_txfm_test.h"

namespace libaom_test {

int get_txfm1d_size(TX_SIZE tx_size) { return tx_size_wide[tx_size]; }

void get_txfm1d_type(TX_TYPE txfm2d_type, TYPE_TXFM *type0, TYPE_TXFM *type1) {
  switch (txfm2d_type) {
    case DCT_DCT:
      *type0 = TYPE_DCT;
      *type1 = TYPE_DCT;
      break;
    case ADST_DCT:
      *type0 = TYPE_ADST;
      *type1 = TYPE_DCT;
      break;
    case DCT_ADST:
      *type0 = TYPE_DCT;
      *type1 = TYPE_ADST;
      break;
    case ADST_ADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_ADST;
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      *type0 = TYPE_ADST;
      *type1 = TYPE_DCT;
      break;
    case DCT_FLIPADST:
      *type0 = TYPE_DCT;
      *type1 = TYPE_ADST;
      break;
    case FLIPADST_FLIPADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_ADST;
      break;
    case ADST_FLIPADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_ADST;
      break;
    case FLIPADST_ADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_ADST;
      break;
#endif  // CONFIG_EXT_TX
    default:
      *type0 = TYPE_DCT;
      *type1 = TYPE_DCT;
      assert(0);
      break;
  }
}

double invSqrt2 = 1 / pow(2, 0.5);

void reference_dct_1d(const double *in, double *out, int size) {
  for (int k = 0; k < size; ++k) {
    out[k] = 0;
    for (int n = 0; n < size; ++n) {
      out[k] += in[n] * cos(M_PI * (2 * n + 1) * k / (2 * size));
    }
    if (k == 0) out[k] = out[k] * invSqrt2;
  }
}

void reference_adst_1d(const double *in, double *out, int size) {
  for (int k = 0; k < size; ++k) {
    out[k] = 0;
    for (int n = 0; n < size; ++n) {
      out[k] += in[n] * sin(M_PI * (2 * n + 1) * (2 * k + 1) / (4 * size));
    }
  }
}

void reference_hybrid_1d(double *in, double *out, int size, int type) {
  if (type == TYPE_DCT)
    reference_dct_1d(in, out, size);
  else
    reference_adst_1d(in, out, size);
}

void reference_hybrid_2d(double *in, double *out, int size, int type0,
                         int type1) {
  double *tempOut = new double[size * size];

  for (int r = 0; r < size; r++) {
    // out ->tempOut
    for (int c = 0; c < size; c++) {
      tempOut[r * size + c] = in[c * size + r];
    }
  }

  // dct each row: in -> out
  for (int r = 0; r < size; r++) {
    reference_hybrid_1d(tempOut + r * size, out + r * size, size, type0);
  }

  for (int r = 0; r < size; r++) {
    // out ->tempOut
    for (int c = 0; c < size; c++) {
      tempOut[r * size + c] = out[c * size + r];
    }
  }

  for (int r = 0; r < size; r++) {
    reference_hybrid_1d(tempOut + r * size, out + r * size, size, type1);
  }
  delete[] tempOut;
}

template <typename Type>
void fliplr(Type *dest, int stride, int length) {
  int i, j;
  for (i = 0; i < length; ++i) {
    for (j = 0; j < length / 2; ++j) {
      const Type tmp = dest[i * stride + j];
      dest[i * stride + j] = dest[i * stride + length - 1 - j];
      dest[i * stride + length - 1 - j] = tmp;
    }
  }
}

template <typename Type>
void flipud(Type *dest, int stride, int length) {
  int i, j;
  for (j = 0; j < length; ++j) {
    for (i = 0; i < length / 2; ++i) {
      const Type tmp = dest[i * stride + j];
      dest[i * stride + j] = dest[(length - 1 - i) * stride + j];
      dest[(length - 1 - i) * stride + j] = tmp;
    }
  }
}

template <typename Type>
void fliplrud(Type *dest, int stride, int length) {
  int i, j;
  for (i = 0; i < length / 2; ++i) {
    for (j = 0; j < length; ++j) {
      const Type tmp = dest[i * stride + j];
      dest[i * stride + j] = dest[(length - 1 - i) * stride + length - 1 - j];
      dest[(length - 1 - i) * stride + length - 1 - j] = tmp;
    }
  }
}

template void fliplr<double>(double *dest, int stride, int length);
template void flipud<double>(double *dest, int stride, int length);
template void fliplrud<double>(double *dest, int stride, int length);

}  // namespace libaom_test
