/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include "sps_sampler.h"
#include "platform.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"
#include "prenv.h"
#include "shared-libraries.h"
#include "mozilla/StackWalk.h"
#include "JSObjectBuilder.h"

// Meta
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsIHttpProtocolHandler.h"
#include "nsServiceManagerUtils.h"
#include "nsIXULRuntime.h"
#include "nsIXULAppInfo.h"

// we eventually want to make this runtime switchable
#if defined(MOZ_PROFILING) && (defined(XP_UNIX) && !defined(XP_MACOSX))
 #ifndef ANDROID
  #define USE_BACKTRACE
 #endif
#endif
#ifdef USE_BACKTRACE
 #include <execinfo.h>
#endif

#if defined(MOZ_PROFILING) && (defined(XP_MACOSX) || defined(XP_WIN))
 #define USE_NS_STACKWALK
#endif
#ifdef USE_NS_STACKWALK
 #include "nsStackWalk.h"
#endif

#if defined(MOZ_PROFILING) && defined(ANDROID)
 #define USE_LIBUNWIND
 #include <libunwind.h>
 #include "android-signal-defs.h"
#endif

using std::string;
using namespace mozilla;

#ifdef XP_WIN
 #include <windows.h>
 #define getpid GetCurrentProcessId
#else
 #include <unistd.h>
#endif

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

#if _MSC_VER
 #define snprintf _snprintf
#endif

static const int DYNAMIC_MAX_STRING = 512;

mozilla::ThreadLocal<ProfileStack *> tlsStack;
mozilla::ThreadLocal<TableTicker *> tlsTicker;
// We need to track whether we've been initialized otherwise
// we end up using tlsStack without initializing it.
// Because tlsStack is totally opaque to us we can't reuse
// it as the flag itself.
bool stack_key_initialized;

TimeStamp sLastTracerEvent;

class ThreadProfile;

class ProfileEntry
{
public:
  ProfileEntry()
    : mTagData(NULL)
    , mTagName(0)
  { }

  // aTagData must not need release (i.e. be a string from the text segment)
  ProfileEntry(char aTagName, const char *aTagData)
    : mTagData(aTagData)
    , mTagName(aTagName)
  { }

  ProfileEntry(char aTagName, void *aTagPtr)
    : mTagPtr(aTagPtr)
    , mTagName(aTagName)
  { }

  ProfileEntry(char aTagName, double aTagFloat)
    : mTagFloat(aTagFloat)
    , mTagName(aTagName)
  { }

  ProfileEntry(char aTagName, uintptr_t aTagOffset)
    : mTagOffset(aTagOffset)
    , mTagName(aTagName)
  { }

  ProfileEntry(char aTagName, Address aTagAddress)
    : mTagAddress(aTagAddress)
    , mTagName(aTagName)
  { }

  friend std::ostream& operator<<(std::ostream& stream, const ProfileEntry& entry);

private:
  friend class ThreadProfile;
  union {
    const char* mTagData;
    char mTagChars[sizeof(void*)];
    void* mTagPtr;
    double mTagFloat;
    Address mTagAddress;
    uintptr_t mTagOffset;
  };
  char mTagName;
};

#define PROFILE_MAX_ENTRY 100000
class ThreadProfile
{
public:
  ThreadProfile(int aEntrySize, ProfileStack *aStack)
    : mWritePos(0)
    , mLastFlushPos(0)
    , mReadPos(0)
    , mEntrySize(aEntrySize)
    , mStack(aStack)
  {
    mEntries = new ProfileEntry[mEntrySize];
  }

  ~ThreadProfile()
  {
    delete[] mEntries;
  }

  void addTag(ProfileEntry aTag)
  {
    // Called from signal, call only reentrant functions
    mEntries[mWritePos] = aTag;
    mWritePos = (mWritePos + 1) % mEntrySize;
    if (mWritePos == mReadPos) {
      // Keep one slot open
      mEntries[mReadPos] = ProfileEntry();
      mReadPos = (mReadPos + 1) % mEntrySize;
    }
    // we also need to move the flush pos to ensure we
    // do not pass it
    if (mWritePos == mLastFlushPos) {
      mLastFlushPos = (mLastFlushPos + 1) % mEntrySize;
    }
  }

  // flush the new entries
  void flush()
  {
    mLastFlushPos = mWritePos;
  }

