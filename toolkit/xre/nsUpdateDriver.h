/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUpdateDriver_h__
#define nsUpdateDriver_h__

#include "nscore.h"

class nsIFile;

/**
 * This function processes any available updates.  As part of that process, it
 * may exit the current process and relaunch it at a later time.
 *
 * Two directories are passed to this function: greDir (where the actual
 * binary resides) and appDir (which contains application.ini for XULRunner
 * apps). If this is not a XULRunner app then appDir is identical to greDir.
 * 
 * The argc and argv passed to this function should be what is needed to
 * relaunch the current process.
 *
 * The appVersion param passed to this function is the current application's
 * version and is used to determine if an update's version is older than the
 * current application version.
 *
 * This function does not modify appDir.
 */
NS_HIDDEN_(nsresult) ProcessUpdates(nsIFile *greDir, nsIFile *appDir,
                                    nsIFile *updRootDir,
                                    int argc, char **argv,
                                    const char *appVersion);

#endif  // nsUpdateDriver_h__
