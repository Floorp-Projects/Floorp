/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoSampler_h
#define GeckoSampler_h

#include "platform.h"
#include "ProfileEntry.h"
#include "mozilla/Vector.h"
#include "ThreadProfile.h"
#include "ThreadInfo.h"
#ifndef SPS_STANDALONE
#include "IntelPowerGadget.h"
#endif
#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#endif

namespace mozilla {
class ProfileGatherer;
} // namespace mozilla

typedef mozilla::Vector<std::string> ThreadNameFilterList;

static bool
threadSelected(ThreadInfo* aInfo, const ThreadNameFilterList &aThreadNameFilters) {
  if (aThreadNameFilters.empty()) {
    return true;
  }

  for (uint32_t i = 0; i < aThreadNameFilters.length(); ++i) {
    if (aThreadNameFilters[i] == aInfo->Name()) {
      return true;
    }
  }

  return false;
}

extern mozilla::TimeStamp sLastTracerEvent;
extern int sFrameNumber;
extern int sLastFrameNumber;

class GeckoSampler: public Sampler {
 public:
  GeckoSampler(double aInterval, int aEntrySize,
              const char** aFeatures, uint32_t aFeatureCount,
              const char** aThreadNameFilters, uint32_t aFilterCount);
  ~GeckoSampler();

  void RegisterThread(ThreadInfo* aInfo) {
    if (!aInfo->IsMainThread() && !mProfileThreads) {
      return;
    }

    if (!threadSelected(aInfo, mThreadNameFilters)) {
      return;
    }

    ThreadProfile* profile = new ThreadProfile(aInfo, mBuffer);
    aInfo->SetProfile(profile);
  }

  // Called within a signal. This function must be reentrant
  virtual void Tick(TickSample* sample) override;

  // Immediately captures the calling thread's call stack and returns it.
  virtual SyncProfile* GetBacktrace() override;

  // Called within a signal. This function must be reentrant
  virtual void RequestSave() override
  {
    mSaveRequested = true;
#ifdef MOZ_TASK_TRACER
    if (mTaskTracer) {
      mozilla::tasktracer::StopLogging();
    }
#endif
  }

  virtual void HandleSaveRequest() override;
  virtual void DeleteExpiredMarkers() override;

  ThreadProfile* GetPrimaryThreadProfile()
  {
    if (!mPrimaryThreadProfile) {
      ::MutexAutoLock lock(*sRegisteredThreadsMutex);

      for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
        ThreadInfo* info = sRegisteredThreads->at(i);
        if (info->IsMainThread() && !info->IsPendingDelete()) {
          mPrimaryThreadProfile = info->Profile();
          break;
        }
      }
    }

    return mPrimaryThreadProfile;
  }

  void ToStreamAsJSON(std::ostream& stream, double aSinceTime = 0);
#ifndef SPS_STANDALONE
  virtual JSObject *ToJSObject(JSContext *aCx, double aSinceTime = 0);
#endif
  mozilla::UniquePtr<char[]> ToJSON(double aSinceTime = 0);
  virtual void ToJSObjectAsync(double aSinceTime = 0, mozilla::dom::Promise* aPromise = 0);
  void StreamMetaJSCustomObject(SpliceableJSONWriter& aWriter);
  void StreamTaskTracer(SpliceableJSONWriter& aWriter);
  void FlushOnJSShutdown(JSRuntime* aRuntime);
  bool ProfileJS() const { return mProfileJS; }
  bool ProfileJava() const { return mProfileJava; }
  bool ProfileGPU() const { return mProfileGPU; }
  bool ProfilePower() const { return mProfilePower; }
  bool ProfileThreads() const override { return mProfileThreads; }
  bool InPrivacyMode() const { return mPrivacyMode; }
  bool AddMainThreadIO() const { return mAddMainThreadIO; }
  bool ProfileMemory() const { return mProfileMemory; }
  bool TaskTracer() const { return mTaskTracer; }
  bool LayersDump() const { return mLayersDump; }
  bool DisplayListDump() const { return mDisplayListDump; }
  bool ProfileRestyle() const { return mProfileRestyle; }

  void GetBufferInfo(uint32_t *aCurrentPosition, uint32_t *aTotalSize, uint32_t *aGeneration);

  void ProfileGathered();

protected:
  // Called within a signal. This function must be reentrant
  virtual void InplaceTick(TickSample* sample);

  // Not implemented on platforms which do not support backtracing
  void doNativeBacktrace(ThreadProfile &aProfile, TickSample* aSample);

  void StreamJSON(SpliceableJSONWriter& aWriter, double aSinceTime);

  // This represent the application's main thread (SAMPLER_INIT)
  ThreadProfile* mPrimaryThreadProfile;
  mozilla::RefPtr<ProfileBuffer> mBuffer;
  bool mSaveRequested;
  bool mAddLeafAddresses;
  bool mUseStackWalk;
  bool mProfileJS;
  bool mProfileGPU;
  bool mProfileThreads;
  bool mProfileJava;
  bool mProfilePower;
  bool mLayersDump;
  bool mDisplayListDump;
  bool mProfileRestyle;

  // Keep the thread filter to check against new thread that
  // are started while profiling
  ThreadNameFilterList mThreadNameFilters;
  bool mPrivacyMode;
  bool mAddMainThreadIO;
  bool mProfileMemory;
  bool mTaskTracer;
#if defined(XP_WIN)
  IntelPowerGadget* mIntelPowerGadget;
#endif

private:
  nsRefPtr<mozilla::ProfileGatherer> mGatherer;
};

#endif

