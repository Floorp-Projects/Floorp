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
#include "nsISupportsPrimitives.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsScriptSecurityManager.h"
#include "nsIStringBundle.h"
#include "prefapi.h"
#include "prmem.h"

#include "nsIFileSpec.h"  // this should be removed eventually
#include "prefapi_private_data.h"

// Definitions
struct EnumerateData {
  const char  *parent;
  nsVoidArray *pref_list;
};

struct PrefCallbackData {
  nsIPrefBranch *pBranch;
  nsIObserver   *pObserver;
};


static NS_DEFINE_CID(kSecurityManagerCID, NS_SCRIPTSECURITYMANAGER_CID);

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
      return NS_OK;
  }

  NS_ASSERTION((res >= PREF_DEFAULT_VALUE_NOT_INITIALIZED) && (res <= PREF_PROFILE_UPGRADE), "you added a new error code to prefapi.h and didn't update _convertRes");
  return NS_OK;
}


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
  PrefCallbackData *pCallback;

  if (mObservers) {
    // unregister the observers
    PRInt32 count;

    count = mObservers->Count();
    if (count > 0) {
      PRInt32 i;
      nsCString domain;
      for (i = 0; i < count; i++) {
        pCallback = (PrefCallbackData *)mObservers->ElementAt(i);
        if (pCallback) {
          mObserverDomains.CStringAt(i, domain);
          PREF_UnregisterCallback(domain, NotifyObserver, pCallback);
          NS_RELEASE(pCallback->pObserver);
        }
        nsMemory::Free(pCallback);
      }

      // now empty the observer arrays in bulk
      mObservers->Clear();
      mObserverDomains.Clear();
    }
    delete mObservers;
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
  NS_INTERFACE_MAP_ENTRY(nsISecurityPref)
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
    rv = _convertRes(PREF_GetBoolPref(pref, _retval, mIsDefault));
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::SetBoolPref(const char *aPrefName, PRInt32 aValue)
{
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aPrefName, &pref);
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
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_SUCCEEDED(rv)) {
    rv = _convertRes(PREF_CopyCharPref(pref, _retval, mIsDefault));
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
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aPrefName, &pref);
  if (NS_SUCCEEDED(rv)) {
    rv = _convertRes(PREF_GetIntPref(pref, _retval, mIsDefault));
  }
  return rv;
}

