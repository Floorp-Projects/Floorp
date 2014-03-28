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
#ifndef SPS_STANDALONE
#include "SaveProfileTask.h"
#include "nsThreadUtils.h"
#include "prenv.h"
#include "prtime.h"
#include "nsXULAppAPI.h"
#endif
#include "ProfileEntry.h"
#include "SyncProfile.h"
#include "platform.h"
#include "shared-libraries.h"
#include "mozilla/StackWalk.h"
#include "GeckoSampler.h"

// JSON
#include "ProfileJSONWriter.h"

#ifndef SPS_STANDALONE
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

#include "mozilla/ProfileGatherer.h"
#endif

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
  #include "AndroidBridge.h"
#endif

#ifndef SPS_STANDALONE
// JS
#include "jsfriendapi.h"
#include "js/ProfilingFrameIterator.h"
#endif

#if defined(MOZ_PROFILING) && (defined(XP_MACOSX) || defined(XP_WIN))
 #define USE_NS_STACKWALK
#endif

#if defined(XP_WIN)
typedef CONTEXT tickcontext_t;
#elif defined(LINUX)
#include <ucontext.h>
typedef ucontext_t tickcontext_t;
#endif

#if defined(LINUX) || defined(XP_MACOSX)
#include <sys/types.h>
pid_t gettid();
#endif

#if defined(__arm__) && defined(ANDROID)
 // Should also work on ARM Linux, but not tested there yet.
 #define USE_EHABI_STACKWALK
#endif
#ifdef USE_EHABI_STACKWALK
 #include "EHABIStackWalk.h"
#endif

#ifndef SPS_STANDALONE
#if defined(SPS_PLAT_amd64_linux) || defined(SPS_PLAT_x86_linux)
# define USE_LUL_STACKWALK
# include "lul/LulMain.h"
# include "lul/platform-linux-lul.h"
#endif
#endif

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

#ifdef MOZ_VALGRIND
# include <valgrind/memcheck.h>
#else
# define VALGRIND_MAKE_MEM_DEFINED(_addr,_len)   ((void)0)
#endif


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

static bool
hasFeature(const char** aFeatures, uint32_t aFeatureCount, const char* aFeature) {
  for(size_t i = 0; i < aFeatureCount; i++) {
    if (strcmp(aFeatures[i], aFeature) == 0)
      return true;
  }
  return false;
}

