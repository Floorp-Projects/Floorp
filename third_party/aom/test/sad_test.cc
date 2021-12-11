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

#include <string.h>
#include <limits.h>
#include <stdio.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "aom/aom_codec.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

typedef unsigned int (*SadMxNFunc)(const uint8_t *src_ptr, int src_stride,
                                   const uint8_t *ref_ptr, int ref_stride);
typedef ::testing::tuple<int, int, SadMxNFunc, int> SadMxNParam;

typedef uint32_t (*SadMxNAvgFunc)(const uint8_t *src_ptr, int src_stride,
                                  const uint8_t *ref_ptr, int ref_stride,
                                  const uint8_t *second_pred);
typedef ::testing::tuple<int, int, SadMxNAvgFunc, int> SadMxNAvgParam;

typedef void (*JntCompAvgFunc)(uint8_t *comp_pred, const uint8_t *pred,
                               int width, int height, const uint8_t *ref,
                               int ref_stride,
                               const JNT_COMP_PARAMS *jcp_param);
typedef ::testing::tuple<int, int, JntCompAvgFunc, int> JntCompAvgParam;

typedef unsigned int (*JntSadMxhFunc)(const uint8_t *src_ptr, int src_stride,
                                      const uint8_t *ref_ptr, int ref_stride,
                                      int width, int height);
typedef ::testing::tuple<int, int, JntSadMxhFunc, int> JntSadMxhParam;

typedef uint32_t (*JntSadMxNAvgFunc)(const uint8_t *src_ptr, int src_stride,
                                     const uint8_t *ref_ptr, int ref_stride,
                                     const uint8_t *second_pred,
                                     const JNT_COMP_PARAMS *jcp_param);
typedef ::testing::tuple<int, int, JntSadMxNAvgFunc, int> JntSadMxNAvgParam;

typedef void (*SadMxNx4Func)(const uint8_t *src_ptr, int src_stride,
                             const uint8_t *const ref_ptr[], int ref_stride,
                             uint32_t *sad_array);
typedef ::testing::tuple<int, int, SadMxNx4Func, int> SadMxNx4Param;

using libaom_test::ACMRandom;

namespace {
class SADTestBase : public ::testing::Test {
 public:
  SADTestBase(int width, int height, int bit_depth)
      : width_(width), height_(height), bd_(bit_depth) {}

  static void SetUpTestCase() {
    source_data8_ = reinterpret_cast<uint8_t *>(
        aom_memalign(kDataAlignment, kDataBlockSize));
    reference_data8_ = reinterpret_cast<uint8_t *>(
        aom_memalign(kDataAlignment, kDataBufferSize));
    second_pred8_ =
        reinterpret_cast<uint8_t *>(aom_memalign(kDataAlignment, 128 * 128));
    comp_pred8_ =
        reinterpret_cast<uint8_t *>(aom_memalign(kDataAlignment, 128 * 128));
    comp_pred8_test_ =
        reinterpret_cast<uint8_t *>(aom_memalign(kDataAlignment, 128 * 128));
    source_data16_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, kDataBlockSize * sizeof(uint16_t)));
    reference_data16_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, kDataBufferSize * sizeof(uint16_t)));
    second_pred16_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, 128 * 128 * sizeof(uint16_t)));
    comp_pred16_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, 128 * 128 * sizeof(uint16_t)));
    comp_pred16_test_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, 128 * 128 * sizeof(uint16_t)));
  }

  static void TearDownTestCase() {
    aom_free(source_data8_);
    source_data8_ = NULL;
    aom_free(reference_data8_);
    reference_data8_ = NULL;
    aom_free(second_pred8_);
    second_pred8_ = NULL;
    aom_free(comp_pred8_);
    comp_pred8_ = NULL;
    aom_free(comp_pred8_test_);
    comp_pred8_test_ = NULL;
    aom_free(source_data16_);
    source_data16_ = NULL;
    aom_free(reference_data16_);
    reference_data16_ = NULL;
    aom_free(second_pred16_);
    second_pred16_ = NULL;
    aom_free(comp_pred16_);
    comp_pred16_ = NULL;
    aom_free(comp_pred16_test_);
    comp_pred16_test_ = NULL;
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  // Handle up to 4 128x128 blocks, with stride up to 256
  static const int kDataAlignment = 16;
  static const int kDataBlockSize = 128 * 256;
  static const int kDataBufferSize = 4 * kDataBlockSize;

  virtual void SetUp() {
    if (bd_ == -1) {
      use_high_bit_depth_ = false;
      bit_depth_ = AOM_BITS_8;
      source_data_ = source_data8_;
      reference_data_ = reference_data8_;
      second_pred_ = second_pred8_;
      comp_pred_ = comp_pred8_;
      comp_pred_test_ = comp_pred8_test_;
    } else {
      use_high_bit_depth_ = true;
      bit_depth_ = static_cast<aom_bit_depth_t>(bd_);
      source_data_ = CONVERT_TO_BYTEPTR(source_data16_);
      reference_data_ = CONVERT_TO_BYTEPTR(reference_data16_);
      second_pred_ = CONVERT_TO_BYTEPTR(second_pred16_);
      comp_pred_ = CONVERT_TO_BYTEPTR(comp_pred16_);
      comp_pred_test_ = CONVERT_TO_BYTEPTR(comp_pred16_test_);
    }
    mask_ = (1 << bit_depth_) - 1;
    source_stride_ = (width_ + 31) & ~31;
    reference_stride_ = width_ * 2;
    rnd_.Reset(ACMRandom::DeterministicSeed());
  }

  virtual uint8_t *GetReference(int block_idx) {
    if (use_high_bit_depth_)
      return CONVERT_TO_BYTEPTR(CONVERT_TO_SHORTPTR(reference_data_) +
                                block_idx * kDataBlockSize);
    return reference_data_ + block_idx * kDataBlockSize;
  }

  // Sum of Absolute Differences. Given two blocks, calculate the absolute
  // difference between two pixels in the same relative location; accumulate.
  unsigned int ReferenceSAD(int block_idx) {
    unsigned int sad = 0;
    const uint8_t *const reference8 = GetReference(block_idx);
    const uint8_t *const source8 = source_data_;
    const uint16_t *const reference16 =
        CONVERT_TO_SHORTPTR(GetReference(block_idx));
    const uint16_t *const source16 = CONVERT_TO_SHORTPTR(source_data_);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          sad += abs(source8[h * source_stride_ + w] -
                     reference8[h * reference_stride_ + w]);
        } else {
          sad += abs(source16[h * source_stride_ + w] -
                     reference16[h * reference_stride_ + w]);
        }
      }
    }
    return sad;
  }

  // Sum of Absolute Differences Average. Given two blocks, and a prediction
  // calculate the absolute difference between one pixel and average of the
  // corresponding and predicted pixels; accumulate.
  unsigned int ReferenceSADavg(int block_idx) {
    unsigned int sad = 0;
    const uint8_t *const reference8 = GetReference(block_idx);
    const uint8_t *const source8 = source_data_;
    const uint8_t *const second_pred8 = second_pred_;
    const uint16_t *const reference16 =
        CONVERT_TO_SHORTPTR(GetReference(block_idx));
    const uint16_t *const source16 = CONVERT_TO_SHORTPTR(source_data_);
    const uint16_t *const second_pred16 = CONVERT_TO_SHORTPTR(second_pred_);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          const int tmp = second_pred8[h * width_ + w] +
                          reference8[h * reference_stride_ + w];
          const uint8_t comp_pred = ROUND_POWER_OF_TWO(tmp, 1);
          sad += abs(source8[h * source_stride_ + w] - comp_pred);
        } else {
          const int tmp = second_pred16[h * width_ + w] +
                          reference16[h * reference_stride_ + w];
          const uint16_t comp_pred = ROUND_POWER_OF_TWO(tmp, 1);
          sad += abs(source16[h * source_stride_ + w] - comp_pred);
        }
      }
    }
    return sad;
  }

  void ReferenceJntCompAvg(int block_idx) {
    const uint8_t *const reference8 = GetReference(block_idx);
    const uint8_t *const second_pred8 = second_pred_;
    uint8_t *const comp_pred8 = comp_pred_;
    const uint16_t *const reference16 =
        CONVERT_TO_SHORTPTR(GetReference(block_idx));
    const uint16_t *const second_pred16 = CONVERT_TO_SHORTPTR(second_pred_);
    uint16_t *const comp_pred16 = CONVERT_TO_SHORTPTR(comp_pred_);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          const int tmp =
              second_pred8[h * width_ + w] * jcp_param_.bck_offset +
              reference8[h * reference_stride_ + w] * jcp_param_.fwd_offset;
          comp_pred8[h * width_ + w] = ROUND_POWER_OF_TWO(tmp, 4);
        } else {
          const int tmp =
              second_pred16[h * width_ + w] * jcp_param_.bck_offset +
              reference16[h * reference_stride_ + w] * jcp_param_.fwd_offset;
          comp_pred16[h * width_ + w] = ROUND_POWER_OF_TWO(tmp, 4);
        }
      }
    }
  }

  unsigned int ReferenceJntSADavg(int block_idx) {
    unsigned int sad = 0;
    const uint8_t *const reference8 = GetReference(block_idx);
    const uint8_t *const source8 = source_data_;
    const uint8_t *const second_pred8 = second_pred_;
    const uint16_t *const reference16 =
        CONVERT_TO_SHORTPTR(GetReference(block_idx));
    const uint16_t *const source16 = CONVERT_TO_SHORTPTR(source_data_);
    const uint16_t *const second_pred16 = CONVERT_TO_SHORTPTR(second_pred_);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          const int tmp =
              second_pred8[h * width_ + w] * jcp_param_.bck_offset +
              reference8[h * reference_stride_ + w] * jcp_param_.fwd_offset;
          const uint8_t comp_pred = ROUND_POWER_OF_TWO(tmp, 4);
          sad += abs(source8[h * source_stride_ + w] - comp_pred);
        } else {
          const int tmp =
              second_pred16[h * width_ + w] * jcp_param_.bck_offset +
              reference16[h * reference_stride_ + w] * jcp_param_.fwd_offset;
          const uint16_t comp_pred = ROUND_POWER_OF_TWO(tmp, 4);
          sad += abs(source16[h * source_stride_ + w] - comp_pred);
        }
      }
    }
    return sad;
  }

  void FillConstant(uint8_t *data, int stride, uint16_t fill_constant) {
    uint8_t *data8 = data;
    uint16_t *data16 = CONVERT_TO_SHORTPTR(data);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          data8[h * stride + w] = static_cast<uint8_t>(fill_constant);
        } else {
          data16[h * stride + w] = fill_constant;
        }
      }
    }
  }

  void FillRandom(uint8_t *data, int stride) {
    uint8_t *data8 = data;
    uint16_t *data16 = CONVERT_TO_SHORTPTR(data);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          data8[h * stride + w] = rnd_.Rand8();
        } else {
          data16[h * stride + w] = rnd_.Rand16() & mask_;
        }
      }
    }
  }

  int width_, height_, mask_, bd_;
  aom_bit_depth_t bit_depth_;
  static uint8_t *source_data_;
  static uint8_t *reference_data_;
  static uint8_t *second_pred_;
  int source_stride_;
  bool use_high_bit_depth_;
  static uint8_t *source_data8_;
  static uint8_t *reference_data8_;
  static uint8_t *second_pred8_;
  static uint16_t *source_data16_;
  static uint16_t *reference_data16_;
  static uint16_t *second_pred16_;
  int reference_stride_;
  static uint8_t *comp_pred_;
  static uint8_t *comp_pred8_;
  static uint16_t *comp_pred16_;
  static uint8_t *comp_pred_test_;
  static uint8_t *comp_pred8_test_;
  static uint16_t *comp_pred16_test_;
  JNT_COMP_PARAMS jcp_param_;

  ACMRandom rnd_;
};

