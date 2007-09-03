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
#include "nsIProcess.h"
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
    mMacType(0),
    mMacCreator(0),
    mType(aMIMEType),
    mClass(eMIMEInfo),
    mPreferredAction(nsIMIMEInfo::saveToDisk),
    mAlwaysAskBeforeHandling(PR_TRUE)
{
}

nsMIMEInfoBase::nsMIMEInfoBase(const nsACString& aMIMEType) :
    mMacType(0),
    mMacCreator(0),
    mType(aMIMEType),
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
    mMacType(0),
    mMacCreator(0),
    mType(aType),
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
nsMIMEInfoBase::ExtensionExists(const nsACString& aExtension, PRBool *_retval)
{
    NS_ASSERTION(!aExtension.IsEmpty(), "no extension");
    PRBool found = PR_FALSE;
    PRUint32 extCount = mExtensions.Count();
    if (extCount < 1) return NS_OK;

    for (PRUint8 i=0; i < extCount; i++) {
        nsCString* ext = (nsCString*)mExtensions.CStringAt(i);
        if (ext->Equals(aExtension, nsCaseInsensitiveCStringComparator())) {
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
    PRUint32 extCount = mExtensions.Count();
    if (extCount < 1) return NS_ERROR_NOT_INITIALIZED;

    _retval = *(mExtensions.CStringAt(0));
    return NS_OK;    
}

NS_IMETHODIMP
nsMIMEInfoBase::SetPrimaryExtension(const nsACString& aExtension)
{
  NS_ASSERTION(!aExtension.IsEmpty(), "no extension");
  PRUint32 extCount = mExtensions.Count();
  PRUint8 i;
  PRBool found = PR_FALSE;
  for (i=0; i < extCount; i++) {
    nsCString* ext = (nsCString*)mExtensions.CStringAt(i);
    if (ext->Equals(aExtension, nsCaseInsensitiveCStringComparator())) {
      found = PR_TRUE;
      break;
    }
  }
  if (found) {
    mExtensions.RemoveCStringAt(i);
  }

  mExtensions.InsertCStringAt(aExtension, 0);
  
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::AppendExtension(const nsACString& aExtension)
{
  mExtensions.AppendCString(aExtension);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetType(nsACString& aType)
{
    if (mType.IsEmpty())
        return NS_ERROR_NOT_INITIALIZED;

    aType = mType;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetMIMEType(nsACString& aMIMEType)
{
    if (mType.IsEmpty())
        return NS_ERROR_NOT_INITIALIZED;

    aMIMEType = mType;
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
nsMIMEInfoBase::Equals(nsIMIMEInfo *aMIMEInfo, PRBool *_retval)
{
    if (!aMIMEInfo) return NS_ERROR_NULL_POINTER;

    nsCAutoString type;
    nsresult rv = aMIMEInfo->GetMIMEType(type);
    if (NS_FAILED(rv)) return rv;

    *_retval = mType.Equals(type);

    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::GetMacType(PRUint32 *aMacType)
{
    *aMacType = mMacType;

    if (!mMacType)
        return NS_ERROR_NOT_INITIALIZED;

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

    if (!mMacCreator)
        return NS_ERROR_NOT_INITIALIZED;

    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoBase::SetMacCreator(PRUint32 aMacCreator)
{
    mMacCreator = aMacCreator;
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
        mExtensions.AppendCString(Substring(extList.get(), extList.get() + breakLocation));
        extList.Cut(0, breakLocation+1 );
    }
    if ( !extList.IsEmpty() )
        mExtensions.AppendCString( extList );
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
  nsresult rv;

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

    // check for and possibly launch with web application
    nsCOMPtr<nsIWebHandlerApp> webHandler = 
      do_QueryInterface(mPreferredApplication, &rv);
    if (NS_SUCCEEDED(rv)) {
      return LaunchWithWebHandler(webHandler, aURI, aWindowContext);         
    }

    // ok, we must have a local handler app
    nsCOMPtr<nsILocalHandlerApp> localHandler = 
      do_QueryInterface(mPreferredApplication, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> executable;
    rv = localHandler->GetExecutable(getter_AddRefs(executable));
    NS_ENSURE_SUCCESS(rv, rv);

    // pass the entire URI to the handler.
    nsCAutoString spec;
    aURI->GetSpec(spec);
    return LaunchWithIProcess(executable, spec);
  } 

  return NS_ERROR_INVALID_ARG;
}

void
nsMIMEInfoBase::CopyBasicDataTo(nsMIMEInfoBase* aOther)
{
  aOther->mType = mType;
  aOther->mDefaultAppDescription = mDefaultAppDescription;
  aOther->mExtensions = mExtensions;

  aOther->mMacType = mMacType;
  aOther->mMacCreator = mMacCreator;
}

/* static */
nsresult
nsMIMEInfoBase::LaunchWithIProcess(nsIFile* aApp, const nsCString& aArg)
{
  NS_ASSERTION(aApp, "Unexpected null pointer, fix caller");

  nsresult rv;
  nsCOMPtr<nsIProcess> process = do_CreateInstance(NS_PROCESS_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  if (NS_FAILED(rv = process->Init(aApp)))
    return rv;

  const char *string = aArg.get();

  PRUint32 pid;
  return process->Run(PR_FALSE, &string, 1, &pid);
}

/* static */
nsresult
nsMIMEInfoBase::LaunchWithWebHandler(nsIWebHandlerApp *aApp, nsIURI *aURI,
                                     nsIInterfaceRequestor *aWindowContext) 
{
  
  nsCAutoString uriTemplate;
  nsresult rv = aApp->GetUriTemplate(uriTemplate);
  if (NS_FAILED(rv)) {
    return NS_ERROR_INVALID_ARG;
  }

  // get the URI spec so we can escape it for insertion into the template 
  nsCAutoString uriSpecToHandle;
  rv = aURI->GetSpec(uriSpecToHandle);
  if (NS_FAILED(rv)) {
    return NS_ERROR_INVALID_ARG;
  }

  // XXX need to strip passwd & username from URI to handle, as per the
  // WhatWG HTML5 draft.  nsSimpleURL, which is what we're going to get,
  // can't do this directly.  Ideally, we'd fix nsStandardURL to make it
  // possible to turn off all of its quirks handling, and use that...

  // XXX this doesn't exactly match how the HTML5 draft is requesting us to
  // escape; at the very least, it should be escaping @ signs, and there
  // may well be more issues (bug 382019).
  nsCAutoString escapedUriSpecToHandle;
  NS_EscapeURL(uriSpecToHandle, esc_Minimal | esc_Forced | esc_Colon,
               escapedUriSpecToHandle);

  // XXX note that this replace all occurrences of %s with the URL to be
  // handled, instead of just the first, as specified by the current draft
  // of the spec.  Bug 394476 filed to track this.
  uriTemplate.ReplaceSubstring(NS_LITERAL_CSTRING("%s"),
                               escapedUriSpecToHandle);

  // convert spec to URI; no original charset needed since there's no way
  // to communicate that information to any handler
  nsCOMPtr<nsIURI> uriToSend;
  rv = NS_NewURI(getter_AddRefs(uriToSend), uriTemplate);
  if (NS_FAILED(rv))
    return rv;

  // create a channel
  nsCOMPtr<nsIChannel> newChannel;
  rv = NS_NewChannel(getter_AddRefs(newChannel), uriToSend, nsnull, nsnull,
                     nsnull, nsIChannel::LOAD_DOCUMENT_URI);
  if (NS_FAILED(rv))
    return rv;

  // load the URI
  nsCOMPtr<nsIURILoader> uriLoader = do_GetService(NS_URI_LOADER_CONTRACTID, 
                                                   &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX ideally, aIsContentPreferred (the second param) should really be
  // passed in from above.  Practically, PR_TRUE is probably a reasonable
  // default since browsers don't care much, and link click is likely to be
  // the more interesting case for non-browser apps.  See 
  // <https://bugzilla.mozilla.org/show_bug.cgi?id=392957#c9> for details.
  return uriLoader->OpenURI(newChannel, PR_TRUE, aWindowContext);
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
nsMIMEInfoImpl::GetHasDefaultHandler(PRBool * _retval)
{
  *_retval = !mDefaultAppDescription.IsEmpty();
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

  nsCAutoString nativePath;
  aFile->GetNativePath(nativePath);
  
  return LaunchWithIProcess(mDefaultApplication, nativePath);
}

