/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
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

#include "nsPrefService.h"
#include "jsapi.h"
#include "nsIObserverService.h"
#include "nsISignatureVerifier.h"
#include "nsPrefBranch.h"
#include "nsXPIDLString.h"
#include "nsIAutoConfig.h"

#include "nsQuickSort.h"
#include "prefapi.h"
class nsIFileSpec;	// needed for prefapi_private_data.h inclusion
#include "prefapi_private_data.h"

// supporting lock files
#include "nsDirectoryServiceDefs.h"
#include "prmem.h"
#include "prprf.h"

// supporting PREF_Init()
#include "nsIJSRuntimeService.h"
#include "nsAppDirectoryServiceDefs.h"

// lose these if possible (supporting nsIFileToFileSpec)
#include "nsIFileSpec.h"
#include "nsFileStream.h"

// Definitions
#define PREFS_HEADER_LINE_1 "# Mozilla User Preferences"
#define PREFS_HEADER_LINE_2	"// This is a generated file!"
#define INITIAL_MAX_DEFAULT_PREF_FILES 10

// Prototypes
static nsresult nsIFileToFileSpec(nsIFile* inFile, nsIFileSpec **aFileSpec);
static nsresult openPrefFile(nsIFile* aFile, PRBool aIsErrorFatal, PRBool aVerifyHash,
                        PRBool aIsGlobalContext, PRBool aSkipFirstLine);
static nsresult openPrefFileSpec(nsIFileSpec* aFilespec, PRBool aIsErrorFatal, PRBool aVerifyHash,
                                     PRBool aIsGlobalContext, PRBool aSkipFirstLine);
static nsresult savePrefFile(nsIFile* aFile);


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
  nsresult rv;

  if (!PREF_Init(nsnull))
    return NS_ERROR_FAILURE;

  rv = readConfigFile();
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIObserverService> observerService = 
           do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  if (observerService) {
    // Our refcnt must be > 0 when we call this, or we'll get deleted!
    ++mRefCnt;
    rv = observerService->AddObserver(this, NS_LITERAL_STRING("profile-before-change").get());
    if (NS_SUCCEEDED(rv)) {
      rv = observerService->AddObserver(this, NS_LITERAL_STRING("profile-do-change").get());
    }
    --mRefCnt;
  }
  return(rv);
}

NS_IMETHODIMP nsPrefService::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
  nsresult rv = NS_OK;

  if (!nsCRT::strcmp(aTopic, NS_LITERAL_STRING("profile-before-change").get())) {
    if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("shutdown-cleanse").get())) {
      if (mCurrentFile) {
        mCurrentFile->Remove(PR_FALSE);
        NS_RELEASE(mCurrentFile);
      }
    } else {
      rv = SavePrefFile(nsnull);
    }
  } else if (!nsCRT::strcmp(aTopic, NS_LITERAL_STRING("profile-do-change").get())) {
    ResetUserPrefs();
    rv = ReadUserPrefs(nsnull);
  }
  return rv;
}


NS_IMETHODIMP nsPrefService::ReadUserPrefs(nsIFile *aFile)
{
  nsresult rv;

  if (nsnull == aFile) {
    rv = useDefaultPrefFile();  // really should return a value...
    if (NS_SUCCEEDED(rv))
      useUserPrefFile(); 

      JS_MaybeGC(gMochaContext);
  } else {
    if (mCurrentFile == aFile)
      return NS_OK;

    NS_IF_RELEASE(mCurrentFile);
    mCurrentFile = aFile;
    NS_ADDREF(mCurrentFile);
    
    gErrorOpeningUserPrefs = PR_FALSE;

    rv = openPrefFile(mCurrentFile, PR_TRUE, PR_FALSE, PR_FALSE, PR_TRUE);
  }
  return rv;
}

NS_IMETHODIMP nsPrefService::ResetPrefs()
{
  nsresult rv;

  PREF_CleanupPrefs();

  if (!PREF_Init(nsnull))
    return NS_ERROR_FAILURE;

  rv = readConfigFile();
  return rv;
}

NS_IMETHODIMP nsPrefService::ResetUserPrefs()
{
  PREF_ClearAllUserPrefs();
  return NS_OK;    
}

