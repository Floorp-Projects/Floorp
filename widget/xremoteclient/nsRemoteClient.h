/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRemoteClient_h__
#define nsRemoteClient_h__

#include "nscore.h"

/**
 * Pure-virtual common base class for remoting implementations.
 */

class nsRemoteClient
{
public:
  /**
   * Initializes the client
   */
  virtual nsresult Init() = 0;

  /**
   * Sends a command to a running instance.
   *
   * @param aProgram This is the preferred program that we want to use
   * for this particular command.
   *
   * @param aNoProgramFallback This boolean attribute tells the client
   * code that if the preferred program isn't found that it should
   * fail not send the command to another server.
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
   * @param aCommand This is the command that is passed to the server.
   * Please see the additional information located at:
   * http://www.mozilla.org/unix/remote.html
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
   */
  virtual nsresult SendCommand(const char *aProgram, const char *aUsername,
                               const char *aProfile, const char *aCommand,
                               const char* aDesktopStartupID,
                               char **aResponse, bool *aSucceeded) = 0;

  /**
   * Send a complete command line to a running instance.
   *
   * @param aDesktopStartupID the contents of the DESKTOP_STARTUP_ID environment
   * variable defined by the Startup Notification specification
   * http://standards.freedesktop.org/startup-notification-spec/startup-notification-0.1.txt
   *
   * @see sendCommand
   * @param argc The number of command-line arguments.
   * 
   */
  virtual nsresult SendCommandLine(const char *aProgram, const char *aUsername,
                                   const char *aProfile,
                                   PRInt32 argc, char **argv,
                                   const char* aDesktopStartupID,
                                   char **aResponse, bool *aSucceeded) = 0;
};

#endif // nsRemoteClient_h__
