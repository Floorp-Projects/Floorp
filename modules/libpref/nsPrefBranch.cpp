/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "nsXULAppAPI.h"

#include "nsPrefBranch.h"
#include "nsILocalFile.h" // nsILocalFile used for backwards compatibility
#include "nsIObserverService.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIDirectoryService.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsPrintfCString.h"
#include "nsIStringBundle.h"
#include "prefapi.h"
#include "PLDHashTable.h"

#include "nsCRT.h"
#include "mozilla/Services.h"

#include "prefapi_private_data.h"

#ifdef MOZ_CRASHREPORTER
#include "nsICrashReporter.h"
#endif

#include "nsIConsoleService.h"

#ifdef DEBUG
#define ENSURE_MAIN_PROCESS(message, pref) do {                                \
  if (GetContentChild()) {                                                     \
    nsPrintfCString msg("ENSURE_MAIN_PROCESS failed. %s %s", message, pref);   \
    NS_ERROR(msg.get());                                                       \
    return NS_ERROR_NOT_AVAILABLE;                                             \
  }                                                                            \
} while (0);
#else
#define ENSURE_MAIN_PROCESS(message, pref)                                     \
  if (GetContentChild()) {                                                     \
    return NS_ERROR_NOT_AVAILABLE;                                             \
  }
#endif

using mozilla::dom::ContentChild;

static ContentChild*
GetContentChild()
{
  if (XRE_IsContentProcess()) {
    ContentChild* cpc = ContentChild::GetSingleton();
    if (!cpc) {
      MOZ_CRASH("Content Protocol is NULL!  We're going to crash!");
    }
    return cpc;
  }
  return nullptr;
}

/*
 * Constructor/Destructor
 */

nsPrefBranch::nsPrefBranch(const char *aPrefRoot, bool aDefaultBranch)
  : mPrefRoot(aPrefRoot)
  , mIsDefault(aDefaultBranch)
  , mFreeingObserverList(false)
  , mObservers()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    ++mRefCnt;    // Our refcnt must be > 0 when we call this, or we'll get deleted!
    // add weak so we don't have to clean up at shutdown
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);
    --mRefCnt;
  }
}

nsPrefBranch::~nsPrefBranch()
{
  freeObserverList();

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService)
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
}


/*
 * nsISupports Implementation
 */

NS_IMPL_ADDREF(nsPrefBranch)
NS_IMPL_RELEASE(nsPrefBranch)

NS_INTERFACE_MAP_BEGIN(nsPrefBranch)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrefBranch)
  NS_INTERFACE_MAP_ENTRY(nsIPrefBranch)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIPrefBranch2, !mIsDefault)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIPrefBranchInternal, !mIsDefault)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END


/*
 * nsIPrefBranch Implementation
 */

