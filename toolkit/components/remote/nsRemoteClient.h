/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TOOLKIT_COMPONENTS_REMOTE_NSREMOTECLIENT_H_
#define TOOLKIT_COMPONENTS_REMOTE_NSREMOTECLIENT_H_

#include "nscore.h"

/**
 * Pure-virtual common base class for remoting implementations.
 */

class nsRemoteClient {
 public:
  virtual ~nsRemoteClient() = default;

  /**
   * Initializes the client
   */
  virtual nsresult Init() = 0;

  /**
   * Send a complete command line to a running instance.
   *
   * @param aProgram This is the preferred program that we want to use
   * for this particular command.
   *
   * @param aUsername This allows someone to only talk to an instance
   * of the server that's running under a particular username.  If
   * this isn't specified here it's pulled from the LOGNAME
   * environmental variable if it's set.
   *
   * @param aProfile This allows you to specify a particular server
   * running under a named profile.  If it is not specified the
   * profile is not checked.
   *
   * @param argc The number of command-line arguments.
   *
   * @param argv The command-line arguments.
   *
   * @param aDesktopStartupID the contents of the DESKTOP_STARTUP_ID environment
   * variable defined by the Startup Notification specification
   * http://standards.freedesktop.org/startup-notification-spec/startup-notification-0.1.txt
   *
   * @param aResponse If there is a response, it will be here.  This
   * includes error messages.  The string is allocated using stdlib
   * string functions, so free it with free().
   *
   * @return true if succeeded, false if no running instance was found.
   *
   */
  virtual nsresult SendCommandLine(const char* aProgram, const char* aProfile,
                                   int32_t argc, char** argv,
                                   const char* aDesktopStartupID,
                                   char** aResponse, bool* aSucceeded) = 0;
};

#endif  // TOOLKIT_COMPONENTS_REMOTE_NSREMOTECLIENT_H_
