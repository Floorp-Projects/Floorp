/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
   ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bobby Holley <bobbyholley@gmail.com>
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

#include "nsComponentManagerUtils.h"
#include "nsITimer.h"
#include "RasterImage.h"
#include "DiscardTracker.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace imagelib {

static bool sInitialized = false;
static bool sTimerOn = false;
static PRUint32 sMinDiscardTimeoutMs = 10000; /* Default if pref unreadable. */
static nsITimer *sTimer = nsnull;
static struct DiscardTrackerNode sHead, sSentinel, sTail;

/*
 * Puts an image in the back of the tracker queue. If the image is already
 * in the tracker, this removes it first.
 */
nsresult
DiscardTracker::Reset(DiscardTrackerNode *node)
{
  nsresult rv;
#ifdef DEBUG
  bool isSentinel = (node == &sSentinel);

  // Sanity check the node.
  NS_ABORT_IF_FALSE(isSentinel || node->curr, "Node doesn't point to anything!");

  // We should not call this function if we can't discard
  NS_ABORT_IF_FALSE(isSentinel || node->curr->CanDiscard(),
                    "trying to reset discarding but can't discard!");

  // As soon as an image becomes animated it is set non-discardable
  NS_ABORT_IF_FALSE(isSentinel || !node->curr->mAnim,
                    "Trying to reset discarding on animated image!");
#endif

  // Initialize the first time through
  if (NS_UNLIKELY(!sInitialized)) {
    rv = Initialize();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Remove the node if it's in the list.
  Remove(node);

  // Append it to the list.
  node->prev = sTail.prev;
  node->next = &sTail;
  node->prev->next = sTail.prev = node;

  // Make sure the timer is running
  rv = TimerOn();
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

/*
 * Removes a node from the tracker. No-op if the node is currently untracked.
 */
void
DiscardTracker::Remove(DiscardTrackerNode *node)
{
  NS_ABORT_IF_FALSE(node != nsnull, "Can't pass null node");

  // If we're not in a list, we have nothing to do.
  if ((node->prev == nsnull) || (node->next == nsnull)) {
    NS_ABORT_IF_FALSE(node->prev == node->next,
                      "Node is half in a list!");
    return;
  }

  // Connect around ourselves
  node->prev->next = node->next;
  node->next->prev = node->prev;

  // Clean up the node we removed.
  node->prev = node->next = nsnull;
}

/*
 * Discard all the images we're tracking.
 */
void
DiscardTracker::DiscardAll()
{
  if (!sInitialized)
    return;

  // Remove the sentinel from the list so that the only elements in the list
  // which don't track an image are the head and tail.
  Remove(&sSentinel);

  // Discard all tracked images.
  for (DiscardTrackerNode *node = sHead.next;
       node != &sTail; node = sHead.next) {
    NS_ABORT_IF_FALSE(node->curr, "empty node!");
    Remove(node);
    node->curr->Discard();
  }

  // Add the sentinel back to the (now empty) list.
  Reset(&sSentinel);

  // Because the sentinel is the only element in the list, the next timer event
  // would be a no-op.  Disable the timer as an optimization.
  TimerOff();
}

/**
 * Initialize the tracker.
 */
nsresult
DiscardTracker::Initialize()
{
  nsresult rv;

  // Set up the list. Head<->Sentinel<->Tail
  sHead.curr = sTail.curr = sSentinel.curr = nsnull;
  sHead.prev = sTail.next = nsnull;
  sHead.next = sTail.prev = &sSentinel;
  sSentinel.prev = &sHead;
  sSentinel.next = &sTail;

  // Load the timeout
  ReloadTimeout();

  // Create and start the timer
  nsCOMPtr<nsITimer> t = do_CreateInstance("@mozilla.org/timer;1");
  NS_ENSURE_TRUE(t, NS_ERROR_OUT_OF_MEMORY);
  t.forget(&sTimer);
  rv = TimerOn();
  NS_ENSURE_SUCCESS(rv, rv);

  // Mark us as initialized
  sInitialized = PR_TRUE;

  return NS_OK;
}

/**
 * Shut down the tracker, deallocating the timer.
 */
void
DiscardTracker::Shutdown()
{
  if (sTimer) {
    sTimer->Cancel();
    NS_RELEASE(sTimer);
    sTimer = nsnull;
  }
}

/**
 * Sets the minimum timeout.
 */
void
DiscardTracker::ReloadTimeout()
{
  nsresult rv;

  // read the timeout pref
  PRInt32 discardTimeout;
  rv = Preferences::GetInt(DISCARD_TIMEOUT_PREF, &discardTimeout);

  // If we got something bogus, return
  if (!NS_SUCCEEDED(rv) || discardTimeout <= 0)
    return;

  // If the value didn't change, return
  if ((PRUint32) discardTimeout == sMinDiscardTimeoutMs)
    return;

  // Update the value
  sMinDiscardTimeoutMs = (PRUint32) discardTimeout;

  // If the timer's on, restart the clock to make changes take effect
  if (sTimerOn) {
    TimerOff();
    TimerOn();
  }
}

/**
 * Enables the timer. No-op if the timer is already running.
 */
nsresult
DiscardTracker::TimerOn()
{
  // Nothing to do if the timer's already on.
  if (sTimerOn)
    return NS_OK;
  sTimerOn = PR_TRUE;

  // Activate
  return sTimer->InitWithFuncCallback(TimerCallback,
                                      nsnull,
                                      sMinDiscardTimeoutMs,
                                      nsITimer::TYPE_REPEATING_SLACK);
}

/*
 * Disables the timer. No-op if the timer isn't running.
 */
void
DiscardTracker::TimerOff()
{
  // Nothing to do if the timer's already off.
  if (!sTimerOn)
    return;
  sTimerOn = PR_FALSE;

  // Deactivate
  sTimer->Cancel();
}

/**
 * Routine activated when the timer fires. This discards everything
 * in front of sentinel, and resets the sentinel to the back of the
 * list.
 */
void
DiscardTracker::TimerCallback(nsITimer *aTimer, void *aClosure)
{
  DiscardTrackerNode *node;

  // Remove and discard everything before the sentinel
  for (node = sSentinel.prev; node != &sHead; node = sSentinel.prev) {
    NS_ABORT_IF_FALSE(node->curr, "empty node!");
    Remove(node);
    node->curr->Discard();
  }

  // Append the sentinel to the back of the list
  Reset(&sSentinel);

  // If there's nothing in front of the sentinel, the next callback
  // is guaranteed to be a no-op. Disable the timer as an optimization.
  if (sSentinel.prev == &sHead)
    TimerOff();
}

} // namespace imagelib
} // namespace mozilla
