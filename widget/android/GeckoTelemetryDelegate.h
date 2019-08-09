/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoTelemetryDelegate_h__
#define GeckoTelemetryDelegate_h__

#include "geckoview/streaming/GeckoViewStreamingTelemetry.h"

#include <jni.h>

#include "mozilla/jni/Natives.h"
#include "GeneratedJNIWrappers.h"

namespace mozilla {
namespace widget {

class GeckoTelemetryDelegate final
    : public GeckoViewStreamingTelemetry::StreamingTelemetryDelegate,
      public mozilla::java::RuntimeTelemetry::Proxy::Natives<
          GeckoTelemetryDelegate> {
 public:
  // Implement Proxy native.
  static void RegisterDelegateProxy(
      mozilla::java::RuntimeTelemetry::Proxy::Param aProxy) {
    MOZ_ASSERT(aProxy);

    GeckoViewStreamingTelemetry::RegisterDelegate(
        new GeckoTelemetryDelegate(aProxy));
  }

  explicit GeckoTelemetryDelegate(
      mozilla::java::RuntimeTelemetry::Proxy::Param aProxy)
      : mProxy(aProxy) {}

 private:
  // Implement StreamingTelemetryDelegate.
  void ReceiveHistogramSamples(const nsCString& aName,
                               const nsTArray<uint32_t>& aSamples) override {
    if (!mozilla::jni::IsAvailable() || !mProxy || aSamples.Length() < 1) {
      return;
    }

    nsTArray<int64_t>* samples = new nsTArray<int64_t>();
    for (size_t i = 0; i < aSamples.Length(); i++) {
      samples->AppendElement(static_cast<int64_t>(aSamples[i]));
    }

    mProxy->DispatchTelemetry(
        aName,
        mozilla::jni::LongArray::New(samples->Elements(), samples->Length()));
  }

  mozilla::java::RuntimeTelemetry::Proxy::GlobalRef mProxy;
};

}  // namespace widget
}  // namespace mozilla

#endif  // GeckoTelemetryDelegate_h__
