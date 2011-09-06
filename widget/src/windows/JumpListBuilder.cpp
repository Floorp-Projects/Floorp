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

#include "JumpListBuilder.h"

#include "nsError.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsArrayUtils.h"
#include "nsIMutableArray.h"
#include "nsWidgetsCID.h"
#include "WinTaskbar.h"
#include "nsDirectoryServiceUtils.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace widget {

static NS_DEFINE_CID(kJumpListItemCID,     NS_WIN_JUMPLISTITEM_CID);
static NS_DEFINE_CID(kJumpListLinkCID,     NS_WIN_JUMPLISTLINK_CID);
static NS_DEFINE_CID(kJumpListShortcutCID, NS_WIN_JUMPLISTSHORTCUT_CID);

// defined in WinTaskbar.cpp
extern const wchar_t *gMozillaJumpListIDGeneric;

PRPackedBool JumpListBuilder::sBuildingList = PR_FALSE;
const char kPrefTaskbarEnabled[] = "browser.taskbar.lists.enabled";

NS_IMPL_ISUPPORTS2(JumpListBuilder, nsIJumpListBuilder, nsIObserver)

JumpListBuilder::JumpListBuilder() :
  mMaxItems(0),
  mHasCommit(PR_FALSE)
{
  ::CoInitialize(NULL);
  
  CoCreateInstance(CLSID_DestinationList, NULL, CLSCTX_INPROC_SERVER,
                   IID_ICustomDestinationList, getter_AddRefs(mJumpListMgr));

  Preferences::AddStrongObserver(this, kPrefTaskbarEnabled);
}

JumpListBuilder::~JumpListBuilder()
{
  Preferences::RemoveObserver(this, kPrefTaskbarEnabled);
  mJumpListMgr = nsnull;
  ::CoUninitialize();
}

/* readonly attribute short available; */
NS_IMETHODIMP JumpListBuilder::GetAvailable(PRInt16 *aAvailable)
{
  *aAvailable = PR_FALSE;

  if (mJumpListMgr)
    *aAvailable = PR_TRUE;

  return NS_OK;
}

/* readonly attribute boolean isListCommitted; */
NS_IMETHODIMP JumpListBuilder::GetIsListCommitted(PRBool *aCommit)
{
  *aCommit = mHasCommit;

  return NS_OK;
}

/* readonly attribute short maxItems; */
NS_IMETHODIMP JumpListBuilder::GetMaxListItems(PRInt16 *aMaxItems)
{
  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  *aMaxItems = 0;

  if (sBuildingList) {
    *aMaxItems = mMaxItems;
    return NS_OK;
  }

  IObjectArray *objArray;
  if (SUCCEEDED(mJumpListMgr->BeginList(&mMaxItems, IID_PPV_ARGS(&objArray)))) {
    *aMaxItems = mMaxItems;

    if (objArray)
      objArray->Release();

    mJumpListMgr->AbortList();
  }

  return NS_OK;
}

/* boolean initListBuild(in nsIMutableArray removedItems); */
NS_IMETHODIMP JumpListBuilder::InitListBuild(nsIMutableArray *removedItems, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(removedItems);

  *_retval = PR_FALSE;

  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  if(sBuildingList)
    AbortListBuild();

  IObjectArray *objArray;

  if (SUCCEEDED(mJumpListMgr->BeginList(&mMaxItems, IID_PPV_ARGS(&objArray)))) {
    if (objArray) {
      TransferIObjectArrayToIMutableArray(objArray, removedItems);
      objArray->Release();
    }

    RemoveIconCacheForItems(removedItems);

    sBuildingList = PR_TRUE;
    *_retval = PR_TRUE;
    return NS_OK;
  }

  return NS_OK;
}

