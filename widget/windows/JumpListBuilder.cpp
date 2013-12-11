/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsStringStream.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "mozilla/LazyIdleThread.h"

#include "WinUtils.h"

// The amount of time, in milliseconds, that our IO thread will stay alive after the last event it processes.
#define DEFAULT_THREAD_TIMEOUT_MS 30000

namespace mozilla {
namespace widget {

static NS_DEFINE_CID(kJumpListItemCID,     NS_WIN_JUMPLISTITEM_CID);
static NS_DEFINE_CID(kJumpListLinkCID,     NS_WIN_JUMPLISTLINK_CID);
static NS_DEFINE_CID(kJumpListShortcutCID, NS_WIN_JUMPLISTSHORTCUT_CID);

// defined in WinTaskbar.cpp
extern const wchar_t *gMozillaJumpListIDGeneric;

bool JumpListBuilder::sBuildingList = false;
const char kPrefTaskbarEnabled[] = "browser.taskbar.lists.enabled";

NS_IMPL_ISUPPORTS2(JumpListBuilder, nsIJumpListBuilder, nsIObserver)

JumpListBuilder::JumpListBuilder() :
  mMaxItems(0),
  mHasCommit(false)
{
  ::CoInitialize(nullptr);
  
  CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER,
                   IID_ICustomDestinationList, getter_AddRefs(mJumpListMgr));

  // Make a lazy thread for any IO
  mIOThread = new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS,
                                 NS_LITERAL_CSTRING("Jump List"),
                                 LazyIdleThread::ManualShutdown);
  Preferences::AddStrongObserver(this, kPrefTaskbarEnabled);
}

JumpListBuilder::~JumpListBuilder()
{
  mIOThread->Shutdown();
  Preferences::RemoveObserver(this, kPrefTaskbarEnabled);
  mJumpListMgr = nullptr;
  ::CoUninitialize();
}

/* readonly attribute short available; */
NS_IMETHODIMP JumpListBuilder::GetAvailable(int16_t *aAvailable)
{
  *aAvailable = false;

  if (mJumpListMgr)
    *aAvailable = true;

  return NS_OK;
}

/* readonly attribute boolean isListCommitted; */
NS_IMETHODIMP JumpListBuilder::GetIsListCommitted(bool *aCommit)
{
  *aCommit = mHasCommit;

  return NS_OK;
}

/* readonly attribute short maxItems; */
NS_IMETHODIMP JumpListBuilder::GetMaxListItems(int16_t *aMaxItems)
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
NS_IMETHODIMP JumpListBuilder::InitListBuild(nsIMutableArray *removedItems, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(removedItems);

  *_retval = false;

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

    sBuildingList = true;
    *_retval = true;
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
  uint32_t length;
  items->GetLength(&length);
  for (uint32_t i = 0; i < length; ++i) {

    //Obtain an IJumpListItem and get the type
    nsCOMPtr<nsIJumpListItem> item = do_QueryElementAt(items, i);
    if (!item) {
      continue;
    }
    int16_t type;
    if (NS_FAILED(item->GetType(&type))) {
      continue;
    }

    // If the item is a shortcut, remove its associated icon if any
    if (type == nsIJumpListItem::JUMPLIST_ITEM_SHORTCUT) {
      nsCOMPtr<nsIJumpListShortcut> shortcut = do_QueryInterface(item);
      if (shortcut) {
        nsCOMPtr<nsIURI> uri;
        rv = shortcut->GetFaviconPageUri(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv) && uri) {
          
          // The local file path is stored inside the nsIURI
          // Get the nsIURI spec which stores the local path for the icon to remove
          nsAutoCString spec;
          nsresult rv = uri->GetSpec(spec);
          NS_ENSURE_SUCCESS(rv, rv);

          nsCOMPtr<nsIRunnable> event 
            = new mozilla::widget::AsyncDeleteIconFromDisk(NS_ConvertUTF8toUTF16(spec));
          mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);

          // The shortcut was generated from an IShellLinkW so IShellLinkW can
          // only tell us what the original icon is and not the URI.
          // So this field was used only temporarily as the actual icon file
          // path.  It should be cleared.
          shortcut->SetFaviconPageUri(nullptr);
        }
      }
    }

  } // end for

  return NS_OK;
}

