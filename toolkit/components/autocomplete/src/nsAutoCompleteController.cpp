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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (Original Author)
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Johnny Stenback <jst@mozilla.jstenback.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#include "nsAutoCompleteController.h"
#ifdef MOZ_MORK
#include "nsAutoCompleteMdbResult.h"
#endif
#include "nsAutoCompleteSimpleResult.h"

#include "nsNetCID.h"
#include "nsIIOService.h"
#include "nsToolkitCompsCID.h"
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIAtomService.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsITreeColumns.h"
#include "nsIGenericFactory.h"

static const char *kAutoCompleteSearchCID = "@mozilla.org/autocomplete/search;1?name=";

NS_IMPL_ISUPPORTS5(nsAutoCompleteController, nsIAutoCompleteController,
                                             nsIAutoCompleteObserver,
                                             nsIRollupListener,
                                             nsITimerCallback,
                                             nsITreeView)

nsAutoCompleteController::nsAutoCompleteController() :
  mEnterAfterSearch(PR_FALSE),
  mDefaultIndexCompleted(PR_FALSE),
  mBackspaced(PR_FALSE),
  mPopupClosedByCompositionStart(PR_FALSE),
  mIsIMEComposing(PR_FALSE),
  mIgnoreHandleText(PR_FALSE),
  mIsOpen(PR_FALSE),
  mSearchStatus(0),
  mRowCount(0),
  mSearchesOngoing(0)
{
  mSearches = do_CreateInstance("@mozilla.org/supports-array;1");
  mResults = do_CreateInstance("@mozilla.org/supports-array;1");
}

nsAutoCompleteController::~nsAutoCompleteController()
{
  SetInput(nsnull);
}

////////////////////////////////////////////////////////////////////////
//// nsIAutoCompleteController

