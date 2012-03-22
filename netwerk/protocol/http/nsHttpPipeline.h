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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
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

#ifndef nsHttpPipeline_h__
#define nsHttpPipeline_h__

#include "nsHttp.h"
#include "nsAHttpConnection.h"
#include "nsAHttpTransaction.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"

class nsHttpPipeline : public nsAHttpConnection
                     , public nsAHttpTransaction
                     , public nsAHttpSegmentReader
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSAHTTPCONNECTION
    NS_DECL_NSAHTTPTRANSACTION
    NS_DECL_NSAHTTPSEGMENTREADER

    nsHttpPipeline(PRUint16 maxPipelineDepth);
    virtual ~nsHttpPipeline();

private:
    nsresult FillSendBuf();
    
    static NS_METHOD ReadFromPipe(nsIInputStream *, void *, const char *,
                                  PRUint32, PRUint32, PRUint32 *);

    // convenience functions
    nsAHttpTransaction *Request(PRInt32 i)
    {
        if (mRequestQ.Length() == 0)
            return nsnull;

        return mRequestQ[i];
    }
    nsAHttpTransaction *Response(PRInt32 i)
    {
        if (mResponseQ.Length() == 0)
            return nsnull;

        return mResponseQ[i];
    }

    PRUint16                      mMaxPipelineDepth;
    nsAHttpConnection            *mConnection;
    nsTArray<nsAHttpTransaction*> mRequestQ;  // array of transactions
    nsTArray<nsAHttpTransaction*> mResponseQ; // array of transactions
    nsresult                      mStatus;

    // these flags indicate whether or not the first request or response
    // is partial.  a partial request means that Request(0) has been 
    // partially written out to the socket.  a partial response means
    // that Response(0) has been partially read in from the socket.
    bool mRequestIsPartial;
    bool mResponseIsPartial;

    // indicates whether or not the pipeline has been explicitly closed.
    bool mClosed;

    // indicates whether or not a true pipeline (more than 1 request without
    // a synchronous response) has been formed.
    bool mUtilizedPipeline;

    // used when calling ReadSegments/WriteSegments on a transaction.
    nsAHttpSegmentReader *mReader;
    nsAHttpSegmentWriter *mWriter;

    // send buffer
    nsCOMPtr<nsIInputStream>  mSendBufIn;
    nsCOMPtr<nsIOutputStream> mSendBufOut;

    // the push back buffer.  not exceeding nsIOService::gDefaultSegmentSize bytes.
    char     *mPushBackBuf;
    PRUint32  mPushBackLen;
    PRUint32  mPushBackMax;

    // The number of transactions completed on this pipeline.
    PRUint32  mHttp1xTransactionCount;

    // For support of OnTransportStatus()
    PRUint64  mReceivingFromProgress;
    PRUint64  mSendingToProgress;
    bool      mSuppressSendEvents;
};

#endif // nsHttpPipeline_h__
