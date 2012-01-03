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

// we eventually want to make this runtime switchable
//#define USE_BACKTRACE
#ifdef USE_BACKTRACE
#include <execinfo.h>
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

class Profile;

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

  ProfileEntry(char aTagName, float aTagFloat)
    : mTagFloat(aTagFloat)
    , mLeafAddress(0)
    , mTagName(aTagName)
  { }

  string TagToString(Profile *profile);

private:
  union {
    const char* mTagData;
    float mTagFloat;
    Address mTagAddress;
  };
  Address mLeafAddress;
  char mTagName;
};

#define PROFILE_MAX_ENTRY 100000
class Profile
{
public:
  Profile(int aEntrySize)
    : mWritePos(0)
    , mReadPos(0)
    , mEntrySize(aEntrySize)
  {
    mEntries = new ProfileEntry[mEntrySize];
    mNeedsSharedLibraryInfo = false;
#if defined(ENABLE_SPS_LEAF_DATA) || defined(USE_BACKTRACE)
    mNeedsSharedLibraryInfo = true;
#endif
  }

  ~Profile()
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
  }

  void ToString(StringBuilder &profile)
  {
    if (mNeedsSharedLibraryInfo) {
      // Can't be called from signal because
      // getting the shared library information can call non-reentrant functions.
      mSharedLibraryInfo = SharedLibraryInfo::GetInfoForSelf();
    }

    int oldReadPos = mReadPos;
    while (mReadPos != mWritePos) {
      profile.Append(mEntries[mReadPos].TagToString(this).c_str());
      mReadPos = (mReadPos + 1) % mEntrySize;
    }
    mReadPos = oldReadPos;
  }

  void WriteProfile(FILE* stream)
  {
    if (mNeedsSharedLibraryInfo) {
      // Can't be called from signal because
      // getting the shared library information can call non-reentrant functions.
      mSharedLibraryInfo = SharedLibraryInfo::GetInfoForSelf();
    }

    int oldReadPos = mReadPos;
    while (mReadPos != mWritePos) {
      string tag = mEntries[mReadPos].TagToString(this);
      fwrite(tag.data(), 1, tag.length(), stream);
      mReadPos = (mReadPos + 1) % mEntrySize;
    }
    mReadPos = oldReadPos;
  }

  SharedLibraryInfo& getSharedLibraryInfo()
  {
    return mSharedLibraryInfo;
  }
private:
  // Circular buffer 'Keep One Slot Open' implementation
  // for simplicity
  ProfileEntry *mEntries;
  int mWritePos; // points to the next entry we will write to
  int mReadPos;  // points to the next entry we will read to
  int mEntrySize;
  bool mNeedsSharedLibraryInfo;
  SharedLibraryInfo mSharedLibraryInfo;
};

class SaveProfileTask;