  // discards all of the entries since the last flush()
  // NOTE: that if mWritePos happens to wrap around past
  // mLastFlushPos we actually only discard mWritePos - mLastFlushPos entries
  //
  // r = mReadPos
  // w = mWritePos
  // f = mLastFlushPos
  //
  //     r          f    w
  // |-----------------------------|
  // |   abcdefghijklmnopq         | -> 'abcdefghijklmnopq'
  // |-----------------------------|
  //
  //
  // mWritePos and mReadPos have passed mLastFlushPos
  //                      f
  //                    w r
  // |-----------------------------|
  // |ABCDEFGHIJKLMNOPQRSqrstuvwxyz|
  // |-----------------------------|
  //                       w
  //                       r
  // |-----------------------------|
  // |ABCDEFGHIJKLMNOPQRSqrstuvwxyz| -> ''
  // |-----------------------------|
  //
  //
  // mWritePos will end up the same as mReadPos
  //                r
  //              w f
  // |-----------------------------|
  // |ABCDEFGHIJKLMklmnopqrstuvwxyz|
  // |-----------------------------|
  //                r
  //                w
  // |-----------------------------|
  // |ABCDEFGHIJKLMklmnopqrstuvwxyz| -> ''
  // |-----------------------------|
  //
  //
  // mWritePos has moved past mReadPos
  //      w r       f
  // |-----------------------------|
  // |ABCDEFdefghijklmnopqrstuvwxyz|
  // |-----------------------------|
  //        r       w
  // |-----------------------------|
  // |ABCDEFdefghijklmnopqrstuvwxyz| -> 'defghijkl'
  // |-----------------------------|

  void erase()
  {
    mWritePos = mLastFlushPos;
  }

  char* processDynamicTag(int readPos, int* tagsConsumed, char* tagBuff)
  {
    int readAheadPos = (readPos + 1) % mEntrySize;
    int tagBuffPos = 0;

    // Read the string stored in mTagData until the null character is seen
    bool seenNullByte = false;
    while (readAheadPos != mLastFlushPos && !seenNullByte) {
      (*tagsConsumed)++;
      ProfileEntry readAheadEntry = mEntries[readAheadPos];
      for (size_t pos = 0; pos < sizeof(void*); pos++) {
        tagBuff[tagBuffPos] = readAheadEntry.mTagChars[pos];
        if (tagBuff[tagBuffPos] == '\0' || tagBuffPos == DYNAMIC_MAX_STRING-2) {
          seenNullByte = true;
          break;
        }
        tagBuffPos++;
      }
      if (!seenNullByte)
        readAheadPos = (readAheadPos + 1) % mEntrySize;
    }
    return tagBuff;
  }

  friend std::ostream& operator<<(std::ostream& stream, const ThreadProfile& profile);

  JSObject *ToJSObject(JSContext *aCx)
  {
    JSObjectBuilder b(aCx);

    JSObject *profile = b.CreateObject();
    JSObject *samples = b.CreateArray();
    b.DefineProperty(profile, "samples", samples);

    JSObject *sample = NULL;
    JSObject *frames = NULL;

    int readPos = mReadPos;
    while (readPos != mLastFlushPos) {
      // Number of tag consumed
      int incBy = 1;
      ProfileEntry entry = mEntries[readPos];

      // Read ahead to the next tag, if it's a 'd' tag process it now
      const char* tagStringData = entry.mTagData;
      int readAheadPos = (readPos + 1) % mEntrySize;
      char tagBuff[DYNAMIC_MAX_STRING];
      // Make sure the string is always null terminated if it fills up DYNAMIC_MAX_STRING-2
      tagBuff[DYNAMIC_MAX_STRING-1] = '\0';

      if (readAheadPos != mLastFlushPos && mEntries[readAheadPos].mTagName == 'd') {
        tagStringData = processDynamicTag(readPos, &incBy, tagBuff);
      }

      switch (entry.mTagName) {
        case 's':
          sample = b.CreateObject();
          b.DefineProperty(sample, "name", tagStringData);
          frames = b.CreateArray();
          b.DefineProperty(sample, "frames", frames);
          b.ArrayPush(samples, sample);
          break;
        case 'r':
          {
            if (sample) {
              b.DefineProperty(sample, "responsiveness", entry.mTagFloat);
            }
          }
          break;
        case 't':
          {
            if (sample) {
              b.DefineProperty(sample, "time", entry.mTagFloat);
            }
          }
          break;
        case 'c':
        case 'l':
          {
            if (sample) {
              JSObject *frame = b.CreateObject();
              if (entry.mTagName == 'l') {
                // Bug 753041
                // We need a double cast here to tell GCC that we don't want to sign
                // extend 32-bit addresses starting with 0xFXXXXXX.
                unsigned long long pc = (unsigned long long)(uintptr_t)entry.mTagPtr;
                snprintf(tagBuff, DYNAMIC_MAX_STRING, "%#llx", pc);
                b.DefineProperty(frame, "location", tagBuff);
              } else {
                b.DefineProperty(frame, "location", tagStringData);
              }
              b.ArrayPush(frames, frame);
            }
          }
      }
      readPos = (readPos + incBy) % mEntrySize;
    }

    return profile;
  }

