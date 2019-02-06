/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWinRemoteClient.h"

nsresult nsWinRemoteClient::Init() {
  return NS_OK;
}

nsresult nsWinRemoteClient::SendCommandLine(const char *aProgram, const char *aProfile,
                                            int32_t argc, char **argv,
                                            const char *aDesktopStartupID,
                                            char **aResponse, bool *aSucceeded) {
  *aSucceeded = false;
  return NS_OK;
}
