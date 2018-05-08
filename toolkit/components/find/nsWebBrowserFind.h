/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWebBrowserFindImpl_h__
#define nsWebBrowserFindImpl_h__

#include "nsIWebBrowserFind.h"

#include "nsCOMPtr.h"
#include "nsWeakReference.h"

#include "nsIFind.h"

#include "nsString.h"

#define NS_WEB_BROWSER_FIND_CONTRACTID "@mozilla.org/embedcomp/find;1"

#define NS_WEB_BROWSER_FIND_CID \
  {0x57cf9383, 0x3405, 0x11d5, {0xbe, 0x5b, 0xaa, 0x20, 0xfa, 0x2c, 0xf3, 0x7c}}

class nsIDOMWindow;

class nsIDocShell;
class nsRange;

namespace mozilla {
namespace dom {
class Selection;
} // namespace dom
} // namespace mozilla

//*****************************************************************************
// class nsWebBrowserFind
//*****************************************************************************

class nsWebBrowserFind
  : public nsIWebBrowserFind
  , public nsIWebBrowserFindInFrames
{
public:
  nsWebBrowserFind();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIWebBrowserFind
  NS_DECL_NSIWEBBROWSERFIND

  // nsIWebBrowserFindInFrames
  NS_DECL_NSIWEBBROWSERFINDINFRAMES

protected:
  virtual ~nsWebBrowserFind();

  bool CanFindNext() { return mSearchString.Length() != 0; }

  nsresult SearchInFrame(nsPIDOMWindowOuter* aWindow, bool aWrapping,
                         bool* aDidFind);

  nsresult OnStartSearchFrame(nsPIDOMWindowOuter* aWindow);
  nsresult OnEndSearchFrame(nsPIDOMWindowOuter* aWindow);

  already_AddRefed<mozilla::dom::Selection>
    GetFrameSelection(nsPIDOMWindowOuter* aWindow);
  nsresult ClearFrameSelection(nsPIDOMWindowOuter* aWindow);

  nsresult OnFind(nsPIDOMWindowOuter* aFoundWindow);

  void SetSelectionAndScroll(nsPIDOMWindowOuter* aWindow, nsRange* aRange);

  nsresult GetRootNode(nsIDOMDocument* aDomDoc, nsIDOMNode** aNode);
  nsresult GetSearchLimits(nsRange* aRange,
                           nsRange* aStartPt, nsRange* aEndPt,
                           nsIDOMDocument* aDoc, mozilla::dom::Selection* aSel,
                           bool aWrap);
  nsresult SetRangeAroundDocument(nsRange* aSearchRange,
                                  nsRange* aStartPoint,
                                  nsRange* aEndPoint,
                                  nsIDOMDocument* aDoc);

protected:
  nsString mSearchString;

  bool mFindBackwards;
  bool mWrapFind;
  bool mEntireWord;
  bool mMatchCase;

  bool mSearchSubFrames;
  bool mSearchParentFrames;

  // These are all weak because who knows if windows can go away during our
  // lifetime.
  nsWeakPtr mCurrentSearchFrame;
  nsWeakPtr mRootSearchFrame;
  nsWeakPtr mLastFocusedWindow;
};

#endif
