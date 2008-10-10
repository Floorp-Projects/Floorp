/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 * The Original Code is Widget code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net> (original author)
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

static nsAppShell *sAppShell;

static nsresult
nsAppShellInit(nsIModule *module)
{
  NS_ASSERTION(!sAppShell, "already initialized");

  sAppShell = new nsAppShell();
  if (!sAppShell)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(sAppShell);

  nsresult rv = sAppShell->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(sAppShell);
    return rv;
  }

  return NS_OK;
}

static void
nsAppShellShutdown(nsIModule *module)
{
  NS_RELEASE(sAppShell);
}

static NS_METHOD
nsAppShellConstructor(nsISupports *outer, const nsIID &iid, void **result)
{
  NS_ENSURE_TRUE(!outer, NS_ERROR_NO_AGGREGATION);
  NS_ENSURE_TRUE(sAppShell, NS_ERROR_NOT_INITIALIZED);

  return sAppShell->QueryInterface(iid, result);
}

#endif  // nsAppShellSingleton_h__
