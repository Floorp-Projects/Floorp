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
#include "js/PropertyAndElement.h"  // JS_SetElement
#include "js/Value.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Services.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/Preferences.h"
#include "nsComponentManagerUtils.h"
#include "nsIFileStreams.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadContext.h"
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

NS_IMPL_ISUPPORTS(nsProfiler, nsIProfiler)

nsProfiler::nsProfiler() : mGathering(false) {}

nsProfiler::~nsProfiler() {
  if (mSymbolTableThread) {
    mSymbolTableThread->Shutdown();
  }
  ResetGathering();
}

nsresult nsProfiler::Init() { return NS_OK; }

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
                          uint64_t aActiveTabID, double aDuration) {
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
                 aActiveTabID, duration);

  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::StopProfiler() {
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
nsProfiler::Pause() {
  profiler_pause();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::Resume() {
  profiler_resume();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::IsSamplingPaused(bool* aIsSamplingPaused) {
  *aIsSamplingPaused = profiler_is_sampling_paused();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::PauseSampling() {
  profiler_pause_sampling();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::ResumeSampling() {
  profiler_resume_sampling();
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

  void Write(const Span<const char>& aStr) override {
    mBuffer.Append(NS_ConvertUTF8toUTF16(aStr.data(), aStr.size()));
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

bool nsProfiler::SendProgressRequest(PendingProfile& aPendingProfile) {
  RefPtr<ProfilerParent::SingleProcessProgressPromise> progressPromise =
      ProfilerParent::RequestGatherProfileProgress(aPendingProfile.childPid);
  if (!progressPromise) {
    LOG("RequestGatherProfileProgress(%u) -> null!",
        unsigned(aPendingProfile.childPid));
    // Failed to send request.
    return false;
  }

  DEBUG_LOG("RequestGatherProfileProgress(%u) sent...",
            unsigned(aPendingProfile.childPid));
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

void nsProfiler::GatheredOOPProfile(base::ProcessId aChildPid,
                                    const nsACString& aProfile) {
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
    // TODO: Remove PromiseFlatCString, see bug 1657033.
    mWriter->Splice(PromiseFlatCString(aProfile));
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

RefPtr<nsProfiler::GatheringPromise> nsProfiler::StartGathering(
    double aSinceTime) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (mGathering) {
    // If we're already gathering, return a rejected promise - this isn't
    // going to end well.
    return GatheringPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  mGathering = true;

  if (mGatheringTimer) {
    mGatheringTimer->Cancel();
    mGatheringTimer = nullptr;
  }

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
    return GatheringPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

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
      mWriter->Splice(exitProfile);
    }
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
      profile.profilePromise->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self = RefPtr<nsProfiler>(this),
           childPid = profile.childPid](mozilla::ipc::Shmem&& aResult) {
            PendingProfile* pendingProfile = self->GetPendingProfile(childPid);
            LOG("GatherProfile(%u) response: %u bytes (%u were pending, %s %u)",
                unsigned(childPid), unsigned(aResult.Size<char>()),
                unsigned(self->mPendingProfiles.length()),
                pendingProfile ? "including" : "excluding", unsigned(childPid));
            const nsDependentCSubstring profileString(aResult.get<char>(),
                                                      aResult.Size<char>() - 1);
            self->GatheredOOPProfile(childPid, profileString);
          },
          [self = RefPtr<nsProfiler>(this),
           childPid = profile.childPid](ipc::ResponseRejectReason&& aReason) {
            PendingProfile* pendingProfile = self->GetPendingProfile(childPid);
            LOG("GatherProfile(%u) rejection: %d (%u were pending, %s %u)",
                unsigned(childPid), (int)aReason,
                unsigned(self->mPendingProfiles.length()),
                pendingProfile ? "including" : "excluding", unsigned(childPid));
            self->GatheredOOPProfile(childPid, ""_ns);
          });
    }
  } else {
    // There are no pending profiles, we're already done.
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

  // Close the "processes" array property.
  mWriter->EndArray();

  // Close the root object of the generated JSON.
  mWriter->End();

  UniquePtr<char[]> buf = mWriter->ChunkedWriteFunc().CopyData();
  size_t len = strlen(buf.get());
  nsCString result;
  result.Adopt(buf.release(), len);
  mPromiseHolder->Resolve(std::move(result), __func__);

  ResetGathering();
}

void nsProfiler::ResetGathering() {
  // If we have an unfulfilled Promise in flight, we should reject it before
  // destroying the promise holder.
  if (mPromiseHolder.isSome()) {
    mPromiseHolder->RejectIfExists(NS_ERROR_DOM_ABORT_ERR, __func__);
    mPromiseHolder.reset();
  }
  mPendingProfiles.clearAndFree();
  mGathering = false;
  if (mGatheringTimer) {
    mGatheringTimer->Cancel();
    mGatheringTimer = nullptr;
  }
  mWriter.reset();
}