/* void savePrefFile (in nsIFile filename); */
NS_IMETHODIMP nsPrefService::SavePrefFile(nsIFile *aFile)
{
  if (nsnull == aFile) {
    // It's possible that we never got a prefs file.
    return mCurrentFile ? savePrefFile(mCurrentFile) : NS_OK;
  } else {
    return savePrefFile(aFile);
  }
}

NS_IMETHODIMP nsPrefService::GetBranch(const char *aPrefRoot, nsIPrefBranch **_retval)
{
  nsresult rv;

  if ((nsnull != aPrefRoot) && (aPrefRoot != "")) {
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

NS_IMETHODIMP nsPrefService::AddObserver(const char *aDomain, nsIObserver *aObserver)
{
  nsresult rv;

  nsCOMPtr<nsIPrefBranchInternal> prefBranch = do_QueryInterface(mRootBranch, &rv);
  if (NS_SUCCEEDED(rv))
    rv = prefBranch->AddObserver(aDomain, aObserver);
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


nsresult nsPrefService::readConfigFile()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFile> lockPrefFile;
  nsXPIDLCString lockFileName;
  nsXPIDLCString lockVendor;
    
  /*
   * This preference is set in the all.js or all-ns.js (depending whether 
   * running mozilla or netscp6) default - preference is commented out, so 
   * it doesn't exist.
   */
  rv = mRootBranch->GetCharPref("general.config.filename", 
                                getter_Copies(lockFileName));
  if (NS_FAILED(rv)) {
    /*
     * if we got a "PREF_ERROR" back, the pref doesn't exist so, we ignore it
     * - PREF_ERROR is converted to NS_ERROR_UNEXPECTED in _convertRes()
     */
   if (rv == NS_ERROR_UNEXPECTED)
      rv = NS_OK;
    return rv;
  }

  rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, 
                              getter_AddRefs(lockPrefFile));
  if (NS_SUCCEEDED(rv)) {

#ifdef XP_MAC
    lockPrefFile->Append("Essential Files");
#endif

    rv = lockPrefFile->Append(lockFileName);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;

    if (NS_FAILED(openPrefFile(lockPrefFile, PR_FALSE, PR_TRUE, 
                               PR_FALSE, PR_TRUE)))
      return NS_ERROR_FAILURE;
      // failure here means problem within the config file script
  }

  /*
   * Once the config file is read, we should check that the vendor name 
   * is consistent By checking for the vendor name after reading the config 
   * file we allow for the preference to be set (and locked) by the creator 
   * of the cfg file meaning the file can not be renamed (successfully).
   */
  rv = mRootBranch->GetCharPref("general.config.filename", 
                                getter_Copies(lockFileName));
  if (NS_FAILED(rv))         // There is NO REASON this should fail.
    return NS_ERROR_FAILURE;

  rv = mRootBranch->GetCharPref("general.config.vendor", 
                                getter_Copies(lockVendor));

  /*
   * If the "vendor" preference exists, do this simple check to add a
   * level of security, albeit a small one, to the validation of the
   * contents of the .cfg file.
   */
  if (NS_SUCCEEDED(rv)) {
    /*
     * lockVendor and lockFileName should be the same with the addition of 
     * .cfg to the filename.  By checking this post reading of the cfg file 
     * this value can, and should, be set within the cfg file adding a level
     * of security.
     */

    PRUint32 fileNameLen = PL_strlen(lockFileName);

    if (PL_strncmp(lockFileName, lockVendor, fileNameLen -4) != 0)
      return NS_ERROR_FAILURE;
  }
  
  // get the value of the autoconfig url
  nsXPIDLCString urlName;
  rv = mRootBranch->GetCharPref("autoadmin.global_config_url",
                                getter_Copies(urlName));
  if (NS_SUCCEEDED(rv) && *urlName != '\0' ) {  
    
    // Instantiating nsAutoConfig object if the pref is present
    nsCOMPtr<nsIAutoConfig> autocfg = do_CreateInstance(NS_AUTOCONFIG_CONTRACTID, &rv);
    if (NS_FAILED(rv))
      return NS_ERROR_OUT_OF_MEMORY;
    rv = autocfg->SetConfigURL(urlName);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}


nsresult nsPrefService::useDefaultPrefFile()
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
  rv = SavePrefFile(aFile); 

  return rv;
}

nsresult nsPrefService::useUserPrefFile()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFile> aFile;

  static const char* userFiles[] = {"user.js"};

  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(aFile));
  if (NS_SUCCEEDED(rv) && aFile) {
    rv = aFile->Append(userFiles[0]);
    if (NS_SUCCEEDED(rv)) {
      rv = openPrefFile(aFile, PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE);
    }
  }
  return rv;
}

