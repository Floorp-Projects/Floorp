/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFormFillController.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h" // for nsIDOMEvent::InternalDOMEvent()
#include "nsIFormAutoComplete.h"
#include "nsIInputListAutoComplete.h"
#include "nsIAutoCompleteSimpleResult.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsPIDOMWindow.h"
#include "nsIWebNavigation.h"
#include "nsIContentViewer.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIFormControl.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsRect.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsILoginManager.h"
#include "nsIDOMMouseEvent.h"
#include "mozilla/ModuleUtils.h"
#include "nsToolkitCompsCID.h"
#include "nsEmbedCID.h"
#include "nsIDOMNSEditableElement.h"
#include "nsContentUtils.h"
#include "nsILoadContext.h"
#include "nsIFrame.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION(nsFormFillController,
                         mController, mLoginManager, mFocusedPopup, mDocShells,
                         mPopups, mLastListener, mLastFormAutoComplete)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFormFillController)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIFormFillController)
  NS_INTERFACE_MAP_ENTRY(nsIFormFillController)
  NS_INTERFACE_MAP_ENTRY(nsIAutoCompleteInput)
  NS_INTERFACE_MAP_ENTRY(nsIAutoCompleteSearch)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIFormAutoCompleteObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFormFillController)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFormFillController)



nsFormFillController::nsFormFillController() :
  mFocusedInput(nullptr),
  mFocusedInputNode(nullptr),
  mListNode(nullptr),
  mTimeout(50),
  mMinResultsForPopup(1),
  mMaxRows(0),
  mDisableAutoComplete(false),
  mCompleteDefaultIndex(false),
  mCompleteSelectedIndex(false),
  mForceComplete(false),
  mSuppressOnInput(false)
{
  mController = do_GetService("@mozilla.org/autocomplete/controller;1");
  MOZ_ASSERT(mController);
}

nsFormFillController::~nsFormFillController()
{
  if (mListNode) {
    mListNode->RemoveMutationObserver(this);
    mListNode = nullptr;
  }
  if (mFocusedInputNode) {
    MaybeRemoveMutationObserver(mFocusedInputNode);
    mFocusedInputNode = nullptr;
    mFocusedInput = nullptr;
  }
  RemoveForDocument(nullptr);

  // Remove ourselves as a focus listener from all cached docShells
  uint32_t count = mDocShells.Length();
  for (uint32_t i = 0; i < count; ++i) {
    nsCOMPtr<nsPIDOMWindowOuter> window = GetWindowForDocShell(mDocShells[i]);
    RemoveWindowListeners(window);
  }
}

////////////////////////////////////////////////////////////////////////
//// nsIMutationObserver
//

void
nsFormFillController::AttributeChanged(nsIDocument* aDocument,
                                       mozilla::dom::Element* aElement,
                                       int32_t aNameSpaceID,
                                       nsIAtom* aAttribute, int32_t aModType,
                                       const nsAttrValue* aOldValue)
{
  if ((aAttribute == nsGkAtoms::type || aAttribute == nsGkAtoms::readonly ||
       aAttribute == nsGkAtoms::autocomplete) &&
      aNameSpaceID == kNameSpaceID_None) {
    nsCOMPtr<nsIDOMHTMLInputElement> focusedInput(mFocusedInput);
    // Reset the current state of the controller, unconditionally.
    StopControllingInput();
    // Then restart based on the new values.  We have to delay this
    // to avoid ending up in an endless loop due to re-registering our
    // mutation observer (which would notify us again for *this* event).
    nsCOMPtr<nsIRunnable> event =
      mozilla::NewRunnableMethod<nsCOMPtr<nsIDOMHTMLInputElement>>
      (this, &nsFormFillController::MaybeStartControllingInput, focusedInput);
    NS_DispatchToCurrentThread(event);
  }

  if (mListNode && mListNode->Contains(aElement)) {
    RevalidateDataList();
  }
}

void
nsFormFillController::ContentAppended(nsIDocument* aDocument,
                                      nsIContent* aContainer,
                                      nsIContent* aChild,
                                      int32_t aIndexInContainer)
{
  if (mListNode && mListNode->Contains(aContainer)) {
    RevalidateDataList();
  }
}

void
nsFormFillController::ContentInserted(nsIDocument* aDocument,
                                      nsIContent* aContainer,
                                      nsIContent* aChild,
                                      int32_t aIndexInContainer)
{
  if (mListNode && mListNode->Contains(aContainer)) {
    RevalidateDataList();
  }
}

