/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSMAEMONETWORKMANAGER_H_
#define NSMAEMONETWORKMANAGER_H_

#include "nscore.h"

class nsMaemoNetworkManager
{
public:
  // Can be called from any thread, most likely the socket transport thread
  static bool OpenConnectionSync();
  static void CloseConnection();

  static bool IsConnected();
  static bool GetLinkStatusKnown();

  // Called from the nsMaemoNetworkLinkService (main thread only)
  static bool Startup();
  static void Shutdown();
};

#endif /* NSMAEMONETWORKMANAGER_H_ */
