/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __JumpListItem_h__
#define __JumpListItem_h__

#include <windows.h>
#include <shobjidl.h>
#undef LogSeverity // SetupAPI.h #defines this as DWORD

#include "mozilla/RefPtr.h"
#include "nsIJumpListItem.h"  // defines nsIJumpListItem
#include "nsIMIMEInfo.h" // defines nsILocalHandlerApp
#include "nsTArray.h"
#include "nsIMutableArray.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsICryptoHash.h"
#include "nsString.h"
#include "nsCycleCollectionParticipant.h"

class nsIThread;

namespace mozilla {
namespace widget {

class JumpListItemBase : public nsIJumpListItem
{
public:
  JumpListItemBase() :
   mItemType(nsIJumpListItem::JUMPLIST_ITEM_EMPTY)
  {}

  explicit JumpListItemBase(int32_t type) :
   mItemType(type)
  {}

  NS_DECL_NSIJUMPLISTITEM

  static const char kJumpListCacheDir[];

protected:
  virtual ~JumpListItemBase()
  {}

  short Type() { return mItemType; }
  short mItemType;

};

class JumpListItem : public JumpListItemBase
{
  ~JumpListItem() {}

public:
  using JumpListItemBase::JumpListItemBase;

  NS_DECL_ISUPPORTS
};

class JumpListSeparator : public JumpListItemBase, public nsIJumpListSeparator
{
  ~JumpListSeparator() {}

public:
  JumpListSeparator() :
   JumpListItemBase(nsIJumpListItem::JUMPLIST_ITEM_SEPARATOR)
  {}

  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIJUMPLISTITEM(JumpListItemBase::)

  static nsresult GetSeparator(RefPtr<IShellLinkW>& aShellLink);
};

class JumpListLink : public JumpListItemBase, public nsIJumpListLink
{
  ~JumpListLink() {}

public:
  JumpListLink() :
   JumpListItemBase(nsIJumpListItem::JUMPLIST_ITEM_LINK)
  {}

  NS_DECL_ISUPPORTS
  NS_IMETHOD GetType(int16_t *aType) override { return JumpListItemBase::GetType(aType); }
  NS_IMETHOD Equals(nsIJumpListItem *item, bool *_retval) override;
  NS_DECL_NSIJUMPLISTLINK

  static nsresult GetShellItem(nsCOMPtr<nsIJumpListItem>& item, RefPtr<IShellItem2>& aShellItem);
  static nsresult GetJumpListLink(IShellItem *pItem, nsCOMPtr<nsIJumpListLink>& aLink);

protected:
  nsString mUriTitle;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsICryptoHash> mCryptoHash;
};

class JumpListShortcut : public JumpListItemBase, public nsIJumpListShortcut
{
  ~JumpListShortcut() {}

public:
  JumpListShortcut() :
   JumpListItemBase(nsIJumpListItem::JUMPLIST_ITEM_SHORTCUT)
  {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(JumpListShortcut, JumpListItemBase)
  NS_IMETHOD GetType(int16_t *aType) override { return JumpListItemBase::GetType(aType); }
  NS_IMETHOD Equals(nsIJumpListItem *item, bool *_retval) override;
  NS_DECL_NSIJUMPLISTSHORTCUT

  static nsresult GetShellLink(nsCOMPtr<nsIJumpListItem>& item,
                               RefPtr<IShellLinkW>& aShellLink,
                               nsCOMPtr<nsIThread> &aIOThread);
  static nsresult GetJumpListShortcut(IShellLinkW *pLink, nsCOMPtr<nsIJumpListShortcut>& aShortcut);
  static nsresult GetOutputIconPath(nsCOMPtr<nsIURI> aFaviconPageURI,
                                    nsCOMPtr<nsIFile> &aICOFile);

protected:
  int32_t mIconIndex;
  nsCOMPtr<nsIURI> mFaviconPageURI;
  nsCOMPtr<nsILocalHandlerApp> mHandlerApp;

  bool ExecutableExists(nsCOMPtr<nsILocalHandlerApp>& handlerApp);
  static nsresult ObtainCachedIconFile(nsCOMPtr<nsIURI> aFaviconPageURI,
                                       nsString &aICOFilePath,
                                       nsCOMPtr<nsIThread> &aIOThread);
  static nsresult CacheIconFileFromFaviconURIAsync(nsCOMPtr<nsIURI> aFaviconPageURI,
                                                   nsCOMPtr<nsIFile> aICOFile,
                                                   nsCOMPtr<nsIThread> &aIOThread);
};

} // namespace widget
} // namespace mozilla

#endif /* __JumpListItem_h__ */
