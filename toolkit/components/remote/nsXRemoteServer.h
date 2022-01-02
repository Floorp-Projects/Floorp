/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSXREMOTESERVER_H
#define NSXREMOTESERVER_H

#include "nsString.h"
#include "nsRemoteServer.h"
#include "nsUnixRemoteServer.h"

#include <X11/Xlib.h>
#include <X11/X.h>

/**
  Base class for GTK/Qt remote service
*/
class nsXRemoteServer : public nsRemoteServer, public nsUnixRemoteServer {
 protected:
  nsXRemoteServer();
  bool HandleNewProperty(Window aWindowId, Display* aDisplay, Time aEventTime,
                         Atom aChangedAtom);
  void XRemoteBaseStartup(const char* aAppName, const char* aProfileName);
  void HandleCommandsFor(Window aWindowId);

 private:
  void EnsureAtoms();

  nsCString mAppName;
  nsCString mProfileName;

  static Atom sMozVersionAtom;
  static Atom sMozLockAtom;
  static Atom sMozResponseAtom;
  static Atom sMozUserAtom;
  static Atom sMozProfileAtom;
  static Atom sMozProgramAtom;
  static Atom sMozCommandLineAtom;
};

#endif  // NSXREMOTESERVER_H
