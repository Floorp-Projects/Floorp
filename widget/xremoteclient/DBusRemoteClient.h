/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 #ifndef DBusRemoteClient_h__
 #define DBusRemoteClient_h__

#include "nsRemoteClient.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/DBusHelpers.h"

class DBusRemoteClient : public nsRemoteClient
{
public:
  DBusRemoteClient();
  ~DBusRemoteClient();

  virtual nsresult Init() override;
  virtual nsresult SendCommandLine(const char *aProgram, const char *aUsername,
                                   const char *aProfile,
                                   int32_t argc, char **argv,
                                   const char* aDesktopStartupID,
                                   char **aResponse, bool *aSucceeded) override;
  void Shutdown();

private:
  nsresult         DoSendDBusCommandLine(const char *aProgram, const char *aProfile,
                                         const char* aBuffer, int aLength);
  RefPtr<DBusConnection> mConnection;
};

#endif // DBusRemoteClient_h__
