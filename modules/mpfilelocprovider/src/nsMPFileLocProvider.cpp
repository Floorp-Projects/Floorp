/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsMPFileLocProvider.h"
#include "nsIAtom.h"
#include "nsILocalFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsISupportsUtils.h"
#include "nsXPIDLString.h"
#include "nsISimpleEnumerator.h"

// Static Variables

nsIAtom*   nsMPFileLocProvider::sApp_PrefsDirectory50;
nsIAtom*   nsMPFileLocProvider::sApp_PreferencesFile50;
nsIAtom*   nsMPFileLocProvider::sApp_UserProfileDirectory50;
nsIAtom*   nsMPFileLocProvider::sApp_UserChromeDirectory;
nsIAtom*   nsMPFileLocProvider::sApp_LocalStore50;
nsIAtom*   nsMPFileLocProvider::sApp_History50;
nsIAtom*   nsMPFileLocProvider::sApp_UsersPanels50;
nsIAtom*   nsMPFileLocProvider::sApp_UsersMimeTypes50;
nsIAtom*   nsMPFileLocProvider::sApp_BookmarksFile50;
nsIAtom*   nsMPFileLocProvider::sApp_SearchFile50;
nsIAtom*   nsMPFileLocProvider::sApp_MailDirectory50;
nsIAtom*   nsMPFileLocProvider::sApp_ImapMailDirectory50;
nsIAtom*   nsMPFileLocProvider::sApp_NewsDirectory50;
nsIAtom*   nsMPFileLocProvider::sApp_MessengerFolderCache50;


//*****************************************************************************
// nsMPFileLocProvider::nsMPFileLocProvider
//*****************************************************************************   

nsMPFileLocProvider::nsMPFileLocProvider() :
    mInitialized(PR_FALSE)
{
    NS_INIT_REFCNT();
}


nsMPFileLocProvider::~nsMPFileLocProvider()
{
    NS_IF_RELEASE(sApp_PrefsDirectory50);
    NS_IF_RELEASE(sApp_PreferencesFile50);
    NS_IF_RELEASE(sApp_UserProfileDirectory50);
    NS_IF_RELEASE(sApp_UserChromeDirectory);
    NS_IF_RELEASE(sApp_LocalStore50);
    NS_IF_RELEASE(sApp_History50);
    NS_IF_RELEASE(sApp_UsersPanels50);
    NS_IF_RELEASE(sApp_UsersMimeTypes50);
    NS_IF_RELEASE(sApp_BookmarksFile50);
    NS_IF_RELEASE(sApp_SearchFile50);
    NS_IF_RELEASE(sApp_MailDirectory50);
    NS_IF_RELEASE(sApp_ImapMailDirectory50);
    NS_IF_RELEASE(sApp_NewsDirectory50);
    NS_IF_RELEASE(sApp_MessengerFolderCache50);
}


nsresult nsMPFileLocProvider::Initialize(nsIFile* profileParentDir, const char *profileDirName)
{
    nsresult rv;
    
    if (mInitialized)
        return NS_OK;
        
    rv = InitProfileDir(profileParentDir, profileDirName, getter_AddRefs(mProfileDir));
    if (NS_FAILED(rv)) return rv;

    // Make our directory atoms

    // Preferences:
    sApp_PrefsDirectory50         = NS_NewAtom(NS_APP_PREFS_50_DIR);
    sApp_PreferencesFile50        = NS_NewAtom(NS_APP_PREFS_50_FILE);

    // Profile:
    sApp_UserProfileDirectory50   = NS_NewAtom(NS_APP_USER_PROFILE_50_DIR);

    // Application Directories:
    sApp_UserChromeDirectory      = NS_NewAtom(NS_APP_USER_CHROME_DIR);

    // Aplication Files:
    sApp_LocalStore50             = NS_NewAtom(NS_APP_LOCALSTORE_50_FILE);
    sApp_History50                = NS_NewAtom(NS_APP_HISTORY_50_FILE);
    sApp_UsersPanels50            = NS_NewAtom(NS_APP_USER_PANELS_50_FILE);
    sApp_UsersMimeTypes50         = NS_NewAtom(NS_APP_USER_MIMETYPES_50_FILE);

    // Bookmarks:
    sApp_BookmarksFile50          = NS_NewAtom(NS_APP_BOOKMARKS_50_FILE);

    // Search
    sApp_SearchFile50             = NS_NewAtom(NS_APP_SEARCH_50_FILE);

    // MailNews
    sApp_MailDirectory50          = NS_NewAtom(NS_APP_MAIL_50_DIR);
    sApp_ImapMailDirectory50      = NS_NewAtom(NS_APP_IMAP_MAIL_50_DIR);
    sApp_NewsDirectory50          = NS_NewAtom(NS_APP_NEWS_50_DIR);
    sApp_MessengerFolderCache50   = NS_NewAtom(NS_APP_MESSENGER_FOLDER_CACHE_50_DIR);

    nsCOMPtr<nsIDirectoryService> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    directoryService->RegisterProvider(this);
    
    mInitialized = PR_TRUE;
    return NS_OK;    
}