NS_IMETHODIMP nsPrefBranch::SetIntPref(const char *aPrefName, PRInt32 aValue)
{
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aPrefName, &pref);
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

  if (aType.Equals(NS_GET_IID(nsISupportsWString))) {
    nsCOMPtr<nsISupportsWString> theString(do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      rv = theString->SetData(NS_ConvertUTF8toUCS2(utf8String).get());
      if (NS_SUCCEEDED(rv)) {
        nsISupportsWString *temp = theString;

        NS_ADDREF(temp);
        *_retval = (void *)temp;
        return NS_OK;
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
  nsresult   rv = NS_NOINTERFACE;

  if (aType.Equals(NS_GET_IID(nsILocalFile))) {
    nsCOMPtr<nsILocalFile> file = do_QueryInterface(aValue);
    nsXPIDLCString descriptorString;

    rv = file->GetPersistentDescriptor(getter_Copies(descriptorString));
    if (NS_SUCCEEDED(rv)) {
      rv = SetCharPref(aPrefName, descriptorString);
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsISupportsWString))) {
    nsCOMPtr<nsISupportsWString> theString = do_QueryInterface(aValue);

    if (theString) {
      nsXPIDLString wideString;

      rv = theString->GetData(getter_Copies(wideString));
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

  // This is depricated and you should not be using it
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
    rv = _convertRes(PREF_ClearUserPref(pref));
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
    rv = _convertRes(PREF_LockPref(pref));
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
  const char *pref;
  nsresult   rv;

  rv = getValidatedPrefName(aStartingAt, &pref);
  if (NS_SUCCEEDED(rv)) {
    rv = _convertRes(PREF_DeleteBranch(pref));
  }
  return rv;
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

  NS_ENSURE_ARG_POINTER(aStartingAt);
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

NS_IMETHODIMP nsPrefBranch::AddObserver(const char *aDomain, nsIObserver *aObserver)
{
 PrefCallbackData *pCallback;

  NS_ENSURE_ARG_POINTER(aDomain);
  NS_ENSURE_ARG_POINTER(aObserver);

  if (!mObservers) {
    nsresult rv = NS_OK;
    mObservers = new nsAutoVoidArray();
    if (nsnull == mObservers)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  pCallback = (PrefCallbackData *)nsMemory::Alloc(sizeof(PrefCallbackData));
  if (nsnull == pCallback)
    return NS_ERROR_OUT_OF_MEMORY;

  pCallback->pBranch = NS_STATIC_CAST(nsIPrefBranch *, this);
  NS_ADDREF(aObserver);
  pCallback->pObserver = aObserver;

  mObservers->AppendElement(pCallback);
  mObserverDomains.AppendCString(nsCString(aDomain));

  PREF_RegisterCallback(aDomain, NotifyObserver, pCallback);
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::RemoveObserver(const char *aDomain, nsIObserver *aObserver)
{
  PrefCallbackData *pCallback;
  PRInt32 count;
  PRInt32 i;
  nsresult rv;
  nsCString domain;

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
    if (pCallback && (pCallback->pObserver == aObserver)) {
      mObserverDomains.CStringAt(i, domain);
      if (domain.Equals(aDomain))
        break;
    }
  }

  if (i == count)             // not found, just return
    return NS_OK;
    
  rv = _convertRes(PREF_UnregisterCallback(aDomain, NotifyObserver, pCallback));
  if (NS_SUCCEEDED(rv)) {
    NS_RELEASE(pCallback->pObserver);
    nsMemory::Free(pCallback);
    mObservers->RemoveElementAt(i);
    mObserverDomains.RemoveCStringAt(i);
  }
  return rv;
}

static int PR_CALLBACK NotifyObserver(const char *newpref, void *data)
{
  PrefCallbackData *pData = (PrefCallbackData *)data;

  nsCOMPtr<nsIObserver> observer = NS_STATIC_CAST(nsIObserver *, pData->pObserver);
  observer->Observe(pData->pBranch,
                    NS_LITERAL_STRING(NS_PREFBRANCH_PREFCHANGE_OBSERVER_ID).get(),
                    NS_ConvertASCIItoUCS2(newpref).get());

    return 0;
}


nsresult nsPrefBranch::GetDefaultFromPropertiesFile(const char *aPrefName, PRUnichar **return_buf)
{
  nsresult rv;

  // the default value contains a URL to a .properties file
    
  nsXPIDLCString propertyFileURL;
  rv = _convertRes(PREF_CopyCharPref(aPrefName, getter_Copies(propertyFileURL), PR_TRUE));
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

  // string names are in unicdoe
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
  const char *fullPref;

  NS_ENSURE_ARG_POINTER(aPrefName);

  // for speed, avoid strcpy if we can:
  if (mPrefRoot.IsEmpty()) {
    fullPref = aPrefName;
  } else {
    // isn't there a better way to do this? this is really kind of gross.
    mPrefRoot.Truncate(mPrefRootLength);

    // only append if anything to append
    if ((nsnull != aPrefName) && (*aPrefName != '\0'))
      mPrefRoot.Append(aPrefName);
    fullPref = mPrefRoot.get();
  }

  // now that we have the pref, check it against the ScriptSecurityManager
  if ((fullPref[0] == 'c') &&
    PL_strncmp(fullPref, capabilityPrefix, sizeof(capabilityPrefix)-1) == 0)
  {
    nsresult rv;
    nsCOMPtr<nsIScriptSecurityManager> secMan = 
             do_GetService(kSecurityManagerCID, &rv);
    PRBool enabled;

    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    rv = secMan->IsCapabilityEnabled("CapabilityPreferencesAccess", &enabled);
    if (NS_FAILED(rv) || !enabled)
      return NS_ERROR_FAILURE;
  }

  *_retval = fullPref;
  return NS_OK;
}

PR_STATIC_CALLBACK(PRIntn) pref_enumChild(PLHashEntry *he, int i, void *arg)
{
  EnumerateData *d = (EnumerateData *) arg;
  if (PL_strncmp((char *)he->key, d->parent, PL_strlen(d->parent)) == 0) {
    d->pref_list->AppendElement((void *)he->key);
  }
  return HT_ENUMERATE_NEXT;
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
  return _convertRes(PREF_GetBoolPref(getPrefName(pref), return_val, PR_FALSE));
}

NS_IMETHODIMP nsPrefBranch::SecuritySetBoolPref(const char *pref, PRBool value)
{
  return _convertRes(PREF_SetBoolPref(getPrefName(pref), value));
}

NS_IMETHODIMP nsPrefBranch::SecurityGetCharPref(const char *pref, char ** return_buf)
{
  return _convertRes(PREF_CopyCharPref(getPrefName(pref), return_buf, PR_FALSE));
}

NS_IMETHODIMP nsPrefBranch::SecuritySetCharPref(const char *pref, const char* value)
{
  return _convertRes(PREF_SetCharPref(getPrefName(pref), value));
}

NS_IMETHODIMP nsPrefBranch::SecurityGetIntPref(const char *pref, PRInt32 * return_val)
{
  return _convertRes(PREF_GetIntPref(getPrefName(pref), return_val, PR_FALSE));
}

NS_IMETHODIMP nsPrefBranch::SecuritySetIntPref(const char *pref, PRInt32 value)
{
  return _convertRes(PREF_SetIntPref(getPrefName(pref), value));
}

NS_IMETHODIMP nsPrefBranch::SecurityClearUserPref(const char *pref_name)
{
  return _convertRes(PREF_ClearUserPref(getPrefName(pref_name)));
}


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

