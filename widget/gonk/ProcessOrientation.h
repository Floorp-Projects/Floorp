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

#ifndef ProcessOrientation_h
#define ProcessOrientation_h

#include "mozilla/Hal.h"

namespace mozilla {

// History of observed tilt angles.
#define TILT_HISTORY_SIZE 40

class ProcessOrientation {
public:
  ProcessOrientation() {};
  ~ProcessOrientation() {};

  int OnSensorChanged(const mozilla::hal::SensorData& event, int deviceCurrentRotation);
  int Reset();

private:
  int GetProposedRotation();

  // Returns true if the tilt angle is acceptable for a given predicted
  // rotation.
  bool IsTiltAngleAcceptable(int rotation, int tiltAngle);

  // Returns true if the orientation angle is acceptable for a given predicted
  // rotation. This function takes into account the gap between adjacent
  // orientations for hysteresis.
  bool IsOrientationAngleAcceptable(int rotation, int orientationAngle,
                                    int currentRotation);

  // Returns true if the predicted rotation is ready to be advertised as a
  // proposed rotation.
  bool IsPredictedRotationAcceptable(long now);

  void ClearPredictedRotation();
  void UpdatePredictedRotation(long now, int rotation);
  bool IsAccelerating(float magnitude);
  void ClearTiltHistory();
  void AddTiltHistoryEntry(long now, float tilt);
  bool IsFlat(long now);
  bool IsSwinging(long now, float tilt);
  int NextTiltHistoryIndex(int index);
  float RemainingMS(long now, long until);

  // The tilt angle range in degrees for each orientation. Beyond these tilt
  // angles, we don't even consider transitioning into the specified orientation.
  // We place more stringent requirements on unnatural orientations than natural
  // ones to make it less likely to accidentally transition into those states.
  // The first value of each pair is negative so it applies a limit when the
  // device is facing down (overhead reading in bed). The second value of each
  // pair is positive so it applies a limit when the device is facing up
  // (resting on a table). The ideal tilt angle is 0 (when the device is vertical)
  // so the limits establish how close to vertical the device must be in order
  // to change orientation.
  static const int tiltTolerance[][4];

  // Timestamp and value of the last accelerometer sample.
  long mLastFilteredTimestampNanos;
  float mLastFilteredX, mLastFilteredY, mLastFilteredZ;

  // The last proposed rotation, -1 if unknown.
  int mProposedRotation;

  // Value of the current predicted rotation, -1 if unknown.
  int mPredictedRotation;

  // Timestamp of when the predicted rotation most recently changed.
  long mPredictedRotationTimestampNanos;

  // Timestamp when the device last appeared to be flat for sure (the flat delay
  // elapsed).
  long mFlatTimestampNanos;

  // Timestamp when the device last appeared to be swinging.
  long mSwingTimestampNanos;

  // Timestamp when the device last appeared to be undergoing external
  // acceleration.
  long mAccelerationTimestampNanos;

  struct {
    struct {
      float tiltAngle;
      long timestampNanos;
    } history[TILT_HISTORY_SIZE];
    int index;
  } mTiltHistory;
};

} // namespace mozilla
#endif
