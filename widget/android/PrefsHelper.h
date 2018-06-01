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

namespace mozilla {

class PrefsHelper
    : public java::PrefsHelper::Natives<PrefsHelper>
{
    PrefsHelper() = delete;

    static bool GetVariantPref(nsIObserverService* aObsServ,
                               nsIWritableVariant* aVariant,
                               jni::Object::Param aPrefHandler,
                               const jni::String::LocalRef& aPrefName)
    {
        if (NS_FAILED(aObsServ->NotifyObservers(aVariant, "android-get-pref",
                                                aPrefName->ToString().get()))) {
            return false;
        }

        uint16_t varType = nsIDataType::VTYPE_EMPTY;
        if (NS_FAILED(aVariant->GetDataType(&varType))) {
            return false;
        }

        int32_t type = java::PrefsHelper::PREF_INVALID;
        bool boolVal = false;
        int32_t intVal = 0;
        nsAutoString strVal;

        switch (varType) {
            case nsIDataType::VTYPE_BOOL:
                type = java::PrefsHelper::PREF_BOOL;
                if (NS_FAILED(aVariant->GetAsBool(&boolVal))) {
                    return false;
                }
                break;
            case nsIDataType::VTYPE_INT32:
                type = java::PrefsHelper::PREF_INT;
                if (NS_FAILED(aVariant->GetAsInt32(&intVal))) {
                    return false;
                }
                break;
            case nsIDataType::VTYPE_ASTRING:
                type = java::PrefsHelper::PREF_STRING;
                if (NS_FAILED(aVariant->GetAsAString(strVal))) {
                    return false;
                }
                break;
            default:
                return false;
        }

        jni::StringParam jstrVal(type == java::PrefsHelper::PREF_STRING ?
                jni::StringParam(strVal, aPrefName.Env()) :
                jni::StringParam(nullptr));

        if (aPrefHandler) {
            java::PrefsHelper::CallPrefHandler(
                    aPrefHandler, type, aPrefName,
                    boolVal, intVal, jstrVal);
        } else {
            java::PrefsHelper::OnPrefChange(
                    aPrefName, type, boolVal, intVal, jstrVal);
        }
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
            case java::PrefsHelper::PREF_BOOL:
                rv = aVariant->SetAsBool(aBoolVal);
                break;
            case java::PrefsHelper::PREF_INT:
                rv = aVariant->SetAsInt32(aIntVal);
                break;
            case java::PrefsHelper::PREF_STRING:
                rv = aVariant->SetAsAString(aStrVal->ToString());
                break;
        }

        if (NS_SUCCEEDED(rv)) {
            rv = aObsServ->NotifyObservers(aVariant, "android-set-pref",
                                           aPrefName->ToString().get());
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
                                   aPrefName->ToCString().get()).get());
        // Pretend we handled the pref.
        return true;
    }

