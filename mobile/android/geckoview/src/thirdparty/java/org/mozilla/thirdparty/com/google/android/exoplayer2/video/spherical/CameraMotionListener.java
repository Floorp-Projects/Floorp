/*
 * Copyright (C) 2018 The Android Open Source Project
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
package org.mozilla.thirdparty.com.google.android.exoplayer2.video.spherical;

/** Listens camera motion. */
public interface CameraMotionListener {

  /**
   * Called when a new camera motion is read. This method is called on the playback thread.
   *
   * @param timeUs The presentation time of the data.
   * @param rotation Angle axis orientation in radians representing the rotation from camera
   *     coordinate system to world coordinate system.
   */
  void onCameraMotion(long timeUs, float[] rotation);

  /** Called when the camera motion track position is reset or the track is disabled. */
  void onCameraMotionReset();
}
