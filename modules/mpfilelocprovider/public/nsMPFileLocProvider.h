/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Conrad Carlen <ccarlen@netscape.com>
 */

// Interfaces Needed
#include "nsIDirectoryService.h"
#include "nsILocalFile.h"
#include "nsString.h"

// Forward Declarations
class nsIAtom;

// --------------------------------------------------------------------------------------
// nsMPFileLocProvider - The MP stands for "MonoProfile" This nsIDirectoryServiceProvider
// can be used in place of the profile mgr for applications which don't need to manage
// distinct user profiles. It provides the same file locations the the profile mgr does.
// --------------------------------------------------------------------------------------

class nsMPFileLocProvider: public nsIDirectoryServiceProvider 
{
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER
    
  public:
                     nsMPFileLocProvider();
   virtual           ~nsMPFileLocProvider();
   
   /*
      Initialize:          Must be called after constructor.
      
      profileParentDir ->  The directory that is the parent dir of our profile folder.
      profileDirName ->    The leaf name of our profile folder.
   */
                        
   virtual nsresult  Initialize(nsIFile* profileParentDir, const char *profileDirName);
   
  protected:

   // Atoms for file locations
   static nsIAtom*   sApp_PrefsDirectory50;
   static nsIAtom*   sApp_PreferencesFile50;
   static nsIAtom*   sApp_UserProfileDirectory50;
   static nsIAtom*   sApp_UserChromeDirectory;
   static nsIAtom*   sApp_LocalStore50;
   static nsIAtom*   sApp_History50;
   static nsIAtom*   sApp_UsersPanels50;
   static nsIAtom*   sApp_UsersMimeTypes50;
   static nsIAtom*   sApp_BookmarksFile50;
   static nsIAtom*   sApp_SearchFile50;
   static nsIAtom*   sApp_MailDirectory50;
   static nsIAtom*   sApp_ImapMailDirectory50;
   static nsIAtom*   sApp_NewsDirectory50;
   static nsIAtom*   sApp_MessengerFolderCache50;
      
   nsresult       InitProfileDir(nsIFile* profileParentDir,
                                 const char *profileDirName,
                                 nsIFile **profileDir);
   
   PRBool         mInitialized;
   nsCOMPtr<nsIFile> mProfileDir;

};
