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
 * Brian Nesse <bnesse@netscape.com>
 */

#include "nsPrefBranch.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "prefapi.h"
#include "prmem.h"

#include "nsIFileSpec.h"  // this should be removed eventually
#include "prefapi_private_data.h"

// Definitions
struct EnumerateData {
    const char *parent;
    nsVoidArray *pref_list;
};


// Prototypes
extern "C" PrefResult pref_UnlockPref(const char *key);
PR_STATIC_CALLBACK(PRIntn) pref_enumChild(PLHashEntry *he, int i, void *arg);
static int PR_CALLBACK NotifyObserver(const char *newpref, void *data);

// this needs to be removed!
static nsresult _convertRes(int res)
//---------------------------------------------------------------------------
{
  switch (res) {
    case PREF_OUT_OF_MEMORY:
      return NS_ERROR_OUT_OF_MEMORY;

    case PREF_NOT_INITIALIZED:
      return NS_ERROR_NOT_INITIALIZED;

    case PREF_BAD_PARAMETER:
      return NS_ERROR_INVALID_ARG;

    case PREF_TYPE_CHANGE_ERR:
    case PREF_ERROR:
    case PREF_BAD_LOCKFILE:
	case PREF_DEFAULT_VALUE_NOT_INITIALIZED:
      return NS_ERROR_UNEXPECTED;

    case PREF_VALUECHANGED:
      return 1;
  }

  NS_ASSERTION((res >= PREF_DEFAULT_VALUE_NOT_INITIALIZED) && (res <= PREF_PROFILE_UPGRADE), "you added a new error code to prefapi.h and didn't update _convertRes");
  return NS_OK;
}

#pragma mark -

/*
 * Constructor/Destructor
 */

nsPrefBranch::nsPrefBranch(const char *aPrefRoot, PRBool aDefaultBranch)
  : mObservers(nsnull)
{
  NS_INIT_ISUPPORTS();

  mPrefRoot = aPrefRoot;
  mPrefRootLength = mPrefRoot.Length();
  mIsDefault = aDefaultBranch;
}

nsPrefBranch::~nsPrefBranch()
{
  if (mObservers) {
    // unregister the observers
    PRUint32 count = 0;
    nsresult rv;

    rv = mObservers->Count(&count);
    if (NS_SUCCEEDED(rv)) {
      PRUint32 i;
      nsCOMPtr<nsIObserver> obs;
      nsCAutoString domain;
      for (i=0; i< count; i++) {
        rv = mObservers->QueryElementAt(i, NS_GET_IID(nsIObserver), getter_AddRefs(obs));
        if (NS_SUCCEEDED(rv)) {
          mObserverDomains.CStringAt(i, domain);
          PREF_UnregisterCallback(domain, NotifyObserver, obs);
        }
      }

      // clear the last reference
      obs = nsnull;

      // now empty the observer arrays in bulk
      mObservers->Clear();
      mObserverDomains.Clear();
    }
  }
}


/*
 * nsISupports Implementation
 */

NS_IMPL_THREADSAFE_ADDREF(nsPrefBranch)
NS_IMPL_THREADSAFE_RELEASE(nsPrefBranch)

NS_INTERFACE_MAP_BEGIN(nsPrefBranch)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrefBranch)
    NS_INTERFACE_MAP_ENTRY(nsIPrefBranch)
    NS_INTERFACE_MAP_ENTRY(nsIPrefBranchInternal)
NS_INTERFACE_MAP_END


/*
 * nsIPrefBranch Implementation
 */

