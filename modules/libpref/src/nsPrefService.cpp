/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
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

#include "nsPrefService.h"
#include "nsSafeSaveFile.h"
#include "jsapi.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsNetUtil.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsPrefBranch.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"

#include "nsQuickSort.h"
#include "prmem.h"
#include "pldhash.h"

#include "prefapi.h"
#include "prefapi_private_data.h"

// supporting PREF_Init()
#include "nsIJSRuntimeService.h"

#include "nsITimelineService.h"

#ifdef MOZ_PROFILESHARING
#include "nsIProfileSharingSetup.h"
#include "nsSharedPrefHandler.h"
#endif

// Definitions
#define INITIAL_MAX_DEFAULT_PREF_FILES 10


// Prototypes
#ifdef MOZ_PROFILESHARING
static PRBool isSharingEnabled();
#endif

static nsresult openPrefFile(nsIFile* aFile, PRBool aIsErrorFatal,
                             PRBool aIsGlobalContext, PRBool aSkipFirstLine);


  // needed so we can still get the JS Runtime Service during XPCOM shutdown
static nsIJSRuntimeService* gJSRuntimeService = nsnull; // owning reference

//-----------------------------------------------------------------------------

/*
 * Constructor/Destructor
 */

nsPrefService::nsPrefService()
: mCurrentFile(nsnull),
  mErrorOpeningUserPrefs(PR_FALSE)
{
  nsPrefBranch *rootBranch;

  rootBranch = new nsPrefBranch("", PR_FALSE); 
  mRootBranch = (nsIPrefBranch *)rootBranch;
  
#if MOZ_PROFILESHARING
  mCurrentSharedFile = nsnull;
  mErrorOpeningSharedUserPrefs = PR_FALSE;
#endif
}

nsPrefService::~nsPrefService()
{
  PREF_Cleanup();
  NS_IF_RELEASE(mCurrentFile);
  NS_IF_RELEASE(gJSRuntimeService);

#ifdef MOZ_PROFILESHARING
  NS_IF_RELEASE(mCurrentSharedFile);
  NS_IF_RELEASE(gSharedPrefHandler);
#endif
}


/*
 * nsISupports Implementation
 */

NS_IMPL_THREADSAFE_ADDREF(nsPrefService)
NS_IMPL_THREADSAFE_RELEASE(nsPrefService)

NS_INTERFACE_MAP_BEGIN(nsPrefService)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrefService)
    NS_INTERFACE_MAP_ENTRY(nsIPrefService)
    NS_INTERFACE_MAP_ENTRY(nsIObserver)
    NS_INTERFACE_MAP_ENTRY(nsIPrefBranch)
    NS_INTERFACE_MAP_ENTRY(nsIPrefBranchInternal)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END


/*
 * nsIPrefService Implementation
 */

nsresult nsPrefService::Init()
{
  nsXPIDLCString lockFileName;
  nsresult rv;

  if (!PREF_Init(nsnull))
    return NS_ERROR_FAILURE;

  /*
   * The following is a small hack which will allow us to only load the library
   * which supports the netscape.cfg file if the preference is defined. We
   * test for the existence of the pref, set in the all.js (mozilla) or
   * all-ns.js (netscape 6), and if it exists we startup the pref config
   * category which will do the rest.
   */

  rv = mRootBranch->GetCharPref("general.config.filename", getter_Copies(lockFileName));
  if (NS_SUCCEEDED(rv))
    NS_CreateServicesFromCategory("pref-config-startup",
                                  NS_STATIC_CAST(nsISupports *, NS_STATIC_CAST(void *, this)),
                                  "pref-config-startup");    

  nsCOMPtr<nsIObserverService> observerService = 
           do_GetService("@mozilla.org/observer-service;1", &rv);
  if (observerService) {
    rv = observerService->AddObserver(this, "profile-before-change", PR_TRUE);
    if (NS_SUCCEEDED(rv)) {
      rv = observerService->AddObserver(this, "profile-do-change", PR_TRUE);
    }
  }

#ifdef MOZ_PROFILESHARING  
  rv = NS_CreateSharedPrefHandler(this);
#endif

  return(rv);
}

