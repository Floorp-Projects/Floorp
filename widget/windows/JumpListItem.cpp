/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JumpListItem.h"

#include <shellapi.h>
#include <propvarutil.h>
#include <propkey.h>

#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsCRT.h"
#include "nsNetCID.h"
#include "nsCExternalHandlerService.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Preferences.h"
#include "JumpListBuilder.h"
#include "WinUtils.h"

namespace mozilla {
namespace widget {

// ISUPPORTS Impl's
NS_IMPL_ISUPPORTS(JumpListItem,
                  nsIJumpListItem)

NS_INTERFACE_MAP_BEGIN(JumpListSeparator)
  NS_INTERFACE_MAP_ENTRY(nsIJumpListSeparator)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIJumpListItem, JumpListItemBase)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, JumpListItemBase)
NS_INTERFACE_MAP_END
NS_IMPL_ADDREF(JumpListSeparator)
NS_IMPL_RELEASE(JumpListSeparator)

NS_INTERFACE_MAP_BEGIN(JumpListLink)
  NS_INTERFACE_MAP_ENTRY(nsIJumpListLink)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIJumpListItem, JumpListItemBase)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, JumpListItemBase)
NS_INTERFACE_MAP_END
NS_IMPL_ADDREF(JumpListLink)
NS_IMPL_RELEASE(JumpListLink)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JumpListShortcut)
  NS_INTERFACE_MAP_ENTRY(nsIJumpListShortcut)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIJumpListItem, JumpListItemBase)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIJumpListShortcut)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(JumpListShortcut)
NS_IMPL_CYCLE_COLLECTING_RELEASE(JumpListShortcut)
NS_IMPL_CYCLE_COLLECTION(JumpListShortcut, mHandlerApp)

NS_IMETHODIMP JumpListItemBase::GetType(int16_t *aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  *aType = mItemType;

  return NS_OK;
}

NS_IMETHODIMP JumpListItemBase::Equals(nsIJumpListItem *aItem, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aItem);

  *aResult = false;

  int16_t theType = nsIJumpListItem::JUMPLIST_ITEM_EMPTY;
  if (NS_FAILED(aItem->GetType(&theType)))
    return NS_OK;

  // Make sure the types match.
  if (Type() != theType)
    return NS_OK;

  *aResult = true;

  return NS_OK;
}

/* link impl. */

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

NS_IMETHODIMP JumpListLink::GetUriHash(nsACString& aUriHash)
{
  if (!mURI)
    return NS_ERROR_NOT_AVAILABLE;

  return mozilla::widget::FaviconHelper::HashURI(mCryptoHash, mURI, aUriHash);
}

NS_IMETHODIMP JumpListLink::CompareHash(nsIURI *aUri, bool *aResult)
{
  nsresult rv;

  if (!mURI) {
    *aResult = !aUri;
    return NS_OK;
  }

  NS_ENSURE_ARG_POINTER(aUri);

  nsAutoCString hash1, hash2;

  rv = mozilla::widget::FaviconHelper::HashURI(mCryptoHash, mURI, hash1);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mozilla::widget::FaviconHelper::HashURI(mCryptoHash, aUri, hash2);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = hash1.Equals(hash2);

  return NS_OK;
}

NS_IMETHODIMP JumpListLink::Equals(nsIJumpListItem *aItem, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aItem);

  nsresult rv;

  *aResult = false;

  int16_t theType = nsIJumpListItem::JUMPLIST_ITEM_EMPTY;
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

NS_IMETHODIMP JumpListShortcut::GetIconIndex(int32_t *aIconIndex)
{
  NS_ENSURE_ARG_POINTER(aIconIndex);

  *aIconIndex = mIconIndex;
  return NS_OK;
}

NS_IMETHODIMP JumpListShortcut::SetIconIndex(int32_t aIconIndex)
{
  mIconIndex = aIconIndex;
  return NS_OK;
}

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

NS_IMETHODIMP JumpListShortcut::Equals(nsIJumpListItem *aItem, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aItem);

  nsresult rv;

  *aResult = false;

  int16_t theType = nsIJumpListItem::JUMPLIST_ITEM_EMPTY;
  if (NS_FAILED(aItem->GetType(&theType)))
    return NS_OK;

  // Make sure the types match.
  if (Type() != theType)
    return NS_OK;

  nsCOMPtr<nsIJumpListShortcut> shortcut = do_QueryInterface(aItem, &rv);
  if (NS_FAILED(rv))
    return rv;

  // Check the icon index
  //int32_t idx;
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
nsresult JumpListSeparator::GetSeparator(RefPtr<IShellLinkW>& aShellLink)
{
  HRESULT hr;
  IShellLinkW* psl;

  // Create a IShellLink.
  hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, 
                        IID_IShellLinkW, (LPVOID*)&psl);
  if (FAILED(hr))
    return NS_ERROR_UNEXPECTED;

  IPropertyStore* pPropStore = nullptr;
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