class TableTicker: public Sampler {
 public:
  explicit TableTicker(int aInterval, int aEntrySize, Stack *aStack)
    : Sampler(aInterval, true)
    , mProfile(aEntrySize)
    , mStack(aStack)
    , mSaveRequested(false)
  {
    mProfile.addTag(ProfileEntry('m', "Start"));
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

  Stack* GetStack()
  {
    return mStack;
  }

  Profile* GetProfile()
  {
    return &mProfile;
  }
 private:
  Profile mProfile;
  Stack *mStack;
  bool mSaveRequested;
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
      t->GetProfile()->WriteProfile(stream);
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
static
void doBacktrace(Profile &aProfile)
{
  void *array[100];
  int count = backtrace (array, 100);

  bool isSignal = true;
#ifndef __i386__
  // the test doesn't work for 64bit
  isSignal = false;
#endif
  for (int i = count-1; i >= 0; i--) {
    if( isSignal ) {
      if( (intptr_t)array[i] == -1 ) { // signal frames have addresses of -1?
        isSignal = false;
      }
      continue;
    }
    aProfile.addTag(ProfileEntry('l', (const char*)array[i]));
  }
  aProfile.addTag(ProfileEntry('s', "XRE_Main", 0));
}
#endif

static
void doSampleStackTrace(Stack *aStack, Profile &aProfile, TickSample *sample)
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

void TableTicker::Tick(TickSample* sample)
{
  // Marker(s) come before the sample
  int i = 0;
  const char *marker = mStack->getMarker(i++);
  for (int i = 0; marker != NULL; i++) {
    mProfile.addTag(ProfileEntry('m', marker));
    marker = mStack->getMarker(i++);
  }
  mStack->mQueueClearMarker = true;

#ifdef USE_BACKTRACE
  doBacktrace(mProfile);
#else
  doSampleStackTrace(mStack, mProfile, sample);
#endif

  if (!sLastTracerEvent.IsNull()) {
    TimeDuration delta = sample->timestamp - sLastTracerEvent;
    mProfile.addTag(ProfileEntry('r', delta.ToMilliseconds()));
  }
}

string ProfileEntry::TagToString(Profile *profile)
{
  string tag = "";
  if (mTagName == 'r') {
    char buff[50];
    snprintf(buff, 50, "%-40f", mTagFloat);
    tag += string(1, mTagName) + string("-") + string(buff) + string("\n");
  } else if (mTagName == 'l') {
    bool found = false;
    char tagBuff[1024];
    SharedLibraryInfo& shlibInfo = profile->getSharedLibraryInfo();
    Address pc = mTagAddress;
    // TODO Use binary sort (STL)
    for (size_t i = 0; i < shlibInfo.GetSize(); i++) {
      SharedLibrary &e = shlibInfo.GetEntry(i);
      if (pc > (Address)e.GetStart() && pc < (Address)e.GetEnd()) {
        if (e.GetName()) {
          found = true;
          snprintf(tagBuff, 1024, "l-%s@%p\n", e.GetName(), pc - e.GetStart());
          tag += string(tagBuff);
          break;
        }
      }
    }
    if (!found) {
      snprintf(tagBuff, 1024, "l-???@%p\n", pc);
      tag += string(tagBuff);
    }
  } else {
    tag += string(1, mTagName) + string("-") + string(mTagData) + string("\n");
  }

#ifdef ENABLE_SPS_LEAF_DATA
  if (mLeafAddress) {
    bool found = false;
    char tagBuff[1024];
    SharedLibraryInfo& shlibInfo = profile->getSharedLibraryInfo();
    unsigned long pc = (unsigned long)mLeafAddress;
    // TODO Use binary sort (STL)
    for (size_t i = 0; i < shlibInfo.GetSize(); i++) {
      SharedLibrary &e = shlibInfo.GetEntry(i);
      if (pc > e.GetStart() && pc < e.GetEnd()) {
        if (e.GetName()) {
          found = true;
          snprintf(tagBuff, 1024, "l-%900s@%llu\n", e.GetName(), pc - e.GetStart());
          tag += string(tagBuff);
          break;
        }
      }
    }
    if (!found) {
      snprintf(tagBuff, 1024, "l-???@%llu\n", pc);
      tag += string(tagBuff);
    }
  }
#endif
  return tag;
}

#define PROFILE_DEFAULT_ENTRY 100000
#define PROFILE_DEFAULT_INTERVAL 10

void mozilla_sampler_init()
{
  // TODO linux port: Use TLS with ifdefs
  if (!mozilla::tls::create(&pkey_stack) ||
      !mozilla::tls::create(&pkey_ticker)) {
    LOG("Failed to init.");
    return;
  }
  stack_key_initialized = true;

  Stack *stack = new Stack();
  mozilla::tls::set(pkey_stack, stack);

  // We can't open pref so we use an environment variable
  // to know if we should trigger the profiler on startup
  // NOTE: Default
  const char *val = PR_GetEnv("MOZ_PROFILER_STARTUP");
  if (!val || !*val) {
    return;
  }

  mozilla_sampler_start(PROFILE_DEFAULT_ENTRY, PROFILE_DEFAULT_INTERVAL);
}

void mozilla_sampler_deinit()
{
  mozilla_sampler_stop();
  // We can't delete the Stack because we can be between a
  // sampler call_enter/call_exit point.
  // TODO Need to find a safe time to delete Stack
}

void mozilla_sampler_save() {
  TableTicker *t = mozilla::tls::get<TableTicker>(pkey_ticker);
  if (!t) {
    return;
  }

  t->RequestSave();
  // We're on the main thread already so we don't
  // have to wait to handle the save request.
  t->HandleSaveRequest();
}

char* mozilla_sampler_get_profile() {
  TableTicker *t = mozilla::tls::get<TableTicker>(pkey_ticker);
  if (!t) {
    return NULL;
  }

  StringBuilder profile;
  t->GetProfile()->ToString(profile);

  char *rtn = (char*)malloc( (profile.Length()+1) * sizeof(char) );
  strcpy(rtn, profile.Buffer());
  return rtn;
}

// Values are only honored on the first start
void mozilla_sampler_start(int aProfileEntries, int aInterval)
{
  Stack *stack = mozilla::tls::get<Stack>(pkey_stack);
  if (!stack) {
    ASSERT(false);
    return;
  }

  mozilla_sampler_stop();

  TableTicker *t = new TableTicker(aInterval, aProfileEntries, stack);
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
  mozilla::tls::set(pkey_ticker, (Stack*)NULL);
}

bool mozilla_sampler_is_active()
{
  TableTicker *t = mozilla::tls::get<TableTicker>(pkey_ticker);
  if (!t) {
    return false;
  }

  return t->IsActive();
}

float sResponsivenessTimes[100];
float sCurrResponsiveness = 0.f;
unsigned int sResponsivenessLoc = 0;
void mozilla_sampler_responsiveness(TimeStamp aTime)
{
  if (!sLastTracerEvent.IsNull()) {
    if (sResponsivenessLoc == 100) {
      for(size_t i = 0; i < 100-1; i++) {
        sResponsivenessTimes[i] = sResponsivenessTimes[i+1];
      }
      sResponsivenessLoc--;
      //for(size_t i = 0; i < 100; i++) {
      //  sResponsivenessTimes[i] = 0;
      //}
      //sResponsivenessLoc = 0;
    }
    TimeDuration delta = aTime - sLastTracerEvent;
    sResponsivenessTimes[sResponsivenessLoc++] = delta.ToMilliseconds();
  }

  sLastTracerEvent = aTime;
}

const float* mozilla_sampler_get_responsiveness()
{
  return sResponsivenessTimes;
}