public:
    static void GetPrefs(const jni::Class::LocalRef& aCls,
                         jni::ObjectArray::Param aPrefNames,
                         jni::Object::Param aPrefHandler)
    {
        nsTArray<jni::Object::LocalRef> nameRefArray(aPrefNames->GetElements());
        nsCOMPtr<nsIObserverService> obsServ;
        nsCOMPtr<nsIWritableVariant> value;
        nsAutoString strVal;

        for (jni::Object::LocalRef& nameRef : nameRefArray) {
            jni::String::LocalRef nameStr(std::move(nameRef));
            const nsCString& name = nameStr->ToCString();

            int32_t type = java::PrefsHelper::PREF_INVALID;
            bool boolVal = false;
            int32_t intVal = 0;
            strVal.Truncate();

            switch (Preferences::GetType(name.get())) {
                case nsIPrefBranch::PREF_BOOL:
                    type = java::PrefsHelper::PREF_BOOL;
                    boolVal = Preferences::GetBool(name.get());
                    break;

                case nsIPrefBranch::PREF_INT:
                    type = java::PrefsHelper::PREF_INT;
                    intVal = Preferences::GetInt(name.get());
                    break;

                case nsIPrefBranch::PREF_STRING: {
                    type = java::PrefsHelper::PREF_STRING;
                    nsresult rv =
                      Preferences::GetLocalizedString(name.get(), strVal);
                    if (NS_FAILED(rv)) {
                        Preferences::GetString(name.get(), strVal);
                    }
                    break;
                }
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

            java::PrefsHelper::CallPrefHandler(
                    aPrefHandler, type, nameStr, boolVal, intVal,
                    jni::StringParam(type == java::PrefsHelper::PREF_STRING ?
                        jni::StringParam(strVal, aCls.Env()) :
                        jni::StringParam(nullptr)));
        }

        java::PrefsHelper::CallPrefHandler(
                aPrefHandler, java::PrefsHelper::PREF_FINISH,
                nullptr, false, 0, nullptr);
    }

    static void SetPref(jni::String::Param aPrefName,
                        bool aFlush,
                        int32_t aType,
                        bool aBoolVal,
                        int32_t aIntVal,
                        jni::String::Param aStrVal)
    {
        const nsCString& name = aPrefName->ToCString();

        if (Preferences::GetType(name.get()) == nsIPrefBranch::PREF_INVALID) {
            // No pref; try asking first.
            nsCOMPtr<nsIObserverService> obsServ =
                    services::GetObserverService();
            nsCOMPtr<nsIWritableVariant> value = new nsVariant();
            if (obsServ && SetVariantPref(obsServ, value, aPrefName, aFlush,
                                          aType, aBoolVal, aIntVal, aStrVal)) {
                // The "pref" has changed; send a notification.
                GetVariantPref(obsServ, value, nullptr,
                               jni::String::LocalRef(aPrefName));
                return;
            }
        }

        switch (aType) {
            case java::PrefsHelper::PREF_BOOL:
                Preferences::SetBool(name.get(), aBoolVal);
                break;
            case java::PrefsHelper::PREF_INT:
                Preferences::SetInt(name.get(), aIntVal);
                break;
            case java::PrefsHelper::PREF_STRING:
                Preferences::SetString(name.get(), aStrVal->ToString());
                break;
            default:
                MOZ_ASSERT(false, "Invalid pref type");
        }

        if (aFlush) {
            Preferences::GetService()->SavePrefFile(nullptr);
        }
    }

    static void AddObserver(const jni::Class::LocalRef& aCls,
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
                aPrefsToObserve->GetElements());
        nsAppShell* const appShell = nsAppShell::Get();
        MOZ_ASSERT(appShell);

        for (jni::Object::LocalRef& nameRef : nameRefArray) {
            jni::String::LocalRef nameStr(std::move(nameRef));
            MOZ_ALWAYS_SUCCEEDS(Preferences::AddStrongObserver(
                    appShell, nameStr->ToCString().get()));
        }
    }

    static void RemoveObserver(const jni::Class::LocalRef& aCls,
                               jni::ObjectArray::Param aPrefsToUnobserve)
    {
        nsTArray<jni::Object::LocalRef> nameRefArray(
                aPrefsToUnobserve->GetElements());
        nsAppShell* const appShell = nsAppShell::Get();
        MOZ_ASSERT(appShell);

        for (jni::Object::LocalRef& nameRef : nameRefArray) {
            jni::String::LocalRef nameStr(std::move(nameRef));
            MOZ_ALWAYS_SUCCEEDS(Preferences::RemoveObserver(
                    appShell, nameStr->ToCString().get()));
        }
    }

    static void OnPrefChange(const char16_t* aData)
    {
        const nsCString& name = NS_LossyConvertUTF16toASCII(aData);

        int32_t type = -1;
        bool boolVal = false;
        int32_t intVal = false;
        nsAutoString strVal;

        switch (Preferences::GetType(name.get())) {
            case nsIPrefBranch::PREF_BOOL:
                type = java::PrefsHelper::PREF_BOOL;
                boolVal = Preferences::GetBool(name.get());
                break;
            case nsIPrefBranch::PREF_INT:
                type = java::PrefsHelper::PREF_INT;
                intVal = Preferences::GetInt(name.get());
                break;
            case nsIPrefBranch::PREF_STRING: {
                type = java::PrefsHelper::PREF_STRING;
                nsresult rv =
                  Preferences::GetLocalizedString(name.get(), strVal);
                if (NS_FAILED(rv)) {
                    Preferences::GetString(name.get(), strVal);
                }
                break;
            }
            default:
                NS_WARNING(nsPrintfCString("Invalid pref %s",
                                           name.get()).get());
                return;
        }

        java::PrefsHelper::OnPrefChange(
                name, type, boolVal, intVal,
                jni::StringParam(type == java::PrefsHelper::PREF_STRING ?
                    jni::StringParam(strVal) : jni::StringParam(nullptr)));
    }
};

} // namespace

#endif // PrefsHelper_h
