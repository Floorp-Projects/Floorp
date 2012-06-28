/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsMIMEInfoAndroid.h"
#include "AndroidBridge.h"
#include "nsAndroidHandlerApp.h"
#include "nsArrayUtils.h"
#include "nsISupportsUtils.h"
#include "nsStringEnumerator.h"
#include "nsNetUtil.h"

NS_IMPL_ISUPPORTS2(nsMIMEInfoAndroid, nsIMIMEInfo, nsIHandlerInfo)

NS_IMETHODIMP
nsMIMEInfoAndroid::LaunchDefaultWithFile(nsIFile* aFile)
{
  LaunchWithFile(aFile);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::LoadUriInternal(nsIURI * aURI)
{
  nsCString uriSpec;
  aURI->GetSpec(uriSpec);

  nsCString uriScheme;
  aURI->GetScheme(uriScheme);

  if (mozilla::AndroidBridge::Bridge())
    return mozilla::AndroidBridge::Bridge()->
      OpenUriExternal(uriSpec, (mType.Equals(uriScheme) || mType.Equals(uriSpec)) ? EmptyCString() : mType) ? NS_OK : NS_ERROR_FAILURE;

  return NS_ERROR_FAILURE;
}


bool
nsMIMEInfoAndroid::GetMimeInfoForMimeType(const nsACString& aMimeType,
                                          nsMIMEInfoAndroid** aMimeInfo)
{
  nsRefPtr<nsMIMEInfoAndroid> info = new nsMIMEInfoAndroid(aMimeType);
  mozilla::AndroidBridge* bridge = mozilla::AndroidBridge::Bridge();
  // we don't have access to the bridge, so just assume we can handle
  // the mime type for now and let the system deal with it
  if (!bridge){
    info.forget(aMimeInfo);
    return false;
  }

  nsIHandlerApp* systemDefault = nsnull;
  bridge->GetHandlersForMimeType(nsCAutoString(aMimeType).get(), 
                                 info->mHandlerApps, &systemDefault);
  
  if (systemDefault)
    info->mPrefApp = systemDefault;

  nsCAutoString fileExt;
  bridge->GetExtensionFromMimeType(aMimeType, fileExt);
  info->SetPrimaryExtension(fileExt);
  
  PRUint32 len;
  info->mHandlerApps->GetLength(&len);
  if (len == 1) {
    info.forget(aMimeInfo);
    return false;
  }
  
  info.forget(aMimeInfo);
  return true;
}
  
bool
nsMIMEInfoAndroid::GetMimeInfoForFileExt(const nsACString& aFileExt,
                                         nsMIMEInfoAndroid **aMimeInfo)
{
  nsCString mimeType;
  if (mozilla::AndroidBridge::Bridge())
    mozilla::AndroidBridge::Bridge()->
      GetMimeTypeFromExtensions(aFileExt, mimeType);
  
  // "*/*" means that the bridge didn't know.
  if (mimeType.Equals(nsDependentCString("*/*"), nsCaseInsensitiveCStringComparator()))
    return false;

  bool found = GetMimeInfoForMimeType(mimeType, aMimeInfo);
  (*aMimeInfo)->SetPrimaryExtension(aFileExt);
  return found;
}

/**
 * Returns MIME info for the aURL, which may contain the whole URL or only a protocol
 */
nsresult
nsMIMEInfoAndroid::GetMimeInfoForURL(const nsACString &aURL,
                                     bool *found,
                                     nsIHandlerInfo **info)
{
  nsMIMEInfoAndroid *mimeinfo = new nsMIMEInfoAndroid(aURL);
  NS_ADDREF(*info = mimeinfo);
  *found = true;
  
  mozilla::AndroidBridge* bridge = mozilla::AndroidBridge::Bridge();
  if (!bridge) {
    // we don't have access to the bridge, so just assume we can handle
    // the protocol for now and let the system deal with it
    return NS_OK;
  }

  nsIHandlerApp* systemDefault = nsnull;
  bridge->GetHandlersForURL(nsCAutoString(aURL).get(), 
                            mimeinfo->mHandlerApps, &systemDefault);
  
  if (systemDefault)
    mimeinfo->mPrefApp = systemDefault;


  nsCAutoString fileExt;
  nsCAutoString mimeType;
  mimeinfo->GetType(mimeType);
  bridge->GetExtensionFromMimeType(mimeType, fileExt);
  mimeinfo->SetPrimaryExtension(fileExt);
  
  PRUint32 len;
  mimeinfo->mHandlerApps->GetLength(&len);
  if (len == 1) {
    // Code that calls this requires an object regardless if the OS has
    // something for us, so we return the empty object.
    *found = false;
    return NS_OK;
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetType(nsACString& aType)
{
  aType.Assign(mType);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetDescription(nsAString& aDesc)
{
  aDesc.Assign(mDescription);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetDescription(const nsAString& aDesc)
{
  mDescription.Assign(aDesc);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPreferredApplicationHandler(nsIHandlerApp** aApp)
{
  *aApp = mPrefApp;
  NS_IF_ADDREF(*aApp);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetPreferredApplicationHandler(nsIHandlerApp* aApp)
{
  mPrefApp = aApp;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPossibleApplicationHandlers(nsIMutableArray **aHandlerApps)
{
  if (!mHandlerApps)
    mHandlerApps = do_CreateInstance(NS_ARRAY_CONTRACTID);

  if (!mHandlerApps)
    return NS_ERROR_OUT_OF_MEMORY;

  *aHandlerApps = mHandlerApps;
  NS_IF_ADDREF(*aHandlerApps);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetHasDefaultHandler(bool* aHasDefault)
{
  PRUint32 len;
  *aHasDefault = false;
  if (!mHandlerApps)
    return NS_OK;

  if (NS_FAILED(mHandlerApps->GetLength(&len)))
    return NS_OK;

  if (len == 0)
    return NS_OK;

  *aHasDefault = true;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetDefaultDescription(nsAString& aDesc)
{
  aDesc.Assign(EmptyString());
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::LaunchWithURI(nsIURI* aURI, nsIInterfaceRequestor* req)
{
  return mPrefApp->LaunchWithURI(aURI, req);
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPreferredAction(nsHandlerInfoAction* aPrefAction)
{
  *aPrefAction = mPrefAction;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetPreferredAction(nsHandlerInfoAction aPrefAction)
{
  mPrefAction = aPrefAction;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetAlwaysAskBeforeHandling(bool* aAlwaysAsk)
{
  *aAlwaysAsk = mAlwaysAsk;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetAlwaysAskBeforeHandling(bool aAlwaysAsk)
{
  mAlwaysAsk = aAlwaysAsk;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetFileExtensions(nsIUTF8StringEnumerator** aResult)
{
  return NS_NewUTF8StringEnumerator(aResult, &mExtensions, this);
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetFileExtensions(const nsACString & aExtensions)
{
  mExtensions.Clear();
  nsCString extList(aExtensions);

  PRInt32 breakLocation = -1;
  while ( (breakLocation = extList.FindChar(',')) != -1)
  {
    mExtensions.AppendElement(Substring(extList.get(), extList.get() + breakLocation));
    extList.Cut(0, breakLocation + 1);
  }
  if (!extList.IsEmpty())
    mExtensions.AppendElement(extList);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::ExtensionExists(const nsACString & aExtension, bool *aRetVal)
{
  NS_ASSERTION(!aExtension.IsEmpty(), "no extension");
  bool found = false;
  PRUint32 extCount = mExtensions.Length();
  if (extCount < 1) return NS_OK;

  for (PRUint8 i=0; i < extCount; i++) {
    const nsCString& ext = mExtensions[i];
    if (ext.Equals(aExtension, nsCaseInsensitiveCStringComparator())) {
      found = true;
      break;
    }
  }

  *aRetVal = found;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::AppendExtension(const nsACString & aExtension)
{
  mExtensions.AppendElement(aExtension);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPrimaryExtension(nsACString & aPrimaryExtension)
{
  if (!mExtensions.Length())
    return NS_ERROR_NOT_INITIALIZED;

  aPrimaryExtension = mExtensions[0];
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetPrimaryExtension(const nsACString & aExtension)
{
  NS_ASSERTION(!aExtension.IsEmpty(), "no extension");
  PRUint32 extCount = mExtensions.Length();
  PRUint8 i;
  bool found = false;
  for (i=0; i < extCount; i++) {
    const nsCString& ext = mExtensions[i];
    if (ext.Equals(aExtension, nsCaseInsensitiveCStringComparator())) {
      found = true;
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
nsMIMEInfoAndroid::GetMIMEType(nsACString & aMIMEType)
{
  aMIMEType.Assign(mType);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::Equals(nsIMIMEInfo *aMIMEInfo, bool *aRetVal)
{
  if (!aMIMEInfo) return NS_ERROR_NULL_POINTER;

  nsCAutoString type;
  nsresult rv = aMIMEInfo->GetMIMEType(type);
  if (NS_FAILED(rv)) return rv;

  *aRetVal = mType.Equals(type);

  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPossibleLocalHandlers(nsIArray * *aPossibleLocalHandlers)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::LaunchWithFile(nsIFile *aFile)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewFileURI(getter_AddRefs(uri), aFile);
  LoadUriInternal(uri);
  return NS_OK;
}

nsMIMEInfoAndroid::nsMIMEInfoAndroid(const nsACString& aMIMEType) :
  mType(aMIMEType), mAlwaysAsk(true),
  mPrefAction(nsIMIMEInfo::useHelperApp)
{
  mPrefApp = new nsMIMEInfoAndroid::SystemChooser(this);
  nsresult rv;
  mHandlerApps = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  mHandlerApps->AppendElement(mPrefApp, false);
}

NS_IMPL_ISUPPORTS1(nsMIMEInfoAndroid::SystemChooser, nsIHandlerApp)


nsresult nsMIMEInfoAndroid::SystemChooser::GetName(nsAString & aName) {
  aName.Assign(NS_LITERAL_STRING("Android chooser"));
  return NS_OK;
}

nsresult
nsMIMEInfoAndroid::SystemChooser::SetName(const nsAString&) {
  return NS_OK;
}

nsresult
nsMIMEInfoAndroid::SystemChooser::GetDetailedDescription(nsAString & aDesc) {
  aDesc.Assign(NS_LITERAL_STRING("Android's default handler application chooser"));
  return NS_OK;
}

nsresult
nsMIMEInfoAndroid::SystemChooser::SetDetailedDescription(const nsAString&) {
  return NS_OK;
}

nsresult
nsMIMEInfoAndroid::SystemChooser::Equals(nsIHandlerApp *aHandlerApp, bool *aRetVal) {
  nsCOMPtr<nsMIMEInfoAndroid::SystemChooser> info = do_QueryInterface(aHandlerApp);
  if (info)
    return mOuter->Equals(info->mOuter, aRetVal);
  *aRetVal = false;
  return NS_OK;
}

nsresult
nsMIMEInfoAndroid::SystemChooser::LaunchWithURI(nsIURI* aURI, nsIInterfaceRequestor*)
{
  return mOuter->LoadUriInternal(aURI);
}
