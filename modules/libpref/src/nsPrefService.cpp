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
#include "nsCOMArray.h"

#include "nsQuickSort.h"
#include "prmem.h"
#include "pldhash.h"

#include "prefapi.h"
#include "prefread.h"
#include "prefapi_private_data.h"

// supporting PREF_Init()
#include "nsIJSRuntimeService.h"

#include "nsITimelineService.h"

#ifdef MOZ_PROFILESHARING
#include "nsIProfileSharingSetup.h"
#include "nsSharedPrefHandler.h"
#endif

// Definitions
#define INITIAL_PREF_FILES 10
#define PREF_READ_BUFFER_SIZE 4096

// Prototypes
#ifdef MOZ_PROFILESHARING
static PRBool isSharingEnabled();
#endif

static nsresult openPrefFile(nsIFile* aFile);
static nsresult pref_InitInitialObjects(void);

//-----------------------------------------------------------------------------

/*
 * Constructor/Destructor
 */

nsPrefService::nsPrefService()
: mErrorOpeningUserPrefs(PR_FALSE)
#if MOZ_PROFILESHARING
  , mErrorOpeningSharedUserPrefs(PR_FALSE)
#endif
{
}

nsPrefService::~nsPrefService()
{
  PREF_Cleanup();

#ifdef MOZ_PROFILESHARING
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
  nsPrefBranch *rootBranch = new nsPrefBranch("", PR_FALSE); 
  if (!rootBranch)
    return NS_ERROR_OUT_OF_MEMORY;

  mRootBranch = (nsIPrefBranchInternal *)rootBranch;
  
  nsXPIDLCString lockFileName;
  nsresult rv;

  rv = PREF_Init();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = pref_InitInitialObjects();
  NS_ENSURE_SUCCESS(rv, rv);

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
        mCurrentFile = nsnull;
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

  nsresult rv = PREF_Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return pref_InitInitialObjects();
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
    if (!prefBranch)
      return NS_ERROR_OUT_OF_MEMORY;

    rv = CallQueryInterface(prefBranch, _retval);
  } else {
    // special case caching the default root
    rv = CallQueryInterface(mRootBranch, _retval);
  }
  return rv;
}

NS_IMETHODIMP nsPrefService::GetDefaultBranch(const char *aPrefRoot, nsIPrefBranch **_retval)
{
  nsresult rv;

  // TODO: - cache this stuff and allow consumers to share branches (hold weak references I think)
  nsPrefBranch* prefBranch = new nsPrefBranch(aPrefRoot, PR_TRUE);
  if (!prefBranch)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = CallQueryInterface(prefBranch, _retval);
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
      rv = openPrefFile(aFile);
    }
  }
  return rv;
}

nsresult nsPrefService::ReadAndOwnUserPrefFile(nsIFile *aFile)
{
  NS_ENSURE_ARG(aFile);
  
  if (mCurrentFile == aFile)
    return NS_OK;
  mCurrentFile = aFile;

#ifdef MOZ_PROFILESHARING
  // We don't want prefs set here to cause transactions
  gSharedPrefHandler->ReadingUserPrefs(PR_TRUE);
#endif

  // We need to track errors in reading the shared and the
  // non-shared files independently. 
  // Set the appropriate member variable from it after reading.
  nsresult rv = openPrefFile(mCurrentFile);
  mErrorOpeningUserPrefs = NS_FAILED(rv);

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
  mCurrentSharedFile = aFile;

#ifdef MOZ_PROFILESHARING
  // We don't want prefs set here to cause transactions
  gSharedPrefHandler->ReadingUserPrefs(PR_TRUE);
#endif

  // We need to track errors in reading the shared and the
  // non-shared files independently. 
  // Set the appropriate member variable from it after reading.
  nsresult rv = openPrefFile(mCurrentSharedFile);
  mErrorOpeningSharedUserPrefs = NS_FAILED(rv);

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

  nsCOMPtr<nsIOutputStream> outStreamSink;
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
  rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(outStreamSink),
                                       aFile);
  if (NS_FAILED(rv)) 
      return rv;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(outStream), outStreamSink, 4096);
  if (NS_FAILED(rv)) 
      return rv;  

  char** valueArray = (char **)PR_Calloc(sizeof(char *), gHashTable.entryCount);
  if (!valueArray)
    return NS_ERROR_OUT_OF_MEMORY;
  
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
  
  // write out the file header
  outStream->Write(outHeader, sizeof(outHeader) - 1, &writeAmount);

  char** walker = valueArray;
  for (PRUint32 valueIdx = 0; valueIdx < gHashTable.entryCount; valueIdx++, walker++) {
    if (*walker) {
      outStream->Write(*walker, strlen(*walker), &writeAmount);
      outStream->Write(NS_LINEBREAK, NS_LINEBREAK_LEN, &writeAmount);
      PR_Free(*walker);
    }
  }
  PR_Free(valueArray);

  // tell the safe output stream to overwrite the real prefs file
  // (it'll abort if there were any errors during writing)
  nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(outStream);
  NS_ASSERTION(safeStream, "expected a safe output stream!");
  if (safeStream) {
    rv = safeStream->Finish();
    if (NS_FAILED(rv)) {
      NS_WARNING("failed to save prefs file! possible dataloss");
      return rv;
    }
  }

  gDirty = PR_FALSE;
  return NS_OK;
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

