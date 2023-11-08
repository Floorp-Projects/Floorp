/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProfiler.h"

#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include "GeckoProfiler.h"
#include "ProfilerControl.h"
#include "ProfilerParent.h"
#include "js/Array.h"  // JS::NewArrayObject
#include "js/JSON.h"
#include "js/PropertyAndElement.h"  // JS_SetElement
#include "js/Value.h"
#include "json/json.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/JSONStringWriteFuncs.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Services.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/Preferences.h"
#include "nsComponentManagerUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadContext.h"
#include "nsIWebNavigation.h"
#include "nsProfilerStartParams.h"
#include "nsProxyRelease.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "platform.h"
#include "shared-libraries.h"
#include "zlib.h"

#ifndef ANDROID
#  include <cstdio>
#else
#  include <android/log.h>
#endif

using namespace mozilla;

using dom::AutoJSAPI;
using dom::Promise;
using std::string;

static constexpr size_t scLengthMax = size_t(JS::MaxStringLength);
// Used when trying to add more JSON data, to account for the extra space needed
// for the log and to close the profile.
static constexpr size_t scLengthAccumulationThreshold = scLengthMax - 16 * 1024;

NS_IMPL_ISUPPORTS(nsProfiler, nsIProfiler)

nsProfiler::nsProfiler() : mGathering(false) {}

nsProfiler::~nsProfiler() {
  if (mSymbolTableThread) {
    mSymbolTableThread->Shutdown();
  }
  ResetGathering(NS_ERROR_ILLEGAL_DURING_SHUTDOWN);
}

nsresult nsProfiler::Init() { return NS_OK; }

template <typename JsonLogObjectUpdater>
void nsProfiler::Log(JsonLogObjectUpdater&& aJsonLogObjectUpdater) {
  if (mGatheringLog) {
    MOZ_ASSERT(mGatheringLog->isObject());
    std::forward<JsonLogObjectUpdater>(aJsonLogObjectUpdater)(*mGatheringLog);
    MOZ_ASSERT(mGatheringLog->isObject());
  }
}

template <typename JsonArrayAppender>
void nsProfiler::LogEvent(JsonArrayAppender&& aJsonArrayAppender) {
  Log([&](Json::Value& aRoot) {
    Json::Value& events = aRoot[Json::StaticString{"events"}];
    if (!events.isArray()) {
      events = Json::Value{Json::arrayValue};
    }
    Json::Value newEvent{Json::arrayValue};
    newEvent.append(ProfilingLog::Timestamp());
    std::forward<JsonArrayAppender>(aJsonArrayAppender)(newEvent);
    MOZ_ASSERT(newEvent.isArray());
    events.append(std::move(newEvent));
  });
}

void nsProfiler::LogEventLiteralString(const char* aEventString) {
  LogEvent([&](Json::Value& aEvent) {
    aEvent.append(Json::StaticString{aEventString});
  });
}

static nsresult FillVectorFromStringArray(Vector<const char*>& aVector,
                                          const nsTArray<nsCString>& aArray) {
  if (NS_WARN_IF(!aVector.reserve(aArray.Length()))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  for (auto& entry : aArray) {
    aVector.infallibleAppend(entry.get());
  }
  return NS_OK;
}

// Given a PromiseReturningFunction: () -> GenericPromise,
// run the function, and return a JS Promise (through aPromise) that will be
// resolved when the function's GenericPromise gets resolved.
template <typename PromiseReturningFunction>
static nsresult RunFunctionAndConvertPromise(
    JSContext* aCx, Promise** aPromise,
    PromiseReturningFunction&& aPromiseReturningFunction) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  std::forward<PromiseReturningFunction>(aPromiseReturningFunction)()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [promise](GenericPromise::ResolveOrRejectValue&&) {
        promise->MaybeResolveWithUndefined();
      });

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::StartProfiler(uint32_t aEntries, double aInterval,
                          const nsTArray<nsCString>& aFeatures,
                          const nsTArray<nsCString>& aFilters,
                          uint64_t aActiveTabID, double aDuration,
                          JSContext* aCx, Promise** aPromise) {
  ResetGathering(NS_ERROR_DOM_ABORT_ERR);

  Vector<const char*> featureStringVector;
  nsresult rv = FillVectorFromStringArray(featureStringVector, aFeatures);
  if (NS_FAILED(rv)) {
    return rv;
  }
  uint32_t features = ParseFeaturesFromStringArray(
      featureStringVector.begin(), featureStringVector.length());
  Maybe<double> duration = aDuration > 0.0 ? Some(aDuration) : Nothing();

  Vector<const char*> filterStringVector;
  rv = FillVectorFromStringArray(filterStringVector, aFilters);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return RunFunctionAndConvertPromise(aCx, aPromise, [&]() {
    return profiler_start(PowerOfTwo32(aEntries), aInterval, features,
                          filterStringVector.begin(),
                          filterStringVector.length(), aActiveTabID, duration);
  });
}

NS_IMETHODIMP
nsProfiler::StopProfiler(JSContext* aCx, Promise** aPromise) {
  ResetGathering(NS_ERROR_DOM_ABORT_ERR);
  return RunFunctionAndConvertPromise(aCx, aPromise,
                                      []() { return profiler_stop(); });
}