static nsresult openPrefFile(nsIFile* aFile, PRBool aIsErrorFatal, PRBool aVerifyHash,
                                     PRBool aIsGlobalContext, PRBool aSkipFirstLine)
{
  nsCOMPtr<nsIFileSpec> fileSpec;
  nsresult rv;

  rv = nsIFileToFileSpec(aFile, getter_AddRefs(fileSpec));
  if (NS_SUCCEEDED(rv)) {
    JS_BeginRequest(gMochaContext);
    rv = openPrefFileSpec(fileSpec, aIsErrorFatal, aVerifyHash, aIsGlobalContext, aSkipFirstLine);
    JS_EndRequest(gMochaContext);
  }
  return rv;        
}

static nsresult openPrefFileSpec(nsIFileSpec* aFilespec, PRBool aIsErrorFatal, PRBool aVerifyHash,
                                     PRBool aIsGlobalContext, PRBool aSkipFirstLine)
{
  nsresult rv;
  char* readBuf;

  // TODO: Validate this entire function, I seriously doubt it does what it is 
  //       supposed to. Note for instance that gErrorOpeningUserPrefs will only
  //       be set if the evaluation of the config script fails AND aIsErrorFatal
  //       is set to PR_TRUE... the readBuf test is irrelavent because it will
  //       bail at the GetFileContents call if it fails.

  // TODO: Convert the rest of this code to nsIFile and avoid this conversion to nsIFileSpec
  rv = aFilespec->ResolveSymlink();
  if (NS_FAILED(rv))
    return rv;

  if (!Exists(aFilespec))
    return NS_ERROR_FILE_NOT_FOUND;

  rv = aFilespec->GetFileContents(&readBuf);
  if (NS_FAILED(rv))
    return rv;

  long fileLength = PL_strlen(readBuf);
  if (aVerifyHash) {
    const int obscure_value = 13;
    // Unobscure file by subtracting some value from every char - old value was 7
    long i;
    for (i = 0; i < fileLength; i++)
      readBuf[i] -= obscure_value;
  }

  if (NS_SUCCEEDED(rv)) {
    if (!PREF_EvaluateConfigScript(readBuf, fileLength, nsnull, aIsGlobalContext, PR_TRUE,
                                   aSkipFirstLine))
    {
      rv = NS_ERROR_FAILURE;
      if (aVerifyHash) {
        PR_Free(readBuf);
        return rv;
      }
    }
  }
  PR_Free(readBuf);

  // If the user prefs file exists but generates an error,
  // don't clobber the file when we try to save it
  if ((!readBuf || rv != NS_OK) && aIsErrorFatal)
    gErrorOpeningUserPrefs = PR_TRUE;

  return rv;
}

static nsresult savePrefFile(nsIFile* aFile)
{
  nsresult              rv;
  nsCOMPtr<nsIFileSpec> fileSpec;

  if (!gHashTable)
    return NS_ERROR_NOT_INITIALIZED;

  /* ?! Don't save (blank) user prefs if there was an error reading them */
  if (gErrorOpeningUserPrefs)
    return NS_OK;

  // TODO: Convert the rest of this code to nsIFile and avoid this conversion to nsIFileSpec
  rv = nsIFileToFileSpec(aFile, getter_AddRefs(fileSpec));
  if (NS_FAILED(rv))
    return rv;        

  char** valueArray = (char**) PR_Calloc(sizeof(char*), gHashTable->nentries);
  if (!valueArray)
    return NS_ERROR_OUT_OF_MEMORY;

  nsOutputFileStream stream(fileSpec);
  if (!stream.is_open())
    return NS_BASE_STREAM_OSERROR;

  stream << PREFS_HEADER_LINE_1 << nsEndl << PREFS_HEADER_LINE_2 << nsEndl << nsEndl;
    
  /* LI_STUFF here we pass in the heSaveProc proc used so that li can do its own thing */
  PR_HashTableEnumerateEntries(gHashTable, (PLHashEnumerator)pref_savePref, valueArray);
    
  /* Sort the preferences to make a readable file on disk */
  NS_QuickSort(valueArray, gHashTable->nentries, sizeof(char*), pref_CompareStrings, NULL);
  char** walker = valueArray;
  for (PRUint32 valueIdx = 0; valueIdx < gHashTable->nentries; valueIdx++, walker++) {
    if (*walker) {
      stream << *walker << nsEndl;
      PR_Free(*walker);
    }
  }
  PR_Free(valueArray);
  fileSpec->CloseStream();
  return NS_OK;
}


