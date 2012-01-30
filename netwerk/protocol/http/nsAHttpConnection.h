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

#ifndef nsAHttpConnection_h__
#define nsAHttpConnection_h__

#include "nsISupports.h"

class nsAHttpTransaction;
class nsHttpRequestHead;
class nsHttpResponseHead;
class nsHttpConnectionInfo;
class nsHttpConnection;
class nsISocketTransport;
class nsIAsyncInputStream;
class nsIAsyncOutputStream;

//-----------------------------------------------------------------------------
// Abstract base class for a HTTP connection
//-----------------------------------------------------------------------------

class nsAHttpConnection : public nsISupports
{
public:
    //-------------------------------------------------------------------------
    // NOTE: these methods may only be called on the socket thread.
    //-------------------------------------------------------------------------

    //
    // called by a transaction when the response headers have all been read.
    // the connection can force the transaction to reset it's response headers,
    // and prepare for a new set of response headers, by setting |*reset=TRUE|.
    //
    // @return failure code to close the transaction.
    //
    virtual nsresult OnHeadersAvailable(nsAHttpTransaction *,
                                        nsHttpRequestHead *,
                                        nsHttpResponseHead *,
                                        bool *reset) = 0;

    //
    // called by a transaction to resume either sending or receiving data
    // after a transaction returned NS_BASE_STREAM_WOULD_BLOCK from its
    // ReadSegments/WriteSegments methods.
    //
    virtual nsresult ResumeSend() = 0;
    virtual nsresult ResumeRecv() = 0;

    // After a connection has had ResumeSend() called by a transaction,
    // and it is ready to write to the network it may need to know the
    // transaction that has data to write. This is only an issue for
    // multiplexed protocols like SPDY - plain HTTP or pipelined HTTP
    // implicitly have this information in a 1:1 relationship with the
    // transaction(s) they manage.
    virtual void TransactionHasDataToWrite(nsAHttpTransaction *)
    {
        // by default do nothing - only multiplexed protocols need to overload
        return;
    }
    //
    // called by the connection manager to close a transaction being processed
    // by this connection.
    //
    // @param transaction
    //        the transaction being closed.
    // @param reason
    //        the reason for closing the transaction.  NS_BASE_STREAM_CLOSED
    //        is equivalent to NS_OK.
    //
    virtual void CloseTransaction(nsAHttpTransaction *transaction,
                                  nsresult reason) = 0;

    // get a reference to the connection's connection info object.
    virtual void GetConnectionInfo(nsHttpConnectionInfo **) = 0;

    // get the transport level information for this connection. This may fail
    // if it is in use.
    virtual nsresult TakeTransport(nsISocketTransport **,
                                   nsIAsyncInputStream **,
                                   nsIAsyncOutputStream **) = 0;

    // called by a transaction to get the security info from the socket.
    virtual void GetSecurityInfo(nsISupports **) = 0;

    // called by a transaction to determine whether or not the connection is
    // persistent... important in determining the end of a response.
    virtual bool IsPersistent() = 0;

    // called to determine if a connection has been reused.
    virtual bool IsReused() = 0;
    
    // called by a transaction when the transaction reads more from the socket
    // than it should have (eg. containing part of the next pipelined response).
    virtual nsresult PushBack(const char *data, PRUint32 length) = 0;

    // Used by a transaction to manage the state of previous response bodies on
    // the same connection and work around buggy servers.
    virtual bool LastTransactionExpectedNoContent() = 0;
    virtual void   SetLastTransactionExpectedNoContent(bool) = 0;

    // Transfer the base http connection object along with a
    // reference to it to the caller.
    virtual nsHttpConnection *TakeHttpConnection() = 0;

    // Get the nsISocketTransport used by the connection without changing
    //  references or ownership.
    virtual nsISocketTransport *Transport() = 0;
};

#define NS_DECL_NSAHTTPCONNECTION \
    nsresult OnHeadersAvailable(nsAHttpTransaction *, nsHttpRequestHead *, nsHttpResponseHead *, bool *reset); \
    nsresult ResumeSend(); \
    nsresult ResumeRecv(); \
    void CloseTransaction(nsAHttpTransaction *, nsresult); \
    void GetConnectionInfo(nsHttpConnectionInfo **); \
    nsresult TakeTransport(nsISocketTransport **,    \
                           nsIAsyncInputStream **,   \
                           nsIAsyncOutputStream **); \
    void GetSecurityInfo(nsISupports **); \
    bool IsPersistent(); \
    bool IsReused(); \
    nsresult PushBack(const char *, PRUint32); \
    bool LastTransactionExpectedNoContent(); \
    void   SetLastTransactionExpectedNoContent(bool); \
    nsHttpConnection *TakeHttpConnection(); \
    nsISocketTransport *Transport();

#endif // nsAHttpConnection_h__
