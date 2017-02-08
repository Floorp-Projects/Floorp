/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <string>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include "GeckoProfiler.h"
#include "nsIProfileSaveEvent.h"
#include "nsThreadUtils.h"
#include "prenv.h"
#include "prtime.h"
#include "nsXULAppAPI.h"
#include "ProfileEntry.h"
#include "SyncProfile.h"
#include "platform.h"
#include "shared-libraries.h"
#include "mozilla/StackWalk.h"

// JSON
#include "ProfileJSONWriter.h"

// Meta
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsIHttpProtocolHandler.h"
#include "nsServiceManagerUtils.h"
#include "nsIXULRuntime.h"
#include "nsIXULAppInfo.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "PlatformMacros.h"
#include "nsTArray.h"

#include "mozilla/Preferences.h"

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
  #include "FennecJNIWrappers.h"
#endif

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#endif // MOZ_TASK_TRACER

// JS
#include "jsfriendapi.h"
#include "js/ProfilingFrameIterator.h"

using std::string;
using namespace mozilla;

#ifndef MAXPATHLEN
 #ifdef PATH_MAX
  #define MAXPATHLEN PATH_MAX
 #elif defined(MAX_PATH)
  #define MAXPATHLEN MAX_PATH
 #elif defined(_MAX_PATH)
  #define MAXPATHLEN _MAX_PATH
 #elif defined(CCHMAXPATH)
  #define MAXPATHLEN CCHMAXPATH
 #else
  #define MAXPATHLEN 1024
 #endif
#endif

///////////////////////////////////////////////////////////////////////
// BEGIN ProfileSaveEvent

class ProfileSaveEvent final : public nsIProfileSaveEvent {
public:
  typedef void (*AddSubProfileFunc)(const char* aProfile, void* aClosure);
  NS_DECL_ISUPPORTS

  ProfileSaveEvent(AddSubProfileFunc aFunc, void* aClosure)
    : mFunc(aFunc)
    , mClosure(aClosure)
  {}

  NS_IMETHOD AddSubProfile(const char* aProfile) override {
    mFunc(aProfile, mClosure);
    return NS_OK;
  }
private:
  ~ProfileSaveEvent() {}

  AddSubProfileFunc mFunc;
  void* mClosure;
};

NS_IMPL_ISUPPORTS(ProfileSaveEvent, nsIProfileSaveEvent)

// END ProfileSaveEvent
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// BEGIN SaveProfileTask et al

static void
AddSharedLibraryInfoToStream(std::ostream& aStream, const SharedLibrary& aLib)
{
  aStream << "{";
  aStream << "\"start\":" << aLib.GetStart();
  aStream << ",\"end\":" << aLib.GetEnd();
  aStream << ",\"offset\":" << aLib.GetOffset();
  aStream << ",\"name\":\"" << aLib.GetName() << "\"";
  const std::string &breakpadId = aLib.GetBreakpadId();
  aStream << ",\"breakpadId\":\"" << breakpadId << "\"";
#ifdef XP_WIN
  // FIXME: remove this XP_WIN code when the profiler plugin has switched to
  // using breakpadId.
  std::string pdbSignature = breakpadId.substr(0, 32);
  std::string pdbAgeStr = breakpadId.substr(32,  breakpadId.size() - 1);

  std::stringstream stream;
  stream << pdbAgeStr;

  unsigned pdbAge;
  stream << std::hex;
  stream >> pdbAge;

#ifdef DEBUG
  std::ostringstream oStream;
  oStream << pdbSignature << std::hex << std::uppercase << pdbAge;
  MOZ_ASSERT(breakpadId == oStream.str());
#endif

  aStream << ",\"pdbSignature\":\"" << pdbSignature << "\"";
  aStream << ",\"pdbAge\":" << pdbAge;
  aStream << ",\"pdbName\":\"" << aLib.GetName() << "\"";
#endif
  aStream << "}";
}

