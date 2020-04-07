/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProfiler.h"

#include <sstream>
#include <string>
#include <utility>

#include "GeckoProfiler.h"
#include "ProfilerParent.h"
#include "js/Array.h"  // JS::NewArrayObject
#include "js/JSON.h"
#include "js/Value.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Services.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TypedArray.h"
#include "nsIFileStreams.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadContext.h"
#include "nsIObserverService.h"
#include "nsIWebNavigation.h"
#include "nsLocalFile.h"
#include "nsMemory.h"
#include "nsProfilerStartParams.h"
#include "nsProxyRelease.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "platform.h"
#include "shared-libraries.h"
#include "zlib.h"

using namespace mozilla;

using dom::AutoJSAPI;
using dom::Promise;
using std::string;

NS_IMPL_ISUPPORTS(nsProfiler, nsIProfiler, nsIObserver)

nsProfiler::nsProfiler()
    : mLockedForPrivateBrowsing(false),
      mPendingProfiles(0),
      mGathering(false) {}

nsProfiler::~nsProfiler() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, "chrome-document-global-created");
    observerService->RemoveObserver(this, "last-pb-context-exited");
  }
  if (mSymbolTableThread) {
    mSymbolTableThread->Shutdown();
  }
}

nsresult nsProfiler::Init() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "chrome-document-global-created", false);
    observerService->AddObserver(this, "last-pb-context-exited", false);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::Observe(nsISupports* aSubject, const char* aTopic,
                    const char16_t* aData) {
  // The profiler's handling of private browsing is as simple as possible: it
  // is stopped when the first PB window opens, and left stopped when the last
  // PB window closes.
  if (strcmp(aTopic, "chrome-document-global-created") == 0) {
    nsCOMPtr<nsIInterfaceRequestor> requestor = do_QueryInterface(aSubject);
    nsCOMPtr<nsIWebNavigation> parentWebNav = do_GetInterface(requestor);
    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(parentWebNav);
    if (loadContext && loadContext->UsePrivateBrowsing() &&
        !mLockedForPrivateBrowsing) {
      mLockedForPrivateBrowsing = true;
      profiler_stop();
    }
  } else if (strcmp(aTopic, "last-pb-context-exited") == 0) {
    mLockedForPrivateBrowsing = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::CanProfile(bool* aCanProfile) {
  *aCanProfile = !mLockedForPrivateBrowsing;
  return NS_OK;
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

NS_IMETHODIMP
nsProfiler::StartProfiler(uint32_t aEntries, double aInterval,
                          const nsTArray<nsCString>& aFeatures,
                          const nsTArray<nsCString>& aFilters,
                          uint64_t aActiveBrowsingContextID, double aDuration) {
  if (mLockedForPrivateBrowsing) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ResetGathering();

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
  profiler_start(PowerOfTwo32(aEntries), aInterval, features,
                 filterStringVector.begin(), filterStringVector.length(),
                 aActiveBrowsingContextID, duration);

  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::StopProfiler() {
  // If we have a Promise in flight, we should reject it.
  if (mPromiseHolder.isSome()) {
    mPromiseHolder->RejectIfExists(NS_ERROR_DOM_ABORT_ERR, __func__);
  }
  ResetGathering();

  profiler_stop();

  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::IsPaused(bool* aIsPaused) {
  *aIsPaused = profiler_is_paused();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::PauseSampling() {
  profiler_pause();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::ResumeSampling() {
  profiler_resume();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::AddMarker(const char* aMarker) {
  PROFILER_ADD_MARKER(aMarker, OTHER);
  return NS_OK;
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
            SchedulerGroup::Dispatch(
                TaskCategory::Other,
                NS_NewRunnableFunction(
                    "nsProfiler::WaitOnePeriodicSampling result on main thread",
                    [promiseHandleInMT = std::move(promiseHandleInSampler),
                     aSamplingState]() {
                      AutoJSAPI jsapi;
                      if (NS_WARN_IF(!jsapi.Init(
                              promiseHandleInMT->GetGlobalObject()))) {
                        // We're really hosed if we can't get a JS context for
                        // some reason.
                        promiseHandleInMT->MaybeReject(
                            NS_ERROR_DOM_UNKNOWN_ERR);
                        return;
                      }

                      switch (aSamplingState) {
                        case SamplingState::JustStopped:
                        case SamplingState::SamplingPaused:
                          promiseHandleInMT->MaybeReject(NS_ERROR_FAILURE);
                          break;

                        case SamplingState::NoStackSamplingCompleted:
                        case SamplingState::SamplingCompleted: {
                          JS::RootedValue val(jsapi.cx());
                          promiseHandleInMT->MaybeResolve(val);
                        } break;

                        default:
                          MOZ_ASSERT(false, "Unexpected SamplingState value");
                          promiseHandleInMT->MaybeReject(
                              NS_ERROR_DOM_UNKNOWN_ERR);
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

namespace {
struct StringWriteFunc : public JSONWriteFunc {
  nsAString& mBuffer;  // This struct must not outlive this buffer
  explicit StringWriteFunc(nsAString& buffer) : mBuffer(buffer) {}

  void Write(const char* aStr) override {
    mBuffer.Append(NS_ConvertUTF8toUTF16(aStr));
  }
};
}  // namespace

NS_IMETHODIMP
nsProfiler::GetSharedLibraries(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aResult) {
  JS::RootedValue val(aCx);
  {
    nsString buffer;
    JSONWriter w(MakeUnique<StringWriteFunc>(buffer));
    w.StartArrayElement();
    AppendSharedLibraries(w);
    w.EndArray();
    MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx,
                                 static_cast<const char16_t*>(buffer.get()),
                                 buffer.Length(), &val));
  }
  JS::RootedObject obj(aCx, &val.toObject());
  if (!obj) {
    return NS_ERROR_FAILURE;
  }
  aResult.setObject(*obj);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetActiveConfiguration(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aResult) {
  JS::RootedValue jsValue(aCx);
  {
    nsString buffer;
    JSONWriter writer(MakeUnique<StringWriteFunc>(buffer));
    profiler_write_active_configuration(writer);
    MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx,
                                 static_cast<const char16_t*>(buffer.get()),
                                 buffer.Length(), &jsValue));
  }
  if (jsValue.isNull()) {
    aResult.setNull();
  } else {
    JS::RootedObject obj(aCx, &jsValue.toObject());
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

  JS::RootedValue val(aCx);
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
          [promise](nsCString aResult) {
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(promise->GetGlobalObject()))) {
              // We're really hosed if we can't get a JS context for some
              // reason.
              promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
              return;
            }

            JSContext* cx = jsapi.cx();

            // Now parse the JSON so that we resolve with a JS Object.
            JS::RootedValue val(cx);
            {
              NS_ConvertUTF8toUTF16 js_string(aResult);
              if (!JS_ParseJSON(cx,
                                static_cast<const char16_t*>(js_string.get()),
                                js_string.Length(), &val)) {
                if (!jsapi.HasException()) {
                  promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
                } else {
                  JS::RootedValue exn(cx);
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
          [promise](nsCString aResult) {
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(promise->GetGlobalObject()))) {
              // We're really hosed if we can't get a JS context for some
              // reason.
              promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
              return;
            }

            JSContext* cx = jsapi.cx();
            JSObject* typedArray = dom::ArrayBuffer::Create(
                cx, aResult.Length(),
                reinterpret_cast<const uint8_t*>(aResult.Data()));
            if (typedArray) {
              JS::RootedValue val(cx, JS::ObjectValue(*typedArray));
              promise->MaybeResolve(val);
            } else {
              promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
            }
          },
          [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
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
          [promise](nsCString aResult) {
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(promise->GetGlobalObject()))) {
              // We're really hosed if we can't get a JS context for some
              // reason.
              promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
              return;
            }

            // Compress a buffer via zlib (as with `compress()`), but emit a
            // gzip header as well. Like `compress()`, this is limited to 4GB in
            // size, but that shouldn't be an issue for our purposes.
            uLongf outSize = compressBound(aResult.Length());
            FallibleTArray<uint8_t> outBuff;
            if (!outBuff.SetLength(outSize, fallible)) {
              promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
              return;
            }

            int zerr;
            z_stream stream;
            stream.zalloc = nullptr;
            stream.zfree = nullptr;
            stream.opaque = nullptr;
            stream.next_out = (Bytef*)outBuff.Elements();
            stream.avail_out = outBuff.Length();
            stream.next_in = (z_const Bytef*)aResult.Data();
            stream.avail_in = aResult.Length();

            // A windowBits of 31 is the default (15) plus 16 for emitting a
            // gzip header; a memLevel of 8 is the default.
            zerr = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                                /* windowBits */ 31, /* memLevel */ 8,
                                Z_DEFAULT_STRATEGY);
            if (zerr != Z_OK) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            zerr = deflate(&stream, Z_FINISH);
            outSize = stream.total_out;
            deflateEnd(&stream);

            if (zerr != Z_STREAM_END) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            outBuff.TruncateLength(outSize);

            JSContext* cx = jsapi.cx();
            JSObject* typedArray = dom::ArrayBuffer::Create(
                cx, outBuff.Length(), outBuff.Elements());
            if (typedArray) {
              JS::RootedValue val(cx, JS::ObjectValue(*typedArray));
              promise->MaybeResolve(val);
            } else {
              promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
            }
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
          [filename, promise](const nsCString& aResult) {
            nsCOMPtr<nsIFile> file =
                do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
            nsresult rv = file->InitWithNativePath(filename);
            if (NS_FAILED(rv)) {
              MOZ_CRASH();
            }
            nsCOMPtr<nsIFileOutputStream> of =
                do_CreateInstance("@mozilla.org/network/file-output-stream;1");
            of->Init(file, -1, -1, 0);
            uint32_t sz;
            of->Write(aResult.get(), aResult.Length(), &sz);
            of->Close();

            promise->MaybeResolveWithUndefined();
          },
          [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetSymbolTable(const nsACString& aDebugPath,
                           const nsACString& aBreakpadID, JSContext* aCx,
                           nsISupports** aPromise) {
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

            JS::RootedObject addrsArray(
                cx, dom::Uint32Array::Create(cx, aSymbolTable.mAddrs.Length(),
                                             aSymbolTable.mAddrs.Elements()));
            JS::RootedObject indexArray(
                cx, dom::Uint32Array::Create(cx, aSymbolTable.mIndex.Length(),
                                             aSymbolTable.mIndex.Elements()));
            JS::RootedObject bufferArray(
                cx, dom::Uint8Array::Create(cx, aSymbolTable.mBuffer.Length(),
                                            aSymbolTable.mBuffer.Elements()));

            if (addrsArray && indexArray && bufferArray) {
              JS::RootedObject tuple(cx, JS::NewArrayObject(cx, 3));
              JS_SetElement(cx, tuple, 0, addrsArray);
              JS_SetElement(cx, tuple, 1, indexArray);
              JS_SetElement(cx, tuple, 2, bufferArray);
              promise->MaybeResolve(tuple);
            } else {
              promise->MaybeReject(NS_ERROR_FAILURE);
            }
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

void nsProfiler::GatheredOOPProfile(const nsACString& aProfile) {
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

  if (!aProfile.IsEmpty()) {
    mWriter->Splice(PromiseFlatCString(aProfile).get());
  }

  mPendingProfiles--;

  if (mPendingProfiles == 0) {
    // We've got all of the async profiles now. Let's
    // finish off the profile and resolve the Promise.
    FinishGathering();
  }
}

void nsProfiler::ReceiveShutdownProfile(const nsCString& aProfile) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  profiler_received_exit_profile(aProfile);
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

  // Request profiles from the other processes. This will trigger asynchronous
  // calls to ProfileGatherer::GatheredOOPProfile as the profiles arrive.
  //
  // Do this before the call to profiler_stream_json_for_this_process() because
  // that call is slow and we want to let the other processes grab their
  // profiles as soon as possible.
  nsTArray<RefPtr<ProfilerParent::SingleProcessProfilePromise>> profiles =
      ProfilerParent::GatherProfiles();

  mWriter.emplace();

  UniquePtr<ProfilerCodeAddressService> service =
      profiler_code_address_service_for_presymbolication();

  // Start building up the JSON result and grab the profile from this process.
  mWriter->Start();
  if (!profiler_stream_json_for_this_process(*mWriter, aSinceTime,
                                             /* aIsShuttingDown */ false,
                                             service.get())) {
    // The profiler is inactive. This either means that it was inactive even
    // at the time that ProfileGatherer::Start() was called, or that it was
    // stopped on a different thread since that call. Either way, we need to
    // reject the promise and stop gathering.
    return GatheringPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  mWriter->StartArrayProperty("processes");

  // If we have any process exit profiles, add them immediately.
  Vector<nsCString> exitProfiles = profiler_move_exit_profiles();
  for (auto& exitProfile : exitProfiles) {
    if (!exitProfile.IsEmpty()) {
      mWriter->Splice(exitProfile.get());
    }
  }

  mPromiseHolder.emplace();
  RefPtr<GatheringPromise> promise = mPromiseHolder->Ensure(__func__);

  // Keep the array property "processes" and the root object in mWriter open
  // until FinishGathering() is called. As profiles from the other processes
  // come in, they will be inserted and end up in the right spot.
  // FinishGathering() will close the array and the root object.

  mPendingProfiles = profiles.Length();
  RefPtr<nsProfiler> self = this;
  for (auto profile : profiles) {
    profile->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [self](mozilla::ipc::Shmem&& aResult) {
          const nsDependentCSubstring profileString(aResult.get<char>(),
                                                    aResult.Size<char>() - 1);
          self->GatheredOOPProfile(profileString);
        },
        [self](ipc::ResponseRejectReason&& aReason) {
          self->GatheredOOPProfile(NS_LITERAL_CSTRING(""));
        });
  }
  if (!mPendingProfiles) {
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

  mSymbolTableThread->Dispatch(NS_NewRunnableFunction(
      "nsProfiler::GetSymbolTableMozPromise runnable on ProfSymbolTable thread",
      [promiseHolder = std::move(promiseHolder),
       debugPath = nsCString(aDebugPath),
       breakpadID = nsCString(aBreakpadID)]() mutable {
        AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING("profiler_get_symbol_table",
                                              OTHER, debugPath);
        SymbolTable symbolTable;
        bool succeeded = profiler_get_symbol_table(
            debugPath.get(), breakpadID.get(), &symbolTable);
        SchedulerGroup::Dispatch(
            TaskCategory::Other,
            NS_NewRunnableFunction(
                "nsProfiler::GetSymbolTableMozPromise result on main thread",
                [promiseHolder = std::move(promiseHolder),
                 symbolTable = std::move(symbolTable), succeeded]() mutable {
                  if (succeeded) {
                    promiseHolder.Resolve(std::move(symbolTable), __func__);
                  } else {
                    promiseHolder.Reject(NS_ERROR_FAILURE, __func__);
                  }
                }));
      }));

  return promise;
}

void nsProfiler::FinishGathering() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(mWriter.isSome());
  MOZ_RELEASE_ASSERT(mPromiseHolder.isSome());

  // Close the "processes" array property.
  mWriter->EndArray();

  // Close the root object of the generated JSON.
  mWriter->End();

  UniquePtr<char[]> buf = mWriter->WriteFunc()->CopyData();
  size_t len = strlen(buf.get());
  nsCString result;
  result.Adopt(buf.release(), len);
  mPromiseHolder->Resolve(std::move(result), __func__);

  ResetGathering();
}

void nsProfiler::ResetGathering() {
  mPromiseHolder.reset();
  mPendingProfiles = 0;
  mGathering = false;
  mWriter.reset();
}
