/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     Conrad Carlen conrad@ingress.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#ifndef nsDirectoryServiceDefs_h___
#define nsDirectoryServiceDefs_h___

//========================================================================================
//
// Defines the property names for directories available from nsIDirectoryService. These
// dirs are always available even if no nsIDirectoryServiceProviders have been registered
// with the service. Application level keys are defined in nsAppDirectoryServiceDefs.h.
//
//========================================================================================



#define NS_XPCOM_INIT_CURRENT_PROCESS_DIR       "MozBinD"   // Can be used to set NS_XPCOM_CURRENT_PROCESS_DIR
                                                            // CANNOT be used to GET a location
                                                                                                                       
#define NS_XPCOM_CURRENT_PROCESS_DIR            "XCurProcD"
#define NS_XPCOM_COMPONENT_REGISTRY_FILE        "ComRegF"
#define NS_XPCOM_COMPONENT_DIR                  "ComsD"

#define NS_OS_HOME_DIR                          "Home"
#define NS_OS_DRIVE_DIR                         "DrvD"
#define NS_OS_TEMP_DIR                          "TmpD"
#define NS_OS_CURRENT_PROCESS_DIR               "CurProcD"
#define NS_OS_CURRENT_WORKING_DIR               "CurWorkD"

#if !defined (XP_UNIX)
    #define NS_OS_SYSTEM_DIR                    "SysD"
#endif

#ifdef XP_MAC
    #define NS_MAC_DESKTOP_DIR                  "Desk"
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
    #define NS_MAC_HOME_DIR                     "Home"
    #define NS_MAC_DEFAULT_DOWNLOAD_DIR         "DfltDwnld"
    #define NS_MAC_USER_LIB_DIR                 "ULibDir"   // Only available under OS X
#elif defined (XP_WIN)
    #define NS_WIN_WINDOWS_DIR                  "WinD"
    #define NS_WIN_HOME_DIR                     "Home"
    #define NS_WIN_DESKTOP_DIR                  "DeskV" // virtual folder at the root of the namespace
    #define NS_WIN_PROGRAMS_DIR                 "Progs"
    #define NS_WIN_CONTROLS_DIR                 "Cntls"
    #define NS_WIN_PRINTERS_DIR                 "Prnts"
    #define NS_WIN_PERSONAL_DIR                 "Pers"
    #define NS_WIN_FAVORITES_DIR                "Favs"
    #define NS_WIN_STARTUP_DIR                  "Strt"
    #define NS_WIN_RECENT_DIR                   "Rcnt"
    #define NS_WIN_SEND_TO_DIR                  "SndTo"
    #define NS_WIN_BITBUCKET_DIR                "Buckt"
    #define NS_WIN_STARTMENU_DIR                "Strt"
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
    #define NS_WIN_APPDATA_DIR                  "AppData"
    #define NS_WIN_PRINTHOOD                    "PrntHd" 
#elif defined (XP_UNIX)
    #define NS_UNIX_LOCAL_DIR                   "Locl"
    #define NS_UNIX_LIB_DIR                     "LibD"
    #define NS_UNIX_HOME_DIR                    "Home" 
#elif defined (XP_OS2)
    #define NS_OS2_DIR                          "OS2Dir"
    #define NS_OS2_HOME_DIR                     "Home"
    #define NS_OS2_DESKTOP_DIR                  "Desk"    
#elif defined (XP_BEOS)
    #define NS_BEOS_SETTINGS_DIR                "Setngs"
    #define NS_BEOS_HOME_DIR                    "Home"
    #define NS_BEOS_DESKTOP_DIR                 "Desk"
#endif


#endif
