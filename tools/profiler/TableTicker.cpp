/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Girard <bgirard@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <string>
#include <stdio.h>
#include "sps_sampler.h"
#include "platform.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"
#include "prenv.h"
#include "shared-libraries.h"
#include "mozilla/StringBuilder.h"
#include "mozilla/StackWalk.h"
#include "JSObjectBuilder.h"

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


mozilla::tls::key pkey_stack;
mozilla::tls::key pkey_ticker;
// We need to track whether we've been initialized otherwise
// we end up using pkey_stack without initializing it.
// Because pkey_stack is totally opaque to us we can't reuse
// it as the flag itself.
bool stack_key_initialized;

TimeStamp sLastTracerEvent;

class ThreadProfile;

class ProfileEntry
{
public:
  ProfileEntry()
    : mTagData(NULL)
    , mLeafAddress(0)
    , mTagName(0)
  { }

  // aTagData must not need release (i.e. be a string from the text segment)
  ProfileEntry(char aTagName, const char *aTagData)
    : mTagData(aTagData)
    , mLeafAddress(0)
    , mTagName(aTagName)
  { }

  // aTagData must not need release (i.e. be a string from the text segment)
  ProfileEntry(char aTagName, const char *aTagData, Address aLeafAddress)
    : mTagData(aTagData)
    , mLeafAddress(aLeafAddress)
    , mTagName(aTagName)
  { }

  ProfileEntry(char aTagName, double aTagFloat)
    : mTagFloat(aTagFloat)
    , mLeafAddress(0)
    , mTagName(aTagName)
  { }

  ProfileEntry(char aTagName, uintptr_t aTagOffset)
    : mTagOffset(aTagOffset)
    , mLeafAddress(0)
    , mTagName(aTagName)
  { }

  string TagToString(ThreadProfile *profile);

private:
  friend class ThreadProfile;
  union {
    const char* mTagData;
    double mTagFloat;
    Address mTagAddress;
    uintptr_t mTagOffset;
  };
  Address mLeafAddress;
  char mTagName;
};

#define PROFILE_MAX_ENTRY 100000
class ThreadProfile
{
public:
  ThreadProfile(int aEntrySize)
    : mWritePos(0)
    , mLastFlushPos(0)
    , mReadPos(0)
    , mEntrySize(aEntrySize)
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

  void ToString(StringBuilder &profile)
  {
    int readPos = mReadPos;
    while (readPos != mLastFlushPos) {
      profile.Append(mEntries[readPos].TagToString(this).c_str());
      readPos = (readPos + 1) % mEntrySize;
    }
  }

  JSObject *ToJSObject(JSContext *aCx)
  {
    JSObjectBuilder b(aCx);

    JSObject *profile = b.CreateObject();
    JSObject *samples = b.CreateArray();
    b.DefineProperty(profile, "samples", samples);

    JSObject *sample = NULL;
    JSObject *frames = NULL;

    int oldReadPos = mReadPos;
    while (mReadPos != mLastFlushPos) {
      ProfileEntry entry = mEntries[mReadPos];
      mReadPos = (mReadPos + 1) % mEntrySize;
      switch (entry.mTagName) {
        case 's':
          sample = b.CreateObject();
          b.DefineProperty(sample, "name", entry.mTagData);
          frames = b.CreateArray();
          b.DefineProperty(sample, "frames", frames);
          b.ArrayPush(samples, sample);
          break;
        case 'c':
        case 'l':
          {
            if (sample) {
              JSObject *frame = b.CreateObject();
              b.DefineProperty(frame, "location", entry.mTagData);
              b.ArrayPush(frames, frame);
            }
          }
      }
    }
    mReadPos = oldReadPos;

    return profile;
  }

