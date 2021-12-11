/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_Telemetry_h__
#define mozilla_widget_Telemetry_h__

#include "mozilla/java/TelemetryUtilsNatives.h"
#include "nsAppShell.h"
#include "nsIAndroidBridge.h"

#include "mozilla/Telemetry.h"

namespace mozilla {
namespace widget {

class Telemetry final : public java::TelemetryUtils::Natives<Telemetry> {
  Telemetry() = delete;

 public:
  static void AddHistogram(jni::String::Param aName, int32_t aValue) {
    MOZ_ASSERT(aName);
    mozilla::Telemetry::Accumulate(aName->ToCString().get(), aValue);
  }
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_Telemetry_h__
