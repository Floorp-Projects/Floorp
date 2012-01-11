/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>
 *   Brian R. Bondy <netzen@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7

#include "JumpListItem.h"

#include <shellapi.h>
#include <propvarutil.h>
#include <propkey.h>

#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsNetUtil.h"
#include "nsCRT.h"
#include "nsNetCID.h"
#include "nsCExternalHandlerService.h"
#include "nsCycleCollectionParticipant.h"
#include "mozIAsyncFavicons.h"
#include "mozilla/Preferences.h"
#include "JumpListBuilder.h"
#include "WinUtils.h"

namespace mozilla {
namespace widget {

const char JumpListItem::kJumpListCacheDir[] = "jumpListCache";

// ISUPPORTS Impl's
NS_IMPL_ISUPPORTS1(JumpListItem,
                   nsIJumpListItem)

NS_IMPL_ISUPPORTS_INHERITED1(JumpListSeparator,
                             JumpListItem,
                             nsIJumpListSeparator)

NS_IMPL_ISUPPORTS_INHERITED1(JumpListLink,
                             JumpListItem,
                             nsIJumpListLink)

NS_IMPL_CYCLE_COLLECTION_CLASS(JumpListShortcut)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JumpListShortcut)
  NS_INTERFACE_MAP_ENTRY(nsIJumpListShortcut)
NS_INTERFACE_MAP_END_INHERITING(JumpListItem)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(JumpListShortcut)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mHandlerApp)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(JumpListShortcut)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mHandlerApp)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(JumpListShortcut)
NS_IMPL_CYCLE_COLLECTING_RELEASE(JumpListShortcut)

/* attribute short type; */
NS_IMETHODIMP JumpListItem::GetType(PRInt16 *aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  *aType = mItemType;

  return NS_OK;
}

/* boolean equals(nsIJumpListItem item); */
NS_IMETHODIMP JumpListItem::Equals(nsIJumpListItem *aItem, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aItem);

  *aResult = false;

  PRInt16 theType = nsIJumpListItem::JUMPLIST_ITEM_EMPTY;
  if (NS_FAILED(aItem->GetType(&theType)))
    return NS_OK;

  // Make sure the types match.
  if (Type() != theType)
    return NS_OK;

  *aResult = true;

  return NS_OK;
}

/* link impl. */

/* attribute nsIURI uri; */
NS_IMETHODIMP JumpListLink::GetUri(nsIURI **aURI)
{
  NS_IF_ADDREF(*aURI = mURI);

  return NS_OK;
}

NS_IMETHODIMP JumpListLink::SetUri(nsIURI *aURI)
{
  mURI = aURI;
  
  return NS_OK;
}

/* attribute AString uriTitle; */
NS_IMETHODIMP JumpListLink::SetUriTitle(const nsAString &aUriTitle)
{
  mUriTitle.Assign(aUriTitle);

  return NS_OK;
}

NS_IMETHODIMP JumpListLink::GetUriTitle(nsAString& aUriTitle)
{
  aUriTitle.Assign(mUriTitle);
  
  return NS_OK;
}

/* readonly attribute long uriHash; */
NS_IMETHODIMP JumpListLink::GetUriHash(nsACString& aUriHash)
{
  if (!mURI)
    return NS_ERROR_NOT_AVAILABLE;

  return JumpListItem::HashURI(mCryptoHash, mURI, aUriHash);
}

/* boolean compareHash(in nsIURI uri); */
NS_IMETHODIMP JumpListLink::CompareHash(nsIURI *aUri, bool *aResult)
{
  nsresult rv;

  if (!mURI) {
    *aResult = !aUri;
    return NS_OK;
  }

  NS_ENSURE_ARG_POINTER(aUri);

  nsCAutoString hash1, hash2;

  rv = JumpListItem::HashURI(mCryptoHash, mURI, hash1);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = JumpListItem::HashURI(mCryptoHash, aUri, hash2);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = hash1.Equals(hash2);

  return NS_OK;
}