//----------------------------------------------------------------------------------------
// So discouraged is the use of nsIFileSpec, nobody wanted to have this routine be
// public - It might lead to continued use of nsIFileSpec. Right now, this code has
// such a need for it, here it is. Let's stop having to use it though.
static nsresult nsIFileToFileSpec(nsIFile* inFile, nsIFileSpec **aFileSpec)
//----------------------------------------------------------------------------------------
{
   nsresult rv;
   nsCOMPtr<nsIFileSpec> newFileSpec;
   nsXPIDLCString pathBuf;
   
   rv = inFile->GetPath(getter_Copies(pathBuf));
   if (NS_FAILED(rv)) return rv;
   rv = NS_NewFileSpec(getter_AddRefs(newFileSpec));
   if (NS_FAILED(rv)) return rv;
   rv = newFileSpec->SetNativePath((const char *)pathBuf);
   if (NS_FAILED(rv)) return rv;
   
   *aFileSpec = newFileSpec;
   NS_ADDREF(*aFileSpec);
   
   return NS_OK;
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
  nsIFileSpec *file1= *(nsIFileSpec **)data1;
  nsIFileSpec *file2= *(nsIFileSpec **)data2;
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
extern "C" JSBool pref_InitInitialObjects()
// Initialize default preference JavaScript buffers from
// appropriate TEXT resources
//----------------------------------------------------------------------------------------
{
  nsresult rv;
  PRBool exists;
  nsCOMPtr<nsIFile> aFile;
  nsCOMPtr <nsIFileSpec> defaultPrefDir;
        
  rv = NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR, getter_AddRefs(aFile));
  if (NS_FAILED(rv))
    return JS_FALSE;
  rv = nsIFileToFileSpec(aFile, getter_AddRefs(defaultPrefDir));
  if (NS_FAILED(rv))
    return JS_FALSE;

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
#elif defined(XP_OS2)
      , "os2pref.js"
#endif
  };

  int k=0;
  nsIFileSpec **defaultPrefFiles = (nsIFileSpec **)nsMemory::Alloc(INITIAL_MAX_DEFAULT_PREF_FILES * sizeof(nsIFileSpec *));
  int maxDefaultPrefFiles = INITIAL_MAX_DEFAULT_PREF_FILES;
  int numFiles = 0;

  // Parse all the random files that happen to be in the components directory.
  nsCOMPtr<nsIDirectoryIterator> dirIterator;
  rv = nsComponentManager::CreateInstance(
        (const char*)NS_DIRECTORYITERATOR_CONTRACTID,
        (nsISupports*)nsnull,
        (const nsID&)NS_GET_IID(nsIDirectoryIterator),
        getter_AddRefs(dirIterator));
  NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Could not make a directory iterator.");
  if (!dirIterator || NS_FAILED(dirIterator->Init(defaultPrefDir, PR_TRUE)))
    return JS_FALSE;

  // Get any old child of the components directory. Warning: aliases get resolved, so
  // SetLeafName will not work here.
  nsCOMPtr<nsIFile> aFile2;

  rv = NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR, getter_AddRefs(aFile2));
  if (NS_FAILED(rv))
    	return JS_TRUE;

  rv = aFile2->Append((char *)specialFiles[0]);
  if (NS_FAILED(rv))
    return JS_TRUE;

  if (NS_FAILED(aFile2->Exists(&exists)))
  {
    return JS_FALSE;
  }

  if (exists)
  {
    rv = openPrefFile(aFile2, PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "initpref.js not parsed successfully");
  }
  // Keep this child

#ifdef DEBUG_prefs
  printf("Parsing default JS files.\n");
