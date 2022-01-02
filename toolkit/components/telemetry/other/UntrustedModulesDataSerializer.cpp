/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UntrustedModulesDataSerializer.h"

#include "core/TelemetryCommon.h"
#include "js/Array.h"               // JS::NewArrayObject
#include "js/PropertyAndElement.h"  // JS_DefineElement, JS_DefineProperty, JS_GetProperty
#include "jsapi.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsITelemetry.h"
#include "nsUnicharUtils.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace Telemetry {

static const uint32_t kThirdPartyModulesPingVersion = 1;

/**
 * Limits the length of a string by removing the middle of the string, replacing
 * with ellipsis.
 * e.g. LimitStringLength("hello world", 6) would result in "he...d"
 *
 * @param  aStr            [in,out] The string to transform
 * @param  aMaxFieldLength [in] The maximum length of the resulting string.
 */
static void LimitStringLength(nsAString& aStr, size_t aMaxFieldLength) {
  if (aStr.Length() <= aMaxFieldLength) {
    return;
  }

  constexpr auto kEllipsis = u"..."_ns;

  if (aMaxFieldLength <= (kEllipsis.Length() + 3)) {
    // An ellipsis is useless in this case, as it would obscure the string to
    // the point that we cannot even determine the string's contents. We might
    // as well just truncate.
    aStr.Truncate(aMaxFieldLength);
    return;
  }

  size_t cutPos = (aMaxFieldLength - kEllipsis.Length()) / 2;
  size_t rightLen = aMaxFieldLength - kEllipsis.Length() - cutPos;
  size_t cutLen = aStr.Length() - (cutPos + rightLen);

  aStr.Replace(cutPos, cutLen, kEllipsis);
}

/**
 * Adds a string property to a JS object, that's limited in length using
 * LimitStringLength().
 *
 * @param  cx              [in] The JS context
 * @param  aObj            [in] The object to add the property to
 * @param  aName           [in] The name of the property to add
 * @param  aVal            [in] The JS value of the resulting property.
 * @param  aMaxFieldLength [in] The maximum length of the value
 *                         (see LimitStringLength())
 * @return true upon success
 */
static bool AddLengthLimitedStringProp(JSContext* cx, JS::HandleObject aObj,
                                       const char* aName, const nsAString& aVal,
                                       size_t aMaxFieldLength = MAX_PATH) {
  JS::RootedValue jsval(cx);
  nsAutoString shortVal(aVal);
  LimitStringLength(shortVal, aMaxFieldLength);
  jsval.setString(Common::ToJSString(cx, shortVal));
  return JS_DefineProperty(cx, aObj, aName, jsval, JSPROP_ENUMERATE);
};

static JSString* ModuleVersionToJSString(JSContext* aCx,
                                         const ModuleVersion& aVersion) {
  auto [major, minor, patch, build] = aVersion.AsTuple();

  constexpr auto dot = u"."_ns;

  nsAutoString strVer;
  strVer.AppendInt(major);
  strVer.Append(dot);
  strVer.AppendInt(minor);
  strVer.Append(dot);
  strVer.AppendInt(patch);
  strVer.Append(dot);
  strVer.AppendInt(build);

  return Common::ToJSString(aCx, strVer);
}

/**
 * Convert the given mozilla::Vector to a JavaScript array.
 *
 * @param  cx                [in] The JS context.
 * @param  aRet              [out] This gets assigned to the newly created
 *                           array object.
 * @param  aContainer        [in] The source array to convert.
 * @param  aElementConverter [in] A callable used to convert each array element
 *                           to a JS element. The form of this function is:
 *                           bool(JSContext *cx,
 *                                JS::MutableHandleValue aRet,
 *                                const ArrayElementT& aElement)
 * @return true if aRet was successfully assigned to the new array object.
 */
template <typename T, size_t N, typename AllocPolicy, typename Converter,
          typename... Args>
