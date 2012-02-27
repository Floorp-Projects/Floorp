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

#ifndef __JumpListItem_h__
#define __JumpListItem_h__

#include <windows.h>
#include <shobjidl.h>

#include "nsIJumpListItem.h"  // defines nsIJumpListItem
#include "nsIMIMEInfo.h" // defines nsILocalHandlerApp
#include "nsTArray.h"
#include "nsIMutableArray.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIURI.h"
#include "nsICryptoHash.h"
#include "nsString.h"
#include "nsCycleCollectionParticipant.h"

class nsIThread;

namespace mozilla {
namespace widget {

class JumpListItem : public nsIJumpListItem
{
public:
  JumpListItem() :
   mItemType(nsIJumpListItem::JUMPLIST_ITEM_EMPTY)
  {}

  JumpListItem(PRInt32 type) :
   mItemType(type)
  {}

  virtual ~JumpListItem() 
  {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIJUMPLISTITEM

  static const char kJumpListCacheDir[];

protected:
  short Type() { return mItemType; }
  short mItemType;

  static nsresult HashURI(nsCOMPtr<nsICryptoHash> &aCryptoHash,
                          nsIURI *aUri, nsACString& aUriHash);
};

class JumpListSeparator : public JumpListItem, public nsIJumpListSeparator
{
public:
  JumpListSeparator() :
   JumpListItem(nsIJumpListItem::JUMPLIST_ITEM_SEPARATOR)
  {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_IMETHOD GetType(PRInt16 *aType) { return JumpListItem::GetType(aType); }
  NS_IMETHOD Equals(nsIJumpListItem *item, bool *_retval) { return JumpListItem::Equals(item, _retval); }

  static nsresult GetSeparator(nsRefPtr<IShellLinkW>& aShellLink);
};

class JumpListLink : public JumpListItem, public nsIJumpListLink
{
public:
  JumpListLink() :
   JumpListItem(nsIJumpListItem::JUMPLIST_ITEM_LINK)
  {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_IMETHOD GetType(PRInt16 *aType) { return JumpListItem::GetType(aType); }
  NS_IMETHOD Equals(nsIJumpListItem *item, bool *_retval);
  NS_DECL_NSIJUMPLISTLINK

  static nsresult GetShellItem(nsCOMPtr<nsIJumpListItem>& item, nsRefPtr<IShellItem2>& aShellItem);
  static nsresult GetJumpListLink(IShellItem *pItem, nsCOMPtr<nsIJumpListLink>& aLink);

protected:
  nsString mUriTitle;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsICryptoHash> mCryptoHash;
};

class JumpListShortcut : public JumpListItem, public nsIJumpListShortcut
{
public:
  JumpListShortcut() :
   JumpListItem(nsIJumpListItem::JUMPLIST_ITEM_SHORTCUT)
  {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(JumpListShortcut, JumpListItem);
  NS_IMETHOD GetType(PRInt16 *aType) { return JumpListItem::GetType(aType); }
  NS_IMETHOD Equals(nsIJumpListItem *item, bool *_retval);
  NS_DECL_NSIJUMPLISTSHORTCUT

  static nsresult GetShellLink(nsCOMPtr<nsIJumpListItem>& item, 
                               nsRefPtr<IShellLinkW>& aShellLink, 
                               nsCOMPtr<nsIThread> &aIOThread);
  static nsresult GetJumpListShortcut(IShellLinkW *pLink, nsCOMPtr<nsIJumpListShortcut>& aShortcut);
  static nsresult GetOutputIconPath(nsCOMPtr<nsIURI> aFaviconPageURI, 
                                    nsCOMPtr<nsIFile> &aICOFile);

protected:
  PRInt32 mIconIndex;
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
