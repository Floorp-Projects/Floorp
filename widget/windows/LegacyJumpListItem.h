/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __LegacyJumpListItem_h__
#define __LegacyJumpListItem_h__

#include <windows.h>
#include <shobjidl.h>
#undef LogSeverity  // SetupAPI.h #defines this as DWORD

#include "mozilla/RefPtr.h"
#include "mozilla/LazyIdleThread.h"
#include "nsILegacyJumpListItem.h"  // defines nsILegacyJumpListItem
#include "nsIMIMEInfo.h"            // defines nsILocalHandlerApp
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsICryptoHash.h"
#include "nsString.h"
#include "nsCycleCollectionParticipant.h"

class nsIThread;

namespace mozilla {
namespace widget {

class LegacyJumpListItemBase : public nsILegacyJumpListItem {
 public:
  LegacyJumpListItemBase()
      : mItemType(nsILegacyJumpListItem::JUMPLIST_ITEM_EMPTY) {}

  explicit LegacyJumpListItemBase(int32_t type) : mItemType(type) {}

  NS_DECL_NSILEGACYJUMPLISTITEM

  static const char kJumpListCacheDir[];

 protected:
  virtual ~LegacyJumpListItemBase() {}

  short Type() { return mItemType; }
  short mItemType;
};

class LegacyJumpListItem : public LegacyJumpListItemBase {
  ~LegacyJumpListItem() {}

 public:
  using LegacyJumpListItemBase::LegacyJumpListItemBase;

  NS_DECL_ISUPPORTS
};

class LegacyJumpListSeparator : public LegacyJumpListItemBase,
                                public nsILegacyJumpListSeparator {
  ~LegacyJumpListSeparator() {}

 public:
  LegacyJumpListSeparator()
      : LegacyJumpListItemBase(nsILegacyJumpListItem::JUMPLIST_ITEM_SEPARATOR) {
  }

  NS_DECL_ISUPPORTS
  NS_FORWARD_NSILEGACYJUMPLISTITEM(LegacyJumpListItemBase::)

  static nsresult GetSeparator(RefPtr<IShellLinkW>& aShellLink);
};

class LegacyJumpListLink : public LegacyJumpListItemBase,
                           public nsILegacyJumpListLink {
  ~LegacyJumpListLink() {}

 public:
  LegacyJumpListLink()
      : LegacyJumpListItemBase(nsILegacyJumpListItem::JUMPLIST_ITEM_LINK) {}

  NS_DECL_ISUPPORTS
  NS_IMETHOD GetType(int16_t* aType) override {
    return LegacyJumpListItemBase::GetType(aType);
  }
  NS_IMETHOD Equals(nsILegacyJumpListItem* item, bool* _retval) override;
  NS_DECL_NSILEGACYJUMPLISTLINK

  static nsresult GetShellItem(nsCOMPtr<nsILegacyJumpListItem>& item,
                               RefPtr<IShellItem2>& aShellItem);
  static nsresult GetJumpListLink(IShellItem* pItem,
                                  nsCOMPtr<nsILegacyJumpListLink>& aLink);

 protected:
  nsString mUriTitle;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsICryptoHash> mCryptoHash;
};

class LegacyJumpListShortcut : public LegacyJumpListItemBase,
                               public nsILegacyJumpListShortcut {
  ~LegacyJumpListShortcut() {}

 public:
  LegacyJumpListShortcut()
      : LegacyJumpListItemBase(nsILegacyJumpListItem::JUMPLIST_ITEM_SHORTCUT) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(LegacyJumpListShortcut,
                                           LegacyJumpListItemBase)
  NS_IMETHOD GetType(int16_t* aType) override {
    return LegacyJumpListItemBase::GetType(aType);
  }
  NS_IMETHOD Equals(nsILegacyJumpListItem* item, bool* _retval) override;
  NS_DECL_NSILEGACYJUMPLISTSHORTCUT

  static nsresult GetShellLink(nsCOMPtr<nsILegacyJumpListItem>& item,
                               RefPtr<IShellLinkW>& aShellLink,
                               RefPtr<LazyIdleThread>& aIOThread);
  static nsresult GetJumpListShortcut(
      IShellLinkW* pLink, nsCOMPtr<nsILegacyJumpListShortcut>& aShortcut);
  static nsresult GetOutputIconPath(nsCOMPtr<nsIURI> aFaviconPageURI,
                                    nsCOMPtr<nsIFile>& aICOFile);

 protected:
  int32_t mIconIndex;
  nsCOMPtr<nsIURI> mFaviconPageURI;
  nsCOMPtr<nsILocalHandlerApp> mHandlerApp;

  bool ExecutableExists(nsCOMPtr<nsILocalHandlerApp>& handlerApp);
};

}  // namespace widget
}  // namespace mozilla

#endif /* __LegacyJumpListItem_h__ */