NS_IMETHODIMP nsPrefService::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  nsresult rv = NS_OK;

  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("shutdown-cleanse").get())) {
      if (mCurrentFile) {
        mCurrentFile->Remove(PR_FALSE);
        NS_RELEASE(mCurrentFile);
      }
    } else {
      rv = SavePrefFile(nsnull);
#ifdef MOZ_PROFILESHARING
      if (isSharingEnabled())
        rv = gSharedPrefHandler->OnSessionEnd();
#endif
    }
  } else if (!nsCRT::strcmp(aTopic, "profile-do-change")) {
  
#ifdef MOZ_PROFILESHARING
    if (isSharingEnabled())
      rv = gSharedPrefHandler->OnSessionBegin();
    else
#endif
    {
      ResetUserPrefs();
      rv = ReadUserPrefs(nsnull);
    }
  }
  return rv;
}


NS_IMETHODIMP nsPrefService::ReadUserPrefs(nsIFile *aFile)
{
  nsresult rv;

  if (nsnull == aFile) {
    rv = UseDefaultPrefFile();
    UseUserPrefFile();

    NotifyServiceObservers(NS_PREFSERVICE_READ_TOPIC_ID);

  } else {
    rv = ReadAndOwnUserPrefFile(aFile);
  }
  return rv;
}

NS_IMETHODIMP nsPrefService::ResetPrefs()
{
  NotifyServiceObservers(NS_PREFSERVICE_RESET_TOPIC_ID);
  PREF_CleanupPrefs();

  if (!PREF_Init(nsnull))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP nsPrefService::ResetUserPrefs()
{
  PREF_ClearAllUserPrefs();
  return NS_OK;    
}

NS_IMETHODIMP nsPrefService::SavePrefFile(nsIFile *aFile)
{
#ifdef MOZ_PROFILESHARING
  // sharing only applies to the default prefs file
  if (aFile == nsnull && isSharingEnabled())
    return gSharedPrefHandler->OnSavePrefs();
#endif
  return SavePrefFileInternal(aFile);
}

NS_IMETHODIMP nsPrefService::GetBranch(const char *aPrefRoot, nsIPrefBranch **_retval)
{
  nsresult rv;

  if ((nsnull != aPrefRoot) && (*aPrefRoot != '\0')) {
    // TODO: - cache this stuff and allow consumers to share branches (hold weak references I think)
    nsPrefBranch* prefBranch = new nsPrefBranch(aPrefRoot, PR_FALSE);

    rv = prefBranch->QueryInterface(NS_GET_IID(nsIPrefBranch), (void **)_retval);
  } else {
    // special case caching the default root
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(mRootBranch, &rv);
    if (NS_SUCCEEDED(rv)) {
      *_retval = prefBranch;
      NS_ADDREF(*_retval);
    }
  }
  return rv;
}

NS_IMETHODIMP nsPrefService::GetDefaultBranch(const char *aPrefRoot, nsIPrefBranch **_retval)
{
  nsresult rv;

  // TODO: - cache this stuff and allow consumers to share branches (hold weak references I think)
  nsPrefBranch* prefBranch = new nsPrefBranch(aPrefRoot, PR_TRUE);

  rv = prefBranch->QueryInterface(NS_GET_IID(nsIPrefBranch), (void **)_retval);
  return rv;
}


// Forward these methods through the nsIPrefBranchInternal headers

NS_IMETHODIMP nsPrefService::AddObserver(const char *aDomain, nsIObserver *aObserver, PRBool aHoldWeak)
{
  nsresult rv;

  nsCOMPtr<nsIPrefBranchInternal> prefBranch = do_QueryInterface(mRootBranch, &rv);
  if (NS_SUCCEEDED(rv))
    rv = prefBranch->AddObserver(aDomain, aObserver, aHoldWeak);
  return rv;
}

NS_IMETHODIMP nsPrefService::RemoveObserver(const char *aDomain, nsIObserver *aObserver)
{
  nsresult rv;

  nsCOMPtr<nsIPrefBranchInternal> prefBranch = do_QueryInterface(mRootBranch, &rv);
  if (NS_SUCCEEDED(rv))
    rv = prefBranch->RemoveObserver(aDomain, aObserver);
  return rv;
}


nsresult nsPrefService::NotifyServiceObservers(const char *aTopic)
{
  nsresult rv;
  nsCOMPtr<nsIObserverService> observerService = 
    do_GetService("@mozilla.org/observer-service;1", &rv);
  
  if (NS_FAILED(rv) || !observerService)
    return rv;

  nsISupports *subject = (nsISupports *)((nsIPrefService *)this);
  observerService->NotifyObservers(subject, aTopic, nsnull);
  
  return NS_OK;
}

nsresult nsPrefService::UseDefaultPrefFile()
{
  nsresult rv, rv2;
  nsCOMPtr<nsIFile> aFile;

#ifdef MOZ_PROFILESHARING
  // First, read the shared file.
  if (isSharingEnabled()) {
    rv = NS_GetSpecialDirectory(NS_SHARED NS_APP_PREFS_50_FILE, getter_AddRefs(aFile));
    if (NS_SUCCEEDED(rv)) {
      rv = ReadAndOwnSharedUserPrefFile(aFile);
      // Most likely cause of failure here is that the file didn't
      // exist, so save a new one. mSharedUserPrefReadFailed will be
      // used to catch an error in actually reading the file.
      if (NS_FAILED(rv)) {
        rv2 = SavePrefFileInternal(aFile);
        NS_ASSERTION(NS_SUCCEEDED(rv2), "Failed to save new shared pref file");
      }
    }
  }
  // Continue on to read the nonshared file.
#endif

  rv = NS_GetSpecialDirectory(NS_APP_PREFS_50_FILE, getter_AddRefs(aFile));
  if (NS_SUCCEEDED(rv)) {
    rv = ReadAndOwnUserPrefFile(aFile);
    // Most likely cause of failure here is that the file didn't
    // exist, so save a new one. mUserPrefReadFailed will be
    // used to catch an error in actually reading the file.
    if (NS_FAILED(rv)) {
      rv2 = SavePrefFileInternal(aFile);
      NS_ASSERTION(NS_SUCCEEDED(rv2), "Failed to save new shared pref file");
    }
  }
  
  return rv;
}

nsresult nsPrefService::UseUserPrefFile()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFile> aFile;

#ifdef MOZ_PROFILESHARING
  nsCAutoString prefsDirProp(NS_APP_PREFS_50_DIR);
  if (isSharingEnabled())
    prefsDirProp.Insert(NS_SHARED, 0); // Prepend modifier so we get shared file
#else
  nsDependentCString prefsDirProp(NS_APP_PREFS_50_DIR);
#endif

  rv = NS_GetSpecialDirectory(prefsDirProp.get(), getter_AddRefs(aFile));
  if (NS_SUCCEEDED(rv) && aFile) {
    rv = aFile->AppendNative(NS_LITERAL_CSTRING("user.js"));
    if (NS_SUCCEEDED(rv)) {
      rv = openPrefFile(aFile, PR_FALSE, PR_FALSE, PR_FALSE);
    }
  }
  return rv;
}