void
nsFormFillController::ContentRemoved(nsIDocument* aDocument,
                                     nsIContent* aContainer,
                                     nsIContent* aChild,
                                     int32_t aIndexInContainer,
                                     nsIContent* aPreviousSibling)
{
  if (mListNode && mListNode->Contains(aContainer)) {
    RevalidateDataList();
  }
}

void
nsFormFillController::CharacterDataWillChange(nsIDocument* aDocument,
                                              nsIContent* aContent,
                                              CharacterDataChangeInfo* aInfo)
{
}

void
nsFormFillController::CharacterDataChanged(nsIDocument* aDocument,
                                           nsIContent* aContent,
                                           CharacterDataChangeInfo* aInfo)
{
}

void
nsFormFillController::AttributeWillChange(nsIDocument* aDocument,
                                          mozilla::dom::Element* aElement,
                                          int32_t aNameSpaceID,
                                          nsIAtom* aAttribute, int32_t aModType,
                                          const nsAttrValue* aNewValue)
{
}

void
nsFormFillController::NativeAnonymousChildListChange(nsIDocument* aDocument,
                                                     nsIContent* aContent,
                                                     bool aIsRemove)
{
}

void
nsFormFillController::ParentChainChanged(nsIContent* aContent)
{
}

void
nsFormFillController::NodeWillBeDestroyed(const nsINode* aNode)
{
  mPwmgrInputs.Remove(aNode);
  if (aNode == mListNode) {
    mListNode = nullptr;
    RevalidateDataList();
  } else if (aNode == mFocusedInputNode) {
    mFocusedInputNode = nullptr;
    mFocusedInput = nullptr;
  }
}

void
nsFormFillController::MaybeRemoveMutationObserver(nsINode* aNode)
{
  // Nodes being tracked in mPwmgrInputs will have their observers removed when
  // they stop being tracked. 
  if (!mPwmgrInputs.Get(aNode)) {
    aNode->RemoveMutationObserver(this);
  }
}

////////////////////////////////////////////////////////////////////////
//// nsIFormFillController