std::string
GetSharedLibraryInfoStringInternal()
{
  SharedLibraryInfo info = SharedLibraryInfo::GetInfoForSelf();
  if (info.GetSize() == 0)
    return "[]";

  std::ostringstream os;
  os << "[";
  AddSharedLibraryInfoToStream(os, info.GetEntry(0));

  for (size_t i = 1; i < info.GetSize(); i++) {
    os << ",";
    AddSharedLibraryInfoToStream(os, info.GetEntry(i));
  }

  os << "]";
  return os.str();
}

Sampler::Sampler()
{
  MOZ_COUNT_CTOR(Sampler);

  bool ignore;
  sStartTime = mozilla::TimeStamp::ProcessCreation(ignore);

  {
    StaticMutexAutoLock lock(sRegisteredThreadsMutex);

    // Set up profiling for each registered thread, if appropriate
    for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
      ThreadInfo* info = sRegisteredThreads->at(i);

      RegisterThread(info);
    }
  }

#ifdef MOZ_TASK_TRACER
  if (mTaskTracer) {
    mozilla::tasktracer::StartLogging();
  }
#endif
}

Sampler::~Sampler()
{
  MOZ_COUNT_DTOR(Sampler);

  if (gIsActive)
    PlatformStop();

  // Destroy ThreadInfo for all threads
  {
    StaticMutexAutoLock lock(sRegisteredThreadsMutex);

    for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
      ThreadInfo* info = sRegisteredThreads->at(i);
      // We've stopped profiling. We no longer need to retain
      // information for an old thread.
      if (info->IsPendingDelete()) {
        // The stack was nulled when SetPendingDelete() was called.
        MOZ_ASSERT(!info->Stack());
        delete info;
        sRegisteredThreads->erase(sRegisteredThreads->begin() + i);
        i--;
      }
    }
  }

#ifdef MOZ_TASK_TRACER
  if (mTaskTracer) {
    mozilla::tasktracer::StopLogging();
  }
#endif
}

void
Sampler::StreamTaskTracer(SpliceableJSONWriter& aWriter)
{
#ifdef MOZ_TASK_TRACER
  aWriter.StartArrayProperty("data");
    UniquePtr<nsTArray<nsCString>> data = mozilla::tasktracer::GetLoggedData(sStartTime);
    for (uint32_t i = 0; i < data->Length(); ++i) {
      aWriter.StringElement((data->ElementAt(i)).get());
    }
  aWriter.EndArray();

  aWriter.StartArrayProperty("threads");
  {
    StaticMutexAutoLock lock(sRegisteredThreadsMutex);

    for (size_t i = 0; i < sRegisteredThreads->size(); i++) {
      // Thread meta data
      ThreadInfo* info = sRegisteredThreads->at(i);
      aWriter.StartObjectElement();
      {
        if (XRE_GetProcessType() == GeckoProcessType_Plugin) {
          // TODO Add the proper plugin name
          aWriter.StringProperty("name", "Plugin");
        } else {
          aWriter.StringProperty("name", info->Name());
        }
        aWriter.IntProperty("tid", static_cast<int>(info->ThreadId()));
      }
      aWriter.EndObject();
    }
  }
  aWriter.EndArray();

  aWriter.DoubleProperty("start", static_cast<double>(mozilla::tasktracer::GetStartTime()));
#endif
}


void
Sampler::StreamMetaJSCustomObject(SpliceableJSONWriter& aWriter)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  aWriter.IntProperty("version", 3);
  aWriter.DoubleProperty("interval", gInterval);
  aWriter.IntProperty("stackwalk", gUseStackWalk);

#ifdef DEBUG
  aWriter.IntProperty("debug", 1);
#else
  aWriter.IntProperty("debug", 0);
