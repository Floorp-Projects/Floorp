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
 * Brian Nesse <bnesse@netscape.com>
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

#include "nsPrefBranch.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIDirectoryService.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStringBundle.h"
#include "prefapi.h"
#include "prmem.h"
#include "pldhash.h"

#include "nsIFileSpec.h"  // this should be removed eventually
#include "prefapi_private_data.h"

// Definitions
struct EnumerateData {
  const char  *parent;
  nsVoidArray *pref_list;
};

struct PrefCallbackData {
  nsIPrefBranch *pBranch;
  nsISupports   *pObserver;
  PRBool        bIsWeakRef;
};


// Prototypes
PR_STATIC_CALLBACK(PLDHashOperator)
  pref_enumChild(PLDHashTable *table, PLDHashEntryHdr *heh,
                 PRUint32 i, void *arg);
PR_STATIC_CALLBACK(nsresult)
  NotifyObserver(const char *newpref, void *data);

/*
 * Constructor/Destructor
 */

nsPrefBranch::nsPrefBranch(const char *aPrefRoot, PRBool aDefaultBranch)
  : mObservers(nsnull)
{
  mPrefRoot = aPrefRoot;
  mPrefRootLength = mPrefRoot.Length();
  mIsDefault = aDefaultBranch;

  nsCOMPtr<nsIObserverService> observerService = 
           do_GetService("@mozilla.org/observer-service;1");
  if (observerService) {
    ++mRefCnt;    // Our refcnt must be > 0 when we call this, or we'll get deleted!
    // add weak so we don't have to clean up at shutdown
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);
    --mRefCnt;
  }
}

