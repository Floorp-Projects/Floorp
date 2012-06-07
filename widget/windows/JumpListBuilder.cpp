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
#include "imgIContainer.h"
#include "imgITools.h"
#include "nsStringStream.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "mozilla/LazyIdleThread.h"

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
NS_IMPL_ISUPPORTS1(AsyncFaviconDataReady, nsIFaviconDataCallback)
NS_IMPL_THREADSAFE_ISUPPORTS1(AsyncWriteIconToDisk, nsIRunnable)
NS_IMPL_THREADSAFE_ISUPPORTS1(AsyncDeleteIconFromDisk, nsIRunnable)
NS_IMPL_THREADSAFE_ISUPPORTS1(AsyncDeleteAllFaviconsFromDisk, nsIRunnable)

JumpListBuilder::JumpListBuilder() :
  mMaxItems(0),
  mHasCommit(false)
{
  ::CoInitialize(NULL);
  
  CoCreateInstance(CLSID_DestinationList, NULL, CLSCTX_INPROC_SERVER,
                   IID_ICustomDestinationList, getter_AddRefs(mJumpListMgr));

  // Make a lazy thread for any IO
  mIOThread = new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS, LazyIdleThread::ManualShutdown);
  Preferences::AddStrongObserver(this, kPrefTaskbarEnabled);
}

JumpListBuilder::~JumpListBuilder()
{
  mIOThread->Shutdown();
  Preferences::RemoveObserver(this, kPrefTaskbarEnabled);
  mJumpListMgr = nsnull;
  ::CoUninitialize();
}

/* readonly attribute short available; */
NS_IMETHODIMP JumpListBuilder::GetAvailable(PRInt16 *aAvailable)
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
        rv = shortcut->GetFaviconPageUri(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv) && uri) {
          
          // The local file path is stored inside the nsIURI
          // Get the nsIURI spec which stores the local path for the icon to remove
          nsCAutoString spec;
          nsresult rv = uri->GetSpec(spec);
          NS_ENSURE_SUCCESS(rv, rv);

          nsCOMPtr<nsIRunnable> event 
            = new AsyncDeleteIconFromDisk(NS_ConvertUTF8toUTF16(spec));
          mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);

          // The shortcut was generated from an IShellLinkW so IShellLinkW can
          // only tell us what the original icon is and not the URI.
          // So this field was used only temporarily as the actual icon file
          // path.  It should be cleared.
          shortcut->SetFaviconPageUri(nsnull);
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
  rv = jumpListCacheDir->AppendNative(nsDependentCString(JumpListItem::kJumpListCacheDir));
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

    PRInt32 len = path.Length();
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
NS_IMETHODIMP JumpListBuilder::AddListToBuild(PRInt16 aCatType, nsIArray *items, const nsAString &catName, bool *_retval)
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
      hr = mJumpListMgr->AppendCategory(catName.BeginReading(), pArray);
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
  PRInt16 type;
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
      
      nsCOMPtr<nsIRunnable> event = new AsyncDeleteAllFaviconsFromDisk();
      mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
    }
  }
  return NS_OK;
}


AsyncFaviconDataReady::AsyncFaviconDataReady(nsIURI *aNewURI, 
                                             nsCOMPtr<nsIThread> &aIOThread) 
                      : mNewURI(aNewURI), 
                        mIOThread(aIOThread)
{
}

NS_IMETHODIMP
AsyncFaviconDataReady::OnComplete(nsIURI *aFaviconURI,
                                  PRUint32 aDataLen,
                                  const PRUint8 *aData, 
                                  const nsACString &aMimeType)
{
  if (!aDataLen || !aData) {
    return NS_OK;
  }

  nsCOMPtr<nsIFile> icoFile;
  nsresult rv = JumpListShortcut::GetOutputIconPath(mNewURI, icoFile);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoString path;
  rv = icoFile->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  // Allocate a new buffer that we own and can use out of line in 
  // another thread.  Copy the favicon raw data into it.
  const fallible_t fallible = fallible_t();
  PRUint8 *data = new (fallible) PRUint8[aDataLen];
  if (!data) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  memcpy(data, aData, aDataLen);

  //AsyncWriteIconToDisk takes ownership of the heap allocated buffer.
  nsCOMPtr<nsIRunnable> event = new AsyncWriteIconToDisk(path, aMimeType, 
                                                         data, 
                                                         aDataLen);
  mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);

  return NS_OK;
}

// Warning: AsyncWriteIconToDisk assumes ownership of the aData buffer passed in
AsyncWriteIconToDisk::AsyncWriteIconToDisk(const nsAString &aIconPath,
                                           const nsACString &aMimeTypeOfInputData,
                                           PRUint8 *aBuffer, 
                                           PRUint32 aBufferLength)
                     : mIconPath(aIconPath),
                       mMimeTypeOfInputData(aMimeTypeOfInputData),
                       mBuffer(aBuffer),
                       mBufferLength(aBufferLength)