  void WriteProfile(FILE* stream)
  {
    int readPos = mReadPos;
    while (readPos != mLastFlushPos) {
      string tag = mEntries[readPos].TagToString(this);
      fwrite(tag.data(), 1, tag.length(), stream);
      readPos = (readPos + 1) % mEntrySize;
    }
  }
private:
  // Circular buffer 'Keep One Slot Open' implementation
  // for simplicity
  ProfileEntry *mEntries;
  int mWritePos; // points to the next entry we will write to
  int mLastFlushPos; // points to the next entry since the last flush()
  int mReadPos;  // points to the next entry we will read to
  int mEntrySize;
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
    , mPrimaryThreadProfile(aEntrySize)
    , mStack(aStack)
    , mSaveRequested(false)
  {
    mUseStackWalk = hasFeature(aFeatures, aFeatureCount, "stackwalk");
    //XXX: It's probably worth splitting the jank profiler out from the regular profiler at some point
    mJankOnly = hasFeature(aFeatures, aFeatureCount, "jank");
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

  ProfileStack* GetStack()
  {
    return mStack;
  }

  ThreadProfile* GetPrimaryThreadProfile()
  {
    return &mPrimaryThreadProfile;
  }

private:
  // Not implemented on platforms which do not support backtracing
  void doBacktrace(ThreadProfile &aProfile, Address pc);

private:
  // This represent the application's main thread (SAMPLER_INIT)
  ThreadProfile mPrimaryThreadProfile;
  ProfileStack *mStack;
  bool mSaveRequested;
  bool mUseStackWalk;
  bool mJankOnly;
};

/**
 * This is an event used to save the profile on the main thread
 * to be sure that it is not being modified while saving.
 */
class SaveProfileTask : public nsRunnable {
public:
  SaveProfileTask() {}