// (static) Creates a ShellLink that encapsulate a shortcut to local apps.
nsresult JumpListShortcut::GetShellLink(nsCOMPtr<nsIJumpListItem>& item, 
                                        RefPtr<IShellLinkW>& aShellLink,
                                        nsCOMPtr<nsIThread> &aIOThread)
{
  HRESULT hr;
  IShellLinkW* psl;
  nsresult rv;

  // Shell links:
  // http://msdn.microsoft.com/en-us/library/bb776891(VS.85).aspx
  // http://msdn.microsoft.com/en-us/library/bb774950(VS.85).aspx

  int16_t type;
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
  hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                        IID_IShellLinkW, (LPVOID*)&psl);
  if (FAILED(hr))
    return NS_ERROR_UNEXPECTED;

  // Retrieve the app path, title, description and optional command line args.
  nsAutoString appPath, appTitle, appDescription, appArgs;
  int32_t appIconIndex = 0;

  // Path
  nsCOMPtr<nsIFile> executable;
  handlerApp->GetExecutable(getter_AddRefs(executable));

  rv = executable->GetPath(appPath);
  NS_ENSURE_SUCCESS(rv, rv);

  // Command line parameters
  uint32_t count = 0;
  handlerApp->GetParameterCount(&count);
  for (uint32_t idx = 0; idx < count; idx++) {
    if (idx > 0)
      appArgs.Append(' ');
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
    IPropertyStore* pPropStore = nullptr;
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
    rv = mozilla::widget::FaviconHelper::ObtainCachedIconFile(iconUri, 
                                                              icoFilePath, 
                                                              aIOThread,
                                                              false);
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
                                     wchar_t *aPath, bool *aSame)
{
  NS_ENSURE_ARG_POINTER(aPath);
  NS_ENSURE_ARG_POINTER(aSame);
 
  *aSame = false;

  // Construct the path of our jump list cache
  nsCOMPtr<nsIFile> jumpListCache;
  nsresult rv = NS_GetSpecialDirectory("ProfLDS", getter_AddRefs(jumpListCache));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = jumpListCache->AppendNative(nsDependentCString(FaviconHelper::kJumpListCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoString jumpListCachePath;
  rv = jumpListCache->GetPath(jumpListCachePath);
  NS_ENSURE_SUCCESS(rv, rv);

  // Construct the parent path of the passed in path
  nsCOMPtr<nsIFile> passedInFile = do_CreateInstance("@mozilla.org/file/local;1");
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

  wchar_t buf[MAX_PATH];

  // Path
  hres = pLink->GetPath(buf, MAX_PATH, nullptr, SLGP_UNCPRIORITY);
  if (FAILED(hres))
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIFile> file;
  nsDependentString filepath(buf);
  rv = NS_NewLocalFile(filepath, false, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = handlerApp->SetExecutable(file);
  NS_ENSURE_SUCCESS(rv, rv);

  // Parameters
  hres = pLink->GetArguments(buf, MAX_PATH);
  if (SUCCEEDED(hres)) {
    LPWSTR *arglist;
    int32_t numArgs;
    int32_t idx;

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
  hres = pLink->GetIconLocation(buf, MAX_PATH, &iconIdx);
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
nsresult JumpListLink::GetShellItem(nsCOMPtr<nsIJumpListItem>& item, RefPtr<IShellItem2>& aShellItem)
{
  IShellItem2 *psi = nullptr;
  nsresult rv;

  int16_t type; 
  if (NS_FAILED(item->GetType(&type)))
    return NS_ERROR_INVALID_ARG;

  if (type != nsIJumpListItem::JUMPLIST_ITEM_LINK)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIJumpListLink> link = do_QueryInterface(item, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  rv = link->GetUri(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the IShellItem
  if (FAILED(WinUtils::SHCreateItemFromParsingName(
               NS_ConvertASCIItoUTF16(spec).get(),
               nullptr, IID_PPV_ARGS(&psi)))) {
    return NS_ERROR_INVALID_ARG;
  }

  // Set the title
  nsAutoString linkTitle;
  link->GetUriTitle(linkTitle);

  IPropertyStore* pPropStore = nullptr;
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
  LPWSTR lpstrName = nullptr;

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

} // namespace widget
} // namespace mozilla

