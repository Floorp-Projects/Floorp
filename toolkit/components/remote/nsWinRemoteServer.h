/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWinRemoteServer_h__
#define __nsWinRemoteServer_h__

#include "nsRemoteServer.h"

#include <windows.h>

class nsWinRemoteServer final : public nsRemoteServer {
 public:
  nsWinRemoteServer() = default;
  ~nsWinRemoteServer() override { Shutdown(); }

  nsresult Startup(const char* aAppName, const char* aProfileName) override;
  void Shutdown() override;

 private:
  HWND mHandle;
};

#endif  // __nsWinRemoteService_h__
