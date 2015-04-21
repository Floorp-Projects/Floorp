/*
 * Copyright (c) 2013, Linux Foundation. All rights reserved
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/basictypes.h"
#include "mozilla/Hal.h"
#include "mozilla/unused.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "OrientationObserver.h"
#include "ProcessOrientation.h"
#include "mozilla/HalSensor.h"
#include "math.h"
#include "limits.h"
#include "android/log.h"

#if 0
#define LOGD(args...)  __android_log_print(ANDROID_LOG_DEBUG, "ProcessOrientation" , ## args)
#else
#define LOGD(args...)
#endif

namespace mozilla {

// We work with all angles in degrees in this class.
#define RADIANS_TO_DEGREES (180/M_PI)

// Number of nanoseconds per millisecond.
#define NANOS_PER_MS 1000000

// Indices into SensorEvent.values for the accelerometer sensor.
#define ACCELEROMETER_DATA_X 0
#define ACCELEROMETER_DATA_Y 1
#define ACCELEROMETER_DATA_Z 2

// The minimum amount of time that a predicted rotation must be stable before
// it is accepted as a valid rotation proposal. This value can be quite small
// because the low-pass filter already suppresses most of the noise so we're
// really just looking for quick confirmation that the last few samples are in
// agreement as to the desired orientation.
#define PROPOSAL_SETTLE_TIME_NANOS (40*NANOS_PER_MS)

// The minimum amount of time that must have elapsed since the device last
// exited the flat state (time since it was picked up) before the proposed
// rotation can change.
#define PROPOSAL_MIN_TIME_SINCE_FLAT_ENDED_NANOS (500*NANOS_PER_MS)

// The minimum amount of time that must have elapsed since the device stopped
// swinging (time since device appeared to be in the process of being put down
// or put away into a pocket) before the proposed rotation can change.
#define PROPOSAL_MIN_TIME_SINCE_SWING_ENDED_NANOS (300*NANOS_PER_MS)

// The minimum amount of time that must have elapsed since the device stopped
// undergoing external acceleration before the proposed rotation can change.
#define PROPOSAL_MIN_TIME_SINCE_ACCELERATION_ENDED_NANOS (500*NANOS_PER_MS)

// If the tilt angle remains greater than the specified angle for a minimum of
// the specified time, then the device is deemed to be lying flat
// (just chillin' on a table).
#define FLAT_ANGLE 75
#define FLAT_TIME_NANOS (1000*NANOS_PER_MS)

// If the tilt angle has increased by at least delta degrees within the
// specified amount of time, then the device is deemed to be swinging away
// from the user down towards flat (tilt = 90).
#define SWING_AWAY_ANGLE_DELTA 20
#define SWING_TIME_NANOS (300*NANOS_PER_MS)

// The maximum sample inter-arrival time in milliseconds. If the acceleration
// samples are further apart than this amount in time, we reset the state of
// the low-pass filter and orientation properties.  This helps to handle
// boundary conditions when the device is turned on, wakes from suspend or
// there is a significant gap in samples.
#define MAX_FILTER_DELTA_TIME_NANOS (1000*NANOS_PER_MS)

// The acceleration filter time constant.
//
// This time constant is used to tune the acceleration filter such that
// impulses and vibrational noise (think car dock) is suppressed before we try
// to calculate the tilt and orientation angles.
//
// The filter time constant is related to the filter cutoff frequency, which
// is the frequency at which signals are attenuated by 3dB (half the passband
// power). Each successive octave beyond this frequency is attenuated by an
// additional 6dB.
//
// Given a time constant t in seconds, the filter cutoff frequency Fc in Hertz
// is given by Fc = 1 / (2pi * t).
//
// The higher the time constant, the lower the cutoff frequency, so more noise
// will be suppressed.
//
// Filtering adds latency proportional the time constant (inversely
// proportional to the cutoff frequency) so we don't want to make the time
// constant too large or we can lose responsiveness.  Likewise we don't want
// to make it too small or we do a poor job suppressing acceleration spikes.
// Empirically, 100ms seems to be too small and 500ms is too large. Android
// default is 200.
#define FILTER_TIME_CONSTANT_MS 200.0f

// State for orientation detection. Thresholds for minimum and maximum
// allowable deviation from gravity.
//
// If the device is undergoing external acceleration (being bumped, in a car
// that is turning around a corner or a plane taking off) then the magnitude
// may be substantially more or less than gravity.  This can skew our
// orientation detection by making us think that up is pointed in a different
// direction.
//
// Conversely, if the device is in freefall, then there will be no gravity to
// measure at all.  This is problematic because we cannot detect the orientation
// without gravity to tell us which way is up. A magnitude near 0 produces
// singularities in the tilt and orientation calculations.
//
// In both cases, we postpone choosing an orientation.
//
// However, we need to tolerate some acceleration because the angular momentum
// of turning the device can skew the observed acceleration for a short period
// of time.
#define NEAR_ZERO_MAGNITUDE 1 // m/s^2
#define ACCELERATION_TOLERANCE 4 // m/s^2
#define STANDARD_GRAVITY 9.80665f
#define MIN_ACCELERATION_MAGNITUDE (STANDARD_GRAVITY-ACCELERATION_TOLERANCE)
#define MAX_ACCELERATION_MAGNITUDE (STANDARD_GRAVITY+ACCELERATION_TOLERANCE)

// Maximum absolute tilt angle at which to consider orientation data. Beyond
// this (i.e. when screen is facing the sky or ground), we completely ignore
// orientation data.
#define MAX_TILT 75

// The gap angle in degrees between adjacent orientation angles for
// hysteresis.This creates a "dead zone" between the current orientation and a
// proposed adjacent orientation. No orientation proposal is made when the
// orientation angle is within the gap between the current orientation and the
// adjacent orientation.
#define ADJACENT_ORIENTATION_ANGLE_GAP 45

const int
ProcessOrientation::tiltTolerance[][4] = {
  {-25, 70}, // ROTATION_0
  {-25, 65}, // ROTATION_90
  {-25, 60}, // ROTATION_180
  {-25, 65}  // ROTATION_270
};

int
ProcessOrientation::GetProposedRotation()
{
  return mProposedRotation;
}

int
ProcessOrientation::OnSensorChanged(const SensorData& event,
                                    int deviceCurrentRotation)
{
  // The vector given in the SensorEvent points straight up (towards the sky)
  // under ideal conditions (the phone is not accelerating). I'll call this up
  // vector elsewhere.
  const InfallibleTArray<float>& values = event.values();
  float x = values[ACCELEROMETER_DATA_X];
  float y = values[ACCELEROMETER_DATA_Y];
  float z = values[ACCELEROMETER_DATA_Z];

  LOGD
    ("ProcessOrientation: Raw acceleration vector: x = %f, y = %f, z = %f,"
     "magnitude = %f\n", x, y, z, sqrt(x * x + y * y + z * z));
  // Apply a low-pass filter to the acceleration up vector in cartesian space.
  // Reset the orientation listener state if the samples are too far apart in
  // time or when we see values of (0, 0, 0) which indicates that we polled the
  // accelerometer too soon after turning it on and we don't have any data yet.
  const int64_t now = (int64_t) event.timestamp();
  const int64_t then = mLastFilteredTimestampNanos;
  const float timeDeltaMS = (now - then) * 0.000001f;
  bool skipSample = false;
  if (now < then
      || now > then + MAX_FILTER_DELTA_TIME_NANOS
      || (x == 0 && y == 0 && z == 0)) {
    LOGD
      ("ProcessOrientation: Resetting orientation listener.");
    Reset();
    skipSample = true;
  } else {
    const float alpha = timeDeltaMS / (FILTER_TIME_CONSTANT_MS + timeDeltaMS);
    x = alpha * (x - mLastFilteredX) + mLastFilteredX;
    y = alpha * (y - mLastFilteredY) + mLastFilteredY;
    z = alpha * (z - mLastFilteredZ) + mLastFilteredZ;
    LOGD
      ("ProcessOrientation: Filtered acceleration vector: x=%f, y=%f, z=%f,"
       "magnitude=%f", z, y, z, sqrt(x * x + y * y + z * z));
    skipSample = false;
  }
  mLastFilteredTimestampNanos = now;
  mLastFilteredX = x;
  mLastFilteredY = y;
  mLastFilteredZ = z;

  bool isAccelerating = false;
  bool isFlat = false;
  bool isSwinging = false;
  if (skipSample) {
    return -1;
  }

  // Calculate the magnitude of the acceleration vector.
  const float magnitude = sqrt(x * x + y * y + z * z);
  if (magnitude < NEAR_ZERO_MAGNITUDE) {
    LOGD
      ("ProcessOrientation: Ignoring sensor data, magnitude too close to"
       " zero.");
    ClearPredictedRotation();
  } else {
    // Determine whether the device appears to be undergoing external
    // acceleration.
    if (this->IsAccelerating(magnitude)) {
      isAccelerating = true;
      mAccelerationTimestampNanos = now;
    }
    // Calculate the tilt angle. This is the angle between the up vector and
    // the x-y plane (the plane of the screen) in a range of [-90, 90]
    // degrees.
    //   -90 degrees: screen horizontal and facing the ground (overhead)
    //     0 degrees: screen vertical
    //    90 degrees: screen horizontal and facing the sky (on table)
    const int tiltAngle =
      static_cast<int>(roundf(asin(z / magnitude) * RADIANS_TO_DEGREES));
    AddTiltHistoryEntry(now, tiltAngle);

    // Determine whether the device appears to be flat or swinging.
    if (this->IsFlat(now)) {
      isFlat = true;
      mFlatTimestampNanos = now;
    }
    if (this->IsSwinging(now, tiltAngle)) {
      isSwinging = true;
      mSwingTimestampNanos = now;
    }
    // If the tilt angle is too close to horizontal then we cannot determine
    // the orientation angle of the screen.
    if (abs(tiltAngle) > MAX_TILT) {
      LOGD
        ("ProcessOrientation: Ignoring sensor data, tilt angle too high:"
         " tiltAngle=%d", tiltAngle);
      ClearPredictedRotation();
    } else {
      // Calculate the orientation angle.
      // This is the angle between the x-y projection of the up vector onto
      // the +y-axis, increasing clockwise in a range of [0, 360] degrees.
      int orientationAngle =
        static_cast<int>(roundf(-atan2f(-x, y) * RADIANS_TO_DEGREES));
      if (orientationAngle < 0) {
        // atan2 returns [-180, 180]; normalize to [0, 360]
        orientationAngle += 360;
      }
      // Find the nearest rotation.
      int nearestRotation = (orientationAngle + 45) / 90;
      if (nearestRotation == 4) {
        nearestRotation = 0;
      }
      // Determine the predicted orientation.
      if (IsTiltAngleAcceptable(nearestRotation, tiltAngle)
          &&
          IsOrientationAngleAcceptable
          (nearestRotation, orientationAngle, deviceCurrentRotation)) {
        UpdatePredictedRotation(now, nearestRotation);
        LOGD
          ("ProcessOrientation: Predicted: tiltAngle=%d, orientationAngle=%d,"
           " predictedRotation=%d, predictedRotationAgeMS=%f",
           tiltAngle,
           orientationAngle,
           mPredictedRotation,
           ((now - mPredictedRotationTimestampNanos) * 0.000001f));
      } else {
        LOGD
          ("ProcessOrientation: Ignoring sensor data, no predicted rotation:"
           " tiltAngle=%d, orientationAngle=%d",
           tiltAngle,
           orientationAngle);
        ClearPredictedRotation();
      }
    }
  }

  // Determine new proposed rotation.
  const int oldProposedRotation = mProposedRotation;
  if (mPredictedRotation < 0 || IsPredictedRotationAcceptable(now)) {
    mProposedRotation = mPredictedRotation;
  }
  // Write final statistics about where we are in the orientation detection
  // process.
  LOGD
    ("ProcessOrientation: Result: oldProposedRotation=%d,currentRotation=%d, "
     "proposedRotation=%d, predictedRotation=%d, timeDeltaMS=%f, "
     "isAccelerating=%d, isFlat=%d, isSwinging=%d, timeUntilSettledMS=%f, "
     "timeUntilAccelerationDelayExpiredMS=%f, timeUntilFlatDelayExpiredMS=%f, "
     "timeUntilSwingDelayExpiredMS=%f",
     oldProposedRotation,
     deviceCurrentRotation, mProposedRotation,
     mPredictedRotation, timeDeltaMS, isAccelerating, isFlat,
     isSwinging, RemainingMS(now,
                             mPredictedRotationTimestampNanos +
                             PROPOSAL_SETTLE_TIME_NANOS),
     RemainingMS(now,
                 mAccelerationTimestampNanos +
                 PROPOSAL_MIN_TIME_SINCE_ACCELERATION_ENDED_NANOS),
     RemainingMS(now,
                 mFlatTimestampNanos +
                 PROPOSAL_MIN_TIME_SINCE_FLAT_ENDED_NANOS),
     RemainingMS(now,
                 mSwingTimestampNanos +
                 PROPOSAL_MIN_TIME_SINCE_SWING_ENDED_NANOS));

  // Avoid unused-but-set compile warnings for these variables, when LOGD is
  // a no-op, as it is by default:
  unused << isAccelerating;
  unused << isFlat;
  unused << isSwinging;

  // Tell the listener.
  if (mProposedRotation != oldProposedRotation && mProposedRotation >= 0) {
    LOGD
      ("ProcessOrientation: Proposed rotation changed!  proposedRotation=%d, "
       "oldProposedRotation=%d",
       mProposedRotation,
       oldProposedRotation);
    return mProposedRotation;
  }
  // Don't rotate screen
  return -1;
}

bool
ProcessOrientation::IsTiltAngleAcceptable(int rotation, int tiltAngle)
{
  return (tiltAngle >= tiltTolerance[rotation][0]
          && tiltAngle <= tiltTolerance[rotation][1]);
}

bool
ProcessOrientation::IsOrientationAngleAcceptable(int rotation,
                                                 int orientationAngle,
                                                 int currentRotation)
{
  // If there is no current rotation, then there is no gap.
  // The gap is used only to introduce hysteresis among advertised orientation
  // changes to avoid flapping.
  if (currentRotation < 0) {
    return true;
  }
  // If the specified rotation is the same or is counter-clockwise adjacent
  // to the current rotation, then we set a lower bound on the orientation
  // angle. For example, if currentRotation is ROTATION_0 and proposed is
  // ROTATION_90, then we want to check orientationAngle > 45 + GAP / 2.
  if (rotation == currentRotation || rotation == (currentRotation + 1) % 4) {
    int lowerBound = rotation * 90 - 45 + ADJACENT_ORIENTATION_ANGLE_GAP / 2;
    if (rotation == 0) {
      if (orientationAngle >= 315 && orientationAngle < lowerBound + 360) {
        return false;
      }
    } else {
      if (orientationAngle < lowerBound) {
        return false;
      }
    }
  }
  // If the specified rotation is the same or is clockwise adjacent, then we
  // set an upper bound on the orientation angle. For example, if
  // currentRotation is ROTATION_0 and rotation is ROTATION_270, then we want
  // to check orientationAngle < 315 - GAP / 2.
  if (rotation == currentRotation || rotation == (currentRotation + 3) % 4) {
    int upperBound = rotation * 90 + 45 - ADJACENT_ORIENTATION_ANGLE_GAP / 2;
    if (rotation == 0) {
      if (orientationAngle <= 45 && orientationAngle > upperBound) {
        return false;
      }
    } else {
      if (orientationAngle > upperBound) {
        return false;
      }
    }
  }
  return true;
}

bool
ProcessOrientation::IsPredictedRotationAcceptable(int64_t now)
{
  // The predicted rotation must have settled long enough.
  if (now < mPredictedRotationTimestampNanos + PROPOSAL_SETTLE_TIME_NANOS) {
    return false;
  }
  // The last flat state (time since picked up) must have been sufficiently long
  // ago.
  if (now < mFlatTimestampNanos + PROPOSAL_MIN_TIME_SINCE_FLAT_ENDED_NANOS) {
    return false;
  }
  // The last swing state (time since last movement to put down) must have been
  // sufficiently long ago.
  if (now < mSwingTimestampNanos + PROPOSAL_MIN_TIME_SINCE_SWING_ENDED_NANOS) {
    return false;
  }
  // The last acceleration state must have been sufficiently long ago.
  if (now < mAccelerationTimestampNanos
      + PROPOSAL_MIN_TIME_SINCE_ACCELERATION_ENDED_NANOS) {
    return false;
  }
  // Looks good!
  return true;
}

int
ProcessOrientation::Reset()
{
  mLastFilteredTimestampNanos = std::numeric_limits<int64_t>::min();
  mProposedRotation = -1;
  mFlatTimestampNanos = std::numeric_limits<int64_t>::min();
  mSwingTimestampNanos = std::numeric_limits<int64_t>::min();
  mAccelerationTimestampNanos = std::numeric_limits<int64_t>::min();
  ClearPredictedRotation();
  ClearTiltHistory();
  return -1;
}

void
ProcessOrientation::ClearPredictedRotation()
{
  mPredictedRotation = -1;
  mPredictedRotationTimestampNanos = std::numeric_limits<int64_t>::min();
}

void
ProcessOrientation::UpdatePredictedRotation(int64_t now, int rotation)
{
  if (mPredictedRotation != rotation) {
    mPredictedRotation = rotation;
    mPredictedRotationTimestampNanos = now;
  }
}

bool
ProcessOrientation::IsAccelerating(float magnitude)
{
  return magnitude < MIN_ACCELERATION_MAGNITUDE
    || magnitude > MAX_ACCELERATION_MAGNITUDE;
}

void
ProcessOrientation::ClearTiltHistory()
{
  mTiltHistory.history[0].timestampNanos = std::numeric_limits<int64_t>::min();
  mTiltHistory.index = 1;
}

void
ProcessOrientation::AddTiltHistoryEntry(int64_t now, float tilt)
{
  mTiltHistory.history[mTiltHistory.index].tiltAngle = tilt;
  mTiltHistory.history[mTiltHistory.index].timestampNanos = now;
  mTiltHistory.index = (mTiltHistory.index + 1) % TILT_HISTORY_SIZE;
  mTiltHistory.history[mTiltHistory.index].timestampNanos = std::numeric_limits<int64_t>::min();
}

bool
ProcessOrientation::IsFlat(int64_t now)
{
  for (int i = mTiltHistory.index; (i = NextTiltHistoryIndex(i)) >= 0;) {
    if (mTiltHistory.history[i].tiltAngle < FLAT_ANGLE) {
      break;
    }
    if (mTiltHistory.history[i].timestampNanos + FLAT_TIME_NANOS <= now) {
      // Tilt has remained greater than FLAT_TILT_ANGLE for FLAT_TIME_NANOS.
      return true;
    }
  }
  return false;
}

bool
ProcessOrientation::IsSwinging(int64_t now, float tilt)
{
  for (int i = mTiltHistory.index; (i = NextTiltHistoryIndex(i)) >= 0;) {
    if (mTiltHistory.history[i].timestampNanos + SWING_TIME_NANOS < now) {
      break;
    }
    if (mTiltHistory.history[i].tiltAngle + SWING_AWAY_ANGLE_DELTA <= tilt) {
      // Tilted away by SWING_AWAY_ANGLE_DELTA within SWING_TIME_NANOS.
      return true;
    }
  }
  return false;
}

int
ProcessOrientation::NextTiltHistoryIndex(int index)
{
  index = (index == 0 ? TILT_HISTORY_SIZE : index) - 1;
  return mTiltHistory.history[index].timestampNanos != std::numeric_limits<int64_t>::min() ? index : -1;
}

float
ProcessOrientation::RemainingMS(int64_t now, int64_t until)
{
  return now >= until ? 0 : (until - now) * 0.000001f;
}

} // namespace mozilla
