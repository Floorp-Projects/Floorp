/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSXREMOTESERVICE_H
#define NSXREMOTESERVICE_H

#include "nsString.h"

#include <X11/Xlib.h>
#include <X11/X.h>

class nsIDOMWindow;
class nsIWeakReference;

#ifdef IS_BIG_ENDIAN
#define TO_LITTLE_ENDIAN32(x) \
    ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) | \
    (((x) & 0x0000ff00) << 8) | (((x) & 0x000000ff) << 24))
#else
#define TO_LITTLE_ENDIAN32(x) (x)
#endif

/**
  Base class for GTK/Qt remote service
*/
class nsXRemoteService
{
protected:
    nsXRemoteService();
    static bool HandleNewProperty(Window aWindowId,Display* aDisplay,
                                  Time aEventTime, Atom aChangedAtom,
                                  nsIWeakReference* aDomWindow);
    void XRemoteBaseStartup(const char *aAppName, const char *aProfileName);
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

#endif // NSXREMOTESERVICE_H