nsresult nsPrefService::ReadAndOwnUserPrefFile(nsIFile *aFile)
{
  NS_ENSURE_ARG(aFile);
  
  if (mCurrentFile == aFile)
    return NS_OK;
  NS_IF_RELEASE(mCurrentFile);
  mCurrentFile = aFile;
  NS_ADDREF(mCurrentFile);

#ifdef MOZ_PROFILESHARING
  // We don't want prefs set here to cause transactions
  gSharedPrefHandler->ReadingUserPrefs(PR_TRUE);
#endif

  // We need to track errors in reading the shared and the
  // non-shared files independently. Clear gErrorOpeningUserPrefs
  // and set the appropriate member variable from it after reading.
  gErrorOpeningUserPrefs = PR_FALSE;
  nsresult rv = openPrefFile(mCurrentFile, PR_TRUE, PR_FALSE, PR_TRUE);
  mErrorOpeningUserPrefs = gErrorOpeningUserPrefs;

#ifdef MOZ_PROFILESHARING
  gSharedPrefHandler->ReadingUserPrefs(PR_FALSE);
#endif

  return rv;
}

#ifdef MOZ_PROFILESHARING
nsresult nsPrefService::ReadAndOwnSharedUserPrefFile(nsIFile *aFile)
{
  NS_ENSURE_ARG(aFile);

  if (mCurrentSharedFile == aFile)
    return NS_OK;
  NS_IF_RELEASE(mCurrentSharedFile);
  mCurrentSharedFile = aFile;
  NS_ADDREF(mCurrentSharedFile);

#ifdef MOZ_PROFILESHARING
  // We don't want prefs set here to cause transactions
  gSharedPrefHandler->ReadingUserPrefs(PR_TRUE);
#endif

  // We need to track errors in reading the shared and the
  // non-shared files independently. Clear gErrorOpeningUserPrefs
  // and set the appropriate member variable from it after reading.
  gErrorOpeningUserPrefs = PR_FALSE;
  nsresult rv = openPrefFile(mCurrentSharedFile, PR_TRUE, PR_FALSE, PR_TRUE);
  mErrorOpeningSharedUserPrefs = gErrorOpeningUserPrefs;
  
#ifdef MOZ_PROFILESHARING
  gSharedPrefHandler->ReadingUserPrefs(PR_FALSE);
#endif

  return rv;
}
#endif