class SADx4Test : public ::testing::WithParamInterface<SadMxNx4Param>,
                  public SADTestBase {
 public:
  SADx4Test() : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  void SADs(unsigned int *results) {
    const uint8_t *references[] = { GetReference(0), GetReference(1),
                                    GetReference(2), GetReference(3) };

    ASM_REGISTER_STATE_CHECK(GET_PARAM(2)(
        source_data_, source_stride_, references, reference_stride_, results));
  }

  void CheckSADs() {
    unsigned int reference_sad, exp_sad[4];

    SADs(exp_sad);
    for (int block = 0; block < 4; ++block) {
      reference_sad = ReferenceSAD(block);

      EXPECT_EQ(reference_sad, exp_sad[block]) << "block " << block;
    }
  }
};

class SADTest : public ::testing::WithParamInterface<SadMxNParam>,
                public SADTestBase {
 public:
  SADTest() : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  unsigned int SAD(int block_idx) {
    unsigned int ret;
    const uint8_t *const reference = GetReference(block_idx);

    ASM_REGISTER_STATE_CHECK(ret = GET_PARAM(2)(source_data_, source_stride_,
                                                reference, reference_stride_));
    return ret;
  }

  void CheckSAD() {
    const unsigned int reference_sad = ReferenceSAD(0);
    const unsigned int exp_sad = SAD(0);

    ASSERT_EQ(reference_sad, exp_sad);
  }

  void SpeedSAD() {
    int test_count = 20000000;
    while (test_count > 0) {
      SAD(0);
      test_count -= 1;
    }
  }
};

class SADavgTest : public ::testing::WithParamInterface<SadMxNAvgParam>,
                   public SADTestBase {
 public:
  SADavgTest() : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  unsigned int SAD_avg(int block_idx) {
    unsigned int ret;
    const uint8_t *const reference = GetReference(block_idx);

    ASM_REGISTER_STATE_CHECK(ret = GET_PARAM(2)(source_data_, source_stride_,
                                                reference, reference_stride_,
                                                second_pred_));
    return ret;
  }

  void CheckSAD() {
    const unsigned int reference_sad = ReferenceSADavg(0);
    const unsigned int exp_sad = SAD_avg(0);

    ASSERT_EQ(reference_sad, exp_sad);
  }
};

