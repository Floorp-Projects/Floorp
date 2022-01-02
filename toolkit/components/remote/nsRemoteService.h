/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TOOLKIT_COMPONENTS_REMOTE_NSREMOTESERVER_H_
#define TOOLKIT_COMPONENTS_REMOTE_NSREMOTESERVER_H_

#include "nsRemoteServer.h"
#include "nsIObserver.h"
#include "mozilla/UniquePtr.h"
#include "nsIFile.h"
#include "nsProfileLock.h"

enum RemoteResult {
  REMOTE_NOT_FOUND = 0,
  REMOTE_FOUND = 1,
  REMOTE_ARG_BAD = 2
};

class nsRemoteService final : public nsIObserver {
 public:
  // We will be a static singleton, so don't use the ordinary methods.
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  explicit nsRemoteService(const char* aProgram);
  void SetProfile(nsACString& aProfile);

  void LockStartup();
  void UnlockStartup();

  RemoteResult StartClient(const char* aDesktopStartupID);
  void StartupServer();
  void ShutdownServer();

 private:
  ~nsRemoteService();

  mozilla::UniquePtr<nsRemoteServer> mRemoteServer;
  nsProfileLock mRemoteLock;
  nsCOMPtr<nsIFile> mRemoteLockDir;
  nsCString mProgram;
  nsCString mProfile;
};

#endif  // TOOLKIT_COMPONENTS_REMOTE_NSREMOTESERVER_H_
