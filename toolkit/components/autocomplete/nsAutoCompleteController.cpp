/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoCompleteController.h"
#include "nsAutoCompleteSimpleResult.h"

#include "nsAutoPtr.h"
#include "nsNetCID.h"
#include "nsIIOService.h"
#include "nsToolkitCompsCID.h"
#include "nsIServiceManager.h"
#include "nsIAtomService.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsITreeBoxObject.h"
#include "nsITreeColumns.h"
#include "nsIObserverService.h"
#include "nsIDOMKeyEvent.h"
#include "mozilla/Services.h"
#include "mozilla/ModuleUtils.h"

static const char *kAutoCompleteSearchCID = "@mozilla.org/autocomplete/search;1?name=";

NS_IMPL_CYCLE_COLLECTION_CLASS(nsAutoCompleteController)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsAutoCompleteController)
  tmp->SetInput(nullptr);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsAutoCompleteController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInput)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSearches)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResults)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsAutoCompleteController)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsAutoCompleteController)
NS_INTERFACE_TABLE_HEAD(nsAutoCompleteController)
  NS_INTERFACE_TABLE(nsAutoCompleteController, nsIAutoCompleteController,
                     nsIAutoCompleteObserver, nsITimerCallback, nsITreeView)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsAutoCompleteController)
NS_INTERFACE_MAP_END

nsAutoCompleteController::nsAutoCompleteController() :
  mDefaultIndexCompleted(false),
  mPopupClosedByCompositionStart(false),
  mProhibitAutoFill(false),
  mUserClearedAutoFill(false),
  mClearingAutoFillSearchesAgain(false),
  mCompositionState(eCompositionState_None),
  mSearchStatus(nsAutoCompleteController::STATUS_NONE),
  mRowCount(0),
  mSearchesOngoing(0),
  mSearchesFailed(0),
  mFirstSearchResult(false),
  mImmediateSearchesCount(0),
  mCompletedSelectionIndex(-1)
{
}

nsAutoCompleteController::~nsAutoCompleteController()
{
  SetInput(nullptr);
}

////////////////////////////////////////////////////////////////////////
//// nsIAutoCompleteController

