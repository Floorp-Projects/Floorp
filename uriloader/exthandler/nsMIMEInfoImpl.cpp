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
#include "nsStringEnumerator.h"
#include "nsIProcess.h"

// nsISupports methods
NS_IMPL_THREADSAFE_ISUPPORTS1(nsMIMEInfoBase, nsIMIMEInfo)

// nsMIMEInfoImpl methods
nsMIMEInfoBase::nsMIMEInfoBase(const char *aMIMEType) :
    mMIMEType(aMIMEType),
    mPreferredAction(nsIMIMEInfo::saveToDisk),
    mAlwaysAskBeforeHandling(PR_TRUE)
{
}

nsMIMEInfoBase::~nsMIMEInfoBase()
{
}

NS_IMETHODIMP
nsMIMEInfoBase::GetFileExtensions(nsIUTF8StringEnumerator** aResult)
{
  return NS_NewUTF8StringEnumerator(aResult, &mExtensions, this);
}

NS_IMETHODIMP
nsMIMEInfoBase::ExtensionExists(const char *aExtension, PRBool *_retval)
{
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
nsMIMEInfoBase::GetPrimaryExtension(char **_retval)
{
    PRUint32 extCount = mExtensions.Count();
    if (extCount < 1) return NS_ERROR_NOT_INITIALIZED;

    *_retval = ToNewCString(*(mExtensions.CStringAt(0)));
    if (!*_retval) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;    
}

NS_IMETHODIMP
nsMIMEInfoBase::SetPrimaryExtension(const char *aExtension)
{
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

NS_IMETHODIMP
nsMIMEInfoBase::AppendExtension(const char *aExtension)
{
	mExtensions.AppendCString( nsCAutoString(aExtension) );
	return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetMIMEType(char * *aMIMEType)
{
    if (!aMIMEType) return NS_ERROR_NULL_POINTER;

    if (mMIMEType.IsEmpty())
        return NS_ERROR_NOT_INITIALIZED;

    *aMIMEType = ToNewCString(mMIMEType);
    if (!*aMIMEType) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::SetMIMEType(const char* aMIMEType)
{
    if (!aMIMEType) return NS_ERROR_NULL_POINTER;

    mMIMEType=aMIMEType;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetDescription(PRUnichar * *aDescription)
{
    if (!aDescription) return NS_ERROR_NULL_POINTER;

    *aDescription = ToNewUnicode(mDescription);
    if (!*aDescription) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::SetDescription(const PRUnichar * aDescription) 
{
	mDescription =  aDescription;
	return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::Equals(nsIMIMEInfo *aMIMEInfo, PRBool *_retval)
{
    if (!aMIMEInfo) return NS_ERROR_NULL_POINTER;

    nsXPIDLCString type;
    nsresult rv = aMIMEInfo->GetMIMEType(getter_Copies(type));
    if (NS_FAILED(rv)) return rv;

    *_retval = mMIMEType.Equals(type);

    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetMacType(PRUint32 *aMacType)
{
	*aMacType = mMacType;
	return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::SetMacType(PRUint32 aMacType)
{
	mMacType = aMacType;
	return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetMacCreator(PRUint32 *aMacCreator)
{
	*aMacCreator = mMacCreator;
	return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::SetMacCreator(PRUint32 aMacCreator)
{
	mMacCreator = aMacCreator;
	return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::SetFileExtensions(const char* aExtensions)
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
	if ( !extList.IsEmpty() )
		mExtensions.AppendCString( extList );
	return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetApplicationDescription(PRUnichar ** aApplicationDescription)
{
  if (mPreferredAppDescription.IsEmpty() && mPreferredApplication) {
    // Don't want to cache this, just in case someone resets the app
    // without changing the description....
    nsAutoString leafName;
    mPreferredApplication->GetLeafName(leafName);
    *aApplicationDescription = ToNewUnicode(leafName);
  } else {
    *aApplicationDescription = ToNewUnicode(mPreferredAppDescription);
  }
  
  return *aApplicationDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
 
NS_IMETHODIMP
nsMIMEInfoBase::SetApplicationDescription(const PRUnichar * aApplicationDescription)
{
  mPreferredAppDescription = aApplicationDescription;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetDefaultDescription(PRUnichar ** aDefaultDescription)
{
  *aDefaultDescription = ToNewUnicode(mDefaultAppDescription);
  return *aDefaultDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetPreferredApplicationHandler(nsIFile ** aPreferredAppHandler)
{
  *aPreferredAppHandler = mPreferredApplication;
  NS_IF_ADDREF(*aPreferredAppHandler);
  return NS_OK;
}
 
NS_IMETHODIMP
nsMIMEInfoBase::SetPreferredApplicationHandler(nsIFile * aPreferredAppHandler)
{
  mPreferredApplication = aPreferredAppHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetPreferredAction(nsMIMEInfoHandleAction * aPreferredAction)
{
  *aPreferredAction = mPreferredAction;
  return NS_OK;
}
 
NS_IMETHODIMP
nsMIMEInfoBase::SetPreferredAction(nsMIMEInfoHandleAction aPreferredAction)
{
  mPreferredAction = aPreferredAction;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetAlwaysAskBeforeHandling(PRBool * aAlwaysAsk)
{
  *aAlwaysAsk = mAlwaysAskBeforeHandling;

  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::SetAlwaysAskBeforeHandling(PRBool aAlwaysAsk)
{
  mAlwaysAskBeforeHandling = aAlwaysAsk;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::LaunchWithFile(nsIFile* aFile)
{
  if (mPreferredAction == useHelperApp) {
    if (!mPreferredApplication)
      return NS_ERROR_FILE_NOT_FOUND;

    return LaunchWithIProcess(mPreferredApplication, aFile);
  }
  else if (mPreferredAction == useSystemDefault) {
    return LaunchDefaultWithFile(aFile);
  }

  return NS_ERROR_INVALID_ARG;
}

void
nsMIMEInfoBase::CopyBasicDataTo(nsMIMEInfoBase* aOther)
{
  aOther->mMIMEType = mMIMEType;
  aOther->mDefaultAppDescription = mDefaultAppDescription;
  aOther->mExtensions = mExtensions;

  aOther->mMacType = mMacType;
  aOther->mMacCreator = mMacCreator;
}

/* static */
nsresult
nsMIMEInfoBase::LaunchWithIProcess(nsIFile* aApp, nsIFile* aFile)
{
  NS_ASSERTION(aApp && aFile, "Unexpected null pointer, fix caller");

  nsresult rv;
  nsCOMPtr<nsIProcess> process = do_CreateInstance(NS_PROCESS_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  if (NS_FAILED(rv = process->Init(aApp)))
    return rv;

  nsCAutoString path;
  aFile->GetNativePath(path);

  const char * strPath = path.get();

  PRUint32 pid;
  return process->Run(PR_FALSE, &strPath, 1, &pid);
}

// nsMIMEInfoImpl implementation
NS_IMETHODIMP
nsMIMEInfoImpl::GetDefaultDescription(PRUnichar ** aDefaultDescription)
{
  if (mDefaultAppDescription.IsEmpty() && mDefaultApplication) {
    // Don't want to cache this, just in case someone resets the app
    // without changing the description....
    nsAutoString leafName;
    mDefaultApplication->GetLeafName(leafName);
    *aDefaultDescription = ToNewUnicode(leafName);
  } else {
    *aDefaultDescription = ToNewUnicode(mDefaultAppDescription);
  }
  
  return *aDefaultDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsMIMEInfoImpl::GetHasDefaultHandler(PRBool * _retval)
{
  *_retval = PR_FALSE;
  if (mDefaultApplication) {
    PRBool exists;
    *_retval = NS_SUCCEEDED(mDefaultApplication->Exists(&exists)) && exists;
  }
  return NS_OK;
}

nsresult
nsMIMEInfoImpl::LaunchDefaultWithFile(nsIFile* aFile)
{
  if (!mDefaultApplication)
    return NS_ERROR_FILE_NOT_FOUND;

  return LaunchWithIProcess(mDefaultApplication, aFile);
}