class JntCompAvgTest : public ::testing::WithParamInterface<JntCompAvgParam>,
                       public SADTestBase {
 public:
  JntCompAvgTest() : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  void jnt_comp_avg(int block_idx) {
    const uint8_t *const reference = GetReference(block_idx);

    ASM_REGISTER_STATE_CHECK(GET_PARAM(2)(comp_pred_test_, second_pred_, width_,
                                          height_, reference, reference_stride_,
                                          &jcp_param_));
  }

  void CheckCompAvg() {
    for (int j = 0; j < 2; ++j) {
      for (int i = 0; i < 4; ++i) {
        jcp_param_.fwd_offset = quant_dist_lookup_table[j][i][0];
        jcp_param_.bck_offset = quant_dist_lookup_table[j][i][1];

        ReferenceJntCompAvg(0);
        jnt_comp_avg(0);

        for (int y = 0; y < height_; ++y)
          for (int x = 0; x < width_; ++x)
            ASSERT_EQ(comp_pred_[y * width_ + x],
                      comp_pred_test_[y * width_ + x]);
      }
    }
  }
};

class JntSADTest : public ::testing::WithParamInterface<JntSadMxhParam>,
                   public SADTestBase {
 public:
  JntSADTest() : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  unsigned int SAD(int block_idx) {
    unsigned int ret;
    const uint8_t *const reference = GetReference(block_idx);

    ASM_REGISTER_STATE_CHECK(ret = GET_PARAM(2)(source_data_, source_stride_,
                                                reference, reference_stride_,
                                                GET_PARAM(0), GET_PARAM(1)));
    return ret;
  }

  void CheckSAD() {
    const unsigned int reference_sad = ReferenceSAD(0);
    const unsigned int exp_sad = SAD(0);

    ASSERT_EQ(reference_sad, exp_sad);
  }

  void SpeedSAD() {
    int test_count = 20000000;
    while (test_count > 0) {
      SAD(0);
      test_count -= 1;
    }
  }
};

class JntSADavgTest : public ::testing::WithParamInterface<JntSadMxNAvgParam>,
                      public SADTestBase {
 public:
  JntSADavgTest() : SADTestBase(GET_PARAM(0), GET_PARAM(1), GET_PARAM(3)) {}

 protected:
  unsigned int jnt_SAD_avg(int block_idx) {
    unsigned int ret;
    const uint8_t *const reference = GetReference(block_idx);

    ASM_REGISTER_STATE_CHECK(ret = GET_PARAM(2)(source_data_, source_stride_,
                                                reference, reference_stride_,
                                                second_pred_, &jcp_param_));
    return ret;
  }

  void CheckSAD() {
    for (int j = 0; j < 2; ++j) {
      for (int i = 0; i < 4; ++i) {
        jcp_param_.fwd_offset = quant_dist_lookup_table[j][i][0];
        jcp_param_.bck_offset = quant_dist_lookup_table[j][i][1];

        const unsigned int reference_sad = ReferenceJntSADavg(0);
        const unsigned int exp_sad = jnt_SAD_avg(0);

        ASSERT_EQ(reference_sad, exp_sad);
      }
    }
  }
};

uint8_t *SADTestBase::source_data_ = NULL;
uint8_t *SADTestBase::reference_data_ = NULL;
uint8_t *SADTestBase::second_pred_ = NULL;
uint8_t *SADTestBase::comp_pred_ = NULL;
uint8_t *SADTestBase::comp_pred_test_ = NULL;
uint8_t *SADTestBase::source_data8_ = NULL;
uint8_t *SADTestBase::reference_data8_ = NULL;
uint8_t *SADTestBase::second_pred8_ = NULL;
uint8_t *SADTestBase::comp_pred8_ = NULL;
uint8_t *SADTestBase::comp_pred8_test_ = NULL;
uint16_t *SADTestBase::source_data16_ = NULL;
uint16_t *SADTestBase::reference_data16_ = NULL;
uint16_t *SADTestBase::second_pred16_ = NULL;
uint16_t *SADTestBase::comp_pred16_ = NULL;
uint16_t *SADTestBase::comp_pred16_test_ = NULL;

TEST_P(SADTest, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(reference_data_, reference_stride_, mask_);
  CheckSAD();
}

TEST_P(SADTest, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(reference_data_, reference_stride_, 0);
  CheckSAD();
}

TEST_P(SADTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADTest, ShortSrc) {
  const int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  int test_count = 2000;
  while (test_count > 0) {
    FillRandom(source_data_, source_stride_);
    FillRandom(reference_data_, reference_stride_);
    CheckSAD();
    test_count -= 1;
  }
  source_stride_ = tmp_stride;
}

#define SPEED_TEST (0)
#if SPEED_TEST
TEST_P(SADTest, Speed) {
  const int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  SpeedSAD();
  source_stride_ = tmp_stride;
}
#endif

TEST_P(SADavgTest, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(reference_data_, reference_stride_, mask_);
  FillConstant(second_pred_, width_, 0);
  CheckSAD();
}
TEST_P(SADavgTest, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(reference_data_, reference_stride_, 0);
  FillConstant(second_pred_, width_, 0);
  CheckSAD();
}

TEST_P(SADavgTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADavgTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADavgTest, ShortSrc) {
  const int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  int test_count = 2000;
  while (test_count > 0) {
    FillRandom(source_data_, source_stride_);
    FillRandom(reference_data_, reference_stride_);
    FillRandom(second_pred_, width_);
    CheckSAD();
    test_count -= 1;
  }
  source_stride_ = tmp_stride;
}

TEST_P(JntCompAvgTest, MaxRef) {
  FillConstant(reference_data_, reference_stride_, mask_);
  FillConstant(second_pred_, width_, 0);
  CheckCompAvg();
}

TEST_P(JntCompAvgTest, MaxSecondPred) {
  FillConstant(reference_data_, reference_stride_, 0);
  FillConstant(second_pred_, width_, mask_);
  CheckCompAvg();
}

TEST_P(JntCompAvgTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckCompAvg();
  reference_stride_ = tmp_stride;
}

TEST_P(JntCompAvgTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckCompAvg();
  reference_stride_ = tmp_stride;
}

TEST_P(JntSADTest, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(reference_data_, reference_stride_, mask_);
  CheckSAD();
}

TEST_P(JntSADTest, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(reference_data_, reference_stride_, 0);
  CheckSAD();
}

