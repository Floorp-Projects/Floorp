/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UntrustedModules.h"

#include "core/TelemetryCommon.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/WinDllServices.h"
#include "nsLocalFile.h"
#include "nsPrintfCString.h"
#include "nsProxyRelease.h"
#include "nsXPCOMCIDInternal.h"

namespace mozilla {
namespace Telemetry {

static const int32_t kUntrustedModuleLoadEventsTelemetryVersion = 1;

/**
 * Limits the length of a string by removing the middle of the string, replacing
 * with ellipses.
 * e.g. LimitStringLength("hello world", 6) would result in "he...d"
 *
 * @param  aStr            [in,out] The string to transform
 * @param  aMaxFieldLength [in] The maximum length of the resulting string.
 *                         this must be long enough to hold the ellipses.
 */
static void LimitStringLength(nsAString& aStr, size_t aMaxFieldLength) {
  if (aStr.Length() <= aMaxFieldLength) {
    return;
  }

  NS_NAMED_LITERAL_STRING(kEllipses, "...");

  MOZ_ASSERT(aMaxFieldLength >= kEllipses.Length());
  size_t cutPos = (aMaxFieldLength - kEllipses.Length()) / 2;
  size_t rightLen = aMaxFieldLength - kEllipses.Length() - cutPos;
  size_t cutLen = aStr.Length() - (cutPos + rightLen);

  aStr.Replace(cutPos, cutLen, kEllipses);
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
                                       size_t aMaxFieldLength = 260) {
  JS::RootedValue jsval(cx);
  nsAutoString shortVal(aVal);
  LimitStringLength(shortVal, aMaxFieldLength);
  jsval.setString(Common::ToJSString(cx, shortVal));
  return JS_DefineProperty(cx, aObj, aName, jsval, JSPROP_ENUMERATE);
};

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
template <typename T, size_t N, typename AllocPolicy, typename Converter>
static bool VectorToJSArray(JSContext* cx, JS::MutableHandleObject aRet,
                            const Vector<T, N, AllocPolicy>& aContainer,
                            Converter&& aElementConverter) {
  JS::RootedObject arr(cx, JS_NewArrayObject(cx, 0));
  if (!arr) {
    return false;
  }

  for (size_t i = 0; i < aContainer.length(); ++i) {
    JS::RootedValue jsel(cx);
    if (!aElementConverter(cx, &jsel, aContainer[i])) {
      return false;
    }
    if (!JS_DefineElement(cx, arr, i, jsel, JSPROP_ENUMERATE)) {
      return false;
    }
  }

  aRet.set(arr);
  return true;
}

/**
 * Converts a ModuleLoadEvent::ModuleInfo to a JS object.
 *
 * @param  cx       [in] The JS context.
 * @param  aRet     [out] This gets assigned to the newly created object.
 * @param  aModInfo [in] The source object to convert.
 * @return true if aRet was successfully assigned.
 */
static bool ModuleInfoToJSObj(JSContext* cx, JS::MutableHandleObject aRet,
                              const ModuleLoadEvent::ModuleInfo& aModInfo) {
  JS::RootedObject modObj(cx, JS_NewObject(cx, nullptr));
  if (!modObj) {
    return false;
  }

  JS::RootedValue jsval(cx);

  nsPrintfCString strBaseAddress("0x%p", (void*)aModInfo.mBase);
  jsval.setString(Common::ToJSString(cx, strBaseAddress));
  if (!JS_DefineProperty(cx, modObj, "baseAddress", jsval, JSPROP_ENUMERATE)) {
    return false;
  }

  jsval.setString(Common::ToJSString(cx, aModInfo.mFileVersion));
  if (!JS_DefineProperty(cx, modObj, "fileVersion", jsval, JSPROP_ENUMERATE)) {
    return false;
  }

  if (!AddLengthLimitedStringProp(cx, modObj, "loaderName",
                                  aModInfo.mLdrName)) {
    return false;
  }

  if (!AddLengthLimitedStringProp(cx, modObj, "moduleName",
                                  aModInfo.mFilePathClean)) {
    return false;
  }

  if (aModInfo.mLoadDurationMS.isSome()) {
    jsval.setNumber(aModInfo.mLoadDurationMS.value());
    if (!JS_DefineProperty(cx, modObj, "loadDurationMS", jsval,
                           JSPROP_ENUMERATE)) {
      return false;
    }
  }

  jsval.setNumber((uint32_t)aModInfo.mTrustFlags);
  if (!JS_DefineProperty(cx, modObj, "moduleTrustFlags", jsval,
                         JSPROP_ENUMERATE)) {
    return false;
  }

  aRet.set(modObj);
  return true;
}

/**
 * Converts a ModuleLoadEvent object to a Javascript array
 *
 * @param  cx     [in] The JS context
 * @param  aRet   [out] Handle that receives the resulting array object
 * @param  aEvent [in] The event to convert from
 * @return true upon success
 */
static bool ModuleLoadEventToJSArray(JSContext* cx, JS::MutableHandleValue aRet,
                                     const ModuleLoadEvent& aEvent) {
  MOZ_ASSERT(NS_IsMainThread());

  JS::RootedValue jsval(cx);
  JS::RootedObject eObj(cx, JS_NewObject(cx, nullptr));
  if (!eObj) {
    return false;
  }

  jsval.setNumber((uint32_t)aEvent.mThreadID);
  if (!JS_DefineProperty(cx, eObj, "threadID", jsval, JSPROP_ENUMERATE)) {
    return false;
  }

  jsval.setBoolean(aEvent.mIsStartup);
  if (!JS_DefineProperty(cx, eObj, "isStartup", jsval, JSPROP_ENUMERATE)) {
    return false;
  }

  // Javascript doesn't like 64-bit integers; convert to double.
  jsval.setNumber((double)aEvent.mProcessUptimeMS);
  if (!JS_DefineProperty(cx, eObj, "processUptimeMS", jsval,
                         JSPROP_ENUMERATE)) {
    return false;
  }

  // This function should always get called on the main thread
  if (::GetCurrentThreadId() == aEvent.mThreadID) {
    jsval.setString(Common::ToJSString(cx, NS_LITERAL_STRING("Main Thread")));
  } else {
    jsval.setString(Common::ToJSString(cx, aEvent.mThreadName));
  }
  if (!JS_DefineProperty(cx, eObj, "threadName", jsval, JSPROP_ENUMERATE)) {
    return false;
  }

  JS::RootedObject modulesArray(cx);
  bool ok = VectorToJSArray(cx, &modulesArray, aEvent.mModules,
                            [](JSContext* cx, JS::MutableHandleValue aRet,
                               const ModuleLoadEvent::ModuleInfo& aModInfo) {
                              JS::RootedObject obj(cx);
                              if (!ModuleInfoToJSObj(cx, &obj, aModInfo)) {
                                return false;
                              }
                              aRet.setObject(*obj);
                              return true;
                            });
  if (!ok) {
    return false;
  }

  if (!JS_DefineProperty(cx, eObj, "modules", modulesArray, JSPROP_ENUMERATE)) {
    return false;
  }

  aRet.setObject(*eObj);
  return true;
}

/**
 * Converts a UntrustedModuleLoadTelemetryData to a JS object.
 *
 * @param  aData [in] The source object to convert.
 * @param  cx    [in] The JS context.
 * @param  aRet  [out] This gets assigned to the newly created object.
 * @return nsresult
 */
nsresult GetUntrustedModuleLoadEventsJSValue(
    const UntrustedModuleLoadTelemetryData& aData, JSContext* cx,
    JS::MutableHandle<JS::Value> aRet) {
  if (aData.mEvents.empty()) {
    aRet.setNull();
    return NS_OK;
  }

  JS::RootedValue jsval(cx);
  JS::RootedObject mainObj(cx, JS_NewObject(cx, nullptr));
  if (!mainObj) {
    return NS_ERROR_FAILURE;
  }

  jsval.setNumber((uint32_t)aData.mErrorModules);
  if (!JS_DefineProperty(cx, mainObj, "errorModules", jsval,
                         JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  jsval.setNumber((uint32_t)kUntrustedModuleLoadEventsTelemetryVersion);
  if (!JS_DefineProperty(cx, mainObj, "structVersion", jsval,
                         JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  if (aData.mXULLoadDurationMS.isSome()) {
    jsval.setNumber(aData.mXULLoadDurationMS.value());
    if (!JS_DefineProperty(cx, mainObj, "xulLoadDurationMS", jsval,
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  JS::RootedObject eventsArray(cx);
  if (!VectorToJSArray(cx, &eventsArray, aData.mEvents,
                       &ModuleLoadEventToJSArray)) {
    return NS_ERROR_FAILURE;
  }

  if (!JS_DefineProperty(cx, mainObj, "events", eventsArray,
                         JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedObject combinedStacksObj(cx,
                                     CreateJSStackObject(cx, aData.mStacks));
  if (!combinedStacksObj) {
    return NS_ERROR_FAILURE;
  }

  if (!JS_DefineProperty(cx, mainObj, "combinedStacks", combinedStacksObj,
                         JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  aRet.setObject(*mainObj);

  return NS_OK;
}

class GetUntrustedModulesMainThreadRunnable final : public Runnable {
  nsMainThreadPtrHandle<dom::Promise> mPromise;
  bool mDataOK;
  UntrustedModuleLoadTelemetryData mData;
  nsCOMPtr<nsIThread> mWorkerThread;

 public:
  GetUntrustedModulesMainThreadRunnable(
      const nsMainThreadPtrHandle<dom::Promise>& aPromise, bool aDataOK,
      UntrustedModuleLoadTelemetryData&& aData)
      : Runnable("GetUntrustedModulesMainThreadRunnable"),
        mPromise(aPromise),
        mDataOK(aDataOK),
        mData(std::move(aData)),
        mWorkerThread(do_GetCurrentThread()) {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    mWorkerThread->Shutdown();

    dom::AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(mPromise->GetGlobalObject()))) {
      mPromise->MaybeReject(NS_ERROR_FAILURE);
      return NS_OK;
    }

    if (!mDataOK) {
      mPromise->MaybeReject(NS_ERROR_FAILURE);
      return NS_OK;
    }

    JSContext* cx = jsapi.cx();
    JS::RootedValue jsval(cx);

    nsresult rv = GetUntrustedModuleLoadEventsJSValue(mData, cx, &jsval);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeReject(rv);
      return NS_OK;
    }

    mPromise->MaybeResolve(jsval);
    return NS_OK;
  }
};

class GetUntrustedModulesTelemetryDataRunnable final : public Runnable {
  nsMainThreadPtrHandle<dom::Promise> mPromise;

 public:
  explicit GetUntrustedModulesTelemetryDataRunnable(
      const nsMainThreadPtrHandle<dom::Promise>& aPromise)
      : Runnable("GetUntrustedModulesTelemetryDataRunnable"),
        mPromise(aPromise) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(!NS_IsMainThread());
    RefPtr<DllServices> dllSvc(DllServices::Get());
    UntrustedModuleLoadTelemetryData data;
    bool ok = dllSvc->GetUntrustedModuleTelemetryData(data);

    // Dispatch back to the main thread for remaining JS processing.
    return NS_DispatchToMainThread(new GetUntrustedModulesMainThreadRunnable(
        mPromise, ok, std::move(data)));
  }
};

nsresult GetUntrustedModuleLoadEvents(JSContext* cx, dom::Promise** aPromise) {
  // Create a promise using global context.
  nsIGlobalObject* global = xpc::CurrentNativeGlobal(cx);
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<dom::Promise> promise = dom::Promise::Create(global, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  // Create a worker thread to perform the heavy work.
  nsCOMPtr<nsIThread> workThread;
  nsresult rv = NS_NewNamedThread("UntrustedDLLs", getter_AddRefs(workThread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_FAILURE);
    return NS_OK;
  }

  // In order to pass the promise through the worker thread to the main thread,
  // this is needed.
  nsMainThreadPtrHandle<dom::Promise> mainThreadPromise(
      new nsMainThreadPtrHolder<dom::Promise>(
          "Telemetry::UntrustedModuleLoadEvents::Promise", promise));

  nsCOMPtr<nsIRunnable> runnable =
      new GetUntrustedModulesTelemetryDataRunnable(mainThreadPromise);
  promise.forget(aPromise);

  return workThread->Dispatch(runnable.forget(),
                              nsIEventTarget::DISPATCH_NORMAL);
}

}  // namespace Telemetry
}  // namespace mozilla
