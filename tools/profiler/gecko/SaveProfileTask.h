/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROFILER_SAVETASK_H_
#define PROFILER_SAVETASK_H_

#include "platform.h"
#include "nsThreadUtils.h"
#include "nsIXULRuntime.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsIProfileSaveEvent.h"

#ifdef XP_WIN
 #include <windows.h>
 #define getpid GetCurrentProcessId
#else
 #include <unistd.h>
#endif

/**
 * This is an event used to save the profile on the main thread
 * to be sure that it is not being modified while saving.
 */
class SaveProfileTask : public mozilla::Runnable {
public:
  SaveProfileTask() {}

  NS_IMETHOD Run();
};

class ProfileSaveEvent final : public nsIProfileSaveEvent {
public:
  typedef void (*AddSubProfileFunc)(const char* aProfile, void* aClosure);
  NS_DECL_ISUPPORTS

  ProfileSaveEvent(AddSubProfileFunc aFunc, void* aClosure)
    : mFunc(aFunc)
    , mClosure(aClosure)
  {}

  NS_IMETHOD AddSubProfile(const char* aProfile) override;
private:
  ~ProfileSaveEvent() {}

  AddSubProfileFunc mFunc;
  void* mClosure;
};

#endif

