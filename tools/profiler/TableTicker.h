/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "platform.h"
#include "ProfileEntry.h"
#include "mozilla/Mutex.h"

static bool
hasFeature(const char** aFeatures, uint32_t aFeatureCount, const char* aFeature) {
  for(size_t i = 0; i < aFeatureCount; i++) {
    if (strcmp(aFeatures[i], aFeature) == 0)
      return true;
  }
  return false;
}

extern TimeStamp sLastTracerEvent;
extern int sFrameNumber;
extern int sLastFrameNumber;
extern unsigned int sCurrentEventGeneration;
extern unsigned int sLastSampledEventGeneration;

class BreakpadSampler;

class TableTicker: public Sampler {
 public:
  TableTicker(int aInterval, int aEntrySize,
              const char** aFeatures, uint32_t aFeatureCount)
    : Sampler(aInterval, true, aEntrySize)
    , mPrimaryThreadProfile(nullptr)
    , mSaveRequested(false)
    , mUnwinderThread(false)
  {
    mUseStackWalk = hasFeature(aFeatures, aFeatureCount, "stackwalk");

    //XXX: It's probably worth splitting the jank profiler out from the regular profiler at some point
    mJankOnly = hasFeature(aFeatures, aFeatureCount, "jank");
    mProfileJS = hasFeature(aFeatures, aFeatureCount, "js");
    mProfileJava = hasFeature(aFeatures, aFeatureCount, "java");
    mProfileThreads = hasFeature(aFeatures, aFeatureCount, "threads");
    mUnwinderThread = hasFeature(aFeatures, aFeatureCount, "unwinder") || sps_version2();
    mAddLeafAddresses = hasFeature(aFeatures, aFeatureCount, "leaf");

    sStartTime = TimeStamp::Now();

    {
      mozilla::MutexAutoLock lock(*sRegisteredThreadsMutex);

      // Create ThreadProfile for each registered thread
      for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
        ThreadInfo* info = sRegisteredThreads->at(i);

        if (!info->IsMainThread() && !mProfileThreads)
          continue;

        ThreadProfile* profile = new ThreadProfile(info->Name(),
                                                   aEntrySize,
                                                   info->Stack(),
                                                   info->ThreadId(),
                                                   info->GetPlatformData(),
                                                   info->IsMainThread());
        profile->addTag(ProfileEntry('m', "Start"));

        info->SetProfile(profile);
      }

      SetActiveSampler(this);
    }
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
      }
    }
  }

  // Called within a signal. This function must be reentrant
  virtual void Tick(TickSample* sample);

  // Called within a signal. This function must be reentrant
  virtual void RequestSave()
  {
    mSaveRequested = true;
  }

  virtual void HandleSaveRequest();

  ThreadProfile* GetPrimaryThreadProfile()
  {
    if (!mPrimaryThreadProfile) {
      mozilla::MutexAutoLock lock(*sRegisteredThreadsMutex);

      for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
        ThreadInfo* info = sRegisteredThreads->at(i);
        if (info->IsMainThread()) {
          mPrimaryThreadProfile = info->Profile();
          break;
        }
      }
    }

    return mPrimaryThreadProfile;
  }

  void ToStreamAsJSON(std::ostream& stream);
  virtual JSObject *ToJSObject(JSContext *aCx);
  JSCustomObject *GetMetaJSCustomObject(JSAObjectBuilder& b);

  bool HasUnwinderThread() const { return mUnwinderThread; }
  bool ProfileJS() const { return mProfileJS; }
  bool ProfileJava() const { return mProfileJava; }
  bool ProfileThreads() const { return mProfileThreads; }

protected:
  // Called within a signal. This function must be reentrant
  virtual void UnwinderTick(TickSample* sample);

  // Called within a signal. This function must be reentrant
  virtual void InplaceTick(TickSample* sample);

  // Not implemented on platforms which do not support backtracing
  void doNativeBacktrace(ThreadProfile &aProfile, TickSample* aSample);

  void BuildJSObject(JSAObjectBuilder& b, JSCustomObject* profile);

  // This represent the application's main thread (SAMPLER_INIT)
  ThreadProfile* mPrimaryThreadProfile;
  bool mSaveRequested;
  bool mAddLeafAddresses;
  bool mUseStackWalk;
  bool mJankOnly;
  bool mProfileJS;
  bool mProfileThreads;
  bool mUnwinderThread;
  bool mProfileJava;
};

