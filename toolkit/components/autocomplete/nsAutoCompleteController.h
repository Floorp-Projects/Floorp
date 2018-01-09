/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsAutoCompleteController__
#define __nsAutoCompleteController__

#include "nsIAutoCompleteController.h"

#include "nsCOMPtr.h"
#include "nsIAutoCompleteInput.h"
#include "nsIAutoCompletePopup.h"
#include "nsIAutoCompleteResult.h"
#include "nsIAutoCompleteSearch.h"
#include "nsINamed.h"
#include "nsString.h"
#include "nsITreeView.h"
#include "nsITreeSelection.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"

class nsAutoCompleteController final : public nsIAutoCompleteController,
                                       public nsIAutoCompleteObserver,
                                       public nsITimerCallback,
                                       public nsITreeView,
                                       public nsINamed
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsAutoCompleteController,
                                           nsIAutoCompleteController)
  NS_DECL_NSIAUTOCOMPLETECONTROLLER
  NS_DECL_NSIAUTOCOMPLETEOBSERVER
  NS_DECL_NSITREEVIEW
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  nsAutoCompleteController();

protected:
  virtual ~nsAutoCompleteController();

  /**
   * SetValueOfInputTo() sets value of mInput to aValue and notifies the input
   * of setting reason.
   */
  void SetValueOfInputTo(const nsString& aValue, uint16_t aReason);

  /**
   * SetSearchStringInternal() sets both mSearchString and mSetValue to
   * aSearchString.
   */
  void SetSearchStringInternal(const nsAString& aSearchString)
  {
    mSearchString = mSetValue = aSearchString;
  }

  nsresult OpenPopup();
  nsresult ClosePopup();

  nsresult StartSearch(uint16_t aSearchType);

  nsresult BeforeSearches();
  nsresult StartSearches();
  void AfterSearches();
  nsresult ClearSearchTimer();
  void MaybeCompletePlaceholder();

  void HandleSearchResult(nsIAutoCompleteSearch *aSearch,
                          nsIAutoCompleteResult *aResult);
  nsresult ProcessResult(int32_t aSearchIndex, nsIAutoCompleteResult *aResult);
  nsresult PostSearchCleanup();

  nsresult EnterMatch(bool aIsPopupSelection,
                      nsIDOMEvent *aEvent);
  nsresult RevertTextValue();

  nsresult CompleteDefaultIndex(int32_t aResultIndex);
  nsresult CompleteValue(nsString &aValue);

  nsresult GetResultAt(int32_t aIndex, nsIAutoCompleteResult** aResult,
                       int32_t* aRowIndex);
  nsresult GetResultValueAt(int32_t aIndex, bool aGetFinalValue,
                            nsAString & _retval);
  nsresult GetResultLabelAt(int32_t aIndex, nsAString & _retval);
private:
  nsresult GetResultValueLabelAt(int32_t aIndex, bool aGetFinalValue,
                                 bool aGetValue, nsAString & _retval);
protected:

  /**
   * Gets and validates the defaultComplete result and the relative
   * defaultIndex value.
   *
   * @param aResultIndex
   *        Index of the defaultComplete result to be used.  Pass -1 to search
   *        for the first result providing a valid defaultIndex.
   * @param _result
   *        The found result.
   * @param _defaultIndex
   *        The defaultIndex relative to _result.
   */
  nsresult GetDefaultCompleteResult(int32_t aResultIndex,
                                    nsIAutoCompleteResult** _result,
                                    int32_t* _defaultIndex);

  /**
   * Gets the defaultComplete value to be suggested to the user.
   *
   * @param aResultIndex
   *        Index of the defaultComplete result to be used.
   * @param aPreserveCasing
   *        Whether user casing should be preserved.
   * @param _retval
   *        The value to be completed.
   */
  nsresult GetDefaultCompleteValue(int32_t aResultIndex, bool aPreserveCasing,
                                   nsAString &_retval);

  /**
   * Gets the defaultComplete value to be used when the user confirms the
   * current match.
   * The value is returned only if it case-insensitively matches the current
   * input text, otherwise the method returns NS_ERROR_FAILURE.
   * This happens because we don't want to replace text if the user backspaces
   * just before Enter.
   *
   * @param _retval
   *        The value to be completed.
   */
  nsresult GetFinalDefaultCompleteValue(nsAString &_retval);

  nsresult ClearResults(bool aIsSearching = false);

  nsresult RowIndexToSearch(int32_t aRowIndex,
                            int32_t *aSearchIndex, int32_t *aItemIndex);

  // members //////////////////////////////////////////

  nsCOMPtr<nsIAutoCompleteInput> mInput;

  nsCOMArray<nsIAutoCompleteSearch> mSearches;
  // This is used as a sparse array, always use SafeObjectAt to access it.
  nsCOMArray<nsIAutoCompleteResult> mResults;
  // Temporarily keeps the results alive while invoking startSearch() for each
  // search.  This is needed to allow the searches to reuse the previous result,
  // since otherwise the first search clears mResults.
  nsCOMArray<nsIAutoCompleteResult> mResultCache;

  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsITreeSelection> mSelection;
  nsCOMPtr<nsITreeBoxObject> mTree;

  // mSearchString stores value which is the original value of the input or
  // typed by the user.  When user is choosing an item from the popup, this
  // is NOT modified by the item because this is used for reverting the input
  // value when user cancels choosing an item from the popup.
  // This should be set through only SetSearchStringInternal().
  nsString mSearchString;
  nsString mPlaceholderCompletionString;
  // mSetValue stores value which is expected in the input.  So, if input's
  // value and mSetValue are different, it means somebody has changed the
  // value like JS of the web content.
  // This is set only by SetValueOfInputTo() or when modifying mSearchString
  // through SetSearchStringInternal().
  nsString mSetValue;
  bool mDefaultIndexCompleted;
  bool mPopupClosedByCompositionStart;

  // Whether autofill is allowed for the next search. May be retrieved by the
  // search through the "prohibit-autofill" searchParam.
  bool mProhibitAutoFill;

  // Indicates whether the user cleared the autofilled part, returning to the
  // originally entered search string.
  bool mUserClearedAutoFill;

  // Indicates whether clearing the autofilled string should issue a new search.
  bool mClearingAutoFillSearchesAgain;

  enum CompositionState {
    eCompositionState_None,
    eCompositionState_Composing,
    eCompositionState_Committing
  };
  CompositionState mCompositionState;
  uint16_t mSearchStatus;
  uint32_t mRowCount;
  uint32_t mSearchesOngoing;
  uint32_t mSearchesFailed;
  int32_t mDelayedRowCountDelta;
  uint32_t mImmediateSearchesCount;
  // The index of the match on the popup that was selected using the keyboard,
  // if the completeselectedindex attribute is set.
  // This is used to distinguish that selection (which would have been put in
  // the input on being selected) from a moused-over selectedIndex value. This
  // distinction is used to prevent mouse moves from inadvertently changing
  // what happens once the user hits Enter on the keyboard.
  // See bug 1043584 for more details.
  int32_t  mCompletedSelectionIndex;
};

#endif /* __nsAutoCompleteController__ */
