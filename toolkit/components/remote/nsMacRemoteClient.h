/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TOOLKIT_COMPONENTS_REMOTE_NSMACREMOTECLIENT_H_
#define TOOLKIT_COMPONENTS_REMOTE_NSMACREMOTECLIENT_H_

#import <CoreFoundation/CoreFoundation.h>

#include "nscore.h"
#include "nsRemoteClient.h"

class nsMacRemoteClient : public nsRemoteClient {
 public:
  virtual ~nsMacRemoteClient() = default;

  nsresult Init() override;

  nsresult SendCommandLine(const char* aProgram, const char* aProfile,
                           int32_t argc, char** argv,
                           const char* aDesktopStartupID, char** aResponse,
                           bool* aSucceeded) override;
};

#endif  // TOOLKIT_COMPONENTS_REMOTE_NSMACREMOTECLIENT_H_
