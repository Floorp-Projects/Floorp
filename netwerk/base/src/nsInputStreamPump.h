/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef nsInputStreamPump_h__
#define nsInputStreamPump_h__

#include "nsIInputStreamPump.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "nsIStreamListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIAsyncInputStream.h"
#include "nsIThread.h"
#include "nsCOMPtr.h"

class nsInputStreamPump : public nsIInputStreamPump
                        , public nsIInputStreamCallback
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSIINPUTSTREAMPUMP
    NS_DECL_NSIINPUTSTREAMCALLBACK

    nsInputStreamPump(); 
    ~nsInputStreamPump();

    static NS_HIDDEN_(nsresult)
                      Create(nsInputStreamPump  **result,
                             nsIInputStream      *stream,
                             PRInt64              streamPos = -1,
                             PRInt64              streamLen = -1,
                             PRUint32             segsize = 0,
                             PRUint32             segcount = 0,
                             bool                 closeWhenDone = false);

    typedef void (*PeekSegmentFun)(void *closure, const PRUint8 *buf,
                                   PRUint32 bufLen);
    /**
     * Peek into the first chunk of data that's in the stream. Note that this
     * method will not call the callback when there is no data in the stream.
     * The callback will be called at most once.
     *
     * The data from the stream will not be consumed, i.e. the pump's listener
     * can still read all the data.
     *
     * Do not call before asyncRead. Do not call after onStopRequest.
     */
    NS_HIDDEN_(nsresult) PeekStream(PeekSegmentFun callback, void *closure);

protected:

    enum {
        STATE_IDLE,
        STATE_START,
        STATE_TRANSFER,
        STATE_STOP
    };

    nsresult EnsureWaiting();
    PRUint32 OnStateStart();
    PRUint32 OnStateTransfer();
    PRUint32 OnStateStop();

    PRUint32                      mState;
    nsCOMPtr<nsILoadGroup>        mLoadGroup;
    nsCOMPtr<nsIStreamListener>   mListener;
    nsCOMPtr<nsISupports>         mListenerContext;
    nsCOMPtr<nsIThread>           mTargetThread;
    nsCOMPtr<nsIInputStream>      mStream;
    nsCOMPtr<nsIAsyncInputStream> mAsyncStream;
    PRUint64                      mStreamOffset;
    PRUint64                      mStreamLength;
    PRUint32                      mSegSize;
    PRUint32                      mSegCount;
    nsresult                      mStatus;
    PRUint32                      mSuspendCount;
    PRUint32                      mLoadFlags;
    bool                          mIsPending;
    bool                          mWaiting; // true if waiting on async source
    bool                          mCloseWhenDone;
};

#endif // !nsInputStreamChannel_h__
