/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen conrad@ingress.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
                                                                                                                       
#define NS_APP_USER_PROFILES_ROOT_DIR           "DefProfRt"     // The dir where user profile dirs get created.

#define NS_APP_RES_DIR                          "ARes"
#define NS_APP_CHROME_DIR                       "AChrom"
#define NS_APP_PLUGINS_DIR                      "APlugns"       // Deprecated - use NS_APP_PLUGINS_DIR_LIST
#define NS_APP_SEARCH_DIR                       "SrchPlugns"

#define NS_APP_PLUGINS_DIR_LIST                 "APluginsDL"

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
        
#define NS_APP_USER_PROFILE_50_DIR              "ProfD"
        
#define NS_APP_USER_CHROME_DIR                  "UChrm"
         
#define NS_APP_LOCALSTORE_50_FILE               "LclSt"
#define NS_APP_HISTORY_50_FILE                  "UHist"
#define NS_APP_USER_PANELS_50_FILE              "UPnls"
#define NS_APP_USER_MIMETYPES_50_FILE           "UMimTyp"
#define NS_APP_CACHE_PARENT_DIR                 "cachePDir"

#define NS_APP_BOOKMARKS_50_FILE                "BMarks"

#define NS_APP_DOWNLOADS_50_FILE                "DLoads"
         
#define NS_APP_SEARCH_50_FILE                   "SrchF"
         
#define NS_APP_MAIL_50_DIR                      "MailD"
#define NS_APP_IMAP_MAIL_50_DIR                 "IMapMD"
#define NS_APP_NEWS_50_DIR                      "NewsD"
#define NS_APP_MESSENGER_FOLDER_CACHE_50_DIR    "MFCaD"

#define NS_APP_INSTALL_CLEANUP_DIR              "XPIClnupD"  //location of xpicleanup.dat xpicleanup.exe 
#endif
