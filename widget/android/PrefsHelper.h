/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PrefsHelper_h
#define PrefsHelper_h

#include "GeneratedJNINatives.h"
#include "MainThreadUtils.h"
#include "nsAppShell.h"

#include "mozilla/UniquePtr.h"

namespace mozilla {

struct PrefsHelper
    : widget::PrefsHelper::Natives<PrefsHelper>
    , UsesGeckoThreadProxy
{
    PrefsHelper() = delete;

    static void GetPrefsById(const jni::ClassObject::LocalRef& cls,
                             int32_t requestId,
                             jni::ObjectArray::Param prefNames,
                             bool observe)
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(nsAppShell::gAppShell);

        nsTArray<jni::Object::LocalRef> namesRefArray(prefNames.GetElements());
        const size_t len = namesRefArray.Length();

        // Java strings are not null-terminated, but XPCOM expects
        // null-terminated strings, so we copy the strings to nsString to
        // ensure null-termination.
        nsTArray<nsString> namesStrArray(len);
        nsTArray<const char16_t*> namesPtrArray(len);

        for (size_t i = 0; i < len; i++) {
            if (!namesRefArray[i]) {
                namesPtrArray.AppendElement(nullptr);
                continue;
            }
            namesPtrArray.AppendElement(namesStrArray.AppendElement(
                    nsString(jni::String::LocalRef(
                    mozilla::Move(namesRefArray[i]))))->Data());
        }

        nsIAndroidBrowserApp* browserApp = nullptr;
        nsAppShell::gAppShell->GetBrowserApp(&browserApp);
        MOZ_ASSERT(browserApp);

        if (observe) {
            browserApp->ObservePreferences(
                    requestId, len ? &namesPtrArray[0] : nullptr, len);
        } else {
            browserApp->GetPreferences(
                    requestId, len ? &namesPtrArray[0] : nullptr, len);
        }
    }

    static void RemovePrefsObserver(int32_t requestId)
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(nsAppShell::gAppShell);

        nsIAndroidBrowserApp* browserApp = nullptr;
        nsAppShell::gAppShell->GetBrowserApp(&browserApp);
        MOZ_ASSERT(browserApp);

        browserApp->RemovePreferenceObservers(requestId);
    }
};

} // namespace

#endif // PrefsHelper_h