NS_IMETHODIMP
nsProfiler::IsPaused(bool* aIsPaused) {
  *aIsPaused = profiler_is_paused();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::Pause(JSContext* aCx, Promise** aPromise) {
  return RunFunctionAndConvertPromise(aCx, aPromise,
                                      []() { return profiler_pause(); });
}

NS_IMETHODIMP
nsProfiler::Resume(JSContext* aCx, Promise** aPromise) {
  return RunFunctionAndConvertPromise(aCx, aPromise,
                                      []() { return profiler_resume(); });
}

NS_IMETHODIMP
nsProfiler::IsSamplingPaused(bool* aIsSamplingPaused) {
  *aIsSamplingPaused = profiler_is_sampling_paused();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::PauseSampling(JSContext* aCx, Promise** aPromise) {
  return RunFunctionAndConvertPromise(
      aCx, aPromise, []() { return profiler_pause_sampling(); });
}

NS_IMETHODIMP
nsProfiler::ResumeSampling(JSContext* aCx, Promise** aPromise) {
  return RunFunctionAndConvertPromise(
      aCx, aPromise, []() { return profiler_resume_sampling(); });
}

NS_IMETHODIMP
nsProfiler::ClearAllPages() {
  profiler_clear_all_pages();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::WaitOnePeriodicSampling(JSContext* aCx, Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  // The callback cannot officially own the promise RefPtr directly, because
  // `Promise` doesn't support multi-threading, and the callback could destroy
  // the promise in the sampler thread.
  // `nsMainThreadPtrHandle` ensures that the promise can only be destroyed on
  // the main thread. And the invocation from the Sampler thread immediately
  // dispatches a task back to the main thread, to resolve/reject the promise.
  // The lambda needs to be `mutable`, to allow moving-from
  // `promiseHandleInSampler`.
  if (!profiler_callback_after_sampling(
          [promiseHandleInSampler = nsMainThreadPtrHandle<Promise>(
               new nsMainThreadPtrHolder<Promise>(
                   "WaitOnePeriodicSampling promise for Sampler", promise))](
              SamplingState aSamplingState) mutable {
            SchedulerGroup::Dispatch(NS_NewRunnableFunction(
                "nsProfiler::WaitOnePeriodicSampling result on main thread",
                [promiseHandleInMT = std::move(promiseHandleInSampler),
                 aSamplingState]() mutable {
                  switch (aSamplingState) {
                    case SamplingState::JustStopped:
                    case SamplingState::SamplingPaused:
                      promiseHandleInMT->MaybeReject(NS_ERROR_FAILURE);
                      break;

                    case SamplingState::NoStackSamplingCompleted:
                    case SamplingState::SamplingCompleted:
                      // The parent process has succesfully done a sampling,
                      // check the child processes (if any).
                      ProfilerParent::WaitOnePeriodicSampling()->Then(
                          GetMainThreadSerialEventTarget(), __func__,
                          [promiseHandleInMT = std::move(promiseHandleInMT)](
                              GenericPromise::ResolveOrRejectValue&&) {
                            promiseHandleInMT->MaybeResolveWithUndefined();
                          });
                      break;

                    default:
                      MOZ_ASSERT(false, "Unexpected SamplingState value");
                      promiseHandleInMT->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
                      break;
                  }
                }));
          })) {
    // Callback was not added (e.g., profiler is not running) and will never be
    // invoked, so we need to resolve the promise here.
    promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
  }

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfile(double aSinceTime, char** aProfile) {
  mozilla::UniquePtr<char[]> profile = profiler_get_profile(aSinceTime);
  *aProfile = profile.release();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetSharedLibraries(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aResult) {
  JS::Rooted<JS::Value> val(aCx);
  {
    JSONStringWriteFunc<nsCString> buffer;
    JSONWriter w(buffer, JSONWriter::SingleLineStyle);
    w.StartArrayElement();
    SharedLibraryInfo sharedLibraryInfo = SharedLibraryInfo::GetInfoForSelf();
    sharedLibraryInfo.SortByAddress();
    AppendSharedLibraries(w, sharedLibraryInfo);
    w.EndArray();
    NS_ConvertUTF8toUTF16 buffer16(buffer.StringCRef());
    MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx,
                                 static_cast<const char16_t*>(buffer16.get()),
                                 buffer16.Length(), &val));
  }
  JS::Rooted<JSObject*> obj(aCx, &val.toObject());
  if (!obj) {
    return NS_ERROR_FAILURE;
  }
  aResult.setObject(*obj);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetActiveConfiguration(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aResult) {
  JS::Rooted<JS::Value> jsValue(aCx);
  {
    JSONStringWriteFunc<nsCString> buffer;
    JSONWriter writer(buffer, JSONWriter::SingleLineStyle);
    profiler_write_active_configuration(writer);
    NS_ConvertUTF8toUTF16 buffer16(buffer.StringCRef());
    MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx,
                                 static_cast<const char16_t*>(buffer16.get()),
                                 buffer16.Length(), &jsValue));
  }
  if (jsValue.isNull()) {
    aResult.setNull();
  } else {
    JS::Rooted<JSObject*> obj(aCx, &jsValue.toObject());
    if (!obj) {
      return NS_ERROR_FAILURE;
    }
    aResult.setObject(*obj);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::DumpProfileToFile(const char* aFilename) {
  profiler_save_profile_to_file(aFilename);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfileData(double aSinceTime, JSContext* aCx,
                           JS::MutableHandle<JS::Value> aResult) {
  mozilla::UniquePtr<char[]> profile = profiler_get_profile(aSinceTime);
  if (!profile) {
    return NS_ERROR_FAILURE;
  }

  NS_ConvertUTF8toUTF16 js_string(nsDependentCString(profile.get()));
  auto profile16 = static_cast<const char16_t*>(js_string.get());

  JS::Rooted<JS::Value> val(aCx);
  MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx, profile16, js_string.Length(), &val));

  aResult.set(val);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfileDataAsync(double aSinceTime, JSContext* aCx,
                                Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!profiler_is_active()) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  StartGathering(aSinceTime)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [promise](const mozilla::ProfileAndAdditionalInformation& aResult) {
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(promise->GetGlobalObject()))) {
              // We're really hosed if we can't get a JS context for some
              // reason.
              promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
              return;
            }

            JSContext* cx = jsapi.cx();

            // Now parse the JSON so that we resolve with a JS Object.
            JS::Rooted<JS::Value> val(cx);
            {
              NS_ConvertUTF8toUTF16 js_string(aResult.mProfile);
              if (!JS_ParseJSON(cx,
                                static_cast<const char16_t*>(js_string.get()),
                                js_string.Length(), &val)) {
                if (!jsapi.HasException()) {
                  promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
                } else {
                  JS::Rooted<JS::Value> exn(cx);
                  DebugOnly<bool> gotException = jsapi.StealException(&exn);
                  MOZ_ASSERT(gotException);

                  jsapi.ClearException();
                  promise->MaybeReject(exn);
                }
              } else {
                promise->MaybeResolve(val);
              }
            }
          },
          [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfileDataAsArrayBuffer(double aSinceTime, JSContext* aCx,
                                        Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!profiler_is_active()) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  StartGathering(aSinceTime)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [promise](const mozilla::ProfileAndAdditionalInformation& aResult) {
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(promise->GetGlobalObject()))) {
              // We're really hosed if we can't get a JS context for some
              // reason.
              promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
              return;
            }

            JSContext* cx = jsapi.cx();
            ErrorResult error;
            JSObject* typedArray =
                dom::ArrayBuffer::Create(cx, aResult.mProfile, error);
            if (!error.Failed()) {
              JS::Rooted<JS::Value> val(cx, JS::ObjectValue(*typedArray));
              promise->MaybeResolve(val);
            } else {
              promise->MaybeReject(std::move(error));
            }
          },
          [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
  return NS_OK;
}

nsresult CompressString(const nsCString& aString,
                        FallibleTArray<uint8_t>& aOutBuff) {
  // Compress a buffer via zlib (as with `compress()`), but emit a
  // gzip header as well. Like `compress()`, this is limited to 4GB in
  // size, but that shouldn't be an issue for our purposes.
  uLongf outSize = compressBound(aString.Length());
  if (!aOutBuff.SetLength(outSize, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  int zerr;
  z_stream stream;
  stream.zalloc = nullptr;
  stream.zfree = nullptr;
  stream.opaque = nullptr;
  stream.next_out = (Bytef*)aOutBuff.Elements();
  stream.avail_out = aOutBuff.Length();
  stream.next_in = (z_const Bytef*)aString.Data();
  stream.avail_in = aString.Length();

  // A windowBits of 31 is the default (15) plus 16 for emitting a
  // gzip header; a memLevel of 8 is the default.
  zerr =
      deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                   /* windowBits */ 31, /* memLevel */ 8, Z_DEFAULT_STRATEGY);
  if (zerr != Z_OK) {
    return NS_ERROR_FAILURE;
  }

  zerr = deflate(&stream, Z_FINISH);
  outSize = stream.total_out;
  deflateEnd(&stream);

  if (zerr != Z_STREAM_END) {
    return NS_ERROR_FAILURE;
  }

  aOutBuff.TruncateLength(outSize);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfileDataAsGzippedArrayBuffer(double aSinceTime,
                                               JSContext* aCx,
                                               Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!profiler_is_active()) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  StartGathering(aSinceTime)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [promise](const mozilla::ProfileAndAdditionalInformation& aResult) {
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(promise->GetGlobalObject()))) {
              // We're really hosed if we can't get a JS context for some
              // reason.
              promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
              return;
            }

            FallibleTArray<uint8_t> outBuff;
            nsresult result = CompressString(aResult.mProfile, outBuff);

            if (result != NS_OK) {
              promise->MaybeReject(result);
              return;
            }

            JSContext* cx = jsapi.cx();
            // Get the profile typedArray.
            ErrorResult error;
            JSObject* typedArray = dom::ArrayBuffer::Create(cx, outBuff, error);
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JS::Value> typedArrayValue(cx,
                                                  JS::ObjectValue(*typedArray));
            // Get the additional information object.
            JS::Rooted<JS::Value> additionalInfoVal(cx);
            if (aResult.mAdditionalInformation.isSome()) {
              aResult.mAdditionalInformation->ToJSValue(cx, &additionalInfoVal);
            } else {
              additionalInfoVal.setUndefined();
            }

            // Create the return object.
            JS::Rooted<JSObject*> resultObj(cx, JS_NewPlainObject(cx));
            JS_SetProperty(cx, resultObj, "profile", typedArrayValue);
            JS_SetProperty(cx, resultObj, "additionalInformation",
                           additionalInfoVal);
            promise->MaybeResolve(resultObj);
          },
          [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::DumpProfileToFileAsync(const nsACString& aFilename,
                                   double aSinceTime, JSContext* aCx,
                                   Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!profiler_is_active()) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  nsCString filename(aFilename);

  StartGathering(aSinceTime)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [filename,
           promise](const mozilla::ProfileAndAdditionalInformation& aResult) {
            if (aResult.mProfile.Length() >=
                size_t(std::numeric_limits<std::streamsize>::max())) {
              promise->MaybeReject(NS_ERROR_FILE_TOO_BIG);
              return;
            }

            std::ofstream stream;
            stream.open(filename.get());
            if (!stream.is_open()) {
              promise->MaybeReject(NS_ERROR_FILE_UNRECOGNIZED_PATH);
              return;
            }

            stream.write(aResult.mProfile.get(),
                         std::streamsize(aResult.mProfile.Length()));
            stream.close();

            promise->MaybeResolveWithUndefined();
          },
          [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetSymbolTable(const nsACString& aDebugPath,
                           const nsACString& aBreakpadID, JSContext* aCx,
                           Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* globalObject =
      xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));

  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  GetSymbolTableMozPromise(aDebugPath, aBreakpadID)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [promise](const SymbolTable& aSymbolTable) {
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(promise->GetGlobalObject()))) {
              // We're really hosed if we can't get a JS context for some
              // reason.
              promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
              return;
            }

            JSContext* cx = jsapi.cx();

            ErrorResult error;
            JS::Rooted<JSObject*> addrsArray(
                cx, dom::Uint32Array::Create(cx, aSymbolTable.mAddrs, error));
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            JS::Rooted<JSObject*> indexArray(
                cx, dom::Uint32Array::Create(cx, aSymbolTable.mIndex, error));
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            JS::Rooted<JSObject*> bufferArray(
                cx, dom::Uint8Array::Create(cx, aSymbolTable.mBuffer, error));
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            JS::Rooted<JSObject*> tuple(cx, JS::NewArrayObject(cx, 3));
            JS_SetElement(cx, tuple, 0, addrsArray);
            JS_SetElement(cx, tuple, 1, indexArray);
            JS_SetElement(cx, tuple, 2, bufferArray);
            promise->MaybeResolve(tuple);
          },
          [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetElapsedTime(double* aElapsedTime) {
  *aElapsedTime = profiler_time();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::IsActive(bool* aIsActive) {
  *aIsActive = profiler_is_active();
  return NS_OK;
}

static void GetArrayOfStringsForFeatures(uint32_t aFeatures,
                                         nsTArray<nsCString>& aFeatureList) {
#define COUNT_IF_SET(n_, str_, Name_, desc_)    \
  if (ProfilerFeature::Has##Name_(aFeatures)) { \
    len++;                                      \
  }

  // Count the number of features in use.
  uint32_t len = 0;
  PROFILER_FOR_EACH_FEATURE(COUNT_IF_SET)

#undef COUNT_IF_SET

  aFeatureList.SetCapacity(len);

#define DUP_IF_SET(n_, str_, Name_, desc_)      \
  if (ProfilerFeature::Has##Name_(aFeatures)) { \
    aFeatureList.AppendElement(str_);           \
  }

  // Insert the strings for the features in use.
  PROFILER_FOR_EACH_FEATURE(DUP_IF_SET)

#undef DUP_IF_SET
}

NS_IMETHODIMP
nsProfiler::GetFeatures(nsTArray<nsCString>& aFeatureList) {
  uint32_t features = profiler_get_available_features();
  GetArrayOfStringsForFeatures(features, aFeatureList);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetAllFeatures(nsTArray<nsCString>& aFeatureList) {
  GetArrayOfStringsForFeatures((uint32_t)-1, aFeatureList);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetBufferInfo(uint32_t* aCurrentPosition, uint32_t* aTotalSize,
                          uint32_t* aGeneration) {
  MOZ_ASSERT(aCurrentPosition);
  MOZ_ASSERT(aTotalSize);
  MOZ_ASSERT(aGeneration);
  Maybe<ProfilerBufferInfo> info = profiler_get_buffer_info();
  if (info) {
    *aCurrentPosition = info->mRangeEnd % info->mEntryCount;
    *aTotalSize = info->mEntryCount;
    *aGeneration = info->mRangeEnd / info->mEntryCount;
  } else {
    *aCurrentPosition = 0;
    *aTotalSize = 0;
    *aGeneration = 0;
  }
  return NS_OK;
}

bool nsProfiler::SendProgressRequest(PendingProfile& aPendingProfile) {
  RefPtr<ProfilerParent::SingleProcessProgressPromise> progressPromise =
      ProfilerParent::RequestGatherProfileProgress(aPendingProfile.childPid);
  if (!progressPromise) {
    LOG("RequestGatherProfileProgress(%u) -> null!",
        unsigned(aPendingProfile.childPid));
    LogEvent([&](Json::Value& aEvent) {
      aEvent.append(
          Json::StaticString{"Failed to send progress request to pid:"});
      aEvent.append(Json::Value::UInt64(aPendingProfile.childPid));
    });
    // Failed to send request.
    return false;
  }

  DEBUG_LOG("RequestGatherProfileProgress(%u) sent...",
            unsigned(aPendingProfile.childPid));
  LogEvent([&](Json::Value& aEvent) {
    aEvent.append(Json::StaticString{"Requested progress from pid:"});
    aEvent.append(Json::Value::UInt64(aPendingProfile.childPid));
  });
  aPendingProfile.lastProgressRequest = TimeStamp::Now();
  progressPromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self = RefPtr<nsProfiler>(this),
       childPid = aPendingProfile.childPid](GatherProfileProgress&& aResult) {
        if (!self->mGathering) {
          return;
        }
        PendingProfile* pendingProfile = self->GetPendingProfile(childPid);
        DEBUG_LOG(
            "RequestGatherProfileProgress(%u) response: %.2f '%s' "
            "(%u were pending, %s %u)",
            unsigned(childPid),
            ProportionValue::FromUnderlyingType(
                aResult.progressProportionValueUnderlyingType())
                    .ToDouble() *
                100.0,
            aResult.progressLocation().Data(),
            unsigned(self->mPendingProfiles.length()),
            pendingProfile ? "including" : "excluding", unsigned(childPid));
        self->LogEvent([&](Json::Value& aEvent) {
          aEvent.append(
              Json::StaticString{"Got response from pid, with progress:"});
          aEvent.append(Json::Value::UInt64(childPid));
          aEvent.append(
              Json::Value{ProportionValue::FromUnderlyingType(
                              aResult.progressProportionValueUnderlyingType())
                              .ToDouble() *
                          100.0});
        });
        if (pendingProfile) {
          // We have a progress report for a still-pending profile.
          pendingProfile->lastProgressResponse = TimeStamp::Now();
          // Has it actually made progress?
          if (aResult.progressProportionValueUnderlyingType() !=
              pendingProfile->progressProportion.ToUnderlyingType()) {
            pendingProfile->lastProgressChange =
                pendingProfile->lastProgressResponse;
            pendingProfile->progressProportion =
                ProportionValue::FromUnderlyingType(
                    aResult.progressProportionValueUnderlyingType());
            pendingProfile->progressLocation = aResult.progressLocation();
            self->RestartGatheringTimer();
          }
        }
      },
      [self = RefPtr<nsProfiler>(this), childPid = aPendingProfile.childPid](
          ipc::ResponseRejectReason&& aReason) {
        if (!self->mGathering) {
          return;
        }
        PendingProfile* pendingProfile = self->GetPendingProfile(childPid);
        LOG("RequestGatherProfileProgress(%u) rejection: %d "
            "(%u were pending, %s %u)",
            unsigned(childPid), (int)aReason,
            unsigned(self->mPendingProfiles.length()),
            pendingProfile ? "including" : "excluding", unsigned(childPid));
        self->LogEvent([&](Json::Value& aEvent) {
          aEvent.append(Json::StaticString{
              "Got progress request rejection from pid, with reason:"});
          aEvent.append(Json::Value::UInt64(childPid));
          aEvent.append(Json::Value::UInt{static_cast<unsigned>(aReason)});
        });
        if (pendingProfile) {
          // Failure response, assume the child process is gone.
          MOZ_ASSERT(self->mPendingProfiles.begin() <= pendingProfile &&
                     pendingProfile < self->mPendingProfiles.end());
          self->mPendingProfiles.erase(pendingProfile);
          if (self->mPendingProfiles.empty()) {
            // We've got all of the async profiles now. Let's finish off the
            // profile and resolve the Promise.
            self->FinishGathering();
          }
        }
      });
  return true;
}

/* static */ void nsProfiler::GatheringTimerCallback(nsITimer* aTimer,
                                                     void* aClosure) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIProfiler> profiler(
      do_GetService("@mozilla.org/tools/profiler;1"));
  if (!profiler) {
    // No (more) profiler service.
    return;
  }
  nsProfiler* self = static_cast<nsProfiler*>(profiler.get());
  if (self != aClosure) {
    // Different service object!?
    return;
  }
  if (aTimer != self->mGatheringTimer) {
    // This timer was cancelled after this callback was queued.
    return;
  }

  bool progressWasMade = false;

  // Going backwards, it's easier and cheaper to erase elements if needed.
  for (auto iPlus1 = self->mPendingProfiles.length(); iPlus1 != 0; --iPlus1) {
    PendingProfile& pendingProfile = self->mPendingProfiles[iPlus1 - 1];

    bool needToSendProgressRequest = false;
    if (pendingProfile.lastProgressRequest.IsNull()) {
      DEBUG_LOG("GatheringTimerCallback() - child %u: No data yet",
                unsigned(pendingProfile.childPid));
      // First time going through the list, send an initial progress request.
      needToSendProgressRequest = true;
      // We pretend that progress was made, so we don't give up yet.
      progressWasMade = true;
    } else if (pendingProfile.lastProgressResponse.IsNull()) {
      LOG("GatheringTimerCallback() - child %u: Waiting for first response",
          unsigned(pendingProfile.childPid));
      // Still waiting for the first response, no progress made here, don't send
      // another request.
    } else if (pendingProfile.lastProgressResponse <=
               pendingProfile.lastProgressRequest) {
      LOG("GatheringTimerCallback() - child %u: Waiting for response",
          unsigned(pendingProfile.childPid));
      // Still waiting for a response to the last request, no progress made
      // here, don't send another request.
    } else if (pendingProfile.lastProgressChange.IsNull()) {
      LOG("GatheringTimerCallback() - child %u: Still waiting for first change",
          unsigned(pendingProfile.childPid));
      // Still waiting for the first change, no progress made here, but send a
      // new request.
      needToSendProgressRequest = true;
    } else if (pendingProfile.lastProgressRequest <
               pendingProfile.lastProgressChange) {
      DEBUG_LOG("GatheringTimerCallback() - child %u: Recent change",
                unsigned(pendingProfile.childPid));
      // We have a recent change, progress was made.
      needToSendProgressRequest = true;
      progressWasMade = true;
    } else {
      LOG("GatheringTimerCallback() - child %u: No recent change",
          unsigned(pendingProfile.childPid));
      needToSendProgressRequest = true;
    }

    // And send a new progress request.
    if (needToSendProgressRequest) {
      if (!self->SendProgressRequest(pendingProfile)) {
        // Failed to even send the request, consider this process gone.
        self->mPendingProfiles.erase(&pendingProfile);
        LOG("... Failed to send progress request");
      } else {
        DEBUG_LOG("... Sent progress request");
      }
    } else {
      DEBUG_LOG("... No progress request");
    }
  }

  if (self->mPendingProfiles.empty()) {
    // We've got all of the async profiles now. Let's finish off the profile
    // and resolve the Promise.
    self->FinishGathering();
    return;
  }

  // Not finished yet.

  if (progressWasMade) {
    // We made some progress, just restart the timer.
    DEBUG_LOG("GatheringTimerCallback() - Progress made, restart timer");
    self->RestartGatheringTimer();
    return;
  }

  DEBUG_LOG("GatheringTimerCallback() - Timeout!");
  self->mGatheringTimer = nullptr;
  if (!profiler_is_active() || !self->mGathering) {
    // Not gathering anymore.
    return;
  }
  self->LogEvent([&](Json::Value& aEvent) {
    aEvent.append(Json::StaticString{
        "No progress made recently, giving up; pending pids:"});
    for (const PendingProfile& pendingProfile : self->mPendingProfiles) {
      aEvent.append(Json::Value::UInt64(pendingProfile.childPid));
    }
  });
  NS_WARNING("Profiler failed to gather profiles from all sub-processes");
  // We have really reached a timeout while gathering, finish now.
  // TODO: Add information about missing processes.
  self->FinishGathering();
}

void nsProfiler::RestartGatheringTimer() {
  if (mGatheringTimer) {
    uint32_t delayMs = 0;
    const nsresult r = mGatheringTimer->GetDelay(&delayMs);
    mGatheringTimer->Cancel();
    if (NS_FAILED(r) || delayMs == 0 ||
        NS_FAILED(mGatheringTimer->InitWithNamedFuncCallback(
            GatheringTimerCallback, this, delayMs,
            nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY,
            "nsProfilerGatheringTimer"))) {
      // Can't restart the timer, so we can't wait any longer.
      FinishGathering();
    }
  }
}

nsProfiler::PendingProfile* nsProfiler::GetPendingProfile(
    base::ProcessId aChildPid) {
  for (PendingProfile& pendingProfile : mPendingProfiles) {
    if (pendingProfile.childPid == aChildPid) {
      return &pendingProfile;
    }
  }
  return nullptr;
}

void nsProfiler::GatheredOOPProfile(
    base::ProcessId aChildPid, const nsACString& aProfile,
    mozilla::Maybe<ProfileGenerationAdditionalInformation>&&
        aAdditionalInformation) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!profiler_is_active()) {
    return;
  }

  if (!mGathering) {
    // If we're not actively gathering, then we don't actually care that we
    // gathered a profile here. This can happen for processes that exit while
    // profiling.
    return;
  }

  MOZ_RELEASE_ASSERT(mWriter.isSome(),
                     "Should always have a writer if mGathering is true");

  // Combine all the additional information into a single struct.
  if (aAdditionalInformation.isSome()) {
    mProfileGenerationAdditionalInformation->Append(
        std::move(*aAdditionalInformation));
  }

  if (!aProfile.IsEmpty()) {
    if (mWriter->ChunkedWriteFunc().Length() + aProfile.Length() <
        scLengthAccumulationThreshold) {
      // TODO: Remove PromiseFlatCString, see bug 1657033.
      mWriter->Splice(PromiseFlatCString(aProfile));
    } else {
      LogEvent([&](Json::Value& aEvent) {
        aEvent.append(
            Json::StaticString{"Discarded child profile that would make the "
                               "full profile too big, pid and size:"});
        aEvent.append(Json::Value::UInt64(aChildPid));
        aEvent.append(Json::Value::UInt64{aProfile.Length()});
      });
    }
  }

  if (PendingProfile* pendingProfile = GetPendingProfile(aChildPid);
      pendingProfile) {
    mPendingProfiles.erase(pendingProfile);

    if (mPendingProfiles.empty()) {
      // We've got all of the async profiles now. Let's finish off the profile
      // and resolve the Promise.
      FinishGathering();
    }
  }

  // Not finished yet, restart the timer to let any remaining child enough time
  // to do their profile-streaming.
  RestartGatheringTimer();
}

