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

#ifndef nsSocketTransport2_h__
#define nsSocketTransport2_h__

#include "nsSocketTransportService2.h"
#include "nsString.h"
#include "nsCOMPtr.h"

#include "nsISocketTransport.h"
#include "nsIInterfaceRequestor.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIDNSService.h"

class nsSocketTransport;

//-----------------------------------------------------------------------------

// after this short interval, we will return to PR_Poll
#define NS_SOCKET_CONNECT_TIMEOUT PR_MillisecondsToInterval(20)

//-----------------------------------------------------------------------------

class nsSocketInputStream : public nsIAsyncInputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIASYNCINPUTSTREAM

    nsSocketInputStream(nsSocketTransport *);
    virtual ~nsSocketInputStream();

    PRBool   IsReferenced() { return mReaderRefCnt > 0; }
    nsresult Condition()    { return mCondition; }
    PRUint32 ByteCount()    { return mByteCount; }

    // called by the socket transport on the socket thread...
    void OnSocketReady(nsresult condition);

private:
    nsSocketTransport             *mTransport;
    nsrefcnt                       mReaderRefCnt;

    // access to these is protected by mTransport->mLock
    nsresult                       mCondition;
    nsCOMPtr<nsIInputStreamNotify> mNotify;
    PRUint32                       mByteCount;
};

//-----------------------------------------------------------------------------

class nsSocketOutputStream : public nsIAsyncOutputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOUTPUTSTREAM
    NS_DECL_NSIASYNCOUTPUTSTREAM

    nsSocketOutputStream(nsSocketTransport *);
    virtual ~nsSocketOutputStream();

    PRBool   IsReferenced() { return mWriterRefCnt > 0; }
    nsresult Condition()    { return mCondition; }
    PRUint32 ByteCount()    { return mByteCount; }

    // called by the socket transport on the socket thread...
    void OnSocketReady(nsresult condition); 

private:
    static NS_METHOD WriteFromSegments(nsIInputStream *, void *,
                                       const char *, PRUint32 offset,
                                       PRUint32 count, PRUint32 *countRead);

    nsSocketTransport              *mTransport;
    nsrefcnt                        mWriterRefCnt;

    // access to these is protected by mTransport->mLock
    nsresult                        mCondition;
    nsCOMPtr<nsIOutputStreamNotify> mNotify;
    PRUint32                        mByteCount;
};

//-----------------------------------------------------------------------------

class nsSocketTransport : public nsASocketHandler
                        , public nsISocketEventHandler
                        , public nsISocketTransport
                        , public nsIDNSListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISOCKETEVENTHANDLER
    NS_DECL_NSITRANSPORT
    NS_DECL_NSISOCKETTRANSPORT
    NS_DECL_NSIDNSLISTENER

    nsSocketTransport();

    nsresult Init(const char **socketTypes, PRUint32 typeCount,
                  const nsACString &host, PRUint16 port,
                  nsIProxyInfo *proxyInfo);

    // nsASocketHandler methods:
    void OnSocketReady(PRFileDesc *, PRInt16 outFlags); 
    void OnSocketDetached(PRFileDesc *);