#endif

  aWriter.IntProperty("gcpoison", JS::IsGCPoisoning() ? 1 : 0);

  bool asyncStacks = Preferences::GetBool("javascript.options.asyncstack");
  aWriter.IntProperty("asyncstack", asyncStacks);

  mozilla::TimeDuration delta = mozilla::TimeStamp::Now() - sStartTime;
  aWriter.DoubleProperty("startTime", static_cast<double>(PR_Now()/1000.0 - delta.ToMilliseconds()));

  aWriter.IntProperty("processType", XRE_GetProcessType());

  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler> http = do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &res);
  if (!NS_FAILED(res)) {
    nsAutoCString string;

    res = http->GetPlatform(string);
    if (!NS_FAILED(res))
      aWriter.StringProperty("platform", string.Data());

    res = http->GetOscpu(string);
    if (!NS_FAILED(res))
      aWriter.StringProperty("oscpu", string.Data());

    res = http->GetMisc(string);
    if (!NS_FAILED(res))
      aWriter.StringProperty("misc", string.Data());
  }

  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (runtime) {
    nsAutoCString string;

    res = runtime->GetXPCOMABI(string);
    if (!NS_FAILED(res))
      aWriter.StringProperty("abi", string.Data());

    res = runtime->GetWidgetToolkit(string);
    if (!NS_FAILED(res))
      aWriter.StringProperty("toolkit", string.Data());
  }

  nsCOMPtr<nsIXULAppInfo> appInfo = do_GetService("@mozilla.org/xre/app-info;1");
  if (appInfo) {
    nsAutoCString string;

    res = appInfo->GetName(string);
    if (!NS_FAILED(res))
      aWriter.StringProperty("product", string.Data());
  }
}

void
Sampler::ToStreamAsJSON(std::ostream& stream, double aSinceTime)
{
  SpliceableJSONWriter b(mozilla::MakeUnique<OStreamJSONWriteFunc>(stream));
  StreamJSON(b, aSinceTime);
}

JSObject*
Sampler::ToJSObject(JSContext *aCx, double aSinceTime)
{
  JS::RootedValue val(aCx);
  {
    UniquePtr<char[]> buf = ToJSON(aSinceTime);
    NS_ConvertUTF8toUTF16 js_string(nsDependentCString(buf.get()));
    MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx, static_cast<const char16_t*>(js_string.get()),
                                 js_string.Length(), &val));
  }
  return &val.toObject();
}

UniquePtr<char[]>
Sampler::ToJSON(double aSinceTime)
{
  SpliceableChunkedJSONWriter b;
  StreamJSON(b, aSinceTime);
  return b.WriteFunc()->CopyData();
}

struct SubprocessClosure {
  explicit SubprocessClosure(SpliceableJSONWriter* aWriter)
    : mWriter(aWriter)
  {}

  SpliceableJSONWriter* mWriter;
};

void SubProcessCallback(const char* aProfile, void* aClosure)
{
  // Called by the observer to get their profile data included
  // as a sub profile
  SubprocessClosure* closure = (SubprocessClosure*)aClosure;

  // Add the string profile into the profile
  closure->mWriter->StringElement(aProfile);
}


#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
static
void BuildJavaThreadJSObject(SpliceableJSONWriter& aWriter)
{
  aWriter.StringProperty("name", "Java Main Thread");

  aWriter.StartArrayProperty("samples");

    // for each sample
    for (int sampleId = 0; true; sampleId++) {
      bool firstRun = true;
      // for each frame
      for (int frameId = 0; true; frameId++) {
        jni::String::LocalRef frameName =
            java::GeckoJavaSampler::GetFrameName(0, sampleId, frameId);
        // when we run out of frames, we stop looping
        if (!frameName) {
          // if we found at least one frame, we have objects to close
          if (!firstRun) {
              aWriter.EndArray();
            aWriter.EndObject();
          }
          break;
        }
        // the first time around, open the sample object and frames array
        if (firstRun) {
          firstRun = false;

          double sampleTime =
              java::GeckoJavaSampler::GetSampleTime(0, sampleId);

          aWriter.StartObjectElement();
            aWriter.DoubleProperty("time", sampleTime);

            aWriter.StartArrayProperty("frames");
        }
        // add a frame to the sample
        aWriter.StartObjectElement();
          aWriter.StringProperty("location",
                                 frameName->ToCString().BeginReading());
        aWriter.EndObject();
      }
      // if we found no frames for this sample, we are done
      if (firstRun) {
        break;
      }
    }

  aWriter.EndArray();
}
#endif