NS_IMETHODIMP
nsFormFillController::AttachToBrowser(nsIDocShell *aDocShell, nsIAutoCompletePopup *aPopup)
{
  NS_ENSURE_TRUE(aDocShell && aPopup, NS_ERROR_ILLEGAL_VALUE);

  mDocShells.AppendElement(aDocShell);
  mPopups.AppendElement(aPopup);

  // Listen for focus events on the domWindow of the docShell
  nsCOMPtr<nsPIDOMWindowOuter> window = GetWindowForDocShell(aDocShell);
  AddWindowListeners(window);

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::DetachFromBrowser(nsIDocShell *aDocShell)
{
  int32_t index = GetIndexOfDocShell(aDocShell);
  NS_ENSURE_TRUE(index >= 0, NS_ERROR_FAILURE);

  // Stop listening for focus events on the domWindow of the docShell
  nsCOMPtr<nsPIDOMWindowOuter> window =
    GetWindowForDocShell(mDocShells.SafeElementAt(index));
  RemoveWindowListeners(window);

  mDocShells.RemoveElementAt(index);
  mPopups.RemoveElementAt(index);

  return NS_OK;
}


NS_IMETHODIMP
nsFormFillController::MarkAsLoginManagerField(nsIDOMHTMLInputElement *aInput)
{
  /*
   * The Login Manager can supply autocomplete results for username fields,
   * when a user has multiple logins stored for a site. It uses this
   * interface to indicate that the form manager shouldn't handle the
   * autocomplete. The form manager also checks for this tag when saving
   * form history (so it doesn't save usernames).
   */
  nsCOMPtr<nsINode> node = do_QueryInterface(aInput);
  NS_ENSURE_STATE(node);
  mPwmgrInputs.Put(node, true);
  node->AddMutationObserverUnlessExists(this);

  if (!mLoginManager)
    mLoginManager = do_GetService("@mozilla.org/login-manager;1");

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
nsFormFillController::GetPopupOpen(bool *aPopupOpen)
{
  if (mFocusedPopup)
    mFocusedPopup->GetPopupOpen(aPopupOpen);
  else
    *aPopupOpen = false;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetPopupOpen(bool aPopupOpen)
{
  if (mFocusedPopup) {
    if (aPopupOpen) {
      // make sure input field is visible before showing popup (bug 320938)
      nsCOMPtr<nsIContent> content = do_QueryInterface(mFocusedInput);
      NS_ENSURE_STATE(content);
      nsCOMPtr<nsIDocShell> docShell = GetDocShellForInput(mFocusedInput);
      NS_ENSURE_STATE(docShell);
      nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
      NS_ENSURE_STATE(presShell);
      presShell->ScrollContentIntoView(content,
                                       nsIPresShell::ScrollAxis(
                                         nsIPresShell::SCROLL_MINIMUM,
                                         nsIPresShell::SCROLL_IF_NOT_VISIBLE),
                                       nsIPresShell::ScrollAxis(
                                         nsIPresShell::SCROLL_MINIMUM,
                                         nsIPresShell::SCROLL_IF_NOT_VISIBLE),
                                       nsIPresShell::SCROLL_OVERFLOW_HIDDEN);
      // mFocusedPopup can be destroyed after ScrollContentIntoView, see bug 420089
      if (mFocusedPopup) {
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mFocusedInput);
        mFocusedPopup->OpenAutocompletePopup(this, element);
      }
    } else
      mFocusedPopup->ClosePopup();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetDisableAutoComplete(bool *aDisableAutoComplete)
{
  *aDisableAutoComplete = mDisableAutoComplete;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetDisableAutoComplete(bool aDisableAutoComplete)
{
  mDisableAutoComplete = aDisableAutoComplete;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetCompleteDefaultIndex(bool *aCompleteDefaultIndex)
{
  *aCompleteDefaultIndex = mCompleteDefaultIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetCompleteDefaultIndex(bool aCompleteDefaultIndex)
{
  mCompleteDefaultIndex = aCompleteDefaultIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetCompleteSelectedIndex(bool *aCompleteSelectedIndex)
{
  *aCompleteSelectedIndex = mCompleteSelectedIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetCompleteSelectedIndex(bool aCompleteSelectedIndex)
{
  mCompleteSelectedIndex = aCompleteSelectedIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetForceComplete(bool *aForceComplete)
{
  *aForceComplete = mForceComplete;
  return NS_OK;
}

NS_IMETHODIMP nsFormFillController::SetForceComplete(bool aForceComplete)
{
  mForceComplete = aForceComplete;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetMinResultsForPopup(uint32_t *aMinResultsForPopup)
{
  *aMinResultsForPopup = mMinResultsForPopup;
  return NS_OK;
}

NS_IMETHODIMP nsFormFillController::SetMinResultsForPopup(uint32_t aMinResultsForPopup)
{
  mMinResultsForPopup = aMinResultsForPopup;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetMaxRows(uint32_t *aMaxRows)
{
  *aMaxRows = mMaxRows;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetMaxRows(uint32_t aMaxRows)
{
  mMaxRows = aMaxRows;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetShowImageColumn(bool *aShowImageColumn)
{
  *aShowImageColumn = false;
  return NS_OK;
}

NS_IMETHODIMP nsFormFillController::SetShowImageColumn(bool aShowImageColumn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsFormFillController::GetShowCommentColumn(bool *aShowCommentColumn)
{
  *aShowCommentColumn = false;
  return NS_OK;
}

NS_IMETHODIMP nsFormFillController::SetShowCommentColumn(bool aShowCommentColumn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFormFillController::GetTimeout(uint32_t *aTimeout)
{
  *aTimeout = mTimeout;
  return NS_OK;
}

NS_IMETHODIMP nsFormFillController::SetTimeout(uint32_t aTimeout)
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
  if (aSearchParam.IsEmpty()) {
    nsCOMPtr<nsIDOMHTMLElement> element = do_QueryInterface(mFocusedInput);
    element->GetId(aSearchParam);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetSearchCount(uint32_t *aSearchCount)
{
  *aSearchCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetSearchAt(uint32_t index, nsACString & _retval)
{
  _retval.AssignLiteral("form-history");
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
  nsCOMPtr<nsIDOMNSEditableElement> editable = do_QueryInterface(mFocusedInput);
  if (editable) {
    mSuppressOnInput = true;
    editable->SetUserInput(aTextValue);
    mSuppressOnInput = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetSelectionStart(int32_t *aSelectionStart)
{
  if (mFocusedInput)
    mFocusedInput->GetSelectionStart(aSelectionStart);
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetSelectionEnd(int32_t *aSelectionEnd)
{
  if (mFocusedInput)
    mFocusedInput->GetSelectionEnd(aSelectionEnd);
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SelectTextRange(int32_t aStartIndex, int32_t aEndIndex)
{
 if (mFocusedInput)
    mFocusedInput->SetSelectionRange(aStartIndex, aEndIndex, EmptyString());
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::OnSearchBegin()
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::OnSearchComplete()
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::OnTextEntered(nsIDOMEvent* aEvent,
                                    bool* aPrevent)
{
  NS_ENSURE_ARG(aPrevent);
  NS_ENSURE_TRUE(mFocusedInput, NS_OK);
  // Fire off a DOMAutoComplete event
  nsCOMPtr<nsIDOMDocument> domDoc;
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mFocusedInput);
  element->GetOwnerDocument(getter_AddRefs(domDoc));
  NS_ENSURE_STATE(domDoc);

  nsCOMPtr<nsIDOMEvent> event;
  domDoc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  NS_ENSURE_STATE(event);

  event->InitEvent(NS_LITERAL_STRING("DOMAutoComplete"), true, true);

  // XXXjst: We mark this event as a trusted event, it's up to the
  // callers of this to ensure that it's only called from trusted
  // code.
  event->SetTrusted(true);

  nsCOMPtr<EventTarget> targ = do_QueryInterface(mFocusedInput);

  bool defaultActionEnabled;
  targ->DispatchEvent(event, &defaultActionEnabled);
  *aPrevent = !defaultActionEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::OnTextReverted(bool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetConsumeRollupEvent(bool *aConsumeRollupEvent)
{
  *aConsumeRollupEvent = false;
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetInPrivateContext(bool *aInPrivateContext)
{
  if (!mFocusedInput) {
    *aInPrivateContext = false;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMDocument> inputDoc;
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mFocusedInput);
  element->GetOwnerDocument(getter_AddRefs(inputDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(inputDoc);
  nsCOMPtr<nsIDocShell> docShell = doc->GetDocShell();
  nsCOMPtr<nsILoadContext> loadContext = doc->GetLoadContext();
  *aInPrivateContext = loadContext && loadContext->UsePrivateBrowsing();
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetNoRollupOnCaretMove(bool *aNoRollupOnCaretMove)
{
  *aNoRollupOnCaretMove = false;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIAutoCompleteSearch

NS_IMETHODIMP
nsFormFillController::StartSearch(const nsAString &aSearchString, const nsAString &aSearchParam,
                                  nsIAutoCompleteResult *aPreviousResult, nsIAutoCompleteObserver *aListener)
{
  nsresult rv;

  // If the login manager has indicated it's responsible for this field, let it
  // handle the autocomplete. Otherwise, handle with form history.
  if (mPwmgrInputs.Get(mFocusedInputNode)) {
    // XXX aPreviousResult shouldn't ever be a historyResult type, since we're not letting
    // satchel manage the field?
    mLastListener = aListener;
    rv = mLoginManager->AutoCompleteSearchAsync(aSearchString,
                                                aPreviousResult,
                                                mFocusedInput,
                                                this);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    mLastListener = aListener;

    nsCOMPtr<nsIAutoCompleteResult> datalistResult;
    if (mFocusedInput) {
      rv = PerformInputListAutoComplete(aSearchString,
                                        getter_AddRefs(datalistResult));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr <nsIFormAutoComplete> formAutoComplete =
      do_GetService("@mozilla.org/satchel/form-autocomplete;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    formAutoComplete->AutoCompleteSearchAsync(aSearchParam,
                                              aSearchString,
                                              mFocusedInput,
                                              aPreviousResult,
                                              datalistResult,
                                              this);
    mLastFormAutoComplete = formAutoComplete;
  }

  return NS_OK;
}

nsresult
nsFormFillController::PerformInputListAutoComplete(const nsAString& aSearch,
                                                   nsIAutoCompleteResult** aResult)
{
  // If an <input> is focused, check if it has a list="<datalist>" which can
  // provide the list of suggestions.

  MOZ_ASSERT(!mPwmgrInputs.Get(mFocusedInputNode));
  nsresult rv;

  nsCOMPtr <nsIInputListAutoComplete> inputListAutoComplete =
    do_GetService("@mozilla.org/satchel/inputlist-autocomplete;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = inputListAutoComplete->AutoCompleteSearch(aSearch,
                                                 mFocusedInput,
                                                 aResult);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mFocusedInput) {
    nsCOMPtr<nsIDOMHTMLElement> list;
    mFocusedInput->GetList(getter_AddRefs(list));

    // Add a mutation observer to check for changes to the items in the <datalist>
    // and update the suggestions accordingly.
    nsCOMPtr<nsINode> node = do_QueryInterface(list);
    if (mListNode != node) {
      if (mListNode) {
        mListNode->RemoveMutationObserver(this);
        mListNode = nullptr;
      }
      if (node) {
        node->AddMutationObserverUnlessExists(this);
        mListNode = node;
      }
    }
  }

  return NS_OK;
}

class UpdateSearchResultRunnable : public mozilla::Runnable
{
public:
  UpdateSearchResultRunnable(nsIAutoCompleteObserver* aObserver,
                             nsIAutoCompleteSearch* aSearch,
                             nsIAutoCompleteResult* aResult)
    : mObserver(aObserver)
    , mSearch(aSearch)
    , mResult(aResult)
  {
    MOZ_ASSERT(mResult, "Should have a valid result");
    MOZ_ASSERT(mObserver, "You shouldn't call this runnable with a null observer!");
  }

  NS_IMETHOD Run() {
    mObserver->OnUpdateSearchResult(mSearch, mResult);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIAutoCompleteObserver> mObserver;
  nsCOMPtr<nsIAutoCompleteSearch> mSearch;
  nsCOMPtr<nsIAutoCompleteResult> mResult;
};

void nsFormFillController::RevalidateDataList()
{
  if (!mLastListener) {
    return;
  }

  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsIAutoCompleteController> controller(do_QueryInterface(mLastListener));
    if (!controller) {
      return;
    }

    controller->StartSearch(mLastSearchString);
    return;
  }

  nsresult rv;
  nsCOMPtr <nsIInputListAutoComplete> inputListAutoComplete =
    do_GetService("@mozilla.org/satchel/inputlist-autocomplete;1", &rv);

  nsCOMPtr<nsIAutoCompleteResult> result;

  rv = inputListAutoComplete->AutoCompleteSearch(mLastSearchString,
                                                 mFocusedInput,
                                                 getter_AddRefs(result));

  nsCOMPtr<nsIRunnable> event =
    new UpdateSearchResultRunnable(mLastListener, this, result);
  NS_DispatchToCurrentThread(event);
}

NS_IMETHODIMP
nsFormFillController::StopSearch()
{
  // Make sure to stop and clear this, otherwise the controller will prevent
  // mLastFormAutoComplete from being deleted.
  if (mLastFormAutoComplete) {
    mLastFormAutoComplete->StopAutoCompleteSearch();
    mLastFormAutoComplete = nullptr;
  } else if (mLoginManager) {
    mLoginManager->StopSearch();
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIFormAutoCompleteObserver

NS_IMETHODIMP
nsFormFillController::OnSearchCompletion(nsIAutoCompleteResult *aResult)
{
  nsAutoString searchString;
  aResult->GetSearchString(searchString);

  mLastSearchString = searchString;

  if (mLastListener) {
    mLastListener->OnSearchResult(this, aResult);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIDOMEventListener

NS_IMETHODIMP
nsFormFillController::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);

  if (type.EqualsLiteral("focus")) {
    return Focus(aEvent);
  }
  if (type.EqualsLiteral("mousedown")) {
    return MouseDown(aEvent);
  }
  if (type.EqualsLiteral("keypress")) {
    return KeyPress(aEvent);
  }
  if (type.EqualsLiteral("input")) {
    return (!mSuppressOnInput && mController && mFocusedInput) ?
           mController->HandleText() : NS_OK;
  }
  if (type.EqualsLiteral("blur")) {
    if (mFocusedInput)
      StopControllingInput();
    return NS_OK;
  }
  if (type.EqualsLiteral("compositionstart")) {
    NS_ASSERTION(mController, "should have a controller!");
    if (mController && mFocusedInput)
      mController->HandleStartComposition();
    return NS_OK;
  }
  if (type.EqualsLiteral("compositionend")) {
    NS_ASSERTION(mController, "should have a controller!");
    if (mController && mFocusedInput)
      mController->HandleEndComposition();
    return NS_OK;
  }
  if (type.EqualsLiteral("contextmenu")) {
    if (mFocusedPopup)
      mFocusedPopup->ClosePopup();
    return NS_OK;
  }
  if (type.EqualsLiteral("pagehide")) {

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(
      aEvent->InternalDOMEvent()->GetTarget());
    if (!doc)
      return NS_OK;

    if (mFocusedInput) {
      if (doc == mFocusedInputNode->OwnerDoc())
        StopControllingInput();
    }

    RemoveForDocument(doc);
  }

  return NS_OK;
}

void
nsFormFillController::RemoveForDocument(nsIDocument* aDoc)
{
  for (auto iter = mPwmgrInputs.Iter(); !iter.Done(); iter.Next()) {
    const nsINode* key = iter.Key();
    if (key && (!aDoc || key->OwnerDoc() == aDoc)) {
      // mFocusedInputNode's observer is tracked separately, so don't remove it
      // here.
      if (key != mFocusedInputNode) {
        const_cast<nsINode*>(key)->RemoveMutationObserver(this);
      }
      iter.Remove();
    }
  }
}

void
nsFormFillController::MaybeStartControllingInput(nsIDOMHTMLInputElement* aInput)
{
  nsCOMPtr<nsINode> inputNode = do_QueryInterface(aInput);
  if (!inputNode)
    return;

  nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(aInput);
  if (!formControl || !formControl->IsSingleLineTextControl(true))
    return;

  bool isReadOnly = false;
  aInput->GetReadOnly(&isReadOnly);
  if (isReadOnly)
    return;

  bool autocomplete = nsContentUtils::IsAutocompleteEnabled(aInput);

  nsCOMPtr<nsIDOMHTMLElement> datalist;
  aInput->GetList(getter_AddRefs(datalist));
  bool hasList = datalist != nullptr;

  bool isPwmgrInput = false;
  if (mPwmgrInputs.Get(inputNode))
      isPwmgrInput = true;

  if (isPwmgrInput || hasList || autocomplete) {
    StartControllingInput(aInput);
  }
}

nsresult
nsFormFillController::Focus(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMHTMLInputElement> input = do_QueryInterface(
    aEvent->InternalDOMEvent()->GetTarget());
  MaybeStartControllingInput(input);
  return NS_OK;
}

nsresult
nsFormFillController::KeyPress(nsIDOMEvent* aEvent)
{
  NS_ASSERTION(mController, "should have a controller!");
  if (!mFocusedInput || !mController)
    return NS_OK;

  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aEvent);
  if (!keyEvent)
    return NS_ERROR_FAILURE;

  bool cancel = false;

  uint32_t k;
  keyEvent->GetKeyCode(&k);
  switch (k) {
  case nsIDOMKeyEvent::DOM_VK_DELETE:
#ifndef XP_MACOSX
    mController->HandleDelete(&cancel);
    break;
  case nsIDOMKeyEvent::DOM_VK_BACK_SPACE:
    mController->HandleText();
    break;
#else
  case nsIDOMKeyEvent::DOM_VK_BACK_SPACE:
    {
      bool isShift = false;
      keyEvent->GetShiftKey(&isShift);

      if (isShift)
        mController->HandleDelete(&cancel);
      else
        mController->HandleText();

      break;
    }
#endif
  case nsIDOMKeyEvent::DOM_VK_PAGE_UP:
  case nsIDOMKeyEvent::DOM_VK_PAGE_DOWN:
    {
      bool isCtrl, isAlt, isMeta;
      keyEvent->GetCtrlKey(&isCtrl);
      keyEvent->GetAltKey(&isAlt);
      keyEvent->GetMetaKey(&isMeta);
      if (isCtrl || isAlt || isMeta)
        break;
    }
    MOZ_FALLTHROUGH;
  case nsIDOMKeyEvent::DOM_VK_UP:
  case nsIDOMKeyEvent::DOM_VK_DOWN:
  case nsIDOMKeyEvent::DOM_VK_LEFT:
  case nsIDOMKeyEvent::DOM_VK_RIGHT:
    {
      // Get the writing-mode of the relevant input element,
      // so that we can remap arrow keys if necessary.
      mozilla::WritingMode wm;
      if (mFocusedInputNode && mFocusedInputNode->IsElement()) {
        mozilla::dom::Element *elem = mFocusedInputNode->AsElement();
        nsIFrame *frame = elem->GetPrimaryFrame();
        if (frame) {
          wm = frame->GetWritingMode();
        }
      }
      if (wm.IsVertical()) {
        switch (k) {
        case nsIDOMKeyEvent::DOM_VK_LEFT:
          k = wm.IsVerticalLR() ? nsIDOMKeyEvent::DOM_VK_UP
                                : nsIDOMKeyEvent::DOM_VK_DOWN;
          break;
        case nsIDOMKeyEvent::DOM_VK_RIGHT:
          k = wm.IsVerticalLR() ? nsIDOMKeyEvent::DOM_VK_DOWN
                                : nsIDOMKeyEvent::DOM_VK_UP;
          break;
        case nsIDOMKeyEvent::DOM_VK_UP:
          k = nsIDOMKeyEvent::DOM_VK_LEFT;
          break;
        case nsIDOMKeyEvent::DOM_VK_DOWN:
          k = nsIDOMKeyEvent::DOM_VK_RIGHT;
          break;
        }
      }
    }
    mController->HandleKeyNavigation(k, &cancel);
    break;
  case nsIDOMKeyEvent::DOM_VK_ESCAPE:
    mController->HandleEscape(&cancel);
    break;
  case nsIDOMKeyEvent::DOM_VK_TAB:
    mController->HandleTab();
    cancel = false;
    break;
  case nsIDOMKeyEvent::DOM_VK_RETURN:
    mController->HandleEnter(false, aEvent, &cancel);
    break;
  }

  if (cancel) {
    aEvent->PreventDefault();
    // Don't let the page see the RETURN event when the popup is open
    // (indicated by cancel=true) so sites don't manually submit forms
    // (e.g. via submit.click()) without the autocompleted value being filled.
    // Bug 286933 will fix this for other key events.
    if (k == nsIDOMKeyEvent::DOM_VK_RETURN) {
      aEvent->StopPropagation();
    }
  }

  return NS_OK;
}

nsresult
nsFormFillController::MouseDown(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aEvent));
  if (!mouseEvent)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMHTMLInputElement> targetInput = do_QueryInterface(
    aEvent->InternalDOMEvent()->GetTarget());
  if (!targetInput)
    return NS_OK;

  int16_t button;
  mouseEvent->GetButton(&button);
  if (button != 0)
    return NS_OK;

  bool isOpen = false;
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
    mController->HandleText();
  } else {
    // Show the popup with the complete result set.  Can't use HandleText()
    // because it doesn't display the popup if the input is blank.
    bool cancel = false;
    mController->HandleKeyNavigation(nsIDOMKeyEvent::DOM_VK_DOWN, &cancel);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsFormFillController

void
nsFormFillController::AddWindowListeners(nsPIDOMWindowOuter* aWindow)
{
  if (!aWindow)
    return;

  EventTarget* target = aWindow->GetChromeEventHandler();

  if (!target)
    return;

  target->AddEventListener(NS_LITERAL_STRING("focus"), this,
                           true, false);
  target->AddEventListener(NS_LITERAL_STRING("blur"), this,
                           true, false);
  target->AddEventListener(NS_LITERAL_STRING("pagehide"), this,
                           true, false);
  target->AddEventListener(NS_LITERAL_STRING("mousedown"), this,
                           true, false);
  target->AddEventListener(NS_LITERAL_STRING("input"), this,
                           true, false);
  target->AddEventListener(NS_LITERAL_STRING("keypress"), this, true, false);
  target->AddEventListener(NS_LITERAL_STRING("compositionstart"), this,
                           true, false);
  target->AddEventListener(NS_LITERAL_STRING("compositionend"), this,
                           true, false);
  target->AddEventListener(NS_LITERAL_STRING("contextmenu"), this,
                           true, false);

  // Note that any additional listeners added should ensure that they ignore
  // untrusted events, which might be sent by content that's up to no good.
}

void
nsFormFillController::RemoveWindowListeners(nsPIDOMWindowOuter* aWindow)
{
  if (!aWindow)
    return;

  StopControllingInput();

  nsCOMPtr<nsIDocument> doc = aWindow->GetDoc();
  RemoveForDocument(doc);

  EventTarget* target = aWindow->GetChromeEventHandler();

  if (!target)
    return;

  target->RemoveEventListener(NS_LITERAL_STRING("focus"), this, true);
  target->RemoveEventListener(NS_LITERAL_STRING("blur"), this, true);
  target->RemoveEventListener(NS_LITERAL_STRING("pagehide"), this, true);
  target->RemoveEventListener(NS_LITERAL_STRING("mousedown"), this, true);
  target->RemoveEventListener(NS_LITERAL_STRING("input"), this, true);
  target->RemoveEventListener(NS_LITERAL_STRING("keypress"), this, true);
  target->RemoveEventListener(NS_LITERAL_STRING("compositionstart"), this,
                              true);
  target->RemoveEventListener(NS_LITERAL_STRING("compositionend"), this,
                              true);
  target->RemoveEventListener(NS_LITERAL_STRING("contextmenu"), this, true);
}

void
nsFormFillController::StartControllingInput(nsIDOMHTMLInputElement *aInput)
{
  // Make sure we're not still attached to an input
  StopControllingInput();

  if (!mController) {
    return;
  }

  // Find the currently focused docShell
  nsCOMPtr<nsIDocShell> docShell = GetDocShellForInput(aInput);
  int32_t index = GetIndexOfDocShell(docShell);
  if (index < 0)
    return;

  // Cache the popup for the focused docShell
  mFocusedPopup = mPopups.SafeElementAt(index);

  nsCOMPtr<nsINode> node = do_QueryInterface(aInput);
  if (!node) {
    return;
  }

  node->AddMutationObserverUnlessExists(this);
  mFocusedInputNode = node;
  mFocusedInput = aInput;

  nsCOMPtr<nsIDOMHTMLElement> list;
  mFocusedInput->GetList(getter_AddRefs(list));
  nsCOMPtr<nsINode> listNode = do_QueryInterface(list);
  if (listNode) {
    listNode->AddMutationObserverUnlessExists(this);
    mListNode = listNode;
  }

  mController->SetInput(this);
}

void
nsFormFillController::StopControllingInput()
{
  if (mListNode) {
    mListNode->RemoveMutationObserver(this);
    mListNode = nullptr;
  }

  if (mController) {
    // Reset the controller's input, but not if it has been switched
    // to another input already, which might happen if the user switches
    // focus by clicking another autocomplete textbox
    nsCOMPtr<nsIAutoCompleteInput> input;
    mController->GetInput(getter_AddRefs(input));
    if (input == this)
      mController->SetInput(nullptr);
  }

  if (mFocusedInputNode) {
    MaybeRemoveMutationObserver(mFocusedInputNode);

    nsresult rv;
    nsCOMPtr <nsIFormAutoComplete> formAutoComplete =
      do_GetService("@mozilla.org/satchel/form-autocomplete;1", &rv);
    if (formAutoComplete) {
      formAutoComplete->StopControllingInput(mFocusedInput);
    }

    mFocusedInputNode = nullptr;
    mFocusedInput = nullptr;
  }
  mFocusedPopup = nullptr;
}

nsIDocShell *
nsFormFillController::GetDocShellForInput(nsIDOMHTMLInputElement *aInput)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aInput);
  NS_ENSURE_TRUE(node, nullptr);

  nsCOMPtr<nsPIDOMWindowOuter> win = node->OwnerDoc()->GetWindow();
  NS_ENSURE_TRUE(win, nullptr);

  return win->GetDocShell();
}

nsPIDOMWindowOuter*
nsFormFillController::GetWindowForDocShell(nsIDocShell *aDocShell)
{
  nsCOMPtr<nsIContentViewer> contentViewer;
  aDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  NS_ENSURE_TRUE(contentViewer, nullptr);

  nsCOMPtr<nsIDOMDocument> domDoc;
  contentViewer->GetDOMDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  NS_ENSURE_TRUE(doc, nullptr);

  return doc->GetWindow();
}

int32_t
nsFormFillController::GetIndexOfDocShell(nsIDocShell *aDocShell)
{
  if (!aDocShell)
    return -1;

  // Loop through our cached docShells looking for the given docShell
  uint32_t count = mDocShells.Length();
  for (uint32_t i = 0; i < count; ++i) {
    if (mDocShells[i] == aDocShell)
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

NS_GENERIC_FACTORY_CONSTRUCTOR(nsFormFillController)

NS_DEFINE_NAMED_CID(NS_FORMFILLCONTROLLER_CID);

static const mozilla::Module::CIDEntry kSatchelCIDs[] = {
  { &kNS_FORMFILLCONTROLLER_CID, false, nullptr, nsFormFillControllerConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kSatchelContracts[] = {
  { "@mozilla.org/satchel/form-fill-controller;1", &kNS_FORMFILLCONTROLLER_CID },
  { NS_FORMHISTORYAUTOCOMPLETE_CONTRACTID, &kNS_FORMFILLCONTROLLER_CID },
  { nullptr }
};

static const mozilla::Module kSatchelModule = {
  mozilla::Module::kVersion,
  kSatchelCIDs,
  kSatchelContracts
};

NSMODULE_DEFN(satchel) = &kSatchelModule;