  ProfileStack* GetStack()
  {
    return mStack;
  }
private:
  // Circular buffer 'Keep One Slot Open' implementation
  // for simplicity
  ProfileEntry *mEntries;
  int mWritePos; // points to the next entry we will write to
  int mLastFlushPos; // points to the next entry since the last flush()
  int mReadPos;  // points to the next entry we will read to
  int mEntrySize;
  ProfileStack *mStack;
};

class SaveProfileTask;

static bool
hasFeature(const char** aFeatures, uint32_t aFeatureCount, const char* aFeature) {
  for(size_t i = 0; i < aFeatureCount; i++) {
    if (strcmp(aFeatures[i], aFeature) == 0)
      return true;
  }
  return false;
}

class TableTicker: public Sampler {
 public:
  TableTicker(int aInterval, int aEntrySize, ProfileStack *aStack,
              const char** aFeatures, uint32_t aFeatureCount)
    : Sampler(aInterval, true)
    , mPrimaryThreadProfile(aEntrySize, aStack)
    , mStartTime(TimeStamp::Now())
    , mSaveRequested(false)
  {
    mUseStackWalk = hasFeature(aFeatures, aFeatureCount, "stackwalk");

    //XXX: It's probably worth splitting the jank profiler out from the regular profiler at some point
    mJankOnly = hasFeature(aFeatures, aFeatureCount, "jank");
    mProfileJS = hasFeature(aFeatures, aFeatureCount, "js");
    mPrimaryThreadProfile.addTag(ProfileEntry('m', "Start"));
  }

  ~TableTicker() { if (IsActive()) Stop(); }

  virtual void SampleStack(TickSample* sample) {}

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
    return &mPrimaryThreadProfile;
  }

  JSObject *ToJSObject(JSContext *aCx);
  JSObject *GetMetaJSObject(JSObjectBuilder& b);

  const bool ProfileJS() { return mProfileJS; }

private:
  // Not implemented on platforms which do not support backtracing
  void doBacktrace(ThreadProfile &aProfile, TickSample* aSample);

private:
  // This represent the application's main thread (SAMPLER_INIT)
  ThreadProfile mPrimaryThreadProfile;
  TimeStamp mStartTime;
  bool mSaveRequested;
  bool mUseStackWalk;
  bool mJankOnly;
  bool mProfileJS;
};

std::string GetSharedLibraryInfoString();

/**
 * This is an event used to save the profile on the main thread
 * to be sure that it is not being modified while saving.
 */
class SaveProfileTask : public nsRunnable {
public:
  SaveProfileTask() {}

  NS_IMETHOD Run() {
    TableTicker *t = tlsTicker.get();

    char buff[MAXPATHLEN];
#ifdef ANDROID
  #define FOLDER "/sdcard/"
#elif defined(XP_WIN)
  #define FOLDER "%TEMP%\\"
#else
  #define FOLDER "/tmp/"
#endif

    snprintf(buff, MAXPATHLEN, "%sprofile_%i_%i.txt", FOLDER, XRE_GetProcessType(), getpid());

#ifdef XP_WIN
    // Expand %TEMP% on Windows
    {
      char tmp[MAXPATHLEN];
      ExpandEnvironmentStringsA(buff, tmp, mozilla::ArrayLength(tmp));
      strcpy(buff, tmp);
    }
#endif

    // Pause the profiler during saving.
    // This will prevent us from recording sampling
    // regarding profile saving. This will also
    // prevent bugs caused by the circular buffer not
    // being thread safe. Bug 750989.
    std::ofstream stream;
    stream.open(buff);
    t->SetPaused(true);
    if (stream.is_open()) {
      stream << *(t->GetPrimaryThreadProfile());
      stream << "h-" << GetSharedLibraryInfoString() << std::endl;
      stream.close();
      LOG("Saved to " FOLDER "profile_TYPE_PID.txt");
    } else {
      LOG("Fail to open profile log file.");
    }
    t->SetPaused(false);

    return NS_OK;
  }
};

