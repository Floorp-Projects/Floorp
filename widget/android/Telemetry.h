/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_Telemetry_h__
#define mozilla_widget_Telemetry_h__

#include "mozilla/java/TelemetryUtilsNatives.h"
#include "nsAppShell.h"
#include "nsIAndroidBridge.h"

#include "mozilla/TimeStamp.h"
#include "mozilla/glean/GleanMetrics.h"

namespace mozilla {
namespace widget {

class Telemetry final : public java::TelemetryUtils::Natives<Telemetry> {
  Telemetry() = delete;

 public:
  static void AddHistogram(jni::String::Param aName, int32_t aValue) {
    MOZ_ASSERT(aName);
    nsCString name = aName->ToCString();
    if (name.EqualsLiteral("GV_STARTUP_RUNTIME_MS")) {
      glean::geckoview::startup_runtime.AccumulateRawDuration(
          TimeDuration::FromMilliseconds(aValue));
    } else if (name.EqualsLiteral("GV_CONTENT_PROCESS_LIFETIME_MS")) {
      glean::geckoview::content_process_lifetime.AccumulateRawDuration(
          TimeDuration::FromMilliseconds(aValue));
    }
  }
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_Telemetry_h__