static bool VectorToJSArray(JSContext* cx, JS::MutableHandleObject aRet,
                            const Vector<T, N, AllocPolicy>& aContainer,
                            Converter&& aElementConverter, Args&&... aArgs) {
  JS::RootedObject arr(cx, JS::NewArrayObject(cx, 0));
  if (!arr) {
    return false;
  }

  for (size_t i = 0, l = aContainer.length(); i < l; ++i) {
    JS::RootedValue jsel(cx);
    if (!aElementConverter(cx, &jsel, aContainer[i],
                           std::forward<Args>(aArgs)...)) {
      return false;
    }
    if (!JS_DefineElement(cx, arr, i, jsel, JSPROP_ENUMERATE)) {
      return false;
    }
  }

  aRet.set(arr);
  return true;
}

static bool SerializeModule(JSContext* aCx, JS::MutableHandleValue aElement,
                            const RefPtr<ModuleRecord>& aModule,
                            uint32_t aFlags) {
  if (!aModule) {
    return false;
  }

  JS::RootedObject obj(aCx, JS_NewPlainObject(aCx));
  if (!obj) {
    return false;
  }

  if (aFlags & nsITelemetry::INCLUDE_PRIVATE_FIELDS_IN_LOADEVENTS) {
    JS::RootedValue jsFileObj(aCx);
    if (!dom::ToJSValue(aCx, aModule->mResolvedDosName, &jsFileObj) ||
        !JS_DefineProperty(aCx, obj, "dllFile", jsFileObj, JSPROP_ENUMERATE)) {
      return false;
    }
  } else {
    if (!AddLengthLimitedStringProp(aCx, obj, "resolvedDllName",
                                    aModule->mSanitizedDllName)) {
      return false;
    }
  }

  if (aModule->mVersion.isSome()) {
    JS::RootedValue jsModuleVersion(aCx);
    jsModuleVersion.setString(
        ModuleVersionToJSString(aCx, aModule->mVersion.ref()));
    if (!JS_DefineProperty(aCx, obj, "fileVersion", jsModuleVersion,
                           JSPROP_ENUMERATE)) {
      return false;
    }
  }

  if (aModule->mVendorInfo.isSome()) {
    const char* propName;

    const VendorInfo& vendorInfo = aModule->mVendorInfo.ref();
    switch (vendorInfo.mSource) {
      case VendorInfo::Source::Signature:
        propName = "signedBy";
        break;
      case VendorInfo::Source::VersionInfo:
        propName = "companyName";
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown VendorInfo Source!");
        return false;
    }

    MOZ_ASSERT(!vendorInfo.mVendor.IsEmpty());
    if (vendorInfo.mVendor.IsEmpty()) {
      return false;
    }

    if (!AddLengthLimitedStringProp(aCx, obj, propName, vendorInfo.mVendor)) {
      return false;
    }
  }

  JS::RootedValue jsTrustFlags(aCx);
  jsTrustFlags.setNumber(static_cast<uint32_t>(aModule->mTrustFlags));
  if (!JS_DefineProperty(aCx, obj, "trustFlags", jsTrustFlags,
                         JSPROP_ENUMERATE)) {
    return false;
  }

  aElement.setObject(*obj);
  return true;
}

