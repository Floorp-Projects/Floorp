/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Maintains a circular buffer of recent messages, and notifies
 * listeners when new messages are logged.
 */

/* Threadsafe. */

#include "nsMemory.h"
#include "nsIServiceManager.h"
#include "nsIProxyObjectManager.h"

#include "nsConsoleService.h"
#include "nsConsoleMessage.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsConsoleService, nsIConsoleService);

nsConsoleService::nsConsoleService()
    : mCurrent(0), mFull(PR_FALSE), mListening(PR_FALSE), mLock(nsnull)
{
    NS_INIT_REFCNT();
 
    // XXX grab this from a pref!
    // hm, but worry about circularity, bc we want to be able to report
    // prefs errs...
    mBufferSize = 250;

    // XXX deal with below three by detecting null mLock in factory?
    nsresult rv;
    rv = nsSupportsArray::Create(NULL, NS_GET_IID(nsISupportsArray),
                                 (void**)getter_AddRefs(mListeners));
    mMessages = (nsIConsoleMessage **)
        nsMemory::Alloc(mBufferSize * sizeof(nsIConsoleMessage *));

    mLock = PR_NewLock();

    // Array elements should be 0 initially for circular buffer algorithm.
    for (PRUint32 i = 0; i < mBufferSize; i++) {
        mMessages[i] = nsnull;
    }

}

nsConsoleService::~nsConsoleService()
{
    PRUint32 i = 0;
    while (i < mBufferSize && mMessages[i] != nsnull) {
        NS_RELEASE(mMessages[i]);
        i++;
    }

#ifdef DEBUG_mccabe
    {
        PRUint32 listenerCount;
        nsresult rv;
        rv = mListeners->Count(&listenerCount);
        if (listenerCount != 0) {
            fprintf(stderr, 
                "WARNING - %d console error listeners still registered!\n"
                "More calls to nsIConsoleService::UnregisterListener needed.\n",
                listenerCount);
        }
    }
#endif

    nsMemory::Free(mMessages);
    if (mLock)
        PR_DestroyLock(mLock);
}

// nsIConsoleService methods
NS_IMETHODIMP
nsConsoleService::LogMessage(nsIConsoleMessage *message)
{
    if (message == nsnull)
        return NS_ERROR_INVALID_ARG;

    nsSupportsArray listenersSnapshot;
    nsIConsoleMessage *retiredMessage;

    NS_ADDREF(message); // early, in case it's same as replaced below.

    /*
     * Lock while updating buffer, and while taking snapshot of
     * listeners array.
     */
    {
        nsAutoLock lock(mLock);

        /*
         * If there's already a message in the slot we're about to replace,
         * we've wrapped around, and we need to release the old message.  We
         * save a pointer to it, so we can release below outside the lock.
         */
        retiredMessage = mMessages[mCurrent];
        
        mMessages[mCurrent++] = message;
        if (mCurrent == mBufferSize) {
            mCurrent = 0; // wrap around.
            mFull = PR_TRUE;
        }

        listenersSnapshot.AppendElements(mListeners);
    }
    if (retiredMessage != nsnull)
        NS_RELEASE(retiredMessage);

    /*
     * Iterate through any registered listeners and tell them about
     * the message.  We use the mListening flag to guard against
     * recursive message logs.  This could sometimes result in
     * listeners being skipped because of activity on other threads,
     * when we only care about the recursive case.
     */
    nsCOMPtr<nsIConsoleListener> listener;
    nsresult rv;
    nsresult returned_rv;
    PRUint32 snapshotCount;
    rv = listenersSnapshot.Count(&snapshotCount);
    if (NS_FAILED(rv))
        return rv;

    {
        nsAutoLock lock(mLock);
        if (mListening)
            return NS_OK;
        mListening = PR_TRUE;
    }

    returned_rv = NS_OK;
    for (PRUint32 i = 0; i < snapshotCount; i++) {
        rv = listenersSnapshot.GetElementAt(i, getter_AddRefs(listener));
        if (NS_FAILED(rv)) {
            returned_rv = rv;
            break; // fall thru to mListening restore code below.
        }
        listener->Observe(message);
    }
    
    {
        nsAutoLock lock(mLock);
        mListening = PR_FALSE;
    }

    return returned_rv;
}