void TableTicker::HandleSaveRequest()
{
  if (!mSaveRequested)
    return;
  mSaveRequested = false;

  // TODO: Use use the ipc/chromium Tasks here to support processes
  // without XPCOM.
  nsCOMPtr<nsIRunnable> runnable = new SaveProfileTask();
  NS_DispatchToMainThread(runnable);
}

JSObject* TableTicker::GetMetaJSObject(JSObjectBuilder& b)
{
  JSObject *meta = b.CreateObject();

  b.DefineProperty(meta, "version", 2);
  b.DefineProperty(meta, "interval", interval());
  b.DefineProperty(meta, "stackwalk", mUseStackWalk);
  b.DefineProperty(meta, "jank", mJankOnly);
  b.DefineProperty(meta, "processType", XRE_GetProcessType());

  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler> http = do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &res);
  if (!NS_FAILED(res)) {
    nsCAutoString string;

    res = http->GetPlatform(string);
    if (!NS_FAILED(res))
      b.DefineProperty(meta, "platform", string.Data());

    res = http->GetOscpu(string);
    if (!NS_FAILED(res))
      b.DefineProperty(meta, "oscpu", string.Data());

    res = http->GetMisc(string);
    if (!NS_FAILED(res))
      b.DefineProperty(meta, "misc", string.Data());
  }

  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (runtime) {
    nsCAutoString string;

    res = runtime->GetXPCOMABI(string);
    if (!NS_FAILED(res))
      b.DefineProperty(meta, "abi", string.Data());

    res = runtime->GetWidgetToolkit(string);
    if (!NS_FAILED(res))
      b.DefineProperty(meta, "toolkit", string.Data());
  }

  nsCOMPtr<nsIXULAppInfo> appInfo = do_GetService("@mozilla.org/xre/app-info;1");
  if (appInfo) {
    nsCAutoString string;

    res = appInfo->GetName(string);
    if (!NS_FAILED(res))
      b.DefineProperty(meta, "product", string.Data());
  }

  return meta;
}

JSObject* TableTicker::ToJSObject(JSContext *aCx)
{
  JSObjectBuilder b(aCx);

  JSObject *profile = b.CreateObject();

  // Put meta data
  JSObject *meta = GetMetaJSObject(b);
  b.DefineProperty(profile, "meta", meta);

  // Lists the samples for each ThreadProfile
  JSObject *threads = b.CreateArray();
  b.DefineProperty(profile, "threads", threads);

  // For now we only have one thread
  SetPaused(true);
  JSObject* threadSamples = GetPrimaryThreadProfile()->ToJSObject(aCx);
  b.ArrayPush(threads, threadSamples);
  SetPaused(false);

  return profile;
}


#ifdef USE_BACKTRACE
void TableTicker::doBacktrace(ThreadProfile &aProfile, TickSample* aSample)
{
  void *array[100];
  int count = backtrace (array, 100);

  aProfile.addTag(ProfileEntry('s', "(root)"));

  for (int i = 0; i < count; i++) {
    if( (intptr_t)array[i] == -1 ) break;
    aProfile.addTag(ProfileEntry('l', (void*)array[i]));
  }
}
#endif


#ifdef USE_NS_STACKWALK
typedef struct {
  void** array;
  size_t size;
  size_t count;
} PCArray;

static
void StackWalkCallback(void* aPC, void* aClosure)
{
  PCArray* array = static_cast<PCArray*>(aClosure);
  if (array->count >= array->size) {
    // too many frames, ignore
    return;
  }
  array->array[array->count++] = aPC;
}

