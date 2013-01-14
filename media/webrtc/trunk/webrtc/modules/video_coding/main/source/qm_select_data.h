/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_SOURCE_QM_SELECT_DATA_H_
#define WEBRTC_MODULES_VIDEO_CODING_SOURCE_QM_SELECT_DATA_H_

/***************************************************************
*QMSelectData.h
* This file includes parameters for content-aware media optimization
****************************************************************/

#include "typedefs.h"

namespace webrtc {
//
// PARAMETERS FOR RESOLUTION ADAPTATION
//

// Initial level of buffer in secs.
const float kInitBufferLevel = 0.5f;

// Threshold of (max) buffer size below which we consider too low (underflow).
const float kPercBufferThr = 0.10f;

// Threshold on the occurrences of low buffer levels.
const float kMaxBufferLow = 0.30f;

// Threshold on rate mismatch.
const float kMaxRateMisMatch = 0.5f;

// Threshold on amount of under/over encoder shooting.
const float kRateOverShoot = 0.75f;
const float kRateUnderShoot = 0.75f;

// Factor to favor weighting the average rates with the current/last data.
const float kWeightRate = 0.70f;

// Factor for transitional rate for going back up in resolution.
const float kTransRateScaleUpSpatial = 1.25f;
const float kTransRateScaleUpTemp = 1.25f;
const float kTransRateScaleUpSpatialTemp = 1.25f;

// Threshold on packet loss rate, above which favor resolution reduction.
const float kPacketLossThr = 0.1f;

// Factor for reducing transitional bitrate under packet loss.
const float kPacketLossRateFac = 1.0f;

// Maximum possible transitional rate for down-sampling:
// (units in kbps), for 30fps.
const uint16_t kMaxRateQm[9] = {
    0,     // QCIF
    50,    // kHCIF
    125,   // kQVGA
    200,   // CIF
    280,   // HVGA
    400,   // VGA
    700,   // QFULLHD
    1000,  // WHD
    1500   // FULLHD
};

// Frame rate scale for maximum transition rate.
const float kFrameRateFac[4] = {
    0.5f,    // Low
    0.7f,    // Middle level 1
    0.85f,   // Middle level 2
    1.0f,    // High
};

// Scale for transitional rate: based on content class
// motion=L/H/D,spatial==L/H/D: for low, high, middle levels
const float kScaleTransRateQm[18] = {
    // VGA and lower
    0.40f,       // L, L
    0.50f,       // L, H
    0.40f,       // L, D
    0.60f,       // H ,L
    0.60f,       // H, H
    0.60f,       // H, D
    0.50f,       // D, L
    0.50f,       // D, D
    0.50f,       // D, H

    // over VGA
    0.40f,       // L, L
    0.50f,       // L, H
    0.40f,       // L, D
    0.60f,       // H ,L
    0.60f,       // H, H
    0.60f,       // H, D
    0.50f,       // D, L
    0.50f,       // D, D
    0.50f,       // D, H
};

// Threshold on the target rate relative to transitional rate.
const float kFacLowRate = 0.5f;

// Action for down-sampling:
// motion=L/H/D,spatial==L/H/D, for low, high, middle levels;
// rate = 0/1/2, for target rate state relative to transition rate.
const uint8_t kSpatialAction[27] = {
// rateClass = 0:
    1,       // L, L
    1,       // L, H
    1,       // L, D
    4,       // H ,L
    1,       // H, H
    4,       // H, D
    4,       // D, L
    1,       // D, H
    2,       // D, D

// rateClass = 1:
    1,       // L, L
    1,       // L, H
    1,       // L, D
    2,       // H ,L
    1,       // H, H
    2,       // H, D
    2,       // D, L
    1,       // D, H
    2,       // D, D

// rateClass = 2:
    1,       // L, L
    1,       // L, H
    1,       // L, D
    2,       // H ,L
    1,       // H, H
    2,       // H, D
    2,       // D, L
    1,       // D, H
    2,       // D, D
};

const uint8_t kTemporalAction[27] = {
// rateClass = 0:
    3,       // L, L
    2,       // L, H
    2,       // L, D
    1,       // H ,L
    3,       // H, H
    1,       // H, D
    1,       // D, L
    2,       // D, H
    1,       // D, D

// rateClass = 1:
    3,       // L, L
    3,       // L, H
    3,       // L, D
    1,       // H ,L
    3,       // H, H
    1,       // H, D
    1,       // D, L
    3,       // D, H
    1,       // D, D

// rateClass = 2:
    1,       // L, L
    3,       // L, H
    3,       // L, D
    1,       // H ,L
    3,       // H, H
    1,       // H, D
    1,       // D, L
    3,       // D, H
    1,       // D, D
};

// Control the total amount of down-sampling allowed.
const float kMaxSpatialDown = 8.0f;
const float kMaxTempDown = 3.0f;
const float kMaxTotalDown = 9.0f;

// Minimum image size for a spatial down-sampling.
const int kMinImageSize = 176 * 144;

// Minimum frame rate for temporal down-sampling:
// no frame rate reduction if incomingFrameRate <= MIN_FRAME_RATE.
const int kMinFrameRate = 8;

//
// PARAMETERS FOR FEC ADJUSTMENT: TODO (marpan)
//

//
// PARAMETETS FOR SETTING LOW/HIGH STATES OF CONTENT METRICS:
//

// Thresholds for frame rate:
const int kLowFrameRate = 10;
const int kMiddleFrameRate = 15;
const int kHighFrameRate = 25;

// Thresholds for motion: motion level is from NFD.
const float kHighMotionNfd = 0.075f;
const float kLowMotionNfd = 0.03f;

// Thresholds for spatial prediction error:
// this is applied on the average of (2x2,1x2,2x1).
const float kHighTexture = 0.035f;
const float kLowTexture = 0.020f;

// Used to reduce thresholds for larger/HD scenes: correction factor since
// higher correlation in HD scenes means lower spatial prediction error.
const float kScaleTexture = 0.9f;

// Percentage reduction in transitional bitrate for 2x2 selected over 1x2/2x1.
const float kRateRedSpatial2X2 = 0.6f;

const float kSpatialErr2x2VsHoriz = 0.1f;   // percentage to favor 2x2 over H
const float kSpatialErr2X2VsVert = 0.1f;    // percentage to favor 2x2 over V
const float kSpatialErrVertVsHoriz = 0.1f;  // percentage to favor H over V

}  //  namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_SOURCE_QM_SELECT_DATA_H_

