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

#include <sys/stat.h>   // open
#include <fcntl.h>      // open
#include <unistd.h>
#include <stdio.h>
#include <semaphore.h>
#include <string.h>
#include "sps_sampler.h"
#include "platform.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"
#include "prenv.h"

pthread_key_t pkey_stack;
pthread_key_t pkey_ticker;

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

  ProfileEntry(char aTagName, const char *aTagData, Address aLeafAddress)
    : mTagData(aTagData)
    , mLeafAddress(aLeafAddress)
    , mTagName(aTagName)
  { }

  void WriteTag(Profile *profile, FILE* stream);

private:
  const char* mTagData;
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

  void WriteProfile(FILE* stream)
  {
    LOG("Save profile");
    // Can't be called from signal because
    // get_maps calls non reentrant functions.
#ifdef ENABLE_SPS_LEAF_DATA
    mMaps = getmaps(getpid());
#endif

    int oldReadPos = mReadPos;
    while (mReadPos != mWritePos) {
      mEntries[mReadPos].WriteTag(this, stream);
      mReadPos = (mReadPos + 1) % mEntrySize;
    }
    mReadPos = oldReadPos;
  }

#ifdef ENABLE_SPS_LEAF_DATA
  MapInfo& getMap()
  {
    return mMaps;
  }
#endif
private:
  // Circular buffer 'Keep One Slot Open' implementation
  // for simplicity
  ProfileEntry *mEntries;
  int mWritePos; // points to the next entry we will write to
  int mReadPos;  // points to the next entry we will read to
  int mEntrySize;
#ifdef ENABLE_SPS_LEAF_DATA
  MapInfo mMaps;
#endif
};

class SaveProfileTask;

class TableTicker: public Sampler {
 public:
  explicit TableTicker(int aEntrySize, int aInterval)
    : Sampler(aInterval, true)
    , mProfile(aEntrySize)
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
    return &mStack;
  }

  Profile* GetProfile()
  {
    return &mProfile;
  }
 private:
  Profile mProfile;
  Stack mStack;
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
    TableTicker *t = (TableTicker*)pthread_getspecific(pkey_ticker);

    char buff[PATH_MAX];
#ifdef ANDROID
  #define FOLDER "/sdcard/"
#else
  #define FOLDER "/tmp/"
#endif
    snprintf(buff, PATH_MAX, FOLDER "profile_%i_%i.txt", XRE_GetProcessType(), getpid());

    FILE* stream = ::fopen(buff, "w");
    if (stream) {
      t->GetProfile()->WriteProfile(stream);
      ::fclose(stream);
      LOG("Saved to " FOLDER "profile_TYPE_ID.txt");
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


void TableTicker::Tick(TickSample* sample)
{
  // Marker(s) come before the sample
  int i = 0;
  const char *marker = mStack.getMarker(i++);
  for (int i = 0; marker != NULL; i++) {
    mProfile.addTag(ProfileEntry('m', marker));
    marker = mStack.getMarker(i++);
  }
  mStack.mQueueClearMarker = true;

  // Sample
  // 's' tag denotes the start of a sample block
  // followed by 0 or more 'c' tags.
  for (int i = 0; i < mStack.mStackPointer; i++) {
    if (i == 0) {
      Address pc = 0;
      if (sample) {
        pc = sample->pc;
      }
      mProfile.addTag(ProfileEntry('s', mStack.mStack[i], pc));
    } else {
      mProfile.addTag(ProfileEntry('c', mStack.mStack[i]));
    }
  }
}


void ProfileEntry::WriteTag(Profile *profile, FILE *stream)
{
  fprintf(stream, "%c-%s\n", mTagName, mTagData);

#ifdef ENABLE_SPS_LEAF_DATA
  if (mLeafAddress) {
    bool found = false;
    MapInfo& maps = profile->getMap();
    unsigned long pc = (unsigned long)mLeafAddress;
    // TODO Use binary sort (STL)
    for (size_t i = 0; i < maps.GetSize(); i++) {
      MapEntry &e = maps.GetEntry(i);
      if (pc > e.GetStart() && pc < e.GetEnd()) {
        if (e.GetName()) {
          found = true;
          fprintf(stream, "l-%s@%li\n", e.GetName(), pc - e.GetStart());
          break;
        }
      }
    }
    if (!found) {
      fprintf(stream, "l-???@%li\n", pc);
    }
  }
#endif
}

#define PROFILE_DEFAULT_ENTRY 100000
void mozilla_sampler_init()
{
  const char *val = PR_GetEnv("MOZ_PROFILER_SPS");
  if (!val || !*val) {
    return;
  }

  // TODO linux port: Use TLS with ifdefs
  // TODO window port: See bug 683229 comment 15
  // profiler uses getspecific because TLS is not supported on android.
  // getspecific was picked over nspr because it had less overhead required
  // to make the checkpoint function fast.
  if (pthread_key_create(&pkey_stack, NULL) ||
        pthread_key_create(&pkey_ticker, NULL)) {
    LOG("Failed to init.");
    return;
  }

  TableTicker *t = new TableTicker(PROFILE_DEFAULT_ENTRY, 10);
  pthread_setspecific(pkey_ticker, t);
  pthread_setspecific(pkey_stack, t->GetStack());

  t->Start();
}

void mozilla_sampler_deinit()
{
  TableTicker *t = (TableTicker*)pthread_getspecific(pkey_ticker);
  if (!t) {
    return;
  }

  t->Stop();
  pthread_setspecific(pkey_stack, NULL);
  // We can't delete the TableTicker because we can be between a
  // sampler call_enter/call_exit point.
  // TODO Need to find a safe time to delete TableTicker
}