void TableTicker::doBacktrace(ThreadProfile &aProfile, TickSample* aSample)
{
#ifndef XP_MACOSX
  uintptr_t thread = GetThreadHandle(platform_data());
  MOZ_ASSERT(thread);
#endif
  void* pc_array[1000];
  PCArray array = {
    pc_array,
    mozilla::ArrayLength(pc_array),
    0
  };

  // Start with the current function.
  StackWalkCallback(aSample->pc, &array);

#ifdef XP_MACOSX
  pthread_t pt = GetProfiledThread(platform_data());
  void *stackEnd = reinterpret_cast<void*>(-1);
  if (pt)
    stackEnd = static_cast<char*>(pthread_get_stackaddr_np(pt));
  nsresult rv = FramePointerStackWalk(StackWalkCallback, 0, &array, reinterpret_cast<void**>(aSample->fp), stackEnd);
#else
  nsresult rv = NS_StackWalk(StackWalkCallback, 0, &array, thread);
#endif
  if (NS_SUCCEEDED(rv)) {
    aProfile.addTag(ProfileEntry('s', "(root)"));

    for (size_t i = array.count; i > 0; --i) {
      aProfile.addTag(ProfileEntry('l', (void*)array.array[i - 1]));
    }
  }
}
#endif

#if defined(USE_LIBUNWIND) && defined(ANDROID)
void TableTicker::doBacktrace(ThreadProfile &aProfile, TickSample* aSample)
{
  void* pc_array[1000];
  size_t count = 0;

  unw_cursor_t cursor; unw_context_t uc;
  unw_word_t ip;
  unw_getcontext(&uc);

  // Dirty hack: replace the registers with values from the signal handler
  // We do this in order to avoid the overhead of walking up to reach the
  // signal handler frame, and the possibility that libunwind fails to
  // handle it correctly.
  unw_tdep_context_t *unw_ctx = reinterpret_cast<unw_tdep_context_t*> (&uc);
  mcontext_t& mcontext = reinterpret_cast<ucontext_t*> (aSample->context)->uc_mcontext;
#define REPLACE_REG(num) unw_ctx->regs[num] = mcontext.gregs[R##num]
  REPLACE_REG(0);
  REPLACE_REG(1);
  REPLACE_REG(2);
  REPLACE_REG(3);
  REPLACE_REG(4);
  REPLACE_REG(5);
  REPLACE_REG(6);
  REPLACE_REG(7);
  REPLACE_REG(8);
  REPLACE_REG(9);
  REPLACE_REG(10);
  REPLACE_REG(11);
  REPLACE_REG(12);
  REPLACE_REG(13);
  REPLACE_REG(14);
  REPLACE_REG(15);
#undef REPLACE_REG
  unw_init_local(&cursor, &uc);
  while (count < ArrayLength(pc_array) &&
         unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    pc_array[count++] = reinterpret_cast<void*> (ip);
  }

  aProfile.addTag(ProfileEntry('s', "(root)"));
  for (size_t i = count; i > 0; --i) {
    aProfile.addTag(ProfileEntry('l', reinterpret_cast<void*>(pc_array[i - 1])));
  }
}
#endif

static
void doSampleStackTrace(ProfileStack *aStack, ThreadProfile &aProfile, TickSample *sample)
{
  // Sample
  // 's' tag denotes the start of a sample block
  // followed by 0 or more 'c' tags.
  aProfile.addTag(ProfileEntry('s', "(root)"));
  for (mozilla::sig_safe_t i = 0;
       i < aStack->mStackPointer && i < mozilla::ArrayLength(aStack->mStack);
       i++) {
    // First entry has tagName 's' (start)
    // Check for magic pointer bit 1 to indicate copy
    const char* sampleLabel = aStack->mStack[i].mLabel;
    if (aStack->mStack[i].isCopyLabel()) {
      // Store the string using 1 or more 'd' (dynamic) tags
      // that will happen to the preceding tag

      aProfile.addTag(ProfileEntry('c', ""));
      // Add one to store the null termination
      size_t strLen = strlen(sampleLabel) + 1;
      for (size_t j = 0; j < strLen;) {
        // Store as many characters in the void* as the platform allows
        char text[sizeof(void*)];
        for (size_t pos = 0; pos < sizeof(void*) && j+pos < strLen; pos++) {
          text[pos] = sampleLabel[j+pos];
        }
        j += sizeof(void*);
        // Take '*((void**)(&text[0]))' to pass the char[] as a single void*
        aProfile.addTag(ProfileEntry('d', *((void**)(&text[0]))));
      }
    } else {
      aProfile.addTag(ProfileEntry('c', sampleLabel));
    }
  }
#ifdef ENABLE_SPS_LEAF_DATA
  if (sample) {
    aProfile.addTag(ProfileEntry('l', (void*)sample->pc));
#ifdef ENABLE_ARM_LR_SAVING
    aProfile.addTag(ProfileEntry('L', (void*)sample->lr));
#endif
  }
#endif
}

