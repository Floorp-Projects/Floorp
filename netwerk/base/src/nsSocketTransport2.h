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

#ifdef DEBUG_darinf
#define ENABLE_SOCKET_TRACING
#endif

#include "mozilla/Mutex.h"
#include "nsSocketTransportService2.h"
#include "nsString.h"
#include "nsCOMPtr.h"

#include "nsISocketTransport.h"
#include "nsIInterfaceRequestor.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIDNSListener.h"
#include "nsIDNSRecord.h"
#include "nsICancelable.h"
#include "nsIClassInfo.h"

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
    PRUint64 ByteCount()    { return mByteCount; }

    // called by the socket transport on the socket thread...
    void OnSocketReady(nsresult condition);

private:
    nsSocketTransport               *mTransport;
    nsrefcnt                         mReaderRefCnt;

    // access to these is protected by mTransport->mLock
    nsresult                         mCondition;
    nsCOMPtr<nsIInputStreamCallback> mCallback;
    PRUint32                         mCallbackFlags;
    PRUint64                         mByteCount;
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
    PRUint64 ByteCount()    { return mByteCount; }

    // called by the socket transport on the socket thread...
    void OnSocketReady(nsresult condition); 

private:
    static NS_METHOD WriteFromSegments(nsIInputStream *, void *,
                                       const char *, PRUint32 offset,
                                       PRUint32 count, PRUint32 *countRead);

    nsSocketTransport                *mTransport;
    nsrefcnt                          mWriterRefCnt;

    // access to these is protected by mTransport->mLock
    nsresult                          mCondition;
    nsCOMPtr<nsIOutputStreamCallback> mCallback;
    PRUint32                          mCallbackFlags;
    PRUint64                          mByteCount;
};

//-----------------------------------------------------------------------------

class nsSocketTransport : public nsASocketHandler
                        , public nsISocketTransport
                        , public nsIDNSListener
                        , public nsIClassInfo
{
    typedef mozilla::Mutex Mutex;

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITRANSPORT
    NS_DECL_NSISOCKETTRANSPORT
    NS_DECL_NSIDNSLISTENER
    NS_DECL_NSICLASSINFO

    nsSocketTransport();

    // this method instructs the socket transport to open a socket of the
    // given type(s) to the given host or proxy.
    nsresult Init(const char **socketTypes, PRUint32 typeCount,
                  const nsACString &host, PRUint16 port,
                  nsIProxyInfo *proxyInfo);

    // this method instructs the socket transport to use an already connected
    // socket with the given address.
    nsresult InitWithConnectedSocket(PRFileDesc *socketFD,
                                     const PRNetAddr *addr);

    // nsASocketHandler methods:
    void OnSocketReady(PRFileDesc *, PRInt16 outFlags); 
    void OnSocketDetached(PRFileDesc *);

    // called when a socket event is handled
    void OnSocketEvent(PRUint32 type, nsresult status, nsISupports *param);

protected:

    virtual ~nsSocketTransport();

private:

    // event types
    enum {
        MSG_ENSURE_CONNECT,
        MSG_DNS_LOOKUP_COMPLETE,
        MSG_RETRY_INIT_SOCKET,
        MSG_TIMEOUT_CHANGED,
        MSG_INPUT_CLOSED,
        MSG_INPUT_PENDING,
        MSG_OUTPUT_CLOSED,
        MSG_OUTPUT_PENDING
    };
    nsresult PostEvent(PRUint32 type, nsresult status = NS_OK, nsISupports *param = nsnull);

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
    char       **mTypes;
    PRUint32     mTypeCount;
    nsCString    mHost;
    nsCString    mProxyHost;
    PRUint16     mPort;
    PRUint16     mProxyPort;
    PRPackedBool mProxyTransparent;
    PRPackedBool mProxyTransparentResolvesHost;
    PRUint32     mConnectionFlags;
    
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

    // this flag is used to determine if the results of a host lookup arrive
    // recursively or not.  this flag is not protected by any lock.
    PRPackedBool mResolving;

    nsCOMPtr<nsICancelable> mDNSRequest;
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

    Mutex       mLock;  // protects members in this section
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

    // socket timeouts are not protected by any lock.
    PRUint16 mTimeouts[2];

    // QoS setting for socket
    PRUint8 mQoSBits;

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
            PostEvent(MSG_INPUT_CLOSED, reason);
    }
    void OnInputPending()
    {
        // no need to post an event if called on the socket thread
        if (PR_GetCurrentThread() == gSocketThread)
            OnMsgInputPending();
        else
            PostEvent(MSG_INPUT_PENDING);
    }
    void OnOutputClosed(nsresult reason)
    {
        // no need to post an event if called on the socket thread
        if (PR_GetCurrentThread() == gSocketThread)
            OnMsgOutputClosed(reason); // XXX need to not be inside lock!
        else
            PostEvent(MSG_OUTPUT_CLOSED, reason);
    }
    void OnOutputPending()
    {
        // no need to post an event if called on the socket thread
        if (PR_GetCurrentThread() == gSocketThread)
            OnMsgOutputPending();
        else
            PostEvent(MSG_OUTPUT_PENDING);
    }

#ifdef ENABLE_SOCKET_TRACING
    void TraceInBuf(const char *buf, PRInt32 n);
    void TraceOutBuf(const char *buf, PRInt32 n);
#endif
};

#endif // !nsSocketTransport_h__
