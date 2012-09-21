/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OrientationObserver_h
#define OrientationObserver_h

#include "mozilla/Observer.h"
#include "mozilla/dom/ScreenOrientation.h"

namespace mozilla {
namespace hal {
class SensorData;
typedef mozilla::Observer<SensorData> ISensorObserver;
} // namespace hal
} // namespace mozilla

using mozilla::hal::ISensorObserver;
using mozilla::hal::SensorData;
using mozilla::dom::ScreenOrientation;

class OrientationObserver : public ISensorObserver {
public:
  OrientationObserver();
  ~OrientationObserver();

  // Call DisableAutoOrientation on the existing OrientatiOnobserver singleton,
  // if it exists.  If no OrientationObserver exists, do nothing.
  static void ShutDown();

  // Notification from sensor.
  void Notify(const SensorData& aSensorData);

  // Methods to enable/disable automatic orientation.
  void EnableAutoOrientation();
  void DisableAutoOrientation();

  // Methods called by methods in hal_impl namespace.
  bool LockScreenOrientation(ScreenOrientation aOrientation);
  void UnlockScreenOrientation();

  static OrientationObserver* GetInstance();

private:
  bool mAutoOrientationEnabled;
  PRTime mLastUpdate;
  uint32_t mAllowedOrientations;

  // 200 ms, the latency which is barely perceptible by human.
  static const PRTime sMinUpdateInterval = 200 * PR_USEC_PER_MSEC;
  static const uint32_t sDefaultOrientations =
      mozilla::dom::eScreenOrientation_PortraitPrimary |
      mozilla::dom::eScreenOrientation_PortraitSecondary |
      mozilla::dom::eScreenOrientation_LandscapePrimary |
      mozilla::dom::eScreenOrientation_LandscapeSecondary;
};

#endif