/* used to keep track of the last event that we sampled during */
unsigned int sLastSampledEventGeneration = 0;

/* a counter that's incremented everytime we get responsiveness event
 * note: it might also be worth tracking everytime we go around
 * the event loop */
unsigned int sCurrentEventGeneration = 0;
/* we don't need to worry about overflow because we only treat the
 * case of them being the same as special. i.e. we only run into
 * a problem if 2^32 events happen between samples that we need
 * to know are associated with different events */

void TableTicker::Tick(TickSample* sample)
{
  // Marker(s) come before the sample
  ProfileStack* stack = mPrimaryThreadProfile.GetStack();
  for (int i = 0; stack->getMarker(i) != NULL; i++) {
    mPrimaryThreadProfile.addTag(ProfileEntry('m', stack->getMarker(i)));
  }
  stack->mQueueClearMarker = true;

  bool recordSample = true;
  if (mJankOnly) {
    // if we are on a different event we can discard any temporary samples
    // we've kept around
    if (sLastSampledEventGeneration != sCurrentEventGeneration) {
      // XXX: we also probably want to add an entry to the profile to help
      // distinguish which samples are part of the same event. That, or record
      // the event generation in each sample
      mPrimaryThreadProfile.erase();
    }
    sLastSampledEventGeneration = sCurrentEventGeneration;

    recordSample = false;
    // only record the events when we have a we haven't seen a tracer event for 100ms
    if (!sLastTracerEvent.IsNull()) {
      TimeDuration delta = sample->timestamp - sLastTracerEvent;
      if (delta.ToMilliseconds() > 100.0) {
          recordSample = true;
      }
    }
  }

#if defined(USE_BACKTRACE) || defined(USE_NS_STACKWALK) || defined(USE_LIBUNWIND)
  if (mUseStackWalk) {
    doBacktrace(mPrimaryThreadProfile, sample);
  } else {
    doSampleStackTrace(stack, mPrimaryThreadProfile, sample);
  }
#else
  doSampleStackTrace(stack, mPrimaryThreadProfile, sample);
#endif

  if (recordSample)
    mPrimaryThreadProfile.flush();

  if (!sLastTracerEvent.IsNull() && sample) {
    TimeDuration delta = sample->timestamp - sLastTracerEvent;
    mPrimaryThreadProfile.addTag(ProfileEntry('r', delta.ToMilliseconds()));
  }

  if (sample) {
    TimeDuration delta = sample->timestamp - mStartTime;
    mPrimaryThreadProfile.addTag(ProfileEntry('t', delta.ToMilliseconds()));
  }
}

