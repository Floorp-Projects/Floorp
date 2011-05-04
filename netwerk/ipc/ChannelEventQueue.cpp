/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=2 ts=8 et tw=80 :
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
 *   Jason Duell <jduell.mcbugs@gmail.com>
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

#include "nsISupports.h"
#include "mozilla/net/ChannelEventQueue.h"

namespace mozilla {
namespace net {

void
ChannelEventQueue::FlushQueue()
{
  // Events flushed could include destruction of channel (and our own
  // destructor) unless we make sure its refcount doesn't drop to 0 while this
  // method is running.
  nsCOMPtr<nsISupports> kungFuDeathGrip(mOwner);

  // Prevent flushed events from flushing the queue recursively
  mFlushing = true;

  PRUint32 i;
  for (i = 0; i < mEventQueue.Length(); i++) {
    mEventQueue[i]->Run();
    if (mSuspended)
      break;
  }

  // We will always want to remove at least one finished callback.
  if (i < mEventQueue.Length())
    i++;

  // It is possible for new callbacks to be enqueued as we are
  // flushing the queue, so the queue must not be cleared until
  // all callbacks have run.
  mEventQueue.RemoveElementsAt(0, i);

  mFlushing = false;
}


}
}
