/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UntrustedModules.h"

#include "core/TelemetryCommon.h"
#include "js/Array.h"  // JS::NewArrayObject
#include "mozilla/dom/ContentParent.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RDDChild.h"
#include "mozilla/RDDProcessManager.h"
#include "mozilla/UntrustedModulesProcessor.h"
#include "mozilla/WinDllServices.h"
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsISupportsImpl.h"
#include "nsLocalFile.h"
#include "nsProxyRelease.h"
#include "nsUnicharUtils.h"
#include "nsXULAppAPI.h"

namespace {
using IndexMap = nsDataHashtable<nsStringHashKey, uint32_t>;
}  // anonymous namespace

namespace mozilla {
namespace Telemetry {

static const uint32_t kThirdPartyModulesPingVersion = 1;
static const uint32_t kMaxModulesArrayLen = 100;

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
  uint16_t major, minor, patch, build;

  Tie(major, minor, patch, build) = aVersion.AsTuple();

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
                            const RefPtr<ModuleRecord>& aModule) {
  if (!aModule) {
    return false;
  }

  JS::RootedObject obj(aCx, JS_NewPlainObject(aCx));
  if (!obj) {
    return false;
  }

  if (!AddLengthLimitedStringProp(aCx, obj, "resolvedDllName",
                                  aModule->mSanitizedDllName)) {
    return false;
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

static bool SerializeEvent(JSContext* aCx, JS::MutableHandleValue aElement,
                           const ProcessedModuleLoadEvent& aEvent,
                           const IndexMap& aModuleIndices) {
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

  aElement.setObject(*obj);

  return true;
}

static nsresult GetPerProcObject(JSContext* aCx, const IndexMap& aModuleIndices,
                                 const UntrustedModulesData& aData,
                                 JS::MutableHandleObject aObj) {
  nsDependentCString strProcType;
  if (aData.mProcessType == GeckoProcessType_Default) {
    strProcType.Rebind("browser"_ns, 0);
  } else {
    strProcType.Rebind(XRE_GeckoProcessTypeToString(aData.mProcessType));
  }

  JS::RootedValue jsProcType(aCx);
  jsProcType.setString(Common::ToJSString(aCx, strProcType));
  if (!JS_DefineProperty(aCx, aObj, "processType", jsProcType,
                         JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedValue jsElapsed(aCx);
  jsElapsed.setNumber(aData.mElapsed.ToSecondsSigDigits());
  if (!JS_DefineProperty(aCx, aObj, "elapsed", jsElapsed, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  if (aData.mXULLoadDurationMS.isSome()) {
    JS::RootedValue jsXulLoadDurationMS(aCx);
    jsXulLoadDurationMS.setNumber(aData.mXULLoadDurationMS.value());
    if (!JS_DefineProperty(aCx, aObj, "xulLoadDurationMS", jsXulLoadDurationMS,
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  JS::RootedValue jsSanitizationFailures(aCx);
  jsSanitizationFailures.setNumber(aData.mSanitizationFailures);
  if (!JS_DefineProperty(aCx, aObj, "sanitizationFailures",
                         jsSanitizationFailures, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedValue jsTrustTestFailures(aCx);
  jsTrustTestFailures.setNumber(aData.mTrustTestFailures);
  if (!JS_DefineProperty(aCx, aObj, "trustTestFailures", jsTrustTestFailures,
                         JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedObject eventsArray(aCx);
  if (!VectorToJSArray(aCx, &eventsArray, aData.mEvents, &SerializeEvent,
                       aModuleIndices)) {
    return NS_ERROR_FAILURE;
  }

  if (!JS_DefineProperty(aCx, aObj, "events", eventsArray, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedObject combinedStacksObj(aCx,
                                     CreateJSStackObject(aCx, aData.mStacks));
  if (!combinedStacksObj) {
    return NS_ERROR_FAILURE;
  }

  if (!JS_DefineProperty(aCx, aObj, "combinedStacks", combinedStacksObj,
                         JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

/**
 * Converts a UntrustedModulesData to a JS object.
 *
 * @param  aData [in] The source objects to convert.
 * @param  aCx   [in] The JS context.
 * @param  aRet  [out] This gets assigned to the newly created object.
 * @return nsresult
 */
static nsresult GetUntrustedModuleLoadEventsJSValue(
    const Vector<UntrustedModulesData>& aData, JSContext* aCx,
    JS::MutableHandleValue aRet) {
  if (aData.empty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  JS::RootedObject mainObj(aCx, JS_NewPlainObject(aCx));
  if (!mainObj) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedValue jsVersion(aCx);
  jsVersion.setNumber(kThirdPartyModulesPingVersion);
  if (!JS_DefineProperty(aCx, mainObj, "structVersion", jsVersion,
                         JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  IndexMap indexMap;
  uint32_t curModulesArrayIdx = 0;

  JS::RootedObject modulesArray(aCx, JS::NewArrayObject(aCx, 0));
  if (!modulesArray) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedObject perProcObjContainer(aCx, JS_NewPlainObject(aCx));
  if (!perProcObjContainer) {
    return NS_ERROR_FAILURE;
  }

  for (auto&& procData : aData) {
    // Serialize each entry in the modules hashtable out to the "modules" array
    // and store the indices in |indexMap|
    for (auto iter = procData.mModules.ConstIter(); !iter.Done(); iter.Next()) {
      auto addPtr = indexMap.LookupForAdd(iter.Key());
      if (!addPtr) {
        addPtr.OrInsert([curModulesArrayIdx]() { return curModulesArrayIdx; });

        JS::RootedValue jsModule(aCx);
        if (!SerializeModule(aCx, &jsModule, iter.Data()) ||
            !JS_DefineElement(aCx, modulesArray, curModulesArrayIdx, jsModule,
                              JSPROP_ENUMERATE)) {
          return NS_ERROR_FAILURE;
        }

        ++curModulesArrayIdx;
      }
    }

    if (curModulesArrayIdx >= kMaxModulesArrayLen) {
      return NS_ERROR_CANNOT_CONVERT_DATA;
    }

    JS::RootedObject perProcObj(aCx, JS_NewPlainObject(aCx));
    if (!perProcObj) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = GetPerProcObject(aCx, indexMap, procData, &perProcObj);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsAutoCString strPid;
    strPid.AppendLiteral("0x");
    strPid.AppendInt(static_cast<uint32_t>(procData.mPid), 16);

    JS::RootedValue jsPerProcObjValue(aCx);
    jsPerProcObjValue.setObject(*perProcObj);
    if (!JS_DefineProperty(aCx, perProcObjContainer, strPid.get(),
                           jsPerProcObjValue, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  JS::RootedValue jsModulesArrayValue(aCx);
  jsModulesArrayValue.setObject(*modulesArray);
  if (!JS_DefineProperty(aCx, mainObj, "modules", jsModulesArrayValue,
                         JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedValue jsPerProcObjContainerValue(aCx);
  jsPerProcObjContainerValue.setObject(*perProcObjContainer);
  if (!JS_DefineProperty(aCx, mainObj, "processes", jsPerProcObjContainerValue,
                         JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  aRet.setObject(*mainObj);
  return NS_OK;
}

static void Serialize(Vector<UntrustedModulesData>&& aData,
                      RefPtr<dom::Promise>&& aPromise) {
  MOZ_ASSERT(NS_IsMainThread());

  dom::AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(aPromise->GetGlobalObject()))) {
    aPromise->MaybeReject(NS_ERROR_FAILURE);
    return;
  }

  if (aData.empty()) {
    aPromise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  JSContext* cx = jsapi.cx();
  JS::RootedValue jsval(cx);

  nsresult rv = GetUntrustedModuleLoadEventsJSValue(aData, cx, &jsval);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aPromise->MaybeReject(rv);
    return;
  }

  aPromise->MaybeResolve(jsval);
}

using UntrustedModulesIpcPromise =
    MozPromise<Maybe<UntrustedModulesData>, ipc::ResponseRejectReason, true>;

using MultiGetUntrustedModulesPromise =
    MozPromise<Vector<UntrustedModulesData>, nsresult, true>;

class MOZ_HEAP_CLASS MultiGetUntrustedModulesData final {
 public:
  MultiGetUntrustedModulesData()
      : mPromise(new MultiGetUntrustedModulesPromise::Private(__func__)),
        mNumPending(0) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MultiGetUntrustedModulesData)

  RefPtr<MultiGetUntrustedModulesPromise> GetUntrustedModuleLoadEvents();

  MultiGetUntrustedModulesData(const MultiGetUntrustedModulesData&) = delete;
  MultiGetUntrustedModulesData(MultiGetUntrustedModulesData&&) = delete;
  MultiGetUntrustedModulesData& operator=(const MultiGetUntrustedModulesData&) =
      delete;
  MultiGetUntrustedModulesData& operator=(MultiGetUntrustedModulesData&&) =
      delete;

 private:
  ~MultiGetUntrustedModulesData() = default;

  void AddPending(RefPtr<UntrustedModulesPromise>&& aNewPending) {
    MOZ_ASSERT(NS_IsMainThread());

    ++mNumPending;

    RefPtr<MultiGetUntrustedModulesData> self(this);
    aNewPending->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [self](Maybe<UntrustedModulesData>&& aResult) {
          self->OnCompletion(std::move(aResult));
        },
        [self](nsresult aReason) { self->OnCompletion(); });
  }

  void AddPending(RefPtr<UntrustedModulesIpcPromise>&& aNewPending) {
    MOZ_ASSERT(NS_IsMainThread());

    ++mNumPending;

    RefPtr<MultiGetUntrustedModulesData> self(this);
    aNewPending->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [self](Maybe<UntrustedModulesData>&& aResult) {
          self->OnCompletion(std::move(aResult));
        },
        [self](ipc::ResponseRejectReason&& aReason) { self->OnCompletion(); });
  }

  void OnCompletion() {
    MOZ_ASSERT(NS_IsMainThread() && mNumPending > 0);

    --mNumPending;
    if (mNumPending) {
      return;
    }

    if (mResults.empty()) {
      mPromise->Reject(NS_ERROR_NOT_AVAILABLE, __func__);
      return;
    }

    mPromise->Resolve(std::move(mResults), __func__);
  }

  void OnCompletion(Maybe<UntrustedModulesData>&& aResult) {
    MOZ_ASSERT(NS_IsMainThread());

    if (aResult.isSome()) {
      Unused << mResults.emplaceBack(std::move(aResult.ref()));
    }

    OnCompletion();
  }

 private:
  RefPtr<MultiGetUntrustedModulesPromise::Private> mPromise;
  Vector<UntrustedModulesData> mResults;
  size_t mNumPending;
};

RefPtr<MultiGetUntrustedModulesPromise>
MultiGetUntrustedModulesData::GetUntrustedModuleLoadEvents() {
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  // Parent process
  RefPtr<DllServices> dllSvc(DllServices::Get());
  AddPending(dllSvc->GetUntrustedModulesData());

  // Child processes
  nsTArray<dom::ContentParent*> contentParents;
  dom::ContentParent::GetAll(contentParents);
  for (auto&& contentParent : contentParents) {
    AddPending(contentParent->SendGetUntrustedModulesData());
  }

  if (RDDProcessManager* rddMgr = RDDProcessManager::Get()) {
    if (RDDChild* rddChild = rddMgr->GetRDDChild()) {
      AddPending(rddChild->SendGetUntrustedModulesData());
    }
  }

  Unused << mResults.reserve(mNumPending);

  return mPromise;
}

nsresult GetUntrustedModuleLoadEvents(JSContext* cx, dom::Promise** aPromise) {
  // Create a promise using global context.
  nsIGlobalObject* global = xpc::CurrentNativeGlobal(cx);
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<dom::Promise> promise(dom::Promise::Create(global, result));
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  RefPtr<MultiGetUntrustedModulesData> multi(
      new MultiGetUntrustedModulesData());

  multi->GetUntrustedModuleLoadEvents()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [promise](Vector<UntrustedModulesData>&& aData) mutable {
        Serialize(std::move(aData), std::move(promise));
      },
      [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
  return NS_OK;
}

}  // namespace Telemetry
}  // namespace mozilla
