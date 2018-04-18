/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFormFillController.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h" // for Event
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/PageTransitionEvent.h"
#include "mozilla/Logging.h"
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
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsRect.h"
#include "nsILoginManager.h"
#include "mozilla/ModuleUtils.h"
#include "nsToolkitCompsCID.h"
#include "nsEmbedCID.h"
#include "nsIDOMNSEditableElement.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"
#include "nsILoadContext.h"
#include "nsIFrame.h"
#include "nsIScriptSecurityManager.h"
#include "nsFocusManager.h"

using namespace mozilla;
using namespace mozilla::dom;
using mozilla::ErrorResult;
using mozilla::LogLevel;

static mozilla::LazyLogModule sLogger("satchel");

static nsIFormAutoComplete*
GetFormAutoComplete()
{
  static nsCOMPtr<nsIFormAutoComplete> sInstance;
  static bool sInitialized = false;
  if (!sInitialized) {
    nsresult rv;
    sInstance =
      do_GetService("@mozilla.org/satchel/form-autocomplete;1",
                    &rv);

    if (NS_SUCCEEDED(rv)) {
      ClearOnShutdown(&sInstance);
      sInitialized = true;
    }
  }
  return sInstance;
}

NS_IMPL_CYCLE_COLLECTION(nsFormFillController,
                         mController, mLoginManager, mLoginReputationService,
                         mFocusedPopup, mDocShells, mPopups, mLastListener,
                         mLastFormAutoComplete)

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
  mListNode(nullptr),
  // The amount of time a context menu event supresses showing a
  // popup from a focus event in ms. This matches the threshold in
  // toolkit/components/passwordmgr/LoginManagerContent.jsm.
  mFocusAfterRightClickThreshold(400),
  mTimeout(50),
  mMinResultsForPopup(1),
  mMaxRows(0),
  mLastRightClickTimeStamp(TimeStamp()),
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
  if (mFocusedInput) {
    MaybeRemoveMutationObserver(mFocusedInput);
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
nsFormFillController::AttributeChanged(mozilla::dom::Element* aElement,
                                       int32_t aNameSpaceID,
                                       nsAtom* aAttribute, int32_t aModType,
                                       const nsAttrValue* aOldValue)
{
  if ((aAttribute == nsGkAtoms::type || aAttribute == nsGkAtoms::readonly ||
       aAttribute == nsGkAtoms::autocomplete) &&
      aNameSpaceID == kNameSpaceID_None) {
    RefPtr<HTMLInputElement> focusedInput(mFocusedInput);
    // Reset the current state of the controller, unconditionally.
    StopControllingInput();
    // Then restart based on the new values.  We have to delay this
    // to avoid ending up in an endless loop due to re-registering our
    // mutation observer (which would notify us again for *this* event).
    nsCOMPtr<nsIRunnable> event =
      mozilla::NewRunnableMethod<RefPtr<HTMLInputElement>>(
        "nsFormFillController::MaybeStartControllingInput",
        this,
        &nsFormFillController::MaybeStartControllingInput,
        focusedInput);
    aElement->OwnerDoc()->Dispatch(TaskCategory::Other, event.forget());
  }

  if (mListNode && mListNode->Contains(aElement)) {
    RevalidateDataList();
  }
}

void
nsFormFillController::ContentAppended(nsIContent* aChild)
{
  if (mListNode && mListNode->Contains(aChild->GetParent())) {
    RevalidateDataList();
  }
}

void
nsFormFillController::ContentInserted(nsIContent* aChild)
{
  if (mListNode && mListNode->Contains(aChild->GetParent())) {
    RevalidateDataList();
  }
}

void
nsFormFillController::ContentRemoved(nsIContent* aChild,
                                     nsIContent* aPreviousSibling)
{
  if (mListNode && mListNode->Contains(aChild->GetParent())) {
    RevalidateDataList();
  }
}

void
nsFormFillController::CharacterDataWillChange(nsIContent* aContent,
                                              const CharacterDataChangeInfo&)
{
}

void
nsFormFillController::CharacterDataChanged(nsIContent* aContent,
                                           const CharacterDataChangeInfo&)
{
}

void
nsFormFillController::AttributeWillChange(mozilla::dom::Element* aElement,
                                          int32_t aNameSpaceID,
                                          nsAtom* aAttribute, int32_t aModType,
                                          const nsAttrValue* aNewValue)
{
}

void
nsFormFillController::NativeAnonymousChildListChange(nsIContent* aContent,
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
  MOZ_LOG(sLogger, LogLevel::Verbose, ("NodeWillBeDestroyed: %p", aNode));
  mPwmgrInputs.Remove(aNode);
  mAutofillInputs.Remove(aNode);
  if (aNode == mListNode) {
    mListNode = nullptr;
    RevalidateDataList();
  } else if (aNode == mFocusedInput) {
    mFocusedInput = nullptr;
  }
}

void
nsFormFillController::MaybeRemoveMutationObserver(nsINode* aNode)
{
  // Nodes being tracked in mPwmgrInputs will have their observers removed when
  // they stop being tracked.
  if (!mPwmgrInputs.Get(aNode) && !mAutofillInputs.Get(aNode)) {
    aNode->RemoveMutationObserver(this);
  }
}

////////////////////////////////////////////////////////////////////////
//// nsIFormFillController

NS_IMETHODIMP
nsFormFillController::AttachToBrowser(nsIDocShell *aDocShell, nsIAutoCompletePopup *aPopup)
{
  MOZ_LOG(sLogger, LogLevel::Debug,
          ("AttachToBrowser for docShell %p with popup %p", aDocShell, aPopup));
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
nsFormFillController::MarkAsLoginManagerField(HTMLInputElement *aInput)
{
  /*
   * The Login Manager can supply autocomplete results for username fields,
   * when a user has multiple logins stored for a site. It uses this
   * interface to indicate that the form manager shouldn't handle the
   * autocomplete. The form manager also checks for this tag when saving
   * form history (so it doesn't save usernames).
   */
  NS_ENSURE_STATE(aInput);

  // If the field was already marked, we don't want to show the popup again.
  if (mPwmgrInputs.Get(aInput)) {
    return NS_OK;
  }

  mPwmgrInputs.Put(aInput, true);
  aInput->AddMutationObserverUnlessExists(this);

  nsFocusManager *fm = nsFocusManager::GetFocusManager();
  if (fm) {
    nsCOMPtr<nsIContent> focusedContent = fm->GetFocusedElement();
    if (focusedContent == aInput) {
      if (!mFocusedInput) {
        MaybeStartControllingInput(aInput);
      }
    }
  }

  if (!mLoginManager) {
    mLoginManager = do_GetService("@mozilla.org/login-manager;1");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::MarkAsAutofillField(HTMLInputElement *aInput)
{
  /*
   * Support other components implementing form autofill and handle autocomplete
   * for the field.
   */
  NS_ENSURE_STATE(aInput);

  MOZ_LOG(sLogger, LogLevel::Verbose,
          ("MarkAsAutofillField: aInput = %p", aInput));

  if (mAutofillInputs.Get(aInput)) {
    return NS_OK;
  }

  mAutofillInputs.Put(aInput, true);
  aInput->AddMutationObserverUnlessExists(this);

  aInput->EnablePreview();

  nsFocusManager *fm = nsFocusManager::GetFocusManager();
  if (fm) {
    nsCOMPtr<nsIContent> focusedContent = fm->GetFocusedElement();
    if (focusedContent == aInput) {
      MaybeStartControllingInput(aInput);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetFocusedInput(HTMLInputElement **aInput)
{
  *aInput = mFocusedInput;
  NS_IF_ADDREF(*aInput);
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
  if (mFocusedPopup) {
    mFocusedPopup->GetPopupOpen(aPopupOpen);
  } else {
    *aPopupOpen = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetPopupOpen(bool aPopupOpen)
{
  if (mFocusedPopup) {
    if (aPopupOpen) {
      // make sure input field is visible before showing popup (bug 320938)
      nsCOMPtr<nsIContent> content = mFocusedInput;
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
        mFocusedPopup->OpenAutocompletePopup(this, mFocusedInput);
      }
    } else {
      mFocusedPopup->ClosePopup();
    }
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
    mFocusedInput->GetId(aSearchParam);
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
  if (mAutofillInputs.Get(mFocusedInput)) {
    MOZ_LOG(sLogger, LogLevel::Debug, ("GetSearchAt: autofill-profiles field"));
    nsCOMPtr<nsIAutoCompleteSearch> profileSearch = do_GetService("@mozilla.org/autocomplete/search;1?name=autofill-profiles");
    if (profileSearch) {
      _retval.AssignLiteral("autofill-profiles");
      return NS_OK;
    }
  }

  MOZ_LOG(sLogger, LogLevel::Debug, ("GetSearchAt: form-history field"));
  _retval.AssignLiteral("form-history");
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::GetTextValue(nsAString & aTextValue)
{
  if (mFocusedInput) {
    mFocusedInput->GetValue(aTextValue, CallerType::System);
  } else {
    aTextValue.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetTextValue(const nsAString & aTextValue)
{
  if (mFocusedInput) {
    mSuppressOnInput = true;
    mFocusedInput->SetUserInput(aTextValue);
    mSuppressOnInput = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFormFillController::SetTextValueWithReason(const nsAString & aTextValue,
                                             uint16_t aReason)
{
  return SetTextValue(aTextValue);
}

NS_IMETHODIMP
nsFormFillController::GetSelectionStart(int32_t *aSelectionStart)
{
  if (!mFocusedInput) {
    return NS_ERROR_UNEXPECTED;
  }
  ErrorResult rv;
  *aSelectionStart = mFocusedInput->GetSelectionStartIgnoringType(rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsFormFillController::GetSelectionEnd(int32_t *aSelectionEnd)
{
  if (!mFocusedInput) {
    return NS_ERROR_UNEXPECTED;
  }
  ErrorResult rv;
  *aSelectionEnd = mFocusedInput->GetSelectionEndIgnoringType(rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsFormFillController::SelectTextRange(int32_t aStartIndex, int32_t aEndIndex)
{
  if (!mFocusedInput) {
    return NS_ERROR_UNEXPECTED;
  }
  ErrorResult rv;
  mFocusedInput->SetSelectionRange(aStartIndex, aEndIndex,
                                   Optional<nsAString>(), rv);
  return rv.StealNSResult();
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
nsFormFillController::OnTextEntered(Event* aEvent,
                                    bool* aPrevent)
{
  NS_ENSURE_ARG(aPrevent);
  NS_ENSURE_TRUE(mFocusedInput, NS_OK);
  // Fire off a DOMAutoComplete event

  IgnoredErrorResult ignored;
  RefPtr<Event> event = mFocusedInput->OwnerDoc()->
    CreateEvent(NS_LITERAL_STRING("Events"), CallerType::System, ignored);
  NS_ENSURE_STATE(event);

  event->InitEvent(NS_LITERAL_STRING("DOMAutoComplete"), true, true);

  // XXXjst: We mark this event as a trusted event, it's up to the
  // callers of this to ensure that it's only called from trusted
  // code.
  event->SetTrusted(true);

  bool defaultActionEnabled =
    mFocusedInput->DispatchEvent(*event, CallerType::System, IgnoreErrors());
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

  nsCOMPtr<nsIDocument> doc = mFocusedInput->OwnerDoc();
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

NS_IMETHODIMP
nsFormFillController::GetUserContextId(uint32_t* aUserContextId)
{
  *aUserContextId = nsIScriptSecurityManager::DEFAULT_USER_CONTEXT_ID;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIAutoCompleteSearch

NS_IMETHODIMP
nsFormFillController::StartSearch(const nsAString &aSearchString, const nsAString &aSearchParam,
                                  nsIAutoCompleteResult *aPreviousResult, nsIAutoCompleteObserver *aListener)
{
  MOZ_LOG(sLogger, LogLevel::Debug, ("StartSearch for %p", mFocusedInput));

  nsresult rv;

  // If the login manager has indicated it's responsible for this field, let it
  // handle the autocomplete. Otherwise, handle with form history.
  // This method is sometimes called in unit tests and from XUL without a focused node.
  if (mFocusedInput &&
      (mPwmgrInputs.Get(mFocusedInput) ||
       mFocusedInput->ControlType() == NS_FORM_INPUT_PASSWORD)) {
    MOZ_LOG(sLogger, LogLevel::Debug, ("StartSearch: login field"));

    // Handle the case where a password field is focused but
    // MarkAsLoginManagerField wasn't called because password manager is disabled.
    if (!mLoginManager) {
      mLoginManager = do_GetService("@mozilla.org/login-manager;1");
    }

    if (NS_WARN_IF(!mLoginManager)) {
      return NS_ERROR_FAILURE;
    }

    // XXX aPreviousResult shouldn't ever be a historyResult type, since we're not letting
    // satchel manage the field?
    mLastListener = aListener;
    rv = mLoginManager->AutoCompleteSearchAsync(aSearchString,
                                                aPreviousResult,
                                                mFocusedInput,
                                                this);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    MOZ_LOG(sLogger, LogLevel::Debug, ("StartSearch: non-login field"));
    mLastListener = aListener;

    nsCOMPtr<nsIAutoCompleteResult> datalistResult;
    if (mFocusedInput) {
      rv = PerformInputListAutoComplete(aSearchString,
                                        getter_AddRefs(datalistResult));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    auto formAutoComplete = GetFormAutoComplete();
    NS_ENSURE_TRUE(formAutoComplete, NS_ERROR_FAILURE);

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

  MOZ_ASSERT(!mPwmgrInputs.Get(mFocusedInput));
  nsresult rv;

  nsCOMPtr <nsIInputListAutoComplete> inputListAutoComplete =
    do_GetService("@mozilla.org/satchel/inputlist-autocomplete;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = inputListAutoComplete->AutoCompleteSearch(aSearch,
                                                 mFocusedInput,
                                                 aResult);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mFocusedInput) {
    Element* list = mFocusedInput->GetList();

    // Add a mutation observer to check for changes to the items in the <datalist>
    // and update the suggestions accordingly.
    if (mListNode != list) {
      if (mListNode) {
        mListNode->RemoveMutationObserver(this);
        mListNode = nullptr;
      }
      if (list) {
        list->AddMutationObserverUnlessExists(this);
        mListNode = list;
      }
    }
  }

  return NS_OK;
}

void nsFormFillController::RevalidateDataList()
{
  if (!mLastListener) {
    return;
  }

  nsCOMPtr<nsIAutoCompleteController> controller(do_QueryInterface(mLastListener));
  if (!controller) {
    return;
  }

  controller->StartSearch(mLastSearchString);
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

nsresult
nsFormFillController::StartQueryLoginReputation(HTMLInputElement *aInput)
{
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
nsFormFillController::HandleEvent(Event* aEvent)
{
  WidgetEvent* internalEvent = aEvent->WidgetEventPtr();
  NS_ENSURE_STATE(internalEvent);

  switch (internalEvent->mMessage) {
  case eFocus:
    return Focus(aEvent);
  case eMouseDown:
    return MouseDown(aEvent);
  case eKeyPress:
    return KeyPress(aEvent);
  case eEditorInput:
    {
      nsCOMPtr<nsINode> input = do_QueryInterface(aEvent->GetTarget());
      if (!IsTextControl(input)) {
        return NS_OK;
      }

      bool unused = false;
      return (!mSuppressOnInput && mController && mFocusedInput) ?
             mController->HandleText(&unused) : NS_OK;
    }
  case eBlur:
    if (mFocusedInput) {
      StopControllingInput();
    }
    return NS_OK;
  case eCompositionStart:
    NS_ASSERTION(mController, "should have a controller!");
    if (mController && mFocusedInput) {
      mController->HandleStartComposition();
    }
    return NS_OK;
  case eCompositionEnd:
    NS_ASSERTION(mController, "should have a controller!");
    if (mController && mFocusedInput) {
      mController->HandleEndComposition();
    }
    return NS_OK;
  case eContextMenu:
    if (mFocusedPopup) {
      mFocusedPopup->ClosePopup();
    }
    return NS_OK;
  case ePageHide:
    {
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(aEvent->GetTarget());
      if (!doc) {
        return NS_OK;
      }

      if (mFocusedInput) {
        if (doc == mFocusedInput->OwnerDoc()) {
          StopControllingInput();
        }
      }

      // Only remove the observer notifications and marked autofill and password
      // manager fields if the page isn't going to be persisted (i.e. it's being
      // unloaded) so that appropriate autocomplete handling works with bfcache.
      bool persisted = aEvent->AsPageTransitionEvent()->Persisted();
      if (!persisted) {
        RemoveForDocument(doc);
      }
    }
    break;
  default:
    // Handling the default case to shut up stupid -Wswitch warnings.
    // One day compilers will be smarter...
    break;
  }

  return NS_OK;
}

void
nsFormFillController::RemoveForDocument(nsIDocument* aDoc)
{
  MOZ_LOG(sLogger, LogLevel::Verbose, ("RemoveForDocument: %p", aDoc));
  for (auto iter = mPwmgrInputs.Iter(); !iter.Done(); iter.Next()) {
    const nsINode* key = iter.Key();
    if (key && (!aDoc || key->OwnerDoc() == aDoc)) {
      // mFocusedInput's observer is tracked separately, so don't remove it
      // here.
      if (key != mFocusedInput) {
        const_cast<nsINode*>(key)->RemoveMutationObserver(this);
      }
      iter.Remove();
    }
  }

  for (auto iter = mAutofillInputs.Iter(); !iter.Done(); iter.Next()) {
    const nsINode* key = iter.Key();
    if (key && (!aDoc || key->OwnerDoc() == aDoc)) {
      // mFocusedInput's observer is tracked separately, so don't remove it
      // here.
      if (key != mFocusedInput) {
        const_cast<nsINode*>(key)->RemoveMutationObserver(this);
      }
      iter.Remove();
    }
  }
}

bool
nsFormFillController::IsTextControl(nsINode* aNode)
{
  nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(aNode);
  return formControl &&
         formControl->IsSingleLineTextControl(false);
}

void
nsFormFillController::MaybeStartControllingInput(HTMLInputElement* aInput)
{
  MOZ_LOG(sLogger, LogLevel::Verbose, ("MaybeStartControllingInput for %p", aInput));
  if (!aInput) {
    return;
  }

  if (!IsTextControl(aInput)) {
    return;
  }

  if (aInput->ReadOnly()) {
    return;
  }

  bool autocomplete = nsContentUtils::IsAutocompleteEnabled(aInput);

  bool hasList = aInput->GetList() != nullptr;

  bool isPwmgrInput = false;
  if (mPwmgrInputs.Get(aInput) ||
      aInput->ControlType() == NS_FORM_INPUT_PASSWORD) {
    isPwmgrInput = true;
  }

  bool isAutofillInput = false;
  if (mAutofillInputs.Get(aInput)) {
    isAutofillInput = true;
  }

  if (isAutofillInput || isPwmgrInput ||  hasList || autocomplete) {
    StartControllingInput(aInput);
  }

#ifdef NIGHTLY_BUILD
  // Trigger an asynchronous login reputation query when user focuses on the
  // password field.
  if (aInput->ControlType() == NS_FORM_INPUT_PASSWORD) {
    StartQueryLoginReputation(aInput);
  }
#endif

}

nsresult
nsFormFillController::Focus(Event* aEvent)
{
  nsCOMPtr<nsIContent> input = do_QueryInterface(aEvent->GetTarget());
  MaybeStartControllingInput(HTMLInputElement::FromNodeOrNull(input));

  // Bail if we didn't start controlling the input.
  if (!mFocusedInput) {
    return NS_OK;
  }

#ifndef ANDROID
  // If this focus doesn't follow a right click within our specified
  // threshold then show the autocomplete popup for all password fields.
  // This is done to avoid showing both the context menu and the popup
  // at the same time.
  // We use a timestamp instead of a bool to avoid complexity when dealing with
  // multiple input forms and the fact that a mousedown into an already focused
  // field does not trigger another focus.

  if (mFocusedInput->ControlType() != NS_FORM_INPUT_PASSWORD) {
    return NS_OK;
  }

  // If we have not seen a right click yet, just show the popup.
  if (mLastRightClickTimeStamp.IsNull()) {
    ShowPopup();
    return NS_OK;
  }

  uint64_t timeDiff = (TimeStamp::Now() - mLastRightClickTimeStamp).ToMilliseconds();
  if (timeDiff > mFocusAfterRightClickThreshold) {
    ShowPopup();
  }
#endif

  return NS_OK;
}

nsresult
nsFormFillController::KeyPress(Event* aEvent)
{
  NS_ASSERTION(mController, "should have a controller!");
  if (!mFocusedInput || !mController) {
    return NS_OK;
  }

  RefPtr<KeyboardEvent> keyEvent = aEvent->AsKeyboardEvent();
  if (!keyEvent) {
    return NS_ERROR_FAILURE;
  }

  bool cancel = false;
  bool unused = false;

  uint32_t k = keyEvent->KeyCode();
  switch (k) {
  case KeyboardEventBinding::DOM_VK_DELETE:
#ifndef XP_MACOSX
    mController->HandleDelete(&cancel);
    break;
  case KeyboardEventBinding::DOM_VK_BACK_SPACE:
    mController->HandleText(&unused);
    break;
#else
  case KeyboardEventBinding::DOM_VK_BACK_SPACE:
    {
      if (keyEvent->ShiftKey()) {
        mController->HandleDelete(&cancel);
      } else {
        mController->HandleText(&unused);
      }

      break;
    }
#endif
  case KeyboardEventBinding::DOM_VK_PAGE_UP:
  case KeyboardEventBinding::DOM_VK_PAGE_DOWN:
    {
      if (keyEvent->CtrlKey() ||
          keyEvent->AltKey() ||
          keyEvent->MetaKey()) {
        break;
      }
    }
    MOZ_FALLTHROUGH;
  case KeyboardEventBinding::DOM_VK_UP:
  case KeyboardEventBinding::DOM_VK_DOWN:
  case KeyboardEventBinding::DOM_VK_LEFT:
  case KeyboardEventBinding::DOM_VK_RIGHT:
    {
      // Get the writing-mode of the relevant input element,
      // so that we can remap arrow keys if necessary.
      mozilla::WritingMode wm;
      if (mFocusedInput) {
        nsIFrame *frame = mFocusedInput->GetPrimaryFrame();
        if (frame) {
          wm = frame->GetWritingMode();
        }
      }
      if (wm.IsVertical()) {
        switch (k) {
        case KeyboardEventBinding::DOM_VK_LEFT:
          k = wm.IsVerticalLR() ? KeyboardEventBinding::DOM_VK_UP
                                : KeyboardEventBinding::DOM_VK_DOWN;
          break;
        case KeyboardEventBinding::DOM_VK_RIGHT:
          k = wm.IsVerticalLR() ? KeyboardEventBinding::DOM_VK_DOWN
                                : KeyboardEventBinding::DOM_VK_UP;
          break;
        case KeyboardEventBinding::DOM_VK_UP:
          k = KeyboardEventBinding::DOM_VK_LEFT;
          break;
        case KeyboardEventBinding::DOM_VK_DOWN:
          k = KeyboardEventBinding::DOM_VK_RIGHT;
          break;
        }
      }
    }
    mController->HandleKeyNavigation(k, &cancel);
    break;
  case KeyboardEventBinding::DOM_VK_ESCAPE:
    mController->HandleEscape(&cancel);
    break;
  case KeyboardEventBinding::DOM_VK_TAB:
    mController->HandleTab();
    cancel = false;
    break;
  case KeyboardEventBinding::DOM_VK_RETURN:
    mController->HandleEnter(false, aEvent, &cancel);
    break;
  }

  if (cancel) {
    aEvent->PreventDefault();
    // Don't let the page see the RETURN event when the popup is open
    // (indicated by cancel=true) so sites don't manually submit forms
    // (e.g. via submit.click()) without the autocompleted value being filled.
    // Bug 286933 will fix this for other key events.
    if (k == KeyboardEventBinding::DOM_VK_RETURN) {
      aEvent->StopPropagation();
    }
  }

  return NS_OK;
}

nsresult
nsFormFillController::MouseDown(Event* aEvent)
{
  MouseEvent* mouseEvent = aEvent->AsMouseEvent();
  if (!mouseEvent) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINode> targetNode = do_QueryInterface(aEvent->GetTarget());
  if (!HTMLInputElement::FromNodeOrNull(targetNode)) {
    return NS_OK;
  }

  int16_t button = mouseEvent->Button();

  // In case of a right click we set a timestamp that
  // will be checked in Focus() to avoid showing
  // both contextmenu and popup at the same time.
  if (button == 2) {
    mLastRightClickTimeStamp = TimeStamp::Now();
    return NS_OK;
  }

  if (button != 0) {
    return NS_OK;
  }

  return ShowPopup();
}

NS_IMETHODIMP
nsFormFillController::ShowPopup()
{
  bool isOpen = false;
  GetPopupOpen(&isOpen);
  if (isOpen) {
    return SetPopupOpen(false);
  }

  nsCOMPtr<nsIAutoCompleteInput> input;
  mController->GetInput(getter_AddRefs(input));
  if (!input) {
    return NS_OK;
  }

  nsAutoString value;
  input->GetTextValue(value);
  if (value.Length() > 0) {
    // Show the popup with a filtered result set
    mController->SetSearchString(EmptyString());
    bool unused = false;
    mController->HandleText(&unused);
  } else {
    // Show the popup with the complete result set.  Can't use HandleText()
    // because it doesn't display the popup if the input is blank.
    bool cancel = false;
    mController->HandleKeyNavigation(KeyboardEventBinding::DOM_VK_DOWN, &cancel);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsFormFillController

void
nsFormFillController::AddWindowListeners(nsPIDOMWindowOuter* aWindow)
{
  MOZ_LOG(sLogger, LogLevel::Debug, ("AddWindowListeners for window %p", aWindow));
  if (!aWindow) {
    return;
  }

  EventTarget* target = aWindow->GetChromeEventHandler();

  if (!target) {
    return;
  }

  EventListenerManager* elm = target->GetOrCreateListenerManager();
  if (NS_WARN_IF(!elm)) {
    return;
  }

  elm->AddEventListenerByType(this, NS_LITERAL_STRING("focus"),
                              TrustedEventsAtCapture());
  elm->AddEventListenerByType(this, NS_LITERAL_STRING("blur"),
                              TrustedEventsAtCapture());
  elm->AddEventListenerByType(this, NS_LITERAL_STRING("pagehide"),
                              TrustedEventsAtCapture());
  elm->AddEventListenerByType(this, NS_LITERAL_STRING("mousedown"),
                              TrustedEventsAtCapture());
  elm->AddEventListenerByType(this, NS_LITERAL_STRING("input"),
                              TrustedEventsAtCapture());
  elm->AddEventListenerByType(this, NS_LITERAL_STRING("keypress"),
                              TrustedEventsAtSystemGroupCapture());
  elm->AddEventListenerByType(this, NS_LITERAL_STRING("compositionstart"),
                              TrustedEventsAtCapture());
  elm->AddEventListenerByType(this, NS_LITERAL_STRING("compositionend"),
                              TrustedEventsAtCapture());
  elm->AddEventListenerByType(this, NS_LITERAL_STRING("contextmenu"),
                              TrustedEventsAtCapture());

  // Note that any additional listeners added should ensure that they ignore
  // untrusted events, which might be sent by content that's up to no good.
}

void
nsFormFillController::RemoveWindowListeners(nsPIDOMWindowOuter* aWindow)
{
  MOZ_LOG(sLogger, LogLevel::Debug, ("RemoveWindowListeners for window %p", aWindow));
  if (!aWindow) {
    return;
  }

  StopControllingInput();

  nsCOMPtr<nsIDocument> doc = aWindow->GetDoc();
  RemoveForDocument(doc);

  EventTarget* target = aWindow->GetChromeEventHandler();

  if (!target) {
    return;
  }

  EventListenerManager* elm = target->GetOrCreateListenerManager();
  if (NS_WARN_IF(!elm)) {
    return;
  }

  elm->RemoveEventListenerByType(this, NS_LITERAL_STRING("focus"),
                                 TrustedEventsAtCapture());
  elm->RemoveEventListenerByType(this, NS_LITERAL_STRING("blur"),
                                 TrustedEventsAtCapture());
  elm->RemoveEventListenerByType(this, NS_LITERAL_STRING("pagehide"),
                                 TrustedEventsAtCapture());
  elm->RemoveEventListenerByType(this, NS_LITERAL_STRING("mousedown"),
                                 TrustedEventsAtCapture());
  elm->RemoveEventListenerByType(this, NS_LITERAL_STRING("input"),
                                 TrustedEventsAtCapture());
  elm->RemoveEventListenerByType(this, NS_LITERAL_STRING("keypress"),
                                 TrustedEventsAtSystemGroupCapture());
  elm->RemoveEventListenerByType(this, NS_LITERAL_STRING("compositionstart"),
                                 TrustedEventsAtCapture());
  elm->RemoveEventListenerByType(this, NS_LITERAL_STRING("compositionend"),
                                 TrustedEventsAtCapture());
  elm->RemoveEventListenerByType(this, NS_LITERAL_STRING("contextmenu"),
                                 TrustedEventsAtCapture());
}

void
nsFormFillController::StartControllingInput(HTMLInputElement *aInput)
{
  MOZ_LOG(sLogger, LogLevel::Verbose, ("StartControllingInput for %p", aInput));
  // Make sure we're not still attached to an input
  StopControllingInput();

  if (!mController) {
    return;
  }

  // Find the currently focused docShell
  nsCOMPtr<nsIDocShell> docShell = GetDocShellForInput(aInput);
  int32_t index = GetIndexOfDocShell(docShell);
  if (index < 0) {
    return;
  }

  MOZ_ASSERT(aInput, "How did we get a docshell index??");

  // Cache the popup for the focused docShell
  mFocusedPopup = mPopups.SafeElementAt(index);

  aInput->AddMutationObserverUnlessExists(this);
  mFocusedInput = aInput;

  Element* list = mFocusedInput->GetList();
  if (list) {
    list->AddMutationObserverUnlessExists(this);
    mListNode = list;
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
    if (input == this) {
      MOZ_LOG(sLogger, LogLevel::Verbose,
              ("StopControllingInput: Nulled controller input for %p", this));
      mController->SetInput(nullptr);
    }
  }

  MOZ_LOG(sLogger, LogLevel::Verbose,
          ("StopControllingInput: Stopped controlling %p", mFocusedInput));
  if (mFocusedInput) {
    MaybeRemoveMutationObserver(mFocusedInput);

    mFocusedInput = nullptr;
  }

  if (mFocusedPopup) {
    mFocusedPopup->ClosePopup();
  }
  mFocusedPopup = nullptr;
}

nsIDocShell *
nsFormFillController::GetDocShellForInput(HTMLInputElement *aInput)
{
  NS_ENSURE_TRUE(aInput, nullptr);

  nsCOMPtr<nsPIDOMWindowOuter> win = aInput->OwnerDoc()->GetWindow();
  NS_ENSURE_TRUE(win, nullptr);

  return win->GetDocShell();
}

nsPIDOMWindowOuter*
nsFormFillController::GetWindowForDocShell(nsIDocShell *aDocShell)
{
  nsCOMPtr<nsIContentViewer> contentViewer;
  aDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  NS_ENSURE_TRUE(contentViewer, nullptr);

  nsCOMPtr<nsIDocument> doc = contentViewer->GetDocument();
  NS_ENSURE_TRUE(doc, nullptr);

  return doc->GetWindow();
}

int32_t
nsFormFillController::GetIndexOfDocShell(nsIDocShell *aDocShell)
{
  if (!aDocShell) {
    return -1;
  }

  // Loop through our cached docShells looking for the given docShell
  uint32_t count = mDocShells.Length();
  for (uint32_t i = 0; i < count; ++i) {
    if (mDocShells[i] == aDocShell) {
      return i;
    }
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
