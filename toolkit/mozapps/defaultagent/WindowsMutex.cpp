/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "WindowsMutex.h"

namespace mozilla::default_agent {

using mozilla::LogLevel;

static LazyLogModule gWindowsMutexLog("WindowsMutex");

NS_IMPL_ISUPPORTS(WindowsMutexFactory, nsIWindowsMutexFactory)

NS_IMETHODIMP
WindowsMutexFactory::CreateMutex(const nsAString& aName,
                                 nsIWindowsMutex** aWindowsMutex) {
  nsAutoHandle mutex;
  auto name = PromiseFlatString(aName);

  mutex.own(CreateMutexW(nullptr, FALSE, name.get()));
  if (mutex.get() == nullptr) {
    MOZ_LOG(gWindowsMutexLog, LogLevel::Error,
            ("Couldn't open mutex \"%s\": %#lX",
             NS_ConvertUTF16toUTF8(name).get(), GetLastError()));
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<WindowsMutex> nsMutex = new WindowsMutex(name, mutex);
  nsMutex.forget(aWindowsMutex);
  return NS_OK;
}

WindowsMutex::WindowsMutex(const nsString& aName, nsAutoHandle& aMutex)
    : mName(NS_ConvertUTF16toUTF8(aName)), mLocked(false) {
  mMutex.steal(aMutex);
}

WindowsMutex::~WindowsMutex() {
  Unlock();
  // nsAutoHandle will take care of closing the mutex's handle.
}

NS_IMPL_ISUPPORTS(WindowsMutex, nsIWindowsMutex)

NS_IMETHODIMP
WindowsMutex::TryLock() {
  // This object may be used on the main thread, so don't wait if it's
  // not signaled.
  DWORD mutexStatus = WaitForSingleObject(mMutex.get(), 0);
  if (mutexStatus == WAIT_OBJECT_0) {
    mLocked = true;
  } else if (mutexStatus == WAIT_TIMEOUT) {
    MOZ_LOG(gWindowsMutexLog, LogLevel::Warning,
            ("Timed out waiting for mutex \"%s\"", mName.get()));
  } else if (mutexStatus == WAIT_ABANDONED) {
    // This status code means that we are supposed to check our data for
    // consistency as the last locking process didn't signal intentional
    // unlocking which might indicate it crashed mid-operation. Current uses of
    // this `WindowsMutex` don't need to worry about corruption of the locked
    // object, if needed the `nsIWindowsMutex` interface should be extended.
    MOZ_LOG(gWindowsMutexLog, LogLevel::Warning,
            ("Found abandoned mutex \"%s\". Continuing...", mName.get()));
    mLocked = true;
  } else {
    // The only other documented status code is WAIT_FAILED. In the case that
    // we somehow get some other code, that is also an error.
    MOZ_LOG(gWindowsMutexLog, LogLevel::Error,
            ("Failed to wait on mutex: mName: %s, error %#lX", mName.get(),
             GetLastError()));
  }
  return mLocked ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
WindowsMutex::IsLocked(bool* aLocked) {
  *aLocked = mLocked;
  return NS_OK;
}

NS_IMETHODIMP
WindowsMutex::Unlock() {
  nsresult rv = NS_OK;

  if (mLocked) {
    BOOL success = ReleaseMutex(mMutex.get());
    if (!success) {
      MOZ_LOG(gWindowsMutexLog, LogLevel::Error,
              ("Failed to release mutex \"%s\"", mName.get()));
      rv = NS_ERROR_UNEXPECTED;
    }

    mLocked = false;
  }

  return rv;
}

}  // namespace mozilla::default_agent