NS_IMETHODIMP nsPrefBranch::GetRoot(char **aRoot)
{
  NS_ENSURE_ARG_POINTER(aRoot);
  *aRoot = ToNewCString(mPrefRoot);
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::GetPrefType(const char *aPrefName, int32_t *_retval)
{
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = getPrefName(aPrefName);
  switch (PREF_GetPrefType(pref.get())) {
    case PrefType::String:
      *_retval = PREF_STRING;
      break;
    case PrefType::Int:
      *_retval = PREF_INT;
      break;
    case PrefType::Bool:
      *_retval = PREF_BOOL;
        break;
    case PrefType::Invalid:
    default:
      *_retval = PREF_INVALID;
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::GetBoolPrefWithDefault(const char *aPrefName,
                                                   bool aDefaultValue,
                                                   uint8_t _argc, bool *_retval)
{
  nsresult rv = GetBoolPref(aPrefName, _retval);

  if (NS_FAILED(rv) && _argc == 1) {
    *_retval = aDefaultValue;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP nsPrefBranch::GetBoolPref(const char *aPrefName, bool *_retval)
{
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = getPrefName(aPrefName);
  return PREF_GetBoolPref(pref.get(), _retval, mIsDefault);
}

NS_IMETHODIMP nsPrefBranch::SetBoolPref(const char *aPrefName, bool aValue)
{
  ENSURE_MAIN_PROCESS("Cannot SetBoolPref from content process:", aPrefName);
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = getPrefName(aPrefName);
  return PREF_SetBoolPref(pref.get(), aValue, mIsDefault);
}

NS_IMETHODIMP nsPrefBranch::GetFloatPrefWithDefault(const char *aPrefName,
                                                    float aDefaultValue,
                                                    uint8_t _argc, float *_retval)
{
  nsresult rv = GetFloatPref(aPrefName, _retval);

  if (NS_FAILED(rv) && _argc == 1) {
    *_retval = aDefaultValue;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP nsPrefBranch::GetFloatPref(const char *aPrefName, float *_retval)
{
  NS_ENSURE_ARG(aPrefName);
  nsAutoCString stringVal;
  nsresult rv = GetCharPref(aPrefName, getter_Copies(stringVal));
  if (NS_SUCCEEDED(rv)) {
    *_retval = stringVal.ToFloat(&rv);
  }

  return rv;
}

NS_IMETHODIMP nsPrefBranch::GetCharPrefWithDefault(const char *aPrefName,
                                                   const char *aDefaultValue,
                                                   uint8_t _argc, char **_retval)
{
  nsresult rv = GetCharPref(aPrefName, _retval);

  if (NS_FAILED(rv) && _argc == 1) {
    NS_ENSURE_ARG(aDefaultValue);
    *_retval = NS_strdup(aDefaultValue);
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP nsPrefBranch::GetCharPref(const char *aPrefName, char **_retval)
{
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = getPrefName(aPrefName);
  return PREF_CopyCharPref(pref.get(), _retval, mIsDefault);
}

NS_IMETHODIMP nsPrefBranch::SetCharPref(const char *aPrefName, const char *aValue)
{
  nsresult rv = CheckSanityOfStringLength(aPrefName, aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return SetCharPrefInternal(aPrefName, aValue);
}

nsresult nsPrefBranch::SetCharPrefInternal(const char *aPrefName, const char *aValue)

{
  ENSURE_MAIN_PROCESS("Cannot SetCharPref from content process:", aPrefName);
  NS_ENSURE_ARG(aPrefName);
  NS_ENSURE_ARG(aValue);
  const PrefName& pref = getPrefName(aPrefName);
  return PREF_SetCharPref(pref.get(), aValue, mIsDefault);
}

NS_IMETHODIMP nsPrefBranch::GetStringPref(const char *aPrefName,
                                          const nsACString& aDefaultValue,
                                          uint8_t _argc,
                                          nsACString& _retval)
{
  nsXPIDLCString utf8String;
  nsresult rv = GetCharPref(aPrefName, getter_Copies(utf8String));
  if (NS_SUCCEEDED(rv)) {
    _retval = utf8String;
    return rv;
  }

  if (_argc == 1) {
    _retval = aDefaultValue;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP nsPrefBranch::SetStringPref(const char *aPrefName, const nsACString& aValue)
{
  nsresult rv = CheckSanityOfStringLength(aPrefName, aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return SetCharPrefInternal(aPrefName, PromiseFlatCString(aValue).get());
}

NS_IMETHODIMP nsPrefBranch::GetIntPrefWithDefault(const char *aPrefName,
                                                  int32_t aDefaultValue,
                                                  uint8_t _argc, int32_t *_retval)
{
  nsresult rv = GetIntPref(aPrefName, _retval);

  if (NS_FAILED(rv) && _argc == 1) {
    *_retval = aDefaultValue;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP nsPrefBranch::GetIntPref(const char *aPrefName, int32_t *_retval)
{
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = getPrefName(aPrefName);
  return PREF_GetIntPref(pref.get(), _retval, mIsDefault);
}

NS_IMETHODIMP nsPrefBranch::SetIntPref(const char *aPrefName, int32_t aValue)
{
  ENSURE_MAIN_PROCESS("Cannot SetIntPref from content process:", aPrefName);
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = getPrefName(aPrefName);
  return PREF_SetIntPref(pref.get(), aValue, mIsDefault);
}

NS_IMETHODIMP nsPrefBranch::GetComplexValue(const char *aPrefName, const nsIID & aType, void **_retval)
{
  NS_ENSURE_ARG(aPrefName);

  nsresult       rv;
  nsXPIDLCString utf8String;

  // we have to do this one first because it's different than all the rest
  if (aType.Equals(NS_GET_IID(nsIPrefLocalizedString))) {
    nsCOMPtr<nsIPrefLocalizedString> theString(do_CreateInstance(NS_PREFLOCALIZEDSTRING_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;

    const PrefName& pref = getPrefName(aPrefName);
    bool    bNeedDefault = false;

    if (mIsDefault) {
      bNeedDefault = true;
    } else {
      // if there is no user (or locked) value
      if (!PREF_HasUserPref(pref.get()) && !PREF_PrefIsLocked(pref.get())) {
        bNeedDefault = true;
      }
    }

    // if we need to fetch the default value, do that instead, otherwise use the
    // value we pulled in at the top of this function
    if (bNeedDefault) {
      nsXPIDLString utf16String;
      rv = GetDefaultFromPropertiesFile(pref.get(), getter_Copies(utf16String));
      if (NS_SUCCEEDED(rv)) {
        theString->SetData(utf16String.get());
      }
    } else {
      rv = GetCharPref(aPrefName, getter_Copies(utf8String));
      if (NS_SUCCEEDED(rv)) {
        theString->SetData(NS_ConvertUTF8toUTF16(utf8String).get());
      }
    }

    if (NS_SUCCEEDED(rv)) {
      theString.forget(reinterpret_cast<nsIPrefLocalizedString**>(_retval));
    }

    return rv;
  }

  // if we can't get the pref, there's no point in being here
  rv = GetCharPref(aPrefName, getter_Copies(utf8String));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // also check nsILocalFile, for backwards compatibility
  if (aType.Equals(NS_GET_IID(nsIFile)) || aType.Equals(NS_GET_IID(nsILocalFile))) {
    if (GetContentChild()) {
      NS_ERROR("cannot get nsIFile pref from content process");
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsIFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      rv = file->SetPersistentDescriptor(utf8String);
      if (NS_SUCCEEDED(rv)) {
        file.forget(reinterpret_cast<nsIFile**>(_retval));
        return NS_OK;
      }
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsIRelativeFilePref))) {
    if (GetContentChild()) {
      NS_ERROR("cannot get nsIRelativeFilePref from content process");
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsACString::const_iterator keyBegin, strEnd;
    utf8String.BeginReading(keyBegin);
    utf8String.EndReading(strEnd);    

    // The pref has the format: [fromKey]a/b/c
    if (*keyBegin++ != '[')        
      return NS_ERROR_FAILURE;
    nsACString::const_iterator keyEnd(keyBegin);
    if (!FindCharInReadable(']', keyEnd, strEnd))
      return NS_ERROR_FAILURE;
    nsAutoCString key(Substring(keyBegin, keyEnd));
    
    nsCOMPtr<nsIFile> fromFile;
    nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;
    rv = directoryService->Get(key.get(), NS_GET_IID(nsIFile), getter_AddRefs(fromFile));
    if (NS_FAILED(rv))
      return rv;
    
    nsCOMPtr<nsIFile> theFile;
    rv = NS_NewNativeLocalFile(EmptyCString(), true, getter_AddRefs(theFile));
    if (NS_FAILED(rv))
      return rv;
    rv = theFile->SetRelativeDescriptor(fromFile, Substring(++keyEnd, strEnd));
    if (NS_FAILED(rv))
      return rv;
    nsCOMPtr<nsIRelativeFilePref> relativePref;
    rv = NS_NewRelativeFilePref(theFile, key, getter_AddRefs(relativePref));
    if (NS_FAILED(rv))
      return rv;

    relativePref.forget(reinterpret_cast<nsIRelativeFilePref**>(_retval));
    return NS_OK;
  }

  if (aType.Equals(NS_GET_IID(nsISupportsString))) {
    nsCOMPtr<nsISupportsString> theString(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      // Debugging to see why we end up with very long strings here with
      // some addons, see bug 836263.
      nsAutoString wdata;
      if (!AppendUTF8toUTF16(utf8String, wdata, mozilla::fallible)) {
#ifdef MOZ_CRASHREPORTER
        nsCOMPtr<nsICrashReporter> cr =
          do_GetService("@mozilla.org/toolkit/crash-reporter;1");
        if (cr) {
          cr->AnnotateCrashReport(NS_LITERAL_CSTRING("bug836263-size"),
                                  nsPrintfCString("%x", utf8String.Length()));
          cr->RegisterAppMemory(uint64_t(utf8String.BeginReading()),
                                std::min(0x1000U, utf8String.Length()));
        }
#endif
        MOZ_CRASH("bug836263");
      }
      theString->SetData(wdata);
      theString.forget(reinterpret_cast<nsISupportsString**>(_retval));
    }
    return rv;
  }

  NS_WARNING("nsPrefBranch::GetComplexValue - Unsupported interface type");
  return NS_NOINTERFACE;
}

nsresult nsPrefBranch::CheckSanityOfStringLength(const char* aPrefName, const char* aValue) {
  if (!aValue) {
    return NS_OK;
  }
  return CheckSanityOfStringLength(aPrefName, strlen(aValue));
}

nsresult nsPrefBranch::CheckSanityOfStringLength(const char* aPrefName, const nsAString& aValue) {
  return CheckSanityOfStringLength(aPrefName, aValue.Length());
}

nsresult nsPrefBranch::CheckSanityOfStringLength(const char* aPrefName, const nsACString& aValue) {
  return CheckSanityOfStringLength(aPrefName, aValue.Length());
}

nsresult nsPrefBranch::CheckSanityOfStringLength(const char* aPrefName, const uint32_t aLength) {
  if (aLength > MAX_PREF_LENGTH) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (aLength <= MAX_ADVISABLE_PREF_LENGTH) {
    return NS_OK;
  }
  nsresult rv;
  nsCOMPtr<nsIConsoleService> console = do_GetService("@mozilla.org/consoleservice;1", &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoCString message(nsPrintfCString("Warning: attempting to write %d bytes to preference %s. This is bad "
                                        "for general performance and memory usage. Such an amount of data "
                                        "should rather be written to an external file. This preference will "
                                        "not be sent to any content processes.",
                                        aLength,
                                        getPrefName(aPrefName).get()));
  rv = console->LogStringMessage(NS_ConvertUTF8toUTF16(message).get());
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

/*static*/
void nsPrefBranch::ReportToConsole(const nsAString& aMessage)
{
  nsresult rv;
  nsCOMPtr<nsIConsoleService> console = do_GetService("@mozilla.org/consoleservice;1", &rv);
  if (NS_FAILED(rv)) {
    return;
  }
  nsAutoString message(aMessage);
  console->LogStringMessage(message.get());
}

NS_IMETHODIMP nsPrefBranch::SetComplexValue(const char *aPrefName, const nsIID & aType, nsISupports *aValue)
{
  ENSURE_MAIN_PROCESS("Cannot SetComplexValue from content process:", aPrefName);
  NS_ENSURE_ARG(aPrefName);

  nsresult   rv = NS_NOINTERFACE;

  // also check nsILocalFile, for backwards compatibility
  if (aType.Equals(NS_GET_IID(nsIFile)) || aType.Equals(NS_GET_IID(nsILocalFile))) {
    nsCOMPtr<nsIFile> file = do_QueryInterface(aValue);
    if (!file)
      return NS_NOINTERFACE;
    nsAutoCString descriptorString;

    rv = file->GetPersistentDescriptor(descriptorString);
    if (NS_SUCCEEDED(rv)) {
      rv = SetCharPrefInternal(aPrefName, descriptorString.get());
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsIRelativeFilePref))) {
    nsCOMPtr<nsIRelativeFilePref> relFilePref = do_QueryInterface(aValue);
    if (!relFilePref)
      return NS_NOINTERFACE;

    nsCOMPtr<nsIFile> file;
    relFilePref->GetFile(getter_AddRefs(file));
    if (!file)
      return NS_NOINTERFACE;
    nsAutoCString relativeToKey;
    (void) relFilePref->GetRelativeToKey(relativeToKey);

    nsCOMPtr<nsIFile> relativeToFile;
    nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;
    rv = directoryService->Get(relativeToKey.get(), NS_GET_IID(nsIFile), getter_AddRefs(relativeToFile));
    if (NS_FAILED(rv))
      return rv;

    nsAutoCString relDescriptor;
    rv = file->GetRelativeDescriptor(relativeToFile, relDescriptor);
    if (NS_FAILED(rv))
      return rv;

    nsAutoCString descriptorString;
    descriptorString.Append('[');
    descriptorString.Append(relativeToKey);
    descriptorString.Append(']');
    descriptorString.Append(relDescriptor);
    return SetCharPrefInternal(aPrefName, descriptorString.get());
  }

  if (aType.Equals(NS_GET_IID(nsISupportsString))) {
    nsCOMPtr<nsISupportsString> theString = do_QueryInterface(aValue);

    if (theString) {
      nsString wideString;

      rv = theString->GetData(wideString);
      if (NS_SUCCEEDED(rv)) {
        // Check sanity of string length before any lengthy conversion
        rv = CheckSanityOfStringLength(aPrefName, wideString);
        if (NS_FAILED(rv)) {
          return rv;
        }
        rv = SetCharPrefInternal(aPrefName, NS_ConvertUTF16toUTF8(wideString).get());
      }
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsIPrefLocalizedString))) {
    nsCOMPtr<nsIPrefLocalizedString> theString = do_QueryInterface(aValue);

    if (theString) {
      nsXPIDLString wideString;

      rv = theString->GetData(getter_Copies(wideString));
      if (NS_SUCCEEDED(rv)) {
        // Check sanity of string length before any lengthy conversion
        rv = CheckSanityOfStringLength(aPrefName, wideString);
        if (NS_FAILED(rv)) {
          return rv;
        }
        rv = SetCharPrefInternal(aPrefName, NS_ConvertUTF16toUTF8(wideString).get());
      }
    }
    return rv;
  }

  NS_WARNING("nsPrefBranch::SetComplexValue - Unsupported interface type");
  return NS_NOINTERFACE;
}

NS_IMETHODIMP nsPrefBranch::ClearUserPref(const char *aPrefName)
{
  ENSURE_MAIN_PROCESS("Cannot ClearUserPref from content process:", aPrefName);
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = getPrefName(aPrefName);
  return PREF_ClearUserPref(pref.get());
}

NS_IMETHODIMP nsPrefBranch::PrefHasUserValue(const char *aPrefName, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = getPrefName(aPrefName);
  *_retval = PREF_HasUserPref(pref.get());
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::LockPref(const char *aPrefName)
{
  ENSURE_MAIN_PROCESS("Cannot LockPref from content process:", aPrefName);
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = getPrefName(aPrefName);
  return PREF_LockPref(pref.get(), true);
}

NS_IMETHODIMP nsPrefBranch::PrefIsLocked(const char *aPrefName, bool *_retval)
{
  ENSURE_MAIN_PROCESS("Cannot check PrefIsLocked from content process:", aPrefName);
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = getPrefName(aPrefName);
  *_retval = PREF_PrefIsLocked(pref.get());
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::UnlockPref(const char *aPrefName)
{
  ENSURE_MAIN_PROCESS("Cannot UnlockPref from content process:", aPrefName);
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = getPrefName(aPrefName);
  return PREF_LockPref(pref.get(), false);
}

NS_IMETHODIMP nsPrefBranch::ResetBranch(const char *aStartingAt)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPrefBranch::DeleteBranch(const char *aStartingAt)
{
  ENSURE_MAIN_PROCESS("Cannot DeleteBranch from content process:", aStartingAt);
  NS_ENSURE_ARG(aStartingAt);
  const PrefName& pref = getPrefName(aStartingAt);
  return PREF_DeleteBranch(pref.get());
}

NS_IMETHODIMP nsPrefBranch::GetChildList(const char *aStartingAt, uint32_t *aCount, char ***aChildArray)
{
  char            **outArray;
  int32_t         numPrefs;
  int32_t         dwIndex;
  AutoTArray<nsCString, 32> prefArray;

  NS_ENSURE_ARG(aStartingAt);
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_ARG_POINTER(aChildArray);

  *aChildArray = nullptr;
  *aCount = 0;

  // this will contain a list of all the pref name strings
  // allocate on the stack for speed

  const PrefName& parent = getPrefName(aStartingAt);
  size_t parentLen = parent.Length();
  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<PrefHashEntry*>(iter.Get());
    if (strncmp(entry->key, parent.get(), parentLen) == 0) {
      prefArray.AppendElement(entry->key);
    }
  }

  // now that we've built up the list, run the callback on
  // all the matching elements
  numPrefs = prefArray.Length();

  if (numPrefs) {
    outArray = (char **)moz_xmalloc(numPrefs * sizeof(char *));
    if (!outArray)
      return NS_ERROR_OUT_OF_MEMORY;

    for (dwIndex = 0; dwIndex < numPrefs; ++dwIndex) {
      // we need to lop off mPrefRoot in case the user is planning to pass this
      // back to us because if they do we are going to add mPrefRoot again.
      const nsCString& element = prefArray[dwIndex];
      outArray[dwIndex] = (char *)nsMemory::Clone(
        element.get() + mPrefRoot.Length(), element.Length() - mPrefRoot.Length() + 1);

      if (!outArray[dwIndex]) {
        // we ran out of memory... this is annoying
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(dwIndex, outArray);
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    *aChildArray = outArray;
  }
  *aCount = numPrefs;

  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::AddObserver(const char *aDomain, nsIObserver *aObserver, bool aHoldWeak)
{
  PrefCallback *pCallback;

  NS_ENSURE_ARG(aDomain);
  NS_ENSURE_ARG(aObserver);

  // hold a weak reference to the observer if so requested
  if (aHoldWeak) {
    nsCOMPtr<nsISupportsWeakReference> weakRefFactory = do_QueryInterface(aObserver);
    if (!weakRefFactory) {
      // the caller didn't give us a object that supports weak reference... tell them
      return NS_ERROR_INVALID_ARG;
    }

    // Construct a PrefCallback with a weak reference to the observer.
    pCallback = new PrefCallback(aDomain, weakRefFactory, this);

  } else {
    // Construct a PrefCallback with a strong reference to the observer.
    pCallback = new PrefCallback(aDomain, aObserver, this);
  }

  auto p = mObservers.LookupForAdd(pCallback);
  if (p) {
    NS_WARNING("Ignoring duplicate observer.");
    delete pCallback;
    return NS_OK;
  }

  p.OrInsert([&pCallback]() {
    return pCallback;
  });

  // We must pass a fully qualified preference name to the callback
  // aDomain == nullptr is the only possible failure, and we trapped it with
  // NS_ENSURE_ARG above.
  const PrefName& pref = getPrefName(aDomain);
  PREF_RegisterCallback(pref.get(), NotifyObserver, pCallback);
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::RemoveObserver(const char *aDomain, nsIObserver *aObserver)
{
  NS_ENSURE_ARG(aDomain);
  NS_ENSURE_ARG(aObserver);

  nsresult rv = NS_OK;

  // If we're in the middle of a call to freeObserverList, don't process this
  // RemoveObserver call -- the observer in question will be removed soon, if
  // it hasn't been already.
  //
  // It's important that we don't touch mObservers in any way -- even a Get()
  // which returns null might cause the hashtable to resize itself, which will
  // break the iteration in freeObserverList.
  if (mFreeingObserverList)
    return NS_OK;

  // Remove the relevant PrefCallback from mObservers and get an owning
  // pointer to it.  Unregister the callback first, and then let the owning
  // pointer go out of scope and destroy the callback.
  PrefCallback key(aDomain, aObserver, this);
  nsAutoPtr<PrefCallback> pCallback;
  mObservers.Remove(&key, &pCallback);
  if (pCallback) {
    // aDomain == nullptr is the only possible failure, trapped above
    const PrefName& pref = getPrefName(aDomain);
    rv = PREF_UnregisterCallback(pref.get(), NotifyObserver, pCallback);
  }

  return rv;
}

NS_IMETHODIMP nsPrefBranch::Observe(nsISupports *aSubject, const char *aTopic, const char16_t *someData)
{
  // watch for xpcom shutdown and free our observers to eliminate any cyclic references
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    freeObserverList();
  }
  return NS_OK;
}

/* static */
void nsPrefBranch::NotifyObserver(const char *newpref, void *data)
{
  PrefCallback *pCallback = (PrefCallback *)data;

  nsCOMPtr<nsIObserver> observer = pCallback->GetObserver();
  if (!observer) {
    // The observer has expired.  Let's remove this callback.
    pCallback->GetPrefBranch()->RemoveExpiredCallback(pCallback);
    return;
  }

  // remove any root this string may contain so as to not confuse the observer
  // by passing them something other than what they passed us as a topic
  uint32_t len = pCallback->GetPrefBranch()->GetRootLength();
  nsAutoCString suffix(newpref + len);

  observer->Observe(static_cast<nsIPrefBranch *>(pCallback->GetPrefBranch()),
                    NS_PREFBRANCH_PREFCHANGE_TOPIC_ID,
                    NS_ConvertASCIItoUTF16(suffix).get());
}

size_t
nsPrefBranch::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  size_t n = aMallocSizeOf(this);
  n += mPrefRoot.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  n += mObservers.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}

void nsPrefBranch::freeObserverList(void)
{
  // We need to prevent anyone from modifying mObservers while we're iterating
  // over it. In particular, some clients will call RemoveObserver() when
  // they're removed and destructed via the iterator; we set
  // mFreeingObserverList to keep those calls from touching mObservers.
  mFreeingObserverList = true;
  for (auto iter = mObservers.Iter(); !iter.Done(); iter.Next()) {
    nsAutoPtr<PrefCallback>& callback = iter.Data();
    nsPrefBranch *prefBranch = callback->GetPrefBranch();
    const PrefName& pref = prefBranch->getPrefName(callback->GetDomain().get());
    PREF_UnregisterCallback(pref.get(), nsPrefBranch::NotifyObserver, callback);
    iter.Remove();
  }
  mFreeingObserverList = false;
}

void
nsPrefBranch::RemoveExpiredCallback(PrefCallback *aCallback)
{
  NS_PRECONDITION(aCallback->IsExpired(), "Callback should be expired.");
  mObservers.Remove(aCallback);
}

nsresult nsPrefBranch::GetDefaultFromPropertiesFile(const char *aPrefName, char16_t **return_buf)
{
  nsresult rv;

  // the default value contains a URL to a .properties file
    
  nsXPIDLCString propertyFileURL;
  rv = PREF_CopyCharPref(aPrefName, getter_Copies(propertyFileURL), true);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  if (!bundleService)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle(propertyFileURL,
                                   getter_AddRefs(bundle));
  if (NS_FAILED(rv))
    return rv;

  // string names are in unicode
  nsAutoString stringId;
  stringId.AssignASCII(aPrefName);

  return bundle->GetStringFromName(stringId.get(), return_buf);
}

nsPrefBranch::PrefName
nsPrefBranch::getPrefName(const char *aPrefName) const
{
  NS_ASSERTION(aPrefName, "null pref name!");

  // for speed, avoid strcpy if we can:
  if (mPrefRoot.IsEmpty())
    return PrefName(aPrefName);

  return PrefName(mPrefRoot + nsDependentCString(aPrefName));
}

//----------------------------------------------------------------------------
// nsPrefLocalizedString
//----------------------------------------------------------------------------

nsPrefLocalizedString::nsPrefLocalizedString()
{
}

nsPrefLocalizedString::~nsPrefLocalizedString()
{
}


/*
 * nsISupports Implementation
 */

NS_IMPL_ADDREF(nsPrefLocalizedString)
NS_IMPL_RELEASE(nsPrefLocalizedString)

NS_INTERFACE_MAP_BEGIN(nsPrefLocalizedString)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrefLocalizedString)
    NS_INTERFACE_MAP_ENTRY(nsIPrefLocalizedString)
    NS_INTERFACE_MAP_ENTRY(nsISupportsString)
NS_INTERFACE_MAP_END

nsresult nsPrefLocalizedString::Init()
{
  nsresult rv;
  mUnicodeString = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);

  return rv;
}

NS_IMETHODIMP
nsPrefLocalizedString::GetData(char16_t **_retval)
{
  nsAutoString data;

  nsresult rv = GetData(data);
  if (NS_FAILED(rv))
    return rv;
  
  *_retval = ToNewUnicode(data);
  if (!*_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsPrefLocalizedString::SetData(const char16_t *aData)
{
  if (!aData)
    return SetData(EmptyString());
  return SetData(nsDependentString(aData));
}

NS_IMETHODIMP
nsPrefLocalizedString::SetDataWithLength(uint32_t aLength,
                                         const char16_t *aData)
{
  if (!aData)
    return SetData(EmptyString());
  return SetData(Substring(aData, aLength));
}

//----------------------------------------------------------------------------
// nsRelativeFilePref
//----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsRelativeFilePref, nsIRelativeFilePref)

nsRelativeFilePref::nsRelativeFilePref()
{
}

nsRelativeFilePref::~nsRelativeFilePref()
{
}

NS_IMETHODIMP nsRelativeFilePref::GetFile(nsIFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);
  *aFile = mFile;
  NS_IF_ADDREF(*aFile);
  return NS_OK;
}

NS_IMETHODIMP nsRelativeFilePref::SetFile(nsIFile *aFile)
{
  mFile = aFile;
  return NS_OK;
}

NS_IMETHODIMP nsRelativeFilePref::GetRelativeToKey(nsACString& aRelativeToKey)
{
  aRelativeToKey.Assign(mRelativeToKey);
  return NS_OK;
}

NS_IMETHODIMP nsRelativeFilePref::SetRelativeToKey(const nsACString& aRelativeToKey)
{
  mRelativeToKey.Assign(aRelativeToKey);
  return NS_OK;
}

#undef ENSURE_MAIN_PROCESS