/* boolean equals(nsIJumpListItem item); */
NS_IMETHODIMP JumpListLink::Equals(nsIJumpListItem *aItem, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aItem);

  nsresult rv;

  *aResult = false;

  PRInt16 theType = nsIJumpListItem::JUMPLIST_ITEM_EMPTY;
  if (NS_FAILED(aItem->GetType(&theType)))
    return NS_OK;

  // Make sure the types match.
  if (Type() != theType)
    return NS_OK;

  nsCOMPtr<nsIJumpListLink> link = do_QueryInterface(aItem, &rv);
  if (NS_FAILED(rv))
    return rv;

  // Check the titles
  nsAutoString title;
  link->GetUriTitle(title);
  if (!mUriTitle.Equals(title))
    return NS_OK;

  // Call the internal object's equals() method to check.
  nsCOMPtr<nsIURI> theUri;
  bool equals = false;
  if (NS_SUCCEEDED(link->GetUri(getter_AddRefs(theUri)))) {
    if (!theUri) {
      if (!mURI)
        *aResult = true;
      return NS_OK;
    }
    if (NS_SUCCEEDED(theUri->Equals(mURI, &equals)) && equals) {
      *aResult = true;
    }
  }

  return NS_OK;
}

/* shortcut impl. */

/* attribute nsILocalHandlerApp app; */
NS_IMETHODIMP JumpListShortcut::GetApp(nsILocalHandlerApp **aApp)
{
  NS_IF_ADDREF(*aApp = mHandlerApp);
  
  return NS_OK;
}

NS_IMETHODIMP JumpListShortcut::SetApp(nsILocalHandlerApp *aApp)
{
  mHandlerApp = aApp;

  // Confirm the app is present on the system
  if (!ExecutableExists(mHandlerApp))
    return NS_ERROR_FILE_NOT_FOUND;

  return NS_OK;
}

/* attribute long iconIndex; */
NS_IMETHODIMP JumpListShortcut::GetIconIndex(PRInt32 *aIconIndex)
{
  NS_ENSURE_ARG_POINTER(aIconIndex);

  *aIconIndex = mIconIndex;
  return NS_OK;
}

NS_IMETHODIMP JumpListShortcut::SetIconIndex(PRInt32 aIconIndex)
{
  mIconIndex = aIconIndex;
  return NS_OK;
}

/* attribute long iconURI; */
NS_IMETHODIMP JumpListShortcut::GetFaviconPageUri(nsIURI **aFaviconPageURI)
{
  NS_IF_ADDREF(*aFaviconPageURI = mFaviconPageURI);

  return NS_OK;
}

NS_IMETHODIMP JumpListShortcut::SetFaviconPageUri(nsIURI *aFaviconPageURI)
{
  mFaviconPageURI = aFaviconPageURI;
  return NS_OK;
}

/* boolean equals(nsIJumpListItem item); */
NS_IMETHODIMP JumpListShortcut::Equals(nsIJumpListItem *aItem, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aItem);

  nsresult rv;

  *aResult = false;

  PRInt16 theType = nsIJumpListItem::JUMPLIST_ITEM_EMPTY;
  if (NS_FAILED(aItem->GetType(&theType)))
    return NS_OK;

  // Make sure the types match.
  if (Type() != theType)
    return NS_OK;

  nsCOMPtr<nsIJumpListShortcut> shortcut = do_QueryInterface(aItem, &rv);
  if (NS_FAILED(rv))
    return rv;

  // Check the icon index
  //PRInt32 idx;
  //shortcut->GetIconIndex(&idx);
  //if (mIconIndex != idx)
  //  return NS_OK;
  // No need to check the icon page URI either

  // Call the internal object's equals() method to check.
  nsCOMPtr<nsILocalHandlerApp> theApp;
  bool equals = false;
  if (NS_SUCCEEDED(shortcut->GetApp(getter_AddRefs(theApp)))) {
    if (!theApp) {
      if (!mHandlerApp)
        *aResult = true;
      return NS_OK;
    }
    if (NS_SUCCEEDED(theApp->Equals(mHandlerApp, &equals)) && equals) {
      *aResult = true;
    }
  }

  return NS_OK;
}

/* internal helpers */

// (static) Creates a ShellLink that encapsulate a separator.
nsresult JumpListSeparator::GetSeparator(nsRefPtr<IShellLinkW>& aShellLink)
{
  HRESULT hr;
  IShellLinkW* psl;

  // Create a IShellLink.
  hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, 
                        IID_IShellLinkW, (LPVOID*)&psl);
  if (FAILED(hr))
    return NS_ERROR_UNEXPECTED;

  IPropertyStore* pPropStore = nsnull;
  hr = psl->QueryInterface(IID_IPropertyStore, (LPVOID*)&pPropStore);
  if (FAILED(hr))
    return NS_ERROR_UNEXPECTED;

  PROPVARIANT pv;
  InitPropVariantFromBoolean(TRUE, &pv);

  pPropStore->SetValue(PKEY_AppUserModel_IsDestListSeparator, pv);
  pPropStore->Commit();
  pPropStore->Release();

  PropVariantClear(&pv);

  aShellLink = dont_AddRef(psl);

  return NS_OK;
}

