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
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIObserverService.h"
#include "nsPrefBranch.h"
#include "nsXPIDLString.h"

#include "nsQuickSort.h"
#include "prmem.h"
#include "pldhash.h"

#include "prefapi.h"
#include "prefapi_private_data.h"

// supporting PREF_Init()
#include "nsIJSRuntimeService.h"

#include "nsITimelineService.h"

// Definitions
#define INITIAL_MAX_DEFAULT_PREF_FILES 10


// Prototypes
static nsresult openPrefFile(nsIFile* aFile, PRBool aIsErrorFatal,
                             PRBool aIsGlobalContext, PRBool aSkipFirstLine);


  // needed so we can still get the JS Runtime Service during XPCOM shutdown
static nsIJSRuntimeService* gJSRuntimeService = nsnull; // owning reference


/*
 * Constructor/Destructor
 */

nsPrefService::nsPrefService()
: mCurrentFile(nsnull)
{
  nsPrefBranch *rootBranch;

  NS_INIT_REFCNT();

  rootBranch = new nsPrefBranch("", PR_FALSE); 
  mRootBranch = (nsIPrefBranch *)rootBranch;

}

nsPrefService::~nsPrefService()
{
  PREF_Cleanup();
  NS_IF_RELEASE(mCurrentFile);
  NS_IF_RELEASE(gJSRuntimeService);
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
    }
  } else if (!nsCRT::strcmp(aTopic, "profile-do-change")) {
    ResetUserPrefs();
    rv = ReadUserPrefs(nsnull);
  }
  return rv;
}


NS_IMETHODIMP nsPrefService::ReadUserPrefs(nsIFile *aFile)
{
  nsresult rv;

  if (nsnull == aFile) {
    rv = UseDefaultPrefFile();  // really should return a value...
    if (NS_SUCCEEDED(rv))
      UseUserPrefFile(); 

    InformObservers(NS_PREFSERVICE_READ_TOPIC_ID);
    
    JS_MaybeGC(gMochaContext);
  } else {
    if (mCurrentFile == aFile)
      return NS_OK;

    NS_IF_RELEASE(mCurrentFile);
    mCurrentFile = aFile;
    NS_ADDREF(mCurrentFile);
    
    gErrorOpeningUserPrefs = PR_FALSE;

    rv = openPrefFile(mCurrentFile, PR_TRUE, PR_FALSE, PR_TRUE);
  }
  return rv;
}

NS_IMETHODIMP nsPrefService::ResetPrefs()
{
  InformObservers(NS_PREFSERVICE_RESET_TOPIC_ID);
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
  if (nsnull == aFile) {
    // It's possible that we never got a prefs file.
    return mCurrentFile ? SafeSavePrefFile(mCurrentFile) : NS_OK;
  } else {
    return SafeSavePrefFile(aFile);
  }
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


nsresult nsPrefService::InformObservers(const char *aTopic)
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

nsresult nsPrefService::SafeSavePrefFile(nsIFile* aFile)
{
  const char                outHeader[] = "# Mozilla User Preferences" 
                                          NS_LINEBREAK 
                                          "// This is a generated file!" 
                                          NS_LINEBREAK 
                                          NS_LINEBREAK;
  nsCOMPtr<nsIOutputStream> outStream;
  nsCOMPtr<nsIFile>         tempFile;
  PRUint32                  writeAmount;
  nsresult                  rv;

  if (!gHashTable.ops)
    return NS_ERROR_NOT_INITIALIZED;

  /* ?! Don't save (blank) user prefs if there was an error reading them */
  if (gErrorOpeningUserPrefs)
    return NS_OK;

  // execute a "safe" save by saving through a tempfile
  PRInt32 numCopies = 1;
  mRootBranch->GetIntPref("backups.number_of_prefs_copies", &numCopies);

  nsSafeSaveFile safeSave(aFile, numCopies);
  rv = safeSave.GetSaveFile(getter_AddRefs(tempFile));
  if (NS_FAILED(rv))
    return NS_ERROR_OUT_OF_MEMORY;

  char** valueArray = (char**) PR_Calloc(sizeof(char*), gHashTable.entryCount);
  if (!valueArray)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outStream), tempFile);
  if (NS_FAILED(rv)) 
      return rv;

  // write out the file header
  rv = outStream->Write(outHeader, sizeof(outHeader) - 1, &writeAmount);

  // get the lines that we're supposed to be writing to the file
  PL_DHashTableEnumerate(&gHashTable, pref_savePref, valueArray);
    
  /* Sort the preferences to make a readable file on disk */
  NS_QuickSort(valueArray, gHashTable.entryCount, sizeof(char*), pref_CompareStrings, NULL);
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

  // if save was successful replace the original file and perform backup(s) if desired
  if (NS_SUCCEEDED(rv))
    rv = safeSave.PostProcessSave();
  else
    safeSave.CleanupFailedSave();
  return rv;
}

