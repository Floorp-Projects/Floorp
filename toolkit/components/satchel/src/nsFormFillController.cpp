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

#include "nsFormFillController.h"

#ifdef MOZ_STORAGE_SATCHEL
#include "nsStorageFormHistory.h"
#include "nsIAutoCompleteSimpleResult.h"
#else
#include "nsFormHistory.h"
#include "nsIAutoCompleteResultTypes.h"
#endif
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsPIDOMWindow.h"
#include "nsIWebNavigation.h"
#include "nsIContentViewer.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMKeyEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMCompositionListener.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsRect.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsPasswordManager.h"
#include "nsSingleSignonPrompt.h"
#include "nsIDOMMouseEvent.h"
#include "nsIGenericFactory.h"
#include "nsToolkitCompsCID.h"
#include "nsEmbedCID.h"

NS_INTERFACE_MAP_BEGIN(nsFormFillController)
  NS_INTERFACE_MAP_ENTRY(nsIFormFillController)
  NS_INTERFACE_MAP_ENTRY(nsIAutoCompleteInput)
  NS_INTERFACE_MAP_ENTRY(nsIAutoCompleteSearch)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFormListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLoadListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCompositionListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMContextMenuListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIFormFillController)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMFocusListener)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsFormFillController)
NS_IMPL_RELEASE(nsFormFillController)

nsFormFillController::nsFormFillController() :
  mTimeout(50),
  mMinResultsForPopup(1),
  mMaxRows(0),
  mDisableAutoComplete(PR_FALSE), 
  mCompleteDefaultIndex(PR_FALSE),
  mCompleteSelectedIndex(PR_FALSE),
  mForceComplete(PR_FALSE),
  mSuppressOnInput(PR_FALSE)
{
  mController = do_GetService("@mozilla.org/autocomplete/controller;1");

  mDocShells = do_CreateInstance("@mozilla.org/supports-array;1");
  mPopups = do_CreateInstance("@mozilla.org/supports-array;1");
}

nsFormFillController::~nsFormFillController()
{
  // Remove ourselves as a focus listener from all cached docShells
  PRUint32 count;
  mDocShells->Count(&count);
  for (PRUint32 i = 0; i < count; ++i) {
    nsCOMPtr<nsIDocShell> docShell;
    mDocShells->GetElementAt(i, getter_AddRefs(docShell));
    nsCOMPtr<nsIDOMWindow> domWindow = GetWindowForDocShell(docShell);
    RemoveWindowListeners(domWindow);
  }
}

////////////////////////////////////////////////////////////////////////

nsRect
GetScreenOrigin(nsIDOMElement* aElement)
{
  nsRect rect(0,0,0,0);

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  nsCOMPtr<nsIDocument> doc = content->GetDocument();

  if (doc) {
    // Get Presentation shell 0
    nsIPresShell* presShell = doc->GetShellAt(0);

    if (presShell) {
      nsIFrame* frame = presShell->GetPrimaryFrameFor(content);
      if (!frame)
        return rect;
      rect = frame->GetScreenRectExternal();
    }
  }
  
  return rect;
}

////////////////////////////////////////////////////////////////////////
//// nsIFormFillController