/* static */
bool UntrustedModulesDataSerializer::SerializeEvent(
    JSContext* aCx, JS::MutableHandleValue aElement,
    const ProcessedModuleLoadEvent& aEvent, const IndexMap& aModuleIndices) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aEvent) {
    return false;
  }

  JS::RootedObject obj(aCx, JS_NewPlainObject(aCx));
  if (!obj) {
    return false;
  }

  JS::RootedValue jsProcessUptimeMS(aCx);
  // Javascript doesn't like 64-bit integers; convert to double.
  jsProcessUptimeMS.setNumber(static_cast<double>(aEvent.mProcessUptimeMS));
  if (!JS_DefineProperty(aCx, obj, "processUptimeMS", jsProcessUptimeMS,
                         JSPROP_ENUMERATE)) {
    return false;
  }

  if (aEvent.mLoadDurationMS) {
    JS::RootedValue jsLoadDurationMS(aCx);
    jsLoadDurationMS.setNumber(aEvent.mLoadDurationMS.value());
    if (!JS_DefineProperty(aCx, obj, "loadDurationMS", jsLoadDurationMS,
                           JSPROP_ENUMERATE)) {
      return false;
    }
  }

  JS::RootedValue jsThreadId(aCx);
  jsThreadId.setNumber(static_cast<uint32_t>(aEvent.mThreadId));
  if (!JS_DefineProperty(aCx, obj, "threadID", jsThreadId, JSPROP_ENUMERATE)) {
    return false;
  }

  nsDependentCString effectiveThreadName;
  if (aEvent.mThreadId == ::GetCurrentThreadId()) {
    effectiveThreadName.Rebind("Main Thread"_ns, 0);
  } else {
    effectiveThreadName.Rebind(aEvent.mThreadName, 0);
  }

  if (!effectiveThreadName.IsEmpty()) {
    JS::RootedValue jsThreadName(aCx);
    jsThreadName.setString(Common::ToJSString(aCx, effectiveThreadName));
    if (!JS_DefineProperty(aCx, obj, "threadName", jsThreadName,
                           JSPROP_ENUMERATE)) {
      return false;
    }
  }

  // Don't add this property unless mRequestedDllName differs from
  // the associated module's mSanitizedDllName
  if (!aEvent.mRequestedDllName.IsEmpty() &&
      !aEvent.mRequestedDllName.Equals(aEvent.mModule->mSanitizedDllName,
                                       nsCaseInsensitiveStringComparator)) {
    if (!AddLengthLimitedStringProp(aCx, obj, "requestedDllName",
                                    aEvent.mRequestedDllName)) {
      return false;
    }
  }

  nsAutoString strBaseAddress;
  strBaseAddress.AppendLiteral(u"0x");
  strBaseAddress.AppendInt(aEvent.mBaseAddress, 16);

  JS::RootedValue jsBaseAddress(aCx);
  jsBaseAddress.setString(Common::ToJSString(aCx, strBaseAddress));
  if (!JS_DefineProperty(aCx, obj, "baseAddress", jsBaseAddress,
                         JSPROP_ENUMERATE)) {
    return false;
  }

  uint32_t index;
  if (!aModuleIndices.Get(aEvent.mModule->mResolvedNtName, &index)) {
    return false;
  }

  JS::RootedValue jsModuleIndex(aCx);
  jsModuleIndex.setNumber(index);
  if (!JS_DefineProperty(aCx, obj, "moduleIndex", jsModuleIndex,
                         JSPROP_ENUMERATE)) {
    return false;
  }

  JS::RootedValue jsIsDependent(aCx);
  jsIsDependent.setBoolean(aEvent.mIsDependent);
  if (!JS_DefineProperty(aCx, obj, "isDependent", jsIsDependent,
                         JSPROP_ENUMERATE)) {
    return false;
  }

  JS::RootedValue jsLoadStatus(aCx);
  jsLoadStatus.setNumber(aEvent.mLoadStatus);
  if (!JS_DefineProperty(aCx, obj, "loadStatus", jsLoadStatus,
                         JSPROP_ENUMERATE)) {
    return false;
  }

  aElement.setObject(*obj);

  return true;
}

static nsDependentCString GetProcessTypeString(GeckoProcessType aType) {
  nsDependentCString strProcType;
  if (aType == GeckoProcessType_Default) {
    strProcType.Rebind("browser"_ns, 0);
  } else {
    strProcType.Rebind(XRE_GeckoProcessTypeToString(aType));
  }
  return strProcType;
}

