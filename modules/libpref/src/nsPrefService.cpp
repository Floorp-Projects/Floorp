/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

#include "nsPrefService.h"
#include "jsapi.h"
#include "nsIGenericFactory.h"
#include "nsIObserverService.h"
#include "nsIProfileChangeStatus.h"
#include "nsISignatureVerifier.h"
#include "nsPrefBranch.h"
#include "nsXPIDLString.h"

#include "nsQuickSort.h"
#include "prefapi.h"
class nsIFileSpec;	// needed for prefapi_private_data.h inclusion
#include "prefapi_private_data.h"

// supporting lock files
#include "nsDirectoryServiceDefs.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"

// supporting PREF_Init()
#include "nsIJSRuntimeService.h"
#include "nsAppDirectoryServiceDefs.h"

// lose these if possible (supporting _nsIFileToFileSpec)
#include "nsIFileSpec.h"
#include "nsFileStream.h"

// Definitions
#define PREFS_HEADER_LINE_1 "# Mozilla User Preferences"
#define PREFS_HEADER_LINE_2	"// This is a generated file!"
#define INITIAL_MAX_DEFAULT_PREF_FILES 10

// Prototypes
static nsresult _nsIFileToFileSpec(nsIFile* inFile, nsIFileSpec **aFileSpec);
static PRBool   verifyFileHash(char* buf, long buflen);
static nsresult openPrefFile(nsIFile* aFile, PRBool aIsErrorFatal, PRBool aVerifyHash,
                        PRBool aIsGlobalContext, PRBool aSkipFirstLine);
static nsresult openPrefFileSpec(nsIFileSpec* aFilespec, PRBool aIsErrorFatal, PRBool aVerifyHash,
                                     PRBool aIsGlobalContext, PRBool aSkipFirstLine);
static nsresult savePrefFile(nsIFile* aFile);



NS_IMPL_ISUPPORTS3(nsPrefService, nsIPrefService, nsIObserver, nsIPrefBranch);

nsPrefService::nsPrefService()
{
  nsPrefBranch *rootBranch;

  NS_INIT_REFCNT();

  rootBranch = new nsPrefBranch("", FALSE); 
  mRootBranch = (nsIPrefBranch *)rootBranch;

}

nsPrefService::~nsPrefService()
{
  PREF_Cleanup();

  if (gLockInfoRead) {
    if (gLockFileName) {
      PR_DELETE(gLockFileName);
    }
    if (gLockVendor) {
      PR_DELETE(gLockVendor);
    }
    gLockInfoRead = PR_FALSE;
  }
}

NS_IMETHODIMP nsPrefService::Init()
{
  nsresult rv;

  if (PREF_Init(nsnull) == NS_OK) {
    NS_WITH_SERVICE(nsIObserverService, observerService, NS_OBSERVERSERVICE_CONTRACTID, &rv);
    if (observerService) {
      // Our refcnt must be > 0 when we call this, or we'll get deleted!
      ++mRefCnt;
      rv = observerService->AddObserver(this, NS_LITERAL_STRING("profile-before-change").get());
      if (NS_SUCCEEDED(rv)) {
        rv = observerService->AddObserver(this, NS_LITERAL_STRING("profile-do-change").get());
      }
      --mRefCnt;
    }
  } else {
    rv = NS_ERROR_FAILURE;
  }
  return(rv);
}

