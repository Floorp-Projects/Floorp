/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_ActiveElementManager_h__
#define __mozilla_widget_ActiveElementManager_h__

#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"

class inIDOMUtils;
class nsIDOMEventTarget;
class nsIDOMElement;
class CancelableTask;

namespace mozilla {
namespace widget {

/**
 * Manages setting and clearing the ':active' CSS pseudostate in the presence
 * of touch input.
 */
class ActiveElementManager {
public:
  NS_INLINE_DECL_REFCOUNTING(ActiveElementManager)

  ActiveElementManager();
  ~ActiveElementManager();

  /**
   * Specify the target of a touch. Typically this should be called right
   * before HandleTouchStart(), but we give callers the flexibility to specify
   * the target later if they don't know it at the time they call
   * HandleTouchStart().
   * |aTarget| may be nullptr.
   */
  void SetTargetElement(nsIDOMEventTarget* aTarget);
  /**
   * Handle a touch-start event.
   * @param aCanBePan whether the touch can be a pan
   */
  void HandleTouchStart(bool aCanBePan);
  /**
   * Handle the start of panning.
   */
  void HandlePanStart();
  /**
   * Handle a touch-end or touch-cancel event.
   * @param aWasClick whether the touch was a click
   */
  void HandleTouchEnd(bool aWasClick);
private:
  nsCOMPtr<inIDOMUtils> mDomUtils;
  /**
   * The target of the first touch point in the current touch block.
   */
  nsCOMPtr<nsIDOMElement> mTarget;
  /**
   * Whether the current touch block can be a pan. Set in HandleTouchStart().
   */
  bool mCanBePan;
  /**
   * Whether mCanBePan has been set for the current touch block.
   * We need to keep track of this to allow HandleTouchStart() and
   * SetTargetElement() to be called in either order.
   */
  bool mCanBePanSet;
  /**
   * A task for calling SetActive() after a timeout.
   */
  CancelableTask* mSetActiveTask;

  // Helpers
  void TriggerElementActivation();
  void SetActive(nsIDOMElement* aTarget);
  void ResetActive();
  void SetActiveTask(nsIDOMElement* aTarget);
  void CancelTask();
};

}
}

#endif /*__mozilla_widget_ActiveElementManager_h__ */