// Ensures that we have no old ICO files left in the jump list cache
nsresult JumpListBuilder::RemoveIconCacheForAllItems() 
{
  // Construct the path of our jump list cache
  nsCOMPtr<nsIFile> jumpListCacheDir;
  nsresult rv = NS_GetSpecialDirectory("ProfLDS", 
                                       getter_AddRefs(jumpListCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = jumpListCacheDir->AppendNative(nsDependentCString(
                         mozilla::widget::FaviconHelper::kJumpListCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = jumpListCacheDir->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Loop through each directory entry and remove all ICO files found
  do {
    bool hasMore = false;
    if (NS_FAILED(entries->HasMoreElements(&hasMore)) || !hasMore)
      break;

    nsCOMPtr<nsISupports> supp;
    if (NS_FAILED(entries->GetNext(getter_AddRefs(supp))))
      break;

    nsCOMPtr<nsIFile> currFile(do_QueryInterface(supp));
    nsAutoString path;
    if (NS_FAILED(currFile->GetPath(path)))
      continue;

    int32_t len = path.Length();
    if (StringTail(path, 4).LowerCaseEqualsASCII(".ico")) {
      // Check if the cached ICO file exists
      bool exists;
      if (NS_FAILED(currFile->Exists(&exists)) || !exists)
        continue;

      // We found an ICO file that exists, so we should remove it
      currFile->Remove(false);
    }
  } while(true);

  return NS_OK;
}

/* boolean addListToBuild(in short aCatType, [optional] in nsIArray items, [optional] in AString catName); */
NS_IMETHODIMP JumpListBuilder::AddListToBuild(int16_t aCatType, nsIArray *items, const nsAString &catName, bool *_retval)
{
  nsresult rv;

  *_retval = false;

  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  switch(aCatType) {
    case nsIJumpListBuilder::JUMPLIST_CATEGORY_TASKS:
    {
      NS_ENSURE_ARG_POINTER(items);

      HRESULT hr;
      nsRefPtr<IObjectCollection> collection;
      hr = CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr,
                            CLSCTX_INPROC_SERVER, IID_IObjectCollection,
                            getter_AddRefs(collection));
      if (FAILED(hr))
        return NS_ERROR_UNEXPECTED;

      // Build the list
      uint32_t length;
      items->GetLength(&length);
      for (uint32_t i = 0; i < length; ++i) {
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
        rv = JumpListShortcut::GetShellLink(item, link, mIOThread);
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
        *_retval = true;
      return NS_OK;
    }
    break;
    case nsIJumpListBuilder::JUMPLIST_CATEGORY_RECENT:
    {
      if (SUCCEEDED(mJumpListMgr->AppendKnownCategory(KDC_RECENT)))
        *_retval = true;
      return NS_OK;
    }
    break;
    case nsIJumpListBuilder::JUMPLIST_CATEGORY_FREQUENT:
    {
      if (SUCCEEDED(mJumpListMgr->AppendKnownCategory(KDC_FREQUENT)))
        *_retval = true;
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
      hr = CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr,
                            CLSCTX_INPROC_SERVER, IID_IObjectCollection,
                            getter_AddRefs(collection));
      if (FAILED(hr))
        return NS_ERROR_UNEXPECTED;

      uint32_t length;
      items->GetLength(&length);
      for (uint32_t i = 0; i < length; ++i) {
        nsCOMPtr<nsIJumpListItem> item = do_QueryElementAt(items, i);
        if (!item)
          continue;
        int16_t type;
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
            rv = JumpListShortcut::GetShellLink(item, shellItem, mIOThread);
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
      hr = mJumpListMgr->AppendCategory(reinterpret_cast<const wchar_t*>(catName.BeginReading()), pArray);
      if (SUCCEEDED(hr))
        *_retval = true;
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
  sBuildingList = false;

  return NS_OK;
}

/* boolean commitListBuild(); */
NS_IMETHODIMP JumpListBuilder::CommitListBuild(bool *_retval)
{
  *_retval = false;

  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  HRESULT hr = mJumpListMgr->CommitList();
  sBuildingList = false;

  // XXX We might want some specific error data here.
  if (SUCCEEDED(hr)) {
    *_retval = true;
    mHasCommit = true;
  }

  return NS_OK;
}

/* boolean deleteActiveList(); */
NS_IMETHODIMP JumpListBuilder::DeleteActiveList(bool *_retval)
{
  *_retval = false;

  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  if(sBuildingList)
    AbortListBuild();

  nsAutoString uid;
  if (!WinTaskbar::GetAppUserModelID(uid))
    return NS_OK;

  if (SUCCEEDED(mJumpListMgr->DeleteList(uid.get())))
    *_retval = true;

  return NS_OK;
}

/* internal */

bool JumpListBuilder::IsSeparator(nsCOMPtr<nsIJumpListItem>& item)
{
  int16_t type;
  item->GetType(&type);
  if (NS_FAILED(item->GetType(&type)))
    return false;
    
  if (type == nsIJumpListItem::JUMPLIST_ITEM_SEPARATOR)
    return true;
  return false;
}

// TransferIObjectArrayToIMutableArray - used in converting removed items
// to our objects.
nsresult JumpListBuilder::TransferIObjectArrayToIMutableArray(IObjectArray *objArray, nsIMutableArray *removedItems)
{
  NS_ENSURE_ARG_POINTER(objArray);
  NS_ENSURE_ARG_POINTER(removedItems);

  nsresult rv;

  uint32_t count = 0;
  objArray->GetCount(&count);

  nsCOMPtr<nsIJumpListItem> item;

  for (uint32_t idx = 0; idx < count; idx++) {
    IShellLinkW * pLink = nullptr;
    IShellItem * pItem = nullptr;

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
      removedItems->AppendElement(item, false);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP JumpListBuilder::Observe(nsISupports* aSubject,
                                        const char* aTopic,
                                        const PRUnichar* aData)
{
  if (nsDependentString(aData).EqualsASCII(kPrefTaskbarEnabled)) {
    bool enabled = Preferences::GetBool(kPrefTaskbarEnabled, true);
    if (!enabled) {
      
      nsCOMPtr<nsIRunnable> event = 
        new mozilla::widget::AsyncDeleteAllFaviconsFromDisk();
      mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
    }
  }
  return NS_OK;
}

} // namespace widget
} // namespace mozilla

