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
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsEscape.h"

#define NEVER_ASK_FOR_SAVE_TO_DISK_PREF "browser.helperApps.neverAsk.saveToDisk"
#define NEVER_ASK_FOR_OPEN_FILE_PREF "browser.helperApps.neverAsk.openFile"

// nsISupports methods
NS_IMPL_THREADSAFE_ISUPPORTS1(nsMIMEInfoImpl, nsIMIMEInfo);

// nsMIMEInfoImpl methods
nsMIMEInfoImpl::nsMIMEInfoImpl() {
    NS_INIT_REFCNT();
    mPreferredAction = nsIMIMEInfo::saveToDisk;
}

nsMIMEInfoImpl::nsMIMEInfoImpl(const char *aMIMEType) :mMIMEType( aMIMEType ){
    NS_INIT_REFCNT();
    mPreferredAction = nsIMIMEInfo::saveToDisk;
}

PRUint32
nsMIMEInfoImpl::GetExtCount() {
    return mExtensions.Count();
}

NS_IMETHODIMP
nsMIMEInfoImpl::GetFileExtensions(PRUint32 *elementCount, char ***extensions) {
    PRUint32 count = mExtensions.Count();
    if (count < 1) return NS_ERROR_NOT_INITIALIZED;

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
    if (extCount < 1) return NS_ERROR_NOT_INITIALIZED;

    if (!aExtension) return NS_ERROR_NULL_POINTER;

    for (PRUint8 i=0; i < extCount; i++) {
        nsCString* ext = (nsCString*)mExtensions.CStringAt(i);
        if (ext->Equals(aExtension)) {
            found = PR_TRUE;
            break;
        }
    }

    *_retval = found;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::FirstExtension(char **_retval) {
    PRUint32 extCount = mExtensions.Count();
    if (extCount < 1) return NS_ERROR_NOT_INITIALIZED;

    *_retval = ToNewCString(*(mExtensions.CStringAt(0)));
    if (!*_retval) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;    
}

NS_IMETHODIMP nsMIMEInfoImpl::AppendExtension(const char *aExtension)
{
	mExtensions.AppendCString( nsCAutoString(aExtension) );
	return NS_OK;
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
	while ( (breakLocation= extList.FindCharInSet( ",",0 ) )!= -1)
	{
		nsCString ext( extList, breakLocation );
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
  // this is a bit complicated. We now store the always ask preference in two preferences:
  // browser.helperApps.neverAsk.saveToDisk and
  // browser.helperApps.neverAsk.openFile
  // Both lists are comma delimited lists containing mime types which the user has specifically
  // decided to override the helper app dialog for. 

  // for get always ask, we just need to search both pref branches for our mime type...
  PRBool mimeTypeIsPresent = PR_FALSE;
  CheckPrefForMimeType(NEVER_ASK_FOR_SAVE_TO_DISK_PREF, &mimeTypeIsPresent);

  if (mimeTypeIsPresent)
  {
    *aAlwaysAsk = PR_FALSE;
    mPreferredAction = nsIMIMEInfo::saveToDisk;
  }
  else
  {
    CheckPrefForMimeType(NEVER_ASK_FOR_OPEN_FILE_PREF, &mimeTypeIsPresent);
    if (mimeTypeIsPresent)
    {
      *aAlwaysAsk = PR_FALSE;
      // do we need to force a preferred action here? I don't think so...
    }
  }

  return NS_OK;
}

// re-write me to use new string apis to avoid the extra string copy of the pref value in order to call ::Find
void nsMIMEInfoImpl::CheckPrefForMimeType(const char * prefName, PRBool * aMimeTypeIsPresent)
{
  *aMimeTypeIsPresent = PR_FALSE;
  nsresult rv = NS_OK;

  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(prefs, &rv);
  if (NS_SUCCEEDED(rv) && prefBranch)
  {
    nsXPIDLCString prefCString;
    nsCAutoString prefValue;
    rv = prefBranch->GetCharPref(prefName, getter_Copies(prefCString));
    if (NS_SUCCEEDED(rv) && *prefCString.get())
    {
      nsUnescape(NS_CONST_CAST(char*,(const char*)prefCString.get()));

      prefValue = prefCString; // i should be able to avoid this string copy
      PRInt32 pos = prefValue.Find(mMIMEType, PR_TRUE);
      if (pos >= 0)
        *aMimeTypeIsPresent = PR_TRUE;
    }
  } // if we got the prefs service
}

void nsMIMEInfoImpl::SetRememberPrefForMimeType(const char * prefName)
{
   // first, make sure it isn't already there...
  PRBool mimeTypeIsPresent = PR_FALSE;
  CheckPrefForMimeType(prefName, &mimeTypeIsPresent);
  if (mimeTypeIsPresent) return; // mime type is already present...

  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(prefs, &rv);
  if (NS_SUCCEEDED(rv) && prefBranch)
  {
    // read out the current pref string and unescape it.
    nsXPIDLCString prefCString;
    nsCAutoString prefValue;
    rv = prefBranch->GetCharPref(prefName, getter_Copies(prefCString));
    if (NS_SUCCEEDED(rv) && *prefCString.get())
    {
      nsUnescape(NS_CONST_CAST(char*,(const char*)prefCString.get()));
      prefValue = prefCString;
    }

    if (!prefValue.IsEmpty()) // if the pref string isn't empty...
    {
      prefValue.Append(", ");
      prefValue.Append(mMIMEType);
    }
    else
      prefValue = mMIMEType;

    // always escape the pref b4 storing it in case someone enters some funky characters for a mime type...
    nsXPIDLCString escapedPrefString;
    *((char **)getter_Copies(escapedPrefString)) = nsEscape(prefValue, url_XAlphas);
    prefBranch->SetCharPref(prefName, escapedPrefString);
  }
}

NS_IMETHODIMP nsMIMEInfoImpl::SetAlwaysAskBeforeHandling(PRBool aAlwaysAsk)
{
  if (!aAlwaysAsk)
  {
    if (mPreferredAction == nsIMIMEInfo::saveToDisk)
      SetRememberPrefForMimeType(NEVER_ASK_FOR_SAVE_TO_DISK_PREF);
    else
      SetRememberPrefForMimeType(NEVER_ASK_FOR_OPEN_FILE_PREF);
  }
  return NS_OK;
}
