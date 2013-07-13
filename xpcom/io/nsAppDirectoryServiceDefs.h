/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#ifndef nsAppDirectoryServiceDefs_h___
#define nsAppDirectoryServiceDefs_h___

//========================================================================================
//
// Defines property names for directories available from standard nsIDirectoryServiceProviders.
// These keys are not guaranteed to exist because the nsIDirectoryServiceProviders which
// provide them are optional.
//
// Keys whose definition ends in "DIR" or "FILE" return a single nsIFile (or subclass).
// Keys whose definition ends in "LIST" return an nsISimpleEnumerator which enumerates a
// list of file objects.
//
// System and XPCOM level properties are defined in nsDirectoryServiceDefs.h.
//
//========================================================================================


// --------------------------------------------------------------------------------------
// Files and directories which exist on a per-product basis
// --------------------------------------------------------------------------------------

#define NS_APP_APPLICATION_REGISTRY_FILE        "AppRegF"
#define NS_APP_APPLICATION_REGISTRY_DIR         "AppRegD"

#define NS_APP_DEFAULTS_50_DIR                  "DefRt"         // The root dir of all defaults dirs
#define NS_APP_PREF_DEFAULTS_50_DIR             "PrfDef"
#define NS_APP_PROFILE_DEFAULTS_50_DIR          "profDef"       // The profile defaults of the "current"
                                                                // locale. Should be first choice.
#define NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR     "ProfDefNoLoc"  // The profile defaults of the "default"
                                                                // installed locale. Second choice
                                                                // when above is not available.
                                                                                                                       
#define NS_APP_USER_PROFILES_ROOT_DIR           "DefProfRt"     // The dir where user profile dirs live.
#define NS_APP_USER_PROFILES_LOCAL_ROOT_DIR     "DefProfLRt"  // The dir where user profile temp dirs live.

#define NS_APP_RES_DIR                          "ARes"
#define NS_APP_CHROME_DIR                       "AChrom"
#define NS_APP_PLUGINS_DIR                      "APlugns"       // Deprecated - use NS_APP_PLUGINS_DIR_LIST
#define NS_APP_SEARCH_DIR                       "SrchPlugns"

#define NS_APP_CHROME_DIR_LIST                  "AChromDL"
#define NS_APP_PLUGINS_DIR_LIST                 "APluginsDL"
#define NS_APP_SEARCH_DIR_LIST                  "SrchPluginsDL"

// --------------------------------------------------------------------------------------
// Files and directories which exist on a per-profile basis
// These locations are typically provided by the profile mgr
// --------------------------------------------------------------------------------------

// In a shared profile environment, prefixing a profile-relative
// key with NS_SHARED returns a location that is shared by
// other users of the profile. Without this prefix, the consumer
// has exclusive access to this location.
 
#define NS_SHARED                               "SHARED"

#define NS_APP_PREFS_50_DIR                     "PrefD"         // Directory which contains user prefs       
#define NS_APP_PREFS_50_FILE                    "PrefF"
#define NS_APP_PREFS_DEFAULTS_DIR_LIST          "PrefDL"
#define NS_EXT_PREFS_DEFAULTS_DIR_LIST          "ExtPrefDL"
#define NS_APP_PREFS_OVERRIDE_DIR               "PrefDOverride" // Directory for per-profile defaults
        
#define NS_APP_USER_PROFILE_50_DIR              "ProfD"
#define NS_APP_USER_PROFILE_LOCAL_50_DIR        "ProfLD"
        
#define NS_APP_USER_CHROME_DIR                  "UChrm"
#define NS_APP_USER_SEARCH_DIR                  "UsrSrchPlugns"
         
#define NS_APP_LOCALSTORE_50_FILE               "LclSt"
#define NS_APP_USER_PANELS_50_FILE              "UPnls"
#define NS_APP_USER_MIMETYPES_50_FILE           "UMimTyp"
#define NS_APP_CACHE_PARENT_DIR                 "cachePDir"

#define NS_APP_BOOKMARKS_50_FILE                "BMarks"

#define NS_APP_DOWNLOADS_50_FILE                "DLoads"
         
#define NS_APP_SEARCH_50_FILE                   "SrchF"
         
#define NS_APP_INSTALL_CLEANUP_DIR              "XPIClnupD"  //location of xpicleanup.dat xpicleanup.exe 

#define NS_APP_INDEXEDDB_PARENT_DIR             "indexedDBPDir"

#define NS_APP_PERMISSION_PARENT_DIR            "permissionDBPDir"
#endif
