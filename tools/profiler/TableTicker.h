/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TableTicker_h
#define TableTicker_h

#include "platform.h"
#include "ProfileEntry.h"
#include "mozilla/Mutex.h"
#include "mozilla/Vector.h"
#include "IntelPowerGadget.h"
#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#endif

static bool
hasFeature(const char** aFeatures, uint32_t aFeatureCount, const char* aFeature) {
  for(size_t i = 0; i < aFeatureCount; i++) {
    if (strcmp(aFeatures[i], aFeature) == 0)
      return true;
  }
  return false;
}

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

class BreakpadSampler;

class TableTicker: public Sampler {
 public:
  TableTicker(double aInterval, int aEntrySize,
              const char** aFeatures, uint32_t aFeatureCount,
              const char** aThreadNameFilters, uint32_t aFilterCount)
    : Sampler(aInterval, true, aEntrySize)
    , mPrimaryThreadProfile(nullptr)
    , mBuffer(new ProfileBuffer(aEntrySize))
    , mSaveRequested(false)
#if defined(XP_WIN)
    , mIntelPowerGadget(nullptr)
#endif
  {
    mUseStackWalk = hasFeature(aFeatures, aFeatureCount, "stackwalk");

    mProfileJS = hasFeature(aFeatures, aFeatureCount, "js");
    mProfileJava = hasFeature(aFeatures, aFeatureCount, "java");
    mProfileGPU = hasFeature(aFeatures, aFeatureCount, "gpu");
    mProfilePower = hasFeature(aFeatures, aFeatureCount, "power");
    // Users sometimes ask to filter by a list of threads but forget to request
    // profiling non main threads. Let's make it implificit if we have a filter
    mProfileThreads = hasFeature(aFeatures, aFeatureCount, "threads") || aFilterCount > 0;
    mAddLeafAddresses = hasFeature(aFeatures, aFeatureCount, "leaf");
    mPrivacyMode = hasFeature(aFeatures, aFeatureCount, "privacy");
    mAddMainThreadIO = hasFeature(aFeatures, aFeatureCount, "mainthreadio");
    mProfileMemory = hasFeature(aFeatures, aFeatureCount, "memory");
    mTaskTracer = hasFeature(aFeatures, aFeatureCount, "tasktracer");
    mLayersDump = hasFeature(aFeatures, aFeatureCount, "layersdump");
    mDisplayListDump = hasFeature(aFeatures, aFeatureCount, "displaylistdump");
    mProfileRestyle = hasFeature(aFeatures, aFeatureCount, "restyle");

#if defined(XP_WIN)
    if (mProfilePower) {
      mIntelPowerGadget = new IntelPowerGadget();
      mProfilePower = mIntelPowerGadget->Init();
    }
#endif

    // Deep copy aThreadNameFilters
    MOZ_ALWAYS_TRUE(mThreadNameFilters.resize(aFilterCount));
    for (uint32_t i = 0; i < aFilterCount; ++i) {
      mThreadNameFilters[i] = aThreadNameFilters[i];
    }

    sStartTime = mozilla::TimeStamp::Now();

    {
      mozilla::MutexAutoLock lock(*sRegisteredThreadsMutex);

      // Create ThreadProfile for each registered thread
      for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
        ThreadInfo* info = sRegisteredThreads->at(i);

        RegisterThread(info);
      }

      SetActiveSampler(this);
    }

#ifdef MOZ_TASK_TRACER
    if (mTaskTracer) {
      mozilla::tasktracer::StartLogging();
    }
#endif
  }

  ~TableTicker() {
    if (IsActive())
      Stop();

    SetActiveSampler(nullptr);

    // Destroy ThreadProfile for all threads
    {
      mozilla::MutexAutoLock lock(*sRegisteredThreadsMutex);

      for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
        ThreadInfo* info = sRegisteredThreads->at(i);
        ThreadProfile* profile = info->Profile();
        if (profile) {
          delete profile;
          info->SetProfile(nullptr);
        }
        // We've stopped profiling. We no longer need to retain
        // information for an old thread.
        if (info->IsPendingDelete()) {
          delete info;
          sRegisteredThreads->erase(sRegisteredThreads->begin() + i);
          i--;
        }
      }
    }
#if defined(XP_WIN)
    delete mIntelPowerGadget;
#endif
  }

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
      mozilla::MutexAutoLock lock(*sRegisteredThreadsMutex);

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

  void ToStreamAsJSON(std::ostream& stream);
  virtual JSObject *ToJSObject(JSContext *aCx);
  void StreamMetaJSCustomObject(JSStreamWriter& b);
  void StreamTaskTracer(JSStreamWriter& b);
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

protected:
  // Called within a signal. This function must be reentrant
  virtual void InplaceTick(TickSample* sample);

  // Not implemented on platforms which do not support backtracing
  void doNativeBacktrace(ThreadProfile &aProfile, TickSample* aSample);

  void StreamJSObject(JSStreamWriter& b);

  // This represent the application's main thread (SAMPLER_INIT)
  ThreadProfile* mPrimaryThreadProfile;
  nsRefPtr<ProfileBuffer> mBuffer;
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
};

#endif