NS_IMETHODIMP nsPrefBranch::GetRoot(char * *aRoot)
{
  NS_ENSURE_ARG_POINTER(aRoot);

  mPrefRoot.Truncate(mPrefRootLength);
  *aRoot = mPrefRoot.ToNewCString();
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::GetPrefType(const char *aPrefName, PRInt32 *_retval)
{
  *_retval = PREF_GetPrefType(getPrefName(aPrefName));
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::GetBoolPref(const char *aPrefName, PRBool *_retval)
{
  const char *pref = getPrefName(aPrefName);
  nsresult   rv;

  rv = QueryObserver(pref);
  if (NS_SUCCEEDED(rv)) {
    rv = _convertRes(PREF_GetBoolPref(pref, _retval, mIsDefault));
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::SetBoolPref(const char *aPrefName, PRInt32 aValue)
{
  const char *pref = getPrefName(aPrefName);
  nsresult   rv;

  rv = QueryObserver(pref);
  if (NS_SUCCEEDED(rv)) {
    if (PR_FALSE == mIsDefault) {
      rv = _convertRes(PREF_SetBoolPref(pref, aValue));
    } else {
      rv = _convertRes(PREF_SetDefaultBoolPref(pref, aValue));
    }
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::GetCharPref(const char *aPrefName, char **_retval)
{
  const char *pref = getPrefName(aPrefName);
  nsresult   rv;

  rv = QueryObserver(pref);
  if (NS_SUCCEEDED(rv)) {
    rv = _convertRes(PREF_CopyCharPref(pref, _retval, mIsDefault));
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::SetCharPref(const char *aPrefName, const char *aValue)
{
  const char *pref = getPrefName(aPrefName);
  nsresult   rv;

  rv = QueryObserver(pref);
  if (NS_SUCCEEDED(rv)) {
    if (PR_FALSE == mIsDefault) {
      rv = _convertRes(PREF_SetCharPref(pref, aValue));
    } else {
      rv = _convertRes(PREF_SetDefaultCharPref(pref, aValue));
    }
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::GetIntPref(const char *aPrefName, PRInt32 *_retval)
{
  const char *pref = getPrefName(aPrefName);
  nsresult   rv;

  rv = QueryObserver(pref);
  if (NS_SUCCEEDED(rv)) {
    rv = _convertRes(PREF_GetIntPref(pref, _retval, mIsDefault));
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::SetIntPref(const char *aPrefName, PRInt32 aValue)
{
  const char *pref = getPrefName(aPrefName);
  nsresult   rv;

  rv = QueryObserver(pref);
  if (NS_SUCCEEDED(rv)) {
    if (PR_FALSE == mIsDefault) {
      rv = _convertRes(PREF_SetIntPref(pref, aValue));
    } else {
      rv = _convertRes(PREF_SetDefaultIntPref(pref, aValue));
    }
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::GetComplexValue(const char *aPrefName, const nsIID & aType, void * *_retval)
{
  const char     *pref = getPrefName(aPrefName);
  nsresult       rv;
  nsXPIDLCString utf8String;

  // if we can't get the pref, there's no point in being here
  rv = QueryObserver(pref);
  if (NS_SUCCEEDED(rv)) {
    rv = GetCharPref(pref, getter_Copies(utf8String));
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsILocalFile))) {
    nsCOMPtr<nsILocalFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      rv = file->SetPersistentDescriptor(utf8String);
      if (NS_SUCCEEDED(rv)) {
        nsILocalFile *temp = file;

        NS_ADDREF(temp);
        *_retval = (void *)temp;
        return NS_OK;
      }
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsISupportsWString))) {
    nsCOMPtr<nsISupportsWString> theString(do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      rv = theString->SetData(NS_ConvertASCIItoUCS2(utf8String).get());
      if (NS_SUCCEEDED(rv)) {
        nsISupportsWString *temp = theString;

        NS_ADDREF(temp);
        *_retval = (void *)temp;
        return NS_OK;
      }
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsIPrefLocalizedString))) {
    nsCOMPtr<nsIPrefLocalizedString> theString(do_CreateInstance(NS_PREFLOCALIZEDSTRING_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      rv = theString->SetData(NS_ConvertASCIItoUCS2(utf8String).get());
      if (NS_SUCCEEDED(rv)) {
        nsIPrefLocalizedString *temp = theString;

        NS_ADDREF(temp);
        *_retval = (void *)temp;
      }
    }
    return rv;
  }

  // This is depricated and you should not be using it
  if (aType.Equals(NS_GET_IID(nsIFileSpec))) {
    nsCOMPtr<nsIFileSpec> file(do_CreateInstance(NS_FILESPEC_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      nsIFileSpec *temp = file;
      PRBool      valid;

      file->SetPersistentDescriptorString(utf8String);	// only returns NS_OK
      file->IsValid(&valid);
      if (!valid) {
        /* if the string wasn't a valid persistent descriptor, it might be a valid native path */
        file->SetNativePath(utf8String);
      }
      NS_ADDREF(temp);
      *_retval = (void *)temp;
      return NS_OK;
    }
    return rv;
  }

  NS_WARNING("nsPrefBranch::GetComplexValue - Unsupported interface type");
  return NS_NOINTERFACE;
}

NS_IMETHODIMP nsPrefBranch::SetComplexValue(const char *aPrefName, const nsIID & aType, nsISupports *aValue)
{
  const char *pref = getPrefName(aPrefName);
  nsresult   rv;

  // if we can't get the pref, there's no point in being here
  rv = QueryObserver(pref);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsILocalFile))) {
    nsCOMPtr<nsILocalFile> file = do_QueryInterface(aValue);
    nsXPIDLCString descriptorString;

    rv = file->GetPersistentDescriptor(getter_Copies(descriptorString));
    if (NS_SUCCEEDED(rv)) {
      rv = SetCharPref(pref, descriptorString);
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsISupportsWString))) {
    nsCOMPtr<nsISupportsWString> theString = do_QueryInterface(aValue);

    if (theString) {
      nsXPIDLString wideString;

      rv = theString->GetData(getter_Copies(wideString));
      if (NS_SUCCEEDED(rv)) {
        rv = SetCharPref(pref, NS_ConvertUCS2toUTF8(wideString).get());
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
        rv = SetCharPref(pref, NS_ConvertUCS2toUTF8(wideString).get());
      }
    }
    return rv;
  }

  // This is depricated and you should not be using it
  if (aType.Equals(NS_GET_IID(nsIFileSpec))) {
    nsCOMPtr<nsIFileSpec> file = do_QueryInterface(aValue);
    nsXPIDLCString descriptorString;

    rv = file->GetPersistentDescriptorString(getter_Copies(descriptorString));
    if (NS_SUCCEEDED(rv)) {
      rv = SetCharPref(pref, descriptorString);
    }
    return rv;
  }

  NS_WARNING("nsPrefBranch::SetComplexValue - Unsupported interface type");
  return NS_NOINTERFACE;
}

NS_IMETHODIMP nsPrefBranch::ClearUserPref(const char *aPrefName)
{
  const char *pref = getPrefName(aPrefName);
  nsresult   rv;

  rv = QueryObserver(pref);
  if (NS_SUCCEEDED(rv)) {
    rv = _convertRes(PREF_ClearUserPref(pref));
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::LockPref(const char *aPrefName)
{
  const char *pref = getPrefName(aPrefName);
  nsresult   rv;

  rv = QueryObserver(pref);
  if (NS_SUCCEEDED(rv)) {
    rv = _convertRes(PREF_LockPref(pref));
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::PrefIsLocked(const char *aPrefName, PRBool *_retval)
{
  const char *pref = getPrefName(aPrefName);
  nsresult   rv;

  NS_ENSURE_ARG_POINTER(_retval);

  rv = QueryObserver(pref);
  if (NS_SUCCEEDED(rv)) {
    *_retval = PREF_PrefIsLocked(pref);
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::UnlockPref(const char *aPrefName)
{
  const char *pref = getPrefName(aPrefName);
  nsresult   rv;

  rv = QueryObserver(pref);
  if (NS_SUCCEEDED(rv)) {
    rv = _convertRes(pref_UnlockPref(pref));
  }
  return rv;
}

/* void resetBranch (in string startingAt); */
NS_IMETHODIMP nsPrefBranch::ResetBranch(const char *aStartingAt)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPrefBranch::DeleteBranch(const char *aStartingAt)
{
  return _convertRes(PREF_DeleteBranch(getPrefName(aStartingAt)));
}

NS_IMETHODIMP nsPrefBranch::GetChildList(const char *aStartingAt, PRUint32 *aCount, char ***aChildArray)
{
  char**          outArray;
  char*           theElement;
  PRInt32         numPrefs;
  PRInt32         dwIndex;
  nsresult        rv = NS_OK;
  EnumerateData   ed;
  nsAutoVoidArray prefArray;

  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_ARG_POINTER(aChildArray);

  // this will contain a list of all the pref name strings
  // allocate on the stack for speed

  ed.parent = getPrefName(aStartingAt);
  ed.pref_list = &prefArray;
  PL_HashTableEnumerateEntries(gHashTable, pref_enumChild, &ed);

  // now that we've built up the list, run the callback on
  // all the matching elements
  numPrefs = prefArray.Count();

  outArray = (char **)nsMemory::Alloc(numPrefs * sizeof(char *));
  if (!outArray)
    return NS_ERROR_OUT_OF_MEMORY;

  for (dwIndex = 0; dwIndex < numPrefs; ++dwIndex) {
  	theElement = (char *)prefArray.ElementAt(dwIndex);
    outArray[dwIndex] = (char *)nsMemory::Clone(theElement, strlen(theElement) + 1);
 
    if (!outArray[dwIndex]) {
      // we ran out of memory... this is annoying
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(dwIndex, outArray);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aCount = numPrefs;
  *aChildArray = outArray;
  return NS_OK;
}

#pragma mark -

NS_IMETHODIMP nsPrefBranch::AddObserver(const char *aDomain, nsIObserver *aObserver)
{
  NS_ENSURE_ARG_POINTER(aDomain);
  NS_ENSURE_ARG_POINTER(aObserver);

  if (!mObservers) {
    nsresult rv = NS_OK;
    rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mObservers->AppendElement(aObserver);
  mObserverDomains.AppendCString(nsCAutoString(aDomain));

  PREF_RegisterCallback(aDomain, NotifyObserver, aObserver);
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::RemoveObserver(const char *aDomain, nsIObserver *aObserver)
{
  nsCOMPtr<nsIObserver> obs;
  PRUint32 count;
  PRUint32 i;
  nsresult rv;
  nsCAutoString domain;

  NS_ENSURE_ARG_POINTER(aDomain);
  NS_ENSURE_ARG_POINTER(aObserver);

  if (!mObservers)
    return NS_OK;
    
  // need to find the index of observer, so we can remove it from the domain list too
  rv = mObservers->Count(&count);
  if (NS_FAILED(rv))
    return NS_OK;

  for (i = 0; i < count; i++) {
    rv = mObservers->QueryElementAt(i, NS_GET_IID(nsIObserver), getter_AddRefs(obs));
    if (NS_SUCCEEDED(rv) && obs.get() == aObserver) {
      mObserverDomains.CStringAt(i, domain);
      if (domain.Equals(aDomain))
        break;
    }
  }

  if (i == count)             // not found, just return
    return NS_OK;
    
  // clear the last reference
  obs = nsnull;
    
  mObservers->RemoveElementAt(i);
  mObserverDomains.RemoveCStringAt(i);

  return _convertRes(PREF_UnregisterCallback(aDomain, NotifyObserver, aObserver));
}

static int PR_CALLBACK NotifyObserver(const char *newpref, void *data)
{
    nsCOMPtr<nsIObserver> observer = NS_STATIC_CAST(nsIObserver *, data);
    observer->Observe(observer,
                      NS_LITERAL_STRING("nsPref:changed").get(),
                      NS_ConvertASCIItoUCS2(newpref).get());
    return 0;
}

#pragma mark -

const char *nsPrefBranch::getPrefName(const char *aPrefName)
{
  // for speed, avoid strcpy if we can:
  if (mPrefRoot.IsEmpty())
    return aPrefName;

  // isn't there a better way to do this? this is really kind of gross.
  mPrefRoot.Truncate(mPrefRootLength);

  // only append if anything to append
  if ((nsnull != aPrefName) && (aPrefName != ""))
    mPrefRoot.Append(aPrefName);

  return mPrefRoot.get();
}

PR_STATIC_CALLBACK(PRIntn) pref_enumChild(PLHashEntry *he, int i, void *arg)
{
    EnumerateData *d = (EnumerateData *) arg;
    if (PL_strncmp((char *)he->key, d->parent, PL_strlen(d->parent)) == 0) {
        d->pref_list->AppendElement((void *)he->key);
    }
    return HT_ENUMERATE_NEXT;
}

nsresult nsPrefBranch::QueryObserver(const char *aPrefName)
{
  PRInt32               dwCount;
  PRInt32               dwIndex;
  nsresult              rv;
  nsCAutoString         domain;
  nsCOMPtr<nsIObserver> observer;

  NS_ENSURE_ARG_POINTER(aPrefName);

  if (!mObservers) {
    return NS_OK;
  }

  dwCount = mObserverDomains.Count();
  for (dwIndex = 0; dwIndex < dwCount; ++dwIndex) {
    mObserverDomains.CStringAt(dwIndex, domain);
    if (domain.Equals(aPrefName)) {
      rv = mObservers->QueryElementAt(dwIndex, NS_GET_IID(nsIObserver), getter_AddRefs(observer));
      if (NS_SUCCEEDED(rv)) {
        return observer->Observe(NS_STATIC_CAST(nsIPrefBranch *, this),
                                 NS_LITERAL_STRING("nsPref:change-request").get(),
                                 NS_ConvertASCIItoUCS2(aPrefName).get());
      }
    }
  }

  return NS_OK;
}

#pragma mark -

nsPrefLocalizedString::nsPrefLocalizedString()
: mUnicodeString(nsnull)
{
  nsresult rv;

  NS_INIT_REFCNT();

  mUnicodeString = do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID, &rv);
}

nsPrefLocalizedString::~nsPrefLocalizedString()
{
}


/*
 * nsISupports Implementation
 */

NS_IMPL_THREADSAFE_ADDREF(nsPrefLocalizedString)
NS_IMPL_THREADSAFE_RELEASE(nsPrefLocalizedString)

NS_INTERFACE_MAP_BEGIN(nsPrefLocalizedString)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrefLocalizedString)
    NS_INTERFACE_MAP_ENTRY(nsIPrefLocalizedString)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWString)
NS_INTERFACE_MAP_END

nsresult nsPrefLocalizedString::Init()
{
  nsresult rv;
  mUnicodeString = do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID, &rv);

  return rv;
}