NS_IMETHODIMP
nsAutoCompleteController::GetSearchStatus(PRUint16 *aSearchStatus)
{
  *aSearchStatus = mSearchStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetMatchCount(PRUint32 *aMatchCount)
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
    ClearSearchTimer();
    ClearResults();
    if (mIsOpen) {
      ClosePopup();
    }
    mSearches->Clear();
  }
    
  mInput = aInput;

  // Nothing more to do if the input was just being set to null.
  if (!aInput)
    return NS_OK;

  nsAutoString newValue;
  mInput->GetTextValue(newValue);
  
  // Reset all search state members to default values
  mSearchString = newValue;
  mEnterAfterSearch = PR_FALSE;
  mDefaultIndexCompleted = PR_FALSE;
  mBackspaced = PR_FALSE;
  mSearchStatus = nsIAutoCompleteController::STATUS_NONE;
  mRowCount = 0;
  mSearchesOngoing = 0;
  
  // Initialize our list of search objects
  PRUint32 searchCount;
  mInput->GetSearchCount(&searchCount);
  mResults->SizeTo(searchCount);
  mSearches->SizeTo(searchCount);
  
  const char *searchCID = kAutoCompleteSearchCID;

  for (PRUint32 i = 0; i < searchCount; ++i) {
    // Use the search name to create the contract id string for the search service
    nsCAutoString searchName;
    mInput->GetSearchAt(i, searchName);
    nsCAutoString cid(searchCID);
    cid.Append(searchName);
    
    // Use the created cid to get a pointer to the search service and store it for later
    nsCOMPtr<nsIAutoCompleteSearch> search = do_GetService(cid.get());
    if (search)
      mSearches->AppendElement(search);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::StartSearch(const nsAString &aSearchString)
{ 
  mSearchString = aSearchString;
  StartSearchTimer();

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleText(PRBool aIgnoreSelection)
{
  if (!mInput) {
    // Stop current search in case it's async.
    StopSearch();
    // Stop the queued up search on a timer
    ClearSearchTimer();
    // Note: if now is after blur and IME end composition,
    // check mInput before calling.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=193544#c31
    NS_ERROR("Called before attaching to the control or after detaching from the control");
    return NS_OK;
  }

  nsAutoString newValue;
  mInput->GetTextValue(newValue);

  // Note: the events occur in the following order when IME is used.
  // 1. composition start event(HandleStartComposition)
  // 2. composition end event(HandleEndComposition)
  // 3. input event(HandleText)
  // Note that the input event occurs if IME composition is cancelled, as well.
  // In HandleEndComposition, we are processing the popup properly.
  // Therefore, the input event after composition end event should do nothing.
  // (E.g., calling StopSearch(), ClearSearchTimer() and ClosePopup().)
  // If it is not, popup is always closed after composition end.
  if (mIgnoreHandleText) {
    mIgnoreHandleText = PR_FALSE;
    if (newValue.Equals(mSearchString))
      return NS_OK;
    NS_ERROR("Now is after composition end event. But the value was changed.");
  }

  // Stop current search in case it's async.
  StopSearch();
  // Stop the queued up search on a timer
  ClearSearchTimer();

  PRBool disabled;
  mInput->GetDisableAutoComplete(&disabled);
  NS_ENSURE_TRUE(!disabled, NS_OK);

  // Don't search again if the new string is the same as the last search
  if (newValue.Length() > 0 && newValue.Equals(mSearchString))
    return NS_OK;

  // Determine if the user has removed text from the end (probably by backspacing)
  if (newValue.Length() < mSearchString.Length() &&
      Substring(mSearchString, 0, newValue.Length()).Equals(newValue))
  {
    // We need to throw away previous results so we don't try to search through them again
    ClearResults();
    mBackspaced = PR_TRUE;
  } else
    mBackspaced = PR_FALSE;

  if (mRowCount == 0)
    // XXX Handle the case where we have no results because of an ignored prefix. 
    // This is just a hack. I have no idea what I'm doing. Hewitt, fix this the right
    // way when you get a chance. -dwh
    ClearResults();

  mSearchString = newValue;

  // Don't search if the value is empty
  if (newValue.Length() == 0) {
    ClosePopup();
    return NS_OK;
  }

  if (aIgnoreSelection) {
    StartSearchTimer();
  } else {
    // Kick off the search only if the cursor is at the end of the textbox
  PRInt32 selectionStart;
  mInput->GetSelectionStart(&selectionStart);
  PRInt32 selectionEnd;
  mInput->GetSelectionEnd(&selectionEnd);

  if (selectionStart == selectionEnd && selectionStart == (PRInt32) mSearchString.Length())
    StartSearchTimer();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleEnter(PRBool *_retval)
{
  *_retval = PR_FALSE;
  if (!mInput)
    return NS_OK;

  // allow the event through unless there is something selected in the popup
  mInput->GetPopupOpen(_retval);
  if (*_retval) {
    nsCOMPtr<nsIAutoCompletePopup> popup;
    mInput->GetPopup(getter_AddRefs(popup));

    if (popup) {
      PRInt32 selectedIndex;
      popup->GetSelectedIndex(&selectedIndex);
      *_retval = selectedIndex >= 0;
    }
  }
  
  ClearSearchTimer();
  EnterMatch();
  
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleEscape(PRBool *_retval)
{
  *_retval = PR_FALSE;
  if (!mInput)
    return NS_OK;

  // allow the event through if the popup is closed
  mInput->GetPopupOpen(_retval);
  
  ClearSearchTimer();
  ClearResults();
  RevertTextValue();
  ClosePopup();

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleStartComposition()
{
  NS_ENSURE_TRUE(!mIsIMEComposing, NS_OK);

  mPopupClosedByCompositionStart = PR_FALSE;
  mIsIMEComposing = PR_TRUE;

  if (!mInput)
    return NS_OK;

  PRBool disabled;
  mInput->GetDisableAutoComplete(&disabled);
  if (disabled)
    return NS_OK;

  StopSearch();
  ClearSearchTimer();

  PRBool isOpen;
  mInput->GetPopupOpen(&isOpen);
  if (isOpen)
    ClosePopup();
  mPopupClosedByCompositionStart = isOpen;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleEndComposition()
{
  NS_ENSURE_TRUE(mIsIMEComposing, NS_OK);

  mIsIMEComposing = PR_FALSE;
  PRBool forceOpenPopup = mPopupClosedByCompositionStart;
  mPopupClosedByCompositionStart = PR_FALSE;

  if (!mInput)
    return NS_OK;

  nsAutoString value;
  mInput->GetTextValue(value);
  SetSearchString(EmptyString());
  if (!value.IsEmpty()) {
    // Show the popup with a filtered result set
    HandleText(PR_TRUE);
  } else if (forceOpenPopup) {
    PRBool cancel;
    HandleKeyNavigation(nsIAutoCompleteController::KEY_DOWN, &cancel);
  }
  // On here, |value| and |mSearchString| are same. Therefore, next HandleText should be
  // ignored. Because there are no reason to research.
  mIgnoreHandleText = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleTab()
{
  PRBool cancel;
  return HandleEnter(&cancel);
}

NS_IMETHODIMP
nsAutoCompleteController::HandleKeyNavigation(PRUint16 aKey, PRBool *_retval)
{
  // By default, don't cancel the event
  *_retval = PR_FALSE;

  if (!mInput) {
    // Note: if now is after blur and IME end composition,
    // check mInput before calling.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=193544#c31
    NS_ERROR("Called before attaching to the control or after detaching from the control");
    return NS_OK;
  }

  nsCOMPtr<nsIAutoCompletePopup> popup;
  mInput->GetPopup(getter_AddRefs(popup));
  NS_ENSURE_TRUE(popup != nsnull, NS_ERROR_FAILURE);

  PRBool disabled;
  mInput->GetDisableAutoComplete(&disabled);
  NS_ENSURE_TRUE(!disabled, NS_OK);

  if (aKey == nsIAutoCompleteController::KEY_UP ||
      aKey == nsIAutoCompleteController::KEY_DOWN || 
      aKey == nsIAutoCompleteController::KEY_PAGE_UP || 
      aKey == nsIAutoCompleteController::KEY_PAGE_DOWN)
  {
    // Prevent the input from handling up/down events, as it may move
    // the cursor to home/end on some systems
    *_retval = PR_TRUE;
    
    PRBool isOpen;
    mInput->GetPopupOpen(&isOpen);
    if (isOpen) {
      PRBool reverse = aKey == nsIAutoCompleteController::KEY_UP ||
                      aKey == nsIAutoCompleteController::KEY_PAGE_UP ? PR_TRUE : PR_FALSE;
      PRBool page = aKey == nsIAutoCompleteController::KEY_PAGE_UP ||
                    aKey == nsIAutoCompleteController::KEY_PAGE_DOWN ? PR_TRUE : PR_FALSE;
      
      // Fill in the value of the textbox with whatever is selected in the popup
      // if the completeSelectedIndex attribute is set.  We check this before
      // calling SelectBy since that can null out mInput in some cases.
      PRBool completeSelection;
      mInput->GetCompleteSelectedIndex(&completeSelection);

      // Instruct the result view to scroll by the given amount and direction
      popup->SelectBy(reverse, page);

      if (completeSelection)
      {
        PRInt32 selectedIndex;
        popup->GetSelectedIndex(&selectedIndex);
        if (selectedIndex >= 0) {
          //  A result is selected, so fill in its value
          nsAutoString value;
          if (NS_SUCCEEDED(GetResultValueAt(selectedIndex, PR_TRUE, value))) {
            mInput->SetTextValue(value);
            mInput->SelectTextRange(value.Length(), value.Length());
          }
        } else {
          // Nothing is selected, so fill in the last typed value
          mInput->SetTextValue(mSearchString);
          mInput->SelectTextRange(mSearchString.Length(), mSearchString.Length());
        }
      }
    } else {
      // Open the popup if there has been a previous search, or else kick off a new search
      PRUint32 resultCount;
      mResults->Count(&resultCount);
      if (resultCount) {
        if (mRowCount) {
          OpenPopup();
        }
      } else
        StartSearchTimer();
    }    
  } else if (   aKey == nsIAutoCompleteController::KEY_LEFT 
             || aKey == nsIAutoCompleteController::KEY_RIGHT 
#ifndef XP_MACOSX
             || aKey == nsIAutoCompleteController::KEY_HOME
#endif
            )
  {
    // The user hit a text-navigation key.
    PRBool isOpen;
    mInput->GetPopupOpen(&isOpen);
    if (isOpen) {
      PRInt32 selectedIndex;
      popup->GetSelectedIndex(&selectedIndex);
      if (selectedIndex >= 0) {
        // The pop-up is open and has a selection, take its value
        nsAutoString value;
        if (NS_SUCCEEDED(GetResultValueAt(selectedIndex, PR_TRUE, value))) {
          mInput->SetTextValue(value);
          mInput->SelectTextRange(value.Length(), value.Length());
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
    mInput->GetTextValue(value);
    mSearchString = value;
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HandleDelete(PRBool *_retval)
{
  *_retval = PR_FALSE;
  if (!mInput)
    return NS_OK;

  PRBool isOpen = PR_FALSE;
  mInput->GetPopupOpen(&isOpen);
  if (!isOpen || mRowCount <= 0) {
    // Nothing left to delete, proceed as normal
    HandleText(PR_FALSE);
    return NS_OK;
  }
  
  nsCOMPtr<nsIAutoCompletePopup> popup;
  mInput->GetPopup(getter_AddRefs(popup));

  PRInt32 index, searchIndex, rowIndex;
  popup->GetSelectedIndex(&index);
  RowIndexToSearch(index, &searchIndex, &rowIndex);
  NS_ENSURE_TRUE(searchIndex >= 0 && rowIndex >= 0, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAutoCompleteResult> result;
  mResults->GetElementAt(searchIndex, getter_AddRefs(result));
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  nsAutoString search;
  mInput->GetSearchParam(search);

  // Clear the row in our result and in the DB.
  result->RemoveValueAt(rowIndex, PR_TRUE);
  --mRowCount;

  // Unselect the current item.
  popup->SetSelectedIndex(-1);

  // Tell the tree that the row count changed. 
  if (mTree)
    mTree->RowCountChanged(mRowCount, -1);

  // Adjust index, if needed.
  if (index >= (PRInt32)mRowCount)
    index = mRowCount - 1;

  if (mRowCount > 0) {
    // There are still rows in the popup, select the current index again.
    popup->SetSelectedIndex(index);

    // Complete to the new current value.
    nsAutoString value;
    if (NS_SUCCEEDED(GetResultValueAt(index, PR_TRUE, value))) {
      CompleteValue(value, PR_FALSE);

      // Make sure we cancel the event that triggerd this call.
      *_retval = PR_TRUE;
    }

    // Invalidate the popup.
    popup->Invalidate();
  } else {
    // Nothing left in the popup, clear any pending search timers and
    // close the popup.
    ClearSearchTimer();
    ClosePopup();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetValueAt(PRInt32 aIndex, nsAString & _retval)
{
  GetResultValueAt(aIndex, PR_FALSE, _retval);
  
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetCommentAt(PRInt32 aIndex, nsAString & _retval)
{
  PRInt32 searchIndex;
  PRInt32 rowIndex;
  RowIndexToSearch(aIndex, &searchIndex, &rowIndex);
  NS_ENSURE_TRUE(searchIndex >= 0 && rowIndex >= 0, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIAutoCompleteResult> result;
  mResults->GetElementAt(searchIndex, getter_AddRefs(result));
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  result->GetCommentAt(rowIndex, _retval);

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetStyleAt(PRInt32 aIndex, nsAString & _retval)
{
  PRInt32 searchIndex;
  PRInt32 rowIndex;
  RowIndexToSearch(aIndex, &searchIndex, &rowIndex);
  NS_ENSURE_TRUE(searchIndex >= 0 && rowIndex >= 0, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIAutoCompleteResult> result;
  mResults->GetElementAt(searchIndex, getter_AddRefs(result));
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  result->GetStyleAt(rowIndex, _retval);

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::SetSearchString(const nsAString &aSearchString)
{ 
  mSearchString = aSearchString;
  
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::AttachRollupListener()
{
  nsIWidget* widget = GetPopupWidget();
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);
  NS_ASSERTION(mInput, "mInput must not be null.");
  PRBool consumeRollupEvent = PR_FALSE;
  mInput->GetConsumeRollupEvent(&consumeRollupEvent);
  return widget->CaptureRollupEvents((nsIRollupListener*)this,
                                     PR_TRUE, consumeRollupEvent);
}

NS_IMETHODIMP
nsAutoCompleteController::DetachRollupListener()
{
  nsIWidget* widget = GetPopupWidget();
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);
  return widget->CaptureRollupEvents((nsIRollupListener*)this,
                                     PR_FALSE, PR_FALSE);
}

////////////////////////////////////////////////////////////////////////
//// nsIAutoCompleteObserver

NS_IMETHODIMP
nsAutoCompleteController::OnSearchResult(nsIAutoCompleteSearch *aSearch, nsIAutoCompleteResult* aResult)
{
  // look up the index of the search which is returning
  PRUint32 count;
  mSearches->Count(&count);
  for (PRUint32 i = 0; i < count; ++i) {
    nsCOMPtr<nsIAutoCompleteSearch> search;
    mSearches->GetElementAt(i, getter_AddRefs(search));
    if (search == aSearch) {
      ProcessResult(i, aResult);
    }
  }
  
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIRollupListener

NS_IMETHODIMP
nsAutoCompleteController::Rollup()
{
  ClearSearchTimer();
  ClearResults();
  ClosePopup();
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::ShouldRollupOnMouseWheelEvent(PRBool *aShouldRollup)
{
  *aShouldRollup = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::ShouldRollupOnMouseActivate(PRBool *aShouldRollup)
{
  *aShouldRollup = PR_FALSE;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsITimerCallback

NS_IMETHODIMP
nsAutoCompleteController::Notify(nsITimer *timer)
{
  mTimer = nsnull;
  StartSearch();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsITreeView

NS_IMETHODIMP
nsAutoCompleteController::GetRowCount(PRInt32 *aRowCount)
{
  *aRowCount = mRowCount;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetRowProperties(PRInt32 index, nsISupportsArray *properties)
{
  // XXX This is a hack because the tree doesn't seem to be painting the selected row
  //     the normal way.  Please remove this ASAP.
  PRInt32 currentIndex;
  mSelection->GetCurrentIndex(&currentIndex);
  
  /*
  if (index == currentIndex) {
    nsCOMPtr<nsIAtomService> atomSvc = do_GetService("@mozilla.org/atom-service;1");
    nsCOMPtr<nsIAtom> atom;
    atomSvc->GetAtom(NS_LITERAL_STRING("menuactive").get(), getter_AddRefs(atom));
    properties->AppendElement(atom);
  }
  */

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetCellProperties(PRInt32 row, nsITreeColumn* col, nsISupportsArray* properties)
{
  GetRowProperties(row, properties);
  
  if (row >= 0) {
    nsAutoString className;
    GetStyleAt(row, className);
    if (!className.IsEmpty()) {
      nsCOMPtr<nsIAtomService> atomSvc = do_GetService("@mozilla.org/atom-service;1");
      nsCOMPtr<nsIAtom> atom;
      atomSvc->GetAtom(className.get(), getter_AddRefs(atom));
      properties->AppendElement(atom);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetColumnProperties(nsITreeColumn* col, nsISupportsArray* properties)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetImageSrc(PRInt32 row, nsITreeColumn* col, nsAString& _retval)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetProgressMode(PRInt32 row, nsITreeColumn* col, PRInt32* _retval)
{
  NS_NOTREACHED("tree has no progress cells");
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetCellValue(PRInt32 row, nsITreeColumn* col, nsAString& _retval)
{  
  NS_NOTREACHED("all of our cells are text");
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetCellText(PRInt32 row, nsITreeColumn* col, nsAString& _retval)
{
  const PRUnichar* colID;
  col->GetIdConst(&colID);
  
  if (NS_LITERAL_STRING("treecolAutoCompleteValue").Equals(colID))
    GetValueAt(row, _retval);
  else if (NS_LITERAL_STRING("treecolAutoCompleteComment").Equals(colID))
    GetCommentAt(row, _retval);
     
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsContainer(PRInt32 index, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsContainerOpen(PRInt32 index, PRBool *_retval)
{
  NS_NOTREACHED("no container cells");
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsContainerEmpty(PRInt32 index, PRBool *_retval)
{
  NS_NOTREACHED("no container cells");
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetLevel(PRInt32 index, PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::GetParentIndex(PRInt32 rowIndex, PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::HasNextSibling(PRInt32 rowIndex, PRInt32 afterIndex, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::ToggleOpenState(PRInt32 index)
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
nsAutoCompleteController::SetCellValue(PRInt32 row, nsITreeColumn* col, const nsAString& value)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::SetCellText(PRInt32 row, nsITreeColumn* col, const nsAString& value)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::CycleHeader(nsITreeColumn* col)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::CycleCell(PRInt32 row, nsITreeColumn* col)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsEditable(PRInt32 row, nsITreeColumn* col, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsSelectable(PRInt32 row, nsITreeColumn* col, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsSeparator(PRInt32 index, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::IsSorted(PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::CanDrop(PRInt32 index, PRInt32 orientation, PRBool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::Drop(PRInt32 row, PRInt32 orientation)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::PerformAction(const PRUnichar *action)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteController::PerformActionOnCell(const PRUnichar* action, PRInt32 row, nsITreeColumn* col)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsAutoCompleteController

nsresult
nsAutoCompleteController::OpenPopup()
{
  PRUint32 minResults;
  mInput->GetMinResultsForPopup(&minResults);

  if (mRowCount >= minResults) {
    mIsOpen = PR_TRUE;
    return mInput->SetPopupOpen(PR_TRUE);
  }
  
  return NS_OK;
}

nsresult
nsAutoCompleteController::ClosePopup()
{
  nsCOMPtr<nsIAutoCompletePopup> popup;
  mInput->GetPopup(getter_AddRefs(popup));
  NS_ENSURE_TRUE(popup != nsnull, NS_ERROR_FAILURE);
  popup->SetSelectedIndex(-1);
  mIsOpen = PR_FALSE;
  return mInput->SetPopupOpen(PR_FALSE);
}

nsresult
nsAutoCompleteController::StartSearch()
{
  NS_ENSURE_STATE(mInput);
  mSearchStatus = nsIAutoCompleteController::STATUS_SEARCHING;
  mDefaultIndexCompleted = PR_FALSE;
  
  PRUint32 count;
  mSearches->Count(&count);
  mSearchesOngoing = count;

  PRUint32 searchesFailed = 0;
  for (PRUint32 i = 0; i < count; ++i) {
    nsCOMPtr<nsIAutoCompleteSearch> search;
    mSearches->GetElementAt(i, getter_AddRefs(search));
    nsCOMPtr<nsIAutoCompleteResult> result;
    mResults->GetElementAt(i, getter_AddRefs(result));
    
    if (result) {
      PRUint16 searchResult;
      result->GetSearchResult(&searchResult);
      if (searchResult != nsIAutoCompleteResult::RESULT_SUCCESS)
        result = nsnull;
    }
    
    nsAutoString searchParam;
    // XXXben - can yank this when we discover what's causing this to 
    // fail & crash. 
    nsresult rv = mInput->GetSearchParam(searchParam);
    if (NS_FAILED(rv))
        return rv;
    
    rv = search->StartSearch(mSearchString, searchParam, result, NS_STATIC_CAST(nsIAutoCompleteObserver *, this));
    if (NS_FAILED(rv)) {
      ++searchesFailed;
      --mSearchesOngoing;
    }
  }
  
  if (searchesFailed == count) {
    PostSearchCleanup();
  }
  return NS_OK;
}

nsresult
nsAutoCompleteController::StopSearch()
{
  // Stop the timer if there is one
  ClearSearchTimer();

  // Stop any ongoing asynchronous searches
  if (mSearchStatus == nsIAutoCompleteController::STATUS_SEARCHING) {
    PRUint32 count;
    mSearches->Count(&count);
  
    for (PRUint32 i = 0; i < count; ++i) {
      nsCOMPtr<nsIAutoCompleteSearch> search;
      mSearches->GetElementAt(i, getter_AddRefs(search));
      search->StopSearch();
    }
  }
  return NS_OK;
}

nsresult
nsAutoCompleteController::StartSearchTimer()
{
  // Don't create a new search timer if we're already waiting for one to fire.
  // If we don't check for this, we won't be able to cancel the original timer
  // and may crash when it fires (bug 236659).
  if (mTimer || !mInput)
    return NS_OK;

  PRUint32 timeout;
  mInput->GetTimeout(&timeout);

  mTimer = do_CreateInstance("@mozilla.org/timer;1");
  mTimer->InitWithCallback(this, timeout, nsITimer::TYPE_ONE_SHOT);
  return NS_OK;
}

nsresult
nsAutoCompleteController::ClearSearchTimer()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }
  return NS_OK;
}

nsresult
nsAutoCompleteController::EnterMatch()
{
  // If a search is still ongoing, bail out of this function
  // and let the search finish, and tell it to come back here when it's done
  if (mSearchStatus == nsIAutoCompleteController::STATUS_SEARCHING) {
    mEnterAfterSearch = PR_TRUE;
    return NS_OK;
  }
  mEnterAfterSearch = PR_FALSE;
  
  nsCOMPtr<nsIAutoCompletePopup> popup;
  mInput->GetPopup(getter_AddRefs(popup));
  NS_ENSURE_TRUE(popup != nsnull, NS_ERROR_FAILURE);
  
  PRBool forceComplete;
  mInput->GetForceComplete(&forceComplete);
  
  // Ask the popup if it wants to enter a special value into the textbox
  nsAutoString value;
  popup->GetOverrideValue(value);
  if (value.IsEmpty()) {
    // If a row is selected in the popup, enter it into the textbox
    PRInt32 selectedIndex;
    popup->GetSelectedIndex(&selectedIndex);
    if (selectedIndex >= 0)
      GetResultValueAt(selectedIndex, PR_TRUE, value);
    
    if (forceComplete && value.IsEmpty()) {
      // Since nothing was selected, and forceComplete is specified, that means
      // we have to find find the first default match and enter it instead
      PRUint32 count;
      mResults->Count(&count);
      for (PRUint32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIAutoCompleteResult> result;
        mResults->GetElementAt(i, getter_AddRefs(result));

        if (result) {
          PRInt32 defaultIndex;
          result->GetDefaultIndex(&defaultIndex);
          if (defaultIndex >= 0) {
            result->GetValueAt(defaultIndex, value);
            break;
          }
        }
      }
    }
  }
  
  if (!value.IsEmpty()) {
    mInput->SetTextValue(value);
    mInput->SelectTextRange(value.Length(), value.Length());
    mSearchString = value;
  }
  
  ClosePopup();
  
  PRBool cancel;
  mInput->OnTextEntered(&cancel);
  
  return NS_OK;
}

nsresult
nsAutoCompleteController::RevertTextValue()
{
  nsAutoString oldValue(mSearchString);
  
  PRBool cancel = PR_FALSE;
  mInput->OnTextReverted(&cancel);  

  if (!cancel)
    mInput->SetTextValue(oldValue);

  return NS_OK;
}

nsresult
nsAutoCompleteController::ProcessResult(PRInt32 aSearchIndex, nsIAutoCompleteResult *aResult)
{
  NS_ENSURE_STATE(mInput);
  // If this is the first search to return, we should clear out the previous cached results
  PRUint32 searchCount;
  mSearches->Count(&searchCount);
  if (mSearchesOngoing == searchCount)
    ClearResults();

  --mSearchesOngoing;
  
  // Cache the result
  mResults->AppendElement(aResult);

  // If the search failed, increase the match count to include the error description
  PRUint16 result = 0;
  PRUint32 oldRowCount = mRowCount;

  if (aResult)
    aResult->GetSearchResult(&result);
  if (result == nsIAutoCompleteResult::RESULT_FAILURE) {
    nsAutoString error;
    aResult->GetErrorDescription(error);
    if (!error.IsEmpty()) {
      ++mRowCount;
      if (mTree)
        mTree->RowCountChanged(oldRowCount, 1);
    }
  } else if (result == nsIAutoCompleteResult::RESULT_SUCCESS) {
    // Increase the match count for all matches in this result
    PRUint32 matchCount = 0;
    aResult->GetMatchCount(&matchCount);
    mRowCount += matchCount;
    if (mTree)
      mTree->RowCountChanged(oldRowCount, matchCount);

    // Try to autocomplete the default index for this search
    CompleteDefaultIndex(aSearchIndex);
  }

  // Refresh the popup view to display the new search results
  nsCOMPtr<nsIAutoCompletePopup> popup;
  mInput->GetPopup(getter_AddRefs(popup));
  NS_ENSURE_TRUE(popup != nsnull, NS_ERROR_FAILURE);
  popup->Invalidate();
  
  // Make sure the popup is open, if necessary, since we now
  // have at least one search result ready to display
  if (mRowCount)
    OpenPopup();
  else
    ClosePopup();

  // If this is the last search to return, cleanup
  if (mSearchesOngoing == 0)
    PostSearchCleanup();

  return NS_OK;
}

nsresult
nsAutoCompleteController::PostSearchCleanup()
{  
  if (mRowCount) {
    OpenPopup();
    mSearchStatus = nsIAutoCompleteController::STATUS_COMPLETE_MATCH;
  } else {
    mSearchStatus = nsIAutoCompleteController::STATUS_COMPLETE_NO_MATCH;
    ClosePopup();
  }
  
  // notify the input that the search is complete
  mInput->OnSearchComplete();
  
  // if mEnterAfterSearch was set, then the user hit enter while the search was ongoing,
  // so we need to enter a match now that the search is done
  if (mEnterAfterSearch)
    EnterMatch();

  return NS_OK;
}

nsresult
nsAutoCompleteController::ClearResults()
{
  PRInt32 oldRowCount = mRowCount;
  mRowCount = 0;
  mResults->Clear();
  if (oldRowCount != 0 && mTree)
    mTree->RowCountChanged(0, -oldRowCount);
  return NS_OK;
}

nsresult
nsAutoCompleteController::CompleteDefaultIndex(PRInt32 aSearchIndex)
{
  if (mDefaultIndexCompleted || mEnterAfterSearch || mBackspaced || mRowCount == 0 || mSearchString.Length() == 0)
    return NS_OK;

  PRBool shouldComplete;
  mInput->GetCompleteDefaultIndex(&shouldComplete);
  if (!shouldComplete)
    return NS_OK;

  nsCOMPtr<nsIAutoCompleteSearch> search;
  mSearches->GetElementAt(aSearchIndex, getter_AddRefs(search));
  nsCOMPtr<nsIAutoCompleteResult> result;
  mResults->GetElementAt(aSearchIndex, getter_AddRefs(result));
  NS_ENSURE_TRUE(result != nsnull, NS_ERROR_FAILURE);
  
  // The search must explicitly provide a default index in order
  // for us to be able to complete 
  PRInt32 defaultIndex;
  result->GetDefaultIndex(&defaultIndex);
  NS_ENSURE_TRUE(defaultIndex >= 0, NS_OK);

  nsAutoString resultValue;
  result->GetValueAt(defaultIndex, resultValue);
  CompleteValue(resultValue, PR_TRUE);
  
  mDefaultIndexCompleted = PR_TRUE;

  return NS_OK;
}

nsresult
nsAutoCompleteController::CompleteValue(nsString &aValue, 
                                        PRBool selectDifference)
/* mInput contains mSearchString, which we want to autocomplete to aValue.  If
 * selectDifference is true, select the remaining portion of aValue not 
 * contained in mSearchString. */
{
  const PRInt32 mSearchStringLength = mSearchString.Length();
  PRInt32 endSelect = aValue.Length();  // By default, select all of aValue.

  if (aValue.IsEmpty() || 
      StringBeginsWith(aValue, mSearchString,
                       nsCaseInsensitiveStringComparator())) {
    // aValue is empty (we were asked to clear mInput), or mSearchString 
    // matches the beginning of aValue.  In either case we can simply 
    // autocomplete to aValue.
    mInput->SetTextValue(aValue);
  } else {
    PRInt32 findIndex;  // Offset of mSearchString within aValue.

    nsresult rv;
    nsCOMPtr<nsIIOService> ios = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString scheme;
    if (NS_SUCCEEDED(ios->ExtractScheme(NS_ConvertUTF16toUTF8(aValue), scheme))) {
      // Trying to autocomplete a URI from somewhere other than the beginning.
      // Only succeed if the missing portion is "http://"; otherwise do not
      // autocomplete.  This prevents us from "helpfully" autocompleting to a
      // URI that isn't equivalent to what the user expected.
      findIndex = 7; // length of "http://"

      if ((endSelect < findIndex + mSearchStringLength) ||
          !scheme.LowerCaseEqualsLiteral("http") ||
          !Substring(aValue, findIndex, mSearchStringLength).Equals(
            mSearchString, nsCaseInsensitiveStringComparator())) {
        return NS_OK;
      }
    } else {
      // Autocompleting something other than a URI from the middle.  Assume we
      // can just go ahead and autocomplete the missing final portion; this
      // seems like a problematic assumption...
      nsString::const_iterator iter, end;
      aValue.BeginReading(iter);
      aValue.EndReading(end);
      const nsString::const_iterator::pointer start = iter.get();
      ++iter;  // Skip past beginning since we know that doesn't match

      FindInReadable(mSearchString, iter, end, 
                     nsCaseInsensitiveStringComparator());

      findIndex = iter.get() - start;
    }

    mInput->SetTextValue(mSearchString + 
                         Substring(aValue, mSearchStringLength + findIndex,
                                   endSelect));

    endSelect -= findIndex; // We're skipping this many characters of aValue.
  }

  mInput->SelectTextRange(selectDifference ? 
                          mSearchStringLength : endSelect, endSelect);

  return NS_OK;
}

nsresult
nsAutoCompleteController::GetResultValueAt(PRInt32 aIndex, PRBool aValueOnly, nsAString & _retval)
{
  NS_ENSURE_TRUE(aIndex >= 0 && (PRUint32) aIndex < mRowCount, NS_ERROR_ILLEGAL_VALUE);

  PRInt32 searchIndex;
  PRInt32 rowIndex;
  RowIndexToSearch(aIndex, &searchIndex, &rowIndex);
  NS_ENSURE_TRUE(searchIndex >= 0 && rowIndex >= 0, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIAutoCompleteResult> result;
  mResults->GetElementAt(searchIndex, getter_AddRefs(result));
  NS_ENSURE_TRUE(result != nsnull, NS_ERROR_FAILURE);
  
  PRUint16 searchResult;
  result->GetSearchResult(&searchResult);
  
  if (searchResult == nsIAutoCompleteResult::RESULT_FAILURE) {
    if (aValueOnly)
      return NS_ERROR_FAILURE;
    result->GetErrorDescription(_retval);
  } else if (searchResult == nsIAutoCompleteResult::RESULT_SUCCESS) {
    result->GetValueAt(rowIndex, _retval);
  }
  
  return NS_OK;
}

nsresult
nsAutoCompleteController::RowIndexToSearch(PRInt32 aRowIndex, PRInt32 *aSearchIndex, PRInt32 *aItemIndex)
{
  *aSearchIndex = -1;
  *aItemIndex = -1;
  
  PRUint32 count;
  mSearches->Count(&count);
  PRUint32 index = 0;
  for (PRUint32 i = 0; i < count; ++i) {
    nsCOMPtr<nsIAutoCompleteResult> result;
    mResults->GetElementAt(i, getter_AddRefs(result));
    if (!result)
      continue;
      
    PRUint16 searchResult;
    result->GetSearchResult(&searchResult);
    
    PRUint32 rowCount = 1;
    if (searchResult == nsIAutoCompleteResult::RESULT_SUCCESS) {
      result->GetMatchCount(&rowCount);
    }
    
    if (index + rowCount-1 >= (PRUint32) aRowIndex) {
      *aSearchIndex = i;
      *aItemIndex = aRowIndex - index;
      return NS_OK;
    }

    index += rowCount;
  }

  return NS_OK;
}

nsIWidget*
nsAutoCompleteController::GetPopupWidget()
{
  NS_ENSURE_TRUE(mInput, nsnull);

  nsCOMPtr<nsIAutoCompletePopup> autoCompletePopup;
  mInput->GetPopup(getter_AddRefs(autoCompletePopup));
  NS_ENSURE_TRUE(autoCompletePopup, nsnull);

  nsCOMPtr<nsIDOMNode> popup = do_QueryInterface(autoCompletePopup);
  NS_ENSURE_TRUE(popup, nsnull);

  nsCOMPtr<nsIDOMDocument> domDoc;
  popup->GetOwnerDocument(getter_AddRefs(domDoc));
  NS_ENSURE_TRUE(domDoc, nsnull);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  NS_ENSURE_TRUE(doc, nsnull);

  nsIPresShell* presShell = doc->GetShellAt(0);
  NS_ENSURE_TRUE(presShell, nsnull);

  nsCOMPtr<nsIContent> content = do_QueryInterface(popup);
  nsIFrame* frame = presShell->GetPrimaryFrameFor(content);
  NS_ENSURE_TRUE(frame, nsnull);

  return frame->GetWindow();
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAutoCompleteController)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAutoCompleteSimpleResult)
#ifdef MOZ_MORK
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAutoCompleteMdbResult)
#endif

static const nsModuleComponentInfo components[] =
{
  { "AutoComplete Controller",
    NS_AUTOCOMPLETECONTROLLER_CID, 
    NS_AUTOCOMPLETECONTROLLER_CONTRACTID,
    nsAutoCompleteControllerConstructor },

  { "AutoComplete Simple Result",
    NS_AUTOCOMPLETESIMPLERESULT_CID, 
    NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID,
    nsAutoCompleteSimpleResultConstructor },

#ifdef MOZ_MORK
  { "AutoComplete Mdb Result",
    NS_AUTOCOMPLETEMDBRESULT_CID, 
    NS_AUTOCOMPLETEMDBRESULT_CONTRACTID,
    nsAutoCompleteMdbResultConstructor },
#endif
};

NS_IMPL_NSGETMODULE(tkAutoCompleteModule, components)
