/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Stopwatch_h__
#define Stopwatch_h__

#include "mozilla/dom/TelemetryStopwatchBinding.h"

namespace mozilla {
namespace telemetry {

class Stopwatch {
  using GlobalObject = mozilla::dom::GlobalObject;

 public:
  static bool Start(const GlobalObject& global, const nsAString& histogram,
                    JS::Handle<JSObject*> obj,
                    const dom::TelemetryStopwatchOptions& options);

  static bool Running(const GlobalObject& global, const nsAString& histogram,
                      JS::Handle<JSObject*> obj);

  static bool Cancel(const GlobalObject& global, const nsAString& histogram,
                     JS::Handle<JSObject*> obj);

  static int32_t TimeElapsed(const GlobalObject& global,
                             const nsAString& histogram,
                             JS::Handle<JSObject*> obj, bool canceledOkay);

  static bool Finish(const GlobalObject& global, const nsAString& histogram,
                     JS::Handle<JSObject*> obj, bool canceledOkay);

  static bool StartKeyed(const GlobalObject& global, const nsAString& histogram,
                         const nsAString& key, JS::Handle<JSObject*> obj,
                         const dom::TelemetryStopwatchOptions& options);

  static bool RunningKeyed(const GlobalObject& global,
                           const nsAString& histogram, const nsAString& key,
                           JS::Handle<JSObject*> obj);

  static bool CancelKeyed(const GlobalObject& global,
                          const nsAString& histogram, const nsAString& key,
                          JS::Handle<JSObject*> obj);

  static int32_t TimeElapsedKeyed(const GlobalObject& global,
                                  const nsAString& histogram,
                                  const nsAString& key,
                                  JS::Handle<JSObject*> obj, bool canceledOkay);

  static bool FinishKeyed(const GlobalObject& global,
                          const nsAString& histogram, const nsAString& key,
                          JS::Handle<JSObject*> obj, bool canceledOkay);

  static void SetTestModeEnabled(const GlobalObject& global, bool testing);
};

}  // namespace telemetry
}  // namespace mozilla

#endif  // Stopwatch_h__
