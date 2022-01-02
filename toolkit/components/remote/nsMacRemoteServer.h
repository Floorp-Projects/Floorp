/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TOOLKIT_COMPONENTS_REMOTE_NSMACREMOTESERVER_H_
#define TOOLKIT_COMPONENTS_REMOTE_NSMACREMOTESERVER_H_

#import <CoreFoundation/CoreFoundation.h>

#include "nsRemoteServer.h"

class nsMacRemoteServer final : public nsRemoteServer {
 public:
  nsMacRemoteServer() = default;
  ~nsMacRemoteServer() override { Shutdown(); }

  nsresult Startup(const char* aAppName, const char* aProfileName) override;
  void Shutdown() override;

  void HandleCommandLine(CFDataRef aData);

 private:
  CFRunLoopSourceRef mRunLoopSource;
  CFMessagePortRef mMessageServer;
};

#endif  // TOOLKIT_COMPONENTS_REMOTE_NSMACREMOTESERVER_H_
