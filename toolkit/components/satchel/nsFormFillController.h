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
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsInterfaceHashtable.h"
#include "nsIDocShell.h"
#include "nsILoginAutoCompleteSearch.h"
#include "nsIMutationObserver.h"
#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsILoginReputation.h"

// X.h defines KeyPress
#ifdef KeyPress
#  undef KeyPress
#endif

class nsFormHistory;
class nsINode;
class nsPIDOMWindowOuter;

namespace mozilla {
namespace dom {
class HTMLInputElement;
}  // namespace dom
}  // namespace mozilla

class nsFormFillController final : public nsIFormFillController,
                                   public nsIAutoCompleteInput,
                                   public nsIAutoCompleteSearch,
                                   public nsIFormAutoCompleteObserver,
                                   public nsIMutationObserver {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIFORMFILLCONTROLLER
  NS_DECL_NSIAUTOCOMPLETESEARCH
  NS_DECL_NSIAUTOCOMPLETEINPUT
  NS_DECL_NSIFORMAUTOCOMPLETEOBSERVER
  NS_DECL_NSIMUTATIONOBSERVER

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsFormFillController,
                                           nsIFormFillController)

  MOZ_CAN_RUN_SCRIPT nsresult Focus(mozilla::dom::Event* aEvent);
  MOZ_CAN_RUN_SCRIPT nsresult KeyDown(mozilla::dom::Event* aKeyEvent);
  MOZ_CAN_RUN_SCRIPT nsresult KeyPress(mozilla::dom::Event* aKeyEvent);
  MOZ_CAN_RUN_SCRIPT nsresult MouseDown(mozilla::dom::Event* aMouseEvent);

  nsFormFillController();

 protected:
  MOZ_CAN_RUN_SCRIPT virtual ~nsFormFillController();

  void AddWindowListeners(nsPIDOMWindowOuter* aWindow);
  MOZ_CAN_RUN_SCRIPT void RemoveWindowListeners(nsPIDOMWindowOuter* aWindow);

  void AddKeyListener(nsINode* aInput);
  void RemoveKeyListener();

  MOZ_CAN_RUN_SCRIPT
  void StartControllingInput(mozilla::dom::HTMLInputElement* aInput);
  MOZ_CAN_RUN_SCRIPT void StopControllingInput();

  bool IsFocusedInputControlled() const;

  MOZ_CAN_RUN_SCRIPT
  nsresult HandleFocus(mozilla::dom::HTMLInputElement* aInput);

  /**
   * Checks that aElement is a type of element we want to fill, then calls
   * StartControllingInput on it.
   */
  MOZ_CAN_RUN_SCRIPT
  void MaybeStartControllingInput(mozilla::dom::HTMLInputElement* aElement);

  nsresult PerformInputListAutoComplete(const nsAString& aSearch,
                                        nsIAutoCompleteResult** aResult);

  MOZ_CAN_RUN_SCRIPT void RevalidateDataList();
  bool RowMatch(nsFormHistory* aHistory, uint32_t aIndex,
                const nsAString& aInputName, const nsAString& aInputValue);

  inline nsIDocShell* GetDocShellForInput(
      mozilla::dom::HTMLInputElement* aInput);

  void MaybeRemoveMutationObserver(nsINode* aNode);

  void RemoveForDocument(mozilla::dom::Document* aDoc);

  bool IsTextControl(nsINode* aNode);

  nsresult StartQueryLoginReputation(mozilla::dom::HTMLInputElement* aInput);

  // members //////////////////////////////////////////

  nsCOMPtr<nsIAutoCompleteController> mController;
  nsCOMPtr<nsILoginAutoCompleteSearch> mLoginManagerAC;
  nsCOMPtr<nsILoginReputationService> mLoginReputationService;
  mozilla::dom::HTMLInputElement* mFocusedInput;

  // mListNode is a <datalist> element which, is set, has the form fill
  // controller as a mutation observer for it.
  nsINode* mListNode;
  nsCOMPtr<nsIAutoCompletePopup> mFocusedPopup;

  nsInterfaceHashtable<nsRefPtrHashKey<mozilla::dom::Document>,
                       nsIAutoCompletePopup>
      mPopups;

  // The observer passed to StartSearch. It will be notified when the search is
  // complete or the data from a datalist changes.
  nsCOMPtr<nsIAutoCompleteObserver> mLastListener;

  // This is cleared by StopSearch().
  nsCOMPtr<nsIFormAutoComplete> mLastFormAutoComplete;
  nsString mLastSearchString;

  nsDataHashtable<nsPtrHashKey<const nsINode>, bool> mPwmgrInputs;
  nsDataHashtable<nsPtrHashKey<const nsINode>, bool> mAutofillInputs;

  uint16_t mFocusAfterRightClickThreshold;
  uint32_t mTimeout;
  uint32_t mMinResultsForPopup;
  uint32_t mMaxRows;
  mozilla::TimeStamp mLastRightClickTimeStamp;
  bool mDisableAutoComplete;
  bool mCompleteDefaultIndex;
  bool mCompleteSelectedIndex;
  bool mForceComplete;
  bool mSuppressOnInput;
  bool mPasswordPopupAutomaticallyOpened;
};

#endif  // __nsFormFillController__