NS_IMETHODIMP
nsAutoCompleteController::GetSearchStatus(uint16_t *aSearchStatus)
{
  *aSearchStatus = mSearchStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetMatchCount(uint32_t *aMatchCount)
{
  *aMatchCount = mRowCount;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetInput(nsIAutoCompleteInput **aInput)
{
  *aInput = mInput;
  NS_IF_ADDREF(*aInput);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::SetInput(nsIAutoCompleteInput *aInput)
{
  // Don't do anything if the input isn't changing.
  if (mInput == aInput)
    return NS_OK;

  // Clear out the current search context
  if (mInput) {
    // Stop all searches in case they are async.
    StopSearch();
    ClearResults();
    ClosePopup();
    mSearches.Clear();
  }

  mInput = aInput;

  // Nothing more to do if the input was just being set to null.
  if (!aInput)
    return NS_OK;

  nsAutoString newValue;
  aInput->GetTextValue(newValue);

  // Clear out this reference in case the new input's popup has no tree
  mTree = nullptr;

  // Reset all search state members to default values
  mSearchString = newValue;
  mPlaceholderCompletionString.Truncate();
  mDefaultIndexCompleted = false;
  mProhibitAutoFill = false;
  mSearchStatus = nsIAutoCompleteController::STATUS_NONE;
  mRowCount = 0;
  mSearchesOngoing = 0;
  mCompletedSelectionIndex = -1;

  // Initialize our list of search objects
  uint32_t searchCount;
  aInput->GetSearchCount(&searchCount);
  mResults.SetCapacity(searchCount);
  mSearches.SetCapacity(searchCount);
  mImmediateSearchesCount = 0;

  const char *searchCID = kAutoCompleteSearchCID;

  // Since the controller can be used as a service it's important to reset this.
  mClearingAutoFillSearchesAgain = false;

  for (uint32_t i = 0; i < searchCount; ++i) {
    // Use the search name to create the contract id string for the search service
    nsAutoCString searchName;
    aInput->GetSearchAt(i, searchName);
    nsAutoCString cid(searchCID);
    cid.Append(searchName);

    // Use the created cid to get a pointer to the search service and store it for later
    nsCOMPtr<nsIAutoCompleteSearch> search = do_GetService(cid.get());
    if (search) {
      mSearches.AppendObject(search);

      // Count immediate searches.
      nsCOMPtr<nsIAutoCompleteSearchDescriptor> searchDesc =
        do_QueryInterface(search);
      if (searchDesc) {
        uint16_t searchType = nsIAutoCompleteSearchDescriptor::SEARCH_TYPE_DELAYED;
        if (NS_SUCCEEDED(searchDesc->GetSearchType(&searchType)) &&
            searchType == nsIAutoCompleteSearchDescriptor::SEARCH_TYPE_IMMEDIATE) {
          mImmediateSearchesCount++;
        }

        if (!mClearingAutoFillSearchesAgain) {
          searchDesc->GetClearingAutoFillSearchesAgain(&mClearingAutoFillSearchesAgain);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::StartSearch(const nsAString &aSearchString)
{
  mSearchString = aSearchString;
  StartSearches();
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleText()
{
  // Note: the events occur in the following order when IME is used.
  // 1. a compositionstart event(HandleStartComposition)
  // 2. some input events (HandleText), eCompositionState_Composing
  // 3. a compositionend event(HandleEndComposition)
  // 4. an input event(HandleText), eCompositionState_Committing
  // We should do nothing during composition.
  if (mCompositionState == eCompositionState_Composing) {
    return NS_OK;
  }

  bool handlingCompositionCommit =
    (mCompositionState == eCompositionState_Committing);
  bool popupClosedByCompositionStart = mPopupClosedByCompositionStart;
  if (handlingCompositionCommit) {
    mCompositionState = eCompositionState_None;
    mPopupClosedByCompositionStart = false;
  }

  if (!mInput) {
    // Stop all searches in case they are async.
    StopSearch();
    // Note: if now is after blur and IME end composition,
    // check mInput before calling.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=193544#c31
    NS_ERROR("Called before attaching to the control or after detaching from the control");
    return NS_OK;
  }

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  nsAutoString newValue;
  input->GetTextValue(newValue);

  // Stop all searches in case they are async.
  StopSearch();

  if (!mInput) {
    // StopSearch() can call PostSearchCleanup() which might result
    // in a blur event, which could null out mInput, so we need to check it
    // again.  See bug #395344 for more details
    return NS_OK;
  }

  bool disabled;
  input->GetDisableAutoComplete(&disabled);
  NS_ENSURE_TRUE(!disabled, NS_OK);

  // Usually we don't search again if the new string is the same as the last one.
  // However, if this is called immediately after compositionend event,
  // we need to search the same value again since the search was canceled
  // at compositionstart event handler.
  // The new string might also be the same as the last search if the autofilled
  // portion was cleared. In this case, we may want to search again.

  // Whether the user removed some text at the end.
  bool userRemovedText =
    newValue.Length() < mSearchString.Length() &&
    Substring(mSearchString, 0, newValue.Length()).Equals(newValue);

  // Whether the user is repeating the previous search.
  bool repeatingPreviousSearch = !userRemovedText &&
                                 newValue.Equals(mSearchString);

  mUserClearedAutoFill =
    repeatingPreviousSearch &&
    newValue.Length() < mPlaceholderCompletionString.Length() &&
    Substring(mPlaceholderCompletionString, 0, newValue.Length()).Equals(newValue);
  bool searchAgainOnAutoFillClear = mUserClearedAutoFill && mClearingAutoFillSearchesAgain;

  if (!handlingCompositionCommit &&
      !searchAgainOnAutoFillClear &&
      newValue.Length() > 0 &&
      repeatingPreviousSearch) {
    return NS_OK;
  }

  if (userRemovedText || searchAgainOnAutoFillClear) {
    if (userRemovedText) {
      // We need to throw away previous results so we don't try to search
      // through them again.
      ClearResults();
    }
    mProhibitAutoFill = true;
    mPlaceholderCompletionString.Truncate();
  } else {
    mProhibitAutoFill = false;
  }

  mSearchString = newValue;

  // Don't search if the value is empty
  if (newValue.Length() == 0) {
    // If autocomplete popup was closed by compositionstart event handler,
    // we should reopen it forcibly even if the value is empty.
    if (popupClosedByCompositionStart && handlingCompositionCommit) {
      bool cancel;
      HandleKeyNavigation(nsIDOMKeyEvent::DOM_VK_DOWN, &cancel);
      return NS_OK;
    }
    ClosePopup();
    return NS_OK;
  }

  StartSearches();

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleEnter(bool aIsPopupSelection, bool *_retval)
{
  *_retval = false;
  if (!mInput)
    return NS_OK;

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  // allow the event through unless there is something selected in the popup
  input->GetPopupOpen(_retval);
  if (*_retval) {
    nsCOMPtr<nsIAutoCompletePopup> popup;
    input->GetPopup(getter_AddRefs(popup));

    if (popup) {
      int32_t selectedIndex;
      popup->GetSelectedIndex(&selectedIndex);
      *_retval = selectedIndex >= 0;
    }
  }

  // Stop the search, and handle the enter.
  StopSearch();
  EnterMatch(aIsPopupSelection);

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleEscape(bool *_retval)
{
  *_retval = false;
  if (!mInput)
    return NS_OK;

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  // allow the event through if the popup is closed
  input->GetPopupOpen(_retval);

  // Stop all searches in case they are async.
  StopSearch();
  ClearResults();
  RevertTextValue();
  ClosePopup();

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleStartComposition()
{
  NS_ENSURE_TRUE(mCompositionState != eCompositionState_Composing, NS_OK);

  mPopupClosedByCompositionStart = false;
  mCompositionState = eCompositionState_Composing;

  if (!mInput)
    return NS_OK;

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  bool disabled;
  input->GetDisableAutoComplete(&disabled);
  if (disabled)
    return NS_OK;

  // Stop all searches in case they are async.
  StopSearch();

  bool isOpen = false;
  input->GetPopupOpen(&isOpen);
  if (isOpen) {
    ClosePopup();

    bool stillOpen = false;
    input->GetPopupOpen(&stillOpen);
    mPopupClosedByCompositionStart = !stillOpen;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleEndComposition()
{
  NS_ENSURE_TRUE(mCompositionState == eCompositionState_Composing, NS_OK);

  // We can't yet retrieve the committed value from the editor, since it isn't
  // completely committed yet. Set mCompositionState to
  // eCompositionState_Committing, so that when HandleText() is called (in
  // response to the "input" event), we know that we should handle the
  // committed text.
  mCompositionState = eCompositionState_Committing;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleTab()
{
  bool cancel;
  return HandleEnter(false, &cancel);
}

NS_IMETHODIMP
nsAutoCompleteController::HandleKeyNavigation(uint32_t aKey, bool *_retval)
{
  // By default, don't cancel the event
  *_retval = false;

  if (!mInput) {
    // Stop all searches in case they are async.
    StopSearch();
    // Note: if now is after blur and IME end composition,
    // check mInput before calling.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=193544#c31
    NS_ERROR("Called before attaching to the control or after detaching from the control");
    return NS_OK;
  }

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  nsCOMPtr<nsIAutoCompletePopup> popup;
  input->GetPopup(getter_AddRefs(popup));
  NS_ENSURE_TRUE(popup != nullptr, NS_ERROR_FAILURE);

  bool disabled;
  input->GetDisableAutoComplete(&disabled);
  NS_ENSURE_TRUE(!disabled, NS_OK);

  if (aKey == nsIDOMKeyEvent::DOM_VK_UP ||
      aKey == nsIDOMKeyEvent::DOM_VK_DOWN ||
      aKey == nsIDOMKeyEvent::DOM_VK_PAGE_UP ||
      aKey == nsIDOMKeyEvent::DOM_VK_PAGE_DOWN)
  {
    // Prevent the input from handling up/down events, as it may move
    // the cursor to home/end on some systems
    *_retval = true;

    bool isOpen = false;
    input->GetPopupOpen(&isOpen);
    if (isOpen) {
      bool reverse = aKey == nsIDOMKeyEvent::DOM_VK_UP ||
                      aKey == nsIDOMKeyEvent::DOM_VK_PAGE_UP ? true : false;
      bool page = aKey == nsIDOMKeyEvent::DOM_VK_PAGE_UP ||
                    aKey == nsIDOMKeyEvent::DOM_VK_PAGE_DOWN ? true : false;

      // Fill in the value of the textbox with whatever is selected in the popup
      // if the completeSelectedIndex attribute is set.  We check this before
      // calling SelectBy of an earlier attempt to avoid crashing.
      bool completeSelection;
      input->GetCompleteSelectedIndex(&completeSelection);

      // Instruct the result view to scroll by the given amount and direction
      popup->SelectBy(reverse, page);

      if (completeSelection)
      {
        int32_t selectedIndex;
        popup->GetSelectedIndex(&selectedIndex);
        if (selectedIndex >= 0) {
          //  A result is selected, so fill in its value
          nsAutoString value;
          if (NS_SUCCEEDED(GetResultValueAt(selectedIndex, false, value))) {
            // If the result is the previously autofilled string, then restore
            // the search string and selection that existed when the result was
            // autofilled.  Else, fill the result and move the caret to the end.
            int32_t start;
            if (value.Equals(mPlaceholderCompletionString,
                             nsCaseInsensitiveStringComparator())) {
              start = mSearchString.Length();
              value = mPlaceholderCompletionString;
            } else {
              start = value.Length();
            }
            input->SetTextValue(value);
            input->SelectTextRange(start, value.Length());
          }
          mCompletedSelectionIndex = selectedIndex;
        } else {
          // Nothing is selected, so fill in the last typed value
          input->SetTextValue(mSearchString);
          input->SelectTextRange(mSearchString.Length(), mSearchString.Length());
          mCompletedSelectionIndex = -1;
        }
      }
    } else {
#ifdef XP_MACOSX
      // on Mac, only show the popup if the caret is at the start or end of
      // the input and there is no selection, so that the default defined key
      // shortcuts for up and down move to the beginning and end of the field
      // otherwise.
      int32_t start, end;
      if (aKey == nsIDOMKeyEvent::DOM_VK_UP) {
        input->GetSelectionStart(&start);
        input->GetSelectionEnd(&end);
        if (start > 0 || start != end)
          *_retval = false;
      }
      else if (aKey == nsIDOMKeyEvent::DOM_VK_DOWN) {
        nsAutoString text;
        input->GetTextValue(text);
        input->GetSelectionStart(&start);
        input->GetSelectionEnd(&end);
        if (start != end || end < (int32_t)text.Length())
          *_retval = false;
      }
#endif
      if (*_retval) {
        // Open the popup if there has been a previous search, or else kick off a new search
        if (!mResults.IsEmpty()) {
          if (mRowCount) {
            OpenPopup();
          }
        } else {
          // Stop all searches in case they are async.
          StopSearch();

          if (!mInput) {
            // StopSearch() can call PostSearchCleanup() which might result
            // in a blur event, which could null out mInput, so we need to check it
            // again.  See bug #395344 for more details
            return NS_OK;
          }

          // Some script may have changed the value of the text field since our
          // last keypress or after our focus handler and we don't want to search
          // for a stale string.
          nsAutoString value;
          input->GetTextValue(value);
          mSearchString = value;

          StartSearches();
        }
      }
    }
  } else if (   aKey == nsIDOMKeyEvent::DOM_VK_LEFT
             || aKey == nsIDOMKeyEvent::DOM_VK_RIGHT
#ifndef XP_MACOSX
             || aKey == nsIDOMKeyEvent::DOM_VK_HOME
#endif
            )
  {
    // The user hit a text-navigation key.
    bool isOpen = false;
    input->GetPopupOpen(&isOpen);

    // If minresultsforpopup > 1 and there's less matches than the minimum
    // required, the popup is not open, but the search suggestion is showing
    // inline, so we should proceed as if we had the popup.
    uint32_t minResultsForPopup;
    input->GetMinResultsForPopup(&minResultsForPopup);
    if (isOpen || (mRowCount > 0 && mRowCount < minResultsForPopup)) {
      // For completeSelectedIndex autocomplete fields, if the popup shouldn't
      // close when the caret is moved, don't adjust the text value or caret
      // position.
      if (isOpen) {
        bool noRollup;
        input->GetNoRollupOnCaretMove(&noRollup);
        if (noRollup) {
          bool completeSelection;
          input->GetCompleteSelectedIndex(&completeSelection);
          if (completeSelection) {
            return NS_OK;
          }
        }
      }

      int32_t selectedIndex;
      popup->GetSelectedIndex(&selectedIndex);
      bool shouldComplete;
      input->GetCompleteDefaultIndex(&shouldComplete);
      if (selectedIndex >= 0) {
        // The pop-up is open and has a selection, take its value
        nsAutoString value;
        if (NS_SUCCEEDED(GetResultValueAt(selectedIndex, false, value))) {
          input->SetTextValue(value);
          input->SelectTextRange(value.Length(), value.Length());
        }
      }
      else if (shouldComplete) {
        // We usually try to preserve the casing of what user has typed, but
        // if he wants to autocomplete, we will replace the value with the
        // actual autocomplete result. Note that the autocomplete input can also
        // be showing e.g. "bar >> foo bar" if the search matched "bar", a
        // word not at the start of the full value "foo bar".
        // The user wants explicitely to use that result, so this ensures
        // association of the result with the autocompleted text.
        nsAutoString value;
        nsAutoString inputValue;
        input->GetTextValue(inputValue);
        if (NS_SUCCEEDED(GetDefaultCompleteValue(-1, false, value))) {
          nsAutoString suggestedValue;
          int32_t pos = inputValue.Find(" >> ");
          if (pos > 0) {
            inputValue.Right(suggestedValue, inputValue.Length() - pos - 4);
          } else {
            suggestedValue = inputValue;
          }

          if (value.Equals(suggestedValue, nsCaseInsensitiveStringComparator())) {
            input->SetTextValue(value);
            input->SelectTextRange(value.Length(), value.Length());
          }
        }
      }

      // Close the pop-up even if nothing was selected
      ClearSearchTimer();
      ClosePopup();
    }
    // Update last-searched string to the current input, since the input may
    // have changed.  Without this, subsequent backspaces look like text
    // additions, not text deletions.
    nsAutoString value;
    input->GetTextValue(value);
    mSearchString = value;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleDelete(bool *_retval)
{
  *_retval = false;
  if (!mInput)
    return NS_OK;

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  bool isOpen = false;
  input->GetPopupOpen(&isOpen);
  if (!isOpen || mRowCount <= 0) {
    // Nothing left to delete, proceed as normal
    HandleText();
    return NS_OK;
  }

  nsCOMPtr<nsIAutoCompletePopup> popup;
  input->GetPopup(getter_AddRefs(popup));

  int32_t index, searchIndex, rowIndex;
  popup->GetSelectedIndex(&index);
  if (index == -1) {
    // No row is selected in the list
    HandleText();
    return NS_OK;
  }

  RowIndexToSearch(index, &searchIndex, &rowIndex);
  NS_ENSURE_TRUE(searchIndex >= 0 && rowIndex >= 0, NS_ERROR_FAILURE);

  nsIAutoCompleteResult *result = mResults.SafeObjectAt(searchIndex);
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  nsAutoString search;
  input->GetSearchParam(search);

  // Clear the row in our result and in the DB.
  result->RemoveValueAt(rowIndex, true);
  --mRowCount;

  // We removed it, so make sure we cancel the event that triggered this call.
  *_retval = true;

  // Unselect the current item.
  popup->SetSelectedIndex(-1);

  // Tell the tree that the row count changed.
  if (mTree)
    mTree->RowCountChanged(mRowCount, -1);

  // Adjust index, if needed.
  if (index >= (int32_t)mRowCount)
    index = mRowCount - 1;

  if (mRowCount > 0) {
    // There are still rows in the popup, select the current index again.
    popup->SetSelectedIndex(index);

    // Complete to the new current value.
    bool shouldComplete = false;
    input->GetCompleteDefaultIndex(&shouldComplete);
    if (shouldComplete) {
      nsAutoString value;
      if (NS_SUCCEEDED(GetResultValueAt(index, false, value))) {
        CompleteValue(value);
      }
    }

    // Invalidate the popup.
    popup->Invalidate();
  } else {
    // Nothing left in the popup, clear any pending search timers and
    // close the popup.
    ClearSearchTimer();
    uint32_t minResults;
    input->GetMinResultsForPopup(&minResults);
    if (minResults) {
      ClosePopup();
    }
  }

  return NS_OK;
}

nsresult 
nsAutoCompleteController::GetResultAt(int32_t aIndex, nsIAutoCompleteResult** aResult,
                                      int32_t* aRowIndex)
{
  int32_t searchIndex;
  RowIndexToSearch(aIndex, &searchIndex, aRowIndex);
  NS_ENSURE_TRUE(searchIndex >= 0 && *aRowIndex >= 0, NS_ERROR_FAILURE);

  *aResult = mResults.SafeObjectAt(searchIndex);
  NS_ENSURE_TRUE(*aResult, NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetValueAt(int32_t aIndex, nsAString & _retval)
{
  GetResultLabelAt(aIndex, _retval);

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetLabelAt(int32_t aIndex, nsAString & _retval)
{
  GetResultLabelAt(aIndex, _retval);

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetCommentAt(int32_t aIndex, nsAString & _retval)
{
  int32_t rowIndex;
  nsIAutoCompleteResult* result;
  nsresult rv = GetResultAt(aIndex, &result, &rowIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  return result->GetCommentAt(rowIndex, _retval);
}

NS_IMETHODIMP
nsAutoCompleteController::GetStyleAt(int32_t aIndex, nsAString & _retval)
{
  int32_t rowIndex;
  nsIAutoCompleteResult* result;
  nsresult rv = GetResultAt(aIndex, &result, &rowIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  return result->GetStyleAt(rowIndex, _retval);
}

NS_IMETHODIMP
nsAutoCompleteController::GetImageAt(int32_t aIndex, nsAString & _retval)
{
  int32_t rowIndex;
  nsIAutoCompleteResult* result;
  nsresult rv = GetResultAt(aIndex, &result, &rowIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  return result->GetImageAt(rowIndex, _retval);
}

NS_IMETHODIMP
nsAutoCompleteController::GetFinalCompleteValueAt(int32_t aIndex,
                                                  nsAString & _retval)
{
  int32_t rowIndex;
  nsIAutoCompleteResult* result;
  nsresult rv = GetResultAt(aIndex, &result, &rowIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  return result->GetFinalCompleteValueAt(rowIndex, _retval);
}

NS_IMETHODIMP
nsAutoCompleteController::SetSearchString(const nsAString &aSearchString)
{
  mSearchString = aSearchString;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetSearchString(nsAString &aSearchString)
{
  aSearchString = mSearchString;
  return NS_OK;
}

void
nsAutoCompleteController::HandleSearchResult(nsIAutoCompleteSearch *aSearch,
                                             nsIAutoCompleteResult *aResult)
{
  // Look up the index of the search which is returning.
  for (uint32_t i = 0; i < mSearches.Length(); ++i) {
    if (mSearches[i] == aSearch) {
      ProcessResult(i, aResult);
    }
  }
}


////////////////////////////////////////////////////////////////////////
//// nsIAutoCompleteObserver

NS_IMETHODIMP
nsAutoCompleteController::OnUpdateSearchResult(nsIAutoCompleteSearch *aSearch, nsIAutoCompleteResult* aResult)
{
  MOZ_ASSERT(mSearches.Contains(aSearch));

  ClearResults();
  HandleSearchResult(aSearch, aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::OnSearchResult(nsIAutoCompleteSearch *aSearch, nsIAutoCompleteResult* aResult)
{
  MOZ_ASSERT(mSearchesOngoing > 0 && mSearches.Contains(aSearch));

  // If this is the first search result we are processing
  // we should clear out the previously cached results.
  if (mFirstSearchResult) {
    ClearResults();
    mFirstSearchResult = false;
  }

  uint16_t result = 0;
  if (aResult) {
    aResult->GetSearchResult(&result);
  }

  // If our results are incremental, the search is still ongoing.
  if (result != nsIAutoCompleteResult::RESULT_SUCCESS_ONGOING &&
      result != nsIAutoCompleteResult::RESULT_NOMATCH_ONGOING) {
    --mSearchesOngoing;
  }

  HandleSearchResult(aSearch, aResult);

  if (mSearchesOngoing == 0) {
    // If this is the last search to return, cleanup.
    PostSearchCleanup();
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsITimerCallback

NS_IMETHODIMP
nsAutoCompleteController::Notify(nsITimer *timer)
{
  mTimer = nullptr;

  if (mImmediateSearchesCount == 0) {
    // If there were no immediate searches, BeforeSearches has not yet been
    // called, so do it now.
    nsresult rv = BeforeSearches();
    if (NS_FAILED(rv))
      return rv;
  }
  StartSearch(nsIAutoCompleteSearchDescriptor::SEARCH_TYPE_DELAYED);
  AfterSearches();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsITreeView

NS_IMETHODIMP
nsAutoCompleteController::GetRowCount(int32_t *aRowCount)
{
  *aRowCount = mRowCount;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetRowProperties(int32_t index, nsAString& aProps)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetCellProperties(int32_t row, nsITreeColumn* col,
                                            nsAString& aProps)
{
  if (row >= 0) {
    GetStyleAt(row, aProps);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetColumnProperties(nsITreeColumn* col, nsAString& aProps)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetImageSrc(int32_t row, nsITreeColumn* col, nsAString& _retval)
{
  const char16_t* colID;
  col->GetIdConst(&colID);

  if (NS_LITERAL_STRING("treecolAutoCompleteValue").Equals(colID))
    return GetImageAt(row, _retval);

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetProgressMode(int32_t row, nsITreeColumn* col, int32_t* _retval)
{
  NS_NOTREACHED("tree has no progress cells");
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetCellValue(int32_t row, nsITreeColumn* col, nsAString& _retval)
{
  NS_NOTREACHED("all of our cells are text");
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetCellText(int32_t row, nsITreeColumn* col, nsAString& _retval)
{
  const char16_t* colID;
  col->GetIdConst(&colID);

  if (NS_LITERAL_STRING("treecolAutoCompleteValue").Equals(colID))
    GetValueAt(row, _retval);
  else if (NS_LITERAL_STRING("treecolAutoCompleteComment").Equals(colID))
    GetCommentAt(row, _retval);

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsContainer(int32_t index, bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsContainerOpen(int32_t index, bool *_retval)
{
  NS_NOTREACHED("no container cells");
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsContainerEmpty(int32_t index, bool *_retval)
{
  NS_NOTREACHED("no container cells");
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetLevel(int32_t index, int32_t *_retval)
{
  *_retval = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetParentIndex(int32_t rowIndex, int32_t *_retval)
{
  *_retval = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HasNextSibling(int32_t rowIndex, int32_t afterIndex, bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::ToggleOpenState(int32_t index)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::SetTree(nsITreeBoxObject *tree)
{
  mTree = tree;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetSelection(nsITreeSelection * *aSelection)
{
  *aSelection = mSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP nsAutoCompleteController::SetSelection(nsITreeSelection * aSelection)
{
  mSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::SelectionChanged()
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::SetCellValue(int32_t row, nsITreeColumn* col, const nsAString& value)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::SetCellText(int32_t row, nsITreeColumn* col, const nsAString& value)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::CycleHeader(nsITreeColumn* col)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::CycleCell(int32_t row, nsITreeColumn* col)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsEditable(int32_t row, nsITreeColumn* col, bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsSelectable(int32_t row, nsITreeColumn* col, bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsSeparator(int32_t index, bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsSorted(bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::CanDrop(int32_t index, int32_t orientation,
                                  nsIDOMDataTransfer* dataTransfer, bool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::Drop(int32_t row, int32_t orientation, nsIDOMDataTransfer* dataTransfer)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::PerformAction(const char16_t *action)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::PerformActionOnRow(const char16_t *action, int32_t row)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::PerformActionOnCell(const char16_t* action, int32_t row, nsITreeColumn* col)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsAutoCompleteController

nsresult
nsAutoCompleteController::OpenPopup()
{
  uint32_t minResults;
  mInput->GetMinResultsForPopup(&minResults);

  if (mRowCount >= minResults) {
    return mInput->SetPopupOpen(true);
  }

  return NS_OK;
}

nsresult
nsAutoCompleteController::ClosePopup()
{
  if (!mInput) {
    return NS_OK;
  }

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  bool isOpen = false;
  input->GetPopupOpen(&isOpen);
  if (!isOpen)
    return NS_OK;

  nsCOMPtr<nsIAutoCompletePopup> popup;
  input->GetPopup(getter_AddRefs(popup));
  NS_ENSURE_TRUE(popup != nullptr, NS_ERROR_FAILURE);
  popup->SetSelectedIndex(-1);
  return input->SetPopupOpen(false);
}

nsresult
nsAutoCompleteController::BeforeSearches()
{
  NS_ENSURE_STATE(mInput);

  mSearchStatus = nsIAutoCompleteController::STATUS_SEARCHING;
  mDefaultIndexCompleted = false;

  // The first search result will clear mResults array, though we should pass
  // the previous result to each search to allow them to reuse it.  So we
  // temporarily cache current results till AfterSearches().
  if (!mResultCache.AppendObjects(mResults)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mSearchesOngoing = mSearches.Length();
  mSearchesFailed = 0;
  mFirstSearchResult = true;

  // notify the input that the search is beginning
  mInput->OnSearchBegin();

  return NS_OK;
}

nsresult
nsAutoCompleteController::StartSearch(uint16_t aSearchType)
{
  NS_ENSURE_STATE(mInput);
  nsCOMPtr<nsIAutoCompleteInput> input = mInput;

  // Iterate a copy of |mSearches| so that we don't run into trouble if the
  // array is mutated while we're still in the loop. An nsIAutoCompleteSearch
  // implementation could synchronously start a new search when StartSearch()
  // is called and that would lead to assertions down the way.
  nsCOMArray<nsIAutoCompleteSearch> searchesCopy(mSearches);
  for (uint32_t i = 0; i < searchesCopy.Length(); ++i) {
    nsCOMPtr<nsIAutoCompleteSearch> search = searchesCopy[i];

    // Filter on search type.  Not all the searches implement this interface,
    // in such a case just consider them delayed.
    uint16_t searchType = nsIAutoCompleteSearchDescriptor::SEARCH_TYPE_DELAYED;
    nsCOMPtr<nsIAutoCompleteSearchDescriptor> searchDesc =
      do_QueryInterface(search);
    if (searchDesc)
      searchDesc->GetSearchType(&searchType);
    if (searchType != aSearchType)
      continue;

    nsIAutoCompleteResult *result = mResultCache.SafeObjectAt(i);

    if (result) {
      uint16_t searchResult;
      result->GetSearchResult(&searchResult);
      if (searchResult != nsIAutoCompleteResult::RESULT_SUCCESS &&
          searchResult != nsIAutoCompleteResult::RESULT_SUCCESS_ONGOING &&
          searchResult != nsIAutoCompleteResult::RESULT_NOMATCH)
        result = nullptr;
    }

    nsAutoString searchParam;
    nsresult rv = input->GetSearchParam(searchParam);
    if (NS_FAILED(rv))
        return rv;

    // FormFill expects the searchParam to only contain the input element id,
    // other consumers may have other expectations, so this modifies it only
    // for new consumers handling autoFill by themselves.
    if (mProhibitAutoFill && mClearingAutoFillSearchesAgain) {
      searchParam.AppendLiteral(" prohibit-autofill");
    }

    rv = search->StartSearch(mSearchString, searchParam, result, static_cast<nsIAutoCompleteObserver *>(this));
    if (NS_FAILED(rv)) {
      ++mSearchesFailed;
      MOZ_ASSERT(mSearchesOngoing > 0);
      --mSearchesOngoing;
    }
    // Because of the joy of nested event loops (which can easily happen when some
    // code uses a generator for an asynchronous AutoComplete search),
    // nsIAutoCompleteSearch::StartSearch might cause us to be detached from our input
    // field.  The next time we iterate, we'd be touching something that we shouldn't
    // be, and result in a crash.
    if (!mInput) {
      // The search operation has been finished.
      return NS_OK;
    }
  }

  return NS_OK;
}

void
nsAutoCompleteController::AfterSearches()
{
  mResultCache.Clear();
  if (mSearchesFailed == mSearches.Length())
    PostSearchCleanup();
}

NS_IMETHODIMP
nsAutoCompleteController::StopSearch()
{
  // Stop the timer if there is one
  ClearSearchTimer();

  // Stop any ongoing asynchronous searches
  if (mSearchStatus == nsIAutoCompleteController::STATUS_SEARCHING) {
    for (uint32_t i = 0; i < mSearches.Length(); ++i) {
      nsCOMPtr<nsIAutoCompleteSearch> search = mSearches[i];
      search->StopSearch();
    }
    mSearchesOngoing = 0;
    // since we were searching, but now we've stopped,
    // we need to call PostSearchCleanup()
    PostSearchCleanup();
  }
  return NS_OK;
}

void
nsAutoCompleteController::MaybeCompletePlaceholder()
{
  MOZ_ASSERT(mInput);

  if (!mInput) { // or mInput depending on what you choose
    MOZ_ASSERT_UNREACHABLE("Input should always be valid at this point");
    return;
  }

  int32_t selectionStart;
  mInput->GetSelectionStart(&selectionStart);
  int32_t selectionEnd;
  mInput->GetSelectionEnd(&selectionEnd);

  // Check if the current input should be completed with the placeholder string
  // from the last completion until the actual search results come back.
  // The new input string needs to be compatible with the last completed string.
  // E.g. if the new value is "fob", but the last completion was "foobar",
  // then the last completion is incompatible.
  // If the search string is the same as the last completion value, then don't
  // complete the value again (this prevents completion to happen e.g. if the
  // cursor is moved and StartSeaches() is invoked).
  // In addition, the selection must be at the end of the current input to
  // trigger the placeholder completion.
  bool usePlaceholderCompletion =
    !mUserClearedAutoFill &&
    !mPlaceholderCompletionString.IsEmpty() &&
    mPlaceholderCompletionString.Length() > mSearchString.Length() &&
    selectionEnd == selectionStart &&
    selectionEnd == (int32_t)mSearchString.Length() &&
    StringBeginsWith(mPlaceholderCompletionString, mSearchString,
                    nsCaseInsensitiveStringComparator());

  if (usePlaceholderCompletion) {
    CompleteValue(mPlaceholderCompletionString);
  } else {
    mPlaceholderCompletionString.Truncate();
  }
}

nsresult
nsAutoCompleteController::StartSearches()
{
  // Don't create a new search timer if we're already waiting for one to fire.
  // If we don't check for this, we won't be able to cancel the original timer
  // and may crash when it fires (bug 236659).
  if (mTimer || !mInput)
    return NS_OK;

  // Check if the current input should be completed with the placeholder string
  // from the last completion until the actual search results come back.
  MaybeCompletePlaceholder();

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  // Get the timeout for delayed searches.
  uint32_t timeout;
  input->GetTimeout(&timeout);

  uint32_t immediateSearchesCount = mImmediateSearchesCount;
  if (timeout == 0) {
    // All the searches should be executed immediately.
    immediateSearchesCount = mSearches.Length();
  }

  if (immediateSearchesCount > 0) {
    nsresult rv = BeforeSearches();
    if (NS_FAILED(rv))
      return rv;
    StartSearch(nsIAutoCompleteSearchDescriptor::SEARCH_TYPE_IMMEDIATE);

    if (mSearches.Length() == immediateSearchesCount) {
      // Either all searches are immediate, or the timeout is 0.  In the
      // latter case we still have to execute the delayed searches, otherwise
      // this will be a no-op.
      StartSearch(nsIAutoCompleteSearchDescriptor::SEARCH_TYPE_DELAYED);

      // All the searches have been started, just finish.
      AfterSearches();
      return NS_OK;
    }
  }

  MOZ_ASSERT(timeout > 0, "Trying to delay searches with a 0 timeout!");

  // Now start the delayed searches.
  nsresult rv;
  mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv))
      return rv;
  rv = mTimer->InitWithCallback(this, timeout, nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv))
      mTimer = nullptr;

  return rv;
}

nsresult
nsAutoCompleteController::ClearSearchTimer()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
  return NS_OK;
}

nsresult
nsAutoCompleteController::EnterMatch(bool aIsPopupSelection)
{
  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  nsCOMPtr<nsIAutoCompletePopup> popup;
  input->GetPopup(getter_AddRefs(popup));
  NS_ENSURE_TRUE(popup != nullptr, NS_ERROR_FAILURE);

  bool forceComplete;
  input->GetForceComplete(&forceComplete);

  // Ask the popup if it wants to enter a special value into the textbox
  nsAutoString value;
  popup->GetOverrideValue(value);
  if (value.IsEmpty()) {
    bool shouldComplete;
    input->GetCompleteDefaultIndex(&shouldComplete);
    bool completeSelection;
    input->GetCompleteSelectedIndex(&completeSelection);

    int32_t selectedIndex;
    popup->GetSelectedIndex(&selectedIndex);
    if (selectedIndex >= 0) {

      nsAutoString inputValue;
      input->GetTextValue(inputValue);
      nsAutoString finalValue;
      if (!completeSelection || aIsPopupSelection ||
          (mDefaultIndexCompleted &&
           inputValue.Equals(mPlaceholderCompletionString,
                             nsCaseInsensitiveStringComparator()))) {
        // We need to fill-in the value if:
        //  * completeselectedindex is false
        //  * A row in the popup was confirmed
        //  * The default index completion was confirmed
        GetResultValueAt(selectedIndex, true, finalValue);
        value = finalValue;
      } else if (mCompletedSelectionIndex != -1) {
        // If completeselectedindex is true, and EnterMatch was not invoked by
        // mouse-clicking a match (for example the user pressed Enter),
        // don't fill in the value as it will have already been filled in as
        // needed, unless the selected match has a final complete value that
        // differs from the user-facing value.
        GetResultValueAt(mCompletedSelectionIndex, true, finalValue);
        nsAutoString completedValue;
        GetResultValueAt(mCompletedSelectionIndex, false, completedValue);
        if (completedValue.Equals(inputValue) && !finalValue.Equals(inputValue)) {
          value = finalValue;
        }
        // Note that if the user opens the popup, mouses over entries without
        // ever selecting one with the keyboard, and then hits enter, none of
        // the above cases will be hit, since mouseover doesn't activate
        // completeselectedindex and thus mCompletedSelectionIndex would be
        // -1.
      }
    } else if (shouldComplete) {
      // We usually try to preserve the casing of what user has typed, but
      // if he wants to autocomplete, we will replace the value with the
      // actual autocomplete result.
      // The user wants explicitely to use that result, so this ensures
      // association of the result with the autocompleted text.
      nsAutoString defaultIndexValue;
      if (NS_SUCCEEDED(GetFinalDefaultCompleteValue(defaultIndexValue)))
        value = defaultIndexValue;
    }

    if (forceComplete && value.IsEmpty() && shouldComplete) {
      // See if inputValue is one of the autocomplete results. It can be an
      // identical value, or if it matched the middle of a result it can be
      // something like "bar >> foobar" (user entered bar and foobar is
      // the result value).
      // If the current search matches one of the autocomplete results, we
      // should use that result, and not overwrite it with the default value.
      // It's indeed possible EnterMatch gets called a second time (for example
      // by the blur handler) and it should not overwrite the current match.
      nsAutoString inputValue;
      input->GetTextValue(inputValue);
      nsAutoString suggestedValue;
      int32_t pos = inputValue.Find(" >> ");
      if (pos > 0) {
        inputValue.Right(suggestedValue, inputValue.Length() - pos - 4);
      } else {
        suggestedValue = inputValue;
      }

      for (uint32_t i = 0; i < mResults.Length(); ++i) {
        nsIAutoCompleteResult *result = mResults[i];
        if (result) {
          uint32_t matchCount = 0;
          result->GetMatchCount(&matchCount);
          for (uint32_t j = 0; j < matchCount; ++j) {
            nsAutoString matchValue;
            result->GetValueAt(j, matchValue);
            if (suggestedValue.Equals(matchValue, nsCaseInsensitiveStringComparator())) {
              nsAutoString finalMatchValue;
              result->GetFinalCompleteValueAt(j, finalMatchValue);
              value = finalMatchValue;
              break;
            }
          }
        }
      }
      // The value should have been set at this point. If not, then it's not
      // a value that should be autocompleted.
    }
    else if (forceComplete && value.IsEmpty() && completeSelection) {
      // Since nothing was selected, and forceComplete is specified, that means
      // we have to find the first default match and enter it instead.
      for (uint32_t i = 0; i < mResults.Length(); ++i) {
        nsIAutoCompleteResult *result = mResults[i];
        if (result) {
          int32_t defaultIndex;
          result->GetDefaultIndex(&defaultIndex);
          if (defaultIndex >= 0) {
            result->GetFinalCompleteValueAt(defaultIndex, value);
            break;
          }
        }
      }
    }
  }

  nsCOMPtr<nsIObserverService> obsSvc =
    mozilla::services::GetObserverService();
  NS_ENSURE_STATE(obsSvc);
  obsSvc->NotifyObservers(input, "autocomplete-will-enter-text", nullptr);

  if (!value.IsEmpty()) {
    input->SetTextValue(value);
    input->SelectTextRange(value.Length(), value.Length());
    mSearchString = value;
  }

  obsSvc->NotifyObservers(input, "autocomplete-did-enter-text", nullptr);
  ClosePopup();

  bool cancel;
  input->OnTextEntered(&cancel);

  return NS_OK;
}

nsresult
nsAutoCompleteController::RevertTextValue()
{
  // StopSearch() can call PostSearchCleanup() which might result
  // in a blur event, which could null out mInput, so we need to check it
  // again.  See bug #408463 for more details
  if (!mInput)
    return NS_OK;

  nsAutoString oldValue(mSearchString);
  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  bool cancel = false;
  input->OnTextReverted(&cancel);

  if (!cancel) {
    nsCOMPtr<nsIObserverService> obsSvc =
      mozilla::services::GetObserverService();
    NS_ENSURE_STATE(obsSvc);
    obsSvc->NotifyObservers(input, "autocomplete-will-revert-text", nullptr);

    nsAutoString inputValue;
    input->GetTextValue(inputValue);
    // Don't change the value if it is the same to prevent sending useless events.
    // NOTE: how can |RevertTextValue| be called with inputValue != oldValue?
    if (!oldValue.Equals(inputValue)) {
      input->SetTextValue(oldValue);
    }

    obsSvc->NotifyObservers(input, "autocomplete-did-revert-text", nullptr);
  }

  return NS_OK;
}

nsresult
nsAutoCompleteController::ProcessResult(int32_t aSearchIndex, nsIAutoCompleteResult *aResult)
{
  NS_ENSURE_STATE(mInput);
  MOZ_ASSERT(aResult, "ProcessResult should always receive a result");
  NS_ENSURE_ARG(aResult);
  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  uint16_t searchResult = 0;
  aResult->GetSearchResult(&searchResult);

  // The following code supports incremental updating results in 2 ways:
  //  * The search may reuse the same result, just by adding entries to it.
  //  * The search may send a new result every time.  In this case we merge
  //    the results and proceed on the same code path as before.
  // This way both mSearches and mResults can be indexed by the search index,
  // cause we'll always have only one result per search.
  if (mResults.IndexOf(aResult) == -1) {
    nsIAutoCompleteResult* oldResult = mResults.SafeObjectAt(aSearchIndex);
    if (oldResult) {
      MOZ_ASSERT(false, "Passing new matches to OnSearchResult with a new "
                        "nsIAutoCompleteResult every time is deprecated, please "
                        "update the same result until the search is done");
      // Build a new nsIAutocompleteSimpleResult and merge results into it.
      RefPtr<nsAutoCompleteSimpleResult> mergedResult =
        new nsAutoCompleteSimpleResult();
      mergedResult->AppendResult(oldResult);
      mergedResult->AppendResult(aResult);
      mResults.ReplaceObjectAt(mergedResult, aSearchIndex);
    } else {
      // This inserts and grows the array if needed.
      mResults.ReplaceObjectAt(aResult, aSearchIndex);
    }
  }
  // When found the result should have the same index as the search.
  MOZ_ASSERT_IF(mResults.IndexOf(aResult) != -1,
                mResults.IndexOf(aResult) == aSearchIndex);
  MOZ_ASSERT(mResults.Count() >= aSearchIndex + 1,
             "aSearchIndex should always be valid for mResults");

  bool isTypeAheadResult = false;
  aResult->GetTypeAheadResult(&isTypeAheadResult);

  if (!isTypeAheadResult) {
    uint32_t oldRowCount = mRowCount;
    // If the search failed, increase the match count to include the error
    // description.
    if (searchResult == nsIAutoCompleteResult::RESULT_FAILURE) {
      nsAutoString error;
      aResult->GetErrorDescription(error);
      if (!error.IsEmpty()) {
        ++mRowCount;
        if (mTree) {
          mTree->RowCountChanged(oldRowCount, 1);
        }
      }
    } else if (searchResult == nsIAutoCompleteResult::RESULT_SUCCESS ||
               searchResult == nsIAutoCompleteResult::RESULT_SUCCESS_ONGOING) {
      // Increase the match count for all matches in this result.
      uint32_t totalMatchCount = 0;
      for (uint32_t i = 0; i < mResults.Length(); i++) {
        nsIAutoCompleteResult* result = mResults.SafeObjectAt(i);
        if (result) {
          // not all results implement this, so it can likely fail.
          bool typeAhead = false;
          result->GetTypeAheadResult(&typeAhead);
          if (!typeAhead) {
            uint32_t matchCount = 0;
            result->GetMatchCount(&matchCount);
            totalMatchCount += matchCount;
          }
        }
      }
      uint32_t delta = totalMatchCount - oldRowCount;

      mRowCount += delta;
      if (mTree) {
        mTree->RowCountChanged(oldRowCount, delta);
      }
    }

    // Refresh the popup view to display the new search results
    nsCOMPtr<nsIAutoCompletePopup> popup;
    input->GetPopup(getter_AddRefs(popup));
    NS_ENSURE_TRUE(popup != nullptr, NS_ERROR_FAILURE);
    popup->Invalidate();

    uint32_t minResults;
    input->GetMinResultsForPopup(&minResults);

    // Make sure the popup is open, if necessary, since we now have at least one
    // search result ready to display. Don't force the popup closed if we might
    // get results in the future to avoid unnecessarily canceling searches.
    if (mRowCount || !minResults) {
      OpenPopup();
    } else if (mSearchesOngoing == 0) {
      ClosePopup();
    }
  }

  if (searchResult == nsIAutoCompleteResult::RESULT_SUCCESS ||
      searchResult == nsIAutoCompleteResult::RESULT_SUCCESS_ONGOING) {
    // Try to autocomplete the default index for this search.
    CompleteDefaultIndex(aSearchIndex);
  }

  return NS_OK;
}

nsresult
nsAutoCompleteController::PostSearchCleanup()
{
  NS_ENSURE_STATE(mInput);
  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  uint32_t minResults;
  input->GetMinResultsForPopup(&minResults);

  if (mRowCount || minResults == 0) {
    OpenPopup();
    if (mRowCount)
      mSearchStatus = nsIAutoCompleteController::STATUS_COMPLETE_MATCH;
    else
      mSearchStatus = nsIAutoCompleteController::STATUS_COMPLETE_NO_MATCH;
  } else {
    mSearchStatus = nsIAutoCompleteController::STATUS_COMPLETE_NO_MATCH;
    ClosePopup();
  }

  // notify the input that the search is complete
  input->OnSearchComplete();

  return NS_OK;
}

nsresult
nsAutoCompleteController::ClearResults()
{
  int32_t oldRowCount = mRowCount;
  mRowCount = 0;
  mResults.Clear();
  if (oldRowCount != 0) {
    if (mTree)
      mTree->RowCountChanged(0, -oldRowCount);
    else if (mInput) {
      nsCOMPtr<nsIAutoCompletePopup> popup;
      mInput->GetPopup(getter_AddRefs(popup));
      NS_ENSURE_TRUE(popup != nullptr, NS_ERROR_FAILURE);
      // if we had a tree, RowCountChanged() would have cleared the selection
      // when the selected row was removed.  But since we don't have a tree,
      // we need to clear the selection manually.
      popup->SetSelectedIndex(-1);
    }
  }
  return NS_OK;
}

nsresult
nsAutoCompleteController::CompleteDefaultIndex(int32_t aResultIndex)
{
  if (mDefaultIndexCompleted || mProhibitAutoFill || mSearchString.Length() == 0 || !mInput)
    return NS_OK;

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  int32_t selectionStart;
  input->GetSelectionStart(&selectionStart);
  int32_t selectionEnd;
  input->GetSelectionEnd(&selectionEnd);

  bool isPlaceholderSelected =
      selectionEnd == (int32_t)mPlaceholderCompletionString.Length() &&
      selectionStart == (int32_t)mSearchString.Length() &&
      StringBeginsWith(mPlaceholderCompletionString,
        mSearchString, nsCaseInsensitiveStringComparator());

  // Don't try to automatically complete to the first result if there's already
  // a selection or the cursor isn't at the end of the input. In case the
  // selection is from the current placeholder completion value, then still
  // automatically complete.
  if (!isPlaceholderSelected && (selectionEnd != selectionStart ||
        selectionEnd != (int32_t)mSearchString.Length()))
    return NS_OK;

  bool shouldComplete;
  input->GetCompleteDefaultIndex(&shouldComplete);
  if (!shouldComplete)
    return NS_OK;

  nsAutoString resultValue;
  if (NS_SUCCEEDED(GetDefaultCompleteValue(aResultIndex, true, resultValue)))
    CompleteValue(resultValue);

  mDefaultIndexCompleted = true;

  return NS_OK;
}

nsresult
nsAutoCompleteController::GetDefaultCompleteResult(int32_t aResultIndex,
                                                   nsIAutoCompleteResult** _result,
                                                   int32_t* _defaultIndex)
{
  *_defaultIndex = -1;
  int32_t resultIndex = aResultIndex;

  // If a result index was not provided, find the first defaultIndex result.
  for (int32_t i = 0; resultIndex < 0 && i < mResults.Count(); ++i) {
    nsIAutoCompleteResult *result = mResults.SafeObjectAt(i);
    if (result &&
        NS_SUCCEEDED(result->GetDefaultIndex(_defaultIndex)) &&
        *_defaultIndex >= 0) {
      resultIndex = i;
    }
  }
  if (resultIndex < 0) {
    return NS_ERROR_FAILURE;
  }

  *_result = mResults.SafeObjectAt(resultIndex);
  NS_ENSURE_TRUE(*_result, NS_ERROR_FAILURE);

  if (*_defaultIndex < 0) {
    // The search must explicitly provide a default index in order
    // for us to be able to complete.
    (*_result)->GetDefaultIndex(_defaultIndex);
  }

  if (*_defaultIndex < 0) {
    // We were given a result index, but that result doesn't want to
    // be autocompleted.
    return NS_ERROR_FAILURE;
  }

  // If the result wrongly notifies a RESULT_SUCCESS with no matches, or
  // provides a defaultIndex greater than its matchCount, avoid trying to
  // complete to an empty value.
  uint32_t matchCount = 0;
  (*_result)->GetMatchCount(&matchCount);
  // Here defaultIndex is surely non-negative, so can be cast to unsigned.
  if ((uint32_t)(*_defaultIndex) >= matchCount) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsAutoCompleteController::GetDefaultCompleteValue(int32_t aResultIndex,
                                                  bool aPreserveCasing,
                                                  nsAString &_retval)
{
  nsIAutoCompleteResult *result;
  int32_t defaultIndex = -1;
  nsresult rv = GetDefaultCompleteResult(aResultIndex, &result, &defaultIndex);
  if (NS_FAILED(rv)) return rv;

  nsAutoString resultValue;
  result->GetValueAt(defaultIndex, resultValue);
  if (aPreserveCasing &&
      StringBeginsWith(resultValue, mSearchString,
                       nsCaseInsensitiveStringComparator())) {
    // We try to preserve user casing, otherwise we would end up changing
    // the case of what he typed, if we have a result with a different casing.
    // For example if we have result "Test", and user starts writing "tuna",
    // after digiting t, we would convert it to T trying to autocomplete "Test".
    // We will still complete to cased "Test" if the user explicitely choose
    // that result, by either selecting it in the results popup, or with
    // keyboard navigation or if autocompleting in the middle.
    nsAutoString casedResultValue;
    casedResultValue.Assign(mSearchString);
    // Use what the user has typed so far.
    casedResultValue.Append(Substring(resultValue,
                                      mSearchString.Length(),
                                      resultValue.Length()));
    _retval = casedResultValue;
  }
  else
    _retval = resultValue;

  return NS_OK;
}

nsresult
nsAutoCompleteController::GetFinalDefaultCompleteValue(nsAString &_retval)
{
  MOZ_ASSERT(mInput, "Must have a valid input");
  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  nsIAutoCompleteResult *result;
  int32_t defaultIndex = -1;
  nsresult rv = GetDefaultCompleteResult(-1, &result, &defaultIndex);
  if (NS_FAILED(rv)) return rv;

  result->GetValueAt(defaultIndex, _retval);
  nsAutoString inputValue;
  input->GetTextValue(inputValue);
  if (!_retval.Equals(inputValue, nsCaseInsensitiveStringComparator())) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString finalCompleteValue;
  rv = result->GetFinalCompleteValueAt(defaultIndex, finalCompleteValue);
  if (NS_SUCCEEDED(rv)) {
    _retval = finalCompleteValue;
  }

  return NS_OK;
}

nsresult
nsAutoCompleteController::CompleteValue(nsString &aValue)
/* mInput contains mSearchString, which we want to autocomplete to aValue.  If
 * selectDifference is true, select the remaining portion of aValue not
 * contained in mSearchString. */
{
  MOZ_ASSERT(mInput, "Must have a valid input");

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  const int32_t mSearchStringLength = mSearchString.Length();
  int32_t endSelect = aValue.Length();  // By default, select all of aValue.

  if (aValue.IsEmpty() ||
      StringBeginsWith(aValue, mSearchString,
                       nsCaseInsensitiveStringComparator())) {
    // aValue is empty (we were asked to clear mInput), or mSearchString
    // matches the beginning of aValue.  In either case we can simply
    // autocomplete to aValue.
    mPlaceholderCompletionString = aValue;
    input->SetTextValue(aValue);
  } else {
    nsresult rv;
    nsCOMPtr<nsIIOService> ios = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsAutoCString scheme;
    if (NS_SUCCEEDED(ios->ExtractScheme(NS_ConvertUTF16toUTF8(aValue), scheme))) {
      // Trying to autocomplete a URI from somewhere other than the beginning.
      // Only succeed if the missing portion is "http://"; otherwise do not
      // autocomplete.  This prevents us from "helpfully" autocompleting to a
      // URI that isn't equivalent to what the user expected.
      const int32_t findIndex = 7; // length of "http://"

      if ((endSelect < findIndex + mSearchStringLength) ||
          !scheme.LowerCaseEqualsLiteral("http") ||
          !Substring(aValue, findIndex, mSearchStringLength).Equals(
            mSearchString, nsCaseInsensitiveStringComparator())) {
        return NS_OK;
      }

      mPlaceholderCompletionString = mSearchString +
        Substring(aValue, mSearchStringLength + findIndex, endSelect);
      input->SetTextValue(mPlaceholderCompletionString);

      endSelect -= findIndex; // We're skipping this many characters of aValue.
    } else {
      // Autocompleting something other than a URI from the middle.
      // Use the format "searchstring >> full string" to indicate to the user
      // what we are going to replace their search string with.
      input->SetTextValue(mSearchString + NS_LITERAL_STRING(" >> ") + aValue);

      endSelect = mSearchString.Length() + 4 + aValue.Length();

      // Reset the last search completion.
      mPlaceholderCompletionString.Truncate();
    }
  }

  input->SelectTextRange(mSearchStringLength, endSelect);

  return NS_OK;
}

nsresult
nsAutoCompleteController::GetResultLabelAt(int32_t aIndex, nsAString & _retval)
{
  return GetResultValueLabelAt(aIndex, false, false, _retval);
}

nsresult
nsAutoCompleteController::GetResultValueAt(int32_t aIndex, bool aGetFinalValue,
                                           nsAString & _retval)
{
  return GetResultValueLabelAt(aIndex, aGetFinalValue, true, _retval);
}

nsresult
nsAutoCompleteController::GetResultValueLabelAt(int32_t aIndex,
                                                bool aGetFinalValue,
                                                bool aGetValue,
                                                nsAString & _retval)
{
  NS_ENSURE_TRUE(aIndex >= 0 && (uint32_t) aIndex < mRowCount, NS_ERROR_ILLEGAL_VALUE);

  int32_t rowIndex;
  nsIAutoCompleteResult *result;
  nsresult rv = GetResultAt(aIndex, &result, &rowIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  uint16_t searchResult;
  result->GetSearchResult(&searchResult);

  if (searchResult == nsIAutoCompleteResult::RESULT_FAILURE) {
    if (aGetValue)
      return NS_ERROR_FAILURE;
    result->GetErrorDescription(_retval);
  } else if (searchResult == nsIAutoCompleteResult::RESULT_SUCCESS ||
             searchResult == nsIAutoCompleteResult::RESULT_SUCCESS_ONGOING) {
    if (aGetFinalValue) {
      // Some implementations may miss finalCompleteValue, try to be backwards
      // compatible.
      if (NS_FAILED(result->GetFinalCompleteValueAt(rowIndex, _retval))) {
        result->GetValueAt(rowIndex, _retval);
      }
    } else if (aGetValue) {
      result->GetValueAt(rowIndex, _retval);
    } else {
      result->GetLabelAt(rowIndex, _retval);
    }
  }

  return NS_OK;
}

/**
 * Given the index of a row in the autocomplete popup, find the
 * corresponding nsIAutoCompleteSearch index, and sub-index into
 * the search's results list.
 */
nsresult
nsAutoCompleteController::RowIndexToSearch(int32_t aRowIndex, int32_t *aSearchIndex, int32_t *aItemIndex)
{
  *aSearchIndex = -1;
  *aItemIndex = -1;

  uint32_t index = 0;

  // Move index through the results of each registered nsIAutoCompleteSearch
  // until we find the given row
  for (uint32_t i = 0; i < mSearches.Length(); ++i) {
    nsIAutoCompleteResult *result = mResults.SafeObjectAt(i);
    if (!result)
      continue;

    uint32_t rowCount = 0;

    // Skip past the result completely if it is marked as hidden
    bool isTypeAheadResult = false;
    result->GetTypeAheadResult(&isTypeAheadResult);

    if (!isTypeAheadResult) {
      uint16_t searchResult;
      result->GetSearchResult(&searchResult);

      // Find out how many results were provided by the
      // current nsIAutoCompleteSearch.
      if (searchResult == nsIAutoCompleteResult::RESULT_SUCCESS ||
          searchResult == nsIAutoCompleteResult::RESULT_SUCCESS_ONGOING) {
        result->GetMatchCount(&rowCount);
      }
    }

    // If the given row index is within the results range
    // of the current nsIAutoCompleteSearch then return the
    // search index and sub-index into the results array
    if ((rowCount != 0) && (index + rowCount-1 >= (uint32_t) aRowIndex)) {
      *aSearchIndex = i;
      *aItemIndex = aRowIndex - index;
      return NS_OK;
    }

    // Advance the popup table index cursor past the
    // results of the current search.
    index += rowCount;
  }

  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAutoCompleteController)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAutoCompleteSimpleResult)

NS_DEFINE_NAMED_CID(NS_AUTOCOMPLETECONTROLLER_CID);
NS_DEFINE_NAMED_CID(NS_AUTOCOMPLETESIMPLERESULT_CID);

static const mozilla::Module::CIDEntry kAutoCompleteCIDs[] = {
  { &kNS_AUTOCOMPLETECONTROLLER_CID, false, nullptr, nsAutoCompleteControllerConstructor },
  { &kNS_AUTOCOMPLETESIMPLERESULT_CID, false, nullptr, nsAutoCompleteSimpleResultConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kAutoCompleteContracts[] = {
  { NS_AUTOCOMPLETECONTROLLER_CONTRACTID, &kNS_AUTOCOMPLETECONTROLLER_CID },
  { NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID, &kNS_AUTOCOMPLETESIMPLERESULT_CID },
  { nullptr }
};

static const mozilla::Module kAutoCompleteModule = {
  mozilla::Module::kVersion,
  kAutoCompleteCIDs,
  kAutoCompleteContracts
};

NSMODULE_DEFN(tkAutoCompleteModule) = &kAutoCompleteModule;
