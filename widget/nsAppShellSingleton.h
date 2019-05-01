/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppShellSingleton_h__
#define nsAppShellSingleton_h__

/**
 * This file is designed to be included into the file that provides the
 * nsIModule implementation for a particular widget toolkit.
 *
 * The following functions are defined:
 *   nsAppShellInit
 *   nsAppShellShutdown
 *   nsAppShellConstructor
 *
 * The nsAppShellInit function is designed to be used as a module constructor.
 * If you already have a module constructor, then call nsAppShellInit from your
 * module constructor.
 *
 * The nsAppShellShutdown function is designed to be used as a module
 * destructor.  If you already have a module destructor, then call
 * nsAppShellShutdown from your module destructor.
 *
 * The nsAppShellConstructor function is designed to be used as a factory
 * method for the nsAppShell class.
 */

#include "nsXULAppAPI.h"

static nsIAppShell* sAppShell;

static nsresult nsAppShellInit() {
  NS_ASSERTION(!sAppShell, "already initialized");

  sAppShell = new nsAppShell();
  if (!sAppShell) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(sAppShell);

  nsresult rv;
  rv = static_cast<nsAppShell*>(sAppShell)->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(sAppShell);
    return rv;
  }

  return NS_OK;
}

static void nsAppShellShutdown() { NS_RELEASE(sAppShell); }

nsresult nsAppShellConstructor(nsISupports* outer, const nsIID& iid,
                               void** result) {
  NS_ENSURE_TRUE(!outer, NS_ERROR_NO_AGGREGATION);
  NS_ENSURE_TRUE(sAppShell, NS_ERROR_NOT_INITIALIZED);

  return sAppShell->QueryInterface(iid, result);
}

#endif  // nsAppShellSingleton_h__
