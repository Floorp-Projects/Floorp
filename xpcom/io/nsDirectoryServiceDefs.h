/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Defines the property names for directories available from
 * nsIDirectoryService. These dirs are always available even if no
 * nsIDirectoryServiceProviders have been registered with the service.
 * Application level keys are defined in nsAppDirectoryServiceDefs.h.
 *
 * Keys whose definition ends in "DIR" or "FILE" return a single nsIFile (or
 * subclass). Keys whose definition ends in "LIST" return an nsISimpleEnumerator
 * which enumerates a list of file objects.
 *
 * Defines listed in this file are FROZEN.  This list may grow.
 */

#ifndef nsDirectoryServiceDefs_h___
#define nsDirectoryServiceDefs_h___

/* General OS specific locations */

// If this value ever changes, the special handling of the
// nsDirectoryService::sOS_HomeDirectory static atom -- i.e. the use of
// nsGkAtoms::Home in its place -- must be removed.
#define NS_OS_HOME_DIR                          "Home"

#define NS_OS_TEMP_DIR                          "TmpD"
#define NS_OS_CURRENT_WORKING_DIR               "CurWorkD"
/* Files stored in this directory will appear on the user's desktop,
 * if there is one, otherwise it's just the same as "Home"
 */
#define NS_OS_DESKTOP_DIR                       "Desk"

/* Property returns the directory in which the procces was started from.
 */
#define NS_OS_CURRENT_PROCESS_DIR               "CurProcD"

/* This location is similar to NS_OS_CURRENT_PROCESS_DIR, however,
 * NS_XPCOM_CURRENT_PROCESS_DIR can be overriden by passing a "bin
 * directory" to NS_InitXPCOM2().
 */
#define NS_XPCOM_CURRENT_PROCESS_DIR            "XCurProcD"

/* Property will return the location of the the XPCOM Shared Library.
 */
#define NS_XPCOM_LIBRARY_FILE                   "XpcomLib"

/* Property will return the current location of the GRE directory.
 * On OSX, this typically points to Contents/Resources in the app bundle.
 * If no GRE is used, this propery will behave like
 * NS_XPCOM_CURRENT_PROCESS_DIR.
 */
#define NS_GRE_DIR                              "GreD"

/* Property will return the current location of the GRE-binaries directory.
 * On OSX, this typically points to Contents/MacOS in the app bundle. On
 * all other platforms, this will be identical to NS_GRE_DIR.
 * Since this property is based on the NS_GRE_DIR, if no GRE is used, this
 * propery will behave like NS_XPCOM_CURRENT_PROCESS_DIR.
 */
#define NS_GRE_BIN_DIR                          "GreBinD"

/* Platform Specific Locations */

#if !defined (XP_UNIX) || defined(MOZ_WIDGET_COCOA)
    #define NS_OS_SYSTEM_DIR                    "SysD"
#endif

#if defined (MOZ_WIDGET_COCOA)
  #define NS_MAC_USER_LIB_DIR                 "ULibDir"
  #define NS_OSX_DEFAULT_DOWNLOAD_DIR         "DfltDwnld"
  #define NS_OSX_LOCAL_APPLICATIONS_DIR       "LocApp"
  #define NS_OSX_USER_PREFERENCES_DIR         "UsrPrfs"
  #define NS_OSX_PICTURE_DOCUMENTS_DIR        "Pct"
#elif defined (XP_WIN)
  #define NS_WIN_WINDOWS_DIR                  "WinD"
  #define NS_WIN_PROGRAM_FILES_DIR            "ProgF"
  #define NS_WIN_HOME_DIR                     NS_OS_HOME_DIR
  #define NS_WIN_PROGRAMS_DIR                 "Progs" // User start menu programs directory!
  #define NS_WIN_FAVORITES_DIR                "Favs"
  #define NS_WIN_APPDATA_DIR                  "AppData"
  #define NS_WIN_LOCAL_APPDATA_DIR            "LocalAppData"
#if defined(MOZ_CONTENT_SANDBOX)
  #define NS_WIN_LOCAL_APPDATA_LOW_DIR        "LocalAppDataLow"
  #define NS_WIN_LOW_INTEGRITY_TEMP_BASE      "LowTmpDBase"
#endif
  #define NS_WIN_COOKIES_DIR                  "CookD"
  #define NS_WIN_DEFAULT_DOWNLOAD_DIR         "DfltDwnld"
#elif defined (XP_UNIX)
  #define NS_UNIX_HOME_DIR                    NS_OS_HOME_DIR
  #define NS_UNIX_DEFAULT_DOWNLOAD_DIR        "DfltDwnld"
#endif

#endif
