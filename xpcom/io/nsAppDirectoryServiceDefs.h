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
 
#ifndef nsAppDirectoryServiceDefs_h___
#define nsAppDirectoryServiceDefs_h___

//========================================================================================
//
// Defines property names for directories available from standard nsIDirectoryServiceProviders.
// These keys are not guaranteed to exist because the nsIDirectoryServiceProviders which
// provide them are optional.
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
#define NS_APP_PLUGINS_DIR                      "APlugns"
#define NS_APP_SEARCH_DIR                       "SrchPlugns"

// --------------------------------------------------------------------------------------
// Files and directories which exist on a per-profile basis
// These locations are typically provided by the profile mgr
// --------------------------------------------------------------------------------------

#define NS_APP_PREFS_50_DIR                     "PrefD"         // Directory which contains user prefs       
#define NS_APP_PREFS_50_FILE                    "PrefF"
        
#define NS_APP_USER_PROFILE_50_DIR              "ProfD"
        
#define NS_APP_USER_CHROME_DIR                  "UChrm"
         
#define NS_APP_LOCALSTORE_50_FILE               "LclSt"
#define NS_APP_HISTORY_50_FILE                  "UHist"
#define NS_APP_USER_PANELS_50_FILE              "UPnls"
#define NS_APP_USER_MIMETYPES_50_FILE           "UMimTyp"

#define NS_APP_BOOKMARKS_50_FILE                "BMarks"
         
#define NS_APP_SEARCH_50_FILE                   "SrchF"
         
#define NS_APP_MAIL_50_DIR                      "MailD"
#define NS_APP_IMAP_MAIL_50_DIR                 "IMapMD"
#define NS_APP_NEWS_50_DIR                      "NewsD"
#define NS_APP_MESSENGER_FOLDER_CACHE_50_DIR    "MFCaD"

#endif
