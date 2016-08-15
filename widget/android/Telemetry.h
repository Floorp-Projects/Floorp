/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_Telemetry_h__
#define mozilla_widget_Telemetry_h__

#include "GeneratedJNINatives.h"
#include "nsAppShell.h"
#include "nsIAndroidBridge.h"

#include "mozilla/Telemetry.h"

namespace mozilla {
namespace widget {

class Telemetry final
    : public java::Telemetry::Natives<Telemetry>
{
    Telemetry() = delete;

    static already_AddRefed<nsIUITelemetryObserver>
    GetObserver()
    {
        nsAppShell* const appShell = nsAppShell::Get();
        if (!appShell) {
            return nullptr;
        }

        nsCOMPtr<nsIAndroidBrowserApp> browserApp = appShell->GetBrowserApp();
        nsCOMPtr<nsIUITelemetryObserver> obs;

        if (!browserApp || NS_FAILED(browserApp->GetUITelemetryObserver(
                getter_AddRefs(obs))) || !obs) {
            return nullptr;
        }

        return obs.forget();
    }

public:
    static void
    AddHistogram(jni::String::Param aName, int32_t aValue)
    {
        MOZ_ASSERT(aName);
        mozilla::Telemetry::Accumulate(aName->ToCString().get(), aValue);
    }

    static void
    AddKeyedHistogram(jni::String::Param aName, jni::String::Param aKey,
                      int32_t aValue)
    {
        MOZ_ASSERT(aName && aKey);
        mozilla::Telemetry::Accumulate(aName->ToCString().get(),
                                       aKey->ToCString(), aValue);
    }

    static void
    StartUISession(jni::String::Param aName, int64_t aTimestamp)
    {
        MOZ_ASSERT(aName);
        nsCOMPtr<nsIUITelemetryObserver> obs = GetObserver();
        if (obs) {
            obs->StartSession(aName->ToString().get(), aTimestamp);
        }
    }

    static void
    StopUISession(jni::String::Param aName, jni::String::Param aReason,
                  int64_t aTimestamp)
    {
        MOZ_ASSERT(aName);
        nsCOMPtr<nsIUITelemetryObserver> obs = GetObserver();
        if (obs) {
            obs->StopSession(aName->ToString().get(),
                             aReason ? aReason->ToString().get() : nullptr,
                             aTimestamp);
        }
    }

    static void
    AddUIEvent(jni::String::Param aAction, jni::String::Param aMethod,
               int64_t aTimestamp, jni::String::Param aExtras)
    {
        MOZ_ASSERT(aAction);
        nsCOMPtr<nsIUITelemetryObserver> obs = GetObserver();
        if (obs) {
            obs->AddEvent(aAction->ToString().get(),
                          aMethod ? aMethod->ToString().get() : nullptr,
                          aTimestamp,
                          aExtras ? aExtras->ToString().get() : nullptr);
        }
    }
};

} // namespace widget
} // namespace mozilla

#endif // mozilla_widget_Telemetry_h__
