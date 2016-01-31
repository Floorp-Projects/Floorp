/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsFormFillController__
#define __nsFormFillController__

#include "nsIFormFillController.h"
#include "nsIAutoCompleteInput.h"
#include "nsIAutoCompleteSearch.h"
#include "nsIAutoCompleteController.h"
#include "nsIAutoCompletePopup.h"
#include "nsIFormAutoComplete.h"
#include "nsIDOMEventListener.h"
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsIDocShell.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsILoginManager.h"
#include "nsIMutationObserver.h"
#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"

// X.h defines KeyPress
#ifdef KeyPress
#undef KeyPress
#endif

class nsFormHistory;
class nsINode;
class nsPIDOMWindowOuter;

class nsFormFillController final : public nsIFormFillController,
                                   public nsIAutoCompleteInput,
                                   public nsIAutoCompleteSearch,
                                   public nsIDOMEventListener,
                                   public nsIFormAutoCompleteObserver,
                                   public nsIMutationObserver
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIFORMFILLCONTROLLER
  NS_DECL_NSIAUTOCOMPLETESEARCH
  NS_DECL_NSIAUTOCOMPLETEINPUT
  NS_DECL_NSIFORMAUTOCOMPLETEOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIMUTATIONOBSERVER

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsFormFillController, nsIFormFillController)

  nsresult Focus(nsIDOMEvent* aEvent);
  nsresult KeyPress(nsIDOMEvent* aKeyEvent);
  nsresult MouseDown(nsIDOMEvent* aMouseEvent);

  nsFormFillController();

protected:
  virtual ~nsFormFillController();

  void AddWindowListeners(nsPIDOMWindowOuter* aWindow);
  void RemoveWindowListeners(nsPIDOMWindowOuter* aWindow);

  void AddKeyListener(nsINode* aInput);
  void RemoveKeyListener();

  void StartControllingInput(nsIDOMHTMLInputElement *aInput);
  void StopControllingInput();
  /**
   * Checks that aElement is a type of element we want to fill, then calls
   * StartControllingInput on it.
   */
  void MaybeStartControllingInput(nsIDOMHTMLInputElement* aElement);

  nsresult PerformInputListAutoComplete(const nsAString& aSearch,
                                        nsIAutoCompleteResult** aResult);

  void RevalidateDataList();
  bool RowMatch(nsFormHistory *aHistory, uint32_t aIndex, const nsAString &aInputName, const nsAString &aInputValue);

  inline nsIDocShell *GetDocShellForInput(nsIDOMHTMLInputElement *aInput);
  inline nsPIDOMWindowOuter *GetWindowForDocShell(nsIDocShell *aDocShell);
  inline int32_t GetIndexOfDocShell(nsIDocShell *aDocShell);

  void MaybeRemoveMutationObserver(nsINode* aNode);

  void RemoveForDocument(nsIDocument* aDoc);
  bool IsEventTrusted(nsIDOMEvent *aEvent);
  // members //////////////////////////////////////////

  nsCOMPtr<nsIAutoCompleteController> mController;
  nsCOMPtr<nsILoginManager> mLoginManager;
  nsIDOMHTMLInputElement* mFocusedInput;
  nsINode* mFocusedInputNode;

  // mListNode is a <datalist> element which, is set, has the form fill controller
  // as a mutation observer for it.
  nsINode* mListNode;
  nsCOMPtr<nsIAutoCompletePopup> mFocusedPopup;

  nsTArray<nsCOMPtr<nsIDocShell> > mDocShells;
  nsTArray<nsCOMPtr<nsIAutoCompletePopup> > mPopups;

  // The observer passed to StartSearch. It will be notified when the search is
  // complete or the data from a datalist changes.
  nsCOMPtr<nsIAutoCompleteObserver> mLastListener;

  // This is cleared by StopSearch().
  nsCOMPtr<nsIFormAutoComplete> mLastFormAutoComplete;
  nsString mLastSearchString;

  nsDataHashtable<nsPtrHashKey<const nsINode>, bool> mPwmgrInputs;

  uint32_t mTimeout;
  uint32_t mMinResultsForPopup;
  uint32_t mMaxRows;
  bool mDisableAutoComplete;
  bool mCompleteDefaultIndex;
  bool mCompleteSelectedIndex;
  bool mForceComplete;
  bool mSuppressOnInput;
};

#endif // __nsFormFillController__
