/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *  Benjamin Smedberg <bsmedberg@covad.net>
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

#ifndef _nsXULAppAPI_h__
#define _nsXULAppAPI_h__

#include "prtypes.h"
class nsILocalFile;

/**
 * Application-specific data needed to start the apprunner.
 */

struct nsXREAppData
{
  /**
   * The name of the application vendor. This must be ASCII, and is normally
   * mixed-case, e.g. "Mozilla".
   */
  const char *appVendor;

  /**
   * The name of the application. This must be ASCII, and is normally
   * mixed-case, e.g. "Firefox".
   */
  const char *appName;

  /**
   * The major version, e.g. "0.8.0+"
   */
  const char *appVersion;

  /** 
   * The application's build identifier, e.g. "2004051604"
   */
  const char *appBuildID;

  /**
   * The copyright information to print for the -h commandline flag,
   * e.g. "Copyright (c) 2003 mozilla.org".
   */
  const char *copyright;

  PRBool useStartupPrefs; // XXXbsmedberg this is going away
};

/**
 * Begin an XUL application. Does not return until the user exits the
 * application.
 * @param aAppData Information about the application being run.
 * @return         A native result code suitable for returning from main().
 *
 * @note           If the binary is linked against the  standalone XPCOM glue,
 *                 XPCOMGlueStartup() should be called before this method.
 *
 * @note           XXXbsmedberg Nobody uses the glue yet, but there is a
 *                 potentital problem: on windows, the glue calls
 *                 SetCurrentDirectory, and relative paths on the command line
 *                 won't be correct.
 */

int xre_main(int argc, char* argv[], const nsXREAppData* aAppData);

#endif // _nsXULAppAPI_h__