//*****************************************************************************
// nsMPFileLocProvider::nsISupports
//*****************************************************************************   

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMPFileLocProvider, nsIDirectoryServiceProvider)


// File Name Defines - Copied straight from nsProfile

#define PREFS_FILE_50_NAME          "prefs.js"
#define USER_CHROME_DIR_50_NAME     "chrome"
#define LOCAL_STORE_FILE_50_NAME    "localstore.rdf"
#define HISTORY_FILE_50_NAME        "history.dat"
#define PANELS_FILE_50_NAME         "panels.rdf"
#define MIME_TYPES_FILE_50_NAME     "mimeTypes.rdf"
#define BOOKMARKS_FILE_50_NAME      "bookmarks.html"
#define SEARCH_FILE_50_NAME         "search.rdf" 
#define MAIL_DIR_50_NAME            "Mail"
#define IMAP_MAIL_DIR_50_NAME       "ImapMail"
#define NEWS_DIR_50_NAME            "News"
#define MSG_FOLDER_CACHE_DIR_50_NAME "panacea.dat"

NS_IMETHODIMP
nsMPFileLocProvider::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{
    nsCOMPtr<nsIFile>  localFile;
    nsresult rv = NS_ERROR_FAILURE;

    *_retval = nsnull;
    *persistant = PR_TRUE;
    
    NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

    nsIAtom* inAtom = NS_NewAtom(prop);
    NS_ENSURE_TRUE(inAtom, NS_ERROR_OUT_OF_MEMORY);

    if (inAtom == sApp_PrefsDirectory50)
    {
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
    }
    else if (inAtom == sApp_PreferencesFile50)
    {
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(PREFS_FILE_50_NAME);
    }
    else if (inAtom == sApp_UserProfileDirectory50)
    {
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
    }
    else if (inAtom == sApp_UserChromeDirectory)
    {
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(USER_CHROME_DIR_50_NAME);
    }
    else if (inAtom == sApp_LocalStore50)
    {
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(LOCAL_STORE_FILE_50_NAME);
    }
    else if (inAtom == sApp_History50)
    {
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(HISTORY_FILE_50_NAME);
    }
    else if (inAtom == sApp_UsersPanels50)
    {
        // Here we differ from nsFileLocator - It checks for the
        // existence of this file and if it does not exist, copies
        // it from the defaults folder to the profile folder. Since
        // WE set up any profile folder, we'll make sure it's copied then.
        
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(PANELS_FILE_50_NAME);
    }
    else if (inAtom == sApp_UsersMimeTypes50)
    {
        // Here we differ from nsFileLocator - It checks for the
        // existence of this file and if it does not exist, copies
        // it from the defaults folder to the profile folder. Since
        // WE set up any profile folder, we'll make sure it's copied then.
        
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(MIME_TYPES_FILE_50_NAME);
    }
    else if (inAtom == sApp_BookmarksFile50)
    {
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(BOOKMARKS_FILE_50_NAME);
    }
    else if (inAtom == sApp_SearchFile50)
    {
        // Here we differ from nsFileLocator - It checks for the
        // existence of this file and if it does not exist, copies
        // it from the defaults folder to the profile folder. Since
        // WE set up any profile folder, we'll make sure it's copied then.
        
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(SEARCH_FILE_50_NAME);
    }
    else if (inAtom == sApp_MailDirectory50)
    {
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(MAIL_DIR_50_NAME);
    }
    else if (inAtom == sApp_ImapMailDirectory50)
    {
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(IMAP_MAIL_DIR_50_NAME);
    }
    else if (inAtom == sApp_NewsDirectory50)
    {
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(NEWS_DIR_50_NAME);
    }
    else if (inAtom == sApp_MessengerFolderCache50)
    {
        rv = mProfileDir->Clone(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(MSG_FOLDER_CACHE_DIR_50_NAME);
    }

    NS_RELEASE(inAtom);

    if (localFile && NS_SUCCEEDED(rv))
    	return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);
    	
    return rv;
}

