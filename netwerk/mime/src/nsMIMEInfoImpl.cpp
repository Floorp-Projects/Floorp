/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsMIMEInfoImpl.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

// nsISupports methods
NS_IMPL_THREADSAFE_ISUPPORTS1(nsMIMEInfoImpl, nsIMIMEInfo);

// nsMIMEInfoImpl methods
nsMIMEInfoImpl::nsMIMEInfoImpl() {
    mPreferredAction = nsIMIMEInfo::saveToDisk;
    mAlwaysAskBeforeHandling = PR_TRUE;
}

nsMIMEInfoImpl::nsMIMEInfoImpl(const char *aMIMEType) :mMIMEType( aMIMEType ){
    mPreferredAction = nsIMIMEInfo::saveToDisk;
    mAlwaysAskBeforeHandling = PR_TRUE;
}

PRUint32
nsMIMEInfoImpl::GetExtCount() {
    return mExtensions.Count();
}

NS_IMETHODIMP
nsMIMEInfoImpl::GetFileExtensions(PRUint32 *elementCount, char ***extensions) {
    PRUint32 count = mExtensions.Count();
    *elementCount = count; 
    *extensions = nsnull;
    if (count < 1) {
      return NS_OK;
    }

    char **_retExts = (char**)nsMemory::Alloc((count)*sizeof(char*));
    if (!_retExts) return NS_ERROR_OUT_OF_MEMORY;

    for (PRUint32 i=0; i < count; i++) {
        nsCString* ext = mExtensions.CStringAt(i);
        _retExts[i] = ToNewCString(*ext);
        if (!_retExts[i]) {
            // clean up all the strings we've allocated
            while (i-- != 0) nsMemory::Free(_retExts[i]);
            nsMemory::Free(_retExts);
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    *elementCount = count;
    *extensions   = _retExts;

    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::ExtensionExists(const char *aExtension, PRBool *_retval) {
    NS_ASSERTION(aExtension, "no extension");
    PRBool found = PR_FALSE;
    PRUint32 extCount = mExtensions.Count();
    if (extCount < 1) return NS_OK;

    if (!aExtension) return NS_ERROR_NULL_POINTER;

    nsDependentCString extension(aExtension);
    for (PRUint8 i=0; i < extCount; i++) {
        nsCString* ext = (nsCString*)mExtensions.CStringAt(i);
        if (ext->Equals(extension, nsCaseInsensitiveCStringComparator())) {
            found = PR_TRUE;
            break;
        }
    }

    *_retval = found;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::GetPrimaryExtension(char **_retval) {
    PRUint32 extCount = mExtensions.Count();
    if (extCount < 1) return NS_ERROR_NOT_INITIALIZED;

    *_retval = ToNewCString(*(mExtensions.CStringAt(0)));
    if (!*_retval) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;    
}

NS_IMETHODIMP
nsMIMEInfoImpl::SetPrimaryExtension(const char *aExtension) {
  NS_ASSERTION(aExtension, "no extension");
  PRUint32 extCount = mExtensions.Count();
  nsCString extension(aExtension);
  PRUint8 i;
  PRBool found = PR_FALSE;
  for (i=0; i < extCount; i++) {
    nsCString* ext = (nsCString*)mExtensions.CStringAt(i);
    if (ext->Equals(extension, nsCaseInsensitiveCStringComparator())) {
      found = PR_TRUE;
      break;
    }
  }
  if (found) {
    mExtensions.RemoveCStringAt(i);
  }

  mExtensions.InsertCStringAt(extension, 0);
  
  return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::AppendExtension(const char *aExtension)
{
	mExtensions.AppendCString( nsCAutoString(aExtension) );
	return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::Clone(nsIMIMEInfo** aClone) {
  NS_ENSURE_ARG_POINTER(aClone);

  nsMIMEInfoImpl* clone = new nsMIMEInfoImpl(mMIMEType.get());
  if (!clone) {
    *aClone = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  clone->mExtensions = mExtensions;
  clone->mDescription = mDescription;
  nsresult result = NS_OK;
  if (mURI) {
    result = mURI->Clone(getter_AddRefs(clone->mURI));
    NS_ASSERTION(NS_SUCCEEDED(result), "Failed to clone URI");
  }
  clone->mMacType = mMacType;
  clone->mMacCreator = mMacCreator;
  if (mPreferredApplication) {
    result = mPreferredApplication->Clone(getter_AddRefs(clone->mPreferredApplication));
    NS_ASSERTION(NS_SUCCEEDED(result), "Failed to clone preferred handler application");
  }
  clone->mPreferredAction = mPreferredAction;
  clone->mPreferredAppDescription = mPreferredAppDescription;

  return CallQueryInterface(clone, aClone);
}

NS_IMETHODIMP
nsMIMEInfoImpl::GetMIMEType(char * *aMIMEType) {
    if (!aMIMEType) return NS_ERROR_NULL_POINTER;

    if (mMIMEType.Length() < 1)
        return NS_ERROR_NOT_INITIALIZED;

    *aMIMEType = ToNewCString(mMIMEType);
    if (!*aMIMEType) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::SetMIMEType(const char* aMIMEType) {
    if (!aMIMEType) return NS_ERROR_NULL_POINTER;

    mMIMEType=aMIMEType;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::GetDescription(PRUnichar * *aDescription) {
    if (!aDescription) return NS_ERROR_NULL_POINTER;

    *aDescription = ToNewUnicode(mDescription);
    if (!*aDescription) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::SetDescription(const PRUnichar * aDescription) 
{
	mDescription =  aDescription;
	return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::GetDataURI(nsIURI * *aDataURI) {
    return mURI->Clone(aDataURI);
}

NS_IMETHODIMP
nsMIMEInfoImpl::Equals(nsIMIMEInfo *aMIMEInfo, PRBool *_retval) {
    if (!aMIMEInfo) return NS_ERROR_NULL_POINTER;

    nsXPIDLCString type;
    nsresult rv = aMIMEInfo->GetMIMEType(getter_Copies(type));
    if (NS_FAILED(rv)) return rv;

    *_retval = mMIMEType.EqualsWithConversion(type);

    return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::GetMacType(PRUint32 *aMacType)
{
	*aMacType = mMacType;
	return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::SetMacType(PRUint32 aMacType)
{
	mMacType = aMacType;
	return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::GetMacCreator(PRUint32 *aMacCreator)
{
	*aMacCreator = mMacCreator;
	return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::SetMacCreator(PRUint32 aMacCreator)
{
	mMacCreator = aMacCreator;
	return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::SetFileExtensions( const char* aExtensions )
{
	mExtensions.Clear();
	nsCString extList( aExtensions );
	
	PRInt32 breakLocation = -1;
	while ( (breakLocation= extList.FindChar(',') )!= -1)
	{
		nsCString ext( extList.get(), breakLocation );
		mExtensions.AppendCString( ext );
		extList.Cut(0, breakLocation+1 );
	}
	if ( extList.Length() )
		mExtensions.AppendCString( extList );
	return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::GetApplicationDescription(PRUnichar ** aApplicationDescription)
{
  *aApplicationDescription = ToNewUnicode(mPreferredAppDescription);
  return NS_OK;
}
 
NS_IMETHODIMP nsMIMEInfoImpl::SetApplicationDescription(const PRUnichar * aApplicationDescription)
{
  mPreferredAppDescription = aApplicationDescription;
  return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::GetDefaultDescription(PRUnichar ** aDefaultDescription)
{
  *aDefaultDescription = ToNewUnicode(mDefaultAppDescription);
  return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::SetDefaultDescription(const PRUnichar * aDefaultDescription)
{
  mDefaultAppDescription = aDefaultDescription;
  return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::GetPreferredApplicationHandler(nsIFile ** aPreferredAppHandler)
{
  *aPreferredAppHandler = mPreferredApplication;
  NS_IF_ADDREF(*aPreferredAppHandler);
  return NS_OK;
}
 
NS_IMETHODIMP nsMIMEInfoImpl::SetPreferredApplicationHandler(nsIFile * aPreferredAppHandler)
{
  mPreferredApplication = aPreferredAppHandler;
  return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::GetDefaultApplicationHandler(nsIFile ** aDefaultAppHandler)
{
  *aDefaultAppHandler = mDefaultApplication;
  NS_IF_ADDREF(*aDefaultAppHandler);
  return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::SetDefaultApplicationHandler(nsIFile * aDefaultAppHandler)
{
  mDefaultApplication = aDefaultAppHandler;
  return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::GetPreferredAction(nsMIMEInfoHandleAction * aPreferredAction)
{
  *aPreferredAction = mPreferredAction;
  return NS_OK;
}
 
NS_IMETHODIMP nsMIMEInfoImpl::SetPreferredAction(nsMIMEInfoHandleAction aPreferredAction)
{
  mPreferredAction = aPreferredAction;
  return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::GetAlwaysAskBeforeHandling(PRBool * aAlwaysAsk)
{
  *aAlwaysAsk = mAlwaysAskBeforeHandling;

  return NS_OK;
}

NS_IMETHODIMP nsMIMEInfoImpl::SetAlwaysAskBeforeHandling(PRBool aAlwaysAsk)
{
  mAlwaysAskBeforeHandling = aAlwaysAsk;
  return NS_OK;
}
