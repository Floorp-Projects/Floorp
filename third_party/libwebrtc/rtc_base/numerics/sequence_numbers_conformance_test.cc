/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>
#include <limits>
#include <type_traits>

#include "modules/include/module_common_types_public.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/strong_alias.h"
#include "rtc_base/time_utils.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Test;

using dcsctp::UnwrappedSequenceNumber;
using Wrapped = webrtc::StrongAlias<class WrappedTag, uint32_t>;
using TestSequence = UnwrappedSequenceNumber<Wrapped>;

template <typename T>
class UnwrapperHelper;

template <>
class UnwrapperHelper<TestSequence::Unwrapper> {
 public:
  int64_t Unwrap(uint32_t val) {
    TestSequence s = unwrapper_.Unwrap(Wrapped(val));
    // UnwrappedSequenceNumber starts counting at 2^32.
    constexpr int64_t kDcsctpUnwrapStart = int64_t{1} << 32;
    return s.value() - kDcsctpUnwrapStart;
  }

 private:
  TestSequence::Unwrapper unwrapper_;
};

// MaxVal is the max of the wrapped space, ie MaxVal + 1 = 0 when wrapped.
template <typename U, int64_t MaxVal = std::numeric_limits<uint32_t>::max()>
struct FixtureParams {
  using Unwrapper = U;
  static constexpr int64_t kMaxVal = MaxVal;
};

template <typename F>
class UnwrapperConformanceFixture : public Test {
 public:
  static constexpr int64_t kMaxVal = F::kMaxVal;
  static constexpr int64_t kMaxIncrease = kMaxVal / 2;
  static constexpr int64_t kMaxBackwardsIncrease = kMaxVal - kMaxIncrease + 1;

  template <typename U>
  static constexpr bool UnwrapperIs() {
    return std::is_same<typename F::Unwrapper, U>();
  }

  typename F::Unwrapper ref_unwrapper_;
};

TYPED_TEST_SUITE_P(UnwrapperConformanceFixture);

TYPED_TEST_P(UnwrapperConformanceFixture, PositiveWrapAround) {
  EXPECT_EQ(0, this->ref_unwrapper_.Unwrap(0));
  EXPECT_EQ(TestFixture::kMaxIncrease,
            this->ref_unwrapper_.Unwrap(TestFixture::kMaxIncrease));
  EXPECT_EQ(2 * TestFixture::kMaxIncrease,
            this->ref_unwrapper_.Unwrap(2 * TestFixture::kMaxIncrease));
  // Now unwrapping 0 should wrap around to be kMaxVal + 1.
  EXPECT_EQ(TestFixture::kMaxVal + 1, this->ref_unwrapper_.Unwrap(0));
  EXPECT_EQ(TestFixture::kMaxVal + 1 + TestFixture::kMaxIncrease,
            this->ref_unwrapper_.Unwrap(TestFixture::kMaxIncrease));
}

TYPED_TEST_P(UnwrapperConformanceFixture, NegativeUnwrap) {
  using UnwrapperT = decltype(this->ref_unwrapper_);
  // webrtc::TimestampUnwrapper known to not handle negative numbers.
  // rtc::TimestampWrapAroundHandler does not wrap around correctly.
  if constexpr (std::is_same<UnwrapperT, webrtc::TimestampUnwrapper>() ||
                std::is_same<UnwrapperT, rtc::TimestampWrapAroundHandler>()) {
    return;
  }
  EXPECT_EQ(0, this->ref_unwrapper_.Unwrap(0));
  // Max backwards wrap is negative.
  EXPECT_EQ(-TestFixture::kMaxIncrease,
            this->ref_unwrapper_.Unwrap(this->kMaxBackwardsIncrease));
  // Increase to a larger negative number.
  EXPECT_EQ(-2, this->ref_unwrapper_.Unwrap(TestFixture::kMaxVal - 1));
  // Increase back positive.
  EXPECT_EQ(1, this->ref_unwrapper_.Unwrap(1));
}

TYPED_TEST_P(UnwrapperConformanceFixture, BackwardUnwrap) {
  EXPECT_EQ(127, this->ref_unwrapper_.Unwrap(127));
  EXPECT_EQ(128, this->ref_unwrapper_.Unwrap(128));
  EXPECT_EQ(127, this->ref_unwrapper_.Unwrap(127));
}