// Ensures that we don't have old ICO files that aren't in our jump lists 
// anymore left over in the cache.
nsresult JumpListBuilder::RemoveIconCacheForItems(nsIMutableArray *items) 
{
  NS_ENSURE_ARG_POINTER(items);
  
  nsresult rv;
  PRUint32 length;
  items->GetLength(&length);
  for (PRUint32 i = 0; i < length; ++i) {

    //Obtain an IJumpListItem and get the type
    nsCOMPtr<nsIJumpListItem> item = do_QueryElementAt(items, i);
    if (!item) {
      continue;
    }
    PRInt16 type;
    if (NS_FAILED(item->GetType(&type))) {
      continue;
    }

    // If the item is a shortcut, remove its associated icon if any
    if (type == nsIJumpListItem::JUMPLIST_ITEM_SHORTCUT) {
      nsCOMPtr<nsIJumpListShortcut> shortcut = do_QueryInterface(item);
      if (shortcut) {
        nsCOMPtr<nsIURI> uri;
        rv = shortcut->GetIconImageUri(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv) && uri) {
          JumpListShortcut::RemoveCacheIcon(uri); // doesn't matter if it fails

          // The shortcut was generated from an IShellLinkW so IShellLinkW can
          // only tell us what the original icon is and not the URI.
          // So this field was used only temporarily as the actual icon file
          // path.  It should be cleared.
          shortcut->SetIconImageUri(nsnull);
        }
      }
    }

  } // end for
}

