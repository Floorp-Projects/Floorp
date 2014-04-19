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
#include "nsString.h"
#include "nsITreeView.h"
#include "nsITreeSelection.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"

class nsAutoCompleteController : public nsIAutoCompleteController,
                                 public nsIAutoCompleteObserver,
                                 public nsITimerCallback,
                                 public nsITreeView
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsAutoCompleteController,
                                           nsIAutoCompleteController)
  NS_DECL_NSIAUTOCOMPLETECONTROLLER
  NS_DECL_NSIAUTOCOMPLETEOBSERVER
  NS_DECL_NSITREEVIEW
  NS_DECL_NSITIMERCALLBACK
   
  nsAutoCompleteController();
  virtual ~nsAutoCompleteController();
  
protected:
  nsresult OpenPopup();
  nsresult ClosePopup();

  nsresult StartSearch(uint16_t aSearchType);

  nsresult BeforeSearches();
  nsresult StartSearches();
  void AfterSearches();
  nsresult ClearSearchTimer();

  nsresult ProcessResult(int32_t aSearchIndex, nsIAutoCompleteResult *aResult);
  nsresult PostSearchCleanup();

  nsresult EnterMatch(bool aIsPopupSelection);
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

  nsresult ClearResults();
  
  nsresult RowIndexToSearch(int32_t aRowIndex,
                            int32_t *aSearchIndex, int32_t *aItemIndex);

  // members //////////////////////////////////////////
  
  nsCOMPtr<nsIAutoCompleteInput> mInput;

  nsCOMArray<nsIAutoCompleteSearch> mSearches;
  nsCOMArray<nsIAutoCompleteResult> mResults;
  // Caches the match counts for the current ongoing results to allow
  // incremental results to keep the rowcount up to date.
  nsTArray<uint32_t> mMatchCounts;
  // Temporarily keeps the results alive while invoking startSearch() for each
  // search.  This is needed to allow the searches to reuse the previous result,
  // since otherwise the first search clears mResults.
  nsCOMArray<nsIAutoCompleteResult> mResultCache;

  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsITreeSelection> mSelection;
  nsCOMPtr<nsITreeBoxObject> mTree;

  nsString mSearchString;
  bool mDefaultIndexCompleted;
  bool mBackspaced;
  bool mPopupClosedByCompositionStart;
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
  bool mFirstSearchResult;
  uint32_t mImmediateSearchesCount;
};

#endif /* __nsAutoCompleteController__ */