NS_IMETHODIMP
nsFormFillController::AttachToBrowser(nsIDocShell *aDocShell, nsIAutoCompletePopup *aPopup)
{
  NS_ENSURE_TRUE(aDocShell && aPopup, NS_ERROR_ILLEGAL_VALUE);
  
  mDocShells->AppendElement(aDocShell);
  mPopups->AppendElement(aPopup);
  
  // Listen for focus events on the domWindow of the docShell
  nsCOMPtr<nsIDOMWindow> domWindow = GetWindowForDocShell(aDocShell);
  AddWindowListeners(domWindow);

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::DetachFromBrowser(nsIDocShell *aDocShell)
{
  PRInt32 index = GetIndexOfDocShell(aDocShell);
  NS_ENSURE_TRUE(index >= 0, NS_ERROR_FAILURE);
  
  // Stop listening for focus events on the domWindow of the docShell
  nsCOMPtr<nsIDocShell> docShell;
  mDocShells->GetElementAt(index, getter_AddRefs(docShell));
  nsCOMPtr<nsIDOMWindow> domWindow = GetWindowForDocShell(docShell);
  RemoveWindowListeners(domWindow);
    
  mDocShells->RemoveElementAt(index);
  mPopups->RemoveElementAt(index);
  
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIAutoCompleteInput

NS_IMETHODIMP
nsFormFillController::GetPopup(nsIAutoCompletePopup **aPopup)
{
  *aPopup = mFocusedPopup;
  NS_IF_ADDREF(*aPopup);
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetController(nsIAutoCompleteController **aController)
{
  *aController = mController;
  NS_IF_ADDREF(*aController);
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetPopupOpen(PRBool *aPopupOpen)
{
  if (mFocusedPopup)
    mFocusedPopup->GetPopupOpen(aPopupOpen);
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetPopupOpen(PRBool aPopupOpen)
{
  if (mFocusedPopup) {
    if (aPopupOpen) {
      // make sure input field is visible before showing popup (bug 320938)
      nsCOMPtr<nsIContent> content = do_QueryInterface(mFocusedInput);
      NS_ENSURE_STATE(content);
      nsCOMPtr<nsIDocShell> docShell = GetDocShellForInput(mFocusedInput);
      NS_ENSURE_STATE(docShell);
      nsCOMPtr<nsIPresShell> presShell;
      docShell->GetPresShell(getter_AddRefs(presShell));
      NS_ENSURE_STATE(presShell);
      presShell->ScrollContentIntoView(content,
                                       NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,
                                       NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);

      nsRect popupRect = GetScreenOrigin(mFocusedInput);
      mFocusedPopup->OpenPopup(this, popupRect.x, popupRect.y+popupRect.height, popupRect.width);
    } else
      mFocusedPopup->ClosePopup();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetDisableAutoComplete(PRBool *aDisableAutoComplete)
{
  *aDisableAutoComplete = mDisableAutoComplete;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetDisableAutoComplete(PRBool aDisableAutoComplete)
{
  mDisableAutoComplete = aDisableAutoComplete;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetCompleteDefaultIndex(PRBool *aCompleteDefaultIndex)
{
  *aCompleteDefaultIndex = mCompleteDefaultIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetCompleteDefaultIndex(PRBool aCompleteDefaultIndex)
{
  mCompleteDefaultIndex = aCompleteDefaultIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetCompleteSelectedIndex(PRBool *aCompleteSelectedIndex)
{
  *aCompleteSelectedIndex = mCompleteSelectedIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetCompleteSelectedIndex(PRBool aCompleteSelectedIndex)
{
  mCompleteSelectedIndex = aCompleteSelectedIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetForceComplete(PRBool *aForceComplete)
{
  *aForceComplete = mForceComplete;
  return NS_OK;
}

NS_IMETHODIMP nsFormFillController::SetForceComplete(PRBool aForceComplete)
{
  mForceComplete = aForceComplete;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetMinResultsForPopup(PRUint32 *aMinResultsForPopup)
{
  *aMinResultsForPopup = mMinResultsForPopup;
  return NS_OK;
}

NS_IMETHODIMP nsFormFillController::SetMinResultsForPopup(PRUint32 aMinResultsForPopup)
{
  mMinResultsForPopup = aMinResultsForPopup;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetMaxRows(PRUint32 *aMaxRows)
{
  *aMaxRows = mMaxRows;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetMaxRows(PRUint32 aMaxRows)
{
  mMaxRows = aMaxRows;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetShowCommentColumn(PRUint32 *aShowCommentColumn)
{
  *aShowCommentColumn = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsFormFillController::SetShowCommentColumn(PRUint32 aShowCommentColumn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFormFillController::GetTimeout(PRUint32 *aTimeout)
{
  *aTimeout = mTimeout;
  return NS_OK;
}

NS_IMETHODIMP nsFormFillController::SetTimeout(PRUint32 aTimeout)
{
  mTimeout = aTimeout;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetSearchParam(const nsAString &aSearchParam)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFormFillController::GetSearchParam(nsAString &aSearchParam)
{
  if (!mFocusedInput) {
    NS_WARNING("mFocusedInput is null for some reason! avoiding a crash. should find out why... - ben");
    return NS_ERROR_FAILURE; // XXX why? fix me. 
  }
    
  mFocusedInput->GetName(aSearchParam);
  if (aSearchParam.IsEmpty())
    mFocusedInput->GetId(aSearchParam);
  
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetSearchCount(PRUint32 *aSearchCount)
{
  *aSearchCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetSearchAt(PRUint32 index, nsACString & _retval)
{
  _retval.Assign("form-history");
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetTextValue(nsAString & aTextValue)
{
  if (mFocusedInput) {
    mFocusedInput->GetValue(aTextValue);
  } else {
    aTextValue.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetTextValue(const nsAString & aTextValue)
{
  if (mFocusedInput) {
    mSuppressOnInput = PR_TRUE;
    mFocusedInput->SetValue(aTextValue);
    mSuppressOnInput = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetSelectionStart(PRInt32 *aSelectionStart)
{
  nsCOMPtr<nsIDOMNSHTMLInputElement> input = do_QueryInterface(mFocusedInput);
  if (input)
    input->GetSelectionStart(aSelectionStart);
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetSelectionEnd(PRInt32 *aSelectionEnd)
{
  nsCOMPtr<nsIDOMNSHTMLInputElement> input = do_QueryInterface(mFocusedInput);
  if (input)
    input->GetSelectionEnd(aSelectionEnd);
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SelectTextRange(PRInt32 aStartIndex, PRInt32 aEndIndex)
{
  nsCOMPtr<nsIDOMNSHTMLInputElement> input = do_QueryInterface(mFocusedInput);
  if (input)
    input->SetSelectionRange(aStartIndex, aEndIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::OnSearchComplete()
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::OnTextEntered(PRBool* aPrevent)
{
  NS_ENSURE_ARG(aPrevent);
  NS_ENSURE_TRUE(mFocusedInput, NS_OK);
  // Fire off a DOMAutoComplete event
  nsCOMPtr<nsIDOMDocument> domDoc;
  mFocusedInput->GetOwnerDocument(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDOMDocumentEvent> doc = do_QueryInterface(domDoc);
  NS_ENSURE_STATE(doc);

  nsCOMPtr<nsIDOMEvent> event;
  doc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(event));
  NS_ENSURE_STATE(privateEvent);

  event->InitEvent(NS_LITERAL_STRING("DOMAutoComplete"), PR_TRUE, PR_TRUE);

  // XXXjst: We mark this event as a trusted event, it's up to the
  // callers of this to ensure that it's only called from trusted
  // code.
  privateEvent->SetTrusted(PR_TRUE);

  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(mFocusedInput);

  PRBool defaultActionEnabled;
  targ->DispatchEvent(event, &defaultActionEnabled);
  *aPrevent = !defaultActionEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::OnTextReverted(PRBool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetConsumeRollupEvent(PRBool *aConsumeRollupEvent)
{
  *aConsumeRollupEvent = PR_FALSE;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIAutoCompleteSearch


NS_IMETHODIMP
nsFormFillController::StartSearch(const nsAString &aSearchString, const nsAString &aSearchParam,
                                  nsIAutoCompleteResult *aPreviousResult, nsIAutoCompleteObserver *aListener)
{
  nsCOMPtr<nsIAutoCompleteResult> result;

#ifdef MOZ_STORAGE_SATCHEL
  // This assumes that FormHistory uses nsIAutoCompleteSimpleResult,
  // while PasswordManager does not.
  nsCOMPtr<nsIAutoCompleteSimpleResult> historyResult;
#else
  nsCOMPtr<nsIAutoCompleteMdbResult2> historyResult;
#endif
  historyResult = do_QueryInterface(aPreviousResult);

  nsPasswordManager* passMgr = nsPasswordManager::GetInstance();
  if (!passMgr)
    return NS_ERROR_OUT_OF_MEMORY;

  // Only hand off a previous result to the password manager if it's
  // a password manager result (i.e. not an nsIAutoCompleteMdb/SimpleResult).

  if (!passMgr->AutoCompleteSearch(aSearchString,
                                   historyResult ? nsnull : aPreviousResult,
                                   mFocusedInput,
                                   getter_AddRefs(result)))
  {
    nsFormHistory *history = nsFormHistory::GetInstance();
    if (history) {
      history->AutoCompleteSearch(aSearchParam,
                                  aSearchString,
                                  historyResult,
                                  getter_AddRefs(result));
    }
  }
  NS_RELEASE(passMgr);

  aListener->OnSearchResult(this, result);  
  
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::StopSearch()
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIDOMEventListener

NS_IMETHODIMP
nsFormFillController::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK; 
}

////////////////////////////////////////////////////////////////////////
//// nsIDOMFocusListener

NS_IMETHODIMP
nsFormFillController::Focus(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetTarget(getter_AddRefs(target));
  
  nsCOMPtr<nsIDOMHTMLInputElement> input = do_QueryInterface(target);
  if (!input)
    return NS_OK;
    
    nsAutoString type;
    input->GetType(type);

    PRBool isReadOnly = PR_FALSE;
    input->GetReadOnly(&isReadOnly);
                                  
    nsAutoString autocomplete; 
    input->GetAttribute(NS_LITERAL_STRING("autocomplete"), autocomplete);
    if (type.LowerCaseEqualsLiteral("text") && !isReadOnly &&
        !autocomplete.LowerCaseEqualsLiteral("off")) {

      nsCOMPtr<nsIDOMHTMLFormElement> form;
      input->GetForm(getter_AddRefs(form));
      if (form)
        form->GetAttribute(NS_LITERAL_STRING("autocomplete"), autocomplete);

      if (!form || !autocomplete.LowerCaseEqualsLiteral("off"))
        StartControllingInput(input);
    }
    
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::Blur(nsIDOMEvent* aEvent)
{
  if (mFocusedInput)
    StopControllingInput();
  
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIDOMKeyListener

NS_IMETHODIMP
nsFormFillController::KeyDown(nsIDOMEvent* aEvent)
{
  return NS_OK;
} 

NS_IMETHODIMP
nsFormFillController::KeyUp(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::KeyPress(nsIDOMEvent* aEvent)
{
  NS_ASSERTION(mController, "should have a controller!");
  if (!mFocusedInput || !mController)
    return NS_OK;

  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aEvent);
  if (!keyEvent)
    return NS_ERROR_FAILURE;

  PRBool cancel = PR_FALSE;

  PRUint32 k;
  keyEvent->GetKeyCode(&k);
  switch (k) {
  case nsIDOMKeyEvent::DOM_VK_DELETE:
#ifndef XP_MACOSX
    mController->HandleDelete(&cancel);
    break;
  case nsIDOMKeyEvent::DOM_VK_BACK_SPACE:
    mController->HandleText(PR_FALSE);
    break;
#else
  case nsIDOMKeyEvent::DOM_VK_BACK_SPACE:
    {
      PRBool isShift = PR_FALSE;
      keyEvent->GetShiftKey(&isShift);

      if (isShift)
        mController->HandleDelete(&cancel);
      else
        mController->HandleText(PR_FALSE);

      break;
    }
#endif
  case nsIDOMKeyEvent::DOM_VK_UP:
  case nsIDOMKeyEvent::DOM_VK_DOWN:
  case nsIDOMKeyEvent::DOM_VK_LEFT:
  case nsIDOMKeyEvent::DOM_VK_RIGHT:
  case nsIDOMKeyEvent::DOM_VK_PAGE_UP:
  case nsIDOMKeyEvent::DOM_VK_PAGE_DOWN:
    mController->HandleKeyNavigation(k, &cancel);
    break;
  case nsIDOMKeyEvent::DOM_VK_ESCAPE:
    mController->HandleEscape(&cancel);
    break;
  case nsIDOMKeyEvent::DOM_VK_TAB:
    mController->HandleTab();
    cancel = PR_FALSE;
    break;
  case nsIDOMKeyEvent::DOM_VK_RETURN:
    mController->HandleEnter(&cancel);
    break;
  }
  
  if (cancel) {
    aEvent->StopPropagation();
    aEvent->PreventDefault();
  }
  
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIDOMCompositionListener

NS_IMETHODIMP
nsFormFillController::HandleStartComposition(nsIDOMEvent* aCompositionEvent)
{
  NS_ASSERTION(mController, "should have a controller!");

  if (mController && mFocusedInput)
    mController->HandleStartComposition();

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::HandleEndComposition(nsIDOMEvent* aCompositionEvent)
{
  NS_ASSERTION(mController, "should have a controller!");

  if (mController && mFocusedInput)
    mController->HandleEndComposition();

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::HandleQueryComposition(nsIDOMEvent* aCompositionEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::HandleQueryReconversion(nsIDOMEvent* aCompositionEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::HandleQueryCaretRect(nsIDOMEvent* aCompostionEvent)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIDOMFormListener

NS_IMETHODIMP
nsFormFillController::Submit(nsIDOMEvent* aEvent)
{
  if (mFocusedInput)
    StopControllingInput();

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::Reset(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::Change(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::Select(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::Input(nsIDOMEvent* aEvent)
{
  if (mSuppressOnInput || !mController || !mFocusedInput)
    return NS_OK;

  return mController->HandleText(PR_FALSE);
}

////////////////////////////////////////////////////////////////////////
//// nsIDOMMouseListener

NS_IMETHODIMP
nsFormFillController::MouseDown(nsIDOMEvent* aMouseEvent)
{
  mIgnoreClick = PR_FALSE;

  nsCOMPtr<nsIDOMEventTarget> target;
  aMouseEvent->GetTarget(getter_AddRefs(target));

  nsCOMPtr<nsIDOMHTMLInputElement> targetInput = do_QueryInterface(target);
  if (!targetInput || (targetInput && targetInput != mFocusedInput)) {
    // A new input will be taking focus.  Ignore the first click
    // so that the popup is not shown.
    mIgnoreClick = PR_TRUE;
    return NS_OK;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::MouseClick(nsIDOMEvent* aMouseEvent)
{
  if (mIgnoreClick) {
    mIgnoreClick = PR_FALSE;
    return NS_OK;
  }

  if (!mFocusedInput)
    return NS_OK;

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aMouseEvent));
  if (!mouseEvent)
    return NS_ERROR_FAILURE;

  PRUint16 button;
  mouseEvent->GetButton(&button);
  if (button != 0)
    return NS_OK;

  PRBool isOpen = PR_FALSE;
  GetPopupOpen(&isOpen);
  if (isOpen)
    return NS_OK;

  nsCOMPtr<nsIAutoCompleteInput> input;
  mController->GetInput(getter_AddRefs(input));
  if (!input)
    return NS_OK;

  nsAutoString value;
  input->GetTextValue(value);
  if (value.Length() > 0) {
    // Show the popup with a filtered result set
    mController->SetSearchString(EmptyString());
    mController->HandleText(PR_TRUE);
  } else {
    // Show the popup with the complete result set.  Can't use HandleText()
    // because it doesn't display the popup if the input is blank.
    PRBool cancel = PR_FALSE;
    mController->HandleKeyNavigation(nsIDOMKeyEvent::DOM_VK_DOWN, &cancel);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::MouseDblClick(nsIDOMEvent* aMouseEvent)
{ 
    return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIDOMLoadListener

NS_IMETHODIMP
nsFormFillController::Load(nsIDOMEvent *aLoadEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::BeforeUnload(nsIDOMEvent *aLoadEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::Unload(nsIDOMEvent *aLoadEvent)
{
  if (mFocusedInput) {
    nsCOMPtr<nsIDOMEventTarget> target;
    aLoadEvent->GetTarget(getter_AddRefs(target));

    nsCOMPtr<nsIDOMDocument> eventDoc = do_QueryInterface(target);
    nsCOMPtr<nsIDOMDocument> inputDoc;
    mFocusedInput->GetOwnerDocument(getter_AddRefs(inputDoc));

    if (eventDoc == inputDoc)
      StopControllingInput();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::Abort(nsIDOMEvent *aLoadEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::Error(nsIDOMEvent *aLoadEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::ContextMenu(nsIDOMEvent* aContextMenuEvent)
{
  if (mFocusedPopup)
    mFocusedPopup->ClosePopup();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsFormFillController

void
nsFormFillController::AddWindowListeners(nsIDOMWindow *aWindow)
{
  if (!aWindow)
    return;

  nsCOMPtr<nsPIDOMWindow> privateDOMWindow(do_QueryInterface(aWindow));
  nsPIDOMEventTarget* chromeEventHandler = nsnull;
  if (privateDOMWindow)
    chromeEventHandler = privateDOMWindow->GetChromeEventHandler();

  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(chromeEventHandler));

  if (!target)
    return;

  target->AddEventListener(NS_LITERAL_STRING("focus"),
                           NS_STATIC_CAST(nsIDOMFocusListener *, this),
                           PR_TRUE);

  target->AddEventListener(NS_LITERAL_STRING("blur"),
                           NS_STATIC_CAST(nsIDOMFocusListener *, this),
                           PR_TRUE);

  target->AddEventListener(NS_LITERAL_STRING("mousedown"),
                           NS_STATIC_CAST(nsIDOMMouseListener *, this),
                           PR_TRUE);

  target->AddEventListener(NS_LITERAL_STRING("click"),
                           NS_STATIC_CAST(nsIDOMMouseListener *, this),
                           PR_TRUE);

  target->AddEventListener(NS_LITERAL_STRING("input"),
                           NS_STATIC_CAST(nsIDOMFormListener *, this),
                           PR_TRUE);

  target->AddEventListener(NS_LITERAL_STRING("unload"),
                           NS_STATIC_CAST(nsIDOMLoadListener *, this),
                           PR_TRUE);

  target->AddEventListener(NS_LITERAL_STRING("compositionstart"),
                           NS_STATIC_CAST(nsIDOMCompositionListener *, this),
                           PR_TRUE);

  target->AddEventListener(NS_LITERAL_STRING("compositionend"),
                           NS_STATIC_CAST(nsIDOMCompositionListener *, this),
                           PR_TRUE);

  target->AddEventListener(NS_LITERAL_STRING("contextmenu"),
                           NS_STATIC_CAST(nsIDOMContextMenuListener *, this),
                           PR_TRUE);

  target->AddEventListener(NS_LITERAL_STRING("keypress"),
                           NS_STATIC_CAST(nsIDOMKeyListener *, this),
                           PR_TRUE);
}

void
nsFormFillController::RemoveWindowListeners(nsIDOMWindow *aWindow)
{
  if (!aWindow)
    return;

  StopControllingInput();
  
  nsCOMPtr<nsPIDOMWindow> privateDOMWindow(do_QueryInterface(aWindow));
  nsPIDOMEventTarget* chromeEventHandler = nsnull;
  if (privateDOMWindow)
    chromeEventHandler = privateDOMWindow->GetChromeEventHandler();
  
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(chromeEventHandler));

  if (!target)
    return;

  target->RemoveEventListener(NS_LITERAL_STRING("focus"),
                              NS_STATIC_CAST(nsIDOMFocusListener *, this),
                              PR_TRUE);

  target->RemoveEventListener(NS_LITERAL_STRING("blur"),
                              NS_STATIC_CAST(nsIDOMFocusListener *, this),
                              PR_TRUE);

  target->RemoveEventListener(NS_LITERAL_STRING("mousedown"),
                              NS_STATIC_CAST(nsIDOMMouseListener *, this),
                              PR_TRUE);

  target->RemoveEventListener(NS_LITERAL_STRING("click"),
                              NS_STATIC_CAST(nsIDOMMouseListener *, this),
                              PR_TRUE);

  target->RemoveEventListener(NS_LITERAL_STRING("input"),
                              NS_STATIC_CAST(nsIDOMFormListener *, this),
                              PR_TRUE);

  target->RemoveEventListener(NS_LITERAL_STRING("unload"),
                              NS_STATIC_CAST(nsIDOMLoadListener *, this),
                              PR_TRUE);

  target->RemoveEventListener(NS_LITERAL_STRING("compositionstart"),
                              NS_STATIC_CAST(nsIDOMCompositionListener *, this),
                              PR_TRUE);

  target->RemoveEventListener(NS_LITERAL_STRING("compositionend"),
                              NS_STATIC_CAST(nsIDOMCompositionListener *, this),
                              PR_TRUE);

  target->RemoveEventListener(NS_LITERAL_STRING("contextmenu"),
                              NS_STATIC_CAST(nsIDOMContextMenuListener *, this),
                              PR_TRUE);

  target->RemoveEventListener(NS_LITERAL_STRING("keypress"),
                              NS_STATIC_CAST(nsIDOMKeyListener *, this),
                              PR_TRUE);
}

void
nsFormFillController::StartControllingInput(nsIDOMHTMLInputElement *aInput)
{
  // Make sure we're not still attached to an input
  StopControllingInput(); 

  // Find the currently focused docShell
  nsCOMPtr<nsIDocShell> docShell = GetDocShellForInput(aInput);
  PRInt32 index = GetIndexOfDocShell(docShell);
  if (index < 0)
    return;
  
  // Cache the popup for the focused docShell
  mPopups->GetElementAt(index, getter_AddRefs(mFocusedPopup));
  
  mFocusedInput = aInput;

  // Now we are the autocomplete controller's bitch
  mController->SetInput(this);
}

void
nsFormFillController::StopControllingInput()
{
  // Reset the controller's input, but not if it has been switched
  // to another input already, which might happen if the user switches
  // focus by clicking another autocomplete textbox
  nsCOMPtr<nsIAutoCompleteInput> input;
  mController->GetInput(getter_AddRefs(input));
  if (input == this)
    mController->SetInput(nsnull);

  mFocusedInput = nsnull;
  mFocusedPopup = nsnull;
}

nsIDocShell *
nsFormFillController::GetDocShellForInput(nsIDOMHTMLInputElement *aInput)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  aInput->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  NS_ENSURE_TRUE(doc, nsnull);
  nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(doc->GetWindow());
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(webNav);
  return docShell;
}

nsIDOMWindow *
nsFormFillController::GetWindowForDocShell(nsIDocShell *aDocShell)
{
  nsCOMPtr<nsIContentViewer> contentViewer;
  aDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  NS_ENSURE_TRUE(contentViewer, nsnull);

  nsCOMPtr<nsIDOMDocument> domDoc;
  contentViewer->GetDOMDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  NS_ENSURE_TRUE(doc, nsnull);

  return doc->GetWindow();
}

PRInt32
nsFormFillController::GetIndexOfDocShell(nsIDocShell *aDocShell)
{
  if (!aDocShell)
    return -1;

  // Loop through our cached docShells looking for the given docShell
  PRUint32 count;
  mDocShells->Count(&count);
  for (PRUint32 i = 0; i < count; ++i) {
    nsCOMPtr<nsIDocShell> docShell;
    mDocShells->GetElementAt(i, getter_AddRefs(docShell));
    if (docShell == aDocShell)
      return i;
  }

  // Recursively check the parent docShell of this one
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(aDocShell);
  nsCOMPtr<nsIDocShellTreeItem> parentItem;
  treeItem->GetParent(getter_AddRefs(parentItem));
  if (parentItem) {
    nsCOMPtr<nsIDocShell> parentShell = do_QueryInterface(parentItem);
    return GetIndexOfDocShell(parentShell);
  }
    
  return -1;
}

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsFormHistory, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFormFillController)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsPasswordManager, nsPasswordManager::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSingleSignonPrompt)
#if defined(MOZ_STORAGE_SATCHEL) && defined(MOZ_MORKREADER)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFormHistoryImporter)
#endif

static void PR_CALLBACK nsFormHistoryModuleDtor(nsIModule* self)
{
  nsPasswordManager::Shutdown();
}

static const nsModuleComponentInfo components[] =
{
  { "Password Manager",
    NS_PASSWORDMANAGER_CID,
    NS_PASSWORDMANAGER_CONTRACTID,
    nsPasswordManagerConstructor,
    nsPasswordManager::Register,
    nsPasswordManager::Unregister },

  { "Password Manager",
    NS_PASSWORDMANAGER_CID,
    NS_PWMGR_AUTHPROMPTFACTORY,
    nsPasswordManagerConstructor,
    nsPasswordManager::Register,
    nsPasswordManager::Unregister },

  { "Single Signon Prompt",
    NS_SINGLE_SIGNON_PROMPT_CID,
    "@mozilla.org/wallet/single-sign-on-prompt;1",
    nsSingleSignonPromptConstructor },

  { "HTML Form History",
    NS_FORMHISTORY_CID, 
    NS_FORMHISTORY_CONTRACTID,
    nsFormHistoryConstructor },

  { "HTML Form Fill Controller",
    NS_FORMFILLCONTROLLER_CID, 
    "@mozilla.org/satchel/form-fill-controller;1",
    nsFormFillControllerConstructor },

  { "HTML Form History AutoComplete",
    NS_FORMFILLCONTROLLER_CID, 
    NS_FORMHISTORYAUTOCOMPLETE_CONTRACTID,
    nsFormFillControllerConstructor },

#if defined(MOZ_STORAGE_SATCHEL) && defined(MOZ_MORKREADER)
  { "Form History Importer",
    NS_FORMHISTORYIMPORTER_CID,
    NS_FORMHISTORYIMPORTER_CONTRACTID,
    nsFormHistoryImporterConstructor },
#endif
};

NS_IMPL_NSGETMODULE_WITH_DTOR(satchel, components, nsFormHistoryModuleDtor)