  NS_IMETHOD Run() {
    TableTicker *t = mozilla::tls::get<TableTicker>(pkey_ticker);

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

    FILE* stream = ::fopen(buff, "w");
    if (stream) {
      t->GetPrimaryThreadProfile()->WriteProfile(stream);
      ::fclose(stream);
      LOG("Saved to " FOLDER "profile_TYPE_PID.txt");
    } else {
      LOG("Fail to open profile log file.");
    }

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


#ifdef USE_BACKTRACE
void TableTicker::doBacktrace(ThreadProfile &aProfile, Address pc)
{
  void *array[100];
  int count = backtrace (array, 100);

  aProfile.addTag(ProfileEntry('s', "(root)", 0));

  for (int i = 0; i < count; i++) {
    if( (intptr_t)array[i] == -1 ) break;
    aProfile.addTag(ProfileEntry('l', (const char*)array[i]));
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

void TableTicker::doBacktrace(ThreadProfile &aProfile, Address fp)
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
#ifdef XP_MACOSX
  pthread_t pt = GetProfiledThread(platform_data());
  void *stackEnd = reinterpret_cast<void*>(-1);
  if (pt)
    stackEnd = static_cast<char*>(pthread_get_stackaddr_np(pt)) - pthread_get_stacksize_np(pt);
  nsresult rv = FramePointerStackWalk(StackWalkCallback, 1, &array, reinterpret_cast<void**>(fp), stackEnd);
#else
  nsresult rv = NS_StackWalk(StackWalkCallback, 0, &array, thread);
#endif
  if (NS_SUCCEEDED(rv)) {
    aProfile.addTag(ProfileEntry('s', "(root)", 0));

    for (size_t i = array.count; i > 0; --i) {
      aProfile.addTag(ProfileEntry('l', (const char*)array.array[i - 1]));
    }
  }
}
#endif

static
void doSampleStackTrace(ProfileStack *aStack, ThreadProfile &aProfile, TickSample *sample)
{
  // Sample
  // 's' tag denotes the start of a sample block
  // followed by 0 or more 'c' tags.
  for (int i = 0; i < aStack->mStackPointer; i++) {
    if (i == 0) {
      Address pc = 0;
      if (sample) {
        pc = sample->pc;
      }
      aProfile.addTag(ProfileEntry('s', aStack->mStack[i], pc));
    } else {
      aProfile.addTag(ProfileEntry('c', aStack->mStack[i]));
    }
  }
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
  for (int i = 0; mStack->getMarker(i) != NULL; i++) {
    mPrimaryThreadProfile.addTag(ProfileEntry('m', mStack->getMarker(i)));
  }
  mStack->mQueueClearMarker = true;

  // if we are on a different event we can discard any temporary samples
  // we've kept around
  if (sLastSampledEventGeneration != sCurrentEventGeneration) {
    // XXX: we also probably want to add an entry to the profile to help
    // distinguish which samples are part of the same event. That, or record
    // the event generation in each sample
    mPrimaryThreadProfile.erase();
  }
  sLastSampledEventGeneration = sCurrentEventGeneration;

  bool recordSample = true;
  if (mJankOnly) {
    recordSample = false;
    // only record the events when we have a we haven't seen a tracer event for 100ms
    if (!sLastTracerEvent.IsNull()) {
      TimeDuration delta = sample->timestamp - sLastTracerEvent;
      if (delta.ToMilliseconds() > 100.0) {
          recordSample = true;
      }
    }
  }

#if defined(USE_BACKTRACE) || defined(USE_NS_STACKWALK)
  if (mUseStackWalk) {
    doBacktrace(mPrimaryThreadProfile, sample->fp);
  } else {
    doSampleStackTrace(mStack, mPrimaryThreadProfile, sample);
  }
#else
  doSampleStackTrace(mStack, mPrimaryThreadProfile, sample);
#endif

  if (recordSample)
    mPrimaryThreadProfile.flush();

  if (!mJankOnly && !sLastTracerEvent.IsNull() && sample) {
    TimeDuration delta = sample->timestamp - sLastTracerEvent;
    mPrimaryThreadProfile.addTag(ProfileEntry('r', delta.ToMilliseconds()));
  }
}

string ProfileEntry::TagToString(mPrimaryThreadProfile *profile)
{
  string tag = "";
  if (mTagName == 'r') {
    char buff[50];
    snprintf(buff, 50, "%-40f", mTagFloat);
    tag += string(1, mTagName) + string("-") + string(buff) + string("\n");
  } else if (mTagName == 'l') {
    char tagBuff[1024];
    Address pc = mTagAddress;
    snprintf(tagBuff, 1024, "l-%p\n", pc);
    tag += string(tagBuff);
  } else {
    tag += string(1, mTagName) + string("-") + string(mTagData) + string("\n");
  }

#ifdef ENABLE_SPS_LEAF_DATA
  if (mLeafAddress) {
    char tagBuff[1024];
    unsigned long pc = (unsigned long)mLeafAddress;
    snprintf(tagBuff, 1024, "l-%llu\n", pc);
    tag += string(tagBuff);
  }
#endif
  return tag;
}

#define PROFILE_DEFAULT_ENTRY 100000
#define PROFILE_DEFAULT_INTERVAL 10
#define PROFILE_DEFAULT_FEATURES NULL
#define PROFILE_DEFAULT_FEATURE_COUNT 0

void mozilla_sampler_init()
{
  // TODO linux port: Use TLS with ifdefs
  if (!mozilla::tls::create(&pkey_stack) ||
      !mozilla::tls::create(&pkey_ticker)) {
    LOG("Failed to init.");
    return;
  }
  stack_key_initialized = true;

  ProfileStack *stack = new ProfileStack();
  mozilla::tls::set(pkey_stack, stack);

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
  TableTicker *t = mozilla::tls::get<TableTicker>(pkey_ticker);
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
  TableTicker *t = mozilla::tls::get<TableTicker>(pkey_ticker);
  if (!t) {
    return NULL;
  }

  StringBuilder profile;
  t->GetPrimaryThreadProfile()->ToString(profile);

  char *rtn = (char*)malloc( (profile.Length()+1) * sizeof(char) );
  strcpy(rtn, profile.Buffer());
  return rtn;
}

JSObject *mozilla_sampler_get_profile_data(JSContext *aCx)
{
  TableTicker *t = mozilla::tls::get<TableTicker>(pkey_ticker);
  if (!t) {
    return NULL;
  }

  return t->GetPrimaryThreadProfile()->ToJSObject(aCx);
}


const char** mozilla_sampler_get_features()
{
  static const char* features[] = {
#if defined(MOZ_PROFILING) && (defined(USE_BACKTRACE) || defined(USE_NS_STACKWALK))
    "stackwalk",
#endif
    "jank",
    NULL
  };

  return features;
}

// Values are only honored on the first start
void mozilla_sampler_start(int aProfileEntries, int aInterval,
                           const char** aFeatures, uint32_t aFeatureCount)
{
  ProfileStack *stack = mozilla::tls::get<ProfileStack>(pkey_stack);
  if (!stack) {
    ASSERT(false);
    return;
  }

  mozilla_sampler_stop();

  TableTicker *t = new TableTicker(aInterval, aProfileEntries, stack,
                                   aFeatures, aFeatureCount);
  mozilla::tls::set(pkey_ticker, t);
  t->Start();
}

void mozilla_sampler_stop()
{
  TableTicker *t = mozilla::tls::get<TableTicker>(pkey_ticker);
  if (!t) {
    return;
  }

  t->Stop();
  mozilla::tls::set(pkey_ticker, (ProfileStack*)NULL);
}

bool mozilla_sampler_is_active()
{
  TableTicker *t = mozilla::tls::get<TableTicker>(pkey_ticker);
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