void
Sampler::StreamJSON(SpliceableJSONWriter& aWriter, double aSinceTime)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  aWriter.Start(SpliceableJSONWriter::SingleLineStyle);
  {
    // Put shared library info
    aWriter.StringProperty("libs", GetSharedLibraryInfoStringInternal().c_str());

    // Put meta data
    aWriter.StartObjectProperty("meta");
      StreamMetaJSCustomObject(aWriter);
    aWriter.EndObject();

    // Data of TaskTracer doesn't belong in the circular buffer.
    if (gTaskTracer) {
      aWriter.StartObjectProperty("tasktracer");
      StreamTaskTracer(aWriter);
      aWriter.EndObject();
    }

    // Lists the samples for each thread profile
    aWriter.StartArrayProperty("threads");
    {
      gIsPaused = true;

      {
        StaticMutexAutoLock lock(sRegisteredThreadsMutex);

        for (size_t i = 0; i < sRegisteredThreads->size(); i++) {
          // Thread not being profiled, skip it
          ThreadInfo* info = sRegisteredThreads->at(i);
          if (!info->hasProfile()) {
            continue;
          }

          // Note that we intentionally include thread profiles which
          // have been marked for pending delete.

          MutexAutoLock lock(info->GetMutex());

          info->StreamJSON(aWriter, aSinceTime);
        }
      }

      if (Sampler::CanNotifyObservers()) {
        // Send a event asking any subprocesses (plugins) to
        // give us their information
        SubprocessClosure closure(&aWriter);
        nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
        if (os) {
          RefPtr<ProfileSaveEvent> pse = new ProfileSaveEvent(SubProcessCallback, &closure);
          os->NotifyObservers(pse, "profiler-subprocess", nullptr);
        }
      }

  #if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
      if (gProfileJava) {
        java::GeckoJavaSampler::Pause();

        aWriter.Start();
        {
          BuildJavaThreadJSObject(aWriter);
        }
        aWriter.End();

        java::GeckoJavaSampler::Unpause();
      }
  #endif

      gIsPaused = false;
    }
    aWriter.EndArray();
  }
  aWriter.End();
}

void PseudoStack::flushSamplerOnJSShutdown()
{
  MOZ_ASSERT(mContext);

  if (!gIsActive) {
    return;
  }

  gIsPaused = true;

  {
    StaticMutexAutoLock lock(sRegisteredThreadsMutex);

    for (size_t i = 0; i < sRegisteredThreads->size(); i++) {
      // Thread not being profiled, skip it.
      ThreadInfo* info = sRegisteredThreads->at(i);
      if (!info->hasProfile() || info->IsPendingDelete()) {
        continue;
      }

      // Thread not profiling the context that's going away, skip it.
      if (info->Stack()->mContext != mContext) {
        continue;
      }

      MutexAutoLock lock(info->GetMutex());
      info->FlushSamplesAndMarkers();
    }
  }

  gIsPaused = false;
}

// END SaveProfileTask et al
////////////////////////////////////////////////////////////////////////

static bool
ThreadSelected(const char* aThreadName)
{
  StaticMutexAutoLock lock(gThreadNameFiltersMutex);

  if (gThreadNameFilters.empty()) {
    return true;
  }

  std::string name = aThreadName;
  std::transform(name.begin(), name.end(), name.begin(), ::tolower);

  for (uint32_t i = 0; i < gThreadNameFilters.length(); ++i) {
    std::string filter = gThreadNameFilters[i];
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

    // Crude, non UTF-8 compatible, case insensitive substring search
    if (name.find(filter) != std::string::npos) {
      return true;
    }
  }

  return false;
}

void
Sampler::RegisterThread(ThreadInfo* aInfo)
{
  if (!aInfo->IsMainThread() && !gProfileThreads) {
    return;
  }

  if (!ThreadSelected(aInfo->Name())) {
    return;
  }

  aInfo->SetProfile(gBuffer);
}

size_t
Sampler::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  {
    StaticMutexAutoLock lock(sRegisteredThreadsMutex);

    for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
      ThreadInfo* info = sRegisteredThreads->at(i);

      n += info->SizeOfIncludingThis(aMallocSizeOf);
    }
  }

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - memory pointed to by the elements within mBuffer
  // - sRegisteredThreads
  // - mThreadNameFilters
  // - mFeatures

  return n;
}
