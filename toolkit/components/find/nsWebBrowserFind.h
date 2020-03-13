/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWebBrowserFindImpl_h__
#define nsWebBrowserFindImpl_h__

#include "nsIWebBrowserFind.h"

#include "nsCOMPtr.h"
#include "nsIWeakReferenceUtils.h"

#include "nsString.h"

class nsIDOMWindow;
class nsIDocShell;
class nsRange;

namespace mozilla {
namespace dom {
class Document;
class Element;
class Selection;
}  // namespace dom
}  // namespace mozilla

//*****************************************************************************
// class nsWebBrowserFind
//*****************************************************************************

class nsWebBrowserFind : public nsIWebBrowserFind,
                         public nsIWebBrowserFindInFrames {
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

  already_AddRefed<mozilla::dom::Selection> GetFrameSelection(
      nsPIDOMWindowOuter* aWindow);
  nsresult ClearFrameSelection(nsPIDOMWindowOuter* aWindow);

  nsresult OnFind(nsPIDOMWindowOuter* aFoundWindow);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY void SetSelectionAndScroll(
      nsPIDOMWindowOuter* aWindow, nsRange* aRange);

  nsresult GetRootNode(mozilla::dom::Document* aDomDoc,
                       mozilla::dom::Element** aNode);
  nsresult GetSearchLimits(nsRange* aRange, nsRange* aStartPt, nsRange* aEndPt,
                           mozilla::dom::Document* aDoc,
                           mozilla::dom::Selection* aSel, bool aWrap);
  nsresult SetRangeAroundDocument(nsRange* aSearchRange, nsRange* aStartPoint,
                                  nsRange* aEndPoint,
                                  mozilla::dom::Document* aDoc);

 protected:
  nsString mSearchString;

  bool mFindBackwards;
  bool mWrapFind;
  bool mEntireWord;
  bool mMatchCase;
  bool mMatchDiacritics;

  bool mSearchSubFrames;
  bool mSearchParentFrames;

  // These are all weak because who knows if windows can go away during our
  // lifetime.
  nsWeakPtr mCurrentSearchFrame;
  nsWeakPtr mRootSearchFrame;
  nsWeakPtr mLastFocusedWindow;
};

#endif