nsresult nsPrefService::SavePrefFileInternal(nsIFile *aFile)
{
  if (nsnull == aFile) {
    // the gDirty flag tells us if we should write to mCurrentFile
    // we only check this flag when the caller wants to write to the default
    if (!gDirty)
      return NS_OK;
    
    // It's possible that we never got a prefs file.
    nsresult rv = NS_OK;
    if (mCurrentFile)
      rv = WritePrefFile(mCurrentFile);

#ifdef MOZ_PROFILESHARING
    if (mCurrentSharedFile) {
      nsresult rv2 = WritePrefFile(mCurrentSharedFile);
      if (NS_SUCCEEDED(rv))
        rv = rv2;
    }
#endif

    return rv;
  } else {
    return WritePrefFile(aFile);
  }
}

nsresult nsPrefService::WritePrefFile(nsIFile* aFile)
{
  const char                outHeader[] =
    "# Mozilla User Preferences"
    NS_LINEBREAK
    NS_LINEBREAK
    "/* Do not edit this file."
    NS_LINEBREAK
    " *"
    NS_LINEBREAK
    " * If you make changes to this file while the browser is running,"
    NS_LINEBREAK
    " * the changes will be overwritten when the browser exits."
    NS_LINEBREAK
    " *"
    NS_LINEBREAK
    " * To make a manual change to preferences, you can visit the URL about:config"
    NS_LINEBREAK
    " * For more information, see http://www.mozilla.org/unix/customizing.html#prefs"
    NS_LINEBREAK
    " */"
    NS_LINEBREAK
    NS_LINEBREAK;

  nsCOMPtr<nsIOutputStream> outStream;
  PRUint32                  writeAmount;
  nsresult                  rv;

  if (!gHashTable.ops)
    return NS_ERROR_NOT_INITIALIZED;

  /* ?! Don't save (blank) user prefs if there was an error reading them */
  if (aFile == mCurrentFile && mErrorOpeningUserPrefs)
    return NS_OK;
#if MOZ_PROFILESHARING
  if (aFile == mCurrentSharedFile && mErrorOpeningSharedUserPrefs)
    return NS_OK;
#endif

  // execute a "safe" save by saving through a tempfile
  PRInt32 numCopies = 1;
  mRootBranch->GetIntPref("backups.number_of_prefs_copies", &numCopies);

  nsSafeSaveFile safeSave(aFile, numCopies);
  rv = safeSave.CreateBackup(nsSafeSaveFile::kPurgeNone);
  if (NS_FAILED(rv))
    return rv;

  char** valueArray = (char **)PR_Calloc(sizeof(char *), gHashTable.entryCount);
  if (!valueArray)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outStream), aFile);
  if (NS_FAILED(rv)) 
      return rv;
  
  // write out the file header
  rv = outStream->Write(outHeader, sizeof(outHeader) - 1, &writeAmount);

  pref_saveArgs saveArgs;
  saveArgs.prefArray = valueArray;
  saveArgs.saveTypes = SAVE_ALL;
  
#if MOZ_PROFILESHARING
  if (isSharingEnabled()) {
    if (aFile == mCurrentSharedFile)
      saveArgs.saveTypes = SAVE_SHARED;
    else if (aFile == mCurrentFile)
      saveArgs.saveTypes = SAVE_NONSHARED;
  }
#endif
  
  // get the lines that we're supposed to be writing to the file
  PL_DHashTableEnumerate(&gHashTable, pref_savePref, &saveArgs);
    
  /* Sort the preferences to make a readable file on disk */
  NS_QuickSort(valueArray, gHashTable.entryCount, sizeof(char *), pref_CompareStrings, NULL);
  char** walker = valueArray;
  for (PRUint32 valueIdx = 0; valueIdx < gHashTable.entryCount; valueIdx++, walker++) {
    if (*walker) {
      // skip writing if an has error occurred
      if (NS_SUCCEEDED(rv)) {
        rv = outStream->Write(*walker, strlen(*walker), &writeAmount);
        if (NS_SUCCEEDED(rv))
          rv = outStream->Write(NS_LINEBREAK, NS_LINEBREAK_LEN, &writeAmount);
      }
      // always free though...
      PR_Free(*walker);
    }
  }
  PR_Free(valueArray);
  outStream->Close();

  // if save failed replace the original file from backup
  if (NS_FAILED(rv)) {
    nsresult rv2;
    rv2 = safeSave.RestoreFromBackup();
    if (NS_SUCCEEDED(rv2)) {
      // we failed to write the file, but managed to restore the previous one...
      rv = NS_OK;
    }
  }
  if (NS_SUCCEEDED(rv))
    gDirty = PR_FALSE;
  return rv;
}

