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
 * Contributor(s): 
 */

/*
 * nsConsoleService class declaration.
 */

#ifndef __nsconsoleservice_h__
#define __nsconsoleservice_h__

#include "nsSupportsArray.h"
#include "nsCOMPtr.h"
#include "nsAutoLock.h"

#include "nsIConsoleService.h"

class nsConsoleService : public nsIConsoleService
{
public:
    nsConsoleService();
    virtual ~nsConsoleService();

    NS_DECL_ISUPPORTS
    NS_DECL_NSICONSOLESERVICE

private:
    // build (or find) a proxy for the listener
    nsresult GetProxyForListener(nsIConsoleListener* aListener,
                                 nsIConsoleListener** aProxy);

    // Circular buffer of saved messages
    nsIConsoleMessage **mMessages;

    // How big?
    PRUint32 mBufferSize;

    // Index of slot in mMessages that'll be filled by *next* log message
    PRUint32 mCurrent;

    // Is the buffer full? (Has mCurrent wrapped around at least once?)
    PRBool mFull;

    // Listeners to notify whenever a new message is logged.
    nsCOMPtr<nsSupportsArray> mListeners;

    // Current listener being notified of a logged error - to prevent
    // stack overflows.
    PRBool mListening;

    // To serialize interesting methods.
    PRLock *mLock;
};

#endif /* __nsconsoleservice_h__ */
