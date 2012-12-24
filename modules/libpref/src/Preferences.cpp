/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"

#include "mozilla/Attributes.h"
#include "mozilla/Util.h"
#include "mozilla/HashFunctions.h"

#include "nsXULAppAPI.h"

#include "mozilla/Preferences.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsNetUtil.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIObserverService.h"
#include "nsIStringEnumerator.h"
#include "nsIZipReader.h"
#include "nsPrefBranch.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"
#include "nsCOMArray.h"
#include "nsXPCOMCID.h"
#include "nsAutoPtr.h"

#include "nsQuickSort.h"
#include "pldhash.h"

#include "prefapi.h"
#include "prefread.h"
#include "prefapi_private_data.h"

#include "mozilla/Omnijar.h"
#include "nsZipArchive.h"

#include "nsTArray.h"
#include "nsRefPtrHashtable.h"
#include "nsIMemoryReporter.h"

namespace mozilla {

// Definitions
#define INITIAL_PREF_FILES 10
static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);

// Prototypes
static nsresult openPrefFile(nsIFile* aFile);
static nsresult pref_InitInitialObjects(void);
static nsresult pref_LoadPrefsInDirList(const char *listId);
static nsresult ReadExtensionPrefs(nsIFile *aFile);

Preferences* Preferences::sPreferences = nullptr;
nsIPrefBranch* Preferences::sRootBranch = nullptr;
nsIPrefBranch* Preferences::sDefaultRootBranch = nullptr;
bool Preferences::sShutdown = false;

class ValueObserverHashKey : public PLDHashEntryHdr {
public:
  typedef ValueObserverHashKey* KeyType;
  typedef const ValueObserverHashKey* KeyTypePointer;

  static const ValueObserverHashKey* KeyToPointer(ValueObserverHashKey *aKey)
  {
    return aKey;
  }

  static PLDHashNumber HashKey(const ValueObserverHashKey *aKey)
  {
    PLDHashNumber hash = HashString(aKey->mPrefName);
    return AddToHash(hash, aKey->mCallback);
  }

  ValueObserverHashKey(const char *aPref, PrefChangedFunc aCallback) :
    mPrefName(aPref), mCallback(aCallback) { }

  ValueObserverHashKey(const ValueObserverHashKey *aOther) :
    mPrefName(aOther->mPrefName), mCallback(aOther->mCallback)
  { }

  bool KeyEquals(const ValueObserverHashKey *aOther) const
  {
    return mCallback == aOther->mCallback && mPrefName == aOther->mPrefName;
  }

  ValueObserverHashKey *GetKey() const
  {
    return const_cast<ValueObserverHashKey*>(this);
  }

  enum { ALLOW_MEMMOVE = true };

  nsCString mPrefName;
  PrefChangedFunc mCallback;
};

class ValueObserver MOZ_FINAL : public nsIObserver,
                                public ValueObserverHashKey
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  ValueObserver(const char *aPref, PrefChangedFunc aCallback)
    : ValueObserverHashKey(aPref, aCallback) { }

  ~ValueObserver() {
    Preferences::RemoveObserver(this, mPrefName.get());
  }

  void AppendClosure(void *aClosure) {
    mClosures.AppendElement(aClosure);
  }

  void RemoveClosure(void *aClosure) {
    mClosures.RemoveElement(aClosure);
  }

  bool HasNoClosures() {
    return mClosures.Length() == 0;
  }

  nsTArray<void*> mClosures;
};

NS_IMPL_ISUPPORTS1(ValueObserver, nsIObserver)

NS_IMETHODIMP
ValueObserver::Observe(nsISupports     *aSubject,
                       const char      *aTopic,
                       const PRUnichar *aData)
{
  NS_ASSERTION(!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID),
               "invalid topic");
  NS_ConvertUTF16toUTF8 data(aData);
  for (uint32_t i = 0; i < mClosures.Length(); i++) {
    mCallback(data.get(), mClosures.ElementAt(i));
  }

  return NS_OK;
}

struct CacheData {
  void* cacheLocation;
  union {
    bool defaultValueBool;
    int32_t defaultValueInt;
    uint32_t defaultValueUint;
  };
};

static nsTArray<nsAutoPtr<CacheData> >* gCacheData = nullptr;
static nsRefPtrHashtable<ValueObserverHashKey,
                         ValueObserver>* gObserverTable = nullptr;

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(PreferencesMallocSizeOf)

static size_t
SizeOfObserverEntryExcludingThis(ValueObserverHashKey* aKey,
                                 const nsRefPtr<ValueObserver>& aData,
                                 nsMallocSizeOfFun aMallocSizeOf,
                                 void*)
{
  size_t n = 0;
  n += aKey->mPrefName.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  n += aData->mClosures.SizeOfExcludingThis(aMallocSizeOf);
  return n;
}

// static
int64_t
Preferences::GetPreferencesMemoryUsed()
{
  size_t n = 0;
  n += PreferencesMallocSizeOf(sPreferences);
  if (gHashTable.ops) {
    // pref keys are allocated in a private arena, which we count elsewhere.
    // pref stringvals are allocated out of the same private arena.
    n += PL_DHashTableSizeOfExcludingThis(&gHashTable, nullptr,
                                          PreferencesMallocSizeOf);
  }
  if (gCacheData) {
    n += gCacheData->SizeOfIncludingThis(PreferencesMallocSizeOf);
    for (uint32_t i = 0, count = gCacheData->Length(); i < count; ++i) {
      n += PreferencesMallocSizeOf((*gCacheData)[i]);
    }
  }
  if (gObserverTable) {
    n += PreferencesMallocSizeOf(gObserverTable);
    n += gObserverTable->SizeOfExcludingThis(SizeOfObserverEntryExcludingThis,
                                             PreferencesMallocSizeOf);
  }
  // We don't measure sRootBranch and sDefaultRootBranch here because
  // DMD indicates they are not significant.
  n += pref_SizeOfPrivateData(PreferencesMallocSizeOf);
  return n;
}