nsresult UntrustedModulesDataSerializer::GetPerProcObject(
    const UntrustedModulesData& aData, JS::MutableHandleObject aObj) {
  JS::RootedValue jsProcType(mCx);
  jsProcType.setString(
      Common::ToJSString(mCx, GetProcessTypeString(aData.mProcessType)));
  if (!JS_DefineProperty(mCx, aObj, "processType", jsProcType,
                         JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedValue jsElapsed(mCx);
  jsElapsed.setNumber(aData.mElapsed.ToSecondsSigDigits());
  if (!JS_DefineProperty(mCx, aObj, "elapsed", jsElapsed, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  if (aData.mXULLoadDurationMS.isSome()) {
    JS::RootedValue jsXulLoadDurationMS(mCx);
    jsXulLoadDurationMS.setNumber(aData.mXULLoadDurationMS.value());
    if (!JS_DefineProperty(mCx, aObj, "xulLoadDurationMS", jsXulLoadDurationMS,
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  JS::RootedValue jsSanitizationFailures(mCx);
  jsSanitizationFailures.setNumber(aData.mSanitizationFailures);
  if (!JS_DefineProperty(mCx, aObj, "sanitizationFailures",
                         jsSanitizationFailures, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedValue jsTrustTestFailures(mCx);
  jsTrustTestFailures.setNumber(aData.mTrustTestFailures);
  if (!JS_DefineProperty(mCx, aObj, "trustTestFailures", jsTrustTestFailures,
                         JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedObject eventsArray(mCx);
  if (!VectorToJSArray(mCx, &eventsArray, aData.mEvents, &SerializeEvent,
                       mIndexMap)) {
    return NS_ERROR_FAILURE;
  }

  if (!JS_DefineProperty(mCx, aObj, "events", eventsArray, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  if (!(mFlags & nsITelemetry::EXCLUDE_STACKINFO_FROM_LOADEVENTS)) {
    JS::RootedObject combinedStacksObj(mCx,
                                       CreateJSStackObject(mCx, aData.mStacks));
    if (!combinedStacksObj) {
      return NS_ERROR_FAILURE;
    }

    if (!JS_DefineProperty(mCx, aObj, "combinedStacks", combinedStacksObj,
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

nsresult UntrustedModulesDataSerializer::AddLoadEvents(
    const Vector<ProcessedModuleLoadEvent>& aEvents,
    JS::MutableHandleObject aPerProcObj) {
  JS::RootedValue eventsArrayVal(mCx);
  if (!JS_GetProperty(mCx, aPerProcObj, "events", &eventsArrayVal) ||
      !eventsArrayVal.isObject()) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedObject eventsArray(mCx, &eventsArrayVal.toObject());
  bool isArray;
  if (!JS::IsArrayObject(mCx, eventsArray, &isArray) && !isArray) {
    return NS_ERROR_FAILURE;
  }

  uint32_t currentPos;
  if (!GetArrayLength(mCx, eventsArray, &currentPos)) {
    return NS_ERROR_FAILURE;
  }

  for (size_t i = 0, l = aEvents.length(); i < l; ++i) {
    JS::RootedValue jsel(mCx);
    if (!SerializeEvent(mCx, &jsel, aEvents[i], mIndexMap) ||
        !JS_DefineElement(mCx, eventsArray, currentPos + i, jsel,
                          JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

nsresult UntrustedModulesDataSerializer::AddSingleData(
    const UntrustedModulesData& aData) {
  // Serialize each entry in the modules hashtable out to the "modules" array
  // and store the indices in |mIndexMap|
  for (const auto& entry : aData.mModules) {
    if (!mIndexMap.WithEntryHandle(entry.GetKey(), [&](auto&& addPtr) {
          if (!addPtr) {
            addPtr.Insert(mCurModulesArrayIdx);

            JS::RootedValue jsModule(mCx);
            if (!SerializeModule(mCx, &jsModule, entry.GetData(), mFlags) ||
                !JS_DefineElement(mCx, mModulesArray, mCurModulesArrayIdx,
                                  jsModule, JSPROP_ENUMERATE)) {
              return false;
            }

            ++mCurModulesArrayIdx;
          }
          return true;
        })) {
      return NS_ERROR_FAILURE;
    }
  }

  if (mCurModulesArrayIdx >= mMaxModulesArrayLen) {
    return NS_ERROR_CANNOT_CONVERT_DATA;
  }

  nsAutoCString strPid;
  strPid.Append(GetProcessTypeString(aData.mProcessType));
  strPid.AppendLiteral(".0x");
  strPid.AppendInt(static_cast<uint32_t>(aData.mPid), 16);

  if (mFlags & nsITelemetry::EXCLUDE_STACKINFO_FROM_LOADEVENTS) {
    JS::RootedValue perProcVal(mCx);
    if (JS_GetProperty(mCx, mPerProcObjContainer, strPid.get(), &perProcVal) &&
        perProcVal.isObject()) {
      // If a corresponding per-proc object already exists in the dictionary,
      // and we skip to serialize CombinedStacks, we can add loading events
      // into the JS object directly.
      JS::RootedObject perProcObj(mCx, &perProcVal.toObject());
      return AddLoadEvents(aData.mEvents, &perProcObj);
    }
  }

  JS::RootedObject perProcObj(mCx, JS_NewPlainObject(mCx));
  if (!perProcObj) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = GetPerProcObject(aData, &perProcObj);
  if (NS_FAILED(rv)) {
    return rv;
  }

  JS::RootedValue jsPerProcObjValue(mCx);
  jsPerProcObjValue.setObject(*perProcObj);
  if (!JS_DefineProperty(mCx, mPerProcObjContainer, strPid.get(),
                         jsPerProcObjValue, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

UntrustedModulesDataSerializer::UntrustedModulesDataSerializer(
    JSContext* aCx, uint32_t aMaxModulesArrayLen, uint32_t aFlags)
    : mCtorResult(NS_ERROR_FAILURE),
      mCx(aCx),
      mMainObj(mCx, JS_NewPlainObject(mCx)),
      mModulesArray(mCx, JS::NewArrayObject(mCx, 0)),
      mPerProcObjContainer(mCx, JS_NewPlainObject(mCx)),
      mMaxModulesArrayLen(aMaxModulesArrayLen),
      mCurModulesArrayIdx(0),
      mFlags(aFlags) {
  if (!mMainObj || !mModulesArray || !mPerProcObjContainer) {
    return;
  }

  JS::RootedValue jsVersion(mCx);
  jsVersion.setNumber(kThirdPartyModulesPingVersion);
  if (!JS_DefineProperty(mCx, mMainObj, "structVersion", jsVersion,
                         JSPROP_ENUMERATE)) {
    return;
  }

  JS::RootedValue jsModulesArrayValue(mCx);
  jsModulesArrayValue.setObject(*mModulesArray);
  if (!JS_DefineProperty(mCx, mMainObj, "modules", jsModulesArrayValue,
                         JSPROP_ENUMERATE)) {
    return;
  }

  JS::RootedValue jsPerProcObjContainerValue(mCx);
  jsPerProcObjContainerValue.setObject(*mPerProcObjContainer);
  if (!JS_DefineProperty(mCx, mMainObj, "processes", jsPerProcObjContainerValue,
                         JSPROP_ENUMERATE)) {
    return;
  }

  mCtorResult = NS_OK;
}

UntrustedModulesDataSerializer::operator bool() const {
  return NS_SUCCEEDED(mCtorResult);
}

void UntrustedModulesDataSerializer::GetObject(JS::MutableHandleValue aRet) {
  aRet.setObject(*mMainObj);
}

nsresult UntrustedModulesDataSerializer::Add(
    const UntrustedModulesBackupData& aData) {
  if (NS_FAILED(mCtorResult)) {
    return mCtorResult;
  }

  for (const RefPtr<UntrustedModulesDataContainer>& container :
       aData.Values()) {
    if (!container) {
      continue;
    }

    nsresult rv = AddSingleData(container->mData);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

}  // namespace Telemetry
}  // namespace mozilla
