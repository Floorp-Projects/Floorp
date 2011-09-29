/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMIMEInfoImpl.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsStringEnumerator.h"
#include "nsILocalFile.h"
#include "nsIFileURL.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsIURILoader.h"
#include "nsCURILoader.h"

// nsISupports methods
NS_IMPL_THREADSAFE_ADDREF(nsMIMEInfoBase)
NS_IMPL_THREADSAFE_RELEASE(nsMIMEInfoBase)

NS_INTERFACE_MAP_BEGIN(nsMIMEInfoBase)
    NS_INTERFACE_MAP_ENTRY(nsIHandlerInfo)
    // This is only an nsIMIMEInfo if it's a MIME handler.
    NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIMIMEInfo, mClass == eMIMEInfo)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIHandlerInfo)
NS_INTERFACE_MAP_END_THREADSAFE

// nsMIMEInfoImpl methods

// Constructors for a MIME handler.
nsMIMEInfoBase::nsMIMEInfoBase(const char *aMIMEType) :
    mSchemeOrType(aMIMEType),
    mClass(eMIMEInfo),
    mPreferredAction(nsIMIMEInfo::saveToDisk),
    mAlwaysAskBeforeHandling(PR_TRUE)
{
}

nsMIMEInfoBase::nsMIMEInfoBase(const nsACString& aMIMEType) :
    mSchemeOrType(aMIMEType),
    mClass(eMIMEInfo),
    mPreferredAction(nsIMIMEInfo::saveToDisk),
    mAlwaysAskBeforeHandling(PR_TRUE)
{
}