NS_IMETHODIMP
nsConsoleService::LogStringMessage(const PRUnichar *message)
{
    nsConsoleMessage *msg = new nsConsoleMessage(message);
    return this->LogMessage(msg);
}

NS_IMETHODIMP
nsConsoleService::GetMessageArray(nsIConsoleMessage ***messages, PRUint32 *count)
{
    nsIConsoleMessage **messageArray;

    /*
     * Lock the whole method, as we don't want anyone mucking with mCurrent or
     * mFull while we're copying out the buffer.
     */
    nsAutoLock lock(mLock);

    if (mCurrent == 0 && !mFull) {
        /*
         * Make a 1-length output array so that nobody gets confused,
         * and return a count of 0.  This should result in a 0-length
         * array object when called from script.
         */
        messageArray = (nsIConsoleMessage **)
            nsMemory::Alloc(sizeof (nsIConsoleMessage *));
        *messageArray = nsnull;
        *messages = messageArray;
        *count = 0;
        
        return NS_OK;
    }

    PRUint32 resultSize = mFull ? mBufferSize : mCurrent;
    messageArray =
        (nsIConsoleMessage **)nsMemory::Alloc((sizeof (nsIConsoleMessage *))
                                              * resultSize);

    if (messageArray == nsnull) {
        *messages = nsnull;
        *count = 0;
        return NS_ERROR_FAILURE;
    }

    PRUint32 i;
    if (mFull) {
        for (i = 0; i < mBufferSize; i++) {
            // if full, fill the buffer starting from mCurrent (which'll be
            // oldest) wrapping around the buffer to the most recent.
            messageArray[i] = mMessages[(mCurrent + i) % mBufferSize];
            NS_ADDREF(messageArray[i]);
        }
    } else {
        for (i = 0; i < mCurrent; i++) {
            messageArray[i] = mMessages[i];
            NS_ADDREF(messageArray[i]);
        }
    }
    *count = resultSize;
    *messages = messageArray;

    return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::RegisterListener(nsIConsoleListener *listener) {
    nsresult rv;

    /*
     * Store a threadsafe proxy to the listener rather than the
     * listener itself; we want the console service to be callable
     * from any thread, but listeners can be implemented in
     * thread-specific ways, and we always want to call them on their
     * originating thread.  JavaScript is the motivating example.
     */
    nsCOMPtr<nsIConsoleListener> proxiedListener;

    rv = GetProxyForListener(listener, getter_AddRefs(proxiedListener));
    if (NS_FAILED(rv))
        return rv;

    {
        nsAutoLock lock(mLock);
        
        // ignore rv for now, as a comment says it returns prbool instead of
        // nsresult.
        // Any need to check for multiply registered listeners?
        mListeners->AppendElement(proxiedListener);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::UnregisterListener(nsIConsoleListener *listener) {
    nsresult rv;

    nsCOMPtr<nsIConsoleListener> proxiedListener;
    rv = GetProxyForListener(listener, getter_AddRefs(proxiedListener));
    if (NS_FAILED(rv))
        return rv;

    {
        nsAutoLock lock(mLock);

        // ignore rv below for now, as a comment says it returns
        // prbool instead of nsresult.

        // Solaris needs the nsISupports cast to avoid confusion with
        // another nsSupportsArray::RemoveElement overloading.
        mListeners->RemoveElement((const nsISupports *)proxiedListener);

    }
    return NS_OK;
}

nsresult 
nsConsoleService::GetProxyForListener(nsIConsoleListener* aListener,
                                      nsIConsoleListener** aProxy)
{
    nsresult rv;
    *aProxy = nsnull;

    nsCOMPtr<nsIProxyObjectManager> proxyManager =
        (do_GetService(NS_XPCOMPROXY_CONTRACTID));

    if (proxyManager == nsnull)
        return NS_ERROR_NOT_AVAILABLE;

    /*
     * NOTE this will fail if the calling thread doesn't have an eventQ.
     *
     * Would it be better to catch that case and leave the listener unproxied?
     */
    rv = proxyManager->GetProxyForObject(NS_CURRENT_EVENTQ,
                                         NS_GET_IID(nsIConsoleListener),
                                         aListener,
                                         PROXY_ASYNC | PROXY_ALWAYS,
                                         (void**) aProxy);
    return rv;
}