nsresult nsPrefService::UseDefaultPrefFile()
{
  nsresult rv;
  nsCOMPtr<nsIFile> aFile;

  // Anything which calls NS_InitXPCOM will have this
  rv = NS_GetSpecialDirectory(NS_APP_PREFS_50_FILE, getter_AddRefs(aFile));

  if (!aFile) {
    // We know we have XPCOM directory services, but we might not have a provider which
    // knows about NS_APP_PREFS_50_FILE. Put the file in NS_XPCOM_CURRENT_PROCESS_DIR.
    rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, getter_AddRefs(aFile));
    if (NS_FAILED(rv)) return rv;
    rv = aFile->Append("default_prefs.js");
    if (NS_FAILED(rv)) return rv;
  } 

  rv = ReadUserPrefs(aFile);
  if (NS_SUCCEEDED(rv)) {
    return rv;
  }

  // need to save the prefs now
  SavePrefFile(aFile); 

  return rv;
}

nsresult nsPrefService::UseUserPrefFile()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFile> aFile;

  static const char* userFiles[] = {"user.js"};

  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(aFile));
  if (NS_SUCCEEDED(rv) && aFile) {
    rv = aFile->Append(userFiles[0]);
    if (NS_SUCCEEDED(rv)) {
      rv = openPrefFile(aFile, PR_FALSE, PR_FALSE, PR_FALSE);
    }
  }
  return rv;
}

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
    nsXPIDLCString str;
    aFile->GetPath(getter_Copies(str));
    NS_TIMELINE_MARK_FUNCTION1("load pref file", str.get());
  }
#endif

  rv = aFile->GetFileSize(&llFileSize);
  if (NS_FAILED(rv))
    return rv;        
  LL_L2UI(fileSize, llFileSize); // Converting 64 bit structure to unsigned int

  rv = NS_NewLocalFileInputStream(getter_AddRefs(inStr), aFile);
  if (NS_FAILED(rv)) 
    return rv;        

  readBuf = (char *)PR_Malloc(fileSize);
  if (!readBuf) 
    return NS_ERROR_OUT_OF_MEMORY;

  JS_BeginRequest(gMochaContext);

  PRUint32 amtRead = 0;
  rv = inStr->Read(readBuf, fileSize, &amtRead);
  NS_ASSERTION((amtRead == fileSize), "failed to read the entire prefs file!!");
  if (NS_SUCCEEDED(rv)) {
    if (!PREF_EvaluateConfigScript(readBuf, amtRead, nsnull, aIsGlobalContext, PR_TRUE,
                                   aSkipFirstLine))
    {
      rv = NS_ERROR_FAILURE;
      if (aIsErrorFatal)
        // If the user prefs file exists but generates an error,
        // don't clobber the file when we try to save it
        gErrorOpeningUserPrefs = PR_TRUE;
    }
  }
  PR_Free(readBuf);
  JS_EndRequest(gMochaContext);

  return rv;        
}


/*
 * some stuff that gets called from Pref_Init()
 */

/// Note: inplaceSortCallback is a small C callback stub for NS_QuickSort
static int PR_CALLBACK
inplaceSortCallback(const void *data1, const void *data2, void *privateData)
{
  char *name1 = nsnull;
  char *name2 = nsnull;
  nsIFile *file1= *(nsIFile **)data1;
  nsIFile *file2= *(nsIFile **)data2;
  nsresult rv;
  int sortResult = 0;

  rv = file1->GetLeafName(&name1);
  NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get the leaf name");
  if (NS_SUCCEEDED(rv)) {
    rv = file2->GetLeafName(&name2);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get the leaf name");
    if (NS_SUCCEEDED(rv)) {
      if (name1 && name2) {
        // we want it so foo.js will come before foo-<bar>.js
        // "foo." is before "foo-", so we have to reverse the order to accomplish
        sortResult = PL_strcmp(name2,name1);
      }
      if (name1)
        nsCRT::free((char*)name1);
      if (name2)
        nsCRT::free((char*)name2);
    }
  }
  return sortResult;
}