#ifdef MOZ_PROFILESHARING
static PRBool isSharingEnabled()
{
  static PRBool gSharingEnabled = PR_FALSE;
  
  // If FALSE, query again. It may not have been set yet.
  if (!gSharingEnabled) {
    nsCOMPtr<nsIProfileSharingSetup> sharingSetup =
        do_GetService("@mozilla.org/embedcomp/profile-sharing-setup;1");
    if (sharingSetup)
      sharingSetup->GetIsSharingEnabled(&gSharingEnabled);
  }
  return gSharingEnabled;
}
#endif

static nsresult openPrefFile(nsIFile* aFile, PRBool aIsErrorFatal,
                             PRBool aIsGlobalContext, PRBool aSkipFirstLine)
{
  nsCOMPtr<nsIInputStream> inStr;
  char *readBuf;
  PRInt64 llFileSize;
  PRUint32 fileSize;
  nsresult rv;

#if MOZ_TIMELINE
  {
    nsCAutoString str;
    aFile->GetNativePath(str);
    NS_TIMELINE_MARK_FUNCTION1("load pref file", str.get());
  }
#endif

  rv = aFile->GetFileSize(&llFileSize);
  if (NS_FAILED(rv))
    return rv;        
  LL_L2UI(fileSize, llFileSize); // Converting 64 bit structure to unsigned int

  // Now that we know the file exists, set this flag until we have
  // successfully read and evaluated the prefs file. This will
  // prevent us from writing an empty or partial prefs.js.
  
  gErrorOpeningUserPrefs = aIsErrorFatal;

  rv = NS_NewLocalFileInputStream(getter_AddRefs(inStr), aFile);
  if (NS_FAILED(rv)) 
    return rv;        

  // XXX maybe we should read the file in chunks instead??
  readBuf = (char *)PR_Malloc(fileSize);
  if (!readBuf) 
    return NS_ERROR_OUT_OF_MEMORY;

  PRUint32 amtRead = 0;
  rv = inStr->Read(readBuf, fileSize, &amtRead);
  NS_ASSERTION((amtRead == fileSize), "failed to read the entire prefs file!!");
  if (amtRead != fileSize)
    return NS_ERROR_FAILURE;
 #ifdef XP_OS2 /* OS/2 workaround - our system editor adds an EOF character */
     if (readBuf[amtRead - 1] == 0x1A) {
        amtRead--;
     }
 #endif
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString leafName;
    aFile->GetNativeLeafName(leafName);
    if (PREF_EvaluateConfigScript(readBuf, amtRead, leafName.get(), aIsGlobalContext, PR_TRUE,
                                   aSkipFirstLine))
      gErrorOpeningUserPrefs = PR_FALSE;
    else
      rv = NS_ERROR_FAILURE;
  }

  PR_Free(readBuf);
  return rv;        
}


/*
 * some stuff that gets called from Pref_Init()
 */

/// Note: inplaceSortCallback is a small C callback stub for NS_QuickSort
static int PR_CALLBACK
inplaceSortCallback(const void *data1, const void *data2, void *privateData)
{
  nsCAutoString name1;
  nsCAutoString name2;
  nsIFile *file1= *(nsIFile **)data1;
  nsIFile *file2= *(nsIFile **)data2;
  nsresult rv;
  int sortResult = 0;

  rv = file1->GetNativeLeafName(name1);
  NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get the leaf name");
  if (NS_SUCCEEDED(rv)) {
    rv = file2->GetNativeLeafName(name2);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get the leaf name");
    if (NS_SUCCEEDED(rv)) {
      if (!name1.IsEmpty() && !name2.IsEmpty()) {
        // we want it so foo.js will come before foo-<bar>.js
        // "foo." is before "foo-", so we have to reverse the order to accomplish
        sortResult = Compare(name2, name1); // XXX i18n
      }
    }
  }
  return sortResult;
}

