/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsPROCESSWIN_H_
#define _nsPROCESSWIN_H_

#if defined(XP_WIN)
#  define PROCESSMODEL_WINAPI
#endif

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "nsIProcess.h"
#include "nsIObserver.h"
#include "nsMaybeWeakPtr.h"
#include "nsString.h"
#ifndef XP_UNIX
#  include "prproces.h"
#endif
#if defined(PROCESSMODEL_WINAPI)
#  include <windows.h>
#  include <shellapi.h>
#endif

#define NS_PROCESS_CID                               \
  {                                                  \
    0x7b4eeb20, 0xd781, 0x11d4, {                    \
      0x8A, 0x83, 0x00, 0x10, 0xa4, 0xe0, 0xc9, 0xca \
    }                                                \
  }

class nsIFile;

class nsProcess final : public nsIProcess, public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPROCESS
  NS_DECL_NSIOBSERVER

  nsProcess();

 private:
  ~nsProcess();
  PRThread* CreateMonitorThread();
  static void Monitor(void* aArg);
  void ProcessComplete();
  nsresult CopyArgsAndRunProcess(bool aBlocking, const char** aArgs,
                                 uint32_t aCount, nsIObserver* aObserver,
                                 bool aHoldWeak);
  nsresult CopyArgsAndRunProcessw(bool aBlocking, const char16_t** aArgs,
                                  uint32_t aCount, nsIObserver* aObserver,
                                  bool aHoldWeak);
  // The 'args' array is null-terminated.
  nsresult RunProcess(bool aBlocking, char** aArgs, nsIObserver* aObserver,
                      bool aHoldWeak, bool aArgsUTF8);

  PRThread* mThread;
  mozilla::Mutex mLock;
  bool mShutdown MOZ_GUARDED_BY(mLock);
  bool mBlocking;
  bool mStartHidden;
  bool mNoShell;

  nsCOMPtr<nsIFile> mExecutable;
  nsString mTargetPath;
  int32_t mPid;
  nsMaybeWeakPtr<nsIObserver> mObserver;

  // These members are modified by multiple threads, any accesses should be
  // protected with mLock.
  int32_t mExitValue MOZ_GUARDED_BY(mLock);
#if defined(PROCESSMODEL_WINAPI)
  HANDLE mProcess MOZ_GUARDED_BY(mLock);
#elif !defined(XP_UNIX)
  PRProcess* mProcess MOZ_GUARDED_BY(mLock);
#endif
};

#endif