#endif /* DEBUG_prefs */
  for (; Exists(dirIterator); dirIterator->Next())
  {
    nsCOMPtr<nsIFileSpec> child;
    PRBool shouldParse = PR_TRUE;
    if NS_FAILED(dirIterator->GetCurrentSpec(getter_AddRefs(child)))
      continue;
    char* leafName;
    rv = child->GetLeafName(&leafName);
    if (NS_SUCCEEDED(rv))
    {
      // Skip non-js files
      if (PL_strstr(leafName, ".js") + PL_strlen(".js") != leafName + PL_strlen(leafName))
        shouldParse = PR_FALSE;
      // Skip files in the special list.
      if (shouldParse)
      {
        for (int j = 0; j < (int) (sizeof(specialFiles) / sizeof(char*)); j++)
          if (PL_strcmp(leafName, specialFiles[j]) == 0)
            shouldParse = PR_FALSE;
      }
      if (shouldParse)
      {
#ifdef DEBUG_prefs
        printf("Adding %s to the list to be sorted\n", leafName);
#endif /* DEBUG_prefs */
        rv = NS_NewFileSpec(&(defaultPrefFiles[numFiles]));
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create a file spec");
        if (NS_SUCCEEDED(rv) && defaultPrefFiles[numFiles]) {
          rv = defaultPrefFiles[numFiles]->FromFileSpec(child);
          NS_ASSERTION(NS_SUCCEEDED(rv),"failed to set the spec");
          if (NS_SUCCEEDED(rv)) {
            numFiles++;
          }
          if (numFiles == maxDefaultPrefFiles) {
            // double the size of the array
            maxDefaultPrefFiles *= 2;
            defaultPrefFiles = (nsIFileSpec **)nsMemory::Realloc(defaultPrefFiles, maxDefaultPrefFiles * sizeof(nsIFileSpec *));
          }
        }
      }
      if (leafName) nsCRT::free((char*)leafName);
    }
  }
#ifdef DEBUG_prefs
  printf("Sort defaultPrefFiles.  we need them sorted so all-ns.js will override all.js (where override == parsed later)\n");
#endif /* DEBUG_prefs */
  NS_QuickSort((void *)defaultPrefFiles, numFiles,sizeof(nsIFileSpec *), inplaceSortCallback, nsnull);

  for (k=0;k<numFiles;k++) {
    char* currentLeafName = nsnull;
    if (defaultPrefFiles[k]) {
      rv = defaultPrefFiles[k]->GetLeafName(&currentLeafName);
#ifdef DEBUG_prefs
      printf("Parsing %s\n", currentLeafName);
#endif /* DEBUG_prefs */
      if (currentLeafName) nsCRT::free((char*)currentLeafName);

      if (NS_SUCCEEDED(rv)) {
        rv = openPrefFileSpec(defaultPrefFiles[k], PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Config file not parsed successfully");
      }
    }
  }
  for (k=0;k<numFiles;k++) {
    NS_IF_RELEASE(defaultPrefFiles[k]);
  }
  nsMemory::Free(defaultPrefFiles);
  defaultPrefFiles = nsnull;

#ifdef DEBUG_prefs
  printf("Parsing platform-specific JS files.\n");
#endif /* DEBUG_prefs */
  nsCOMPtr<nsIFile> aFile3;

  // Finally, parse any other special files (platform-specific ones).
  for (k = 1; k < (int) (sizeof(specialFiles) / sizeof(char*)); k++) {
    // we must get the directory every time so we can append the child
    // because SetLeafName will not work here.
    rv = NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR, getter_AddRefs(aFile3));
    if (NS_SUCCEEDED(rv)) {
      rv = aFile3->Append((char*)specialFiles[k]);
      if (NS_SUCCEEDED(rv)) {
#ifdef DEBUG_prefs
        printf("Parsing %s\n", specialFiles[k]);
#endif /* DEBUG_prefs */
        if (NS_SUCCEEDED(aFile3->Exists(&exists)) && exists) {
          rv = openPrefFile(aFile3, PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE);
          NS_ASSERTION(NS_SUCCEEDED(rv), "<platform>.js was not parsed successfully");
        }
      }
    }
  }

  JS_MaybeGC(gMochaContext);
  return JS_TRUE;
}


extern "C" JSRuntime* PREF_GetJSRuntime()
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

