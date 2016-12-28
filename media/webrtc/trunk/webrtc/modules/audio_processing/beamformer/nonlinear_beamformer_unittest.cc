/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "webrtc/modules/audio_processing/beamformer/nonlinear_beamformer.h"

#include <math.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc {
namespace {

const int kChunkSizeMs = 10;
const int kSampleRateHz = 16000;

SphericalPointf AzimuthToSphericalPoint(float azimuth_radians) {
  return SphericalPointf(azimuth_radians, 0.f, 1.f);
}

void Verify(NonlinearBeamformer* bf, float target_azimuth_radians) {
  EXPECT_TRUE(bf->IsInBeam(AzimuthToSphericalPoint(target_azimuth_radians)));
  EXPECT_TRUE(bf->IsInBeam(AzimuthToSphericalPoint(
      target_azimuth_radians - NonlinearBeamformer::kHalfBeamWidthRadians +
      0.001f)));
  EXPECT_TRUE(bf->IsInBeam(AzimuthToSphericalPoint(
      target_azimuth_radians + NonlinearBeamformer::kHalfBeamWidthRadians -
      0.001f)));
  EXPECT_FALSE(bf->IsInBeam(AzimuthToSphericalPoint(
      target_azimuth_radians - NonlinearBeamformer::kHalfBeamWidthRadians -
      0.001f)));
  EXPECT_FALSE(bf->IsInBeam(AzimuthToSphericalPoint(
      target_azimuth_radians + NonlinearBeamformer::kHalfBeamWidthRadians +
      0.001f)));
}

void AimAndVerify(NonlinearBeamformer* bf, float target_azimuth_radians) {
  bf->AimAt(AzimuthToSphericalPoint(target_azimuth_radians));
  Verify(bf, target_azimuth_radians);
}

}  // namespace

TEST(NonlinearBeamformerTest, AimingModifiesBeam) {
  std::vector<Point> array_geometry;
  array_geometry.push_back(Point(-0.025f, 0.f, 0.f));
  array_geometry.push_back(Point(0.025f, 0.f, 0.f));
  NonlinearBeamformer bf(array_geometry);
  bf.Initialize(kChunkSizeMs, kSampleRateHz);
  // The default constructor parameter sets the target angle to PI / 2.
  Verify(&bf, static_cast<float>(M_PI) / 2.f);
  AimAndVerify(&bf, static_cast<float>(M_PI) / 3.f);
  AimAndVerify(&bf, 3.f * static_cast<float>(M_PI) / 4.f);
  AimAndVerify(&bf, static_cast<float>(M_PI) / 6.f);
  AimAndVerify(&bf, static_cast<float>(M_PI));
}

