/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

/*
 * Maintains a circular buffer of recent messages, and notifies
 * listeners when new messages are logged.
 */

#include "nsConsoleService.h"
#include "nsConsoleMessage.h"
#include "nsIAllocator.h"

NS_IMPL_THREADSAFE_ISUPPORTS(nsConsoleService, NS_GET_IID(nsIConsoleService));

nsConsoleService::nsConsoleService()
{
    mCurrent = 0;
    mFull = PR_FALSE;
 
    // XXX grab this from a pref!
    // hm, but worry about circularity, bc we want to be able to report
    // prefs errs...
    mBufferSize = 250;

    // Any reasonable way to deal with failure of the below two?
    nsresult rv;
    rv = nsSupportsArray::Create(NULL, NS_GET_IID(nsISupportsArray),
                                 (void**)&mListeners);
    mMessages = (nsIConsoleMessage **)
        nsAllocator::Alloc(mBufferSize * sizeof(nsIConsoleMessage *));

    // Array elements should be 0 initially for circular buffer algorithm.
    for (PRUint32 i = 0; i < mBufferSize; i++) {
        mMessages[i] = nsnull;
    }

    NS_INIT_REFCNT();
}

nsConsoleService::~nsConsoleService()
{
    PRUint32 i = 0;
    while (i < mBufferSize && mMessages[i] != nsnull) {
        NS_RELEASE(mMessages[i]);
        i++;
    }
    nsAllocator::Free(mMessages);
}

NS_IMETHODIMP
nsConsoleService::LogMessage(nsIConsoleMessage *message)
{
    if (message == nsnull)
        return NS_ERROR_FAILURE;

    // If there's already a message in the slot we're about to replace,
    // we've wrapped around, and we need to release the old message before
    // overwriting it.
    if (mMessages[mCurrent] != nsnull)
        NS_RELEASE(mMessages[mCurrent]);

    NS_ADDREF(message);
    mMessages[mCurrent++] = message;
    if (mCurrent == mBufferSize) {
        mCurrent = 0;
        mFull = PR_TRUE;
    }
    
    // Iterate through any registered listeners and tell them about the message.
    nsIConsoleListener *listener;
    nsresult rv;
    PRUint32 listenerCount;
    rv = mListeners->Count(&listenerCount);
    if (NS_FAILED(rv))
        return rv;

    for (PRUint32 i = 0; i < listenerCount; i++) {
        rv = mListeners->GetElementAt(i, (nsISupports **)&listener);
        if (NS_FAILED(rv))
            return rv;
        listener->Observe(message);
        NS_RELEASE(listener);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::LogStringMessage(const PRUnichar *message)
{
    // LogMessage adds a ref to this, and eventual release does delete.
    nsConsoleMessage *msg = new nsConsoleMessage(message);
    return this->LogMessage(msg);
}

NS_IMETHODIMP
nsConsoleService::GetMessageArray(nsIConsoleMessage ***messages, PRUint32 *count)
{
    nsIConsoleMessage **messageArray;

    if (mCurrent == 0 && !mFull) {
        // Make a 1-length output array so that nobody gets confused,
        // and return 0 count.
        // Hopefully this will (XXX test me) result in a 0-length
        // array object on the script end.
        
        // XXX if it works, remove the try/catch in console.js.
        messageArray = (nsIConsoleMessage **)
            nsAllocator::Alloc(sizeof (nsIConsoleMessage *));
        *messageArray = nsnull;
        *messages = messageArray;
        *count = 0;
        
        return NS_OK;
    }

    PRUint32 resultSize = mFull ? mBufferSize : mCurrent;
    messageArray =
        (nsIConsoleMessage **)nsAllocator::Alloc((sizeof (nsIConsoleMessage *))
                                                 * resultSize);

    
    // XXX jband sez malloc array with 1 member and return 0 count;
    // might not work; if not, bug jband.
    // could also return null if I want to do such.
    // open question about interface contract here.
    //
    // jband sez: should guard against both b/c the interface doesn't specify
    // which.  And add my convention to /** */ in the interface.
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
    // ignore rv for now, as a comment says it returns prbool instead of
    // nsresult.

    // Any need to check for multiply registered listeners?
    mListeners->AppendElement(listener);
    return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::UnregisterListener(nsIConsoleListener *listener) {
    // ignore rv for now, as a comment says it returns prbool instead of
    // nsresult.
    mListeners->RemoveElement(listener);
    return NS_OK;
}