NS_IMETHODIMP nsPrefService::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
  nsresult rv = NS_OK;

  if (!nsCRT::strcmp(aTopic, NS_LITERAL_STRING("profile-before-change").get())) {
    if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("shutdown-cleanse").get())) {
      if (mCurrentFile) {
        mCurrentFile->Delete(PR_FALSE);
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

NS_IMETHODIMP nsPrefService::ReadCFGFile()
{
  nsresult rv = NS_OK;
  PRUint32 fileNameLen;
  PRUint32 vendorLen;

  gLockFileName = nsnull;
  gLockVendor = nsnull;

  if (gLockInfoRead)
    return rv;

  gLockInfoRead = PR_TRUE;

  rv = mRootBranch->GetCharPref("general.config.filename", &gLockFileName);
  if (NS_SUCCEEDED(rv) && (gLockFileName)) {
#ifdef NS_DEBUG
    printf("\ngLockFileName %s \n", gLockFileName);
#endif
    rv = mRootBranch->GetCharPref("general.config.vendor", &gLockVendor);
    if (NS_SUCCEEDED(rv) && (gLockVendor)) {
      // if we got here, we have a lockfile pref and a vendor pref
#ifdef NS_DEBUG
      printf("\ngLockVendor %s \n", gLockVendor);
#endif
      fileNameLen = PL_strlen(gLockFileName);
      vendorLen = PL_strlen(gLockVendor);
      if (PL_strncmp(gLockFileName, gLockVendor, fileNameLen -4) != 0) {
        // configuration filename and vendor name do not match
        rv = NS_ERROR_FAILURE;
      } else {
        nsCOMPtr<nsIFile> lockFile; 

        rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, getter_AddRefs(lockFile));
        if (NS_SUCCEEDED(rv)) {
#ifdef XP_MAC
          rv = lockFile->Append("Essential Files");
#endif
          rv = lockFile->Append(gLockFileName);
          rv = openPrefFile(lockFile, PR_TRUE, PR_TRUE, PR_FALSE, PR_FALSE);
        }
      }
    } else {
      // No vendor name specified, hence cannot verify it
      rv = NS_ERROR_FAILURE;
    }
  } else {
    // config file is not specified, hence no need to read it.
    rv = NS_OK;
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
  rv = PREF_Init(nsnull);
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
    return savePrefFile(mCurrentFile);
  } else {
    return savePrefFile(aFile);
  }
}

NS_IMETHODIMP nsPrefService::GetBranch(const char *aPrefRoot, nsIPrefBranch **_retval)
{
  nsresult rv;

  // TODO: - cache this stuff and allow consumers to share branches (hold weak references I think)
  nsPrefBranch* prefBranch = new nsPrefBranch(aPrefRoot, PR_FALSE);

  rv = prefBranch->QueryInterface(NS_GET_IID(nsIPrefBranch), (void **)_retval);
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


#pragma mark -

static nsresult openPrefFile(nsIFile* aFile, PRBool aIsErrorFatal, PRBool aVerifyHash,
                                     PRBool aIsGlobalContext, PRBool aSkipFirstLine)
{
  nsCOMPtr<nsIFileSpec> fileSpec;
  nsresult rv;

  rv = _nsIFileToFileSpec(aFile, getter_AddRefs(fileSpec));
  if (NS_FAILED(rv))
    return rv;        

  return openPrefFileSpec(fileSpec, aIsErrorFatal, aVerifyHash, aIsGlobalContext, aSkipFirstLine);
}

static nsresult openPrefFileSpec(nsIFileSpec* aFilespec, PRBool aIsErrorFatal, PRBool aVerifyHash,
                                     PRBool aIsGlobalContext, PRBool aSkipFirstLine)
{
  nsresult rv;
  char* readBuf;

  // TODO: Validate this entire function, I seriously doubt it does what it is supposed to.
  //       Note for instance that gErrorOpeningUserPrefs will only be set if the evaluation
  //       of the config script fails AND aIsErrorFatal is set TRUE... the readBuf test is
  //       irrelavent because it will bail at the GetFileContents call if it fails.

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
  if (aVerifyHash && !verifyFileHash(readBuf, fileLength)) {
    rv = NS_ERROR_FAILURE;
    if (aIsErrorFatal) {
      PR_Free(readBuf);
      return rv;
    }
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
  rv = _nsIFileToFileSpec(aFile, getter_AddRefs(fileSpec));
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

#pragma mark -

//----------------------------------------------------------------------------------------
// So discouraged is the use of nsIFileSpec, nobody wanted to have this routine be
// public - It might lead to continued use of nsIFileSpec. Right now, this code has
// such a need for it, here it is. Let's stop having to use it though.
static nsresult _nsIFileToFileSpec(nsIFile* inFile, nsIFileSpec **aFileSpec)
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

//----------------------------------------------------------------------------------------
// Computes the MD5 hash of the given buffer (not including the first line)
//   and verifies the first line of the buffer expresses the correct hash in the form:
//   // xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx
//   where each 'xx' is a hex value. */
//----------------------------------------------------------------------------------------
PRBool verifyFileHash(char* buf, long buflen)
//----------------------------------------------------------------------------------------
{

  PRBool success = PR_FALSE;
  const int obscure_value = 7;
  const long hash_length = 51;		/* len = 48 chars of MD5 + // + EOL */
  unsigned char *digest;
  char szHash[64];
   
  /* Unobscure file by subtracting some value from every char. */
  long i;
  for (i = 0; i < buflen; i++)
    buf[i] -= obscure_value;

  if (buflen >= hash_length) {
    const unsigned char magic_key[] = "VonGloda5652TX75235ISBN";
    unsigned char *pStart = (unsigned char*) buf + hash_length;
    unsigned int len;
       
    nsresult rv;

    NS_WITH_SERVICE(nsISignatureVerifier, verifier, SIGNATURE_VERIFIER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return success; // No signature verifier available
       
    //-- Calculate the digest
    PRUint32 id;
    rv = verifier->HashBegin(nsISignatureVerifier::MD5, &id);
    if (NS_FAILED(rv)) return success;

    rv = verifier->HashUpdate(id, (const char*)magic_key, sizeof(magic_key));
    if (NS_FAILED(rv)) return success;

    rv = verifier->HashUpdate(id, (const char*)pStart, (unsigned int)(buflen - hash_length));
    if (NS_FAILED(rv)) return success;
  
    digest = (unsigned char*)PR_MALLOC(nsISignatureVerifier::MD5_LENGTH);
    if (digest == nsnull) return success;

    rv = verifier->HashEnd(id, &digest, &len, nsISignatureVerifier::MD5_LENGTH);
    if (NS_FAILED(rv)) { PR_FREEIF(digest); return success; }

    PR_snprintf(szHash, 64, "%x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
    (int)digest[0],(int)digest[1],(int)digest[2],(int)digest[3],
    (int)digest[4],(int)digest[5],(int)digest[6],(int)digest[7],
    (int)digest[8],(int)digest[9],(int)digest[10],(int)digest[11],
    (int)digest[12],(int)digest[13],(int)digest[14],(int)digest[15]);

    success = ( PL_strncmp((const char*) buf + 3, szHash, (PRUint32)(hash_length - 4)) == 0 );

    PR_FREEIF(digest);
 
  }
  return success;
}

#pragma mark -

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
  rv = _nsIFileToFileSpec(aFile, getter_AddRefs(defaultPrefDir));
  if (NS_FAILED(rv))
    return JS_FALSE;

  static const char* specialFiles[] = {
        "initpref.js"
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

  rv = NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR, getter_AddRefs(aFile3));
  if (NS_SUCCEEDED(rv)) {
    // Finally, parse any other special files (platform-specific ones).
    for (k = 1; k < (int) (sizeof(specialFiles) / sizeof(char*)); k++)
    {
      rv = aFile3->SetLeafName((char*)specialFiles[k]);
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

done:
  JS_MaybeGC(gMochaContext);
  return JS_TRUE;
}


extern "C" JSRuntime* PREF_GetJSRuntime()
{
  nsresult rv;

  NS_WITH_SERVICE(nsIJSRuntimeService, rtsvc, "@mozilla.org/js/xpc/RuntimeService;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    JSRuntime* rt;
    rv = rtsvc->GetRuntime(&rt);
    if (NS_SUCCEEDED(rv)) {
      return rt;
    }
  }
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsJSRuntimeService is missing");
  return nsnull;
}

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrefService, Init)