TYPED_TEST_P(UnwrapperConformanceFixture, MultiplePositiveWrapArounds) {
  using UnwrapperT = decltype(this->ref_unwrapper_);
  // rtc::TimestampWrapAroundHandler does not wrap around correctly.
  if constexpr (std::is_same<UnwrapperT, rtc::TimestampWrapAroundHandler>()) {
    return;
  }
  int64_t val = 0;
  uint32_t wrapped_val = 0;
  for (int i = 0; i < 16; ++i) {
    EXPECT_EQ(val, this->ref_unwrapper_.Unwrap(wrapped_val));
    val += TestFixture::kMaxIncrease;
    wrapped_val =
        (wrapped_val + TestFixture::kMaxIncrease) % (TestFixture::kMaxVal + 1);
  }
}

TYPED_TEST_P(UnwrapperConformanceFixture, WrapBoundaries) {
  EXPECT_EQ(0, this->ref_unwrapper_.Unwrap(0));
  EXPECT_EQ(TestFixture::kMaxIncrease,
            this->ref_unwrapper_.Unwrap(TestFixture::kMaxIncrease));
  // Increases by more than TestFixture::kMaxIncrease which indicates a negative
  // rollback.
  EXPECT_EQ(0, this->ref_unwrapper_.Unwrap(0));
  EXPECT_EQ(10, this->ref_unwrapper_.Unwrap(10));
}

TYPED_TEST_P(UnwrapperConformanceFixture, MultipleNegativeWrapArounds) {
  using UnwrapperT = decltype(this->ref_unwrapper_);
  // webrtc::TimestampUnwrapper known to not handle negative numbers.
  // webrtc::SequenceNumberUnwrapper can only wrap negative once.
  // rtc::TimestampWrapAroundHandler does not wrap around correctly.
  if constexpr (std::is_same<UnwrapperT, webrtc::TimestampUnwrapper>() ||
                std::is_same<UnwrapperT,
                             UnwrapperHelper<TestSequence::Unwrapper>>() ||
                std::is_same<UnwrapperT, rtc::TimestampWrapAroundHandler>()) {
    return;
  }
  int64_t val = 0;
  uint32_t wrapped_val = 0;
  for (int i = 0; i < 16; ++i) {
    EXPECT_EQ(val, this->ref_unwrapper_.Unwrap(wrapped_val));
    val -= TestFixture::kMaxIncrease;
    wrapped_val = (wrapped_val + this->kMaxBackwardsIncrease) %
                  (TestFixture::kMaxVal + 1);
  }
}

REGISTER_TYPED_TEST_SUITE_P(UnwrapperConformanceFixture,
                            NegativeUnwrap,
                            PositiveWrapAround,
                            BackwardUnwrap,
                            WrapBoundaries,
                            MultiplePositiveWrapArounds,
                            MultipleNegativeWrapArounds);

constexpr int64_t k15BitMax = (int64_t{1} << 15) - 1;
using UnwrapperTypes = ::testing::Types<
    FixtureParams<rtc::TimestampWrapAroundHandler>,
    FixtureParams<webrtc::TimestampUnwrapper>,
    FixtureParams<webrtc::SeqNumUnwrapper<uint32_t>>,
    FixtureParams<UnwrapperHelper<TestSequence::Unwrapper>>,
    // SeqNumUnwrapper supports arbitrary limits.
    FixtureParams<webrtc::SeqNumUnwrapper<uint32_t, k15BitMax + 1>, k15BitMax>>;

class TestNames {
 public:
  template <typename T>
  static std::string GetName(int) {
    if constexpr (std::is_same<typename T::Unwrapper,
                               rtc::TimestampWrapAroundHandler>())
      return "TimestampWrapAroundHandler";
    if constexpr (std::is_same<typename T::Unwrapper,
                               webrtc::TimestampUnwrapper>())
      return "TimestampUnwrapper";
    if constexpr (std::is_same<typename T::Unwrapper,
                               webrtc::SeqNumUnwrapper<uint32_t>>())
      return "SeqNumUnwrapper";
    if constexpr (std::is_same<
                      typename T::Unwrapper,
                      webrtc::SeqNumUnwrapper<uint32_t, k15BitMax + 1>>())
      return "SeqNumUnwrapper15bit";
    if constexpr (std::is_same<typename T::Unwrapper,
                               UnwrapperHelper<TestSequence::Unwrapper>>())
      return "UnwrappedSequenceNumber";
  }
};

INSTANTIATE_TYPED_TEST_SUITE_P(UnwrapperConformanceTest,
                               UnwrapperConformanceFixture,
                               UnwrapperTypes,
                               TestNames);

}  // namespace
}  // namespace webrtc