RefPtr<nsProfiler::GatheringPromiseAndroid>
nsProfiler::GetProfileDataAsGzippedArrayBufferAndroid(double aSinceTime) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!profiler_is_active()) {
    return GatheringPromiseAndroid::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  return StartGathering(aSinceTime)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [](const mozilla::ProfileAndAdditionalInformation& aResult) {
            FallibleTArray<uint8_t> outBuff;
            nsresult result = CompressString(aResult.mProfile, outBuff);
            if (result != NS_OK) {
              return GatheringPromiseAndroid::CreateAndReject(result, __func__);
            }
            return GatheringPromiseAndroid::CreateAndResolve(std::move(outBuff),
                                                             __func__);
          },
          [](nsresult aRv) {
            return GatheringPromiseAndroid::CreateAndReject(aRv, __func__);
          });
}

RefPtr<nsProfiler::GatheringPromise> nsProfiler::StartGathering(
    double aSinceTime) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (mGathering) {
    // If we're already gathering, return a rejected promise - this isn't
    // going to end well.
    return GatheringPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  mGathering = true;
  mGatheringLog = mozilla::MakeUnique<Json::Value>(Json::objectValue);
  (*mGatheringLog)[Json::StaticString{
      "profileGatheringLogBegin" TIMESTAMP_JSON_SUFFIX}] =
      ProfilingLog::Timestamp();

  if (mGatheringTimer) {
    mGatheringTimer->Cancel();
    mGatheringTimer = nullptr;
  }

  // Start building shared library info starting from the current process.
  mProfileGenerationAdditionalInformation.emplace(
      SharedLibraryInfo::GetInfoForSelf());

  // Request profiles from the other processes. This will trigger asynchronous
  // calls to ProfileGatherer::GatheredOOPProfile as the profiles arrive.
  //
  // Do this before the call to profiler_stream_json_for_this_process() because
  // that call is slow and we want to let the other processes grab their
  // profiles as soon as possible.
  nsTArray<ProfilerParent::SingleProcessProfilePromiseAndChildPid> profiles =
      ProfilerParent::GatherProfiles();

  MOZ_ASSERT(mPendingProfiles.empty());
  if (!mPendingProfiles.reserve(profiles.Length())) {
    ResetGathering(NS_ERROR_OUT_OF_MEMORY);
    return GatheringPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  mFailureLatchSource.emplace();
  mWriter.emplace(*mFailureLatchSource);

  UniquePtr<ProfilerCodeAddressService> service =
      profiler_code_address_service_for_presymbolication();

  // Start building up the JSON result and grab the profile from this process.
  mWriter->Start();
  auto rv = profiler_stream_json_for_this_process(*mWriter, aSinceTime,
                                                  /* aIsShuttingDown */ false,
                                                  service.get());
  if (rv.isErr()) {
    // The profiler is inactive. This either means that it was inactive even
    // at the time that ProfileGatherer::Start() was called, or that it was
    // stopped on a different thread since that call. Either way, we need to
    // reject the promise and stop gathering.
    ResetGathering(NS_ERROR_NOT_AVAILABLE);
    return GatheringPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  LogEvent([&](Json::Value& aEvent) {
    aEvent.append(
        Json::StaticString{"Generated parent process profile, size:"});
    aEvent.append(Json::Value::UInt64{mWriter->ChunkedWriteFunc().Length()});
  });

  mWriter->StartArrayProperty("processes");

  // If we have any process exit profiles, add them immediately.
  if (Vector<nsCString> exitProfiles = profiler_move_exit_profiles();
      !exitProfiles.empty()) {
    for (auto& exitProfile : exitProfiles) {
      if (!exitProfile.IsEmpty()) {
        if (exitProfile[0] == '*') {
          LogEvent([&](Json::Value& aEvent) {
            aEvent.append(
                Json::StaticString{"Exit non-profile with error message:"});
            aEvent.append(exitProfile.Data() + 1);
          });
        } else if (mWriter->ChunkedWriteFunc().Length() + exitProfile.Length() <
                   scLengthAccumulationThreshold) {
          mWriter->Splice(exitProfile);
          LogEvent([&](Json::Value& aEvent) {
            aEvent.append(Json::StaticString{"Added exit profile with size:"});
            aEvent.append(Json::Value::UInt64{exitProfile.Length()});
          });
        } else {
          LogEvent([&](Json::Value& aEvent) {
            aEvent.append(
                Json::StaticString{"Discarded an exit profile that would make "
                                   "the full profile too big, size:"});
            aEvent.append(Json::Value::UInt64{exitProfile.Length()});
          });
        }
      }
    }

    LogEvent([&](Json::Value& aEvent) {
      aEvent.append(Json::StaticString{
          "Processed all exit profiles, total size so far:"});
      aEvent.append(Json::Value::UInt64{mWriter->ChunkedWriteFunc().Length()});
    });
  } else {
    // There are no pending profiles, we're already done.
    LogEventLiteralString("No exit profiles.");
  }

  mPromiseHolder.emplace();
  RefPtr<GatheringPromise> promise = mPromiseHolder->Ensure(__func__);

  // Keep the array property "processes" and the root object in mWriter open
  // until FinishGathering() is called. As profiles from the other processes
  // come in, they will be inserted and end up in the right spot.
  // FinishGathering() will close the array and the root object.

  if (!profiles.IsEmpty()) {
    // There *are* pending profiles, let's add handlers for their promises.

    // This timeout value is used to monitor progress while gathering child
    // profiles. The timer will be restarted after we receive a response with
    // any progress.
    constexpr uint32_t cMinChildTimeoutS = 1u;  // 1 second minimum and default.
    constexpr uint32_t cMaxChildTimeoutS = 60u;  // 1 minute max.
    uint32_t childTimeoutS = Preferences::GetUint(
        "devtools.performance.recording.child.timeout_s", cMinChildTimeoutS);
    if (childTimeoutS < cMinChildTimeoutS) {
      childTimeoutS = cMinChildTimeoutS;
    } else if (childTimeoutS > cMaxChildTimeoutS) {
      childTimeoutS = cMaxChildTimeoutS;
    }
    const uint32_t childTimeoutMs = childTimeoutS * PR_MSEC_PER_SEC;
    Unused << NS_NewTimerWithFuncCallback(
        getter_AddRefs(mGatheringTimer), GatheringTimerCallback, this,
        childTimeoutMs, nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY,
        "nsProfilerGatheringTimer", GetMainThreadSerialEventTarget());

    MOZ_ASSERT(mPendingProfiles.capacity() >= profiles.Length());
    for (const auto& profile : profiles) {
      mPendingProfiles.infallibleAppend(PendingProfile{profile.childPid});
      LogEvent([&](Json::Value& aEvent) {
        aEvent.append(Json::StaticString{"Waiting for pending profile, pid:"});
        aEvent.append(Json::Value::UInt64(profile.childPid));
      });
      profile.profilePromise->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self = RefPtr<nsProfiler>(this), childPid = profile.childPid](
              IPCProfileAndAdditionalInformation&& aResult) {
            PendingProfile* pendingProfile = self->GetPendingProfile(childPid);
            mozilla::ipc::Shmem profileShmem = aResult.profileShmem();
            LOG("GatherProfile(%u) response: %u bytes (%u were pending, %s %u)",
                unsigned(childPid), unsigned(profileShmem.Size<char>()),
                unsigned(self->mPendingProfiles.length()),
                pendingProfile ? "including" : "excluding", unsigned(childPid));
            if (profileShmem.IsReadable()) {
              self->LogEvent([&](Json::Value& aEvent) {
                aEvent.append(
                    Json::StaticString{"Got profile from pid, with size:"});
                aEvent.append(Json::Value::UInt64(childPid));
                aEvent.append(Json::Value::UInt64{profileShmem.Size<char>()});
              });
              const nsDependentCSubstring profileString(
                  profileShmem.get<char>(), profileShmem.Size<char>() - 1);
              if (profileString.IsEmpty() || profileString[0] != '*') {
                self->GatheredOOPProfile(
                    childPid, profileString,
                    std::move(aResult.additionalInformation()));
              } else {
                self->LogEvent([&](Json::Value& aEvent) {
                  aEvent.append(Json::StaticString{
                      "Child non-profile from pid, with error message:"});
                  aEvent.append(Json::Value::UInt64(childPid));
                  aEvent.append(profileString.Data() + 1);
                });
                self->GatheredOOPProfile(childPid, ""_ns, Nothing());
              }
            } else {
              // This can happen if the child failed to allocate
              // the Shmem (or maliciously sent an invalid Shmem).
              self->LogEvent([&](Json::Value& aEvent) {
                aEvent.append(Json::StaticString{"Got failure from pid:"});
                aEvent.append(Json::Value::UInt64(childPid));
              });
              self->GatheredOOPProfile(childPid, ""_ns, Nothing());
            }
          },
          [self = RefPtr<nsProfiler>(this),
           childPid = profile.childPid](ipc::ResponseRejectReason&& aReason) {
            PendingProfile* pendingProfile = self->GetPendingProfile(childPid);
            LOG("GatherProfile(%u) rejection: %d (%u were pending, %s %u)",
                unsigned(childPid), (int)aReason,
                unsigned(self->mPendingProfiles.length()),
                pendingProfile ? "including" : "excluding", unsigned(childPid));
            self->LogEvent([&](Json::Value& aEvent) {
              aEvent.append(
                  Json::StaticString{"Got rejection from pid, with reason:"});
              aEvent.append(Json::Value::UInt64(childPid));
              aEvent.append(Json::Value::UInt{static_cast<unsigned>(aReason)});
            });
            self->GatheredOOPProfile(childPid, ""_ns, Nothing());
          });
    }
  } else {
    // There are no pending profiles, we're already done.
    LogEventLiteralString("No pending child profiles.");
    FinishGathering();
  }

  return promise;
}

