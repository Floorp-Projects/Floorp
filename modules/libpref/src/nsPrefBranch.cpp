/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIComponentManager.h"
#include "nsXPIDLString.h"

#include "nsTextFormatter.h"

#include "nsPrefBranch.h"
#include "nsIPrefListener.h"
#include "prefapi.h"
#include "prefapi_private_data.h"

// this needs to be removed!
static nsresult _convertRes(int res)
//---------------------------------------------------------------------------
{
    switch (res)
    {
        case PREF_OUT_OF_MEMORY:
            return NS_ERROR_OUT_OF_MEMORY;
        case PREF_NOT_INITIALIZED:
            return NS_ERROR_NOT_INITIALIZED;
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

struct EnumerateData {
    char *parent;
    PrefEnumerationFunc callback;
    void *arg;
};

// roll this into the base class por favor
PR_STATIC_CALLBACK(PRIntn)
pref_enumChild(PLHashEntry *he, int i, void *arg)
{
    EnumerateData *d = (EnumerateData *) arg;
    if (PL_strncmp((char*)he->key, d->parent, PL_strlen(d->parent)) == 0) {
        (*d->callback)((char*)he->key, d->arg);
    }
    return HT_ENUMERATE_NEXT;
}

nsPrefBranch::nsPrefBranch(const char *aPrefRoot)
{
  mPrefRoot = aPrefRoot;
  mPrefRootLength = mPrefRoot.Length();
}

nsPrefBranch::~nsPrefBranch()
{

}

NS_IMPL_ISUPPORTS1(nsPrefBranch, nsIPrefBranch)

nsCString&
nsPrefBranch::getPrefRoot()
{
  mPrefRoot.Truncate(mPrefRootLength);
  return mPrefRoot;
}

const char *
nsPrefBranch::getPrefName(const char *aPrefName)
{
  // for speed, avoid strcpy if we can:
  if (mPrefRoot.IsEmpty())
    return aPrefName;

  getPrefRoot();
  mPrefRoot.Append(aPrefName);
  return mPrefRoot.GetBuffer();
}

nsresult
nsPrefBranch::GetRoot(char **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = getPrefRoot().ToNewCString();

  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::GetBoolPref(const char *aPref, PRBool *aResult)
{
  return _convertRes(PREF_GetBoolPref(getPrefName(aPref), aResult));
}

NS_IMETHODIMP
nsPrefBranch::GetIntPref(const char *aPref, PRInt32 *aResult)
{
  return _convertRes(PREF_GetIntPref(getPrefName(aPref), aResult));
}

nsresult
nsPrefBranch::GetCharPref(const char *aPref, char **aResult)
{
  return _convertRes(PREF_CopyCharPref(getPrefName(aPref), aResult));
}

NS_IMETHODIMP
nsPrefBranch::SetBoolPref(const char *aPref, PRBool aValue)
{
  return _convertRes(PREF_SetBoolPref(getPrefName(aPref), aValue));
}

NS_IMETHODIMP
nsPrefBranch::SetIntPref(const char *aPref, PRInt32 aValue)
{
  return _convertRes(PREF_SetIntPref(getPrefName(aPref), aValue));
}

NS_IMETHODIMP
nsPrefBranch::SetCharPref(const char *aPref, const char* aValue)
{
  return _convertRes(PREF_SetCharPref(getPrefName(aPref), aValue));
}

// defaults
NS_IMETHODIMP
nsPrefBranch::GetDefaultBoolPref(const char *aPref, PRBool *aResult)
{
  return _convertRes(PREF_GetDefaultBoolPref(getPrefName(aPref), aResult));
}

NS_IMETHODIMP
nsPrefBranch::GetDefaultIntPref(const char *aPref, PRInt32 *aResult)
{
  return _convertRes(PREF_GetDefaultIntPref(getPrefName(aPref), aResult));
}

nsresult
nsPrefBranch::GetDefaultCharPref(const char *aPref, char **aResult)
{
  return _convertRes(PREF_CopyDefaultCharPref(getPrefName(aPref), aResult));
}

NS_IMETHODIMP
nsPrefBranch::SetDefaultBoolPref(const char *aPref, PRBool aValue)
{
  return _convertRes(PREF_SetDefaultBoolPref(getPrefName(aPref), aValue));
}

NS_IMETHODIMP
nsPrefBranch::SetDefaultIntPref(const char *aPref, PRInt32 aValue)
{
  return _convertRes(PREF_SetDefaultIntPref(getPrefName(aPref), aValue));
}

NS_IMETHODIMP
nsPrefBranch::SetDefaultCharPref(const char *aPref, const char* aValue)
{
  return _convertRes(PREF_SetDefaultCharPref(getPrefName(aPref), aValue));
}


NS_IMETHODIMP
nsPrefBranch::GetFileSpecPref(const char *pref_name, nsIFileSpec** value)
{
  return getFileSpecPrefInternal(getPrefName(pref_name), value, PR_FALSE);
}

NS_IMETHODIMP
nsPrefBranch::GetDefaultFileSpecPref(const char *pref_name,
                                     nsIFileSpec** value)
{
  return getFileSpecPrefInternal(getPrefName(pref_name), value, PR_TRUE);
}

nsresult
nsPrefBranch::getFileSpecPrefInternal(const char *real_pref_name,
                                      nsIFileSpec **value,
                                      PRBool get_default)
{
    if (!value)
        return NS_ERROR_NULL_POINTER;        

        nsresult rv = nsComponentManager::CreateInstance(
        	(const char*)NS_FILESPEC_CONTRACTID,
        	(nsISupports*)nsnull,
        	(const nsID&)NS_GET_IID(nsIFileSpec),
        	(void**)value);
        NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Could not make a file spec.");
    if (!*value)
      return NS_ERROR_FAILURE;

    nsXPIDLCString encodedString;
    PrefResult result;

    if (get_default)
      result = PREF_CopyCharPref(real_pref_name,
                                 getter_Copies(encodedString));
    else
      result = PREF_CopyDefaultCharPref(real_pref_name,
                                        getter_Copies(encodedString));
    if (result != PREF_NOERROR)
        return _convertRes(result);

	PRBool valid;
    (*value)->SetPersistentDescriptorString(encodedString);
    (*value)->IsValid(&valid);
    if (! valid)
    	/* if the ecodedString wasn't a valid persitent descriptor, it might be a valid native path*/
    	(*value)->SetNativePath(encodedString);
    
    return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::SetFileSpecPref(const char *pref_name, 
                              nsIFileSpec* value)
{
  return setFileSpecPrefInternal(getPrefName(pref_name), value, PR_FALSE);
}

NS_IMETHODIMP
nsPrefBranch::SetDefaultFileSpecPref(const char *pref_name, 
                              nsIFileSpec* value)
{
  return setFileSpecPrefInternal(getPrefName(pref_name), value, PR_TRUE);
}

nsresult
nsPrefBranch::setFileSpecPrefInternal(const char *real_pref_name,
                                      nsIFileSpec* value,
                                      PRBool set_default)
{
    if (!value)
        return NS_ERROR_NULL_POINTER;        
    nsresult rv = NS_OK;
    if (!Exists(value))
    {
        // nsPersistentFileDescriptor requires an existing
        // object. Make it first. COM makes this difficult, of course...
	    nsIFileSpec* tmp = nsnull;
        rv = nsComponentManager::CreateInstance(NS_FILESPEC_CONTRACTID,
        	nsnull,
        	NS_GET_IID(nsIFileSpec),
        	(void**)&tmp);
        NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Could not make a file spec.");
	    if (!tmp)
	    	return NS_ERROR_FAILURE;
		tmp->FromFileSpec(value);
        tmp->CreateDir();
        NS_RELEASE(tmp);
    }

    nsXPIDLCString encodedString;
    value->GetPersistentDescriptorString(getter_Copies(encodedString));
    if (encodedString && *(const char*)encodedString)
    {
      if (set_default)
        rv = PREF_SetDefaultCharPref(real_pref_name, encodedString);
      else
        rv = PREF_SetCharPref(real_pref_name, encodedString);
    }

    return _convertRes(rv);
}

// unicode "%s" format string
static const PRUnichar unicodeFormatter[] = {
    (PRUnichar)'%',
    (PRUnichar)'s',
    (PRUnichar)0,
};

NS_IMETHODIMP nsPrefBranch::GetUnicharPref(const char *pref, PRUnichar ** return_buf)
{
    nsresult rv;
    
    // get the UTF8 string for conversion
    nsXPIDLCString utf8String;
    rv = GetCharPref(pref, getter_Copies(utf8String));
    if (NS_FAILED(rv)) return rv;

    return convertUTF8ToUnicode(utf8String, return_buf);
}

nsresult
nsPrefBranch::convertUTF8ToUnicode(const char *utf8String, PRUnichar ** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    // convert to PRUnichar using nsTextFormatter
    // this is so ugly, it allocates memory at least 4 times :(
    PRUnichar *unicodeString =
        nsTextFormatter::smprintf(unicodeFormatter, utf8String);
    if (!unicodeString) return NS_ERROR_OUT_OF_MEMORY;

    // use the right allocator
    *aResult = nsCRT::strdup(unicodeString);
    nsTextFormatter::smprintf_free(unicodeString);
    if (!*aResult) return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::GetDefaultUnicharPref( const char *pref,
                                             PRUnichar ** return_buf)
{
    nsresult rv;
    
    nsXPIDLCString utf8String;
    rv = GetDefaultCharPref(pref, getter_Copies(utf8String));
    if (NS_FAILED(rv)) return rv;
    
    return convertUTF8ToUnicode(utf8String, return_buf);
}

NS_IMETHODIMP
nsPrefBranch::SetUnicharPref(const char *pref, const PRUnichar *value)
{
    nsresult rv;
    nsAutoString str(value);

    char *utf8String = str.ToNewUTF8String();

    if (!utf8String) return NS_ERROR_OUT_OF_MEMORY;

    rv = SetCharPref(pref, utf8String);
    nsCRT::free(utf8String);

    return rv;
}

NS_IMETHODIMP
nsPrefBranch::SetDefaultUnicharPref(const char *pref,
                                            const PRUnichar *value)
{
    nsresult rv;
    nsAutoString str(value);

    char *utf8String = str.ToNewUTF8String();

    if (!utf8String) return NS_ERROR_OUT_OF_MEMORY;

    rv = SetDefaultCharPref(pref, utf8String);
    nsCRT::free(utf8String);

    return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::ClearBranch(const char *branchName)
{
    return _convertRes(PREF_DeleteBranch(getPrefName(branchName)));
}


NS_IMETHODIMP
nsPrefBranch::ClearPref(const char *pref_name)
{
    return _convertRes(PREF_ClearUserPref(getPrefName(pref_name)));
}


NS_IMETHODIMP
nsPrefBranch::EnumerateChildren(const char *parent,
                                PrefEnumerationFunc callback, void *arg) 
{   
    EnumerateData ed;
    ed.parent = nsCRT::strdup(getPrefName(parent));
    ed.callback = callback;
    ed.arg = arg;
    PL_HashTableEnumerateEntries(gHashTable, pref_enumChild, &ed);
    nsCRT::free(ed.parent);
    return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::RegisterCallback( const char* domain,
                          PrefChangedFunc callback, 
                          void* instance_data )
{
    PREF_RegisterCallback(getPrefName(domain), callback, instance_data);
    return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::UnregisterCallback( const char* domain,
                                  PrefChangedFunc callback, 
                                  void* instance_data )
{
    return _convertRes(PREF_UnregisterCallback(getPrefName(domain), callback, instance_data));
}

NS_IMETHODIMP
nsPrefBranch::AddListener(const char* domain,
                          nsIPrefListener *listener)
{
  NS_ASSERTION(0, "nsPrefBranch::AddListener not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrefBranch::RemoveListener(const char* domain,
                             nsIPrefListener *listener)
{
  NS_ASSERTION(0, "nsPrefBranch::RemoveListener not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrefBranch::GetPrefType(const char *pref, PRInt32 *aResult)
{
  *aResult = PREF_GetPrefType(getPrefName(pref));
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::PrefIsLocked(const char *pref, PRBool *res)
{
    if (res == NULL)
        return NS_ERROR_INVALID_POINTER;

    *res = PREF_PrefIsLocked(getPrefName(pref));
    return NS_OK;
}
