/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComponentManagerUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISelectionController.h"
#include "nsIDocShell.h"
#include "nsIObserver.h"
#include "nsUnicharUtils.h"
#include "nsIFind.h"
#include "nsIWebBrowserFind.h"
#include "nsWeakReference.h"
#include "nsITypeAheadFind.h"
#include "nsISound.h"

class nsPIDOMWindowInner;
class nsPresContext;
class nsRange;

namespace mozilla {
class PresShell;
namespace dom {
class Element;
class Selection;
}  // namespace dom
}  // namespace mozilla

#define TYPEAHEADFIND_NOTFOUND_WAV_URL "chrome://global/content/notfound.wav"

class nsTypeAheadFind : public nsITypeAheadFind,
                        public nsIObserver,
                        public nsSupportsWeakReference {
 public:
  nsTypeAheadFind();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSITYPEAHEADFIND
  NS_DECL_NSIOBSERVER

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsTypeAheadFind, nsITypeAheadFind)

 protected:
  virtual ~nsTypeAheadFind();

  nsresult PrefsReset();

  void SaveFind();
  void PlayNotFoundSound();
  nsresult GetWebBrowserFind(nsIDocShell* aDocShell,
                             nsIWebBrowserFind** aWebBrowserFind);

  nsresult FindInternal(uint32_t aMode, const nsAString& aSearchString,
                        bool aLinksOnly, bool aDontIterateFrames,
                        uint16_t* aResult);

  void RangeStartsInsideLink(nsRange* aRange, bool* aIsInsideLink,
                             bool* aIsStartingLink);

  void GetSelection(mozilla::PresShell* aPresShell,
                    nsISelectionController** aSelCon,
                    mozilla::dom::Selection** aDomSel);
  bool IsRangeVisible(nsRange* aRange, bool aMustBeVisible,
                      bool aGetTopVisibleLeaf, bool* aUsesIndependentSelection);
  bool IsRangeRendered(nsRange* aRange);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult FindItNow(uint32_t aMode, bool aIsLinksOnly,
                     bool aIsFirstVisiblePreferred, bool aDontIterateFrames,
                     uint16_t* aResult);
  nsresult GetSearchContainers(nsISupports* aContainer,
                               nsISelectionController* aSelectionController,
                               bool aIsFirstVisiblePreferred, bool aFindPrev,
                               mozilla::PresShell** aPresShell,
                               nsPresContext** aPresContext);

  // Get the pres shell from mPresShell and return it only if it is still
  // attached to the DOM window.
  already_AddRefed<mozilla::PresShell> GetPresShell();

  void ReleaseStrongMemberVariables();

  // Current find state
  nsString mTypeAheadBuffer;
  nsCString mNotFoundSoundURL;

  // PRBools are used instead of PRPackedBools because the address of the
  // boolean variable is getting passed into a method.
  bool mStartLinksOnlyPref;
  bool mCaretBrowsingOn;
  bool mDidAddObservers;
  nsCOMPtr<mozilla::dom::Element>
      mFoundLink;  // Most recent elem found, if a link
  nsCOMPtr<mozilla::dom::Element>
      mFoundEditable;           // Most recent elem found, if editable
  RefPtr<nsRange> mFoundRange;  // Most recent range found
  nsCOMPtr<nsPIDOMWindowInner> mCurrentWindow;
  // mLastFindLength is the character length of the last find string.  It is
  // used for disabling the "not found" sound when using backspace or delete
  uint32_t mLastFindLength;

  // Sound is played asynchronously on some platforms.
  // If we destroy mSoundInterface before sound has played, it won't play
  nsCOMPtr<nsISound> mSoundInterface;
  bool mIsSoundInitialized;

  // where selection was when user started the find
  RefPtr<nsRange> mStartFindRange;
  RefPtr<nsRange> mSearchRange;
  RefPtr<nsRange> mStartPointRange;
  RefPtr<nsRange> mEndPointRange;

  // Cached useful interfaces
  nsCOMPtr<nsIFind> mFind;

  bool mCaseSensitive;
  bool mEntireWord;
  bool mMatchDiacritics;

  bool EnsureFind() {
    if (mFind) {
      return true;
    }

    mFind = do_CreateInstance("@mozilla.org/embedcomp/rangefind;1");
    if (!mFind) {
      return false;
    }

    mFind->SetCaseSensitive(mCaseSensitive);
    mFind->SetEntireWord(mEntireWord);
    mFind->SetMatchDiacritics(mMatchDiacritics);

    return true;
  }

  nsCOMPtr<nsIWebBrowserFind> mWebBrowserFind;

  // The focused content window that we're listening to and its cached objects
  nsWeakPtr mDocShell;
  nsWeakPtr mPresShell;
  nsWeakPtr mSelectionController;
  // Most recent match's controller
};