TEST_P(JntSADTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(JntSADTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(JntSADTest, ShortSrc) {
  const int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  int test_count = 2000;
  while (test_count > 0) {
    FillRandom(source_data_, source_stride_);
    FillRandom(reference_data_, reference_stride_);
    CheckSAD();
    test_count -= 1;
  }
  source_stride_ = tmp_stride;
}

TEST_P(JntSADavgTest, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(reference_data_, reference_stride_, mask_);
  FillConstant(second_pred_, width_, 0);
  CheckSAD();
}
TEST_P(JntSADavgTest, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(reference_data_, reference_stride_, 0);
  FillConstant(second_pred_, width_, 0);
  CheckSAD();
}

TEST_P(JntSADavgTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(JntSADavgTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(JntSADavgTest, ShortSrc) {
  const int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  int test_count = 2000;
  while (test_count > 0) {
    FillRandom(source_data_, source_stride_);
    FillRandom(reference_data_, reference_stride_);
    FillRandom(second_pred_, width_);
    CheckSAD();
    test_count -= 1;
  }
  source_stride_ = tmp_stride;
}

TEST_P(SADx4Test, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(GetReference(0), reference_stride_, mask_);
  FillConstant(GetReference(1), reference_stride_, mask_);
  FillConstant(GetReference(2), reference_stride_, mask_);
  FillConstant(GetReference(3), reference_stride_, mask_);
  CheckSADs();
}

TEST_P(SADx4Test, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(GetReference(0), reference_stride_, 0);
  FillConstant(GetReference(1), reference_stride_, 0);
  FillConstant(GetReference(2), reference_stride_, 0);
  FillConstant(GetReference(3), reference_stride_, 0);
  CheckSADs();
}

TEST_P(SADx4Test, ShortRef) {
  int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  reference_stride_ = tmp_stride;
}

TEST_P(SADx4Test, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  reference_stride_ = tmp_stride;
}

TEST_P(SADx4Test, ShortSrc) {
  int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  int test_count = 1000;
  while (test_count > 0) {
    FillRandom(source_data_, source_stride_);
    FillRandom(GetReference(0), reference_stride_);
    FillRandom(GetReference(1), reference_stride_);
    FillRandom(GetReference(2), reference_stride_);
    FillRandom(GetReference(3), reference_stride_);
    CheckSADs();
    test_count -= 1;
  }
  source_stride_ = tmp_stride;
}

TEST_P(SADx4Test, SrcAlignedByWidth) {
  uint8_t *tmp_source_data = source_data_;
  source_data_ += width_;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  source_data_ = tmp_source_data;
}

using ::testing::make_tuple;

//------------------------------------------------------------------------------
// C functions
const SadMxNParam c_tests[] = {
  make_tuple(128, 128, &aom_sad128x128_c, -1),
  make_tuple(128, 64, &aom_sad128x64_c, -1),
  make_tuple(64, 128, &aom_sad64x128_c, -1),
  make_tuple(64, 64, &aom_sad64x64_c, -1),
  make_tuple(64, 32, &aom_sad64x32_c, -1),
  make_tuple(32, 64, &aom_sad32x64_c, -1),
  make_tuple(32, 32, &aom_sad32x32_c, -1),
  make_tuple(32, 16, &aom_sad32x16_c, -1),
  make_tuple(16, 32, &aom_sad16x32_c, -1),
  make_tuple(16, 16, &aom_sad16x16_c, -1),
  make_tuple(16, 8, &aom_sad16x8_c, -1),
  make_tuple(8, 16, &aom_sad8x16_c, -1),
  make_tuple(8, 8, &aom_sad8x8_c, -1),
  make_tuple(8, 4, &aom_sad8x4_c, -1),
  make_tuple(4, 8, &aom_sad4x8_c, -1),
  make_tuple(4, 4, &aom_sad4x4_c, -1),
  make_tuple(128, 128, &aom_highbd_sad128x128_c, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64_c, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128_c, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64_c, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32_c, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64_c, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32_c, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16_c, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32_c, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16_c, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8_c, 8),
  make_tuple(8, 16, &aom_highbd_sad8x16_c, 8),
  make_tuple(8, 8, &aom_highbd_sad8x8_c, 8),
  make_tuple(8, 4, &aom_highbd_sad8x4_c, 8),
  make_tuple(4, 8, &aom_highbd_sad4x8_c, 8),
  make_tuple(4, 4, &aom_highbd_sad4x4_c, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128_c, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64_c, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128_c, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64_c, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32_c, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64_c, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32_c, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16_c, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32_c, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16_c, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8_c, 10),
  make_tuple(8, 16, &aom_highbd_sad8x16_c, 10),
  make_tuple(8, 8, &aom_highbd_sad8x8_c, 10),
  make_tuple(8, 4, &aom_highbd_sad8x4_c, 10),
  make_tuple(4, 8, &aom_highbd_sad4x8_c, 10),
  make_tuple(4, 4, &aom_highbd_sad4x4_c, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128_c, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64_c, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128_c, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64_c, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32_c, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64_c, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32_c, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16_c, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32_c, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16_c, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8_c, 12),
  make_tuple(8, 16, &aom_highbd_sad8x16_c, 12),
  make_tuple(8, 8, &aom_highbd_sad8x8_c, 12),
  make_tuple(8, 4, &aom_highbd_sad8x4_c, 12),
  make_tuple(4, 8, &aom_highbd_sad4x8_c, 12),
  make_tuple(4, 4, &aom_highbd_sad4x4_c, 12),
};
INSTANTIATE_TEST_CASE_P(C, SADTest, ::testing::ValuesIn(c_tests));

const SadMxNAvgParam avg_c_tests[] = {
  make_tuple(128, 128, &aom_sad128x128_avg_c, -1),
  make_tuple(128, 64, &aom_sad128x64_avg_c, -1),
  make_tuple(64, 128, &aom_sad64x128_avg_c, -1),
  make_tuple(64, 64, &aom_sad64x64_avg_c, -1),
  make_tuple(64, 32, &aom_sad64x32_avg_c, -1),
  make_tuple(32, 64, &aom_sad32x64_avg_c, -1),
  make_tuple(32, 32, &aom_sad32x32_avg_c, -1),
  make_tuple(32, 16, &aom_sad32x16_avg_c, -1),
  make_tuple(16, 32, &aom_sad16x32_avg_c, -1),
  make_tuple(16, 16, &aom_sad16x16_avg_c, -1),
  make_tuple(16, 8, &aom_sad16x8_avg_c, -1),
  make_tuple(8, 16, &aom_sad8x16_avg_c, -1),
  make_tuple(8, 8, &aom_sad8x8_avg_c, -1),
  make_tuple(8, 4, &aom_sad8x4_avg_c, -1),
  make_tuple(4, 8, &aom_sad4x8_avg_c, -1),
  make_tuple(4, 4, &aom_sad4x4_avg_c, -1),
  make_tuple(128, 128, &aom_highbd_sad128x128_avg_c, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64_avg_c, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128_avg_c, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_c, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_c, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_c, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_c, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_c, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_c, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_c, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_c, 8),
  make_tuple(8, 16, &aom_highbd_sad8x16_avg_c, 8),
  make_tuple(8, 8, &aom_highbd_sad8x8_avg_c, 8),
  make_tuple(8, 4, &aom_highbd_sad8x4_avg_c, 8),
  make_tuple(4, 8, &aom_highbd_sad4x8_avg_c, 8),
  make_tuple(4, 4, &aom_highbd_sad4x4_avg_c, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128_avg_c, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64_avg_c, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128_avg_c, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_c, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_c, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_c, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_c, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_c, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_c, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_c, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_c, 10),
  make_tuple(8, 16, &aom_highbd_sad8x16_avg_c, 10),
  make_tuple(8, 8, &aom_highbd_sad8x8_avg_c, 10),
  make_tuple(8, 4, &aom_highbd_sad8x4_avg_c, 10),
  make_tuple(4, 8, &aom_highbd_sad4x8_avg_c, 10),
  make_tuple(4, 4, &aom_highbd_sad4x4_avg_c, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128_avg_c, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64_avg_c, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128_avg_c, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_c, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_c, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_c, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_c, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_c, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_c, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_c, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_c, 12),
  make_tuple(8, 16, &aom_highbd_sad8x16_avg_c, 12),
  make_tuple(8, 8, &aom_highbd_sad8x8_avg_c, 12),
  make_tuple(8, 4, &aom_highbd_sad8x4_avg_c, 12),
  make_tuple(4, 8, &aom_highbd_sad4x8_avg_c, 12),
  make_tuple(4, 4, &aom_highbd_sad4x4_avg_c, 12),
};
INSTANTIATE_TEST_CASE_P(C, SADavgTest, ::testing::ValuesIn(avg_c_tests));

// TODO(chengchen): add highbd tests
const JntCompAvgParam jnt_comp_avg_c_tests[] = {
  make_tuple(128, 128, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(128, 64, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(64, 128, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(64, 64, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(64, 32, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(32, 64, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(32, 32, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(32, 16, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(16, 32, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(16, 16, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(16, 8, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(8, 16, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(8, 8, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(8, 4, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(4, 8, &aom_jnt_comp_avg_pred_c, -1),
  make_tuple(4, 4, &aom_jnt_comp_avg_pred_c, -1),
};

INSTANTIATE_TEST_CASE_P(C, JntCompAvgTest,
                        ::testing::ValuesIn(jnt_comp_avg_c_tests));

const JntSadMxNAvgParam jnt_avg_c_tests[] = {
  make_tuple(128, 128, &aom_jnt_sad128x128_avg_c, -1),
  make_tuple(128, 64, &aom_jnt_sad128x64_avg_c, -1),
  make_tuple(64, 128, &aom_jnt_sad64x128_avg_c, -1),
  make_tuple(64, 64, &aom_jnt_sad64x64_avg_c, -1),
  make_tuple(64, 32, &aom_jnt_sad64x32_avg_c, -1),
  make_tuple(32, 64, &aom_jnt_sad32x64_avg_c, -1),
  make_tuple(32, 32, &aom_jnt_sad32x32_avg_c, -1),
  make_tuple(32, 16, &aom_jnt_sad32x16_avg_c, -1),
  make_tuple(16, 32, &aom_jnt_sad16x32_avg_c, -1),
  make_tuple(16, 16, &aom_jnt_sad16x16_avg_c, -1),
  make_tuple(16, 8, &aom_jnt_sad16x8_avg_c, -1),
  make_tuple(8, 16, &aom_jnt_sad8x16_avg_c, -1),
  make_tuple(8, 8, &aom_jnt_sad8x8_avg_c, -1),
  make_tuple(8, 4, &aom_jnt_sad8x4_avg_c, -1),
  make_tuple(4, 8, &aom_jnt_sad4x8_avg_c, -1),
  make_tuple(4, 4, &aom_jnt_sad4x4_avg_c, -1),
};
INSTANTIATE_TEST_CASE_P(C, JntSADavgTest, ::testing::ValuesIn(jnt_avg_c_tests));

const SadMxNx4Param x4d_c_tests[] = {
  make_tuple(128, 128, &aom_sad128x128x4d_c, -1),
  make_tuple(128, 64, &aom_sad128x64x4d_c, -1),
  make_tuple(64, 128, &aom_sad64x128x4d_c, -1),
  make_tuple(64, 64, &aom_sad64x64x4d_c, -1),
  make_tuple(64, 32, &aom_sad64x32x4d_c, -1),
  make_tuple(32, 64, &aom_sad32x64x4d_c, -1),
  make_tuple(32, 32, &aom_sad32x32x4d_c, -1),
  make_tuple(32, 16, &aom_sad32x16x4d_c, -1),
  make_tuple(16, 32, &aom_sad16x32x4d_c, -1),
  make_tuple(16, 16, &aom_sad16x16x4d_c, -1),
  make_tuple(16, 8, &aom_sad16x8x4d_c, -1),
  make_tuple(8, 16, &aom_sad8x16x4d_c, -1),
  make_tuple(8, 8, &aom_sad8x8x4d_c, -1),
  make_tuple(8, 4, &aom_sad8x4x4d_c, -1),
  make_tuple(4, 8, &aom_sad4x8x4d_c, -1),
  make_tuple(4, 4, &aom_sad4x4x4d_c, -1),
  make_tuple(128, 128, &aom_highbd_sad128x128x4d_c, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64x4d_c, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128x4d_c, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_c, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_c, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_c, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_c, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_c, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_c, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_c, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_c, 8),
  make_tuple(8, 16, &aom_highbd_sad8x16x4d_c, 8),
  make_tuple(8, 8, &aom_highbd_sad8x8x4d_c, 8),
  make_tuple(8, 4, &aom_highbd_sad8x4x4d_c, 8),
  make_tuple(4, 8, &aom_highbd_sad4x8x4d_c, 8),
  make_tuple(4, 4, &aom_highbd_sad4x4x4d_c, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128x4d_c, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64x4d_c, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128x4d_c, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_c, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_c, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_c, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_c, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_c, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_c, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_c, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_c, 10),
  make_tuple(8, 16, &aom_highbd_sad8x16x4d_c, 10),
  make_tuple(8, 8, &aom_highbd_sad8x8x4d_c, 10),
  make_tuple(8, 4, &aom_highbd_sad8x4x4d_c, 10),
  make_tuple(4, 8, &aom_highbd_sad4x8x4d_c, 10),
  make_tuple(4, 4, &aom_highbd_sad4x4x4d_c, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128x4d_c, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64x4d_c, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128x4d_c, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_c, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_c, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_c, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_c, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_c, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_c, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_c, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_c, 12),
  make_tuple(8, 16, &aom_highbd_sad8x16x4d_c, 12),
  make_tuple(8, 8, &aom_highbd_sad8x8x4d_c, 12),
  make_tuple(8, 4, &aom_highbd_sad8x4x4d_c, 12),
  make_tuple(4, 8, &aom_highbd_sad4x8x4d_c, 12),
  make_tuple(4, 4, &aom_highbd_sad4x4x4d_c, 12),
};
INSTANTIATE_TEST_CASE_P(C, SADx4Test, ::testing::ValuesIn(x4d_c_tests));

//------------------------------------------------------------------------------
// ARM functions
#if HAVE_NEON
const SadMxNParam neon_tests[] = {
  make_tuple(64, 64, &aom_sad64x64_neon, -1),
  make_tuple(32, 32, &aom_sad32x32_neon, -1),
  make_tuple(16, 16, &aom_sad16x16_neon, -1),
  make_tuple(16, 8, &aom_sad16x8_neon, -1),
  make_tuple(8, 16, &aom_sad8x16_neon, -1),
  make_tuple(8, 8, &aom_sad8x8_neon, -1),
  make_tuple(4, 4, &aom_sad4x4_neon, -1),
};
INSTANTIATE_TEST_CASE_P(NEON, SADTest, ::testing::ValuesIn(neon_tests));

const SadMxNx4Param x4d_neon_tests[] = {
  make_tuple(64, 64, &aom_sad64x64x4d_neon, -1),
  make_tuple(32, 32, &aom_sad32x32x4d_neon, -1),
  make_tuple(16, 16, &aom_sad16x16x4d_neon, -1),
};
INSTANTIATE_TEST_CASE_P(NEON, SADx4Test, ::testing::ValuesIn(x4d_neon_tests));
#endif  // HAVE_NEON

//------------------------------------------------------------------------------
// x86 functions
#if HAVE_SSE2
const SadMxNParam sse2_tests[] = {
  make_tuple(128, 128, &aom_sad128x128_sse2, -1),
  make_tuple(128, 64, &aom_sad128x64_sse2, -1),
  make_tuple(64, 128, &aom_sad64x128_sse2, -1),
  make_tuple(64, 64, &aom_sad64x64_sse2, -1),
  make_tuple(64, 32, &aom_sad64x32_sse2, -1),
  make_tuple(32, 64, &aom_sad32x64_sse2, -1),
  make_tuple(32, 32, &aom_sad32x32_sse2, -1),
  make_tuple(32, 16, &aom_sad32x16_sse2, -1),
  make_tuple(16, 32, &aom_sad16x32_sse2, -1),
  make_tuple(16, 16, &aom_sad16x16_sse2, -1),
  make_tuple(16, 8, &aom_sad16x8_sse2, -1),
  make_tuple(8, 16, &aom_sad8x16_sse2, -1),
  make_tuple(8, 8, &aom_sad8x8_sse2, -1),
  make_tuple(8, 4, &aom_sad8x4_sse2, -1),
  make_tuple(4, 8, &aom_sad4x8_sse2, -1),
  make_tuple(4, 4, &aom_sad4x4_sse2, -1),
  make_tuple(64, 64, &aom_highbd_sad64x64_sse2, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32_sse2, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64_sse2, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32_sse2, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16_sse2, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32_sse2, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16_sse2, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8_sse2, 8),
  make_tuple(8, 16, &aom_highbd_sad8x16_sse2, 8),
  make_tuple(8, 8, &aom_highbd_sad8x8_sse2, 8),
  make_tuple(8, 4, &aom_highbd_sad8x4_sse2, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64_sse2, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32_sse2, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64_sse2, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32_sse2, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16_sse2, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32_sse2, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16_sse2, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8_sse2, 10),
  make_tuple(8, 16, &aom_highbd_sad8x16_sse2, 10),
  make_tuple(8, 8, &aom_highbd_sad8x8_sse2, 10),
  make_tuple(8, 4, &aom_highbd_sad8x4_sse2, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64_sse2, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32_sse2, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64_sse2, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32_sse2, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16_sse2, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32_sse2, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16_sse2, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8_sse2, 12),
  make_tuple(8, 16, &aom_highbd_sad8x16_sse2, 12),
  make_tuple(8, 8, &aom_highbd_sad8x8_sse2, 12),
  make_tuple(8, 4, &aom_highbd_sad8x4_sse2, 12),
};
INSTANTIATE_TEST_CASE_P(SSE2, SADTest, ::testing::ValuesIn(sse2_tests));

const SadMxNAvgParam avg_sse2_tests[] = {
  make_tuple(128, 128, &aom_sad128x128_avg_sse2, -1),
  make_tuple(128, 64, &aom_sad128x64_avg_sse2, -1),
  make_tuple(64, 128, &aom_sad64x128_avg_sse2, -1),
  make_tuple(64, 64, &aom_sad64x64_avg_sse2, -1),
  make_tuple(64, 32, &aom_sad64x32_avg_sse2, -1),
  make_tuple(32, 64, &aom_sad32x64_avg_sse2, -1),
  make_tuple(32, 32, &aom_sad32x32_avg_sse2, -1),
  make_tuple(32, 16, &aom_sad32x16_avg_sse2, -1),
  make_tuple(16, 32, &aom_sad16x32_avg_sse2, -1),
  make_tuple(16, 16, &aom_sad16x16_avg_sse2, -1),
  make_tuple(16, 8, &aom_sad16x8_avg_sse2, -1),
  make_tuple(8, 16, &aom_sad8x16_avg_sse2, -1),
  make_tuple(8, 8, &aom_sad8x8_avg_sse2, -1),
  make_tuple(8, 4, &aom_sad8x4_avg_sse2, -1),
  make_tuple(4, 8, &aom_sad4x8_avg_sse2, -1),
  make_tuple(4, 4, &aom_sad4x4_avg_sse2, -1),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_sse2, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_sse2, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_sse2, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_sse2, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_sse2, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_sse2, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_sse2, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_sse2, 8),
  make_tuple(8, 16, &aom_highbd_sad8x16_avg_sse2, 8),
  make_tuple(8, 8, &aom_highbd_sad8x8_avg_sse2, 8),
  make_tuple(8, 4, &aom_highbd_sad8x4_avg_sse2, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_sse2, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_sse2, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_sse2, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_sse2, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_sse2, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_sse2, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_sse2, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_sse2, 10),
  make_tuple(8, 16, &aom_highbd_sad8x16_avg_sse2, 10),
  make_tuple(8, 8, &aom_highbd_sad8x8_avg_sse2, 10),
  make_tuple(8, 4, &aom_highbd_sad8x4_avg_sse2, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_sse2, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_sse2, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_sse2, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_sse2, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_sse2, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_sse2, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_sse2, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_sse2, 12),
  make_tuple(8, 16, &aom_highbd_sad8x16_avg_sse2, 12),
  make_tuple(8, 8, &aom_highbd_sad8x8_avg_sse2, 12),
  make_tuple(8, 4, &aom_highbd_sad8x4_avg_sse2, 12),
};
INSTANTIATE_TEST_CASE_P(SSE2, SADavgTest, ::testing::ValuesIn(avg_sse2_tests));

const SadMxNx4Param x4d_sse2_tests[] = {
  make_tuple(128, 128, &aom_sad128x128x4d_sse2, -1),
  make_tuple(128, 64, &aom_sad128x64x4d_sse2, -1),
  make_tuple(64, 128, &aom_sad64x128x4d_sse2, -1),
  make_tuple(64, 64, &aom_sad64x64x4d_sse2, -1),
  make_tuple(64, 32, &aom_sad64x32x4d_sse2, -1),
  make_tuple(32, 64, &aom_sad32x64x4d_sse2, -1),
  make_tuple(32, 32, &aom_sad32x32x4d_sse2, -1),
  make_tuple(32, 16, &aom_sad32x16x4d_sse2, -1),
  make_tuple(16, 32, &aom_sad16x32x4d_sse2, -1),
  make_tuple(16, 16, &aom_sad16x16x4d_sse2, -1),
  make_tuple(16, 8, &aom_sad16x8x4d_sse2, -1),
  make_tuple(8, 16, &aom_sad8x16x4d_sse2, -1),
  make_tuple(8, 8, &aom_sad8x8x4d_sse2, -1),
  make_tuple(8, 4, &aom_sad8x4x4d_sse2, -1),
  make_tuple(4, 8, &aom_sad4x8x4d_sse2, -1),
  make_tuple(4, 4, &aom_sad4x4x4d_sse2, -1),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_sse2, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_sse2, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_sse2, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_sse2, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_sse2, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_sse2, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_sse2, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_sse2, 8),
  make_tuple(8, 16, &aom_highbd_sad8x16x4d_sse2, 8),
  make_tuple(8, 8, &aom_highbd_sad8x8x4d_sse2, 8),
  make_tuple(8, 4, &aom_highbd_sad8x4x4d_sse2, 8),
  make_tuple(4, 8, &aom_highbd_sad4x8x4d_sse2, 8),
  make_tuple(4, 4, &aom_highbd_sad4x4x4d_sse2, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_sse2, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_sse2, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_sse2, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_sse2, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_sse2, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_sse2, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_sse2, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_sse2, 10),
  make_tuple(8, 16, &aom_highbd_sad8x16x4d_sse2, 10),
  make_tuple(8, 8, &aom_highbd_sad8x8x4d_sse2, 10),
  make_tuple(8, 4, &aom_highbd_sad8x4x4d_sse2, 10),
  make_tuple(4, 8, &aom_highbd_sad4x8x4d_sse2, 10),
  make_tuple(4, 4, &aom_highbd_sad4x4x4d_sse2, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_sse2, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_sse2, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_sse2, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_sse2, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_sse2, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_sse2, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_sse2, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_sse2, 12),
  make_tuple(8, 16, &aom_highbd_sad8x16x4d_sse2, 12),
  make_tuple(8, 8, &aom_highbd_sad8x8x4d_sse2, 12),
  make_tuple(8, 4, &aom_highbd_sad8x4x4d_sse2, 12),
  make_tuple(4, 8, &aom_highbd_sad4x8x4d_sse2, 12),
  make_tuple(4, 4, &aom_highbd_sad4x4x4d_sse2, 12),
};
INSTANTIATE_TEST_CASE_P(SSE2, SADx4Test, ::testing::ValuesIn(x4d_sse2_tests));
#endif  // HAVE_SSE2

#if HAVE_SSSE3
// Note: These are named sse2, but part of ssse3 file and only built and linked
// when ssse3 is enabled.
const JntSadMxhParam jnt_sad_sse2_tests[] = {
  make_tuple(4, 4, &aom_sad4xh_sse2, -1),
  make_tuple(4, 8, &aom_sad4xh_sse2, -1),
  make_tuple(8, 4, &aom_sad8xh_sse2, -1),
  make_tuple(8, 8, &aom_sad8xh_sse2, -1),
  make_tuple(8, 16, &aom_sad8xh_sse2, -1),
  make_tuple(16, 8, &aom_sad16xh_sse2, -1),
  make_tuple(16, 16, &aom_sad16xh_sse2, -1),
  make_tuple(16, 32, &aom_sad16xh_sse2, -1),
  make_tuple(32, 16, &aom_sad32xh_sse2, -1),
  make_tuple(32, 32, &aom_sad32xh_sse2, -1),
  make_tuple(32, 64, &aom_sad32xh_sse2, -1),
  make_tuple(64, 32, &aom_sad64xh_sse2, -1),
  make_tuple(64, 64, &aom_sad64xh_sse2, -1),
  make_tuple(128, 128, &aom_sad128xh_sse2, -1),
  make_tuple(128, 64, &aom_sad128xh_sse2, -1),
  make_tuple(64, 128, &aom_sad64xh_sse2, -1),
  make_tuple(4, 16, &aom_sad4xh_sse2, -1),
  make_tuple(16, 4, &aom_sad16xh_sse2, -1),
  make_tuple(8, 32, &aom_sad8xh_sse2, -1),
  make_tuple(32, 8, &aom_sad32xh_sse2, -1),
  make_tuple(16, 64, &aom_sad16xh_sse2, -1),
  make_tuple(64, 16, &aom_sad64xh_sse2, -1),
};
INSTANTIATE_TEST_CASE_P(SSE2, JntSADTest,
                        ::testing::ValuesIn(jnt_sad_sse2_tests));

#endif  // HAVE_SSSE3

#if HAVE_SSE3
// Only functions are x3, which do not have tests.
#endif  // HAVE_SSE3

#if HAVE_SSSE3
const JntCompAvgParam jnt_comp_avg_ssse3_tests[] = {
  make_tuple(128, 128, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(128, 64, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(64, 128, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(64, 64, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(64, 32, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(32, 64, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(32, 32, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(32, 16, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(16, 32, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(16, 16, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(16, 8, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(8, 16, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(8, 8, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(8, 4, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(4, 8, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(4, 4, &aom_jnt_comp_avg_pred_ssse3, -1),
  make_tuple(16, 16, &aom_jnt_comp_avg_pred_ssse3, -1),
};

INSTANTIATE_TEST_CASE_P(SSSE3, JntCompAvgTest,
                        ::testing::ValuesIn(jnt_comp_avg_ssse3_tests));

const JntSadMxNAvgParam jnt_avg_ssse3_tests[] = {
  make_tuple(128, 128, &aom_jnt_sad128x128_avg_ssse3, -1),
  make_tuple(128, 64, &aom_jnt_sad128x64_avg_ssse3, -1),
  make_tuple(64, 128, &aom_jnt_sad64x128_avg_ssse3, -1),
  make_tuple(64, 64, &aom_jnt_sad64x64_avg_ssse3, -1),
  make_tuple(64, 32, &aom_jnt_sad64x32_avg_ssse3, -1),
  make_tuple(32, 64, &aom_jnt_sad32x64_avg_ssse3, -1),
  make_tuple(32, 32, &aom_jnt_sad32x32_avg_ssse3, -1),
  make_tuple(32, 16, &aom_jnt_sad32x16_avg_ssse3, -1),
  make_tuple(16, 32, &aom_jnt_sad16x32_avg_ssse3, -1),
  make_tuple(16, 16, &aom_jnt_sad16x16_avg_ssse3, -1),
  make_tuple(16, 8, &aom_jnt_sad16x8_avg_ssse3, -1),
  make_tuple(8, 16, &aom_jnt_sad8x16_avg_ssse3, -1),
  make_tuple(8, 8, &aom_jnt_sad8x8_avg_ssse3, -1),
  make_tuple(8, 4, &aom_jnt_sad8x4_avg_ssse3, -1),
  make_tuple(4, 8, &aom_jnt_sad4x8_avg_ssse3, -1),
  make_tuple(4, 4, &aom_jnt_sad4x4_avg_ssse3, -1),
};
INSTANTIATE_TEST_CASE_P(SSSE3, JntSADavgTest,
                        ::testing::ValuesIn(jnt_avg_ssse3_tests));
#endif  // HAVE_SSSE3

#if HAVE_SSE4_1
// Only functions are x8, which do not have tests.
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
const SadMxNParam avx2_tests[] = {
  make_tuple(64, 128, &aom_sad64x128_avx2, -1),
  make_tuple(128, 64, &aom_sad128x64_avx2, -1),
  make_tuple(128, 128, &aom_sad128x128_avx2, -1),
  make_tuple(64, 64, &aom_sad64x64_avx2, -1),
  make_tuple(64, 32, &aom_sad64x32_avx2, -1),
  make_tuple(32, 64, &aom_sad32x64_avx2, -1),
  make_tuple(32, 32, &aom_sad32x32_avx2, -1),
  make_tuple(32, 16, &aom_sad32x16_avx2, -1),
  make_tuple(128, 128, &aom_highbd_sad128x128_avx2, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128_avx2, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128_avx2, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64_avx2, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64_avx2, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64_avx2, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128_avx2, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128_avx2, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128_avx2, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64_avx2, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64_avx2, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64_avx2, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32_avx2, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32_avx2, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32_avx2, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64_avx2, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64_avx2, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64_avx2, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32_avx2, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32_avx2, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32_avx2, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16_avx2, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16_avx2, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16_avx2, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32_avx2, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32_avx2, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32_avx2, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16_avx2, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16_avx2, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16_avx2, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8_avx2, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8_avx2, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8_avx2, 12),
};
INSTANTIATE_TEST_CASE_P(AVX2, SADTest, ::testing::ValuesIn(avx2_tests));

const SadMxNAvgParam avg_avx2_tests[] = {
  make_tuple(64, 128, &aom_sad64x128_avg_avx2, -1),
  make_tuple(128, 64, &aom_sad128x64_avg_avx2, -1),
  make_tuple(128, 128, &aom_sad128x128_avg_avx2, -1),
  make_tuple(64, 64, &aom_sad64x64_avg_avx2, -1),
  make_tuple(64, 32, &aom_sad64x32_avg_avx2, -1),
  make_tuple(32, 64, &aom_sad32x64_avg_avx2, -1),
  make_tuple(32, 32, &aom_sad32x32_avg_avx2, -1),
  make_tuple(32, 16, &aom_sad32x16_avg_avx2, -1),
  make_tuple(128, 128, &aom_highbd_sad128x128_avg_avx2, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128_avg_avx2, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128_avg_avx2, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64_avg_avx2, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64_avg_avx2, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64_avg_avx2, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128_avg_avx2, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128_avg_avx2, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128_avg_avx2, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_avx2, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_avx2, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64_avg_avx2, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_avx2, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_avx2, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32_avg_avx2, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_avx2, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_avx2, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64_avg_avx2, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_avx2, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_avx2, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32_avg_avx2, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_avx2, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_avx2, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16_avg_avx2, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_avx2, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_avx2, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32_avg_avx2, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_avx2, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_avx2, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16_avg_avx2, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_avx2, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_avx2, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8_avg_avx2, 12),
};
INSTANTIATE_TEST_CASE_P(AVX2, SADavgTest, ::testing::ValuesIn(avg_avx2_tests));

const SadMxNx4Param x4d_avx2_tests[] = {
  make_tuple(64, 128, &aom_sad64x128x4d_avx2, -1),
  make_tuple(128, 64, &aom_sad128x64x4d_avx2, -1),
  make_tuple(128, 128, &aom_sad128x128x4d_avx2, -1),
  make_tuple(64, 64, &aom_sad64x64x4d_avx2, -1),
  make_tuple(32, 64, &aom_sad32x64x4d_avx2, -1),
  make_tuple(64, 32, &aom_sad64x32x4d_avx2, -1),
  make_tuple(32, 32, &aom_sad32x32x4d_avx2, -1),
  make_tuple(128, 128, &aom_highbd_sad128x128x4d_avx2, 8),
  make_tuple(128, 128, &aom_highbd_sad128x128x4d_avx2, 10),
  make_tuple(128, 128, &aom_highbd_sad128x128x4d_avx2, 12),
  make_tuple(128, 64, &aom_highbd_sad128x64x4d_avx2, 8),
  make_tuple(128, 64, &aom_highbd_sad128x64x4d_avx2, 10),
  make_tuple(128, 64, &aom_highbd_sad128x64x4d_avx2, 12),
  make_tuple(64, 128, &aom_highbd_sad64x128x4d_avx2, 8),
  make_tuple(64, 128, &aom_highbd_sad64x128x4d_avx2, 10),
  make_tuple(64, 128, &aom_highbd_sad64x128x4d_avx2, 12),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_avx2, 8),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_avx2, 10),
  make_tuple(64, 64, &aom_highbd_sad64x64x4d_avx2, 12),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_avx2, 8),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_avx2, 10),
  make_tuple(64, 32, &aom_highbd_sad64x32x4d_avx2, 12),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_avx2, 8),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_avx2, 10),
  make_tuple(32, 64, &aom_highbd_sad32x64x4d_avx2, 12),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_avx2, 8),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_avx2, 10),
  make_tuple(32, 32, &aom_highbd_sad32x32x4d_avx2, 12),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_avx2, 8),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_avx2, 10),
  make_tuple(32, 16, &aom_highbd_sad32x16x4d_avx2, 12),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_avx2, 8),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_avx2, 10),
  make_tuple(16, 32, &aom_highbd_sad16x32x4d_avx2, 12),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_avx2, 8),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_avx2, 10),
  make_tuple(16, 16, &aom_highbd_sad16x16x4d_avx2, 12),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_avx2, 8),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_avx2, 10),
  make_tuple(16, 8, &aom_highbd_sad16x8x4d_avx2, 12),
};
INSTANTIATE_TEST_CASE_P(AVX2, SADx4Test, ::testing::ValuesIn(x4d_avx2_tests));
#endif  // HAVE_AVX2

//------------------------------------------------------------------------------
// MIPS functions
#if HAVE_MSA
const SadMxNParam msa_tests[] = {
  make_tuple(64, 64, &aom_sad64x64_msa, -1),
  make_tuple(64, 32, &aom_sad64x32_msa, -1),
  make_tuple(32, 64, &aom_sad32x64_msa, -1),
  make_tuple(32, 32, &aom_sad32x32_msa, -1),
  make_tuple(32, 16, &aom_sad32x16_msa, -1),
  make_tuple(16, 32, &aom_sad16x32_msa, -1),
  make_tuple(16, 16, &aom_sad16x16_msa, -1),
  make_tuple(16, 8, &aom_sad16x8_msa, -1),
  make_tuple(8, 16, &aom_sad8x16_msa, -1),
  make_tuple(8, 8, &aom_sad8x8_msa, -1),
  make_tuple(8, 4, &aom_sad8x4_msa, -1),
  make_tuple(4, 8, &aom_sad4x8_msa, -1),
  make_tuple(4, 4, &aom_sad4x4_msa, -1),
};
INSTANTIATE_TEST_CASE_P(MSA, SADTest, ::testing::ValuesIn(msa_tests));

const SadMxNAvgParam avg_msa_tests[] = {
  make_tuple(64, 64, &aom_sad64x64_avg_msa, -1),
  make_tuple(64, 32, &aom_sad64x32_avg_msa, -1),
  make_tuple(32, 64, &aom_sad32x64_avg_msa, -1),
  make_tuple(32, 32, &aom_sad32x32_avg_msa, -1),
  make_tuple(32, 16, &aom_sad32x16_avg_msa, -1),
  make_tuple(16, 32, &aom_sad16x32_avg_msa, -1),
  make_tuple(16, 16, &aom_sad16x16_avg_msa, -1),
  make_tuple(16, 8, &aom_sad16x8_avg_msa, -1),
  make_tuple(8, 16, &aom_sad8x16_avg_msa, -1),
  make_tuple(8, 8, &aom_sad8x8_avg_msa, -1),
  make_tuple(8, 4, &aom_sad8x4_avg_msa, -1),
  make_tuple(4, 8, &aom_sad4x8_avg_msa, -1),
  make_tuple(4, 4, &aom_sad4x4_avg_msa, -1),
};
INSTANTIATE_TEST_CASE_P(MSA, SADavgTest, ::testing::ValuesIn(avg_msa_tests));

const SadMxNx4Param x4d_msa_tests[] = {
  make_tuple(64, 64, &aom_sad64x64x4d_msa, -1),
  make_tuple(64, 32, &aom_sad64x32x4d_msa, -1),
  make_tuple(32, 64, &aom_sad32x64x4d_msa, -1),
  make_tuple(32, 32, &aom_sad32x32x4d_msa, -1),
  make_tuple(32, 16, &aom_sad32x16x4d_msa, -1),
  make_tuple(16, 32, &aom_sad16x32x4d_msa, -1),
  make_tuple(16, 16, &aom_sad16x16x4d_msa, -1),
  make_tuple(16, 8, &aom_sad16x8x4d_msa, -1),
  make_tuple(8, 16, &aom_sad8x16x4d_msa, -1),
  make_tuple(8, 8, &aom_sad8x8x4d_msa, -1),
  make_tuple(8, 4, &aom_sad8x4x4d_msa, -1),
  make_tuple(4, 8, &aom_sad4x8x4d_msa, -1),
  make_tuple(4, 4, &aom_sad4x4x4d_msa, -1),
};
INSTANTIATE_TEST_CASE_P(MSA, SADx4Test, ::testing::ValuesIn(x4d_msa_tests));
#endif  // HAVE_MSA

}  // namespace