{
}

NS_IMETHODIMP AsyncWriteIconToDisk::Run()
{
  NS_PRECONDITION(!NS_IsMainThread(), "Should not be called on the main thread.");

  // Convert the obtained favicon data to an input stream
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                                      reinterpret_cast<const char*>(mBuffer.get()), 
                                      mBufferLength,
                                      NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);

  // Decode the image from the format it was returned to us in (probably PNG)
  nsCOMPtr<imgIContainer> container;
  nsCOMPtr<imgITools> imgtool = do_CreateInstance("@mozilla.org/image/tools;1");
  rv = imgtool->DecodeImageData(stream, mMimeTypeOfInputData, 
                                getter_AddRefs(container));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the recommended icon width and height, or if failure to obtain 
  // these settings, fall back to 16x16 ICOs.  These values can be different
  // if the user has a different DPI setting other than 100%.
  // Windows would scale the 16x16 icon themselves, but it's better
  // we let our ICO encoder do it.
  PRInt32 systemIconWidth = GetSystemMetrics(SM_CXSMICON);
  PRInt32 systemIconHeight = GetSystemMetrics(SM_CYSMICON);
  if (systemIconWidth == 0 || systemIconHeight == 0) {
    systemIconWidth = 16;
    systemIconHeight = 16;
  }
  // Scale the image to the needed size and in ICO format
  mMimeTypeOfInputData.AssignLiteral("image/vnd.microsoft.icon");
  nsCOMPtr<nsIInputStream> iconStream;
  rv = imgtool->EncodeScaledImage(container, mMimeTypeOfInputData,
                                  systemIconWidth,
                                  systemIconHeight,
                                  EmptyString(),
                                  getter_AddRefs(iconStream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> icoFile
    = do_CreateInstance("@mozilla.org/file/local;1");
  NS_ENSURE_TRUE(icoFile, NS_ERROR_FAILURE);
  rv = icoFile->InitWithPath(mIconPath);

  // Setup the output stream for the ICO file on disk
  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), icoFile);
  NS_ENSURE_SUCCESS(rv, rv);

  // Obtain the ICO buffer size from the re-encoded ICO stream
  PRUint32 bufSize;
  rv = iconStream->Available(&bufSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // Setup a buffered output stream from the stream object
  // so that we can simply use WriteFrom with the stream object
  nsCOMPtr<nsIOutputStream> bufferedOutputStream;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream),
                                  outputStream, bufSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // Write out the icon stream to disk and make sure we wrote everything
  PRUint32 wrote;
  rv = bufferedOutputStream->WriteFrom(iconStream, bufSize, &wrote);
  NS_ASSERTION(bufSize == wrote, "Icon wrote size should be equal to requested write size");

  // Cleanup
  bufferedOutputStream->Close();
  outputStream->Close();
  return rv;
}

AsyncWriteIconToDisk::~AsyncWriteIconToDisk()
{
}

AsyncDeleteIconFromDisk::AsyncDeleteIconFromDisk(const nsAString &aIconPath)
                        : mIconPath(aIconPath)
{
}

NS_IMETHODIMP AsyncDeleteIconFromDisk::Run()
{
  // Construct the parent path of the passed in path
  nsCOMPtr<nsIFile> icoFile = do_CreateInstance("@mozilla.org/file/local;1");
  NS_ENSURE_TRUE(icoFile, NS_ERROR_FAILURE);
  nsresult rv = icoFile->InitWithPath(mIconPath);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check if the cached ICO file exists
  bool exists;
  rv = icoFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check that we aren't deleting some arbitrary file that is not an icon
  if (StringTail(mIconPath, 4).LowerCaseEqualsASCII(".ico")) {
    // Check if the cached ICO file exists
    bool exists;
    if (NS_FAILED(icoFile->Exists(&exists)) || !exists)
      return NS_ERROR_FAILURE;

    // We found an ICO file that exists, so we should remove it
    icoFile->Remove(false);
  }

  return NS_OK;
}

AsyncDeleteIconFromDisk::~AsyncDeleteIconFromDisk()
{
}

AsyncDeleteAllFaviconsFromDisk::AsyncDeleteAllFaviconsFromDisk()
{
}

NS_IMETHODIMP AsyncDeleteAllFaviconsFromDisk::Run()
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

    PRInt32 len = path.Length();
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

AsyncDeleteAllFaviconsFromDisk::~AsyncDeleteAllFaviconsFromDisk()
{
}


} // namespace widget
} // namespace mozilla