// Ensures that we have no old ICO files left in the jump list cache
nsresult JumpListBuilder::RemoveIconCacheForAllItems() 
{
  // Construct the path of our jump list cache
  nsCOMPtr<nsIFile> jumpListCacheDir;
  nsresult rv = NS_GetSpecialDirectory("ProfLDS", 
                                       getter_AddRefs(jumpListCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = jumpListCacheDir->AppendNative(nsDependentCString(JumpListItem::kJumpListCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = jumpListCacheDir->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Loop through each directory entry and remove all ICO files found
  do {
    PRBool hasMore = PR_FALSE;
    if (NS_FAILED(entries->HasMoreElements(&hasMore)) || !hasMore)
      break;

    nsCOMPtr<nsISupports> supp;
    if (NS_FAILED(entries->GetNext(getter_AddRefs(supp))))
      break;

    nsCOMPtr<nsIFile> currFile(do_QueryInterface(supp));
    nsAutoString path;
    if (NS_FAILED(currFile->GetPath(path)))
      continue;

    PRInt32 len = path.Length();
    if (StringTail(path, 4).LowerCaseEqualsASCII(".ico")) {
      // Check if the cached ICO file exists
      PRBool exists;
      if (NS_FAILED(currFile->Exists(&exists)) || !exists)
        continue;

      // We found an ICO file that exists, so we should remove it
      currFile->Remove(PR_FALSE);
    }
  } while(true);

  return NS_OK;
}

/* boolean addListToBuild(in short aCatType, [optional] in nsIArray items, [optional] in AString catName); */
NS_IMETHODIMP JumpListBuilder::AddListToBuild(PRInt16 aCatType, nsIArray *items, const nsAString &catName, PRBool *_retval)
{
  nsresult rv;

  *_retval = PR_FALSE;

  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  switch(aCatType) {
    case nsIJumpListBuilder::JUMPLIST_CATEGORY_TASKS:
    {
      NS_ENSURE_ARG_POINTER(items);

      HRESULT hr;
      nsRefPtr<IObjectCollection> collection;
      hr = CoCreateInstance(CLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC_SERVER,
                            IID_IObjectCollection, getter_AddRefs(collection));
      if (FAILED(hr))
        return NS_ERROR_UNEXPECTED;

      // Build the list
      PRUint32 length;
      items->GetLength(&length);
      for (PRUint32 i = 0; i < length; ++i) {
        nsCOMPtr<nsIJumpListItem> item = do_QueryElementAt(items, i);
        if (!item)
          continue;
        // Check for separators 
        if (IsSeparator(item)) {
          nsRefPtr<IShellLinkW> link;
          rv = JumpListSeparator::GetSeparator(link);
          if (NS_FAILED(rv))
            return rv;
          collection->AddObject(link);
          continue;
        }
        // These should all be ShellLinks
        nsRefPtr<IShellLinkW> link;
        rv = JumpListShortcut::GetShellLink(item, link);
        if (NS_FAILED(rv))
          return rv;
        collection->AddObject(link);
      }

      // We need IObjectArray to submit
      nsRefPtr<IObjectArray> pArray;
      hr = collection->QueryInterface(IID_IObjectArray, getter_AddRefs(pArray));
      if (FAILED(hr))
        return NS_ERROR_UNEXPECTED;

      // Add the tasks
      hr = mJumpListMgr->AddUserTasks(pArray);
      if (SUCCEEDED(hr))
        *_retval = PR_TRUE;
      return NS_OK;
    }
    break;
    case nsIJumpListBuilder::JUMPLIST_CATEGORY_RECENT:
    {
      if (SUCCEEDED(mJumpListMgr->AppendKnownCategory(KDC_RECENT)))
        *_retval = PR_TRUE;
      return NS_OK;
    }
    break;
    case nsIJumpListBuilder::JUMPLIST_CATEGORY_FREQUENT:
    {
      if (SUCCEEDED(mJumpListMgr->AppendKnownCategory(KDC_FREQUENT)))
        *_retval = PR_TRUE;
      return NS_OK;
    }
    break;
    case nsIJumpListBuilder::JUMPLIST_CATEGORY_CUSTOMLIST:
    {
      NS_ENSURE_ARG_POINTER(items);

      if (catName.IsEmpty())
        return NS_ERROR_INVALID_ARG;

      HRESULT hr;
      nsRefPtr<IObjectCollection> collection;
      hr = CoCreateInstance(CLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC_SERVER,
                            IID_IObjectCollection, getter_AddRefs(collection));
      if (FAILED(hr))
        return NS_ERROR_UNEXPECTED;

      PRUint32 length;
      items->GetLength(&length);
      for (PRUint32 i = 0; i < length; ++i) {
        nsCOMPtr<nsIJumpListItem> item = do_QueryElementAt(items, i);
        if (!item)
          continue;
        PRInt16 type;
        if (NS_FAILED(item->GetType(&type)))
          continue;
        switch(type) {
          case nsIJumpListItem::JUMPLIST_ITEM_SEPARATOR:
          {
            nsRefPtr<IShellLinkW> shellItem;
            rv = JumpListSeparator::GetSeparator(shellItem);
            if (NS_FAILED(rv))
              return rv;
            collection->AddObject(shellItem);
          }
          break;
          case nsIJumpListItem::JUMPLIST_ITEM_LINK:
          {
            nsRefPtr<IShellItem2> shellItem;
            rv = JumpListLink::GetShellItem(item, shellItem);
            if (NS_FAILED(rv))
              return rv;
            collection->AddObject(shellItem);
          }
          break;
          case nsIJumpListItem::JUMPLIST_ITEM_SHORTCUT:
          {
            nsRefPtr<IShellLinkW> shellItem;
            rv = JumpListShortcut::GetShellLink(item, shellItem);
            if (NS_FAILED(rv))
              return rv;
            collection->AddObject(shellItem);
          }
          break;
        }
      }

      // We need IObjectArray to submit
      nsRefPtr<IObjectArray> pArray;
      hr = collection->QueryInterface(IID_IObjectArray, (LPVOID*)&pArray);
      if (FAILED(hr))
        return NS_ERROR_UNEXPECTED;

      // Add the tasks
      hr = mJumpListMgr->AppendCategory(catName.BeginReading(), pArray);
      if (SUCCEEDED(hr))
        *_retval = PR_TRUE;
      return NS_OK;
    }
    break;
  }
  return NS_OK;
}

/* void abortListBuild(); */
NS_IMETHODIMP JumpListBuilder::AbortListBuild()
{
  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  mJumpListMgr->AbortList();
  sBuildingList = PR_FALSE;

  return NS_OK;
}

/* boolean commitListBuild(); */
NS_IMETHODIMP JumpListBuilder::CommitListBuild(PRBool *_retval)
{
  *_retval = PR_FALSE;

  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  HRESULT hr = mJumpListMgr->CommitList();
  sBuildingList = PR_FALSE;

  // XXX We might want some specific error data here.
  if (SUCCEEDED(hr)) {
    *_retval = PR_TRUE;
    mHasCommit = PR_TRUE;
  }

  return NS_OK;
}

/* boolean deleteActiveList(); */
NS_IMETHODIMP JumpListBuilder::DeleteActiveList(PRBool *_retval)
{
  *_retval = PR_FALSE;

  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  if(sBuildingList)
    AbortListBuild();

  nsAutoString uid;
  if (!WinTaskbar::GetAppUserModelID(uid))
    return NS_OK;

  if (SUCCEEDED(mJumpListMgr->DeleteList(uid.get())))
    *_retval = PR_TRUE;

  return NS_OK;
}

/* internal */

PRBool JumpListBuilder::IsSeparator(nsCOMPtr<nsIJumpListItem>& item)
{
  PRInt16 type;
  item->GetType(&type);
  if (NS_FAILED(item->GetType(&type)))
    return PR_FALSE;
    
  if (type == nsIJumpListItem::JUMPLIST_ITEM_SEPARATOR)
    return PR_TRUE;
  return PR_FALSE;
}

// TransferIObjectArrayToIMutableArray - used in converting removed items
// to our objects.
nsresult JumpListBuilder::TransferIObjectArrayToIMutableArray(IObjectArray *objArray, nsIMutableArray *removedItems)
{
  NS_ENSURE_ARG_POINTER(objArray);
  NS_ENSURE_ARG_POINTER(removedItems);

  nsresult rv;

  PRUint32 count = 0;
  objArray->GetCount(&count);

  nsCOMPtr<nsIJumpListItem> item;

  for (PRUint32 idx = 0; idx < count; idx++) {
    IShellLinkW * pLink = nsnull;
    IShellItem * pItem = nsnull;

    if (SUCCEEDED(objArray->GetAt(idx, IID_IShellLinkW, (LPVOID*)&pLink))) {
      nsCOMPtr<nsIJumpListShortcut> shortcut = 
        do_CreateInstance(kJumpListShortcutCID, &rv);
      if (NS_FAILED(rv))
        return NS_ERROR_UNEXPECTED;
      rv = JumpListShortcut::GetJumpListShortcut(pLink, shortcut);
      item = do_QueryInterface(shortcut);
    }
    else if (SUCCEEDED(objArray->GetAt(idx, IID_IShellItem, (LPVOID*)&pItem))) {
      nsCOMPtr<nsIJumpListLink> link = 
        do_CreateInstance(kJumpListLinkCID, &rv);
      if (NS_FAILED(rv))
        return NS_ERROR_UNEXPECTED;
      rv = JumpListLink::GetJumpListLink(pItem, link);
      item = do_QueryInterface(link);
    }

    if (pLink)
      pLink->Release();
    if (pItem)
      pItem->Release();

    if (NS_SUCCEEDED(rv)) {
      removedItems->AppendElement(item, PR_FALSE);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP JumpListBuilder::Observe(nsISupports* aSubject,
                                        const char* aTopic,
                                        const PRUnichar* aData)
{
  if (nsDependentString(aData).EqualsASCII(kPrefTaskbarEnabled)) {
    PRBool enabled = Preferences::GetBool(kPrefTaskbarEnabled, true);
    if (!enabled) {
      RemoveIconCacheForAllItems();
    }
  }
  return NS_OK;
}

} // namespace widget
} // namespace mozilla

#endif // MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