//----------------------------------------------------------------------------------------
PRBool pref_InitInitialObjects()
// Initialize default preference JavaScript buffers from
// appropriate TEXT resources
//----------------------------------------------------------------------------------------
{
  nsCOMPtr<nsIFile> aFile;
  nsCOMPtr<nsIFile> defaultPrefDir;
  nsresult          rv;
  PRBool            hasMoreElements;
        
  static const char* specialFiles[] = {
#if defined(XP_MAC) || defined(XP_MACOSX)
      "macprefs.js"
#elif defined(XP_WIN)
      "winpref.js"
#elif defined(XP_UNIX)
      "unix.js"
#if defined(VMS)
      , "openvms.js"
#elif defined(_AIX)
      , "aix.js"
#endif
#if defined(MOZ_WIDGET_PHOTON)
	  , "photon.js"
#endif		 
#elif defined(XP_OS2)
      "os2pref.js"
#elif defined(XP_BEOS)
      "beos.js"
#endif
  };

  rv = NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR, getter_AddRefs(defaultPrefDir));
  if (NS_FAILED(rv))
    return PR_FALSE;

  nsIFile **defaultPrefFiles = (nsIFile **)nsMemory::Alloc(INITIAL_MAX_DEFAULT_PREF_FILES * sizeof(nsIFile *));
  int maxDefaultPrefFiles = INITIAL_MAX_DEFAULT_PREF_FILES;
  int numFiles = 0;

  // Parse all the random files that happen to be in the components directory.
  nsCOMPtr<nsISimpleEnumerator> dirIterator;
  rv = defaultPrefDir->GetDirectoryEntries(getter_AddRefs(dirIterator));
  if (!dirIterator) {
    NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Could not make a directory iterator.");
    return PR_FALSE;
  }

  dirIterator->HasMoreElements(&hasMoreElements);
  if (!hasMoreElements) {
    NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Prefs directory is empty.");
    return PR_FALSE;
  }

  while (hasMoreElements) {
    PRBool shouldParse = PR_TRUE;
    nsCAutoString leafName;

    dirIterator->GetNext(getter_AddRefs(aFile));
    dirIterator->HasMoreElements(&hasMoreElements);

    rv = aFile->GetNativeLeafName(leafName);
    if (NS_SUCCEEDED(rv)) {
      // Skip non-js files
      if (!StringEndsWith(leafName, NS_LITERAL_CSTRING(".js")))
        shouldParse = PR_FALSE;
      // Skip files in the special list.
      if (shouldParse) {
        for (int j = 0; j < (int) (sizeof(specialFiles) / sizeof(char *)); j++)
          if (!strcmp(leafName.get(), specialFiles[j]))
            shouldParse = PR_FALSE;
      }
      if (shouldParse) {
        rv = aFile->Clone(&(defaultPrefFiles[numFiles]));
        if NS_SUCCEEDED(rv) {
          ++numFiles;
          if (numFiles == maxDefaultPrefFiles) {
            // double the size of the array
            maxDefaultPrefFiles *= 2;
            defaultPrefFiles = (nsIFile **)nsMemory::Realloc(defaultPrefFiles, maxDefaultPrefFiles * sizeof(nsIFile *));
          }
        }
      }
    }
  };

  NS_QuickSort((void *)defaultPrefFiles, numFiles, sizeof(nsIFile *), inplaceSortCallback, nsnull);

  int k;
  for (k = 0; k < numFiles; k++) {
    rv = openPrefFile(defaultPrefFiles[k], PR_FALSE, PR_FALSE, PR_FALSE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Config file not parsed successfully");
    NS_RELEASE(defaultPrefFiles[k]);
  }
  nsMemory::Free(defaultPrefFiles);

  // Finally, parse any other special files (platform-specific ones).
  for (k = 0; k < (int) (sizeof(specialFiles) / sizeof(char *)); k++) {
    // we must get the directory every time so we can append the child
    // because SetLeafName will not work here.
    rv = defaultPrefDir->Clone(getter_AddRefs(aFile));
    if (NS_SUCCEEDED(rv)) {
      rv = aFile->AppendNative(nsDependentCString(specialFiles[k]));
      if (NS_SUCCEEDED(rv)) {
        rv = openPrefFile(aFile, PR_FALSE, PR_FALSE, PR_FALSE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "<platform>.js was not parsed successfully");
      }
    }
  }

  return PR_TRUE;
}