/*
    Copies the contents of srcDir into destDir.
    destDir will be created if it doesn't exist.
*/

static
nsresult RecursiveCopy(nsIFile* srcDir, nsIFile* destDir)
{
    nsresult rv;
    PRBool isDir;
    
    rv = srcDir->IsDirectory(&isDir);
    if (NS_FAILED(rv)) return rv;
      if (!isDir) return NS_ERROR_INVALID_ARG;

    PRBool exists;
    rv = destDir->Exists(&exists);
      if (NS_SUCCEEDED(rv) && !exists)
              rv = destDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
      if (NS_FAILED(rv)) return rv;

    PRBool hasMore = PR_FALSE;
    nsCOMPtr<nsISimpleEnumerator> dirIterator;
    rv = srcDir->GetDirectoryEntries(getter_AddRefs(dirIterator));
    if (NS_FAILED(rv)) return rv;
    
    rv = dirIterator->HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIFile> dirEntry;
    
      while (hasMore)
      {
              rv = dirIterator->GetNext((nsISupports**)getter_AddRefs(dirEntry));
              if (NS_SUCCEEDED(rv))
              {
                  rv = dirEntry->IsDirectory(&isDir);
                  if (NS_SUCCEEDED(rv))
                  {
                      if (isDir)
                      {
                          nsCOMPtr<nsIFile> destClone;
                          rv = destDir->Clone(getter_AddRefs(destClone));
                          if (NS_SUCCEEDED(rv))
                          {
                              nsCOMPtr<nsILocalFile> newChild(do_QueryInterface(destClone));
                              nsXPIDLCString leafName;
                              dirEntry->GetLeafName(getter_Copies(leafName));
                              newChild->AppendRelativePath(leafName);
                              rv = RecursiveCopy(dirEntry, newChild);
                          }
                      }
                      else
                          rv = dirEntry->CopyTo(destDir, nsnull);
                  }
              
              }
        rv = dirIterator->HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) return rv;
      }

      return rv;
}

nsresult nsMPFileLocProvider::InitProfileDir(nsIFile *profileParentDir,
                                                const char *profileDirName,
                                                nsIFile **outProfileDir)
{
    NS_ENSURE_ARG_POINTER(outProfileDir);
    
    // Make sure our "Profile" folder exists
    
    nsresult    rv;
    nsCOMPtr<nsIFile> profileDir;
        
    rv = profileParentDir->Clone(getter_AddRefs(profileDir));
    if (NS_FAILED(rv)) return rv;
    rv = profileDir->Append(profileDirName);
    if (NS_FAILED(rv)) return rv;
    
    PRBool exists;
    rv = profileDir->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    if (!exists)
    {
        nsCOMPtr<nsIFile> profDefaultsFolder;
        
        rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_50_DIR, getter_AddRefs(profDefaultsFolder));
        if (NS_FAILED(rv))
        {
            rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR, getter_AddRefs(profDefaultsFolder));
            if (NS_FAILED(rv)) return rv;
        }
        rv = RecursiveCopy(profDefaultsFolder, profileDir);
        if (NS_FAILED(rv)) return rv;
    }

    if (NS_FAILED(rv)) return rv;
    *outProfileDir = profileDir;
    NS_ADDREF(*outProfileDir);
    
    return NS_OK;
}