//----------------------------------------------------------------------------------------
JSBool pref_InitInitialObjects()
// Initialize default preference JavaScript buffers from
// appropriate TEXT resources
//----------------------------------------------------------------------------------------
{
  nsCOMPtr<nsIFile> aFile;
  nsCOMPtr<nsIFile> defaultPrefDir;
  nsresult          rv;
  PRBool            hasMoreElements;
        
  static const char* specialFiles[] = {
        "initpref.js"
#ifdef NS_DEBUG
      , "debug-developer.js"
#endif
#ifdef XP_MAC
      , "macprefs.js"
#elif defined(XP_WIN)
      , "winpref.js"
#elif defined(XP_UNIX)
      , "unix.js"
#if defined(VMS)
      , "openvms.js"
#endif
#if defined(MOZ_WIDGET_PHOTON)
	  , "photon.js"
#endif		 
#elif defined(XP_OS2)
      , "os2pref.js"
#endif
  };

  rv = NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR, getter_AddRefs(defaultPrefDir));
  if (NS_FAILED(rv))
    return JS_FALSE;

  nsIFile **defaultPrefFiles = (nsIFile **)nsMemory::Alloc(INITIAL_MAX_DEFAULT_PREF_FILES * sizeof(nsIFile *));
  int maxDefaultPrefFiles = INITIAL_MAX_DEFAULT_PREF_FILES;
  int numFiles = 0;

  // Parse all the random files that happen to be in the components directory.
  nsCOMPtr<nsISimpleEnumerator> dirIterator;
  rv = defaultPrefDir->GetDirectoryEntries(getter_AddRefs(dirIterator));
  if (!dirIterator) {
    NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Could not make a directory iterator.");
    return JS_FALSE;
  }

  dirIterator->HasMoreElements(&hasMoreElements);
  if (!hasMoreElements) {
    NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Prefs directory is empty.");
    return JS_FALSE;
  }

  // Read in initpref.js.
  // Warning: aliases get resolved, so SetLeafName will not work here.
  rv = defaultPrefDir->Clone(getter_AddRefs(aFile));
  if (NS_FAILED(rv))
    return JS_FALSE;

  rv = aFile->Append((char *)specialFiles[0]);
  if (NS_FAILED(rv))
    return JS_FALSE;

  rv = openPrefFile(aFile, PR_FALSE, PR_FALSE, PR_FALSE);
  NS_ASSERTION(NS_SUCCEEDED(rv), "initpref.js not parsed successfully");

  // Keep this child

  while (hasMoreElements) {
    PRBool shouldParse = PR_TRUE;
    char* leafName;

    dirIterator->GetNext(getter_AddRefs(aFile));
    dirIterator->HasMoreElements(&hasMoreElements);

    rv = aFile->GetLeafName(&leafName);
    if (NS_SUCCEEDED(rv)) {
      // Skip non-js files
      if (PL_strstr(leafName, ".js") + PL_strlen(".js") != leafName + PL_strlen(leafName))
        shouldParse = PR_FALSE;
      // Skip files in the special list.
      if (shouldParse) {
        for (int j = 0; j < (int) (sizeof(specialFiles) / sizeof(char *)); j++)
          if (!PL_strcmp(leafName, specialFiles[j]))
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
      if (leafName)
        nsCRT::free((char*)leafName);
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
  for (k = 1; k < (int) (sizeof(specialFiles) / sizeof(char *)); k++) {
    // we must get the directory every time so we can append the child
    // because SetLeafName will not work here.
    rv = defaultPrefDir->Clone(getter_AddRefs(aFile));
    if (NS_SUCCEEDED(rv)) {
      rv = aFile->Append((char*)specialFiles[k]);
      if (NS_SUCCEEDED(rv)) {
        rv = openPrefFile(aFile, PR_FALSE, PR_FALSE, PR_FALSE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "<platform>.js was not parsed successfully");
      }
    }
  }

  JS_MaybeGC(gMochaContext);
  return JS_TRUE;
}


JSRuntime* PREF_GetJSRuntime()
{
  nsresult rv;

  if (!gJSRuntimeService) {
    rv = CallGetService("@mozilla.org/js/xpc/RuntimeService;1",
                        &gJSRuntimeService);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsJSRuntimeService is missing");
      gJSRuntimeService = nsnull;
      return nsnull;
    }
  }

  JSRuntime* rt;
  rv = gJSRuntimeService->GetRuntime(&rt);
  if (NS_SUCCEEDED(rv))
    return rt;
  return nsnull;
}

