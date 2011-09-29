/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Simon Bünzli <zeniko@gmail.com>
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

/*
 * Maintains a circular buffer of recent messages, and notifies
 * listeners when new messages are logged.
 */

/* Threadsafe. */

#include "nsMemory.h"
#include "nsIServiceManager.h"
#include "nsIProxyObjectManager.h"
#include "nsCOMArray.h"
#include "nsThreadUtils.h"

#include "nsConsoleService.h"
#include "nsConsoleMessage.h"
#include "nsIClassInfoImpl.h"

using namespace mozilla;

NS_IMPL_THREADSAFE_ADDREF(nsConsoleService)
NS_IMPL_THREADSAFE_RELEASE(nsConsoleService)
NS_IMPL_CLASSINFO(nsConsoleService, NULL, nsIClassInfo::THREADSAFE | nsIClassInfo::SINGLETON, NS_CONSOLESERVICE_CID)
NS_IMPL_QUERY_INTERFACE1_CI(nsConsoleService, nsIConsoleService)
NS_IMPL_CI_INTERFACE_GETTER1(nsConsoleService, nsIConsoleService)

nsConsoleService::nsConsoleService()
    : mMessages(nsnull), mCurrent(0), mFull(PR_FALSE), mListening(PR_FALSE), mLock("nsConsoleService.mLock")
{
    // XXX grab this from a pref!
    // hm, but worry about circularity, bc we want to be able to report
    // prefs errs...
    mBufferSize = 250;
}

nsConsoleService::~nsConsoleService()
{
    PRUint32 i = 0;
    while (i < mBufferSize && mMessages[i] != nsnull) {
        NS_RELEASE(mMessages[i]);
        i++;
    }

#ifdef DEBUG_mccabe
    if (mListeners.Count() != 0) {
        fprintf(stderr, 
            "WARNING - %d console error listeners still registered!\n"
            "More calls to nsIConsoleService::UnregisterListener needed.\n",
            mListeners.Count());
    }
    
#endif

    if (mMessages)
        nsMemory::Free(mMessages);
}

nsresult
nsConsoleService::Init()
{
    mMessages = (nsIConsoleMessage **)
        nsMemory::Alloc(mBufferSize * sizeof(nsIConsoleMessage *));
    if (!mMessages)
        return NS_ERROR_OUT_OF_MEMORY;

    // Array elements should be 0 initially for circular buffer algorithm.
    memset(mMessages, 0, mBufferSize * sizeof(nsIConsoleMessage *));

    return NS_OK;
}

static bool snapshot_enum_func(nsHashKey *key, void *data, void* closure)
{
    nsCOMArray<nsIConsoleListener> *array =
      reinterpret_cast<nsCOMArray<nsIConsoleListener> *>(closure);

    // Copy each element into the temporary nsCOMArray...
    array->AppendObject((nsIConsoleListener*)data);
    return PR_TRUE;
}

// nsIConsoleService methods
NS_IMETHODIMP
nsConsoleService::LogMessage(nsIConsoleMessage *message)
{
    if (message == nsnull)
        return NS_ERROR_INVALID_ARG;

    nsCOMArray<nsIConsoleListener> listenersSnapshot;
    nsIConsoleMessage *retiredMessage;

    NS_ADDREF(message); // early, in case it's same as replaced below.

    /*
     * Lock while updating buffer, and while taking snapshot of
     * listeners array.
     */
    {
        MutexAutoLock lock(mLock);

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

        /*
         * Copy the listeners into the snapshot array - in case a listener
         * is removed during an Observe(...) notification...
         */
        mListeners.Enumerate(snapshot_enum_func, &listenersSnapshot);
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
    PRInt32 snapshotCount = listenersSnapshot.Count();

    {
        MutexAutoLock lock(mLock);
        if (mListening)
            return NS_OK;
        mListening = PR_TRUE;
    }

    for (PRInt32 i = 0; i < snapshotCount; i++) {
        listenersSnapshot[i]->Observe(message);
    }
    
    {
        MutexAutoLock lock(mLock);
        mListening = PR_FALSE;
    }

    return NS_OK;
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
    MutexAutoLock lock(mLock);

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
        MutexAutoLock lock(mLock);
        nsISupportsKey key(listener);

        /*
         * Put the proxy event listener into a hashtable using the *real* 
         * listener as the key.
         *
         * This is necessary because proxy objects do *not* maintain
         * nsISupports identity.  Therefore, since GetProxyForListener(...)
         * can return different proxies for the same object (see bug #85831)
         * we need to use the real object as the unique key...
         */
        mListeners.Put(&key, proxiedListener);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::UnregisterListener(nsIConsoleListener *listener) {
    MutexAutoLock lock(mLock);

    nsISupportsKey key(listener);
    mListeners.Remove(&key);
    return NS_OK;
}

nsresult 
nsConsoleService::GetProxyForListener(nsIConsoleListener* aListener,
                                      nsIConsoleListener** aProxy)
{
    /*
     * Would it be better to catch that case and leave the listener unproxied?
     */
    return NS_GetProxyForObject(NS_PROXY_TO_CURRENT_THREAD,
                                NS_GET_IID(nsIConsoleListener),
                                aListener,
                                NS_PROXY_ASYNC | NS_PROXY_ALWAYS,
                                (void**) aProxy);
}

NS_IMETHODIMP
nsConsoleService::Reset()
{
    /*
     * Make sure nobody trips into the buffer while it's being reset
     */
    MutexAutoLock lock(mLock);

    mCurrent = 0;
    mFull = PR_FALSE;

    /*
     * Free all messages stored so far (cf. destructor)
     */
    for (PRUint32 i = 0; i < mBufferSize && mMessages[i] != nsnull; i++)
        NS_RELEASE(mMessages[i]);

    return NS_OK;
}
