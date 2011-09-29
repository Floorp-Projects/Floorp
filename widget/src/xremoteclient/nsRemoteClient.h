/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
