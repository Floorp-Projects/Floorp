/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DBusRemoteClient_h__
#define DBusRemoteClient_h__

#ifdef MOZ_ENABLE_DBUS
#  include <gio/gio.h>
#  include "mozilla/RefPtr.h"
#  include "mozilla/GRefPtr.h"
#endif
#include "nsRemoteClient.h"
#include "mozilla/DBusHelpers.h"
#include "mozilla/RefPtr.h"
#include "nsStringFwd.h"
#include "nscore.h"

class nsDBusRemoteClient : public nsRemoteClient {
 public:
  nsDBusRemoteClient();
  ~nsDBusRemoteClient();

  nsresult Init() override { return NS_OK; };
  nsresult SendCommandLine(const char* aProgram, const char* aProfile,
                           int32_t argc, char** argv, const char* aStartupToken,
                           char** aResponse, bool* aSucceeded) override;
  void Shutdown();

 private:
  bool GetRemoteDestinationName(const char* aProgram, const char* aProfile,
                                nsCString& aDestinationName);
  nsresult DoSendDBusCommandLine(const char* aProgram, const char* aProfile,
                                 const char* aBuffer, int aLength);
};

#endif  // DBusRemoteClient_h__
