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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#ifndef nsAHttpTransaction_h__
#define nsAHttpTransaction_h__

#include "nsISupports.h"
#include "nsTArray.h"

class nsAHttpConnection;
class nsAHttpSegmentReader;
class nsAHttpSegmentWriter;
class nsIInterfaceRequestor;
class nsIEventTarget;
class nsITransport;
class nsHttpRequestHead;

//----------------------------------------------------------------------------
// Abstract base class for a HTTP transaction:
//
// A transaction is a "sink" for the response data.  The connection pushes
// data to the transaction by writing to it.  The transaction supports
// WriteSegments and may refuse to accept data if its buffers are full (its
// write function returns NS_BASE_STREAM_WOULD_BLOCK in this case).
//----------------------------------------------------------------------------

class nsAHttpTransaction : public nsISupports
{
public:
    // called by the connection when it takes ownership of the transaction.
    virtual void SetConnection(nsAHttpConnection *) = 0;
    virtual nsAHttpConnection *Connection() = 0;

    // called by the connection to get security callbacks to set on the
    // socket transport.
    virtual void GetSecurityCallbacks(nsIInterfaceRequestor **,
                                      nsIEventTarget **) = 0;

    // called to report socket status (see nsITransportEventSink)
    virtual void OnTransportStatus(nsITransport* transport,
                                   nsresult status, PRUint64 progress) = 0;

    // called to check the transaction status.
    virtual bool     IsDone() = 0;
    virtual nsresult Status() = 0;
    virtual PRUint8  Caps() = 0;

    // called to find out how much request data is available for writing.
    virtual PRUint32 Available() = 0;

    // called to read request data from the transaction.
    virtual nsresult ReadSegments(nsAHttpSegmentReader *reader,
                                  PRUint32 count, PRUint32 *countRead) = 0;

    // called to write response data to the transaction.
    virtual nsresult WriteSegments(nsAHttpSegmentWriter *writer,
                                   PRUint32 count, PRUint32 *countWritten) = 0;

    // called to close the transaction
    virtual void Close(nsresult reason) = 0;

    // called to indicate a failure at the SSL setup level
    virtual void SetSSLConnectFailed() = 0;
    
    // called to retrieve the request headers of the transaction
    virtual nsHttpRequestHead *RequestHead() = 0;

    // determine the number of real http/1.x transactions on this
    // abstract object. Pipelines may have multiple, SPDY has 0,
    // normal http transactions have 1.
    virtual PRUint32 Http1xTransactionCount() = 0;

    // called to remove the unused sub transactions from an object that can
    // handle multiple transactions simultaneously (i.e. pipelines or spdy).
    //
    // Returns NS_ERROR_NOT_IMPLEMENTED if the object does not implement
    // sub-transactions.
    //
    // Returns NS_ERROR_ALREADY_OPENED if the subtransactions have been
    // at least partially written and cannot be moved.
    //
    virtual nsresult TakeSubTransactions(
        nsTArray<nsRefPtr<nsAHttpTransaction> > &outTransactions) = 0;

    // called to add a sub-transaction in the case of pipelined transactions
    // classes that do not implement sub transactions
    // return NS_ERROR_NOT_IMPLEMENTED
    virtual nsresult AddTransaction(nsAHttpTransaction *transaction) = 0;
    
    // called to count the number of sub transactions that can be added
    virtual PRUint16 PipelineDepthAvailable() = 0;
};

#define NS_DECL_NSAHTTPTRANSACTION \
    void SetConnection(nsAHttpConnection *); \
    nsAHttpConnection *Connection(); \
    void GetSecurityCallbacks(nsIInterfaceRequestor **, \
                              nsIEventTarget **);       \
    void OnTransportStatus(nsITransport* transport, \
                           nsresult status, PRUint64 progress); \
    bool     IsDone(); \
    nsresult Status(); \
    PRUint8  Caps();   \
    PRUint32 Available(); \
    nsresult ReadSegments(nsAHttpSegmentReader *, PRUint32, PRUint32 *); \
    nsresult WriteSegments(nsAHttpSegmentWriter *, PRUint32, PRUint32 *); \
    void     Close(nsresult reason);                                    \
    void     SetSSLConnectFailed();                                     \
    nsHttpRequestHead *RequestHead();                                   \
    PRUint32 Http1xTransactionCount();                                  \
    nsresult TakeSubTransactions(nsTArray<nsRefPtr<nsAHttpTransaction> > &outTransactions); \
    nsresult AddTransaction(nsAHttpTransaction *);                      \
    PRUint16 PipelineDepthAvailable();

//-----------------------------------------------------------------------------
// nsAHttpSegmentReader
//-----------------------------------------------------------------------------

class nsAHttpSegmentReader
{
public:
    // any returned failure code stops segment iteration
    virtual nsresult OnReadSegment(const char *segment,
                                   PRUint32 count,
                                   PRUint32 *countRead) = 0;

    // Ask the segment reader to commit to accepting size bytes of
    // data from subsequent OnReadSegment() calls or throw hard
    // (i.e. not wouldblock) exceptions. Implementations
    // can return NS_ERROR_FAILURE if they never make commitments of that size
    // (the default), NS_BASE_STREAM_WOULD_BLOCK if they cannot make
    // the commitment now but might in the future, or NS_OK
    // if they make the commitment.
    //
    // Spdy uses this to make sure frames are atomic.
    virtual nsresult CommitToSegmentSize(PRUint32 size)
    {
        return NS_ERROR_FAILURE;
    }
};

#define NS_DECL_NSAHTTPSEGMENTREADER \
    nsresult OnReadSegment(const char *, PRUint32, PRUint32 *);

//-----------------------------------------------------------------------------
// nsAHttpSegmentWriter
//-----------------------------------------------------------------------------

class nsAHttpSegmentWriter
{
public:
    // any returned failure code stops segment iteration
    virtual nsresult OnWriteSegment(char *segment,
                                    PRUint32 count,
                                    PRUint32 *countWritten) = 0;
};

#define NS_DECL_NSAHTTPSEGMENTWRITER \
    nsresult OnWriteSegment(char *, PRUint32, PRUint32 *);

#endif // nsAHttpTransaction_h__
