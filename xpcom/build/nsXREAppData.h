/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXREAppData_h
#define nsXREAppData_h

#include "mozilla/StandardInteger.h"

class nsIFile;

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
  nsIFile* directory;

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
  nsIFile* xreDirectory;

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

  /**
   * The application name to use in the User Agent string.
   */
  const char *UAName;
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
