/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWinRemoteServer.h"

nsresult nsWinRemoteServer::Startup(const char* aAppName,
    const char* aProfileName) {
  return NS_OK;
}

void nsWinRemoteServer::Shutdown() {
}