std::ostream& operator<<(std::ostream& stream, const ThreadProfile& profile)
{
  int readPos = profile.mReadPos;
  while (readPos != profile.mLastFlushPos) {
    stream << profile.mEntries[readPos];
    readPos = (readPos + 1) % profile.mEntrySize;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const ProfileEntry& entry)
{
  if (entry.mTagName == 'r' || entry.mTagName == 't') {
    stream << entry.mTagName << "-" << std::fixed << entry.mTagFloat << "\n";
  } else if (entry.mTagName == 'l' || entry.mTagName == 'L') {
    // Bug 739800 - Force l-tag addresses to have a "0x" prefix on all platforms
    // Additionally, stringstream seemed to be ignoring formatter flags.
    char tagBuff[1024];
    unsigned long long pc = (unsigned long long)(uintptr_t)entry.mTagPtr;
    snprintf(tagBuff, 1024, "%c-%#llx\n", entry.mTagName, pc);
    stream << tagBuff;
  } else if (entry.mTagName == 'd') {
    // TODO implement 'd' tag for text profile
  } else {
    stream << entry.mTagName << "-" << entry.mTagData << "\n";
  }
  return stream;
}

void mozilla_sampler_init()
{
  if (!tlsStack.init() || !tlsTicker.init()) {
    LOG("Failed to init.");
    return;
  }
  stack_key_initialized = true;

  ProfileStack *stack = new ProfileStack();
  tlsStack.set(stack);

#if defined(USE_LIBUNWIND) && defined(ANDROID)
  // Only try debug_frame and exidx unwinding
  putenv("UNW_ARM_UNWIND_METHOD=5");

  // Allow the profiler to be started and stopped using signals
  OS::RegisterStartStopHandlers();

  // On Android, this is too soon in order to start up the
  // profiler.
  return;
#endif

  // We can't open pref so we use an environment variable
  // to know if we should trigger the profiler on startup
  // NOTE: Default
  const char *val = PR_GetEnv("MOZ_PROFILER_STARTUP");
  if (!val || !*val) {
    return;
  }

  mozilla_sampler_start(PROFILE_DEFAULT_ENTRY, PROFILE_DEFAULT_INTERVAL,
                        PROFILE_DEFAULT_FEATURES, PROFILE_DEFAULT_FEATURE_COUNT);
}

void mozilla_sampler_deinit()
{
  mozilla_sampler_stop();
  // We can't delete the Stack because we can be between a
  // sampler call_enter/call_exit point.
  // TODO Need to find a safe time to delete Stack
}

void mozilla_sampler_save()
{
  TableTicker *t = tlsTicker.get();
  if (!t) {
    return;
  }

  t->RequestSave();
  // We're on the main thread already so we don't
  // have to wait to handle the save request.
  t->HandleSaveRequest();
}

char* mozilla_sampler_get_profile()
{
  TableTicker *t = tlsTicker.get();
  if (!t) {
    return NULL;
  }

  std::stringstream profile;
  t->SetPaused(true);
  profile << *(t->GetPrimaryThreadProfile());
  t->SetPaused(false);

  std::string profileString = profile.str();
  char *rtn = (char*)malloc( (profileString.length() + 1) * sizeof(char) );
  strcpy(rtn, profileString.c_str());
  return rtn;
}

JSObject *mozilla_sampler_get_profile_data(JSContext *aCx)
{
  TableTicker *t = tlsTicker.get();
  if (!t) {
    return NULL;
  }

  return t->ToJSObject(aCx);
}


const char** mozilla_sampler_get_features()
{
  static const char* features[] = {
#if defined(MOZ_PROFILING) && (defined(USE_BACKTRACE) || defined(USE_NS_STACKWALK) || defined(USE_LIBUNWIND))
    "stackwalk",
#endif
    "jank",
    "js",
    NULL
  };

  return features;
}

// Values are only honored on the first start
void mozilla_sampler_start(int aProfileEntries, int aInterval,
                           const char** aFeatures, uint32_t aFeatureCount)
{
  if (!stack_key_initialized)
    mozilla_sampler_init();

  ProfileStack *stack = tlsStack.get();
  if (!stack) {
    ASSERT(false);
    return;
  }

  mozilla_sampler_stop();

  TableTicker *t = new TableTicker(aInterval, aProfileEntries, stack,
                                   aFeatures, aFeatureCount);
  tlsTicker.set(t);
  t->Start();
  if (t->ProfileJS())
      stack->installJSSampling();
}

void mozilla_sampler_stop()
{
  if (!stack_key_initialized)
    mozilla_sampler_init();

  TableTicker *t = tlsTicker.get();
  if (!t) {
    return;
  }

  bool uninstallJS = t->ProfileJS();

  t->Stop();
  delete t;
  tlsTicker.set(NULL);
  ProfileStack *stack = tlsStack.get();
  ASSERT(stack != NULL);

  if (uninstallJS)
    stack->uninstallJSSampling();
}

bool mozilla_sampler_is_active()
{
  if (!stack_key_initialized)
    mozilla_sampler_init();

  TableTicker *t = tlsTicker.get();
  if (!t) {
    return false;
  }

  return t->IsActive();
}

double sResponsivenessTimes[100];
double sCurrResponsiveness = 0.f;
unsigned int sResponsivenessLoc = 0;
void mozilla_sampler_responsiveness(TimeStamp aTime)
{
  if (!sLastTracerEvent.IsNull()) {
    if (sResponsivenessLoc == 100) {
      for(size_t i = 0; i < 100-1; i++) {
        sResponsivenessTimes[i] = sResponsivenessTimes[i+1];
      }
      sResponsivenessLoc--;
    }
    TimeDuration delta = aTime - sLastTracerEvent;
    sResponsivenessTimes[sResponsivenessLoc++] = delta.ToMilliseconds();
  }
  sCurrentEventGeneration++;

  sLastTracerEvent = aTime;
}

const double* mozilla_sampler_get_responsiveness()
{
  return sResponsivenessTimes;
}

