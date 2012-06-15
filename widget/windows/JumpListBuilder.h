/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __JumpListBuilder_h__
#define __JumpListBuilder_h__

#include <windows.h>

#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN7
// Needed for various com interfaces
#include <shobjidl.h>

#include "nsString.h"
#include "nsIMutableArray.h"

#include "nsIJumpListBuilder.h"
#include "nsIJumpListItem.h"
#include "JumpListItem.h"
#include "nsIObserver.h"
#include "nsIFaviconService.h"
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace widget {

class JumpListBuilder : public nsIJumpListBuilder, 
                        public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIJUMPLISTBUILDER
  NS_DECL_NSIOBSERVER

  JumpListBuilder();
  virtual ~JumpListBuilder();

protected:
  static bool sBuildingList; 

private:
  nsRefPtr<ICustomDestinationList> mJumpListMgr;
  PRUint32 mMaxItems;
  bool mHasCommit;
  nsCOMPtr<nsIThread> mIOThread;

  bool IsSeparator(nsCOMPtr<nsIJumpListItem>& item);
  nsresult TransferIObjectArrayToIMutableArray(IObjectArray *objArray, nsIMutableArray *removedItems);
  nsresult RemoveIconCacheForItems(nsIMutableArray *removedItems);
  nsresult RemoveIconCacheForAllItems();

  friend class WinTaskbar;
};


class AsyncFaviconDataReady MOZ_FINAL : public nsIFaviconDataCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFAVICONDATACALLBACK

  AsyncFaviconDataReady(nsIURI *aNewURI, nsCOMPtr<nsIThread> &aIOThread);
private:
  nsCOMPtr<nsIURI> mNewURI;
  nsCOMPtr<nsIThread> mIOThread;
};

/**
  * Asynchronously tries add the list to the build
  */
class AsyncWriteIconToDisk : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  // Warning: AsyncWriteIconToDisk assumes ownership of the aData buffer passed in
  AsyncWriteIconToDisk(const nsAString &aIconPath,
                       const nsACString &aMimeTypeOfInputData,
                       PRUint8 *aData, 
                       PRUint32 aDataLen);
  virtual ~AsyncWriteIconToDisk();

private:
  nsAutoString mIconPath;
  nsCAutoString mMimeTypeOfInputData;
  nsAutoArrayPtr<PRUint8> mBuffer;
  PRUint32 mBufferLength;
};

class AsyncDeleteIconFromDisk : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  AsyncDeleteIconFromDisk(const nsAString &aIconPath);
  virtual ~AsyncDeleteIconFromDisk();

private:
  nsAutoString mIconPath;
};

class AsyncDeleteAllFaviconsFromDisk : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  AsyncDeleteAllFaviconsFromDisk();
  virtual ~AsyncDeleteAllFaviconsFromDisk();
};

} // namespace widget
} // namespace mozilla

#endif /* __JumpListBuilder_h__ */

