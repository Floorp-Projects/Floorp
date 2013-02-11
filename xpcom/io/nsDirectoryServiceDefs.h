/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#define NS_OS_HOME_DIR                          "Home"
#define NS_OS_TEMP_DIR                          "TmpD"
#define NS_OS_CURRENT_WORKING_DIR               "CurWorkD"
/* Files stored in this directory will appear on the user's desktop,
 * if there is one, otherwise it's just the same as "Home"
 */
#define NS_OS_DESKTOP_DIR                       "Desk"

/* Property returns the directory in which the procces was started from.  
 * On Unix this will be the path in the MOZILLA_FIVE_HOME env var and if 
 * unset will be the current working directory. 
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

/* Property will return the current location of the the GRE directory.  
 * If no GRE is used, this propery will behave like 
 * NS_XPCOM_CURRENT_PROCESS_DIR.
 */
#define NS_GRE_DIR                              "GreD"

/* Platform Specific Locations */

#if !defined (XP_UNIX) || defined(MOZ_WIDGET_COCOA)
    #define NS_OS_SYSTEM_DIR                    "SysD"
#endif

#if defined (MOZ_WIDGET_COCOA)
    #define NS_MAC_DESKTOP_DIR                  NS_OS_DESKTOP_DIR
    #define NS_MAC_TRASH_DIR                    "Trsh"
    #define NS_MAC_STARTUP_DIR                  "Strt"
    #define NS_MAC_SHUTDOWN_DIR                 "Shdwn"
    #define NS_MAC_APPLE_MENU_DIR               "ApplMenu"
    #define NS_MAC_CONTROL_PANELS_DIR           "CntlPnl"
    #define NS_MAC_EXTENSIONS_DIR               "Exts"
    #define NS_MAC_FONTS_DIR                    "Fnts"
    #define NS_MAC_PREFS_DIR                    "Prfs"
    #define NS_MAC_DOCUMENTS_DIR                "Docs"
    #define NS_MAC_INTERNET_SEARCH_DIR          "ISrch"
    #define NS_OSX_HOME_DIR                     NS_OS_HOME_DIR
    #define NS_MAC_HOME_DIR                     NS_OS_HOME_DIR
    #define NS_MAC_DEFAULT_DOWNLOAD_DIR         "DfltDwnld"
    #define NS_MAC_USER_LIB_DIR                 "ULibDir"   // Only available under OS X
    #define NS_OSX_DEFAULT_DOWNLOAD_DIR         NS_MAC_DEFAULT_DOWNLOAD_DIR
    #define NS_OSX_USER_DESKTOP_DIR             "UsrDsk"
    #define NS_OSX_LOCAL_DESKTOP_DIR            "LocDsk"
    #define NS_OSX_USER_APPLICATIONS_DIR        "UsrApp"
    #define NS_OSX_LOCAL_APPLICATIONS_DIR       "LocApp"
    #define NS_OSX_USER_DOCUMENTS_DIR           "UsrDocs"
    #define NS_OSX_LOCAL_DOCUMENTS_DIR          "LocDocs"
    #define NS_OSX_USER_INTERNET_PLUGIN_DIR     "UsrIntrntPlgn"
    #define NS_OSX_LOCAL_INTERNET_PLUGIN_DIR    "LoclIntrntPlgn"
    #define NS_OSX_USER_FRAMEWORKS_DIR          "UsrFrmwrks"
    #define NS_OSX_LOCAL_FRAMEWORKS_DIR         "LocFrmwrks"
    #define NS_OSX_USER_PREFERENCES_DIR         "UsrPrfs"
    #define NS_OSX_LOCAL_PREFERENCES_DIR        "LocPrfs"
    #define NS_OSX_PICTURE_DOCUMENTS_DIR        "Pct"
    #define NS_OSX_MOVIE_DOCUMENTS_DIR          "Mov"
    #define NS_OSX_MUSIC_DOCUMENTS_DIR          "Music"
    #define NS_OSX_INTERNET_SITES_DIR           "IntrntSts"