static nsresult openPrefFile(nsIFile* aFile)
{
  nsCOMPtr<nsIInputStream> inStr;
  char      readBuf[PREF_READ_BUFFER_SIZE];

#if MOZ_TIMELINE
  {
    nsCAutoString str;
    aFile->GetNativePath(str);
    NS_TIMELINE_MARK_FUNCTION1("load pref file", str.get());
  }
#endif

  nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(inStr), aFile);
  if (NS_FAILED(rv)) 
    return rv;        

  PrefParseState ps;
  PREF_InitParseState(&ps, PREF_ReaderCallback, NULL);
  for (;;) {
    PRUint32 amtRead = 0;
    rv = inStr->Read(readBuf, sizeof(readBuf), &amtRead);
    if (NS_FAILED(rv) || amtRead == 0)
      break;

    PREF_ParseBuf(&ps, readBuf, amtRead); 
  }
  PREF_FinalizeParseState(&ps);
  return rv;        
}

/*
 * some stuff that gets called from Pref_Init()
 */

static int
pref_CompareFileNames(nsIFile* aFile1, nsIFile* aFile2, void* /*unused*/)
{
  nsCAutoString filename1, filename2;
  aFile1->GetNativeLeafName(filename1);
  aFile2->GetNativeLeafName(filename2);

  return Compare(filename2, filename1);
}

/**
 * Load default pref files from a directory. The files in the
 * directory are sorted reverse-alphabetically; a set of "special file
 * names" may be specified which are loaded after all the others.
 */