// Obtains the jump list 'ICO cache timeout in seconds' pref
static PRInt32 GetICOCacheSecondsTimeout() {

  // Only obtain the setting at most once from the pref service.
  // In the rare case that 2 threads call this at the same
  // time it is no harm and we will simply obtain the pref twice.
  // None of the taskbar list prefs are currently updated via a
  // pref observer so I think this should suffice.
  const PRInt32 kSecondsPerDay = 86400;
  static bool alreadyObtained = false;
  static PRInt32 icoReCacheSecondsTimeout = kSecondsPerDay;
  if (alreadyObtained) {
    return icoReCacheSecondsTimeout;
  }

  // Obtain the pref
  const char PREF_ICOTIMEOUT[]  = "browser.taskbar.lists.icoTimeoutInSeconds";
  icoReCacheSecondsTimeout = Preferences::GetInt(PREF_ICOTIMEOUT, 
                                                 kSecondsPerDay);
  alreadyObtained = true;
  return icoReCacheSecondsTimeout;
}

// (static) If the data is available, will return the path on disk where 
// the favicon for page aFaviconPageURI is stored.  If the favicon does not
// exist, or its cache is expired, this method will kick off an async request
// for the icon so that next time the method is called it will be available. 
nsresult JumpListShortcut::ObtainCachedIconFile(nsCOMPtr<nsIURI> aFaviconPageURI,
                                                nsString &aICOFilePath,
                                                nsCOMPtr<nsIThread> &aIOThread)
{
  // Obtain the ICO file path
  nsCOMPtr<nsIFile> icoFile;
  nsresult rv = GetOutputIconPath(aFaviconPageURI, icoFile);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check if the cached ICO file already exists
  bool exists;
  rv = icoFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {

    // Obtain the file's last modification date in seconds
    PRInt64 fileModTime = LL_ZERO;
    rv = icoFile->GetLastModifiedTime(&fileModTime);
    fileModTime /= PR_MSEC_PER_SEC;
    PRInt32 icoReCacheSecondsTimeout = GetICOCacheSecondsTimeout();
    PRInt64 nowTime = PR_Now() / PRInt64(PR_USEC_PER_SEC);

    // If the last mod call failed or the icon is old then re-cache it
    // This check is in case the favicon of a page changes
    // the next time we try to build the jump list, the data will be available.
    if (NS_FAILED(rv) ||
        (nowTime - fileModTime) > icoReCacheSecondsTimeout) {
      CacheIconFileFromFaviconURIAsync(aFaviconPageURI, icoFile, aIOThread);
      return NS_ERROR_NOT_AVAILABLE;
    }
  } else {

    // The file does not exist yet, obtain it async from the favicon service so that
    // the next time we try to build the jump list it'll be available.
    CacheIconFileFromFaviconURIAsync(aFaviconPageURI, icoFile, aIOThread);
    return NS_ERROR_NOT_AVAILABLE;
  }

  // The icoFile is filled with a path that exists, get its path
  rv = icoFile->GetPath(aICOFilePath);
  return rv;
}

