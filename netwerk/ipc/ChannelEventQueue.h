/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Matthews <josh@joshmatthews.net> (initial developer)
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

#ifndef mozilla_net_ChannelEventQueue_h
#define mozilla_net_ChannelEventQueue_h

namespace mozilla {
namespace net {

class ChannelEvent
{
 public:
  ChannelEvent() { MOZ_COUNT_CTOR(ChannelEvent); }
  virtual ~ChannelEvent() { MOZ_COUNT_DTOR(ChannelEvent); }
  virtual void Run() = 0;
};

// Workaround for Necko re-entrancy dangers. We buffer IPDL messages in a
// queue if still dispatching previous one(s) to listeners/observers.
// Otherwise synchronous XMLHttpRequests and/or other code that spins the
// event loop (ex: IPDL rpc) could cause listener->OnDataAvailable (for
// instance) to be called before mListener->OnStartRequest has completed.

class ChannelEventQueue
{
 public:
  ChannelEventQueue() : mQueuePhase(PHASE_UNQUEUED) {}
  ~ChannelEventQueue() {}
  
 protected:
  void BeginEventQueueing();
  void EndEventQueueing();
  void EnqueueEvent(ChannelEvent* callback);
  bool ShouldEnqueue();

  // Consumers must implement their own flushing routine, as there are too many
  // implementation-specific details to generalize easily.
  virtual void FlushEventQueue() = 0;

  nsTArray<nsAutoPtr<ChannelEvent> > mEventQueue;
  enum {
    PHASE_UNQUEUED,
    PHASE_QUEUEING,
    PHASE_FINISHED_QUEUEING,
    PHASE_FLUSHING
  } mQueuePhase;

  friend class AutoEventEnqueuer;
};

inline void
ChannelEventQueue::BeginEventQueueing()
{
  if (mQueuePhase != PHASE_UNQUEUED)
    return;
  // Store incoming IPDL messages for later.
  mQueuePhase = PHASE_QUEUEING;
}

inline void
ChannelEventQueue::EndEventQueueing()
{
  if (mQueuePhase != PHASE_QUEUEING)
    return;

  mQueuePhase = PHASE_FINISHED_QUEUEING;
}

inline bool
ChannelEventQueue::ShouldEnqueue()
{
  return mQueuePhase != PHASE_UNQUEUED;
}

inline void
ChannelEventQueue::EnqueueEvent(ChannelEvent* callback)
{
  mEventQueue.AppendElement(callback);
}

// Ensures any incoming IPDL msgs are queued during its lifetime, and flushes
// the queue when it goes out of scope.
class AutoEventEnqueuer 
{
 public:
  AutoEventEnqueuer(ChannelEventQueue* queue) : mEventQueue(queue) 
  {
    mEventQueue->BeginEventQueueing();
  }
  ~AutoEventEnqueuer() 
  { 
    mEventQueue->EndEventQueueing();
    mEventQueue->FlushEventQueue(); 
  }
 private:
  ChannelEventQueue *mEventQueue;
};

}
}

#endif
