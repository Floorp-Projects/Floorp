/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoCompleteController.h"
#include "nsAutoCompleteSimpleResult.h"

#include "nsNetCID.h"
#include "nsIIOService.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "mozilla/dom/Event.h"

static const char* kAutoCompleteSearchCID =
    "@mozilla.org/autocomplete/search;1?name=";

using namespace mozilla;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsAutoCompleteController)

MOZ_CAN_RUN_SCRIPT_BOUNDARY
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsAutoCompleteController)
  MOZ_KnownLive(tmp)->SetInput(nullptr);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsAutoCompleteController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInput)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSearches)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResults)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResultCache)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsAutoCompleteController)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsAutoCompleteController)
NS_INTERFACE_TABLE_HEAD(nsAutoCompleteController)
  NS_INTERFACE_TABLE(nsAutoCompleteController, nsIAutoCompleteController,
                     nsIAutoCompleteObserver, nsITimerCallback, nsINamed)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsAutoCompleteController)
NS_INTERFACE_MAP_END

nsAutoCompleteController::nsAutoCompleteController()
    : mDefaultIndexCompleted(false),
      mPopupClosedByCompositionStart(false),
      mProhibitAutoFill(false),
      mUserClearedAutoFill(false),
      mClearingAutoFillSearchesAgain(false),
      mCompositionState(eCompositionState_None),
      mSearchStatus(nsAutoCompleteController::STATUS_NONE),
      mMatchCount(0),
      mSearchesOngoing(0),
      mSearchesFailed(0),
      mImmediateSearchesCount(0),
      mCompletedSelectionIndex(-1) {}

nsAutoCompleteController::~nsAutoCompleteController() { SetInput(nullptr); }

void nsAutoCompleteController::SetValueOfInputTo(const nsString& aValue,
                                                 uint16_t aReason) {
  mSetValue = aValue;
  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  nsresult rv = input->SetTextValueWithReason(aValue, aReason);
  if (NS_FAILED(rv)) {
    input->SetTextValue(aValue);
  }
}

////////////////////////////////////////////////////////////////////////
//// nsIAutoCompleteController