// (static) Obtains the ICO file for the favicon at page aFaviconPageURI
// If successful, the file path on disk is in the format:
// <ProfLDS>\jumpListCache\<hash(aFaviconPageURI)>.ico
nsresult JumpListShortcut::GetOutputIconPath(nsCOMPtr<nsIURI> aFaviconPageURI,
                                             nsCOMPtr<nsIFile> &aICOFile) 
{
  // Hash the input URI and replace any / with _
  nsCAutoString inputURIHash;
  nsCOMPtr<nsICryptoHash> cryptoHash;
  nsresult rv = JumpListItem::HashURI(cryptoHash, aFaviconPageURI,
                                      inputURIHash);
  NS_ENSURE_SUCCESS(rv, rv);
  char* cur = inputURIHash.BeginWriting();
  char* end = inputURIHash.EndWriting();
  for (; cur < end; ++cur) {
    if ('/' == *cur) {
      *cur = '_';
    }
  }

  // Obtain the local profile directory and construct the output icon file path
  rv = NS_GetSpecialDirectory("ProfLDS", getter_AddRefs(aICOFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aICOFile->AppendNative(nsDependentCString(JumpListItem::kJumpListCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);

  // Try to create the directory if it's not there yet
  rv = aICOFile->Create(nsIFile::DIRECTORY_TYPE, 0777);
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS) {
    return rv;
  }
  
  // Append the icon extension
  inputURIHash.Append(".ico");
  rv = aICOFile->AppendNative(inputURIHash);

  return rv;
}

// (static) Asynchronously creates a cached ICO file on disk for the favicon of
// page aFaviconPageURI and stores it to disk at the path of aICOFile.
nsresult 
JumpListShortcut::CacheIconFileFromFaviconURIAsync(nsCOMPtr<nsIURI> aFaviconPageURI,
                                                   nsCOMPtr<nsIFile> aICOFile,
                                                   nsCOMPtr<nsIThread> &aIOThread)
{
  // Obtain the favicon service and get the favicon for the specified page
  nsCOMPtr<mozIAsyncFavicons> favIconSvc(
                      do_GetService("@mozilla.org/browser/favicon-service;1"));
  NS_ENSURE_TRUE(favIconSvc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIFaviconDataCallback> callback = 
    new AsyncFaviconDataReady(aFaviconPageURI, aIOThread);

  favIconSvc->GetFaviconDataForPage(aFaviconPageURI, callback);
  return NS_OK;
}

// (static) Creates a ShellLink that encapsulate a shortcut to local apps.
nsresult JumpListShortcut::GetShellLink(nsCOMPtr<nsIJumpListItem>& item, 
                                        nsRefPtr<IShellLinkW>& aShellLink,
                                        nsCOMPtr<nsIThread> &aIOThread)
{
  HRESULT hr;
  IShellLinkW* psl;
  nsresult rv;

  // Shell links:
  // http://msdn.microsoft.com/en-us/library/bb776891(VS.85).aspx
  // http://msdn.microsoft.com/en-us/library/bb774950(VS.85).aspx

  PRInt16 type;
  if (NS_FAILED(item->GetType(&type)))
    return NS_ERROR_INVALID_ARG;

  if (type != nsIJumpListItem::JUMPLIST_ITEM_SHORTCUT)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIJumpListShortcut> shortcut = do_QueryInterface(item, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocalHandlerApp> handlerApp;
  rv = shortcut->GetApp(getter_AddRefs(handlerApp));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create a IShellLink 
  hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                        IID_IShellLinkW, (LPVOID*)&psl);
  if (FAILED(hr))
    return NS_ERROR_UNEXPECTED;

  // Retrieve the app path, title, description and optional command line args.
  nsAutoString appPath, appTitle, appDescription, appArgs;
  PRInt32 appIconIndex = 0;

  // Path
  nsCOMPtr<nsIFile> executable;
  handlerApp->GetExecutable(getter_AddRefs(executable));
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(executable, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = localFile->GetPath(appPath);
  NS_ENSURE_SUCCESS(rv, rv);

  // Command line parameters
  PRUint32 count = 0;
  handlerApp->GetParameterCount(&count);
  for (PRUint32 idx = 0; idx < count; idx++) {
    if (idx > 0)
      appArgs.Append(NS_LITERAL_STRING(" "));
    nsAutoString param;
    rv = handlerApp->GetParameter(idx, param);
    if (NS_FAILED(rv))
      return rv;
    appArgs.Append(param);
  }

  handlerApp->GetName(appTitle);
  handlerApp->GetDetailedDescription(appDescription);

  bool useUriIcon = false; // if we want to use the URI icon
  bool usedUriIcon = false; // if we did use the URI icon
  shortcut->GetIconIndex(&appIconIndex);
  
  nsCOMPtr<nsIURI> iconUri;
  rv = shortcut->GetFaviconPageUri(getter_AddRefs(iconUri));
  if (NS_SUCCEEDED(rv) && iconUri) {
    useUriIcon = true;
  }

  // Store the title of the app
  if (appTitle.Length() > 0) {
    IPropertyStore* pPropStore = nsnull;
    hr = psl->QueryInterface(IID_IPropertyStore, (LPVOID*)&pPropStore);
    if (FAILED(hr))
      return NS_ERROR_UNEXPECTED;

    PROPVARIANT pv;
    InitPropVariantFromString(appTitle.get(), &pv);

    pPropStore->SetValue(PKEY_Title, pv);
    pPropStore->Commit();
    pPropStore->Release();

    PropVariantClear(&pv);
  }

  // Store the rest of the params
  psl->SetPath(appPath.get());
  psl->SetDescription(appDescription.get());
  psl->SetArguments(appArgs.get());

  if (useUriIcon) {
    nsString icoFilePath;
    rv = ObtainCachedIconFile(iconUri, icoFilePath, aIOThread);
    if (NS_SUCCEEDED(rv)) {
      // Always use the first icon in the ICO file
      // our encoded icon only has 1 resource
      psl->SetIconLocation(icoFilePath.get(), 0);
      usedUriIcon = true;
    }
  }

  // We didn't use an ICO via URI so fall back to the app icon
  if (!usedUriIcon) {
    psl->SetIconLocation(appPath.get(), appIconIndex);
  }

  aShellLink = dont_AddRef(psl);

  return NS_OK;
}

// If successful fills in the aSame parameter
// aSame will be true if the path is in our icon cache
static nsresult IsPathInOurIconCache(nsCOMPtr<nsIJumpListShortcut>& aShortcut, 
                                     PRUnichar *aPath, bool *aSame)
{
  NS_ENSURE_ARG_POINTER(aPath);
  NS_ENSURE_ARG_POINTER(aSame);
 
  *aSame = false;

  // Construct the path of our jump list cache
  nsCOMPtr<nsIFile> jumpListCache;
  nsresult rv = NS_GetSpecialDirectory("ProfLDS", getter_AddRefs(jumpListCache));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = jumpListCache->AppendNative(nsDependentCString(JumpListItem::kJumpListCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoString jumpListCachePath;
  rv = jumpListCache->GetPath(jumpListCachePath);
  NS_ENSURE_SUCCESS(rv, rv);

  // Construct the parent path of the passed in path
  nsCOMPtr<nsILocalFile> passedInFile = do_CreateInstance("@mozilla.org/file/local;1");
  NS_ENSURE_TRUE(passedInFile, NS_ERROR_FAILURE);
  nsAutoString passedInPath(aPath);
  rv = passedInFile->InitWithPath(passedInPath);
  nsCOMPtr<nsIFile> passedInParentFile;
  passedInFile->GetParent(getter_AddRefs(passedInParentFile));
  nsAutoString passedInParentPath;
  rv = jumpListCache->GetPath(passedInParentPath);
  NS_ENSURE_SUCCESS(rv, rv);

  *aSame = jumpListCachePath.Equals(passedInParentPath);
  return NS_OK;
}

// (static) For a given IShellLink, create and return a populated nsIJumpListShortcut.
nsresult JumpListShortcut::GetJumpListShortcut(IShellLinkW *pLink, nsCOMPtr<nsIJumpListShortcut>& aShortcut)
{
  NS_ENSURE_ARG_POINTER(pLink);

  nsresult rv;
  HRESULT hres;

  nsCOMPtr<nsILocalHandlerApp> handlerApp = 
    do_CreateInstance(NS_LOCALHANDLERAPP_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUnichar buf[MAX_PATH];

  // Path
  hres = pLink->GetPath((LPWSTR)&buf, MAX_PATH, NULL, SLGP_UNCPRIORITY);
  if (FAILED(hres))
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsILocalFile> file;
  nsDependentString filepath(buf);
  rv = NS_NewLocalFile(filepath, false, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = handlerApp->SetExecutable(file);
  NS_ENSURE_SUCCESS(rv, rv);

  // Parameters
  hres = pLink->GetArguments((LPWSTR)&buf, MAX_PATH);
  if (SUCCEEDED(hres)) {
    LPWSTR *arglist;
    PRInt32 numArgs;
    PRInt32 idx;

    arglist = ::CommandLineToArgvW(buf, &numArgs);
    if(arglist) {
      for (idx = 0; idx < numArgs; idx++) {
        // szArglist[i] is null terminated
        nsDependentString arg(arglist[idx]);
        handlerApp->AppendParameter(arg);
      }
      ::LocalFree(arglist);
    }
  }

  rv = aShortcut->SetApp(handlerApp);
  NS_ENSURE_SUCCESS(rv, rv);

  // Icon index or file location
  int iconIdx = 0;
  hres = pLink->GetIconLocation((LPWSTR)&buf, MAX_PATH, &iconIdx);
  if (SUCCEEDED(hres)) {
    // XXX How do we handle converting local files to images here? Do we need to?
    aShortcut->SetIconIndex(iconIdx);

    // Obtain the local profile directory and construct the output icon file path
    // We only set the Icon Uri if we're sure it was from our icon cache.
    bool isInOurCache;
    if (NS_SUCCEEDED(IsPathInOurIconCache(aShortcut, buf, &isInOurCache)) && 
        isInOurCache) {
      nsCOMPtr<nsIURI> iconUri;
      nsAutoString path(buf);
      rv = NS_NewURI(getter_AddRefs(iconUri), path);
      if (NS_SUCCEEDED(rv)) {
        aShortcut->SetFaviconPageUri(iconUri);
      }
    }
  }

  // Do we need the title and description? Probably not since handler app doesn't compare
  // these in equals.

  return NS_OK;
}

// (static) ShellItems are used to encapsulate links to things. We currently only support URI links,
// but more support could be added, such as local file and directory links.
nsresult JumpListLink::GetShellItem(nsCOMPtr<nsIJumpListItem>& item, nsRefPtr<IShellItem2>& aShellItem)
{
  IShellItem2 *psi = nsnull;
  nsresult rv;

  PRInt16 type; 
  if (NS_FAILED(item->GetType(&type)))
    return NS_ERROR_INVALID_ARG;

  if (type != nsIJumpListItem::JUMPLIST_ITEM_LINK)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIJumpListLink> link = do_QueryInterface(item, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  rv = link->GetUri(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString spec;
  rv = uri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // Load vista+ SHCreateItemFromParsingName
  if (!WinUtils::VistaCreateItemFromParsingNameInit()) {
    return NS_ERROR_UNEXPECTED;
  }

  // Create the IShellItem
  if (FAILED(WinUtils::SHCreateItemFromParsingName(
               NS_ConvertASCIItoUTF16(spec).get(), NULL, IID_PPV_ARGS(&psi)))) {
    return NS_ERROR_INVALID_ARG;
  }

  // Set the title
  nsAutoString linkTitle;
  link->GetUriTitle(linkTitle);

  IPropertyStore* pPropStore = nsnull;
  HRESULT hres = psi->GetPropertyStore(GPS_DEFAULT, IID_IPropertyStore, (void**)&pPropStore);
  if (FAILED(hres))
    return NS_ERROR_UNEXPECTED;

  PROPVARIANT pv;
  InitPropVariantFromString(linkTitle.get(), &pv);

  // May fail due to shell item access permissions.
  pPropStore->SetValue(PKEY_ItemName, pv);
  pPropStore->Commit();
  pPropStore->Release();

  PropVariantClear(&pv);

  aShellItem = dont_AddRef(psi);

  return NS_OK;
}

// (static) For a given IShellItem, create and return a populated nsIJumpListLink.
nsresult JumpListLink::GetJumpListLink(IShellItem *pItem, nsCOMPtr<nsIJumpListLink>& aLink)
{
  NS_ENSURE_ARG_POINTER(pItem);

  // We assume for now these are URI links, but through properties we could
  // query and create other types.
  nsresult rv;
  LPWSTR lpstrName = NULL;

  if (SUCCEEDED(pItem->GetDisplayName(SIGDN_URL, &lpstrName))) {
    nsCOMPtr<nsIURI> uri;
    nsAutoString spec(lpstrName);

    rv = NS_NewURI(getter_AddRefs(uri), NS_ConvertUTF16toUTF8(spec));
    if (NS_FAILED(rv))
      return NS_ERROR_INVALID_ARG;

    aLink->SetUri(uri);

    ::CoTaskMemFree(lpstrName);
  }

  return NS_OK;
}

// Confirm the app is on the system
bool JumpListShortcut::ExecutableExists(nsCOMPtr<nsILocalHandlerApp>& handlerApp)
{
  nsresult rv;

  if (!handlerApp)
    return false;

  nsCOMPtr<nsIFile> executable;
  rv = handlerApp->GetExecutable(getter_AddRefs(executable));
  if (NS_SUCCEEDED(rv) && executable) {
    bool exists;
    executable->Exists(&exists);
    return exists;
  }
  return false;
}

// (static) Helper method which will hash a URI
nsresult JumpListItem::HashURI(nsCOMPtr<nsICryptoHash> &aCryptoHash, 
                               nsIURI *aUri, nsACString& aUriHash)
{
  if (!aUri)
    return NS_ERROR_INVALID_ARG;

  nsCAutoString spec;
  nsresult rv = aUri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aCryptoHash) {
    aCryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aCryptoHash->Init(nsICryptoHash::MD5);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCryptoHash->Update(reinterpret_cast<const PRUint8*>(spec.BeginReading()), 
                                                            spec.Length());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCryptoHash->Finish(true, aUriHash);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // namespace widget
} // namespace mozilla

#endif // MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