NS_MEMORY_REPORTER_IMPLEMENT(Preferences,
  "explicit/preferences",
  KIND_HEAP,
  UNITS_BYTES,
  Preferences::GetPreferencesMemoryUsed,
  "Memory used by the preferences system.")

// static
Preferences*
Preferences::GetInstanceForService()
{
  if (sPreferences) {
    NS_ADDREF(sPreferences);
    return sPreferences;
  }

  NS_ENSURE_TRUE(!sShutdown, nullptr);

  sRootBranch = new nsPrefBranch("", false);
  NS_ADDREF(sRootBranch);
  sDefaultRootBranch = new nsPrefBranch("", true);
  NS_ADDREF(sDefaultRootBranch);

  sPreferences = new Preferences();
  NS_ADDREF(sPreferences);

  if (NS_FAILED(sPreferences->Init())) {
    // The singleton instance will delete sRootBranch and sDefaultRootBranch.
    NS_RELEASE(sPreferences);
    return nullptr;
  }

  gCacheData = new nsTArray<nsAutoPtr<CacheData> >();

  gObserverTable = new nsRefPtrHashtable<ValueObserverHashKey, ValueObserver>();
  gObserverTable->Init();

  nsCOMPtr<nsIMemoryReporter> reporter(new NS_MEMORY_REPORTER_NAME(Preferences));
  NS_RegisterMemoryReporter(reporter);

  NS_ADDREF(sPreferences);
  return sPreferences;
}

// static
bool
Preferences::InitStaticMembers()
{
  if (!sShutdown && !sPreferences) {
    nsCOMPtr<nsIPrefService> prefService =
      do_GetService(NS_PREFSERVICE_CONTRACTID);
  }

  return sPreferences != nullptr;
}

// static
void
Preferences::Shutdown()
{
  if (!sShutdown) {
    sShutdown = true; // Don't create the singleton instance after here.

    // Don't set NULL to sPreferences here.  The instance may be grabbed by
    // other modules.  The utility methods of Preferences should be available
    // until the singleton instance actually released.
    if (sPreferences) {
      sPreferences->Release();
    }
  }
}

//-----------------------------------------------------------------------------

/*
 * Constructor/Destructor
 */

Preferences::Preferences()
{
}

Preferences::~Preferences()
{
  NS_ASSERTION(sPreferences == this, "Isn't this the singleton instance?");

  delete gObserverTable;
  gObserverTable = nullptr;

  delete gCacheData;
  gCacheData = nullptr;

  NS_RELEASE(sRootBranch);
  NS_RELEASE(sDefaultRootBranch);

  sPreferences = nullptr;

  PREF_Cleanup();
}


/*
 * nsISupports Implementation
 */

NS_IMPL_THREADSAFE_ADDREF(Preferences)
NS_IMPL_THREADSAFE_RELEASE(Preferences)

NS_INTERFACE_MAP_BEGIN(Preferences)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrefService)
    NS_INTERFACE_MAP_ENTRY(nsIPrefService)
    NS_INTERFACE_MAP_ENTRY(nsIObserver)
    NS_INTERFACE_MAP_ENTRY(nsIPrefBranch)
    NS_INTERFACE_MAP_ENTRY(nsIPrefBranch2)
    NS_INTERFACE_MAP_ENTRY(nsIPrefBranchInternal)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END


/*
 * nsIPrefService Implementation
 */

nsresult
Preferences::Init()
{
  nsresult rv;

  rv = PREF_Init();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = pref_InitInitialObjects();
  NS_ENSURE_SUCCESS(rv, rv);

  using mozilla::dom::ContentChild;
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    InfallibleTArray<PrefSetting> prefs;
    ContentChild::GetSingleton()->SendReadPrefsArray(&prefs);

    // Store the array
    for (uint32_t i = 0; i < prefs.Length(); ++i) {
      pref_SetPref(prefs[i]);
    }
    return NS_OK;
  }

  nsXPIDLCString lockFileName;
  /*
   * The following is a small hack which will allow us to only load the library
   * which supports the netscape.cfg file if the preference is defined. We
   * test for the existence of the pref, set in the all.js (mozilla) or
   * all-ns.js (netscape 6), and if it exists we startup the pref config
   * category which will do the rest.
   */

  rv = PREF_CopyCharPref("general.config.filename", getter_Copies(lockFileName), false);
  if (NS_SUCCEEDED(rv))
    NS_CreateServicesFromCategory("pref-config-startup",
                                  static_cast<nsISupports *>(static_cast<void *>(this)),
                                  "pref-config-startup");    

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService)
    return NS_ERROR_FAILURE;

  rv = observerService->AddObserver(this, "profile-before-change", true);

  observerService->AddObserver(this, "load-extension-defaults", true);

  return(rv);
}

// static
nsresult
Preferences::ResetAndReadUserPrefs()
{
  sPreferences->ResetUserPrefs();
  return sPreferences->ReadUserPrefs(nullptr);
}

NS_IMETHODIMP
Preferences::Observe(nsISupports *aSubject, const char *aTopic,
                     const PRUnichar *someData)
{
  if (XRE_GetProcessType() == GeckoProcessType_Content)
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = NS_OK;

  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    if (someData && !NS_strcmp(someData, NS_LITERAL_STRING("shutdown-cleanse").get())) {
      if (mCurrentFile) {
        mCurrentFile->Remove(false);
        mCurrentFile = nullptr;
      }
    } else {
      rv = SavePrefFile(nullptr);
    }
  } else if (!strcmp(aTopic, "load-extension-defaults")) {
    pref_LoadPrefsInDirList(NS_EXT_PREFS_DEFAULTS_DIR_LIST);
  } else if (!nsCRT::strcmp(aTopic, "reload-default-prefs")) {
    // Reload the default prefs from file.
    pref_InitInitialObjects();
  }
  return rv;
}


