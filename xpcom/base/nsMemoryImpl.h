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

#ifndef nsMemoryImpl_h__
#define nsMemoryImpl_h__

#include "nsMemory.h"
#include "nsISupportsArray.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsCOMPtr.h"
#include "plevent.h"

struct PRLock;
class MemoryFlusher;

class nsMemoryImpl : public nsIMemory
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIMEMORY

    nsMemoryImpl();
    virtual ~nsMemoryImpl();

    nsresult FlushMemory(const PRUnichar* aReason, PRBool aImmediate);

    // called from xpcom initialization/finalization:
    static nsresult Startup();
    static nsresult Shutdown();

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

protected:
    MemoryFlusher* mFlusher;
    nsCOMPtr<nsIThread> mFlusherThread;

    PRLock* mFlushLock;
    PRBool  mIsFlushing;

    struct FlushEvent {
        PLEvent mEvent;
        const PRUnichar* mReason;
    };

    FlushEvent mFlushEvent;

    static nsresult RunFlushers(nsMemoryImpl* aSelf, const PRUnichar* aReason);

    static void* PR_CALLBACK HandleFlushEvent(PLEvent* aEvent);
    static void  PR_CALLBACK DestroyFlushEvent(PLEvent* aEvent);
};

#endif // nsMemoryImpl_h__
