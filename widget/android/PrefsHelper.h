/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PrefsHelper_h
#define PrefsHelper_h

#include "GeneratedJNINatives.h"
#include "MainThreadUtils.h"
#include "nsAppShell.h"
#include "nsCOMPtr.h"
#include "nsVariant.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class PrefsHelper
    : public widget::PrefsHelper::Natives<PrefsHelper>
    , public UsesGeckoThreadProxy
{
    PrefsHelper() = delete;

    static bool GetVariantPref(nsIObserverService* aObsServ,
                               nsIWritableVariant* aVariant,
                               jni::Object::Param aPrefHandler,
                               jni::String::LocalRef& aPrefName)
    {
        if (NS_FAILED(aObsServ->NotifyObservers(aVariant, "android-get-pref",
                                                nsString(aPrefName).get()))) {
            return false;
        }

        uint16_t varType = nsIDataType::VTYPE_EMPTY;
        if (NS_FAILED(aVariant->GetDataType(&varType))) {
            return false;
        }

        int32_t type = widget::PrefsHelper::PREF_INVALID;
        bool boolVal = false;
        int32_t intVal = 0;
        nsAutoString strVal;

        switch (varType) {
            case nsIDataType::VTYPE_BOOL:
                type = widget::PrefsHelper::PREF_BOOL;
                if (NS_FAILED(aVariant->GetAsBool(&boolVal))) {
                    return false;
                }
                break;
            case nsIDataType::VTYPE_INT32:
                type = widget::PrefsHelper::PREF_INT;
                if (NS_FAILED(aVariant->GetAsInt32(&intVal))) {
                    return false;
                }
                break;
            case nsIDataType::VTYPE_ASTRING:
                type = widget::PrefsHelper::PREF_STRING;
                if (NS_FAILED(aVariant->GetAsAString(strVal))) {
                    return false;
                }
                break;
            default:
                return false;
        }

        const auto& jstrVal = type == widget::PrefsHelper::PREF_STRING ?
                jni::Param<jni::String>(strVal, aPrefName.Env()) :
                jni::Param<jni::String>(nullptr);

        widget::PrefsHelper::CallPrefHandler(
                aPrefHandler, type, aPrefName, boolVal, intVal, jstrVal);
        return true;
    }

    static bool SetVariantPref(nsIObserverService* aObsServ,
                               nsIWritableVariant* aVariant,
                               jni::String::Param aPrefName,
                               bool aFlush,
                               int32_t aType,
                               bool aBoolVal,
                               int32_t aIntVal,
                               jni::String::Param aStrVal)
    {
        nsresult rv = NS_ERROR_FAILURE;

        switch (aType) {
            case widget::PrefsHelper::PREF_BOOL:
                rv = aVariant->SetAsBool(aBoolVal);
                break;
            case widget::PrefsHelper::PREF_INT:
                rv = aVariant->SetAsInt32(aIntVal);
                break;
            case widget::PrefsHelper::PREF_STRING:
                rv = aVariant->SetAsAString(nsString(aStrVal));
                break;
        }

        if (NS_SUCCEEDED(rv)) {
            rv = aObsServ->NotifyObservers(aVariant, "android-set-pref",
                                           nsString(aPrefName).get());
        }

        uint16_t varType = nsIDataType::VTYPE_EMPTY;
        if (NS_SUCCEEDED(rv)) {
            rv = aVariant->GetDataType(&varType);
        }

        // We use set-to-empty to signal the pref was handled.
        const bool handled = varType == nsIDataType::VTYPE_EMPTY;

        if (NS_SUCCEEDED(rv) && handled && aFlush) {
            rv = Preferences::GetService()->SavePrefFile(nullptr);
        }

        if (NS_SUCCEEDED(rv)) {
            return handled;
        }

        NS_WARNING(nsPrintfCString("Failed to set pref %s",
                                   nsCString(aPrefName).get()).get());
        // Pretend we handled the pref.
        return true;
    }

public:
    static void GetPrefs(const jni::ClassObject::LocalRef& aCls,
                         jni::ObjectArray::Param aPrefNames,
                         jni::Object::Param aPrefHandler)
    {
        nsTArray<jni::Object::LocalRef> nameRefArray(aPrefNames.GetElements());
        nsCOMPtr<nsIObserverService> obsServ;
        nsCOMPtr<nsIWritableVariant> value;
        nsAdoptingString strVal;

        for (jni::Object::LocalRef& nameRef : nameRefArray) {
            jni::String::LocalRef nameStr(mozilla::Move(nameRef));
            const nsCString& name = nsCString(nameStr);

            int32_t type = widget::PrefsHelper::PREF_INVALID;
            bool boolVal = false;
            int32_t intVal = 0;

            switch (Preferences::GetType(name.get())) {
                case nsIPrefBranch::PREF_BOOL:
                    type = widget::PrefsHelper::PREF_BOOL;
                    boolVal = Preferences::GetBool(name.get());
                    break;

                case nsIPrefBranch::PREF_INT:
                    type = widget::PrefsHelper::PREF_INT;
                    intVal = Preferences::GetInt(name.get());
                    break;

                case nsIPrefBranch::PREF_STRING:
                    type = widget::PrefsHelper::PREF_STRING;
                    strVal = Preferences::GetLocalizedString(name.get());
                    if (!strVal) {
                        strVal = Preferences::GetString(name.get());
                    }
                    break;

                default:
                    // Pref not found; try to find it.
                    if (!obsServ) {
                        obsServ = services::GetObserverService();
                        if (!obsServ) {
                            continue;
                        }
                    }
                    if (value) {
                        value->SetAsEmpty();
                    } else {
                        value = new nsVariant();
                    }
                    if (!GetVariantPref(obsServ, value,
                                        aPrefHandler, nameStr)) {
                        NS_WARNING(nsPrintfCString("Failed to get pref %s",
                                                   name.get()).get());
                    }
                    continue;
            }

            const auto& jstrVal = type == widget::PrefsHelper::PREF_STRING ?
                    jni::Param<jni::String>(strVal, aCls.Env()) :
                    jni::Param<jni::String>(nullptr);

            widget::PrefsHelper::CallPrefHandler(
                    aPrefHandler, type, nameStr, boolVal, intVal, jstrVal);
        }

        widget::PrefsHelper::CallPrefHandler(
                aPrefHandler, widget::PrefsHelper::PREF_FINISH,
                nullptr, false, 0, nullptr);
    }

    static void SetPref(jni::String::Param aPrefName,
                        bool aFlush,
                        int32_t aType,
                        bool aBoolVal,
                        int32_t aIntVal,
                        jni::String::Param aStrVal)
    {
        const nsCString& name = nsCString(aPrefName);

        if (Preferences::GetType(name.get()) == nsIPrefBranch::PREF_INVALID) {
            // No pref; try asking first.
            nsCOMPtr<nsIObserverService> obsServ =
                    services::GetObserverService();
            nsCOMPtr<nsIWritableVariant> value = new nsVariant();
            if (obsServ && SetVariantPref(obsServ, value, aPrefName, aFlush,
                                          aType, aBoolVal, aIntVal, aStrVal)) {
                return;
            }
        }

        switch (aType) {
            case widget::PrefsHelper::PREF_BOOL:
                Preferences::SetBool(name.get(), aBoolVal);
                break;
            case widget::PrefsHelper::PREF_INT:
                Preferences::SetInt(name.get(), aIntVal);
                break;
            case widget::PrefsHelper::PREF_STRING:
                Preferences::SetString(name.get(), nsString(aStrVal));
                break;
            default:
                MOZ_ASSERT(false, "Invalid pref type");
        }

        if (aFlush) {
            Preferences::GetService()->SavePrefFile(nullptr);
        }
    }

    static void AddObserver(const jni::ClassObject::LocalRef& aCls,
                            jni::ObjectArray::Param aPrefNames,
                            jni::Object::Param aPrefHandler,
                            jni::ObjectArray::Param aPrefsToObserve)
    {
        // Call observer immediately with existing pref values.
        GetPrefs(aCls, aPrefNames, aPrefHandler);

        if (!aPrefsToObserve) {
            return;
        }

        nsTArray<jni::Object::LocalRef> nameRefArray(
                aPrefsToObserve.GetElements());
        nsAppShell* const appShell = nsAppShell::Get();
        MOZ_ASSERT(appShell);

        for (jni::Object::LocalRef& nameRef : nameRefArray) {
            jni::String::LocalRef nameStr(mozilla::Move(nameRef));
            MOZ_ALWAYS_TRUE(NS_SUCCEEDED(Preferences::AddStrongObserver(
                    appShell, nsCString(nameStr).get())));
        }
    }

    static void RemoveObserver(const jni::ClassObject::LocalRef& aCls,
                               jni::ObjectArray::Param aPrefsToUnobserve)
    {
        nsTArray<jni::Object::LocalRef> nameRefArray(
                aPrefsToUnobserve.GetElements());
        nsAppShell* const appShell = nsAppShell::Get();
        MOZ_ASSERT(appShell);

        for (jni::Object::LocalRef& nameRef : nameRefArray) {
            jni::String::LocalRef nameStr(mozilla::Move(nameRef));
            MOZ_ALWAYS_TRUE(NS_SUCCEEDED(Preferences::RemoveObserver(
                    appShell, nsCString(nameStr).get())));
        }
    }

    static void OnPrefChange(const char16_t* aData)
    {
        const nsCString& name = NS_LossyConvertUTF16toASCII(aData);

        int32_t type = -1;
        bool boolVal = false;
        int32_t intVal = false;
        nsAdoptingString strVal;

        switch (Preferences::GetType(name.get())) {
            case nsIPrefBranch::PREF_BOOL:
                type = widget::PrefsHelper::PREF_BOOL;
                boolVal = Preferences::GetBool(name.get());
                break;
            case nsIPrefBranch::PREF_INT:
                type = widget::PrefsHelper::PREF_INT;
                intVal = Preferences::GetInt(name.get());
                break;
            case nsIPrefBranch::PREF_STRING:
                type = widget::PrefsHelper::PREF_STRING;
                strVal = Preferences::GetLocalizedString(name.get());
                if (!strVal) {
                    strVal = Preferences::GetString(name.get());
                }
                break;
            default:
                NS_WARNING(nsPrintfCString("Invalid pref %s",
                                           name.get()).get());
                return;
        }

        const auto& jstrVal = type == widget::PrefsHelper::PREF_STRING ?
                jni::Param<jni::String>(strVal) :
                jni::Param<jni::String>(nullptr);

        widget::PrefsHelper::OnPrefChange(name, type, boolVal, intVal, jstrVal);
    }
};

} // namespace

#endif // PrefsHelper_h
