/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#ifndef nsHttpPipeline_h__
#define nsHttpPipeline_h__

#include "nsHttp.h"
#include "nsAHttpConnection.h"
#include "nsAHttpTransaction.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"

class nsHttpPipeline : public nsAHttpConnection
                     , public nsAHttpTransaction
{
public:
    NS_DECL_ISUPPORTS

    nsHttpPipeline();
    virtual ~nsHttpPipeline();

    nsresult Init(nsAHttpTransaction *);
    nsresult AppendTransaction(nsAHttpTransaction *);

    // nsAHttpConnection methods:
    nsresult OnHeadersAvailable(nsAHttpTransaction *, nsHttpRequestHead *, nsHttpResponseHead *, PRBool *reset);
    nsresult OnTransactionComplete(nsAHttpTransaction *, nsresult status);
    nsresult OnSuspend();
    nsresult OnResume();
    void GetConnectionInfo(nsHttpConnectionInfo **);
    void DropTransaction(nsAHttpTransaction *);
    PRBool IsPersistent();
    nsresult PushBack(const char *, PRUint32);
    
    // nsAHttpTransaction methods:
    void SetConnection(nsAHttpConnection *);
    void SetSecurityInfo(nsISupports *);
    void GetNotificationCallbacks(nsIInterfaceRequestor **);
    PRUint32 GetRequestSize();
    nsresult OnDataWritable(nsIOutputStream *);
    nsresult OnDataReadable(nsIInputStream *);
    nsresult OnStopTransaction(nsresult status);
    void OnStatus(nsresult status, const PRUnichar *statusText);
    PRBool   IsDone();
    nsresult Status();

private:
    //
    // simple nsIInputStream implementation that wraps the push-back data
    // and returns NS_BASE_STREAM_WOULD_BLOCK when empty.  unfortunately, none
    // of the string stream classes behave this way.
    //
    class nsInputStreamWrapper : public nsIInputStream
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIINPUTSTREAM

        nsInputStreamWrapper(const char *data, PRUint32 dataLen);
        virtual ~nsInputStreamWrapper();

    private:
        const char *mData;
        PRUint32    mDataLen;
        PRUint32    mDataPos;
    };

    //
    // helpers
    //
    PRBool   IsDone_Locked();
    PRInt8   LocateTransaction_Locked(nsAHttpTransaction *);
    void     DropTransaction_Locked(PRInt8);
    PRUint32 GetRequestSize_Locked();

private:
    enum {
        eTransactionReading  = PR_BIT(1),
        eTransactionComplete = PR_BIT(2)
    };
    nsAHttpConnection       *mConnection;                                       // hard ref
    nsAHttpTransaction      *mTransactionQ    [NS_HTTP_MAX_PIPELINED_REQUESTS]; // hard refs
    PRUint32                 mTransactionFlags[NS_HTTP_MAX_PIPELINED_REQUESTS];
    PRInt8                   mNumTrans;      // between 2 and NS_HTTP_MAX_PIPELINED_REQUESTS
    PRInt8                   mCurrentReader; // index of transaction currently reading
    PRLock                  *mLock;
    nsresult                 mStatus;
    nsCOMPtr<nsIInputStream> mRequestData;
};

#endif // nsHttpPipeline_h__