RefPtr<nsProfiler::SymbolTablePromise> nsProfiler::GetSymbolTableMozPromise(
    const nsACString& aDebugPath, const nsACString& aBreakpadID) {
  MozPromiseHolder<SymbolTablePromise> promiseHolder;
  RefPtr<SymbolTablePromise> promise = promiseHolder.Ensure(__func__);

  if (!mSymbolTableThread) {
    nsresult rv = NS_NewNamedThread("ProfSymbolTable",
                                    getter_AddRefs(mSymbolTableThread));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      promiseHolder.Reject(NS_ERROR_FAILURE, __func__);
      return promise;
    }
  }

  nsresult rv = mSymbolTableThread->Dispatch(NS_NewRunnableFunction(
      "nsProfiler::GetSymbolTableMozPromise runnable on ProfSymbolTable thread",
      [promiseHolder = std::move(promiseHolder),
       debugPath = nsCString(aDebugPath),
       breakpadID = nsCString(aBreakpadID)]() mutable {
        AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING("profiler_get_symbol_table",
                                              OTHER, debugPath);
        SymbolTable symbolTable;
        bool succeeded = profiler_get_symbol_table(
            debugPath.get(), breakpadID.get(), &symbolTable);
        if (succeeded) {
          promiseHolder.Resolve(std::move(symbolTable), __func__);
        } else {
          promiseHolder.Reject(NS_ERROR_FAILURE, __func__);
        }
      }));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Get-symbol task was not dispatched and therefore won't fulfill the
    // promise, we must reject the promise now.
    promiseHolder.Reject(NS_ERROR_FAILURE, __func__);
  }

  return promise;
}

