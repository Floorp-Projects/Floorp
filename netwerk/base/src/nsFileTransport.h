/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsFileTransport_h___
#define nsFileTransport_h___

#include "nsIChannel.h"
#include "nsIThread.h"
#include "nsIEventQueue.h"
#include "prmon.h"
#include "nsIEventQueueService.h"
#include "nsIBuffer.h"


class nsFileTransportService;
class nsIBaseStream;
class nsIString;
class nsIBuffer;
class nsIBufferInputStream;

class nsFileTransport : public nsIChannel, public nsIRunnable, nsIBufferObserver
{
public:
    NS_DECL_ISUPPORTS

    // nsIRequest methods:
    NS_IMETHOD IsPending(PRBool *result);
    NS_IMETHOD Cancel(void);
    NS_IMETHOD Suspend(void);
    NS_IMETHOD Resume(void);

    // nsIChannel methods:
    NS_IMETHOD GetURI(nsIURI * *aURL);
    NS_IMETHOD OpenInputStream(PRUint32 startPosition, PRInt32 readCount, nsIInputStream **_retval);
    NS_IMETHOD OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval);
    NS_IMETHOD AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                         nsISupports *ctxt,
                         nsIStreamListener *listener);
    NS_IMETHOD AsyncWrite(nsIInputStream *fromStream, 
                          PRUint32 startPosition, PRInt32 writeCount,
                          nsISupports *ctxt,
                          nsIStreamObserver *observer);

    NS_IMETHOD GetLoadAttributes(nsLoadFlags *aLoadAttributes);
    NS_IMETHOD SetLoadAttributes(nsLoadFlags aLoadAttributes);

    NS_IMETHOD GetContentType(char * *aContentType);


    // nsIRunnable methods:
    NS_IMETHOD Run(void);

    // nsIBufferObserver methods:
    NS_IMETHOD OnFull(nsIBuffer* buffer);
    NS_IMETHOD OnWrite(nsIBuffer* aBuffer, PRUint32 aCount);
    NS_IMETHOD OnEmpty(nsIBuffer* buffer);

    // nsFileTransport methods:
    nsFileTransport();
    virtual ~nsFileTransport();

    enum State {
        START_READ,
        READING,
        START_WRITE,
        WRITING,
        ENDING,
        ENDED
    };

    nsresult Init(const char* path,
                  nsFileTransportService* service);
    nsresult Init(nsISupports* context,
                  nsIStreamListener* listener,
                  State state, PRUint32 startPosition, PRInt32 count);
    void Process(void);

protected:
    char*                       mPath;
    nsISupports*                mContext;
    nsIStreamListener*          mListener;
    nsFileTransportService*     mService;
    State                       mState;
    PRBool                      mSuspended;

    // state variables:
    nsIBaseStream*              mFileStream;    // cast to nsIInputStream/nsIOutputStream for reading/writing
    nsIBuffer*                  mBuffer;
    nsIBufferInputStream*       mBufferStream;
    nsresult                    mStatus;
    PRUint32                    mSourceOffset;
    PRInt32                     mAmount;
    nsIEventQueue*              mEventQueue;

private:
    PRMonitor*                  mMonitor;
};

#define NS_FILE_TRANSPORT_BUFFER_SIZE   (4*1024)

#endif /* nsFileTransport_h___ */
