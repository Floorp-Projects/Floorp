/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <sstream>
#include "GeckoProfiler.h"
#include "nsIFileStreams.h"
#include "nsProfiler.h"
#include "nsProfilerStartParams.h"
#include "nsMemory.h"
#include "nsString.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"
#include "nsIWebNavigation.h"
#include "nsIInterfaceRequestorUtils.h"
#include "shared-libraries.h"
#include "js/Value.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"
#include "ProfileGatherer.h"
#include "nsLocalFile.h"
#include "platform.h"

using namespace mozilla;

using dom::AutoJSAPI;
using dom::Promise;
using std::string;

NS_IMPL_ISUPPORTS(nsProfiler, nsIProfiler)

nsProfiler::nsProfiler()
  : mLockedForPrivateBrowsing(false)
{
  // If MOZ_PROFILER_STARTUP is set, the profiler will already be running. We
  // need to create a ProfileGatherer in that case.
  // XXX: this is probably not the best approach. See bug 1356694 for details.
  if (profiler_is_active()) {
    mGatherer = new ProfileGatherer();
  }
}

nsProfiler::~nsProfiler()
{
  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, "chrome-document-global-created");
    observerService->RemoveObserver(this, "last-pb-context-exited");
  }
}

nsresult
nsProfiler::Init() {
  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "chrome-document-global-created", false);
    observerService->AddObserver(this, "last-pb-context-exited", false);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::Observe(nsISupports *aSubject,
                    const char *aTopic,
                    const char16_t *aData)
{
  // The profiler's handling of private browsing is as simple as possible: it
  // is stopped when the first PB window opens, and left stopped when the last
  // PB window closes.
  if (strcmp(aTopic, "chrome-document-global-created") == 0) {
    nsCOMPtr<nsIInterfaceRequestor> requestor = do_QueryInterface(aSubject);
    nsCOMPtr<nsIWebNavigation> parentWebNav = do_GetInterface(requestor);
    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(parentWebNav);
    if (loadContext && loadContext->UsePrivateBrowsing() && !mLockedForPrivateBrowsing) {
      mLockedForPrivateBrowsing = true;
      profiler_stop();
    }
  } else if (strcmp(aTopic, "last-pb-context-exited") == 0) {
    mLockedForPrivateBrowsing = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::CanProfile(bool *aCanProfile)
{
  *aCanProfile = !mLockedForPrivateBrowsing;
  return NS_OK;
}

static bool
HasFeature(const char** aFeatures, uint32_t aFeatureCount, const char* aFeature)
{
  for (size_t i = 0; i < aFeatureCount; i++) {
    if (strcmp(aFeatures[i], aFeature) == 0) {
      return true;
    }
  }
  return false;
}

NS_IMETHODIMP
nsProfiler::StartProfiler(uint32_t aEntries, double aInterval,
                          const char** aFeatures, uint32_t aFeatureCount,
                          const char** aFilters, uint32_t aFilterCount)
{
  if (mLockedForPrivateBrowsing) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  #define ADD_FEATURE_BIT(n_, str_, Name_) \
    if (HasFeature(aFeatures, aFeatureCount, str_)) { \
      features |= ProfilerFeature::Name_; \
    }

  // Convert the array of strings to a bitfield.
  uint32_t features = 0;
  PROFILER_FOR_EACH_FEATURE(ADD_FEATURE_BIT)

  #undef ADD_FEATURE_BIT

  profiler_start(aEntries, aInterval, features, aFilters, aFilterCount);

  // Do this after profiler_start().
  mGatherer = new ProfileGatherer();

  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::StopProfiler()
{
  // Do this before profiler_stop().
  mGatherer = nullptr;

  profiler_stop();

  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::IsPaused(bool *aIsPaused)
{
  *aIsPaused = profiler_is_paused();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::PauseSampling()
{
  profiler_pause();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::ResumeSampling()
{
  profiler_resume();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::AddMarker(const char *aMarker)
{
  PROFILER_MARKER(aMarker);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfile(double aSinceTime, char** aProfile)
{
  mozilla::UniquePtr<char[]> profile = profiler_get_profile(aSinceTime);
  if (profile) {
    size_t len = strlen(profile.get());
    char *profileStr = static_cast<char *>
                         (nsMemory::Clone(profile.get(), (len + 1) * sizeof(char)));
    profileStr[len] = '\0';
    *aProfile = profileStr;
  }
  return NS_OK;
}

namespace {
  struct StringWriteFunc : public JSONWriteFunc
  {
    nsAString& mBuffer; // This struct must not outlive this buffer
    explicit StringWriteFunc(nsAString& buffer) : mBuffer(buffer) {}

    void Write(const char* aStr)
    {
      mBuffer.Append(NS_ConvertUTF8toUTF16(aStr));
    }
  };
}

NS_IMETHODIMP
nsProfiler::GetSharedLibraries(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aResult)
{
  JS::RootedValue val(aCx);
  {
    nsString buffer;
    JSONWriter w(MakeUnique<StringWriteFunc>(buffer));
    w.StartArrayElement();
    AppendSharedLibraries(w);
    w.EndArray();
    MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx, static_cast<const char16_t*>(buffer.get()),
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
nsProfiler::DumpProfileToFile(const char* aFilename)
{
  profiler_save_profile_to_file(aFilename);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfileData(double aSinceTime, JSContext* aCx,
                           JS::MutableHandle<JS::Value> aResult)
{
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
                                nsISupports** aPromise)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mGatherer) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* go = xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));

  if (NS_WARN_IF(!go)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(go, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  mGatherer->Start(aSinceTime)->Then(
    AbstractThread::MainThread(), __func__,
    [promise](nsCString aResult) {
      AutoJSAPI jsapi;
      if (NS_WARN_IF(!jsapi.Init(promise->GlobalJSObject()))) {
        // We're really hosed if we can't get a JS context for some reason.
        promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
        return;
      }

      JSContext* cx = jsapi.cx();

      // Now parse the JSON so that we resolve with a JS Object.
      JS::RootedValue val(cx);
      {
        NS_ConvertUTF8toUTF16 js_string(aResult);
        if (!JS_ParseJSON(cx, static_cast<const char16_t*>(js_string.get()),
                          js_string.Length(), &val)) {
          if (!jsapi.HasException()) {
            promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
          } else {
            JS::RootedValue exn(cx);
            DebugOnly<bool> gotException = jsapi.StealException(&exn);
            MOZ_ASSERT(gotException);

            jsapi.ClearException();
            promise->MaybeReject(cx, exn);
          }
        } else {
          promise->MaybeResolve(val);
        }
      }
    },
    [promise](nsresult aRv) {
      promise->MaybeReject(aRv);
    });

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::DumpProfileToFileAsync(const nsACString& aFilename,
                                   double aSinceTime)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mGatherer) {
    return NS_ERROR_FAILURE;
  }

  nsCString filename(aFilename);

  mGatherer->Start(aSinceTime)->Then(
    AbstractThread::MainThread(), __func__,
    [filename](const nsCString& aResult) {
      nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
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
    },
    [](nsresult aRv) { });

  return NS_OK;
}


NS_IMETHODIMP
nsProfiler::GetElapsedTime(double* aElapsedTime)
{
  *aElapsedTime = profiler_time();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::IsActive(bool *aIsActive)
{
  *aIsActive = profiler_is_active();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetFeatures(uint32_t* aCount, char*** aFeatureList)
{
  uint32_t features = profiler_get_available_features();

  #define COUNT_IF_SET(n_, str_, Name_) \
    if (ProfilerFeature::Has##Name_(features)) { \
      len++; \
    }

  // Count the number of features in use.
  uint32_t len = 0;
  PROFILER_FOR_EACH_FEATURE(COUNT_IF_SET)

  #undef COUNT_IF_SET

  auto featureList = static_cast<char**>(moz_xmalloc(len * sizeof(char*)));

  #define DUP_IF_SET(n_, str_, Name_) \
    if (ProfilerFeature::Has##Name_(features)) { \
      size_t strLen = strlen(str_); \
      featureList[i] = static_cast<char*>( \
        nsMemory::Clone(str_, (strLen + 1) * sizeof(char))); \
      i++; \
    }

  // Insert the strings for the features in use.
  size_t i = 0;
  PROFILER_FOR_EACH_FEATURE(DUP_IF_SET)

  #undef STRDUP_IF_SET

  *aFeatureList = featureList;
  *aCount = len;
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetStartParams(nsIProfilerStartParams** aRetVal)
{
  if (!profiler_is_active()) {
    *aRetVal = nullptr;
  } else {
    int entries = 0;
    double interval = 0;
    uint32_t features = 0;
    mozilla::Vector<const char*> filters;
    profiler_get_start_params(&entries, &interval, &features, &filters);

    nsTArray<nsCString> filtersArray;
    for (uint32_t i = 0; i < filters.length(); ++i) {
      filtersArray.AppendElement(filters[i]);
    }

    nsCOMPtr<nsIProfilerStartParams> startParams =
      new nsProfilerStartParams(entries, interval, features, filtersArray);

    startParams.forget(aRetVal);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetBufferInfo(uint32_t* aCurrentPosition, uint32_t* aTotalSize,
                          uint32_t* aGeneration)
{
  MOZ_ASSERT(aCurrentPosition);
  MOZ_ASSERT(aTotalSize);
  MOZ_ASSERT(aGeneration);
  profiler_get_buffer_info(aCurrentPosition, aTotalSize, aGeneration);
  return NS_OK;
}

void
nsProfiler::WillGatherOOPProfile()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!mGatherer) {
    return;
  }

  mGatherer->WillGatherOOPProfile();
}

void
nsProfiler::GatheredOOPProfile(const nsACString& aProfile)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!mGatherer) {
    return;
  }

  mGatherer->GatheredOOPProfile(aProfile);
}

void
nsProfiler::OOPExitProfile(const nsACString& aProfile)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!mGatherer) {
    return;
  }

  mGatherer->OOPExitProfile(aProfile);
}

