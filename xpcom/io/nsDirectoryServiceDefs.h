/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Conrad Carlen conrad@ingress.com
 */
 
#ifndef nsDirectoryServiceDefs_h___
#define nsDirectoryServiceDefs_h___

//========================================================================================
//
// Defines the property names for directories available from nsIDirectoryService. These
// dirs are always available even if no nsIDirectoryServiceProviders have been registered
// with the service. Application level keys are defined in mozilla\xpfe\appfilelocprovider\.
//
//========================================================================================



#define NS_XPCOM_INIT_CURRENT_PROCESS_DIR       "MozBinD"   // Can be used to set NS_XPCOM_CURRENT_PROCESS_DIR
                                                            // CANNOT be used to GET a location
                                                                                                                       
#define NS_XPCOM_CURRENT_PROCESS_DIR            "CurProcD"
#define NS_XPCOM_COMPONENT_REGISTRY_FILE        "ComRegF"
#define NS_XPCOM_COMPONENT_DIR                  "ComsD"
#define NS_XPCOM_APPLICATION_REGISTRY_FILE      "AppRegF"
#define NS_XPCOM_APPLICATION_REGISTRY_DIR       "AppRegD"

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
#elif defined (XP_OS2)
    #define NS_OS2_DIR                          "0S2Dir"
    #define NS_OS2_HOME_DIR                     "Home"
    #define NS_OS2_DESKTOP_DIR                  "Desk"    
#elif defined (XP_PC)
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
#elif defined (XP_BEOS)
    #define NS_BEOS_SETTINGS_DIR                "Setngs"
    #define NS_BEOS_HOME_DIR                    "Home"
    #define NS_BEOS_DESKTOP_DIR                 "Desk"
#endif


#endif