#elif defined (XP_WIN)
    #define NS_WIN_WINDOWS_DIR                  "WinD"
    #define NS_WIN_PROGRAM_FILES_DIR            "ProgF"
    #define NS_WIN_HOME_DIR                     NS_OS_HOME_DIR
    #define NS_WIN_DESKTOP_DIR                  "DeskV" // virtual folder at the root of the namespace
    #define NS_WIN_PROGRAMS_DIR                 "Progs" // User start menu programs directory!
    #define NS_WIN_CONTROLS_DIR                 "Cntls"
    #define NS_WIN_PRINTERS_DIR                 "Prnts"
    #define NS_WIN_PERSONAL_DIR                 "Pers"
    #define NS_WIN_FAVORITES_DIR                "Favs"
    #define NS_WIN_STARTUP_DIR                  "Strt"
    #define NS_WIN_RECENT_DIR                   "Rcnt"
    #define NS_WIN_SEND_TO_DIR                  "SndTo"
    #define NS_WIN_BITBUCKET_DIR                "Buckt"
    #define NS_WIN_STARTMENU_DIR                "Strt"
// This gives the same thing as NS_OS_DESKTOP_DIR
    #define NS_WIN_DESKTOP_DIRECTORY            "DeskP" // file sys dir which physically stores objects on desktop
    #define NS_WIN_DRIVES_DIR                   "Drivs"
    #define NS_WIN_NETWORK_DIR                  "NetW"
    #define NS_WIN_NETHOOD_DIR                  "netH"
    #define NS_WIN_FONTS_DIR                    "Fnts"
    #define NS_WIN_TEMPLATES_DIR                "Tmpls"
    #define NS_WIN_COMMON_STARTMENU_DIR         "CmStrt"
    #define NS_WIN_COMMON_PROGRAMS_DIR          "CmPrgs"
    #define NS_WIN_COMMON_STARTUP_DIR           "CmStrt"
    #define NS_WIN_COMMON_DESKTOP_DIRECTORY     "CmDeskP"
    #define NS_WIN_COMMON_APPDATA_DIR           "CmAppData"
    #define NS_WIN_APPDATA_DIR                  "AppData"
    #define NS_WIN_LOCAL_APPDATA_DIR            "LocalAppData"
    #define NS_WIN_PRINTHOOD                    "PrntHd"
    #define NS_WIN_COOKIES_DIR                  "CookD"
    #define NS_WIN_DEFAULT_DOWNLOAD_DIR         "DfltDwnld"
    // On Win7 and up these ids will return the default save-to location for
    // Windows Libraries associated with the specific content type. For other
    // os they return the local user folder. Note these can return network file
    // paths which can jank the ui thread so be careful how you access them.
    #define NS_WIN_DOCUMENTS_DIR                "Docs"
    #define NS_WIN_PICTURES_DIR                 "Pict"
    #define NS_WIN_MUSIC_DIR                    "Music"
    #define NS_WIN_VIDEOS_DIR                   "Vids"
#elif defined (XP_UNIX)
    #define NS_UNIX_LOCAL_DIR                   "Locl"
    #define NS_UNIX_LIB_DIR                     "LibD"
    #define NS_UNIX_HOME_DIR                    NS_OS_HOME_DIR
    #define NS_UNIX_XDG_DESKTOP_DIR             "XDGDesk"
    #define NS_UNIX_XDG_DOCUMENTS_DIR           "XDGDocs"
    #define NS_UNIX_XDG_DOWNLOAD_DIR            "XDGDwnld"
    #define NS_UNIX_XDG_MUSIC_DIR               "XDGMusic"
    #define NS_UNIX_XDG_PICTURES_DIR            "XDGPict"
    #define NS_UNIX_XDG_PUBLIC_SHARE_DIR        "XDGPubSh"
    #define NS_UNIX_XDG_TEMPLATES_DIR           "XDGTempl"
    #define NS_UNIX_XDG_VIDEOS_DIR              "XDGVids"
    #define NS_UNIX_DEFAULT_DOWNLOAD_DIR        "DfltDwnld"
#elif defined (XP_OS2)
    #define NS_OS2_DIR                          "OS2Dir"
    #define NS_OS2_HOME_DIR                     NS_OS_HOME_DIR
    #define NS_OS2_DESKTOP_DIR                  NS_OS_DESKTOP_DIR
#endif

/* Deprecated */

#define NS_OS_DRIVE_DIR                         "DrvD"



#endif