TEST(NonlinearBeamformerTest, InterfAnglesTakeAmbiguityIntoAccount) {
  {
    // For linear arrays there is ambiguity.
    std::vector<Point> array_geometry;
    array_geometry.push_back(Point(-0.1f, 0.f, 0.f));
    array_geometry.push_back(Point(0.f, 0.f, 0.f));
    array_geometry.push_back(Point(0.2f, 0.f, 0.f));
    NonlinearBeamformer bf(array_geometry);
    bf.Initialize(kChunkSizeMs, kSampleRateHz);
    EXPECT_EQ(2u, bf.interf_angles_radians_.size());
    EXPECT_FLOAT_EQ(M_PI / 2.f - bf.away_radians_,
                    bf.interf_angles_radians_[0]);
    EXPECT_FLOAT_EQ(M_PI / 2.f + bf.away_radians_,
                    bf.interf_angles_radians_[1]);
    bf.AimAt(AzimuthToSphericalPoint(bf.away_radians_ / 2.f));
    EXPECT_EQ(2u, bf.interf_angles_radians_.size());
    EXPECT_FLOAT_EQ(M_PI - bf.away_radians_ / 2.f,
                    bf.interf_angles_radians_[0]);
    EXPECT_FLOAT_EQ(3.f * bf.away_radians_ / 2.f, bf.interf_angles_radians_[1]);
  }
  {
    // For planar arrays with normal in the xy-plane there is ambiguity.
    std::vector<Point> array_geometry;
    array_geometry.push_back(Point(-0.1f, 0.f, 0.f));
    array_geometry.push_back(Point(0.f, 0.f, 0.f));
    array_geometry.push_back(Point(0.2f, 0.f, 0.f));
    array_geometry.push_back(Point(0.1f, 0.f, 0.2f));
    array_geometry.push_back(Point(0.f, 0.f, -0.1f));
    NonlinearBeamformer bf(array_geometry);
    bf.Initialize(kChunkSizeMs, kSampleRateHz);
    EXPECT_EQ(2u, bf.interf_angles_radians_.size());
    EXPECT_FLOAT_EQ(M_PI / 2.f - bf.away_radians_,
                    bf.interf_angles_radians_[0]);
    EXPECT_FLOAT_EQ(M_PI / 2.f + bf.away_radians_,
                    bf.interf_angles_radians_[1]);
    bf.AimAt(AzimuthToSphericalPoint(bf.away_radians_ / 2.f));
    EXPECT_EQ(2u, bf.interf_angles_radians_.size());
    EXPECT_FLOAT_EQ(M_PI - bf.away_radians_ / 2.f,
                    bf.interf_angles_radians_[0]);
    EXPECT_FLOAT_EQ(3.f * bf.away_radians_ / 2.f, bf.interf_angles_radians_[1]);
  }
  {
    // For planar arrays with normal not in the xy-plane there is no ambiguity.
    std::vector<Point> array_geometry;
    array_geometry.push_back(Point(0.f, 0.f, 0.f));
    array_geometry.push_back(Point(0.2f, 0.f, 0.f));
    array_geometry.push_back(Point(0.f, 0.1f, -0.2f));
    NonlinearBeamformer bf(array_geometry);
    bf.Initialize(kChunkSizeMs, kSampleRateHz);
    EXPECT_EQ(2u, bf.interf_angles_radians_.size());
    EXPECT_FLOAT_EQ(M_PI / 2.f - bf.away_radians_,
                    bf.interf_angles_radians_[0]);
    EXPECT_FLOAT_EQ(M_PI / 2.f + bf.away_radians_,
                    bf.interf_angles_radians_[1]);
    bf.AimAt(AzimuthToSphericalPoint(bf.away_radians_ / 2.f));
    EXPECT_EQ(2u, bf.interf_angles_radians_.size());
    EXPECT_FLOAT_EQ(-bf.away_radians_ / 2.f, bf.interf_angles_radians_[0]);
    EXPECT_FLOAT_EQ(3.f * bf.away_radians_ / 2.f, bf.interf_angles_radians_[1]);
  }
  {
    // For arrays which are not linear or planar there is no ambiguity.
    std::vector<Point> array_geometry;
    array_geometry.push_back(Point(0.f, 0.f, 0.f));
    array_geometry.push_back(Point(0.1f, 0.f, 0.f));
    array_geometry.push_back(Point(0.f, 0.2f, 0.f));
    array_geometry.push_back(Point(0.f, 0.f, 0.3f));
    NonlinearBeamformer bf(array_geometry);
    bf.Initialize(kChunkSizeMs, kSampleRateHz);
    EXPECT_EQ(2u, bf.interf_angles_radians_.size());
    EXPECT_FLOAT_EQ(M_PI / 2.f - bf.away_radians_,
                    bf.interf_angles_radians_[0]);
    EXPECT_FLOAT_EQ(M_PI / 2.f + bf.away_radians_,
                    bf.interf_angles_radians_[1]);
    bf.AimAt(AzimuthToSphericalPoint(bf.away_radians_ / 2.f));
    EXPECT_EQ(2u, bf.interf_angles_radians_.size());
    EXPECT_FLOAT_EQ(-bf.away_radians_ / 2.f, bf.interf_angles_radians_[0]);
    EXPECT_FLOAT_EQ(3.f * bf.away_radians_ / 2.f, bf.interf_angles_radians_[1]);
  }
}

}  // namespace webrtc