static nsresult
pref_LoadPrefsInDir(nsIFile* aDir, char const *const *aSpecialFiles, PRUint32 aSpecialFilesCount)
{
  nsresult rv, rv2;
  PRBool hasMoreElements;

  nsCOMPtr<nsISimpleEnumerator> dirIterator;

  // this may fail in some normal cases, such as embedders who do not use a GRE
  rv = aDir->GetDirectoryEntries(getter_AddRefs(dirIterator));
  if (NS_FAILED(rv)) return rv;

  rv = dirIterator->HasMoreElements(&hasMoreElements);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMArray<nsIFile> prefFiles(INITIAL_PREF_FILES);
  nsCOMArray<nsIFile> specialFiles(aSpecialFilesCount);
  nsCOMPtr<nsIFile> prefFile;

  while (hasMoreElements && NS_SUCCEEDED(rv)) {
    nsCAutoString leafName;

    rv = dirIterator->GetNext(getter_AddRefs(prefFile));
    if (NS_FAILED(rv)) {
      break;
    }

    prefFile->GetNativeLeafName(leafName);
    NS_ASSERTION(!leafName.IsEmpty(), "Failure in default prefs: directory enumerator returned empty file?");

    // Skip non-js files
    if (StringEndsWith(leafName, NS_LITERAL_CSTRING(".js"))) {
      PRBool shouldParse = PR_TRUE;
      // separate out special files
      for (PRUint32 i = 0; i < aSpecialFilesCount; ++i) {
        if (leafName.Equals(nsDependentCString(aSpecialFiles[i]))) {
          shouldParse = PR_FALSE;
          // special files should be process in order; we put them into
          // the array by index; this can make the array sparse
          specialFiles.ReplaceObjectAt(prefFile, i);
        }
      }

      if (shouldParse) {
        prefFiles.AppendObject(prefFile);
      }
    }

    rv = dirIterator->HasMoreElements(&hasMoreElements);
  }

  if (prefFiles.Count() + specialFiles.Count() == 0) {
    NS_WARNING("No default pref files found.");
    if (NS_SUCCEEDED(rv)) {
      rv = NS_SUCCESS_FILE_DIRECTORY_EMPTY;
    }
    return rv;
  }

  prefFiles.Sort(pref_CompareFileNames, nsnull);
  
  PRUint32 arrayCount = prefFiles.Count();
  PRUint32 i;
  for (i = 0; i < arrayCount; ++i) {
    rv2 = openPrefFile(prefFiles[i]);
    if (NS_FAILED(rv2)) {
      NS_ERROR("Default pref file not parsed successfully.");
      rv = rv2;
    }
  }

  arrayCount = specialFiles.Count();
  for (i = 0; i < arrayCount; ++i) {
    // this may be a sparse array; test before parsing
    nsIFile* file = specialFiles[i];
    if (file) {
      rv2 = openPrefFile(file);
      if (NS_FAILED(rv2)) {
        NS_ERROR("Special default pref file not parsed successfully.");
        rv = rv2;
      }
    }
  }

  return rv;
}


//----------------------------------------------------------------------------------------
// Initialize default preference JavaScript buffers from
// appropriate TEXT resources
//----------------------------------------------------------------------------------------
static nsresult pref_InitInitialObjects()
{
  nsCOMPtr<nsIFile> aFile;
  nsCOMPtr<nsIFile> defaultPrefDir;
  nsresult          rv;

  // first we parse the GRE default prefs. This also works if we're not using a GRE, 

  rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(defaultPrefDir));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = defaultPrefDir->AppendNative(NS_LITERAL_CSTRING("greprefs"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = pref_LoadPrefsInDir(defaultPrefDir, nsnull, 0);
  if (NS_FAILED(rv)) {
    NS_WARNING("Error parsing GRE default preferences. Is this an old-style embedding app?");
  }

  // now parse the "application" default preferences
  rv = NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR, getter_AddRefs(defaultPrefDir));
  NS_ENSURE_SUCCESS(rv, rv);

  /* these pref file names should not be used: we process them after all other application pref files for backwards compatibility */
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

  rv = pref_LoadPrefsInDir(defaultPrefDir, specialFiles, NS_ARRAY_LENGTH(specialFiles));
  if (NS_FAILED(rv)) {
    NS_WARNING("Error parsing application default preferences.");
  }

  // xxxbsmedberg: TODO load default prefs from a category
  // but the architecture is not quite there yet

  static NS_DEFINE_CID(kDirectoryServiceCID, NS_DIRECTORY_SERVICE_CID);
  nsCOMPtr<nsIProperties> dirSvc(do_GetService(kDirectoryServiceCID, &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISimpleEnumerator> dirList;
  dirSvc->Get(NS_APP_PREFS_DEFAULTS_DIR_LIST,
              NS_GET_IID(nsISimpleEnumerator),
              getter_AddRefs(dirList));
  if (dirList) {
    PRBool hasMore;
    while (NS_SUCCEEDED(dirList->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> elem;
      dirList->GetNext(getter_AddRefs(elem));
      if (elem) {
        nsCOMPtr<nsIFile> dir = do_QueryInterface(elem);
        if (dir) {
          // Do we care if a file provided by this process fails to load?
          pref_LoadPrefsInDir(dir, nsnull, 0); 
        }
      }
    }
  }

  return NS_OK;
}

