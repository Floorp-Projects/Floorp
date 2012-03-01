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
 *  Benjamin Smedberg <benjamin@smedbergs.us>
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

#ifndef nsXREAppData_h
#define nsXREAppData_h

#include "mozilla/StandardInteger.h"

class nsILocalFile;

/**
 * Application-specific data needed to start the apprunner.
 *
 * @note When this structure is allocated and manipulated by XRE_CreateAppData,
 *       string fields will be allocated with NS_Alloc, and interface pointers
 *       are strong references.
 */
struct nsXREAppData
{
  /**
   * This should be set to sizeof(nsXREAppData). This structure may be
   * extended in future releases, and this ensures that binary compatibility
   * is maintained.
   */
  uint32_t size;

  /**
   * The directory of the application to be run. May be null if the
   * xulrunner and the app are installed into the same directory.
   */
  nsILocalFile* directory;

  /**
   * The name of the application vendor. This must be ASCII, and is normally
   * mixed-case, e.g. "Mozilla". Optional (may be null), but highly
   * recommended. Must not be the empty string.
   */
  const char *vendor;

  /**
   * The name of the application. This must be ASCII, and is normally
   * mixed-case, e.g. "Firefox". Required (must not be null or an empty
   * string).
   */
  const char *name;

  /**
   * The major version, e.g. "0.8.0+". Optional (may be null), but
   * required for advanced application features such as the extension
   * manager and update service. Must not be the empty string.
   */
  const char *version;

  /**
   * The application's build identifier, e.g. "2004051604"
   */
  const char *buildID;

  /**
   * The application's UUID. Used by the extension manager to determine
   * compatible extensions. Optional, but required for advanced application
   * features such as the extension manager and update service.
   *
   * This has traditionally been in the form
   * "{AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE}" but for new applications
   * a more readable form is encouraged: "appname@vendor.tld". Only
   * the following characters are allowed: a-z A-Z 0-9 - . @ _ { } *
   */
  const char *ID;

  /**
   * The copyright information to print for the -h commandline flag,
   * e.g. "Copyright (c) 2003 mozilla.org".
   */
  const char *copyright;

  /**
   * Combination of NS_XRE_ prefixed flags (defined below).
   */
  uint32_t flags;

  /**
   * The location of the XRE. XRE_main may not be able to figure this out
   * programatically.
   */
  nsILocalFile* xreDirectory;

  /**
   * The minimum/maximum compatible XRE version.
   */
  const char *minVersion;
  const char *maxVersion;

  /**
   * The server URL to send crash reports to.
   */
  const char *crashReporterURL;

  /**
   * The profile directory that will be used. Optional (may be null). Must not
   * be the empty string, must be ASCII. The path is split into components
   * along the path separator characters '/' and '\'.
   *
   * The application data directory ("UAppData", see below) is normally
   * composed as follows, where $HOME is platform-specific:
   *
   *   UAppData = $HOME[/$vendor]/$name
   *
   * If present, the 'profile' string will be used instead of the combination of
   * vendor and name as follows:
   *
   *   UAppData = $HOME/$profile
   */
  const char *profile;
};

/**
 * Indicates whether or not the profile migrator service may be
 * invoked at startup when creating a profile.
 */
#define NS_XRE_ENABLE_PROFILE_MIGRATOR (1 << 1)

/**
 * Indicates whether or not the extension manager service should be
 * initialized at startup.
 */
#define NS_XRE_ENABLE_EXTENSION_MANAGER (1 << 2)

/**
 * Indicates whether or not to use Breakpad crash reporting.
 */
#define NS_XRE_ENABLE_CRASH_REPORTER (1 << 3)

#endif // nsXREAppData_h