private:

    virtual ~nsSocketTransport();

    enum {
        MSG_ENSURE_CONNECT,       // no args
        MSG_DNS_LOOKUP_COMPLETE,  // uparam holds "status"
        MSG_RETRY_INIT_SOCKET,    // no args
        MSG_INPUT_CLOSED,         // uparam holds "reason"
        MSG_INPUT_PENDING,        // no args
        MSG_OUTPUT_CLOSED,        // uparam holds "reason"
        MSG_OUTPUT_PENDING        // no args
    };

    enum {
        STATE_CLOSED,
        STATE_IDLE,
        STATE_RESOLVING,
        STATE_CONNECTING,
        STATE_TRANSFERRING
    };

    //-------------------------------------------------------------------------
    // these members are "set" at initialization time and are never modified
    // afterwards.  this allows them to be safely accessed from any thread.
    //-------------------------------------------------------------------------

    // socket type info:
    char      **mTypes;
    PRUint32    mTypeCount;
    nsCString   mHost;
    nsCString   mProxyHost;
    PRUint16    mPort;
    PRUint16    mProxyPort;
    PRBool      mProxyTransparent;

    PRUint16         SocketPort() { return (!mProxyHost.IsEmpty() && !mProxyTransparent) ? mProxyPort : mPort; }
    const nsCString &SocketHost() { return (!mProxyHost.IsEmpty() && !mProxyTransparent) ? mProxyHost : mHost; }

    //-------------------------------------------------------------------------
    // members accessible only on the socket transport thread:
    //  (the exception being initialization/shutdown time)
    //-------------------------------------------------------------------------

    // socket state vars:
    PRUint32     mState;     // STATE_??? flags
    PRPackedBool mAttached;
    PRPackedBool mInputClosed;
    PRPackedBool mOutputClosed;

    nsCOMPtr<nsIDNSRequest> mDNSRequest;
    nsCOMPtr<nsIDNSRecord>  mDNSRecord;
    PRNetAddr               mNetAddr;

    // socket methods (these can only be called on the socket thread):

    void     SendStatus(nsresult status);
    nsresult ResolveHost();
    nsresult BuildSocket(PRFileDesc *&, PRBool &, PRBool &); 
    nsresult InitiateSocket();
    PRBool   RecoverFromError();

    void OnMsgInputPending()
    {
        if (mState == STATE_TRANSFERRING)
            mPollFlags |= (PR_POLL_READ | PR_POLL_EXCEPT);
    }
    void OnMsgOutputPending()
    {
        if (mState == STATE_TRANSFERRING)
            mPollFlags |= (PR_POLL_WRITE | PR_POLL_EXCEPT);
    }
    void OnMsgInputClosed(nsresult reason);
    void OnMsgOutputClosed(nsresult reason);

    // called when the socket is connected
    void OnSocketConnected();

    //-------------------------------------------------------------------------
    // socket input/output objects.  these may be accessed on any thread with
    // the exception of some specific methods (XXX).

    PRLock     *mLock;  // protects members in this section
    PRFileDesc *mFD;
    nsrefcnt    mFDref;       // mFD is closed when mFDref goes to zero.
    PRBool      mFDconnected; // mFD is available to consumer when TRUE.

    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    nsCOMPtr<nsITransportEventSink> mEventSink;
    nsCOMPtr<nsISupports>           mSecInfo;

    nsSocketInputStream  mInput;
    nsSocketOutputStream mOutput;

    friend class nsSocketInputStream;
    friend class nsSocketOutputStream;

    //
    // mFD access methods: called with mLock held.
    //
    PRFileDesc *GetFD_Locked();
    void        ReleaseFD_Locked(PRFileDesc *fd);

    //
    // stream state changes (called outside mLock):
    //
    void OnInputClosed(nsresult reason)
    {
        // no need to post an event if called on the socket thread
        if (PR_GetCurrentThread() == gSocketThread)
            OnMsgInputClosed(reason);
        else
            gSocketTransportService->PostEvent(this, MSG_INPUT_CLOSED, reason, nsnull);
    }
    void OnInputPending()
    {
        // no need to post an event if called on the socket thread
        if (PR_GetCurrentThread() == gSocketThread)
            OnMsgInputPending();
        else
            gSocketTransportService->PostEvent(this, MSG_INPUT_PENDING, 0, nsnull);
    }
    void OnOutputClosed(nsresult reason)
    {
        // no need to post an event if called on the socket thread
        if (PR_GetCurrentThread() == gSocketThread)
            OnMsgOutputClosed(reason); // XXX need to not be inside lock!
        else
            gSocketTransportService->PostEvent(this, MSG_OUTPUT_CLOSED, reason, nsnull);
    }
    void OnOutputPending()
    {
        // no need to post an event if called on the socket thread
        if (PR_GetCurrentThread() == gSocketThread)
            OnMsgOutputPending();
        else
            gSocketTransportService->PostEvent(this, MSG_OUTPUT_PENDING, 0, nsnull);
    }
};

#endif // !nsSocketTransport_h__