// Constructor for a handler that lets the caller specify whether this is a
// MIME handler or a protocol handler.  In the long run, these will be distinct
// classes (f.e. nsMIMEInfo and nsProtocolInfo), but for now we reuse this class
// for both and distinguish between the two kinds of handlers via the aClass
// argument to this method, which can be either eMIMEInfo or eProtocolInfo.
nsMIMEInfoBase::nsMIMEInfoBase(const nsACString& aType, HandlerClass aClass) :
    mSchemeOrType(aType),
    mClass(aClass),
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
nsMIMEInfoBase::ExtensionExists(const nsACString& aExtension, bool *_retval)
{
    NS_ASSERTION(!aExtension.IsEmpty(), "no extension");
    bool found = false;
    PRUint32 extCount = mExtensions.Length();
    if (extCount < 1) return NS_OK;

    for (PRUint8 i=0; i < extCount; i++) {
        const nsCString& ext = mExtensions[i];
        if (ext.Equals(aExtension, nsCaseInsensitiveCStringComparator())) {
            found = PR_TRUE;
            break;
        }
    }

    *_retval = found;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetPrimaryExtension(nsACString& _retval)
{
    if (!mExtensions.Length())
      return NS_ERROR_NOT_INITIALIZED;

    _retval = mExtensions[0];
    return NS_OK;    
}

NS_IMETHODIMP
nsMIMEInfoBase::SetPrimaryExtension(const nsACString& aExtension)
{
  NS_ASSERTION(!aExtension.IsEmpty(), "no extension");
  PRUint32 extCount = mExtensions.Length();
  PRUint8 i;
  bool found = false;
  for (i=0; i < extCount; i++) {
    const nsCString& ext = mExtensions[i];
    if (ext.Equals(aExtension, nsCaseInsensitiveCStringComparator())) {
      found = PR_TRUE;
      break;
    }
  }
  if (found) {
    mExtensions.RemoveElementAt(i);
  }

  mExtensions.InsertElementAt(0, aExtension);
  
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::AppendExtension(const nsACString& aExtension)
{
  mExtensions.AppendElement(aExtension);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetType(nsACString& aType)
{
    if (mSchemeOrType.IsEmpty())
        return NS_ERROR_NOT_INITIALIZED;

    aType = mSchemeOrType;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetMIMEType(nsACString& aMIMEType)
{
    if (mSchemeOrType.IsEmpty())
        return NS_ERROR_NOT_INITIALIZED;

    aMIMEType = mSchemeOrType;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetDescription(nsAString& aDescription)
{
    aDescription = mDescription;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::SetDescription(const nsAString& aDescription) 
{
    mDescription = aDescription;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::Equals(nsIMIMEInfo *aMIMEInfo, bool *_retval)
{
    if (!aMIMEInfo) return NS_ERROR_NULL_POINTER;

    nsCAutoString type;
    nsresult rv = aMIMEInfo->GetMIMEType(type);
    if (NS_FAILED(rv)) return rv;

    *_retval = mSchemeOrType.Equals(type);

    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::SetFileExtensions(const nsACString& aExtensions)
{
    mExtensions.Clear();
    nsCString extList( aExtensions );
    
    PRInt32 breakLocation = -1;
    while ( (breakLocation= extList.FindChar(',') )!= -1)
    {
        mExtensions.AppendElement(Substring(extList.get(), extList.get() + breakLocation));
        extList.Cut(0, breakLocation+1 );
    }
    if ( !extList.IsEmpty() )
        mExtensions.AppendElement( extList );
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetDefaultDescription(nsAString& aDefaultDescription)
{
  aDefaultDescription = mDefaultAppDescription;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetPreferredApplicationHandler(nsIHandlerApp ** aPreferredAppHandler)
{
  *aPreferredAppHandler = mPreferredApplication;
  NS_IF_ADDREF(*aPreferredAppHandler);
  return NS_OK;
}
 
NS_IMETHODIMP
nsMIMEInfoBase::SetPreferredApplicationHandler(nsIHandlerApp * aPreferredAppHandler)
{
  mPreferredApplication = aPreferredAppHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetPossibleApplicationHandlers(nsIMutableArray ** aPossibleAppHandlers)
{
  if (!mPossibleApplications)
    mPossibleApplications = do_CreateInstance(NS_ARRAY_CONTRACTID);

  if (!mPossibleApplications)
    return NS_ERROR_OUT_OF_MEMORY;

  *aPossibleAppHandlers = mPossibleApplications;
  NS_IF_ADDREF(*aPossibleAppHandlers);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetPreferredAction(nsHandlerInfoAction * aPreferredAction)
{
  *aPreferredAction = mPreferredAction;
  return NS_OK;
}
 
NS_IMETHODIMP
nsMIMEInfoBase::SetPreferredAction(nsHandlerInfoAction aPreferredAction)
{
  mPreferredAction = aPreferredAction;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetAlwaysAskBeforeHandling(bool * aAlwaysAsk)
{
  *aAlwaysAsk = mAlwaysAskBeforeHandling;

  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::SetAlwaysAskBeforeHandling(bool aAlwaysAsk)
{
  mAlwaysAskBeforeHandling = aAlwaysAsk;
  return NS_OK;
}

/* static */
nsresult 
nsMIMEInfoBase::GetLocalFileFromURI(nsIURI *aURI, nsILocalFile **aFile)
{
  nsresult rv;

  nsCOMPtr<nsIFileURL> fileUrl = do_QueryInterface(aURI, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIFile> file;
  rv = fileUrl->GetFile(getter_AddRefs(file));
  if (NS_FAILED(rv)) return rv;    

  return CallQueryInterface(file, aFile);
}

NS_IMETHODIMP
nsMIMEInfoBase::LaunchWithFile(nsIFile* aFile)
{
  nsresult rv;

  // it doesn't make any sense to call this on protocol handlers
  NS_ASSERTION(mClass == eMIMEInfo,
               "nsMIMEInfoBase should have mClass == eMIMEInfo");

  if (mPreferredAction == useSystemDefault) {
    return LaunchDefaultWithFile(aFile);
  }

  if (mPreferredAction == useHelperApp) {
    if (!mPreferredApplication)
      return NS_ERROR_FILE_NOT_FOUND;

    // at the moment, we only know how to hand files off to local handlers
    nsCOMPtr<nsILocalHandlerApp> localHandler = 
      do_QueryInterface(mPreferredApplication, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> executable;
    rv = localHandler->GetExecutable(getter_AddRefs(executable));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString path;
    aFile->GetNativePath(path);
    return LaunchWithIProcess(executable, path);
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsMIMEInfoBase::LaunchWithURI(nsIURI* aURI,
                              nsIInterfaceRequestor* aWindowContext)
{
  // for now, this is only being called with protocol handlers; that
  // will change once we get to more general registerContentHandler
  // support
  NS_ASSERTION(mClass == eProtocolInfo,
               "nsMIMEInfoBase should be a protocol handler");

  if (mPreferredAction == useSystemDefault) {
    return LoadUriInternal(aURI);
  }

  if (mPreferredAction == useHelperApp) {
    if (!mPreferredApplication)
      return NS_ERROR_FILE_NOT_FOUND;

    return mPreferredApplication->LaunchWithURI(aURI, aWindowContext);
  } 

  return NS_ERROR_INVALID_ARG;
}

void
nsMIMEInfoBase::CopyBasicDataTo(nsMIMEInfoBase* aOther)
{
  aOther->mSchemeOrType = mSchemeOrType;
  aOther->mDefaultAppDescription = mDefaultAppDescription;
  aOther->mExtensions = mExtensions;
}

/* static */
already_AddRefed<nsIProcess>
nsMIMEInfoBase::InitProcess(nsIFile* aApp, nsresult* aResult)
{
  NS_ASSERTION(aApp, "Unexpected null pointer, fix caller");

  nsCOMPtr<nsIProcess> process = do_CreateInstance(NS_PROCESS_CONTRACTID,
                                                   aResult);
  if (NS_FAILED(*aResult))
    return nsnull;

  *aResult = process->Init(aApp);
  if (NS_FAILED(*aResult))
    return nsnull;

  return process.forget();
}

/* static */
nsresult
nsMIMEInfoBase::LaunchWithIProcess(nsIFile* aApp, const nsCString& aArg)
{
  nsresult rv;
  nsCOMPtr<nsIProcess> process = InitProcess(aApp, &rv);
  if (NS_FAILED(rv))
    return rv;

  const char *string = aArg.get();

  return process->Run(PR_FALSE, &string, 1);
}

/* static */
nsresult
nsMIMEInfoBase::LaunchWithIProcess(nsIFile* aApp, const nsString& aArg)
{
  nsresult rv;
  nsCOMPtr<nsIProcess> process = InitProcess(aApp, &rv);
  if (NS_FAILED(rv))
    return rv;

  const PRUnichar *string = aArg.get();

  return process->Runw(PR_FALSE, &string, 1);
}

// nsMIMEInfoImpl implementation
NS_IMETHODIMP
nsMIMEInfoImpl::GetDefaultDescription(nsAString& aDefaultDescription)
{
  if (mDefaultAppDescription.IsEmpty() && mDefaultApplication) {
    // Don't want to cache this, just in case someone resets the app
    // without changing the description....
    mDefaultApplication->GetLeafName(aDefaultDescription);
  } else {
    aDefaultDescription = mDefaultAppDescription;
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoImpl::GetHasDefaultHandler(bool * _retval)
{
  *_retval = !mDefaultAppDescription.IsEmpty();
  if (mDefaultApplication) {
    bool exists;
    *_retval = NS_SUCCEEDED(mDefaultApplication->Exists(&exists)) && exists;
  }
  return NS_OK;
}

nsresult
nsMIMEInfoImpl::LaunchDefaultWithFile(nsIFile* aFile)
{
  if (!mDefaultApplication)
    return NS_ERROR_FILE_NOT_FOUND;

  nsCAutoString nativePath;
  aFile->GetNativePath(nativePath);
  
  return LaunchWithIProcess(mDefaultApplication, nativePath);
}

NS_IMETHODIMP
nsMIMEInfoBase::GetPossibleLocalHandlers(nsIArray **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
