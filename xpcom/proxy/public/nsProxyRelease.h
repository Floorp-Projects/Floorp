/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 3 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsProxyRelease_h_
#define nsProxyRelease_h__

#include "nsIEventQueueService.h"
#include "pratom.h"
#include "prmem.h"

/**
 * Ensures that the delete of a nsISupports object occurs on the target thread.
 *
 * @param target
 *        the target thread where the doomed object should be released.
 * @param doomed
 *        the doomed object; the object to be released on the target thread.
 * @param alwaysProxy
 *        normally, if NS_ProxyRelease is called on the target thread, then the
 *        doomed object will released directly.  however, if this parameter is
 *        true, then a PLEvent will always be posted to the target thread and
 *        the release will happen when that PLEvent is handled.
 */
NS_COM nsresult NS_ProxyRelease
    (nsIEventTarget *target, nsISupports *doomed, PRBool alwaysProxy=PR_FALSE);


#define NS_IMPL_PROXY_RELEASE(_class)                                           \
NS_IMETHODIMP_(nsrefcnt) _class::Release(void)                                  \
{                                                                               \
  NS_PRECONDITION(0 != mRefCnt, "dup release");                                 \
  nsrefcnt count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);                     \
  NS_LOG_RELEASE(this, count, #_class);                                         \
                                                                                \
  if (count == 0)                                                               \
  {                                                                             \
    mRefCnt = 1; /* stabilize */                                                \
    PRBool callDirectly = PR_TRUE;                                              \
    static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);      \
    nsCOMPtr<nsIEventQueueService> eventQService                                \
      = do_GetService(kEventQueueServiceCID);                                   \
    NS_ASSERTION(eventQService, "event queue service is unavailable");          \
                                                                                \
    nsCOMPtr<nsIEventQueue> eventQ;                                             \
    if (eventQService) {                                                        \
      eventQService->GetThreadEventQueue(NS_UI_THREAD, getter_AddRefs(eventQ)); \
      if (eventQ)                                                               \
        eventQ->IsOnCurrentThread(&callDirectly);                          \
    }                                                                           \
                                                                                \
    if (callDirectly)                                                           \
    {                                                                           \
      NS_RELEASE(this);                                                         \
      return 0;                                                                 \
    }                                                                           \
    PLEvent *event = new PLEvent;                                               \
    if (event == nsnull)                                                        \
    {                                                                           \
      NS_ASSERTION(0, "Could not create a plevent. Deleting on wrong thread!"); \
      NS_DELETEXPCOM(this);                                                     \
      return 0;                                                                 \
    }                                                                           \
                                                                                \
    PL_InitEvent(event,                                                         \
                 NS_STATIC_CAST(nsISupports*, this),                            \
                 ReleaseDestructorEventHandler,                                 \
                 ReleaseDestructorDestroyHandler);                              \
                                                                                \
    eventQ->PostEvent(event);                                                   \
    return 0;                                                                   \
  }                                                                             \
  return count;                                                                 \
}                                                                               \



           
#endif