GeckoSampler::GeckoSampler(double aInterval, int aEntrySize,
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

  bool ignore;
  sStartTime = mozilla::TimeStamp::ProcessCreation(ignore);

  {
    ::MutexAutoLock lock(*sRegisteredThreadsMutex);

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

GeckoSampler::~GeckoSampler()
{
  if (IsActive())
    Stop();

  SetActiveSampler(nullptr);

  // Destroy ThreadProfile for all threads
  {
    ::MutexAutoLock lock(*sRegisteredThreadsMutex);

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

void GeckoSampler::HandleSaveRequest()
{
  if (!mSaveRequested)
    return;
  mSaveRequested = false;

#ifndef SPS_STANDALONE
  // TODO: Use use the ipc/chromium Tasks here to support processes
  // without XPCOM.
  nsCOMPtr<nsIRunnable> runnable = new SaveProfileTask();
  NS_DispatchToMainThread(runnable);
#endif
}

void GeckoSampler::DeleteExpiredMarkers()
{
  mBuffer->deleteExpiredStoredMarkers();
}

void GeckoSampler::StreamTaskTracer(SpliceableJSONWriter& aWriter)
{
#ifdef MOZ_TASK_TRACER
  aWriter.StartArrayProperty("data");
    nsAutoPtr<nsTArray<nsCString>> data(mozilla::tasktracer::GetLoggedData(sStartTime));
    for (uint32_t i = 0; i < data->Length(); ++i) {
      aWriter.StringElement((data->ElementAt(i)).get());
    }
  aWriter.EndArray();

  aWriter.StartArrayProperty("threads");
    ::MutexAutoLock lock(*sRegisteredThreadsMutex);
    for (size_t i = 0; i < sRegisteredThreads->size(); i++) {
      // Thread meta data
      ThreadInfo* info = sRegisteredThreads->at(i);
      aWriter.StartObjectElement();
        if (XRE_GetProcessType() == GeckoProcessType_Plugin) {
          // TODO Add the proper plugin name
          aWriter.StringProperty("name", "Plugin");
        } else {
          aWriter.StringProperty("name", info->Name());
        }
        aWriter.IntProperty("tid", static_cast<int>(info->ThreadId()));
      aWriter.EndObject();
    }
  aWriter.EndArray();

  aWriter.DoubleProperty("start", static_cast<double>(mozilla::tasktracer::GetStartTime()));
#endif
}


void GeckoSampler::StreamMetaJSCustomObject(SpliceableJSONWriter& aWriter)
{
  aWriter.IntProperty("version", 3);
  aWriter.DoubleProperty("interval", interval());
  aWriter.IntProperty("stackwalk", mUseStackWalk);

#ifndef SPS_STANDALONE
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
#endif
}

void GeckoSampler::ToStreamAsJSON(std::ostream& stream, double aSinceTime)
{
  SpliceableJSONWriter b(mozilla::MakeUnique<OStreamJSONWriteFunc>(stream));
  StreamJSON(b, aSinceTime);
}

#ifndef SPS_STANDALONE
JSObject* GeckoSampler::ToJSObject(JSContext *aCx, double aSinceTime)
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
#endif

UniquePtr<char[]> GeckoSampler::ToJSON(double aSinceTime)
{
  SpliceableChunkedJSONWriter b;
  StreamJSON(b, aSinceTime);
  return b.WriteFunc()->CopyData();
}

void GeckoSampler::ToJSObjectAsync(double aSinceTime,
                                  mozilla::dom::Promise* aPromise)
{
  if (NS_WARN_IF(mGatherer)) {
    return;
  }

  mGatherer = new mozilla::ProfileGatherer(this, aSinceTime, aPromise);
  mGatherer->Start();
}

void GeckoSampler::ProfileGathered()
{
  mGatherer = nullptr;
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
        nsCString result;
        bool hasFrame = AndroidBridge::Bridge()->GetFrameNameJavaProfiling(0, sampleId, frameId, result);
        // when we run out of frames, we stop looping
        if (!hasFrame) {
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
            mozilla::widget::GeckoJavaSampler::GetSampleTimeJavaProfiling(0, sampleId);

          aWriter.StartObjectElement();
            aWriter.DoubleProperty("time", sampleTime);

            aWriter.StartArrayProperty("frames");
        }
        // add a frame to the sample
        aWriter.StartObjectElement();
          aWriter.StringProperty("location", result.BeginReading());
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

void GeckoSampler::StreamJSON(SpliceableJSONWriter& aWriter, double aSinceTime)
{
  aWriter.Start(SpliceableJSONWriter::SingleLineStyle);
  {
    // Put shared library info
    aWriter.StringProperty("libs", GetSharedLibraryInfoStringInternal().c_str());

    // Put meta data
    aWriter.StartObjectProperty("meta");
      StreamMetaJSCustomObject(aWriter);
    aWriter.EndObject();

    // Data of TaskTracer doesn't belong in the circular buffer.
    if (TaskTracer()) {
      aWriter.StartObjectProperty("tasktracer");
      StreamTaskTracer(aWriter);
    }

    // Lists the samples for each ThreadProfile
    aWriter.StartArrayProperty("threads");
    {
      SetPaused(true);

      {
        ::MutexAutoLock lock(*sRegisteredThreadsMutex);

        for (size_t i = 0; i < sRegisteredThreads->size(); i++) {
          // Thread not being profiled, skip it
          if (!sRegisteredThreads->at(i)->Profile())
            continue;

          // Note that we intentionally include ThreadProfile which
          // have been marked for pending delete.

          ::MutexAutoLock lock(sRegisteredThreads->at(i)->Profile()->GetMutex());

          sRegisteredThreads->at(i)->Profile()->StreamJSON(aWriter, aSinceTime);
        }
      }

#ifndef SPS_STANDALONE
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
      if (ProfileJava()) {
        mozilla::widget::GeckoJavaSampler::PauseJavaProfiling();

        aWriter.Start();
        {
          BuildJavaThreadJSObject(aWriter);
        }
        aWriter.End();

        mozilla::widget::GeckoJavaSampler::UnpauseJavaProfiling();
      }
  #endif
#endif

      SetPaused(false);
    }
    aWriter.EndArray();
  }
  aWriter.End();
}

void GeckoSampler::FlushOnJSShutdown(JSRuntime* aRuntime)
{
#ifndef SPS_STANDALONE
  SetPaused(true);

  {
    ::MutexAutoLock lock(*sRegisteredThreadsMutex);

    for (size_t i = 0; i < sRegisteredThreads->size(); i++) {
      // Thread not being profiled, skip it.
      if (!sRegisteredThreads->at(i)->Profile() ||
          sRegisteredThreads->at(i)->IsPendingDelete()) {
        continue;
      }

      // Thread not profiling the runtime that's going away, skip it.
      if (sRegisteredThreads->at(i)->Profile()->GetPseudoStack()->mRuntime != aRuntime) {
        continue;
      }

      ::MutexAutoLock lock(sRegisteredThreads->at(i)->Profile()->GetMutex());
      sRegisteredThreads->at(i)->Profile()->FlushSamplesAndMarkers();
    }
  }

  SetPaused(false);
#endif
}

void PseudoStack::flushSamplerOnJSShutdown()
{
#ifndef SPS_STANDALONE
  MOZ_ASSERT(mRuntime);
  GeckoSampler* t = tlsTicker.get();
  if (t) {
    t->FlushOnJSShutdown(mRuntime);
  }
#endif
}

// END SaveProfileTask et al
////////////////////////////////////////////////////////////////////////

static
void addDynamicTag(ThreadProfile &aProfile, char aTagName, const char *aStr)
{
  aProfile.addTag(ProfileEntry(aTagName, ""));
  // Add one to store the null termination
  size_t strLen = strlen(aStr) + 1;
  for (size_t j = 0; j < strLen;) {
    // Store as many characters in the void* as the platform allows
    char text[sizeof(void*)];
    size_t len = sizeof(void*)/sizeof(char);
    if (j+len >= strLen) {
      len = strLen - j;
    }
    memcpy(text, &aStr[j], len);
    j += sizeof(void*)/sizeof(char);
    // Cast to *((void**) to pass the text data to a void*
    aProfile.addTag(ProfileEntry('d', *((void**)(&text[0]))));
  }
}

static
void addPseudoEntry(volatile StackEntry &entry, ThreadProfile &aProfile,
                    PseudoStack *stack, void *lastpc)
{
  // Pseudo-frames with the BEGIN_PSEUDO_JS flag are just annotations
  // and should not be recorded in the profile.
  if (entry.hasFlag(StackEntry::BEGIN_PSEUDO_JS))
    return;

  int lineno = -1;

  // First entry has tagName 's' (start)
  // Check for magic pointer bit 1 to indicate copy
  const char* sampleLabel = entry.label();
  if (entry.isCopyLabel()) {
    // Store the string using 1 or more 'd' (dynamic) tags
    // that will happen to the preceding tag

    addDynamicTag(aProfile, 'c', sampleLabel);
#ifndef SPS_STANDALONE
    if (entry.isJs()) {
      if (!entry.pc()) {
        // The JIT only allows the top-most entry to have a nullptr pc
        MOZ_ASSERT(&entry == &stack->mStack[stack->stackSize() - 1]);
        // If stack-walking was disabled, then that's just unfortunate
        if (lastpc) {
          jsbytecode *jspc = js::ProfilingGetPC(stack->mRuntime, entry.script(),
                                                lastpc);
          if (jspc) {
            lineno = JS_PCToLineNumber(entry.script(), jspc);
          }
        }
      } else {
        lineno = JS_PCToLineNumber(entry.script(), entry.pc());
      }
    } else {
      lineno = entry.line();
    }
#endif
  } else {
    aProfile.addTag(ProfileEntry('c', sampleLabel));

    // XXX: Bug 1010578. Don't assume a CPP entry and try to get the
    // line for js entries as well.
    if (entry.isCpp()) {
      lineno = entry.line();
    }
  }

  if (lineno != -1) {
    aProfile.addTag(ProfileEntry('n', lineno));
  }

  uint32_t category = entry.category();
  MOZ_ASSERT(!(category & StackEntry::IS_CPP_ENTRY));
  MOZ_ASSERT(!(category & StackEntry::FRAME_LABEL_COPY));

  if (category) {
    aProfile.addTag(ProfileEntry('y', (int)category));
  }
}

struct NativeStack
{
  void** pc_array;
  void** sp_array;
  size_t size;
  size_t count;
};

mozilla::Atomic<bool> WALKING_JS_STACK(false);

struct AutoWalkJSStack {
  bool walkAllowed;

  AutoWalkJSStack() : walkAllowed(false) {
    walkAllowed = WALKING_JS_STACK.compareExchange(false, true);
  }

  ~AutoWalkJSStack() {
    if (walkAllowed)
        WALKING_JS_STACK = false;
  }
};

static
void mergeStacksIntoProfile(ThreadProfile& aProfile, TickSample* aSample, NativeStack& aNativeStack)
{
  PseudoStack* pseudoStack = aProfile.GetPseudoStack();
  volatile StackEntry *pseudoFrames = pseudoStack->mStack;
  uint32_t pseudoCount = pseudoStack->stackSize();

  // Make a copy of the JS stack into a JSFrame array. This is necessary since,
  // like the native stack, the JS stack is iterated youngest-to-oldest and we
  // need to iterate oldest-to-youngest when adding entries to aProfile.

  // Synchronous sampling reports an invalid buffer generation to
  // ProfilingFrameIterator to avoid incorrectly resetting the generation of
  // sampled JIT entries inside the JS engine. See note below concerning 'J'
  // entries.
  uint32_t startBufferGen;
  if (aSample->isSamplingCurrentThread) {
    startBufferGen = UINT32_MAX;
  } else {
    startBufferGen = aProfile.bufferGeneration();
  }
  uint32_t jsCount = 0;
#ifndef SPS_STANDALONE
  JS::ProfilingFrameIterator::Frame jsFrames[1000];
  // Only walk jit stack if profiling frame iterator is turned on.
  if (pseudoStack->mRuntime && JS::IsProfilingEnabledForRuntime(pseudoStack->mRuntime)) {
    AutoWalkJSStack autoWalkJSStack;
    const uint32_t maxFrames = mozilla::ArrayLength(jsFrames);

    if (aSample && autoWalkJSStack.walkAllowed) {
      JS::ProfilingFrameIterator::RegisterState registerState;
      registerState.pc = aSample->pc;
      registerState.sp = aSample->sp;
#ifdef ENABLE_ARM_LR_SAVING
      registerState.lr = aSample->lr;
#endif

      JS::ProfilingFrameIterator jsIter(pseudoStack->mRuntime,
                                        registerState,
                                        startBufferGen);
      for (; jsCount < maxFrames && !jsIter.done(); ++jsIter) {
        // See note below regarding 'J' entries.
        if (aSample->isSamplingCurrentThread || jsIter.isAsmJS()) {
          uint32_t extracted = jsIter.extractStack(jsFrames, jsCount, maxFrames);
          jsCount += extracted;
          if (jsCount == maxFrames)
            break;
        } else {
          mozilla::Maybe<JS::ProfilingFrameIterator::Frame> frame =
            jsIter.getPhysicalFrameWithoutLabel();
          if (frame.isSome())
            jsFrames[jsCount++] = frame.value();
        }
      }
    }
  }
#endif

  // Start the sample with a root entry.
  aProfile.addTag(ProfileEntry('s', "(root)"));

  // While the pseudo-stack array is ordered oldest-to-youngest, the JS and
  // native arrays are ordered youngest-to-oldest. We must add frames to
  // aProfile oldest-to-youngest. Thus, iterate over the pseudo-stack forwards
  // and JS and native arrays backwards. Note: this means the terminating
  // condition jsIndex and nativeIndex is being < 0.
  uint32_t pseudoIndex = 0;
  int32_t jsIndex = jsCount - 1;
  int32_t nativeIndex = aNativeStack.count - 1;

  uint8_t *lastPseudoCppStackAddr = nullptr;

  // Iterate as long as there is at least one frame remaining.
  while (pseudoIndex != pseudoCount || jsIndex >= 0 || nativeIndex >= 0) {
    // There are 1 to 3 frames available. Find and add the oldest.

    uint8_t *pseudoStackAddr = nullptr;
    uint8_t *jsStackAddr = nullptr;
    uint8_t *nativeStackAddr = nullptr;

    if (pseudoIndex != pseudoCount) {
      volatile StackEntry &pseudoFrame = pseudoFrames[pseudoIndex];

      if (pseudoFrame.isCpp())
        lastPseudoCppStackAddr = (uint8_t *) pseudoFrame.stackAddress();

#ifndef SPS_STANDALONE
      // Skip any pseudo-stack JS frames which are marked isOSR
      // Pseudostack frames are marked isOSR when the JS interpreter
      // enters a jit frame on a loop edge (via on-stack-replacement,
      // or OSR).  To avoid both the pseudoframe and jit frame being
      // recorded (and showing up twice), the interpreter marks the
      // interpreter pseudostack entry with the OSR flag to ensure that
      // it doesn't get counted.
      if (pseudoFrame.isJs() && pseudoFrame.isOSR()) {
          pseudoIndex++;
          continue;
      }
#endif

      MOZ_ASSERT(lastPseudoCppStackAddr);
      pseudoStackAddr = lastPseudoCppStackAddr;
    }

#ifndef SPS_STANDALONE
    if (jsIndex >= 0)
      jsStackAddr = (uint8_t *) jsFrames[jsIndex].stackAddress;
#endif

    if (nativeIndex >= 0)
      nativeStackAddr = (uint8_t *) aNativeStack.sp_array[nativeIndex];

    // If there's a native stack entry which has the same SP as a
    // pseudo stack entry, pretend we didn't see the native stack
    // entry.  Ditto for a native stack entry which has the same SP as
    // a JS stack entry.  In effect this means pseudo or JS entries
    // trump conflicting native entries.
    if (nativeStackAddr && (pseudoStackAddr == nativeStackAddr || jsStackAddr == nativeStackAddr)) {
      nativeStackAddr = nullptr;
      nativeIndex--;
      MOZ_ASSERT(pseudoStackAddr || jsStackAddr);
    }

    // Sanity checks.
    MOZ_ASSERT_IF(pseudoStackAddr, pseudoStackAddr != jsStackAddr &&
                                   pseudoStackAddr != nativeStackAddr);
    MOZ_ASSERT_IF(jsStackAddr, jsStackAddr != pseudoStackAddr &&
                               jsStackAddr != nativeStackAddr);
    MOZ_ASSERT_IF(nativeStackAddr, nativeStackAddr != pseudoStackAddr &&
                                   nativeStackAddr != jsStackAddr);

    // Check to see if pseudoStack frame is top-most.
    if (pseudoStackAddr > jsStackAddr && pseudoStackAddr > nativeStackAddr) {
      MOZ_ASSERT(pseudoIndex < pseudoCount);
      volatile StackEntry &pseudoFrame = pseudoFrames[pseudoIndex];
      addPseudoEntry(pseudoFrame, aProfile, pseudoStack, nullptr);
      pseudoIndex++;
      continue;
    }

#ifndef SPS_STANDALONE
    // Check to see if JS jit stack frame is top-most
    if (jsStackAddr > nativeStackAddr) {
      MOZ_ASSERT(jsIndex >= 0);
      const JS::ProfilingFrameIterator::Frame& jsFrame = jsFrames[jsIndex];

      // Stringifying non-asm.js JIT frames is delayed until streaming
      // time. To re-lookup the entry in the JitcodeGlobalTable, we need to
      // store the JIT code address ('J') in the circular buffer.
      //
      // Note that we cannot do this when we are sychronously sampling the
      // current thread; that is, when called from profiler_get_backtrace. The
      // captured backtrace is usually externally stored for an indeterminate
      // amount of time, such as in nsRefreshDriver. Problematically, the
      // stored backtrace may be alive across a GC during which the profiler
      // itself is disabled. In that case, the JS engine is free to discard
      // its JIT code. This means that if we inserted such 'J' entries into
      // the buffer, nsRefreshDriver would now be holding on to a backtrace
      // with stale JIT code return addresses.
      if (aSample->isSamplingCurrentThread ||
          jsFrame.kind == JS::ProfilingFrameIterator::Frame_AsmJS) {
        addDynamicTag(aProfile, 'c', jsFrame.label);
      } else {
        MOZ_ASSERT(jsFrame.kind == JS::ProfilingFrameIterator::Frame_Ion ||
                   jsFrame.kind == JS::ProfilingFrameIterator::Frame_Baseline);
        aProfile.addTag(ProfileEntry('J', jsFrames[jsIndex].returnAddress));
      }

      jsIndex--;
      continue;
    }
#endif

    // If we reach here, there must be a native stack entry and it must be the
    // greatest entry.
    if (nativeStackAddr) {
      MOZ_ASSERT(nativeIndex >= 0);
      aProfile
        .addTag(ProfileEntry('l', (void*)aNativeStack.pc_array[nativeIndex]));
    }
    if (nativeIndex >= 0) {
      nativeIndex--;
    }
  }

#ifndef SPS_STANDALONE
  // Update the JS runtime with the current profile sample buffer generation.
  //
  // Do not do this for synchronous sampling, which create their own
  // ProfileBuffers.
  if (!aSample->isSamplingCurrentThread && pseudoStack->mRuntime) {
    MOZ_ASSERT(aProfile.bufferGeneration() >= startBufferGen);
    uint32_t lapCount = aProfile.bufferGeneration() - startBufferGen;
    JS::UpdateJSRuntimeProfilerSampleBufferGen(pseudoStack->mRuntime,
                                               aProfile.bufferGeneration(),
                                               lapCount);
  }
#endif
}

#ifdef USE_NS_STACKWALK
static
void StackWalkCallback(uint32_t aFrameNumber, void* aPC, void* aSP,
                       void* aClosure)
{
  NativeStack* nativeStack = static_cast<NativeStack*>(aClosure);
  MOZ_ASSERT(nativeStack->count < nativeStack->size);
  nativeStack->sp_array[nativeStack->count] = aSP;
  nativeStack->pc_array[nativeStack->count] = aPC;
  nativeStack->count++;
}

void GeckoSampler::doNativeBacktrace(ThreadProfile &aProfile, TickSample* aSample)
{
  void* pc_array[1000];
  void* sp_array[1000];
  NativeStack nativeStack = {
    pc_array,
    sp_array,
    mozilla::ArrayLength(pc_array),
    0
  };

  // Start with the current function. We use 0 as the frame number here because
  // the FramePointerStackWalk() and MozStackWalk() calls below will use 1..N.
  // This is a bit weird but it doesn't matter because StackWalkCallback()
  // doesn't use the frame number argument.
  StackWalkCallback(/* frameNumber */ 0, aSample->pc, aSample->sp, &nativeStack);

  uint32_t maxFrames = uint32_t(nativeStack.size - nativeStack.count);
#if defined(XP_MACOSX) || defined(XP_WIN)
  void *stackEnd = aSample->threadProfile->GetStackTop();
  bool rv = true;
  if (aSample->fp >= aSample->sp && aSample->fp <= stackEnd)
    rv = FramePointerStackWalk(StackWalkCallback, /* skipFrames */ 0,
                               maxFrames, &nativeStack,
                               reinterpret_cast<void**>(aSample->fp), stackEnd);
#else
  void *platformData = nullptr;

  uintptr_t thread = GetThreadHandle(aSample->threadProfile->GetPlatformData());
  MOZ_ASSERT(thread);
  bool rv = MozStackWalk(StackWalkCallback, /* skipFrames */ 0, maxFrames,
                             &nativeStack, thread, platformData);
#endif
  if (rv)
    mergeStacksIntoProfile(aProfile, aSample, nativeStack);
}
#endif


#ifdef USE_EHABI_STACKWALK
void GeckoSampler::doNativeBacktrace(ThreadProfile &aProfile, TickSample* aSample)
{
  void *pc_array[1000];
  void *sp_array[1000];
  NativeStack nativeStack = {
    pc_array,
    sp_array,
    mozilla::ArrayLength(pc_array),
    0
  };

  const mcontext_t *mcontext = &reinterpret_cast<ucontext_t *>(aSample->context)->uc_mcontext;
  mcontext_t savedContext;
  PseudoStack *pseudoStack = aProfile.GetPseudoStack();

  nativeStack.count = 0;
  // The pseudostack contains an "EnterJIT" frame whenever we enter
  // JIT code with profiling enabled; the stack pointer value points
  // the saved registers.  We use this to unwind resume unwinding
  // after encounting JIT code.
  for (uint32_t i = pseudoStack->stackSize(); i > 0; --i) {
    // The pseudostack grows towards higher indices, so we iterate
    // backwards (from callee to caller).
    volatile StackEntry &entry = pseudoStack->mStack[i - 1];
    if (!entry.isJs() && strcmp(entry.label(), "EnterJIT") == 0) {
      // Found JIT entry frame.  Unwind up to that point (i.e., force
      // the stack walk to stop before the block of saved registers;
      // note that it yields nondecreasing stack pointers), then restore
      // the saved state.
      uint32_t *vSP = reinterpret_cast<uint32_t*>(entry.stackAddress());

      nativeStack.count += EHABIStackWalk(*mcontext,
                                          /* stackBase = */ vSP,
                                          sp_array + nativeStack.count,
                                          pc_array + nativeStack.count,
                                          nativeStack.size - nativeStack.count);

      memset(&savedContext, 0, sizeof(savedContext));
      // See also: struct EnterJITStack in js/src/jit/arm/Trampoline-arm.cpp
      savedContext.arm_r4 = *vSP++;
      savedContext.arm_r5 = *vSP++;
      savedContext.arm_r6 = *vSP++;
      savedContext.arm_r7 = *vSP++;
      savedContext.arm_r8 = *vSP++;
      savedContext.arm_r9 = *vSP++;
      savedContext.arm_r10 = *vSP++;
      savedContext.arm_fp = *vSP++;
      savedContext.arm_lr = *vSP++;
      savedContext.arm_sp = reinterpret_cast<uint32_t>(vSP);
      savedContext.arm_pc = savedContext.arm_lr;
      mcontext = &savedContext;
    }
  }

  // Now unwind whatever's left (starting from either the last EnterJIT
  // frame or, if no EnterJIT was found, the original registers).
  nativeStack.count += EHABIStackWalk(*mcontext,
                                      aProfile.GetStackTop(),
                                      sp_array + nativeStack.count,
                                      pc_array + nativeStack.count,
                                      nativeStack.size - nativeStack.count);

  mergeStacksIntoProfile(aProfile, aSample, nativeStack);
}
#endif


#ifdef USE_LUL_STACKWALK
void GeckoSampler::doNativeBacktrace(ThreadProfile &aProfile, TickSample* aSample)
{
  const mcontext_t* mc
    = &reinterpret_cast<ucontext_t *>(aSample->context)->uc_mcontext;

  lul::UnwindRegs startRegs;
  memset(&startRegs, 0, sizeof(startRegs));

# if defined(SPS_PLAT_amd64_linux)
  startRegs.xip = lul::TaggedUWord(mc->gregs[REG_RIP]);
  startRegs.xsp = lul::TaggedUWord(mc->gregs[REG_RSP]);
  startRegs.xbp = lul::TaggedUWord(mc->gregs[REG_RBP]);
# elif defined(SPS_PLAT_arm_android)
  startRegs.r15 = lul::TaggedUWord(mc->arm_pc);
  startRegs.r14 = lul::TaggedUWord(mc->arm_lr);
  startRegs.r13 = lul::TaggedUWord(mc->arm_sp);
  startRegs.r12 = lul::TaggedUWord(mc->arm_ip);
  startRegs.r11 = lul::TaggedUWord(mc->arm_fp);
  startRegs.r7  = lul::TaggedUWord(mc->arm_r7);
# elif defined(SPS_PLAT_x86_linux) || defined(SPS_PLAT_x86_android)
  startRegs.xip = lul::TaggedUWord(mc->gregs[REG_EIP]);
  startRegs.xsp = lul::TaggedUWord(mc->gregs[REG_ESP]);
  startRegs.xbp = lul::TaggedUWord(mc->gregs[REG_EBP]);
# else
#   error "Unknown plat"
# endif

  /* Copy up to N_STACK_BYTES from rsp-REDZONE upwards, but not
     going past the stack's registered top point.  Do some basic
     sanity checks too.  This assumes that the TaggedUWord holding
     the stack pointer value is valid, but it should be, since it
     was constructed that way in the code just above. */

  lul::StackImage stackImg;

  {
#   if defined(SPS_PLAT_amd64_linux)
    uintptr_t rEDZONE_SIZE = 128;
    uintptr_t start = startRegs.xsp.Value() - rEDZONE_SIZE;
#   elif defined(SPS_PLAT_arm_android)
    uintptr_t rEDZONE_SIZE = 0;
    uintptr_t start = startRegs.r13.Value() - rEDZONE_SIZE;
#   elif defined(SPS_PLAT_x86_linux) || defined(SPS_PLAT_x86_android)
    uintptr_t rEDZONE_SIZE = 0;
    uintptr_t start = startRegs.xsp.Value() - rEDZONE_SIZE;
#   else
#     error "Unknown plat"
#   endif
    uintptr_t end   = reinterpret_cast<uintptr_t>(aProfile.GetStackTop());
    uintptr_t ws    = sizeof(void*);
    start &= ~(ws-1);
    end   &= ~(ws-1);
    uintptr_t nToCopy = 0;
    if (start < end) {
      nToCopy = end - start;
      if (nToCopy > lul::N_STACK_BYTES)
        nToCopy = lul::N_STACK_BYTES;
    }
    MOZ_ASSERT(nToCopy <= lul::N_STACK_BYTES);
    stackImg.mLen       = nToCopy;
    stackImg.mStartAvma = start;
    if (nToCopy > 0) {
      memcpy(&stackImg.mContents[0], (void*)start, nToCopy);
      (void)VALGRIND_MAKE_MEM_DEFINED(&stackImg.mContents[0], nToCopy);
    }
  }

  // The maximum number of frames that LUL will produce.  Setting it
  // too high gives a risk of it wasting a lot of time looping on
  // corrupted stacks.
  const int MAX_NATIVE_FRAMES = 256;

  size_t scannedFramesAllowed = 0;

  uintptr_t framePCs[MAX_NATIVE_FRAMES];
  uintptr_t frameSPs[MAX_NATIVE_FRAMES];
  size_t framesAvail = mozilla::ArrayLength(framePCs);
  size_t framesUsed  = 0;
  size_t scannedFramesAcquired = 0;
  sLUL->Unwind( &framePCs[0], &frameSPs[0],
                &framesUsed, &scannedFramesAcquired,
                framesAvail, scannedFramesAllowed,
                &startRegs, &stackImg );

  NativeStack nativeStack = {
    reinterpret_cast<void**>(framePCs),
    reinterpret_cast<void**>(frameSPs),
    mozilla::ArrayLength(framePCs),
    0
  };

  nativeStack.count = framesUsed;

  mergeStacksIntoProfile(aProfile, aSample, nativeStack);

  // Update stats in the LUL stats object.  Unfortunately this requires
  // three global memory operations.
  sLUL->mStats.mContext += 1;
  sLUL->mStats.mCFI     += framesUsed - 1 - scannedFramesAcquired;
  sLUL->mStats.mScanned += scannedFramesAcquired;
}
#endif


static
void doSampleStackTrace(ThreadProfile &aProfile, TickSample *aSample, bool aAddLeafAddresses)
{
  NativeStack nativeStack = { nullptr, nullptr, 0, 0 };
  mergeStacksIntoProfile(aProfile, aSample, nativeStack);

#ifdef ENABLE_SPS_LEAF_DATA
  if (aSample && aAddLeafAddresses) {
    aProfile.addTag(ProfileEntry('l', (void*)aSample->pc));
#ifdef ENABLE_ARM_LR_SAVING
    aProfile.addTag(ProfileEntry('L', (void*)aSample->lr));
#endif
  }
#endif
}

void GeckoSampler::Tick(TickSample* sample)
{
  // Don't allow for ticks to happen within other ticks.
  InplaceTick(sample);
}

void GeckoSampler::InplaceTick(TickSample* sample)
{
  ThreadProfile& currThreadProfile = *sample->threadProfile;

  currThreadProfile.addTag(ProfileEntry('T', currThreadProfile.ThreadId()));

  if (sample) {
    mozilla::TimeDuration delta = sample->timestamp - sStartTime;
    currThreadProfile.addTag(ProfileEntry('t', delta.ToMilliseconds()));
  }

  PseudoStack* stack = currThreadProfile.GetPseudoStack();

#if defined(USE_NS_STACKWALK) || defined(USE_EHABI_STACKWALK) || \
    defined(USE_LUL_STACKWALK)
  if (mUseStackWalk) {
    doNativeBacktrace(currThreadProfile, sample);
  } else {
    doSampleStackTrace(currThreadProfile, sample, mAddLeafAddresses);
  }
#else
  doSampleStackTrace(currThreadProfile, sample, mAddLeafAddresses);
#endif

  // Don't process the PeudoStack's markers if we're
  // synchronously sampling the current thread.
  if (!sample->isSamplingCurrentThread) {
    ProfilerMarkerLinkedList* pendingMarkersList = stack->getPendingMarkers();
    while (pendingMarkersList && pendingMarkersList->peek()) {
      ProfilerMarker* marker = pendingMarkersList->popHead();
      currThreadProfile.addStoredMarker(marker);
      currThreadProfile.addTag(ProfileEntry('m', marker));
    }
  }

#ifndef SPS_STANDALONE
  if (sample && currThreadProfile.GetThreadResponsiveness()->HasData()) {
    mozilla::TimeDuration delta = currThreadProfile.GetThreadResponsiveness()->GetUnresponsiveDuration(sample->timestamp);
    currThreadProfile.addTag(ProfileEntry('r', delta.ToMilliseconds()));
  }
#endif

  // rssMemory is equal to 0 when we are not recording.
  if (sample && sample->rssMemory != 0) {
    currThreadProfile.addTag(ProfileEntry('R', static_cast<double>(sample->rssMemory)));
  }

  // ussMemory is equal to 0 when we are not recording.
  if (sample && sample->ussMemory != 0) {
    currThreadProfile.addTag(ProfileEntry('U', static_cast<double>(sample->ussMemory)));
  }

#if defined(XP_WIN)
  if (mProfilePower) {
    mIntelPowerGadget->TakeSample();
    currThreadProfile.addTag(ProfileEntry('p', static_cast<double>(mIntelPowerGadget->GetTotalPackagePowerInWatts())));
  }
#endif

  if (sLastFrameNumber != sFrameNumber) {
    currThreadProfile.addTag(ProfileEntry('f', sFrameNumber));
    sLastFrameNumber = sFrameNumber;
  }
}

namespace {

SyncProfile* NewSyncProfile()
{
  PseudoStack* stack = tlsPseudoStack.get();
  if (!stack) {
    MOZ_ASSERT(stack);
    return nullptr;
  }
  Thread::tid_t tid = Thread::GetCurrentId();

  ThreadInfo* info = new ThreadInfo("SyncProfile", tid, false, stack, nullptr);
  SyncProfile* profile = new SyncProfile(info, GET_BACKTRACE_DEFAULT_ENTRY);
  return profile;
}

} // namespace

SyncProfile* GeckoSampler::GetBacktrace()
{
  SyncProfile* profile = NewSyncProfile();

  TickSample sample;
  sample.threadProfile = profile;

#if defined(HAVE_NATIVE_UNWIND)
#if defined(XP_WIN) || defined(LINUX)
  tickcontext_t context;
  sample.PopulateContext(&context);
#elif defined(XP_MACOSX)
  sample.PopulateContext(nullptr);
#endif
#endif

  sample.isSamplingCurrentThread = true;
  sample.timestamp = mozilla::TimeStamp::Now();

  profile->BeginUnwind();
  Tick(&sample);
  profile->EndUnwind();

  return profile;
}

void
GeckoSampler::GetBufferInfo(uint32_t *aCurrentPosition, uint32_t *aTotalSize, uint32_t *aGeneration)
{
  *aCurrentPosition = mBuffer->mWritePos;
  *aTotalSize = mBuffer->mEntrySize;
  *aGeneration = mBuffer->mGeneration;
}