nsPrefBranch::~nsPrefBranch()
{
  freeObserverList();
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
  NS_INTERFACE_MAP_ENTRY(nsISecurityPref)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END


/*
 * nsIPrefBranch Implementation
 */

NS_IMETHODIMP nsPrefBranch::GetRoot(char * *aRoot)
{
  NS_ENSURE_ARG_POINTER(aRoot);

  mPrefRoot.Truncate(mPrefRootLength);
  *aRoot = ToNewCString(mPrefRoot);
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::GetPrefType(const char *aPrefName, PRInt32 *_retval)
{
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_FAILED(rv))
    return rv;

  *_retval = PREF_GetPrefType(pref);
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::GetBoolPref(const char *aPrefName, PRBool *_retval)
{
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_SUCCEEDED(rv)) {
    rv = PREF_GetBoolPref(pref, _retval, mIsDefault);
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::SetBoolPref(const char *aPrefName, PRInt32 aValue)
{
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_SUCCEEDED(rv)) {
    rv = PREF_SetBoolPref(pref, aValue, mIsDefault);
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::GetCharPref(const char *aPrefName, char **_retval)
{
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_SUCCEEDED(rv)) {
    rv = PREF_CopyCharPref(pref, _retval, mIsDefault);
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::SetCharPref(const char *aPrefName, const char *aValue)
{
  const char *pref;
  nsresult   rv;

  NS_ENSURE_ARG_POINTER(aValue);
  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_SUCCEEDED(rv)) {
    rv = PREF_SetCharPref(pref, aValue, mIsDefault);
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::GetIntPref(const char *aPrefName, PRInt32 *_retval)
{
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_SUCCEEDED(rv)) {
    rv = PREF_GetIntPref(pref, _retval, mIsDefault);
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::SetIntPref(const char *aPrefName, PRInt32 aValue)
{
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_SUCCEEDED(rv)) {
    rv = PREF_SetIntPref(pref, aValue, mIsDefault);
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::GetComplexValue(const char *aPrefName, const nsIID & aType, void * *_retval)
{
  nsresult       rv;
  nsXPIDLCString utf8String;

  // we have to do this one first because it's different than all the rest
  if (aType.Equals(NS_GET_IID(nsIPrefLocalizedString))) {
    nsCOMPtr<nsIPrefLocalizedString> theString(do_CreateInstance(NS_PREFLOCALIZEDSTRING_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      const char *pref;
      PRBool  bNeedDefault = PR_FALSE;

      rv = getValidatedPrefName(aPrefName, &pref);
      if (NS_FAILED(rv))
        return rv;

      if (mIsDefault) {
        bNeedDefault = PR_TRUE;
      } else {
        // if there is no user (or locked) value
        if (!PREF_HasUserPref(pref) && !PREF_PrefIsLocked(pref)) {
          bNeedDefault = PR_TRUE;
        }
      }

      // if we need to fetch the default value, do that instead, otherwise use the
      // value we pulled in at the top of this function
      if (bNeedDefault) {
        nsXPIDLString utf16String;
        rv = GetDefaultFromPropertiesFile(pref, getter_Copies(utf16String));
        if (NS_SUCCEEDED(rv)) {
          rv = theString->SetData(utf16String.get());
        }
      } else {
        rv = GetCharPref(aPrefName, getter_Copies(utf8String));
        if (NS_SUCCEEDED(rv)) {
          rv = theString->SetData(NS_ConvertUTF8toUCS2(utf8String).get());
        }
      }
      if (NS_SUCCEEDED(rv)) {
        nsIPrefLocalizedString *temp = theString;

        NS_ADDREF(temp);
        *_retval = (void *)temp;
      }
    }

    return rv;
  }

  // if we can't get the pref, there's no point in being here
  rv = GetCharPref(aPrefName, getter_Copies(utf8String));
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

  if (aType.Equals(NS_GET_IID(nsIRelativeFilePref))) {
    nsACString::const_iterator keyBegin, strEnd;
    utf8String.BeginReading(keyBegin);
    utf8String.EndReading(strEnd);    

    // The pref has the format: [fromKey]a/b/c
    if (*keyBegin++ != '[')        
        return NS_ERROR_FAILURE;
    nsACString::const_iterator keyEnd(keyBegin);
    if (!FindCharInReadable(']', keyEnd, strEnd))
        return NS_ERROR_FAILURE;
    nsCAutoString key(Substring(keyBegin, keyEnd));
    
    nsCOMPtr<nsILocalFile> fromFile;        
    nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;
    rv = directoryService->Get(key.get(), NS_GET_IID(nsILocalFile), getter_AddRefs(fromFile));
    if (NS_FAILED(rv))
      return rv;
    
    nsCOMPtr<nsILocalFile> theFile;
    rv = NS_NewNativeLocalFile(nsCString(), PR_TRUE, getter_AddRefs(theFile));
    if (NS_FAILED(rv))
      return rv;
    rv = theFile->SetRelativeDescriptor(fromFile, Substring(++keyEnd, strEnd));
    if (NS_FAILED(rv))
      return rv;
    nsCOMPtr<nsIRelativeFilePref> relativePref;
    rv = NS_NewRelativeFilePref(theFile, key, getter_AddRefs(relativePref));
    if (NS_FAILED(rv))
      return rv;

    *_retval = relativePref;
    NS_ADDREF(NS_STATIC_CAST(nsIRelativeFilePref*, *_retval));
    return NS_OK;
  }

  if (aType.Equals(NS_GET_IID(nsISupportsString))) {
    nsCOMPtr<nsISupportsString> theString(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      rv = theString->SetData(NS_ConvertUTF8toUCS2(utf8String));
      if (NS_SUCCEEDED(rv)) {
        nsISupportsString *temp = theString;

        NS_ADDREF(temp);
        *_retval = (void *)temp;
        return NS_OK;
      }
    }
    return rv;
  }

  // This is deprecated and you should not be using it
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
  nsresult   rv = NS_NOINTERFACE;

  if (aType.Equals(NS_GET_IID(nsILocalFile))) {
    nsCOMPtr<nsILocalFile> file = do_QueryInterface(aValue);
    nsCAutoString descriptorString;

    rv = file->GetPersistentDescriptor(descriptorString);
    if (NS_SUCCEEDED(rv)) {
      rv = SetCharPref(aPrefName, descriptorString.get());
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsIRelativeFilePref))) {
    nsCOMPtr<nsIRelativeFilePref> relFilePref = do_QueryInterface(aValue);
    if (!relFilePref)
      return NS_NOINTERFACE;
    
    nsCOMPtr<nsILocalFile> file;
    relFilePref->GetFile(getter_AddRefs(file));
    if (!file)
      return NS_ERROR_FAILURE;
    nsCAutoString relativeToKey;
    (void) relFilePref->GetRelativeToKey(relativeToKey);

    nsCOMPtr<nsILocalFile> relativeToFile;        
    nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;
    rv = directoryService->Get(relativeToKey.get(), NS_GET_IID(nsILocalFile), getter_AddRefs(relativeToFile));
    if (NS_FAILED(rv))
      return rv;

    nsCAutoString relDescriptor;
    rv = file->GetRelativeDescriptor(relativeToFile, relDescriptor);
    if (NS_FAILED(rv))
      return rv;
    
    nsCAutoString descriptorString;
    descriptorString.Append('[');
    descriptorString.Append(relativeToKey);
    descriptorString.Append(']');
    descriptorString.Append(relDescriptor);
    return SetCharPref(aPrefName, descriptorString.get());
  }

  if (aType.Equals(NS_GET_IID(nsISupportsString))) {
    nsCOMPtr<nsISupportsString> theString = do_QueryInterface(aValue);

    if (theString) {
      nsAutoString wideString;

      rv = theString->GetData(wideString);
      if (NS_SUCCEEDED(rv)) {
        rv = SetCharPref(aPrefName, NS_ConvertUCS2toUTF8(wideString).get());
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
        rv = SetCharPref(aPrefName, NS_ConvertUCS2toUTF8(wideString).get());
      }
    }
    return rv;
  }

  // This is deprecated and you should not be using it
  if (aType.Equals(NS_GET_IID(nsIFileSpec))) {
    nsCOMPtr<nsIFileSpec> file = do_QueryInterface(aValue);
    nsXPIDLCString descriptorString;

    rv = file->GetPersistentDescriptorString(getter_Copies(descriptorString));
    if (NS_SUCCEEDED(rv)) {
      rv = SetCharPref(aPrefName, descriptorString);
    }
    return rv;
  }

  NS_WARNING("nsPrefBranch::SetComplexValue - Unsupported interface type");
  return NS_NOINTERFACE;
}

NS_IMETHODIMP nsPrefBranch::ClearUserPref(const char *aPrefName)
{
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_SUCCEEDED(rv)) {
    rv = PREF_ClearUserPref(pref);
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::PrefHasUserValue(const char *aPrefName, PRBool *_retval)
{
  const char *pref;
  nsresult   rv;

  NS_ENSURE_ARG_POINTER(_retval);

  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_SUCCEEDED(rv)) {
    *_retval = PREF_HasUserPref(pref);
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::LockPref(const char *aPrefName)
{
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_SUCCEEDED(rv)) {
    rv = PREF_LockPref(pref, PR_TRUE);
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::PrefIsLocked(const char *aPrefName, PRBool *_retval)
{
  const char *pref;
  nsresult   rv;

  NS_ENSURE_ARG_POINTER(_retval);

  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_SUCCEEDED(rv)) {
    *_retval = PREF_PrefIsLocked(pref);
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::UnlockPref(const char *aPrefName)
{
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_SUCCEEDED(rv)) {
    rv = PREF_LockPref(pref, PR_FALSE);
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
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aStartingAt, &pref);
  if (NS_SUCCEEDED(rv)) {
    rv = PREF_DeleteBranch(pref);
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::GetChildList(const char *aStartingAt, PRUint32 *aCount, char ***aChildArray)
{
  char**          outArray;
  char*           theElement;
  PRInt32         numPrefs;
  PRInt32         dwIndex;
  EnumerateData   ed;
  nsAutoVoidArray prefArray;

  NS_ENSURE_ARG_POINTER(aStartingAt);
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_ARG_POINTER(aChildArray);

  if (!gHashTable.ops) {
    *aChildArray = nsnull;
    *aCount = 0;
    return NS_ERROR_NOT_INITIALIZED;
  }

  // this will contain a list of all the pref name strings
  // allocate on the stack for speed

  ed.parent = getPrefName(aStartingAt);
  ed.pref_list = &prefArray;
  PL_DHashTableEnumerate(&gHashTable, pref_enumChild, &ed);

  // now that we've built up the list, run the callback on
  // all the matching elements
  numPrefs = prefArray.Count();

  if (numPrefs) {
    outArray = (char **)nsMemory::Alloc(numPrefs * sizeof(char *));
    if (!outArray)
      return NS_ERROR_OUT_OF_MEMORY;

    for (dwIndex = 0; dwIndex < numPrefs; ++dwIndex) {
      // we need to lop off mPrefRoot in case the user is planning to pass this
      // back to us because if they do we are going to add mPrefRoot again.
      theElement = ((char *)prefArray.ElementAt(dwIndex)) + mPrefRootLength;
      outArray[dwIndex] = (char *)nsMemory::Clone(theElement, strlen(theElement) + 1);
 
      if (!outArray[dwIndex]) {
        // we ran out of memory... this is annoying
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(dwIndex, outArray);
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    *aChildArray = outArray;
  } else {
    *aChildArray = nsnull;
  } /* endif */
  *aCount = numPrefs;

  return NS_OK;
}


/*
 *  nsIPrefBranchInternal methods
 */

NS_IMETHODIMP nsPrefBranch::AddObserver(const char *aDomain, nsIObserver *aObserver, PRBool aHoldWeak)
{
  PrefCallbackData *pCallback;
  const char *pref;

  NS_ENSURE_ARG_POINTER(aDomain);
  NS_ENSURE_ARG_POINTER(aObserver);

  if (!mObservers) {
    mObservers = new nsAutoVoidArray();
    if (nsnull == mObservers)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  pCallback = (PrefCallbackData *)nsMemory::Alloc(sizeof(PrefCallbackData));
  if (nsnull == pCallback)
    return NS_ERROR_OUT_OF_MEMORY;

  pCallback->pBranch = NS_STATIC_CAST(nsIPrefBranch *, this);
  pCallback->bIsWeakRef = aHoldWeak;

  // hold a weak reference to the observer if so requested
  nsCOMPtr<nsISupports> observerRef;
  if (aHoldWeak) {
    nsCOMPtr<nsISupportsWeakReference> weakRefFactory = do_QueryInterface(aObserver);
    if (!weakRefFactory) {
      // the caller didn't give us a object that supports weak reference... tell them
      nsMemory::Free(pCallback);
      return NS_ERROR_INVALID_ARG;
    }
    observerRef = do_GetWeakReference(weakRefFactory);
  } else {
    observerRef = aObserver;
  }
  pCallback->pObserver = observerRef;
  NS_ADDREF(pCallback->pObserver);

  mObservers->AppendElement(pCallback);
  mObserverDomains.AppendCString(nsCString(aDomain));

  // We must pass a fully qualified preference name to the callback
  pref = getPrefName(aDomain); // aDomain == nsnull only possible failure, trapped above
  PREF_RegisterCallback(pref, NotifyObserver, pCallback);
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::RemoveObserver(const char *aDomain, nsIObserver *aObserver)
{
  const char *pref;
  PrefCallbackData *pCallback;
  PRInt32 count;
  PRInt32 i;
  nsresult rv;
  nsCAutoString domain;

  NS_ENSURE_ARG_POINTER(aDomain);
  NS_ENSURE_ARG_POINTER(aObserver);

  if (!mObservers)
    return NS_OK;
    
  // need to find the index of observer, so we can remove it from the domain list too
  count = mObservers->Count();
  if (count == 0)
    return NS_OK;

  for (i = 0; i < count; i++) {
    pCallback = (PrefCallbackData *)mObservers->ElementAt(i);
    if (pCallback) {
     nsCOMPtr<nsISupports> observerRef;
     if (pCallback->bIsWeakRef) {
       nsCOMPtr<nsISupportsWeakReference> weakRefFactory = do_QueryInterface(aObserver);
       if (weakRefFactory)
         observerRef = do_GetWeakReference(aObserver);
     }
     if (!observerRef)
       observerRef = aObserver;

      if (pCallback->pObserver == observerRef) {
        mObserverDomains.CStringAt(i, domain);
        if (domain.Equals(aDomain)) {
          // We must pass a fully qualified preference name to remove the callback
          pref = getPrefName(aDomain); // aDomain == nsnull only possible failure, trapped above
          rv = PREF_UnregisterCallback(pref, NotifyObserver, pCallback);
          if (NS_SUCCEEDED(rv)) {
            // Remove this observer from our array so that nobody else can remove
            // what we're trying to remove ourselves right now.
            mObservers->RemoveElementAt(i);
            mObserverDomains.RemoveCStringAt(i);
            NS_RELEASE(pCallback->pObserver);
            nsMemory::Free(pCallback);
          }
          return rv;
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  // watch for xpcom shutdown and free our observers to eliminate any cyclic references
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    freeObserverList();
  }
  return NS_OK;
}

PR_STATIC_CALLBACK(nsresult) NotifyObserver(const char *newpref, void *data)
{
  PrefCallbackData *pData = (PrefCallbackData *)data;
  nsPrefBranch *prefBranch = NS_STATIC_CAST(nsPrefBranch *, pData->pBranch);

  // remove any root this string may contain so as to not confuse the observer
  // by passing them something other than what they passed us as a topic
  PRUint32 len = prefBranch->GetRootLength();
  nsCAutoString suffix(newpref + len);  

  nsCOMPtr<nsIObserver> observer;
  if (pData->bIsWeakRef) {
    nsIWeakReference *weakRef = NS_STATIC_CAST(nsIWeakReference *, pData->pObserver);
    observer = do_QueryReferent(weakRef);
    if (!observer) {
      // this weak referenced observer went away, remove them from the list
      nsCOMPtr<nsIPrefBranchInternal> pbi = do_QueryInterface(pData->pBranch);
      if (pbi) {
        observer = NS_STATIC_CAST(nsIObserver *, pData->pObserver);
        pbi->RemoveObserver(newpref, observer);
      }
      return NS_OK;
    }
  } else
    observer = NS_STATIC_CAST(nsIObserver *, pData->pObserver);

  observer->Observe(pData->pBranch, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID,
                    NS_ConvertASCIItoUCS2(suffix).get());
  return NS_OK;
}


void nsPrefBranch::freeObserverList(void)
{
  const char *pref;
  PrefCallbackData *pCallback;

  if (mObservers) {
    // unregister the observers
    PRInt32 count;

    count = mObservers->Count();
    if (count > 0) {
      PRInt32 i;
      nsCAutoString domain;
      for (i = 0; i < count; ++i) {
        pCallback = (PrefCallbackData *)mObservers->ElementAt(i);
        if (pCallback) {
          mObserverDomains.CStringAt(i, domain);
          // We must pass a fully qualified preference name to remove the callback
          pref = getPrefName(domain.get()); // can't fail because domain must be valid
          // Remove this observer from our array so that nobody else can remove
          // what we're trying to remove right now.
          mObservers->ReplaceElementAt(nsnull, i);
          PREF_UnregisterCallback(pref, NotifyObserver, pCallback);
          NS_RELEASE(pCallback->pObserver);
          nsMemory::Free(pCallback);
        }
      }

      // now empty the observer domains array in bulk
      mObserverDomains.Clear();
    }
    delete mObservers;
    mObservers = 0;
  }
}
 
nsresult nsPrefBranch::GetDefaultFromPropertiesFile(const char *aPrefName, PRUnichar **return_buf)
{
  nsresult rv;

  // the default value contains a URL to a .properties file
    
  nsXPIDLCString propertyFileURL;
  rv = PREF_CopyCharPref(aPrefName, getter_Copies(propertyFileURL), PR_TRUE);
  if (NS_FAILED(rv))
    return rv;
    
  nsCOMPtr<nsIStringBundleService> bundleService =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle(propertyFileURL,
                                   getter_AddRefs(bundle));
  if (NS_FAILED(rv))
    return rv;

  // string names are in unicode
  nsAutoString stringId;
  stringId.AssignWithConversion(aPrefName);

  return bundle->GetStringFromName(stringId.get(), return_buf);
}

const char *nsPrefBranch::getPrefName(const char *aPrefName)
{
  // for speed, avoid strcpy if we can:
  if (mPrefRoot.IsEmpty())
    return aPrefName;

  // isn't there a better way to do this? this is really kind of gross.
  mPrefRoot.Truncate(mPrefRootLength);

  // only append if anything to append
  if ((nsnull != aPrefName) && (*aPrefName != '\0'))
    mPrefRoot.Append(aPrefName);

  return mPrefRoot.get();
}

nsresult nsPrefBranch::getValidatedPrefName(const char *aPrefName, const char **_retval)
{
  static const char capabilityPrefix[] = "capability.";

  NS_ENSURE_ARG_POINTER(aPrefName);
  const char *fullPref = getPrefName(aPrefName);

  // now that we have the pref, check it against the ScriptSecurityManager
  if ((fullPref[0] == 'c') &&
    PL_strncmp(fullPref, capabilityPrefix, sizeof(capabilityPrefix)-1) == 0)
  {
    nsresult rv;
    nsCOMPtr<nsIScriptSecurityManager> secMan = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);

    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;

    PRBool enabled;
    rv = secMan->IsCapabilityEnabled("CapabilityPreferencesAccess", &enabled);
    if (NS_FAILED(rv) || !enabled)
      return NS_ERROR_FAILURE;
  }

  *_retval = fullPref;
  return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
pref_enumChild(PLDHashTable *table, PLDHashEntryHdr *heh,
               PRUint32 i, void *arg)
{
  PrefHashEntry *he = NS_STATIC_CAST(PrefHashEntry*, heh);
  EnumerateData *d = NS_REINTERPRET_CAST(EnumerateData *, arg);
  if (PL_strncmp(he->key, d->parent, PL_strlen(d->parent)) == 0) {
    d->pref_list->AppendElement((void*)he->key);
  }
  return PL_DHASH_NEXT;
}


/*
 * nsISecurityPref methods
 *
 * Pref access without security check - these are here
 * to support nsScriptSecurityManager.
 * These functions are part of nsISecurityPref, not nsIPref.
 * **PLEASE** do not call these functions from elsewhere
 */
NS_IMETHODIMP nsPrefBranch::SecurityGetBoolPref(const char *pref, PRBool * return_val)
{
  return PREF_GetBoolPref(getPrefName(pref), return_val, PR_FALSE);
}

NS_IMETHODIMP nsPrefBranch::SecuritySetBoolPref(const char *pref, PRBool value)
{
  return PREF_SetBoolPref(getPrefName(pref), value);
}

NS_IMETHODIMP nsPrefBranch::SecurityGetCharPref(const char *pref, char ** return_buf)
{
  return PREF_CopyCharPref(getPrefName(pref), return_buf, PR_FALSE);
}

NS_IMETHODIMP nsPrefBranch::SecuritySetCharPref(const char *pref, const char* value)
{
  return PREF_SetCharPref(getPrefName(pref), value);
}

NS_IMETHODIMP nsPrefBranch::SecurityGetIntPref(const char *pref, PRInt32 * return_val)
{
  return PREF_GetIntPref(getPrefName(pref), return_val, PR_FALSE);
}

NS_IMETHODIMP nsPrefBranch::SecuritySetIntPref(const char *pref, PRInt32 value)
{
  return PREF_SetIntPref(getPrefName(pref), value);
}

NS_IMETHODIMP nsPrefBranch::SecurityClearUserPref(const char *pref_name)
{
  return PREF_ClearUserPref(getPrefName(pref_name));
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

NS_IMPL_THREADSAFE_ADDREF(nsPrefLocalizedString)
NS_IMPL_THREADSAFE_RELEASE(nsPrefLocalizedString)

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
nsPrefLocalizedString::GetData(PRUnichar** _retval)
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
nsPrefLocalizedString::SetData(const PRUnichar *aData)
{
  return SetData(nsDependentString(aData));
}

NS_IMETHODIMP
nsPrefLocalizedString::SetDataWithLength(PRUint32 aLength,
                                         const PRUnichar* aData)
{
  return SetData(Substring(aData, aData + aLength));
}

//----------------------------------------------------------------------------
// nsRelativeFilePref
//----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRelativeFilePref, nsIRelativeFilePref)

nsRelativeFilePref::nsRelativeFilePref()
{
}

nsRelativeFilePref::~nsRelativeFilePref()
{
}

NS_IMETHODIMP nsRelativeFilePref::GetFile(nsILocalFile * *aFile)
{
    NS_ENSURE_ARG_POINTER(aFile);
    *aFile = mFile;
    NS_IF_ADDREF(*aFile);
    return NS_OK;
}

NS_IMETHODIMP nsRelativeFilePref::SetFile(nsILocalFile * aFile)
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