NS_IMETHODIMP
nsAutoCompleteController::GetSearchStatus(uint16_t* aSearchStatus) {
  *aSearchStatus = mSearchStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetMatchCount(uint32_t* aMatchCount) {
  *aMatchCount = mMatchCount;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetInput(nsIAutoCompleteInput** aInput) {
  *aInput = mInput;
  NS_IF_ADDREF(*aInput);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::SetInitiallySelectedIndex(int32_t aSelectedIndex) {
  // First forward to the popup.
  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  NS_ENSURE_STATE(input);

  nsCOMPtr<nsIAutoCompletePopup> popup(GetPopup());
  NS_ENSURE_STATE(popup);
  popup->SetSelectedIndex(aSelectedIndex);

  // Now take care of internal stuff.
  bool completeSelection;
  if (NS_SUCCEEDED(input->GetCompleteSelectedIndex(&completeSelection)) &&
      completeSelection) {
    mCompletedSelectionIndex = aSelectedIndex;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::SetInput(nsIAutoCompleteInput* aInput) {
  // Don't do anything if the input isn't changing.
  if (mInput == aInput) return NS_OK;

  Unused << ResetInternalState();
  if (mInput) {
    mSearches.Clear();
    ClosePopup();
  }

  mInput = aInput;

  // Nothing more to do if the input was just being set to null.
  if (!mInput) {
    return NS_OK;
  }
  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  // Reset the current search string.
  nsAutoString value;
  input->GetTextValue(value);
  SetSearchStringInternal(value);

  // Since the controller can be used as a service it's important to reset this.
  mClearingAutoFillSearchesAgain = false;

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::ResetInternalState() {
  // Clear out the current search context
  if (mInput) {
    nsAutoString value;
    mInput->GetTextValue(value);
    // Stop all searches in case they are async.
    Unused << StopSearch();
    Unused << ClearResults();
    SetSearchStringInternal(value);
  }

  mPlaceholderCompletionString.Truncate();
  mDefaultIndexCompleted = false;
  mProhibitAutoFill = false;
  mSearchStatus = nsIAutoCompleteController::STATUS_NONE;
  mMatchCount = 0;
  mCompletedSelectionIndex = -1;

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::StartSearch(const nsAString& aSearchString) {
  // If composition is ongoing don't start searching yet, until it is committed.
  if (mCompositionState == eCompositionState_Composing) {
    return NS_OK;
  }

  SetSearchStringInternal(aSearchString);
  StartSearches();
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleText(bool* _retval) {
  *_retval = false;
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
    NS_ERROR(
        "Called before attaching to the control or after detaching from the "
        "control");
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

  // Usually we don't search again if the new string is the same as the last
  // one. However, if this is called immediately after compositionend event, we
  // need to search the same value again since the search was canceled at
  // compositionstart event handler. The new string might also be the same as
  // the last search if the autofilled portion was cleared. In this case, we may
  // want to search again.

  // Whether the user removed some text at the end.
  bool userRemovedText =
      newValue.Length() < mSearchString.Length() &&
      Substring(mSearchString, 0, newValue.Length()).Equals(newValue);

  // Whether the user is repeating the previous search.
  bool repeatingPreviousSearch =
      !userRemovedText && newValue.Equals(mSearchString);

  mUserClearedAutoFill =
      repeatingPreviousSearch &&
      newValue.Length() < mPlaceholderCompletionString.Length() &&
      Substring(mPlaceholderCompletionString, 0, newValue.Length())
          .Equals(newValue);
  bool searchAgainOnAutoFillClear =
      mUserClearedAutoFill && mClearingAutoFillSearchesAgain;

  if (!handlingCompositionCommit && !searchAgainOnAutoFillClear &&
      newValue.Length() > 0 && repeatingPreviousSearch) {
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

  SetSearchStringInternal(newValue);

  bool noRollupOnEmptySearch;
  nsresult rv = input->GetNoRollupOnEmptySearch(&noRollupOnEmptySearch);
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't search if the value is empty
  if (newValue.Length() == 0 && !noRollupOnEmptySearch) {
    // If autocomplete popup was closed by compositionstart event handler,
    // we should reopen it forcibly even if the value is empty.
    if (popupClosedByCompositionStart && handlingCompositionCommit) {
      bool cancel;
      HandleKeyNavigation(dom::KeyboardEvent_Binding::DOM_VK_DOWN, &cancel);
      return NS_OK;
    }
    ClosePopup();
    return NS_OK;
  }

  *_retval = true;
  StartSearches();

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleEnter(bool aIsPopupSelection,
                                      dom::Event* aEvent, bool* _retval) {
  *_retval = false;
  if (!mInput) return NS_OK;

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  // allow the event through unless there is something selected in the popup
  input->GetPopupOpen(_retval);
  if (*_retval) {
    nsCOMPtr<nsIAutoCompletePopup> popup(GetPopup());
    if (popup) {
      int32_t selectedIndex;
      popup->GetSelectedIndex(&selectedIndex);
      *_retval = selectedIndex >= 0;
    }
  }

  // Stop the search, and handle the enter.
  StopSearch();
  // StopSearch() can call PostSearchCleanup() which might result
  // in a blur event, which could null out mInput, so we need to check it
  // again.  See bug #408463 for more details
  if (!mInput) {
    return NS_OK;
  }

  EnterMatch(aIsPopupSelection, aEvent);

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleEscape(bool* _retval) {
  *_retval = false;
  if (!mInput) return NS_OK;

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
nsAutoCompleteController::HandleStartComposition() {
  NS_ENSURE_TRUE(mCompositionState != eCompositionState_Composing, NS_OK);

  mPopupClosedByCompositionStart = false;
  mCompositionState = eCompositionState_Composing;

  if (!mInput) return NS_OK;

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  bool disabled;
  input->GetDisableAutoComplete(&disabled);
  if (disabled) return NS_OK;

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
nsAutoCompleteController::HandleEndComposition() {
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
nsAutoCompleteController::HandleTab() {
  bool cancel;
  return HandleEnter(false, nullptr, &cancel);
}

NS_IMETHODIMP
nsAutoCompleteController::HandleKeyNavigation(uint32_t aKey, bool* _retval) {
  // By default, don't cancel the event
  *_retval = false;

  if (!mInput) {
    // Stop all searches in case they are async.
    StopSearch();
    // Note: if now is after blur and IME end composition,
    // check mInput before calling.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=193544#c31
    NS_ERROR(
        "Called before attaching to the control or after detaching from the "
        "control");
    return NS_OK;
  }

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  nsCOMPtr<nsIAutoCompletePopup> popup(GetPopup());
  NS_ENSURE_TRUE(popup != nullptr, NS_ERROR_FAILURE);

  bool disabled;
  input->GetDisableAutoComplete(&disabled);
  NS_ENSURE_TRUE(!disabled, NS_OK);

  if (aKey == dom::KeyboardEvent_Binding::DOM_VK_UP ||
      aKey == dom::KeyboardEvent_Binding::DOM_VK_DOWN ||
      aKey == dom::KeyboardEvent_Binding::DOM_VK_PAGE_UP ||
      aKey == dom::KeyboardEvent_Binding::DOM_VK_PAGE_DOWN) {
    bool isOpen = false;
    input->GetPopupOpen(&isOpen);
    if (isOpen) {
      // Prevent the input from handling up/down events, as it may move
      // the cursor to home/end on some systems
      *_retval = true;
      bool reverse = aKey == dom::KeyboardEvent_Binding::DOM_VK_UP ||
                     aKey == dom::KeyboardEvent_Binding::DOM_VK_PAGE_UP;
      bool page = aKey == dom::KeyboardEvent_Binding::DOM_VK_PAGE_UP ||
                  aKey == dom::KeyboardEvent_Binding::DOM_VK_PAGE_DOWN;

      // Fill in the value of the textbox with whatever is selected in the popup
      // if the completeSelectedIndex attribute is set.  We check this before
      // calling SelectBy of an earlier attempt to avoid crashing.
      bool completeSelection;
      input->GetCompleteSelectedIndex(&completeSelection);

      // The user has keyed up or down to change the selection.  Stop the search
      // (if there is one) now so that the results do not change while the user
      // is making a selection.
      Unused << StopSearch();

      // Instruct the result view to scroll by the given amount and direction
      popup->SelectBy(reverse, page);

      if (completeSelection) {
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
              SetValueOfInputTo(
                  value,
                  nsIAutoCompleteInput::TEXTVALUE_REASON_COMPLETEDEFAULT);
            } else {
              start = value.Length();
              SetValueOfInputTo(
                  value,
                  nsIAutoCompleteInput::TEXTVALUE_REASON_COMPLETESELECTED);
            }

            input->SelectTextRange(start, value.Length());
          }
          mCompletedSelectionIndex = selectedIndex;
        } else {
          // Nothing is selected, so fill in the last typed value
          SetValueOfInputTo(mSearchString,
                            nsIAutoCompleteInput::TEXTVALUE_REASON_REVERT);
          input->SelectTextRange(mSearchString.Length(),
                                 mSearchString.Length());
          mCompletedSelectionIndex = -1;
        }
      }
    } else {
      // Only show the popup if the caret is at the start or end of the input
      // and there is no selection, so that the default defined key shortcuts
      // for up and down move to the beginning and end of the field otherwise.
      if (aKey == dom::KeyboardEvent_Binding::DOM_VK_UP ||
          aKey == dom::KeyboardEvent_Binding::DOM_VK_DOWN) {
        const bool isUp = aKey == dom::KeyboardEvent_Binding::DOM_VK_UP;

        int32_t start, end;
        input->GetSelectionStart(&start);
        input->GetSelectionEnd(&end);

        if (isUp) {
          if (start > 0 || start != end) {
            return NS_OK;
          }
        } else {
          nsAutoString text;
          input->GetTextValue(text);
          if (start != end || end < (int32_t)text.Length()) {
            return NS_OK;
          }
        }
      }

      nsAutoString oldSearchString;
      uint16_t oldResult = 0;

      // Open the popup if there has been a previous non-errored search, or
      // else kick off a new search
      if (!mResults.IsEmpty() &&
          NS_SUCCEEDED(mResults[0]->GetSearchResult(&oldResult)) &&
          oldResult != nsIAutoCompleteResult::RESULT_FAILURE &&
          NS_SUCCEEDED(mResults[0]->GetSearchString(oldSearchString)) &&
          oldSearchString.Equals(mSearchString,
                                 nsCaseInsensitiveStringComparator())) {
        if (mMatchCount) {
          OpenPopup();
        }
      } else {
        // Stop all searches in case they are async.
        StopSearch();

        if (!mInput) {
          // StopSearch() can call PostSearchCleanup() which might result
          // in a blur event, which could null out mInput, so we need to check
          // it again.  See bug #395344 for more details
          return NS_OK;
        }

        // Some script may have changed the value of the text field since our
        // last keypress or after our focus handler and we don't want to
        // search for a stale string.
        nsAutoString value;
        input->GetTextValue(value);
        SetSearchStringInternal(value);

        StartSearches();
      }

      bool isOpen = false;
      input->GetPopupOpen(&isOpen);
      if (isOpen) {
        // Prevent the default action if we opened the popup in any of the code
        // paths above.
        *_retval = true;
      }
    }
  } else if (aKey == dom::KeyboardEvent_Binding::DOM_VK_LEFT ||
             aKey == dom::KeyboardEvent_Binding::DOM_VK_RIGHT
#ifndef XP_MACOSX
             || aKey == dom::KeyboardEvent_Binding::DOM_VK_HOME
#endif
  ) {
    // The user hit a text-navigation key.
    bool isOpen = false;
    input->GetPopupOpen(&isOpen);

    // If minresultsforpopup > 1 and there's less matches than the minimum
    // required, the popup is not open, but the search suggestion is showing
    // inline, so we should proceed as if we had the popup.
    uint32_t minResultsForPopup;
    input->GetMinResultsForPopup(&minResultsForPopup);
    if (isOpen || (mMatchCount > 0 && mMatchCount < minResultsForPopup)) {
      // For completeSelectedIndex autocomplete fields, if the popup shouldn't
      // close when the caret is moved, don't adjust the text value or caret
      // position.
      bool completeSelection;
      input->GetCompleteSelectedIndex(&completeSelection);
      if (isOpen) {
        bool noRollup;
        input->GetNoRollupOnCaretMove(&noRollup);
        if (noRollup) {
          if (completeSelection) {
            return NS_OK;
          }
        }
      }

      int32_t selectionEnd;
      input->GetSelectionEnd(&selectionEnd);
      int32_t selectionStart;
      input->GetSelectionStart(&selectionStart);
      bool shouldCompleteSelection =
          (uint32_t)selectionEnd == mPlaceholderCompletionString.Length() &&
          selectionStart < selectionEnd;
      int32_t selectedIndex;
      popup->GetSelectedIndex(&selectedIndex);
      bool completeDefaultIndex;
      input->GetCompleteDefaultIndex(&completeDefaultIndex);
      if (completeDefaultIndex && shouldCompleteSelection) {
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

          if (value.Equals(suggestedValue,
                           nsCaseInsensitiveStringComparator())) {
            SetValueOfInputTo(
                value, nsIAutoCompleteInput::TEXTVALUE_REASON_COMPLETEDEFAULT);
            input->SelectTextRange(value.Length(), value.Length());
          }
        }
      } else if (!completeDefaultIndex && !completeSelection &&
                 selectedIndex >= 0) {
        // The pop-up is open and has a selection, take its value
        nsAutoString value;
        if (NS_SUCCEEDED(GetResultValueAt(selectedIndex, false, value))) {
          SetValueOfInputTo(
              value, nsIAutoCompleteInput::TEXTVALUE_REASON_COMPLETESELECTED);
          input->SelectTextRange(value.Length(), value.Length());
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
    SetSearchStringInternal(value);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleDelete(bool* _retval) {
  *_retval = false;
  if (!mInput) return NS_OK;

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  bool isOpen = false;
  input->GetPopupOpen(&isOpen);
  if (!isOpen || mMatchCount == 0) {
    // Nothing left to delete, proceed as normal
    bool unused = false;
    HandleText(&unused);
    return NS_OK;
  }

  nsCOMPtr<nsIAutoCompletePopup> popup(GetPopup());
  NS_ENSURE_TRUE(popup, NS_ERROR_FAILURE);

  int32_t index, searchIndex, matchIndex;
  popup->GetSelectedIndex(&index);
  if (index == -1) {
    // No match is selected in the list
    bool unused = false;
    HandleText(&unused);
    return NS_OK;
  }

  MatchIndexToSearch(index, &searchIndex, &matchIndex);
  NS_ENSURE_TRUE(searchIndex >= 0 && matchIndex >= 0, NS_ERROR_FAILURE);

  nsIAutoCompleteResult* result = mResults.SafeObjectAt(searchIndex);
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  nsAutoString search;
  input->GetSearchParam(search);

  // Clear the match in our result and in the DB.
  result->RemoveValueAt(matchIndex);
  --mMatchCount;

  // We removed it, so make sure we cancel the event that triggered this call.
  *_retval = true;

  // Unselect the current item.
  popup->SetSelectedIndex(-1);

  // Adjust index, if needed.
  MOZ_ASSERT(index >= 0);  // We verified this above, after MatchIndexToSearch.
  if (static_cast<uint32_t>(index) >= mMatchCount) index = mMatchCount - 1;

  if (mMatchCount > 0) {
    // There are still matches in the popup, select the current index again.
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
    popup->Invalidate(nsIAutoCompletePopup::INVALIDATE_REASON_DELETE);
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

nsresult nsAutoCompleteController::GetResultAt(int32_t aIndex,
                                               nsIAutoCompleteResult** aResult,
                                               int32_t* aMatchIndex) {
  int32_t searchIndex;
  MatchIndexToSearch(aIndex, &searchIndex, aMatchIndex);
  NS_ENSURE_TRUE(searchIndex >= 0 && *aMatchIndex >= 0, NS_ERROR_FAILURE);

  *aResult = mResults.SafeObjectAt(searchIndex);
  NS_ENSURE_TRUE(*aResult, NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetValueAt(int32_t aIndex, nsAString& _retval) {
  GetResultLabelAt(aIndex, _retval);

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetLabelAt(int32_t aIndex, nsAString& _retval) {
  GetResultLabelAt(aIndex, _retval);

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetCommentAt(int32_t aIndex, nsAString& _retval) {
  int32_t matchIndex;
  nsIAutoCompleteResult* result;
  nsresult rv = GetResultAt(aIndex, &result, &matchIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  return result->GetCommentAt(matchIndex, _retval);
}

NS_IMETHODIMP
nsAutoCompleteController::GetStyleAt(int32_t aIndex, nsAString& _retval) {
  int32_t matchIndex;
  nsIAutoCompleteResult* result;
  nsresult rv = GetResultAt(aIndex, &result, &matchIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  return result->GetStyleAt(matchIndex, _retval);
}

NS_IMETHODIMP
nsAutoCompleteController::GetImageAt(int32_t aIndex, nsAString& _retval) {
  int32_t matchIndex;
  nsIAutoCompleteResult* result;
  nsresult rv = GetResultAt(aIndex, &result, &matchIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  return result->GetImageAt(matchIndex, _retval);
}

NS_IMETHODIMP
nsAutoCompleteController::GetFinalCompleteValueAt(int32_t aIndex,
                                                  nsAString& _retval) {
  int32_t matchIndex;
  nsIAutoCompleteResult* result;
  nsresult rv = GetResultAt(aIndex, &result, &matchIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  return result->GetFinalCompleteValueAt(matchIndex, _retval);
}

NS_IMETHODIMP
nsAutoCompleteController::SetSearchString(const nsAString& aSearchString) {
  SetSearchStringInternal(aSearchString);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetSearchString(nsAString& aSearchString) {
  aSearchString = mSearchString;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIAutoCompleteObserver

NS_IMETHODIMP
nsAutoCompleteController::OnSearchResult(nsIAutoCompleteSearch* aSearch,
                                         nsIAutoCompleteResult* aResult) {
  MOZ_ASSERT(mSearchesOngoing > 0 && mSearches.Contains(aSearch));

  uint16_t result = 0;
  if (aResult) {
    aResult->GetSearchResult(&result);
  }

  // If our results are incremental, the search is still ongoing.
  if (result != nsIAutoCompleteResult::RESULT_SUCCESS_ONGOING &&
      result != nsIAutoCompleteResult::RESULT_NOMATCH_ONGOING) {
    --mSearchesOngoing;
  }

  // Look up the index of the search which is returning.
  for (uint32_t i = 0; i < mSearches.Length(); ++i) {
    if (mSearches[i] == aSearch) {
      ProcessResult(i, aResult);
    }
  }

  if (mSearchesOngoing == 0) {
    // If this is the last search to return, cleanup.
    PostSearchCleanup();
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsITimerCallback

MOZ_CAN_RUN_SCRIPT_BOUNDARY
NS_IMETHODIMP
nsAutoCompleteController::Notify(nsITimer* timer) {
  mTimer = nullptr;

  if (mImmediateSearchesCount == 0) {
    // If there were no immediate searches, BeforeSearches has not yet been
    // called, so do it now.
    nsresult rv = BeforeSearches();
    if (NS_FAILED(rv)) return rv;
  }
  StartSearch(nsIAutoCompleteSearchDescriptor::SEARCH_TYPE_DELAYED);
  AfterSearches();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsINamed

NS_IMETHODIMP
nsAutoCompleteController::GetName(nsACString& aName) {
  aName.AssignLiteral("nsAutoCompleteController");
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsAutoCompleteController

nsresult nsAutoCompleteController::OpenPopup() {
  uint32_t minResults;
  mInput->GetMinResultsForPopup(&minResults);

  if (mMatchCount >= minResults) {
    nsCOMPtr<nsIAutoCompleteInput> input = mInput;
    return input->SetPopupOpen(true);
  }

  return NS_OK;
}

nsresult nsAutoCompleteController::ClosePopup() {
  if (!mInput) {
    return NS_OK;
  }

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  bool isOpen = false;
  input->GetPopupOpen(&isOpen);
  if (!isOpen) return NS_OK;

  nsCOMPtr<nsIAutoCompletePopup> popup(GetPopup());
  NS_ENSURE_TRUE(popup != nullptr, NS_ERROR_FAILURE);
  MOZ_ALWAYS_SUCCEEDS(input->SetPopupOpen(false));
  return popup->SetSelectedIndex(-1);
}

nsresult nsAutoCompleteController::BeforeSearches() {
  NS_ENSURE_STATE(mInput);

  mSearchStatus = nsIAutoCompleteController::STATUS_SEARCHING;
  mDefaultIndexCompleted = false;

  // ClearResults will clear the mResults array, but we should pass the previous
  // result to each search to allow reusing it.  So we temporarily cache the
  // current results until AfterSearches().
  if (!mResultCache.AppendObjects(mResults)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  ClearResults(true);
  mSearchesOngoing = mSearches.Length();
  mSearchesFailed = 0;

  // notify the input that the search is beginning
  mInput->OnSearchBegin();

  return NS_OK;
}

nsresult nsAutoCompleteController::StartSearch(uint16_t aSearchType) {
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
    if (searchDesc) searchDesc->GetSearchType(&searchType);
    if (searchType != aSearchType) continue;

    nsIAutoCompleteResult* result = mResultCache.SafeObjectAt(i);

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
    if (NS_FAILED(rv)) return rv;

    // FormFill expects the searchParam to only contain the input element id,
    // other consumers may have other expectations, so this modifies it only
    // for new consumers handling autoFill by themselves.
    if (mProhibitAutoFill && mClearingAutoFillSearchesAgain) {
      searchParam.AppendLiteral(" prohibit-autofill");
    }

    uint32_t userContextId;
    rv = input->GetUserContextId(&userContextId);
    if (NS_SUCCEEDED(rv) &&
        userContextId != nsIScriptSecurityManager::DEFAULT_USER_CONTEXT_ID) {
      searchParam.AppendLiteral(" user-context-id:");
      searchParam.AppendInt(userContextId, 10);
    }

    rv = search->StartSearch(mSearchString, searchParam, result,
                             static_cast<nsIAutoCompleteObserver*>(this));
    if (NS_FAILED(rv)) {
      ++mSearchesFailed;
      MOZ_ASSERT(mSearchesOngoing > 0);
      --mSearchesOngoing;
    }
    // Because of the joy of nested event loops (which can easily happen when
    // some code uses a generator for an asynchronous AutoComplete search),
    // nsIAutoCompleteSearch::StartSearch might cause us to be detached from our
    // input field.  The next time we iterate, we'd be touching something that
    // we shouldn't be, and result in a crash.
    if (!mInput) {
      // The search operation has been finished.
      return NS_OK;
    }
  }

  return NS_OK;
}

void nsAutoCompleteController::AfterSearches() {
  mResultCache.Clear();
  if (mSearchesFailed == mSearches.Length()) {
    PostSearchCleanup();
  }
}

NS_IMETHODIMP
nsAutoCompleteController::StopSearch() {
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

void nsAutoCompleteController::MaybeCompletePlaceholder() {
  MOZ_ASSERT(mInput);

  if (!mInput) {  // or mInput depending on what you choose
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
      !mUserClearedAutoFill && !mPlaceholderCompletionString.IsEmpty() &&
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

nsresult nsAutoCompleteController::StartSearches() {
  // Don't create a new search timer if we're already waiting for one to fire.
  // If we don't check for this, we won't be able to cancel the original timer
  // and may crash when it fires (bug 236659).
  if (mTimer || !mInput) return NS_OK;

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  if (!mSearches.Length()) {
    // Initialize our list of search objects
    uint32_t searchCount;
    input->GetSearchCount(&searchCount);
    mResults.SetCapacity(searchCount);
    mSearches.SetCapacity(searchCount);
    mImmediateSearchesCount = 0;

    const char* searchCID = kAutoCompleteSearchCID;

    for (uint32_t i = 0; i < searchCount; ++i) {
      // Use the search name to create the contract id string for the search
      // service
      nsAutoCString searchName;
      input->GetSearchAt(i, searchName);
      nsAutoCString cid(searchCID);
      cid.Append(searchName);

      // Use the created cid to get a pointer to the search service and store it
      // for later
      nsCOMPtr<nsIAutoCompleteSearch> search = do_GetService(cid.get());
      if (search) {
        mSearches.AppendObject(search);

        // Count immediate searches.
        nsCOMPtr<nsIAutoCompleteSearchDescriptor> searchDesc =
            do_QueryInterface(search);
        if (searchDesc) {
          uint16_t searchType =
              nsIAutoCompleteSearchDescriptor::SEARCH_TYPE_DELAYED;
          if (NS_SUCCEEDED(searchDesc->GetSearchType(&searchType)) &&
              searchType ==
                  nsIAutoCompleteSearchDescriptor::SEARCH_TYPE_IMMEDIATE) {
            mImmediateSearchesCount++;
          }

          if (!mClearingAutoFillSearchesAgain) {
            searchDesc->GetClearingAutoFillSearchesAgain(
                &mClearingAutoFillSearchesAgain);
          }
        }
      }
    }
  }

  // Check if the current input should be completed with the placeholder string
  // from the last completion until the actual search results come back.
  MaybeCompletePlaceholder();

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
    if (NS_FAILED(rv)) return rv;
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
  return NS_NewTimerWithCallback(getter_AddRefs(mTimer), this, timeout,
                                 nsITimer::TYPE_ONE_SHOT);
}

nsresult nsAutoCompleteController::ClearSearchTimer() {
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
  return NS_OK;
}

nsresult nsAutoCompleteController::EnterMatch(bool aIsPopupSelection,
                                              dom::Event* aEvent) {
  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  nsCOMPtr<nsIAutoCompletePopup> popup(GetPopup());
  NS_ENSURE_TRUE(popup != nullptr, NS_ERROR_FAILURE);

  bool forceComplete;
  input->GetForceComplete(&forceComplete);

  int32_t selectedIndex;
  popup->GetSelectedIndex(&selectedIndex);

  // Ask the popup if it wants to enter a special value into the textbox
  nsAutoString value;
  popup->GetOverrideValue(value);
  if (value.IsEmpty()) {
    bool shouldComplete;
    input->GetCompleteDefaultIndex(&shouldComplete);
    bool completeSelection;
    input->GetCompleteSelectedIndex(&completeSelection);

    if (selectedIndex >= 0) {
      nsAutoString inputValue;
      input->GetTextValue(inputValue);
      if (aIsPopupSelection || !completeSelection) {
        // We need to fill-in the value if:
        //  * completeselectedindex is false
        //  * A match in the popup was confirmed
        GetResultValueAt(selectedIndex, true, value);
      } else if (mDefaultIndexCompleted &&
                 inputValue.Equals(mPlaceholderCompletionString,
                                   nsCaseInsensitiveStringComparator())) {
        // We also need to fill-in the value if the default index completion was
        // confirmed, though we cannot use the selectedIndex cause the selection
        // may have been changed by the mouse in the meanwhile.
        GetFinalDefaultCompleteValue(value);
      } else if (mCompletedSelectionIndex != -1) {
        // If completeselectedindex is true, and EnterMatch was not invoked by
        // mouse-clicking a match (for example the user pressed Enter),
        // don't fill in the value as it will have already been filled in as
        // needed, unless the selected match has a final complete value that
        // differs from the user-facing value.
        nsAutoString finalValue;
        GetResultValueAt(mCompletedSelectionIndex, true, finalValue);
        if (!inputValue.Equals(finalValue)) {
          value = finalValue;
        }
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
        nsIAutoCompleteResult* result = mResults[i];
        if (result) {
          uint32_t matchCount = 0;
          result->GetMatchCount(&matchCount);
          for (uint32_t j = 0; j < matchCount; ++j) {
            nsAutoString matchValue;
            result->GetValueAt(j, matchValue);
            if (suggestedValue.Equals(matchValue,
                                      nsCaseInsensitiveStringComparator())) {
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
    } else if (forceComplete && value.IsEmpty() && completeSelection) {
      // Since nothing was selected, and forceComplete is specified, that means
      // we have to find the first default match and enter it instead.
      for (uint32_t i = 0; i < mResults.Length(); ++i) {
        nsIAutoCompleteResult* result = mResults[i];
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

  nsCOMPtr<nsIObserverService> obsSvc = services::GetObserverService();
  NS_ENSURE_STATE(obsSvc);
  obsSvc->NotifyObservers(input, "autocomplete-will-enter-text", nullptr);

  if (!value.IsEmpty()) {
    SetValueOfInputTo(value, nsIAutoCompleteInput::TEXTVALUE_REASON_ENTERMATCH);
    input->SelectTextRange(value.Length(), value.Length());
    SetSearchStringInternal(value);
  }

  obsSvc->NotifyObservers(input, "autocomplete-did-enter-text", nullptr);

  bool cancel;
  bool itemWasSelected = selectedIndex >= 0 && !value.IsEmpty();
  input->OnTextEntered(aEvent, itemWasSelected, &cancel);

  ClosePopup();

  return NS_OK;
}

nsresult nsAutoCompleteController::RevertTextValue() {
  // StopSearch() can call PostSearchCleanup() which might result
  // in a blur event, which could null out mInput, so we need to check it
  // again.  See bug #408463 for more details
  if (!mInput) return NS_OK;

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  // If current input value is different from what we have set, it means
  // somebody modified the value like JS of the web content.  In such case,
  // we shouldn't overwrite it with the old value.
  nsAutoString currentValue;
  input->GetTextValue(currentValue);
  if (currentValue != mSetValue) {
    SetSearchStringInternal(currentValue);
    return NS_OK;
  }

  bool cancel = false;
  input->OnTextReverted(&cancel);

  if (!cancel) {
    nsCOMPtr<nsIObserverService> obsSvc = services::GetObserverService();
    NS_ENSURE_STATE(obsSvc);
    obsSvc->NotifyObservers(input, "autocomplete-will-revert-text", nullptr);

    // Don't change the value if it is the same to prevent sending useless
    // events. NOTE: how can |RevertTextValue| be called with inputValue !=
    // oldValue?
    if (mSearchString != currentValue) {
      SetValueOfInputTo(mSearchString,
                        nsIAutoCompleteInput::TEXTVALUE_REASON_REVERT);
    }

    obsSvc->NotifyObservers(input, "autocomplete-did-revert-text", nullptr);
  }

  return NS_OK;
}

nsresult nsAutoCompleteController::ProcessResult(
    int32_t aSearchIndex, nsIAutoCompleteResult* aResult) {
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
      MOZ_ASSERT(false,
                 "Passing new matches to OnSearchResult with a new "
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

  uint32_t oldMatchCount = mMatchCount;
  // If the search failed, increase the match count to include the error
  // description.
  if (searchResult == nsIAutoCompleteResult::RESULT_FAILURE) {
    nsAutoString error;
    aResult->GetErrorDescription(error);
    if (!error.IsEmpty()) {
      ++mMatchCount;
    }
  } else if (searchResult == nsIAutoCompleteResult::RESULT_SUCCESS ||
             searchResult == nsIAutoCompleteResult::RESULT_SUCCESS_ONGOING) {
    // Increase the match count for all matches in this result.
    uint32_t totalMatchCount = 0;
    for (uint32_t i = 0; i < mResults.Length(); i++) {
      nsIAutoCompleteResult* result = mResults.SafeObjectAt(i);
      if (result) {
        uint32_t matchCount = 0;
        result->GetMatchCount(&matchCount);
        totalMatchCount += matchCount;
      }
    }
    uint32_t delta = totalMatchCount - oldMatchCount;
    mMatchCount += delta;
  }

  // Try to autocomplete the default index for this search.
  // Do this before invalidating so the binding knows about it.
  CompleteDefaultIndex(aSearchIndex);

  // Refresh the popup view to display the new search results
  nsCOMPtr<nsIAutoCompletePopup> popup(GetPopup());
  NS_ENSURE_TRUE(popup != nullptr, NS_ERROR_FAILURE);
  popup->Invalidate(nsIAutoCompletePopup::INVALIDATE_REASON_NEW_RESULT);

  uint32_t minResults;
  input->GetMinResultsForPopup(&minResults);

  // Make sure the popup is open, if necessary, since we now have at least one
  // search result ready to display. Don't force the popup closed if we might
  // get results in the future to avoid unnecessarily canceling searches.
  if (mMatchCount || !minResults) {
    OpenPopup();
  } else if (mSearchesOngoing == 0) {
    ClosePopup();
  }

  return NS_OK;
}

nsresult nsAutoCompleteController::PostSearchCleanup() {
  NS_ENSURE_STATE(mInput);
  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  uint32_t minResults;
  input->GetMinResultsForPopup(&minResults);

  if (mMatchCount || minResults == 0) {
    OpenPopup();
    if (mMatchCount)
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

nsresult nsAutoCompleteController::ClearResults(bool aIsSearching) {
  int32_t oldMatchCount = mMatchCount;
  mMatchCount = 0;
  mResults.Clear();
  if (oldMatchCount != 0) {
    if (mInput) {
      nsCOMPtr<nsIAutoCompletePopup> popup(GetPopup());
      NS_ENSURE_TRUE(popup != nullptr, NS_ERROR_FAILURE);
      // Clear the selection.
      popup->SetSelectedIndex(-1);
    }
  }
  return NS_OK;
}

nsresult nsAutoCompleteController::CompleteDefaultIndex(int32_t aResultIndex) {
  if (mDefaultIndexCompleted || mProhibitAutoFill ||
      mSearchString.Length() == 0 || !mInput)
    return NS_OK;

  nsCOMPtr<nsIAutoCompleteInput> input(mInput);

  int32_t selectionStart;
  input->GetSelectionStart(&selectionStart);
  int32_t selectionEnd;
  input->GetSelectionEnd(&selectionEnd);

  bool isPlaceholderSelected =
      selectionEnd == (int32_t)mPlaceholderCompletionString.Length() &&
      selectionStart == (int32_t)mSearchString.Length() &&
      StringBeginsWith(mPlaceholderCompletionString, mSearchString,
                       nsCaseInsensitiveStringComparator());

  // Don't try to automatically complete to the first result if there's already
  // a selection or the cursor isn't at the end of the input. In case the
  // selection is from the current placeholder completion value, then still
  // automatically complete.
  if (!isPlaceholderSelected &&
      (selectionEnd != selectionStart ||
       selectionEnd != (int32_t)mSearchString.Length()))
    return NS_OK;

  bool shouldComplete;
  input->GetCompleteDefaultIndex(&shouldComplete);
  if (!shouldComplete) return NS_OK;

  nsAutoString resultValue;
  if (NS_SUCCEEDED(GetDefaultCompleteValue(aResultIndex, true, resultValue))) {
    CompleteValue(resultValue);
    mDefaultIndexCompleted = true;
  } else {
    // Reset the search string again, in case it was completed with
    // mPlaceholderCompletionString, but the actually received result doesn't
    // have a default index result. Only reset the input when necessary, to
    // avoid triggering unnecessary new searches.
    nsAutoString inputValue;
    input->GetTextValue(inputValue);
    if (!inputValue.Equals(mSearchString)) {
      SetValueOfInputTo(mSearchString,
                        nsIAutoCompleteInput::TEXTVALUE_REASON_REVERT);
      input->SelectTextRange(mSearchString.Length(), mSearchString.Length());
    }
    mPlaceholderCompletionString.Truncate();
  }

  return NS_OK;
}

nsresult nsAutoCompleteController::GetDefaultCompleteResult(
    int32_t aResultIndex, nsIAutoCompleteResult** _result,
    int32_t* _defaultIndex) {
  *_defaultIndex = -1;
  int32_t resultIndex = aResultIndex;

  // If a result index was not provided, find the first defaultIndex result.
  for (int32_t i = 0; resultIndex < 0 && i < mResults.Count(); ++i) {
    nsIAutoCompleteResult* result = mResults.SafeObjectAt(i);
    if (result && NS_SUCCEEDED(result->GetDefaultIndex(_defaultIndex)) &&
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

nsresult nsAutoCompleteController::GetDefaultCompleteValue(int32_t aResultIndex,
                                                           bool aPreserveCasing,
                                                           nsAString& _retval) {
  nsIAutoCompleteResult* result;
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
    casedResultValue.Append(
        Substring(resultValue, mSearchString.Length(), resultValue.Length()));
    _retval = casedResultValue;
  } else
    _retval = resultValue;

  return NS_OK;
}

nsresult nsAutoCompleteController::GetFinalDefaultCompleteValue(
    nsAString& _retval) {
  MOZ_ASSERT(mInput, "Must have a valid input");
  nsCOMPtr<nsIAutoCompleteInput> input(mInput);
  nsIAutoCompleteResult* result;
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

nsresult nsAutoCompleteController::CompleteValue(nsString& aValue)
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
    SetValueOfInputTo(aValue,
                      nsIAutoCompleteInput::TEXTVALUE_REASON_COMPLETEDEFAULT);
  } else {
    nsresult rv;
    nsCOMPtr<nsIIOService> ios = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsAutoCString scheme;
    if (NS_SUCCEEDED(
            ios->ExtractScheme(NS_ConvertUTF16toUTF8(aValue), scheme))) {
      // Trying to autocomplete a URI from somewhere other than the beginning.
      // Only succeed if the missing portion is "http://"; otherwise do not
      // autocomplete.  This prevents us from "helpfully" autocompleting to a
      // URI that isn't equivalent to what the user expected.
      const int32_t findIndex = 7;  // length of "http://"

      if ((endSelect < findIndex + mSearchStringLength) ||
          !scheme.EqualsLiteral("http") ||
          !Substring(aValue, findIndex, mSearchStringLength)
               .Equals(mSearchString, nsCaseInsensitiveStringComparator())) {
        return NS_OK;
      }

      mPlaceholderCompletionString =
          mSearchString +
          Substring(aValue, mSearchStringLength + findIndex, endSelect);
      SetValueOfInputTo(mPlaceholderCompletionString,
                        nsIAutoCompleteInput::TEXTVALUE_REASON_COMPLETEDEFAULT);

      endSelect -= findIndex;  // We're skipping this many characters of aValue.
    } else {
      // Autocompleting something other than a URI from the middle.
      // Use the format "searchstring >> full string" to indicate to the user
      // what we are going to replace their search string with.
      SetValueOfInputTo(mSearchString + NS_LITERAL_STRING(" >> ") + aValue,
                        nsIAutoCompleteInput::TEXTVALUE_REASON_COMPLETEDEFAULT);

      endSelect = mSearchString.Length() + 4 + aValue.Length();

      // Reset the last search completion.
      mPlaceholderCompletionString.Truncate();
    }
  }

  input->SelectTextRange(mSearchStringLength, endSelect);

  return NS_OK;
}

nsresult nsAutoCompleteController::GetResultLabelAt(int32_t aIndex,
                                                    nsAString& _retval) {
  return GetResultValueLabelAt(aIndex, false, false, _retval);
}

nsresult nsAutoCompleteController::GetResultValueAt(int32_t aIndex,
                                                    bool aGetFinalValue,
                                                    nsAString& _retval) {
  return GetResultValueLabelAt(aIndex, aGetFinalValue, true, _retval);
}

nsresult nsAutoCompleteController::GetResultValueLabelAt(int32_t aIndex,
                                                         bool aGetFinalValue,
                                                         bool aGetValue,
                                                         nsAString& _retval) {
  NS_ENSURE_TRUE(aIndex >= 0 && static_cast<uint32_t>(aIndex) < mMatchCount,
                 NS_ERROR_ILLEGAL_VALUE);

  int32_t matchIndex;
  nsIAutoCompleteResult* result;
  nsresult rv = GetResultAt(aIndex, &result, &matchIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  uint16_t searchResult;
  result->GetSearchResult(&searchResult);

  if (searchResult == nsIAutoCompleteResult::RESULT_FAILURE) {
    if (aGetValue) return NS_ERROR_FAILURE;
    result->GetErrorDescription(_retval);
  } else if (searchResult == nsIAutoCompleteResult::RESULT_SUCCESS ||
             searchResult == nsIAutoCompleteResult::RESULT_SUCCESS_ONGOING) {
    if (aGetFinalValue) {
      // Some implementations may miss finalCompleteValue, try to be backwards
      // compatible.
      if (NS_FAILED(result->GetFinalCompleteValueAt(matchIndex, _retval))) {
        result->GetValueAt(matchIndex, _retval);
      }
    } else if (aGetValue) {
      result->GetValueAt(matchIndex, _retval);
    } else {
      result->GetLabelAt(matchIndex, _retval);
    }
  }

  return NS_OK;
}

/**
 * Given the index of a match in the autocomplete popup, find the
 * corresponding nsIAutoCompleteSearch index, and sub-index into
 * the search's results list.
 */
nsresult nsAutoCompleteController::MatchIndexToSearch(int32_t aMatchIndex,
                                                      int32_t* aSearchIndex,
                                                      int32_t* aItemIndex) {
  *aSearchIndex = -1;
  *aItemIndex = -1;

  uint32_t index = 0;

  // Move index through the results of each registered nsIAutoCompleteSearch
  // until we find the given match
  for (uint32_t i = 0; i < mSearches.Length(); ++i) {
    nsIAutoCompleteResult* result = mResults.SafeObjectAt(i);
    if (!result) continue;

    uint32_t matchCount = 0;

    uint16_t searchResult;
    result->GetSearchResult(&searchResult);

    // Find out how many results were provided by the
    // current nsIAutoCompleteSearch.
    if (searchResult == nsIAutoCompleteResult::RESULT_SUCCESS ||
        searchResult == nsIAutoCompleteResult::RESULT_SUCCESS_ONGOING) {
      result->GetMatchCount(&matchCount);
    }

    // If the given match index is within the results range
    // of the current nsIAutoCompleteSearch then return the
    // search index and sub-index into the results array
    if ((matchCount != 0) &&
        (index + matchCount - 1 >= (uint32_t)aMatchIndex)) {
      *aSearchIndex = i;
      *aItemIndex = aMatchIndex - index;
      return NS_OK;
    }

    // Advance the popup table index cursor past the
    // results of the current search.
    index += matchCount;
  }

  return NS_OK;
}
