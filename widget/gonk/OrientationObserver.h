/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  PRUint32 mAllowedOrientations;

  // 200 ms, the latency which is barely perceptible by human.
  static const PRTime sMinUpdateInterval = 200 * PR_USEC_PER_MSEC;
  static const PRUint32 sDefaultOrientations =
      mozilla::dom::eScreenOrientation_Portrait |
      mozilla::dom::eScreenOrientation_Landscape;
};

#endif