NS_IMETHODIMP
Preferences::ReadUserPrefs(nsIFile *aFile)
{
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    NS_ERROR("cannot load prefs from content process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv;

  if (nullptr == aFile) {
    rv = UseDefaultPrefFile();
    // A user pref file is optional.
    // Ignore all errors related to it, so we retain 'rv' value :-|
    (void) UseUserPrefFile();

    NotifyServiceObservers(NS_PREFSERVICE_READ_TOPIC_ID);
  } else {
    rv = ReadAndOwnUserPrefFile(aFile);
  }

  return rv;
}

NS_IMETHODIMP
Preferences::ResetPrefs()
{
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    NS_ERROR("cannot set prefs from content process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  NotifyServiceObservers(NS_PREFSERVICE_RESET_TOPIC_ID);
  PREF_CleanupPrefs();

  nsresult rv = PREF_Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return pref_InitInitialObjects();
}

NS_IMETHODIMP
Preferences::ResetUserPrefs()
{
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    NS_ERROR("cannot set prefs from content process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  PREF_ClearAllUserPrefs();
  return NS_OK;    
}

NS_IMETHODIMP
Preferences::SavePrefFile(nsIFile *aFile)
{
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    NS_ERROR("cannot save prefs from content process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  return SavePrefFileInternal(aFile);
}

static nsresult
ReadExtensionPrefs(nsIFile *aFile)
{
  nsresult rv;
  nsCOMPtr<nsIZipReader> reader = do_CreateInstance(kZipReaderCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = reader->Open(aFile);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUTF8StringEnumerator> files;
  rv = reader->FindEntries(nsDependentCString("defaults/preferences/*.(J|j)(S|s)$"),
                           getter_AddRefs(files));
  NS_ENSURE_SUCCESS(rv, rv);

  char buffer[4096];

  bool more;
  while (NS_SUCCEEDED(rv = files->HasMore(&more)) && more) {
    nsAutoCString entry;
    rv = files->GetNext(entry);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIInputStream> stream;
    rv = reader->GetInputStream(entry, getter_AddRefs(stream));
    NS_ENSURE_SUCCESS(rv, rv);

    uint64_t avail;
    uint32_t read;

    PrefParseState ps;
    PREF_InitParseState(&ps, PREF_ReaderCallback, NULL);
    while (NS_SUCCEEDED(rv = stream->Available(&avail)) && avail) {
      rv = stream->Read(buffer, 4096, &read);
      if (NS_FAILED(rv)) {
        NS_WARNING("Pref stream read failed");
        break;
      }

      PREF_ParseBuf(&ps, buffer, read);
    }
    PREF_FinalizeParseState(&ps);
  }
  return rv;
}

void
Preferences::SetPreference(const PrefSetting& aPref)
{
  pref_SetPref(aPref);
}

void
Preferences::GetPreference(PrefSetting* aPref)
{
  PrefHashEntry *entry = pref_HashTableLookup(aPref->name().get());
  if (!entry)
    return;

  pref_GetPrefFromEntry(entry, aPref);
}

void
Preferences::GetPreferences(InfallibleTArray<PrefSetting>* aPrefs)
{
  aPrefs->SetCapacity(PL_DHASH_TABLE_SIZE(&gHashTable));
  PL_DHashTableEnumerate(&gHashTable, pref_GetPrefs, aPrefs);
}

NS_IMETHODIMP
Preferences::GetBranch(const char *aPrefRoot, nsIPrefBranch **_retval)
{
  nsresult rv;

  if ((nullptr != aPrefRoot) && (*aPrefRoot != '\0')) {
    // TODO: - cache this stuff and allow consumers to share branches (hold weak references I think)
    nsPrefBranch* prefBranch = new nsPrefBranch(aPrefRoot, false);
    if (!prefBranch)
      return NS_ERROR_OUT_OF_MEMORY;

    rv = CallQueryInterface(prefBranch, _retval);
  } else {
    // special case caching the default root
    rv = CallQueryInterface(sRootBranch, _retval);
  }
  return rv;
}

NS_IMETHODIMP
Preferences::GetDefaultBranch(const char *aPrefRoot, nsIPrefBranch **_retval)
{
  if (!aPrefRoot || !aPrefRoot[0]) {
    return CallQueryInterface(sDefaultRootBranch, _retval);
  }

  // TODO: - cache this stuff and allow consumers to share branches (hold weak references I think)
  nsPrefBranch* prefBranch = new nsPrefBranch(aPrefRoot, true);
  if (!prefBranch)
    return NS_ERROR_OUT_OF_MEMORY;

  return CallQueryInterface(prefBranch, _retval);
}


nsresult
Preferences::NotifyServiceObservers(const char *aTopic)
{
  nsCOMPtr<nsIObserverService> observerService = 
    mozilla::services::GetObserverService();  
  if (!observerService)
    return NS_ERROR_FAILURE;

  nsISupports *subject = (nsISupports *)((nsIPrefService *)this);
  observerService->NotifyObservers(subject, aTopic, nullptr);
  
  return NS_OK;
}

nsresult
Preferences::UseDefaultPrefFile()
{
  nsresult rv;
  nsCOMPtr<nsIFile> aFile;

  rv = NS_GetSpecialDirectory(NS_APP_PREFS_50_FILE, getter_AddRefs(aFile));
  if (NS_SUCCEEDED(rv)) {
    rv = ReadAndOwnUserPrefFile(aFile);
    // Most likely cause of failure here is that the file didn't
    // exist, so save a new one. mUserPrefReadFailed will be
    // used to catch an error in actually reading the file.
    if (NS_FAILED(rv)) {
      if (NS_FAILED(SavePrefFileInternal(aFile)))
        NS_ERROR("Failed to save new shared pref file");
      else
        rv = NS_OK;
    }
  }
  
  return rv;
}

nsresult
Preferences::UseUserPrefFile()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFile> aFile;
  nsDependentCString prefsDirProp(NS_APP_PREFS_50_DIR);

  rv = NS_GetSpecialDirectory(prefsDirProp.get(), getter_AddRefs(aFile));
  if (NS_SUCCEEDED(rv) && aFile) {
    rv = aFile->AppendNative(NS_LITERAL_CSTRING("user.js"));
    if (NS_SUCCEEDED(rv)) {
      bool exists = false;
      aFile->Exists(&exists);
      if (exists) {
        rv = openPrefFile(aFile);
      } else {
        rv = NS_ERROR_FILE_NOT_FOUND;
      }
    }
  }
  return rv;
}

nsresult
Preferences::MakeBackupPrefFile(nsIFile *aFile)
{
  // Example: this copies "prefs.js" to "Invalidprefs.js" in the same directory.
  // "Invalidprefs.js" is removed if it exists, prior to making the copy.
  nsAutoString newFilename;
  nsresult rv = aFile->GetLeafName(newFilename);
  NS_ENSURE_SUCCESS(rv, rv);
  newFilename.Insert(NS_LITERAL_STRING("Invalid"), 0);
  nsCOMPtr<nsIFile> newFile;
  rv = aFile->GetParent(getter_AddRefs(newFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = newFile->Append(newFilename);
  NS_ENSURE_SUCCESS(rv, rv);
  bool exists = false;
  newFile->Exists(&exists);
  if (exists) {
    rv = newFile->Remove(false);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = aFile->CopyTo(nullptr, newFilename);
  NS_ENSURE_SUCCESS(rv, rv);
  return rv;
}

nsresult
Preferences::ReadAndOwnUserPrefFile(nsIFile *aFile)
{
  NS_ENSURE_ARG(aFile);
  
  if (mCurrentFile == aFile)
    return NS_OK;
  mCurrentFile = aFile;

  nsresult rv = NS_OK;
  bool exists = false;
  mCurrentFile->Exists(&exists);
  if (exists) {
    rv = openPrefFile(mCurrentFile);
    if (NS_FAILED(rv)) {
      // Save a backup copy of the current (invalid) prefs file, since all prefs
      // from the error line to the end of the file will be lost (bug 361102).
      // TODO we should notify the user about it (bug 523725).
      MakeBackupPrefFile(mCurrentFile);
    }
  } else {
    rv = NS_ERROR_FILE_NOT_FOUND;
  }

  return rv;
}

nsresult
Preferences::SavePrefFileInternal(nsIFile *aFile)
{
  if (nullptr == aFile) {
    // the gDirty flag tells us if we should write to mCurrentFile
    // we only check this flag when the caller wants to write to the default
    if (!gDirty)
      return NS_OK;
    
    // It's possible that we never got a prefs file.
    nsresult rv = NS_OK;
    if (mCurrentFile)
      rv = WritePrefFile(mCurrentFile);

    return rv;
  } else {
    return WritePrefFile(aFile);
  }
}

nsresult
Preferences::WritePrefFile(nsIFile* aFile)
{
  const char                outHeader[] =
    "# Mozilla User Preferences"
    NS_LINEBREAK
    NS_LINEBREAK
    "/* Do not edit this file."
    NS_LINEBREAK
    " *"
    NS_LINEBREAK
    " * If you make changes to this file while the application is running,"
    NS_LINEBREAK
    " * the changes will be overwritten when the application exits."
    NS_LINEBREAK
    " *"
    NS_LINEBREAK
    " * To make a manual change to preferences, you can visit the URL about:config"
    NS_LINEBREAK
    " */"
    NS_LINEBREAK
    NS_LINEBREAK;

  nsCOMPtr<nsIOutputStream> outStreamSink;
  nsCOMPtr<nsIOutputStream> outStream;
  uint32_t                  writeAmount;
  nsresult                  rv;

  if (!gHashTable.ops)
    return NS_ERROR_NOT_INITIALIZED;

  // execute a "safe" save by saving through a tempfile
  rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(outStreamSink),
                                       aFile,
                                       -1,
                                       0600);
  if (NS_FAILED(rv)) 
      return rv;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(outStream), outStreamSink, 4096);
  if (NS_FAILED(rv)) 
      return rv;  

  nsAutoArrayPtr<char*> valueArray(new char*[gHashTable.entryCount]);
  memset(valueArray, 0, gHashTable.entryCount * sizeof(char*));
  pref_saveArgs saveArgs;
  saveArgs.prefArray = valueArray;
  saveArgs.saveTypes = SAVE_ALL;
  
  // get the lines that we're supposed to be writing to the file
  PL_DHashTableEnumerate(&gHashTable, pref_savePref, &saveArgs);
    
  /* Sort the preferences to make a readable file on disk */
  NS_QuickSort(valueArray, gHashTable.entryCount, sizeof(char *), pref_CompareStrings, NULL);
  
  // write out the file header
  outStream->Write(outHeader, sizeof(outHeader) - 1, &writeAmount);

  char** walker = valueArray;
  for (uint32_t valueIdx = 0; valueIdx < gHashTable.entryCount; valueIdx++, walker++) {
    if (*walker) {
      outStream->Write(*walker, strlen(*walker), &writeAmount);
      outStream->Write(NS_LINEBREAK, NS_LINEBREAK_LEN, &writeAmount);
      NS_Free(*walker);
    }
  }

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

  gDirty = false;
  return NS_OK;
}

static nsresult openPrefFile(nsIFile* aFile)
{
  nsCOMPtr<nsIInputStream> inStr;

  nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(inStr), aFile);
  if (NS_FAILED(rv)) 
    return rv;        

  uint64_t fileSize64;
  rv = inStr->Available(&fileSize64);
  if (NS_FAILED(rv))
    return rv;
  NS_ENSURE_TRUE(fileSize64 <= UINT32_MAX, NS_ERROR_FILE_TOO_BIG);

  uint32_t fileSize = (uint32_t)fileSize64;
  nsAutoArrayPtr<char> fileBuffer(new char[fileSize]);
  if (fileBuffer == nullptr)
    return NS_ERROR_OUT_OF_MEMORY;

  PrefParseState ps;
  PREF_InitParseState(&ps, PREF_ReaderCallback, NULL);

  // Read is not guaranteed to return a buf the size of fileSize,
  // but usually will.
  nsresult rv2 = NS_OK;
  for (;;) {
    uint32_t amtRead = 0;
    rv = inStr->Read((char*)fileBuffer, fileSize, &amtRead);
    if (NS_FAILED(rv) || amtRead == 0)
      break;
    if (!PREF_ParseBuf(&ps, fileBuffer, amtRead))
      rv2 = NS_ERROR_FILE_CORRUPTED;
  }

  PREF_FinalizeParseState(&ps);

  return NS_FAILED(rv) ? rv : rv2;
}

/*
 * some stuff that gets called from Pref_Init()
 */

static int
pref_CompareFileNames(nsIFile* aFile1, nsIFile* aFile2, void* /*unused*/)
{
  nsAutoCString filename1, filename2;
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
pref_LoadPrefsInDir(nsIFile* aDir, char const *const *aSpecialFiles, uint32_t aSpecialFilesCount)
{
  nsresult rv, rv2;
  bool hasMoreElements;

  nsCOMPtr<nsISimpleEnumerator> dirIterator;

  // this may fail in some normal cases, such as embedders who do not use a GRE
  rv = aDir->GetDirectoryEntries(getter_AddRefs(dirIterator));
  if (NS_FAILED(rv)) {
    // If the directory doesn't exist, then we have no reason to complain.  We
    // loaded everything (and nothing) successfully.
    if (rv == NS_ERROR_FILE_NOT_FOUND || rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST)
      rv = NS_OK;
    return rv;
  }

  rv = dirIterator->HasMoreElements(&hasMoreElements);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMArray<nsIFile> prefFiles(INITIAL_PREF_FILES);
  nsCOMArray<nsIFile> specialFiles(aSpecialFilesCount);
  nsCOMPtr<nsIFile> prefFile;

  while (hasMoreElements && NS_SUCCEEDED(rv)) {
    nsAutoCString leafName;

    rv = dirIterator->GetNext(getter_AddRefs(prefFile));
    if (NS_FAILED(rv)) {
      break;
    }

    prefFile->GetNativeLeafName(leafName);
    NS_ASSERTION(!leafName.IsEmpty(), "Failure in default prefs: directory enumerator returned empty file?");

    // Skip non-js files
    if (StringEndsWith(leafName, NS_LITERAL_CSTRING(".js"),
                       nsCaseInsensitiveCStringComparator())) {
      bool shouldParse = true;
      // separate out special files
      for (uint32_t i = 0; i < aSpecialFilesCount; ++i) {
        if (leafName.Equals(nsDependentCString(aSpecialFiles[i]))) {
          shouldParse = false;
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

  prefFiles.Sort(pref_CompareFileNames, nullptr);
  
  uint32_t arrayCount = prefFiles.Count();
  uint32_t i;
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

static nsresult pref_LoadPrefsInDirList(const char *listId)
{
  nsresult rv;
  nsCOMPtr<nsIProperties> dirSvc(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISimpleEnumerator> list;
  dirSvc->Get(listId,
              NS_GET_IID(nsISimpleEnumerator),
              getter_AddRefs(list));
  if (!list)
    return NS_OK;

  bool hasMore;
  while (NS_SUCCEEDED(list->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> elem;
    list->GetNext(getter_AddRefs(elem));
    if (!elem)
      continue;

    nsCOMPtr<nsIFile> path = do_QueryInterface(elem);
    if (!path)
      continue;

    nsAutoCString leaf;
    path->GetNativeLeafName(leaf);

    // Do we care if a file provided by this process fails to load?
    if (Substring(leaf, leaf.Length() - 4).Equals(NS_LITERAL_CSTRING(".xpi")))
      ReadExtensionPrefs(path);
    else
      pref_LoadPrefsInDir(path, nullptr, 0);
  }
  return NS_OK;
}

static nsresult pref_ReadPrefFromJar(nsZipArchive* jarReader, const char *name)
{
  nsZipItemPtr<char> manifest(jarReader, name, true);
  NS_ENSURE_TRUE(manifest.Buffer(), NS_ERROR_NOT_AVAILABLE);

  PrefParseState ps;
  PREF_InitParseState(&ps, PREF_ReaderCallback, NULL);
  PREF_ParseBuf(&ps, manifest, manifest.Length());
  PREF_FinalizeParseState(&ps);

  return NS_OK;
}

//----------------------------------------------------------------------------------------
// Initialize default preference JavaScript buffers from
// appropriate TEXT resources
//----------------------------------------------------------------------------------------
static nsresult pref_InitInitialObjects()
{
  nsresult rv;

  // In omni.jar case, we load the following prefs:
  // - jar:$gre/omni.jar!/greprefs.js
  // - jar:$gre/omni.jar!/defaults/pref/*.js
  // In non omni.jar case, we load:
  // - $gre/greprefs.js
  //
  // In both cases, we also load:
  // - $gre/defaults/pref/*.js
  // This is kept for bug 591866 (channel-prefs.js should not be in omni.jar)
  // on $app == $gre case ; we load all files instead of channel-prefs.js only
  // to have the same behaviour as $app != $gre, where this is required as
  // a supported location for GRE preferences.
  //
  // When $app != $gre, we additionally load, in omni.jar case:
  // - jar:$app/omni.jar!/defaults/preferences/*.js
  // - $app/defaults/preferences/*.js
  // and in non omni.jar case:
  // - $app/defaults/preferences/*.js
  // When $app == $gre, we additionally load, in omni.jar case:
  // - jar:$gre/omni.jar!/defaults/preferences/*.js
  // Thus, in omni.jar case, we always load app-specific default preferences
  // from omni.jar, whether or not $app == $gre.

  nsZipFind *findPtr;
  nsAutoPtr<nsZipFind> find;
  nsTArray<nsCString> prefEntries;
  const char *entryName;
  uint16_t entryNameLen;

  nsRefPtr<nsZipArchive> jarReader = mozilla::Omnijar::GetReader(mozilla::Omnijar::GRE);
  if (jarReader) {
    // Load jar:$gre/omni.jar!/greprefs.js
    rv = pref_ReadPrefFromJar(jarReader, "greprefs.js");
    NS_ENSURE_SUCCESS(rv, rv);

    // Load jar:$gre/omni.jar!/defaults/pref/*.js
    rv = jarReader->FindInit("defaults/pref/*.js$", &findPtr);
    NS_ENSURE_SUCCESS(rv, rv);

    find = findPtr;
    while (NS_SUCCEEDED(find->FindNext(&entryName, &entryNameLen))) {
      prefEntries.AppendElement(Substring(entryName, entryNameLen));
    }

    prefEntries.Sort();
    for (uint32_t i = prefEntries.Length(); i--; ) {
      rv = pref_ReadPrefFromJar(jarReader, prefEntries[i].get());
      if (NS_FAILED(rv))
        NS_WARNING("Error parsing preferences.");
    }
  } else {
    // Load $gre/greprefs.js
    nsCOMPtr<nsIFile> greprefsFile;
    rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(greprefsFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = greprefsFile->AppendNative(NS_LITERAL_CSTRING("greprefs.js"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = openPrefFile(greprefsFile);
    if (NS_FAILED(rv))
      NS_WARNING("Error parsing GRE default preferences. Is this an old-style embedding app?");
  }

  // Load $gre/defaults/pref/*.js
  nsCOMPtr<nsIFile> defaultPrefDir;

  rv = NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR, getter_AddRefs(defaultPrefDir));
  NS_ENSURE_SUCCESS(rv, rv);

  /* these pref file names should not be used: we process them after all other application pref files for backwards compatibility */
  static const char* specialFiles[] = {
#if defined(XP_MACOSX)
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
#elif defined(XP_OS2)
    "os2pref.js"
#elif defined(XP_BEOS)
    "beos.js"
#endif
  };

  rv = pref_LoadPrefsInDir(defaultPrefDir, specialFiles, ArrayLength(specialFiles));
  if (NS_FAILED(rv))
    NS_WARNING("Error parsing application default preferences.");

  // Load jar:$app/omni.jar!/defaults/preferences/*.js
  // or jar:$gre/omni.jar!/defaults/preferences/*.js.
  nsRefPtr<nsZipArchive> appJarReader = mozilla::Omnijar::GetReader(mozilla::Omnijar::APP);
  // GetReader(mozilla::Omnijar::APP) returns null when $app == $gre, in which
  // case we look for app-specific default preferences in $gre.
  if (!appJarReader)
    appJarReader = mozilla::Omnijar::GetReader(mozilla::Omnijar::GRE);
  if (appJarReader) {
    rv = appJarReader->FindInit("defaults/preferences/*.js$", &findPtr);
    NS_ENSURE_SUCCESS(rv, rv);
    find = findPtr;
    prefEntries.Clear();
    while (NS_SUCCEEDED(find->FindNext(&entryName, &entryNameLen))) {
      prefEntries.AppendElement(Substring(entryName, entryNameLen));
    }
    prefEntries.Sort();
    for (uint32_t i = prefEntries.Length(); i--; ) {
      rv = pref_ReadPrefFromJar(appJarReader, prefEntries[i].get());
      if (NS_FAILED(rv))
        NS_WARNING("Error parsing preferences.");
    }
  }

  rv = pref_LoadPrefsInDirList(NS_APP_PREFS_DEFAULTS_DIR_LIST);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_CreateServicesFromCategory(NS_PREFSERVICE_APPDEFAULTS_TOPIC_ID,
                                nullptr, NS_PREFSERVICE_APPDEFAULTS_TOPIC_ID);

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService)
    return NS_ERROR_FAILURE;

  observerService->NotifyObservers(nullptr, NS_PREFSERVICE_APPDEFAULTS_TOPIC_ID, nullptr);

  return pref_LoadPrefsInDirList(NS_EXT_PREFS_DEFAULTS_DIR_LIST);
}


/******************************************************************************
 *
 * static utilities
 *
 ******************************************************************************/

// static
nsresult
Preferences::GetBool(const char* aPref, bool* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_GetBoolPref(aPref, aResult, false);
}

// static
nsresult
Preferences::GetInt(const char* aPref, int32_t* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_GetIntPref(aPref, aResult, false);
}

// static
nsresult
Preferences::GetFloat(const char* aPref, float* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsAutoCString result;
  nsresult rv = PREF_CopyCharPref(aPref, getter_Copies(result), false);
  if (NS_SUCCEEDED(rv)) {
    *aResult = result.ToFloat(&rv);
  }

  return rv;
}

// static
nsAdoptingCString
Preferences::GetCString(const char* aPref)
{
  nsAdoptingCString result;
  PREF_CopyCharPref(aPref, getter_Copies(result), false);
  return result;
}

// static
nsAdoptingString
Preferences::GetString(const char* aPref)
{
  nsAdoptingString result;
  GetString(aPref, &result);
  return result;
}

// static
nsresult
Preferences::GetCString(const char* aPref, nsACString* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsAutoCString result;
  nsresult rv = PREF_CopyCharPref(aPref, getter_Copies(result), false);
  if (NS_SUCCEEDED(rv)) {
    *aResult = result;
  }
  return rv;
}

// static
nsresult
Preferences::GetString(const char* aPref, nsAString* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsAutoCString result;
  nsresult rv = PREF_CopyCharPref(aPref, getter_Copies(result), false);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(result, *aResult);
  }
  return rv;
}

// static
nsAdoptingCString
Preferences::GetLocalizedCString(const char* aPref)
{
  nsAdoptingCString result;
  GetLocalizedCString(aPref, &result);
  return result;
}

// static
nsAdoptingString
Preferences::GetLocalizedString(const char* aPref)
{
  nsAdoptingString result;
  GetLocalizedString(aPref, &result);
  return result;
}

// static
nsresult
Preferences::GetLocalizedCString(const char* aPref, nsACString* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  nsAutoString result;
  nsresult rv = GetLocalizedString(aPref, &result);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF16toUTF8(result, *aResult);
  }
  return rv;
}

// static
nsresult
Preferences::GetLocalizedString(const char* aPref, nsAString* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsCOMPtr<nsIPrefLocalizedString> prefLocalString;
  nsresult rv = sRootBranch->GetComplexValue(aPref,
                                             NS_GET_IID(nsIPrefLocalizedString),
                                             getter_AddRefs(prefLocalString));
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(prefLocalString, "Succeeded but the result is NULL");
    prefLocalString->GetData(getter_Copies(*aResult));
  }
  return rv;
}

// static
nsresult
Preferences::GetComplex(const char* aPref, const nsIID &aType, void** aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sRootBranch->GetComplexValue(aPref, aType, aResult);
}

// static
nsresult
Preferences::SetCString(const char* aPref, const char* aValue)
{
  NS_ENSURE_TRUE(XRE_GetProcessType() == GeckoProcessType_Default, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetCharPref(aPref, aValue, false);
}

// static
nsresult
Preferences::SetCString(const char* aPref, const nsACString &aValue)
{
  NS_ENSURE_TRUE(XRE_GetProcessType() == GeckoProcessType_Default, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetCharPref(aPref, PromiseFlatCString(aValue).get(), false);
}

// static
nsresult
Preferences::SetString(const char* aPref, const PRUnichar* aValue)
{
  NS_ENSURE_TRUE(XRE_GetProcessType() == GeckoProcessType_Default, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetCharPref(aPref, NS_ConvertUTF16toUTF8(aValue).get(), false);
}

// static
nsresult
Preferences::SetString(const char* aPref, const nsAString &aValue)
{
  NS_ENSURE_TRUE(XRE_GetProcessType() == GeckoProcessType_Default, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetCharPref(aPref, NS_ConvertUTF16toUTF8(aValue).get(), false);
}

// static
nsresult
Preferences::SetBool(const char* aPref, bool aValue)
{
  NS_ENSURE_TRUE(XRE_GetProcessType() == GeckoProcessType_Default, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetBoolPref(aPref, aValue, false);
}

// static
nsresult
Preferences::SetInt(const char* aPref, int32_t aValue)
{
  NS_ENSURE_TRUE(XRE_GetProcessType() == GeckoProcessType_Default, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetIntPref(aPref, aValue, false);
}

// static
nsresult
Preferences::SetComplex(const char* aPref, const nsIID &aType,
                        nsISupports* aValue)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sRootBranch->SetComplexValue(aPref, aType, aValue);
}

// static
nsresult
Preferences::ClearUser(const char* aPref)
{
  NS_ENSURE_TRUE(XRE_GetProcessType() == GeckoProcessType_Default, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_ClearUserPref(aPref);
}

// static
bool
Preferences::HasUserValue(const char* aPref)
{
  NS_ENSURE_TRUE(InitStaticMembers(), false);
  return PREF_HasUserPref(aPref);
}

// static
int32_t
Preferences::GetType(const char* aPref)
{
  NS_ENSURE_TRUE(InitStaticMembers(), nsIPrefBranch::PREF_INVALID);
  int32_t result;
  return NS_SUCCEEDED(sRootBranch->GetPrefType(aPref, &result)) ?
    result : nsIPrefBranch::PREF_INVALID;
}

// static
nsresult
Preferences::AddStrongObserver(nsIObserver* aObserver,
                               const char* aPref)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sRootBranch->AddObserver(aPref, aObserver, false);
}

// static
nsresult
Preferences::AddWeakObserver(nsIObserver* aObserver,
                             const char* aPref)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sRootBranch->AddObserver(aPref, aObserver, true);
}

// static
nsresult
Preferences::RemoveObserver(nsIObserver* aObserver,
                            const char* aPref)
{
  if (!sPreferences && sShutdown) {
    return NS_OK; // Observers have been released automatically.
  }
  NS_ENSURE_TRUE(sPreferences, NS_ERROR_NOT_AVAILABLE);
  return sRootBranch->RemoveObserver(aPref, aObserver);
}

// static
nsresult
Preferences::AddStrongObservers(nsIObserver* aObserver,
                                const char** aPrefs)
{
  for (uint32_t i = 0; aPrefs[i]; i++) {
    nsresult rv = AddStrongObserver(aObserver, aPrefs[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

// static
nsresult
Preferences::AddWeakObservers(nsIObserver* aObserver,
                              const char** aPrefs)
{
  for (uint32_t i = 0; aPrefs[i]; i++) {
    nsresult rv = AddWeakObserver(aObserver, aPrefs[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

// static
nsresult
Preferences::RemoveObservers(nsIObserver* aObserver,
                             const char** aPrefs)
{
  if (!sPreferences && sShutdown) {
    return NS_OK; // Observers have been released automatically.
  }
  NS_ENSURE_TRUE(sPreferences, NS_ERROR_NOT_AVAILABLE);

  for (uint32_t i = 0; aPrefs[i]; i++) {
    nsresult rv = RemoveObserver(aObserver, aPrefs[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

// static
nsresult
Preferences::RegisterCallback(PrefChangedFunc aCallback,
                              const char* aPref,
                              void* aClosure)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  ValueObserverHashKey hashKey(aPref, aCallback);
  nsRefPtr<ValueObserver> observer;
  gObserverTable->Get(&hashKey, getter_AddRefs(observer));
  if (observer) {
    observer->AppendClosure(aClosure);
    return NS_OK;
  }

  observer = new ValueObserver(aPref, aCallback);
  observer->AppendClosure(aClosure);
  nsresult rv = AddStrongObserver(observer, aPref);
  NS_ENSURE_SUCCESS(rv, rv);
  gObserverTable->Put(observer, observer);
  return NS_OK;
}

// static
nsresult
Preferences::UnregisterCallback(PrefChangedFunc aCallback,
                                const char* aPref,
                                void* aClosure)
{
  if (!sPreferences && sShutdown) {
    return NS_OK; // Observers have been released automatically.
  }
  NS_ENSURE_TRUE(sPreferences, NS_ERROR_NOT_AVAILABLE);

  ValueObserverHashKey hashKey(aPref, aCallback);
  nsRefPtr<ValueObserver> observer;
  gObserverTable->Get(&hashKey, getter_AddRefs(observer));
  if (!observer) {
    return NS_OK;
  }

  observer->RemoveClosure(aClosure);
  if (observer->HasNoClosures()) {
    // Delete the callback since its list of closures is empty.
    gObserverTable->Remove(observer);
  }
  return NS_OK;
}

static int BoolVarChanged(const char* aPref, void* aClosure)
{
  CacheData* cache = static_cast<CacheData*>(aClosure);
  *((bool*)cache->cacheLocation) =
    Preferences::GetBool(aPref, cache->defaultValueBool);
  return 0;
}

// static
nsresult
Preferences::AddBoolVarCache(bool* aCache,
                             const char* aPref,
                             bool aDefault)
{
  NS_ASSERTION(aCache, "aCache must not be NULL");
  *aCache = GetBool(aPref, aDefault);
  CacheData* data = new CacheData();
  data->cacheLocation = aCache;
  data->defaultValueBool = aDefault;
  gCacheData->AppendElement(data);
  return RegisterCallback(BoolVarChanged, aPref, data);
}

static int IntVarChanged(const char* aPref, void* aClosure)
{
  CacheData* cache = static_cast<CacheData*>(aClosure);
  *((int32_t*)cache->cacheLocation) =
    Preferences::GetInt(aPref, cache->defaultValueInt);
  return 0;
}

// static
nsresult
Preferences::AddIntVarCache(int32_t* aCache,
                            const char* aPref,
                            int32_t aDefault)
{
  NS_ASSERTION(aCache, "aCache must not be NULL");
  *aCache = Preferences::GetInt(aPref, aDefault);
  CacheData* data = new CacheData();
  data->cacheLocation = aCache;
  data->defaultValueInt = aDefault;
  gCacheData->AppendElement(data);
  return RegisterCallback(IntVarChanged, aPref, data);
}

static int UintVarChanged(const char* aPref, void* aClosure)
{
  CacheData* cache = static_cast<CacheData*>(aClosure);
  *((uint32_t*)cache->cacheLocation) =
    Preferences::GetUint(aPref, cache->defaultValueUint);
  return 0;
}

// static
nsresult
Preferences::AddUintVarCache(uint32_t* aCache,
                             const char* aPref,
                             uint32_t aDefault)
{
  NS_ASSERTION(aCache, "aCache must not be NULL");
  *aCache = Preferences::GetUint(aPref, aDefault);
  CacheData* data = new CacheData();
  data->cacheLocation = aCache;
  data->defaultValueUint = aDefault;
  gCacheData->AppendElement(data);
  return RegisterCallback(UintVarChanged, aPref, data);
}

// static
nsresult
Preferences::GetDefaultBool(const char* aPref, bool* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_GetBoolPref(aPref, aResult, true);
}

// static
nsresult
Preferences::GetDefaultInt(const char* aPref, int32_t* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_GetIntPref(aPref, aResult, true);
}

// static
nsresult
Preferences::GetDefaultCString(const char* aPref, nsACString* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsAutoCString result;
  nsresult rv = PREF_CopyCharPref(aPref, getter_Copies(result), true);
  if (NS_SUCCEEDED(rv)) {
    *aResult = result;
  }
  return rv;
}

// static
nsresult
Preferences::GetDefaultString(const char* aPref, nsAString* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsAutoCString result;
  nsresult rv = PREF_CopyCharPref(aPref, getter_Copies(result), true);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(result, *aResult);
  }
  return rv;
}

// static
nsresult
Preferences::GetDefaultLocalizedCString(const char* aPref,
                                        nsACString* aResult)
{
  nsAutoString result;
  nsresult rv = GetDefaultLocalizedString(aPref, &result);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF16toUTF8(result, *aResult);
  }
  return rv;
}

// static
nsresult
Preferences::GetDefaultLocalizedString(const char* aPref,
                                       nsAString* aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsCOMPtr<nsIPrefLocalizedString> prefLocalString;
  nsresult rv =
    sDefaultRootBranch->GetComplexValue(aPref,
                                        NS_GET_IID(nsIPrefLocalizedString),
                                        getter_AddRefs(prefLocalString));
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(prefLocalString, "Succeeded but the result is NULL");
    prefLocalString->GetData(getter_Copies(*aResult));
  }
  return rv;
}

// static
nsAdoptingString
Preferences::GetDefaultString(const char* aPref)
{
  nsAdoptingString result;
  GetDefaultString(aPref, &result);
  return result;
}

// static
nsAdoptingCString
Preferences::GetDefaultCString(const char* aPref)
{
  nsAdoptingCString result;
  PREF_CopyCharPref(aPref, getter_Copies(result), true);
  return result;
}

// static
nsAdoptingString
Preferences::GetDefaultLocalizedString(const char* aPref)
{
  nsAdoptingString result;
  GetDefaultLocalizedString(aPref, &result);
  return result;
}

// static
nsAdoptingCString
Preferences::GetDefaultLocalizedCString(const char* aPref)
{
  nsAdoptingCString result;
  GetDefaultLocalizedCString(aPref, &result);
  return result;
}

// static
nsresult
Preferences::GetDefaultComplex(const char* aPref, const nsIID &aType,
                               void** aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sDefaultRootBranch->GetComplexValue(aPref, aType, aResult);
}

// static
int32_t
Preferences::GetDefaultType(const char* aPref)
{
  NS_ENSURE_TRUE(InitStaticMembers(), nsIPrefBranch::PREF_INVALID);
  int32_t result;
  return NS_SUCCEEDED(sDefaultRootBranch->GetPrefType(aPref, &result)) ?
    result : nsIPrefBranch::PREF_INVALID;
}

} // namespace mozilla
