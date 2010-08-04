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
 * The Original Code is mozilla.org networking code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Honza Bambas <honzab@firemni.cz>
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

#include "nsAsyncRedirectVerifyHelper.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"

#include "nsIOService.h"
#include "nsIChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIAsyncVerifyRedirectCallback.h"

NS_IMPL_ISUPPORTS1(nsAsyncRedirectVerifyHelper, nsIRunnable)

nsresult
nsAsyncRedirectVerifyHelper::Init(nsIChannel* oldChan, nsIChannel* newChan,
                                  PRUint32 flags, PRBool synchronize)
{
    mOldChan = oldChan;
    mNewChan = newChan;
    mFlags = flags;

    if (synchronize)
      mWaitingForRedirectCallback = PR_TRUE;

    nsresult rv;
    rv = NS_DispatchToMainThread(this);
    NS_ENSURE_SUCCESS(rv, rv);

    if (synchronize) {
      nsIThread *thread = NS_GetCurrentThread();
      while (mWaitingForRedirectCallback) {
        if (!NS_ProcessNextEvent(thread)) {
          return NS_ERROR_UNEXPECTED;
        }
      }
    }

    return NS_OK;
}

void
nsAsyncRedirectVerifyHelper::Callback(nsresult result)
{
    // TODO E10S OnRedirectCallback has to be called on the original process
    nsCOMPtr<nsIAsyncVerifyRedirectCallback> callback(do_QueryInterface(mOldChan));
    NS_ASSERTION(callback, "nsAsyncRedirectVerifyHelper: oldChannel doesn't"
                           " implement nsIAsyncVerifyRedirectCallback");

    if (callback)
        callback->OnRedirectVerifyCallback(result);

    mWaitingForRedirectCallback = PR_FALSE;
}

NS_IMETHODIMP
nsAsyncRedirectVerifyHelper::Run()
{
    /* If the channel got canceled after it fired AsyncOnChannelRedirect
     * (bug 546606) and before we got here, mostly because docloader
     * load has been canceled, we must completely ignore this notification
     * and prevent any further notification.
     *
     * TODO Bug 546606, this must be checked before every single call!
     */
    PRBool canceled;
    nsCOMPtr<nsIHttpChannelInternal> oldChannelInternal =
        do_QueryInterface(mOldChan);
    if (oldChannelInternal) {
      oldChannelInternal->GetCanceled(&canceled);
      if (canceled) {
          Callback(NS_BINDING_ABORTED);
          return NS_OK;
      }
    }

    // First, the global observer
    NS_ASSERTION(gIOService, "Must have an IO service at this point");
    nsresult rv = gIOService->OnChannelRedirect(mOldChan, mNewChan, mFlags);
    if (NS_FAILED(rv)) {
        Callback(rv);
        return NS_OK;
    }

    // Now, the per-channel observers
    nsCOMPtr<nsIChannelEventSink> sink;
    NS_QueryNotificationCallbacks(mOldChan, sink);
    if (sink)
        rv = sink->OnChannelRedirect(mOldChan, mNewChan, mFlags);

    Callback(rv);
    return NS_OK;
}