void nsProfiler::FinishGathering() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(mWriter.isSome());
  MOZ_RELEASE_ASSERT(mPromiseHolder.isSome());
  MOZ_RELEASE_ASSERT(mProfileGenerationAdditionalInformation.isSome());

  // Close the "processes" array property.
  mWriter->EndArray();

  if (mGatheringLog) {
    LogEvent([&](Json::Value& aEvent) {
      aEvent.append(Json::StaticString{"Finished gathering, total size:"});
      aEvent.append(Json::Value::UInt64{mWriter->ChunkedWriteFunc().Length()});
    });
    (*mGatheringLog)[Json::StaticString{
        "profileGatheringLogEnd" TIMESTAMP_JSON_SUFFIX}] =
        ProfilingLog::Timestamp();
    mWriter->StartObjectProperty("profileGatheringLog");
    {
      nsAutoCString pid;
      pid.AppendInt(int64_t(profiler_current_process_id().ToNumber()));
      Json::String logString = ToCompactString(*mGatheringLog);
      mGatheringLog = nullptr;
      mWriter->SplicedJSONProperty(pid, logString);
    }
    mWriter->EndObject();
  }

  // Close the root object of the generated JSON.
  mWriter->End();

  if (const char* failure = mWriter->GetFailure(); failure) {
#ifndef ANDROID
    fprintf(stderr, "JSON generation failure: %s", failure);
#else
    __android_log_print(ANDROID_LOG_INFO, "GeckoProfiler",
                        "JSON generation failure: %s", failure);
#endif
    NS_WARNING("Error during JSON generation, probably OOM.");
    ResetGathering(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  // And try to resolve the promise with the profile JSON.
  const size_t len = mWriter->ChunkedWriteFunc().Length();
  if (len >= scLengthMax) {
    NS_WARNING("Profile JSON is too big to fit in a string.");
    ResetGathering(NS_ERROR_FILE_TOO_BIG);
    return;
  }

  nsCString result;
  if (!result.SetLength(len, fallible)) {
    NS_WARNING("Cannot allocate a string for the Profile JSON.");
    ResetGathering(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  MOZ_ASSERT(*(result.Data() + len) == '\0',
             "We expected a null at the end of the string buffer, to be "
             "rewritten by CopyDataIntoLazilyAllocatedBuffer");

  char* const resultBeginWriting = result.BeginWriting();
  if (!resultBeginWriting) {
    NS_WARNING("Cannot access the string to write the Profile JSON.");
    ResetGathering(NS_ERROR_CACHE_WRITE_ACCESS_DENIED);
    return;
  }

  // Here, we have enough space reserved in `result`, starting at
  // `resultBeginWriting`, copy the JSON profile there.
  if (!mWriter->ChunkedWriteFunc().CopyDataIntoLazilyAllocatedBuffer(
          [&](size_t aBufferLen) -> char* {
            MOZ_RELEASE_ASSERT(aBufferLen == len + 1);
            return resultBeginWriting;
          })) {
    NS_WARNING("Could not copy profile JSON, probably OOM.");
    ResetGathering(NS_ERROR_FILE_TOO_BIG);
    return;
  }
  MOZ_ASSERT(*(result.Data() + len) == '\0',
             "We still expected a null at the end of the string buffer");

  mProfileGenerationAdditionalInformation->FinishGathering();
  mPromiseHolder->Resolve(
      ProfileAndAdditionalInformation{
          std::move(result),
          std::move(*mProfileGenerationAdditionalInformation)},
      __func__);

  ResetGathering(NS_ERROR_UNEXPECTED);
}

void nsProfiler::ResetGathering(nsresult aPromiseRejectionIfPending) {
  // If we have an unfulfilled Promise in flight, we should reject it before
  // destroying the promise holder.
  if (mPromiseHolder.isSome()) {
    mPromiseHolder->RejectIfExists(aPromiseRejectionIfPending, __func__);
    mPromiseHolder.reset();
  }
  mPendingProfiles.clearAndFree();
  mGathering = false;
  mGatheringLog = nullptr;
  if (mGatheringTimer) {
    mGatheringTimer->Cancel();
    mGatheringTimer = nullptr;
  }
  mWriter.reset();
  mFailureLatchSource.reset();
  mProfileGenerationAdditionalInformation.reset();
}
