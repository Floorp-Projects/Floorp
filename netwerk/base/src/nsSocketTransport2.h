/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "mozilla/net/DNS.h"

#include "prerror.h"

class nsSocketTransport;

nsresult
ErrorAccordingToNSPR(PRErrorCode errorCode);

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

    bool     IsReferenced() { return mReaderRefCnt > 0; }
    nsresult Condition()    { return mCondition; }
    uint64_t ByteCount()    { return mByteCount; }

    // called by the socket transport on the socket thread...
    void OnSocketReady(nsresult condition);

private:
    nsSocketTransport               *mTransport;
    nsrefcnt                         mReaderRefCnt;

    // access to these is protected by mTransport->mLock
    nsresult                         mCondition;
    nsCOMPtr<nsIInputStreamCallback> mCallback;
    uint32_t                         mCallbackFlags;
    uint64_t                         mByteCount;
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

    bool     IsReferenced() { return mWriterRefCnt > 0; }
    nsresult Condition()    { return mCondition; }
    uint64_t ByteCount()    { return mByteCount; }

    // called by the socket transport on the socket thread...
    void OnSocketReady(nsresult condition); 

private:
    static NS_METHOD WriteFromSegments(nsIInputStream *, void *,
                                       const char *, uint32_t offset,
                                       uint32_t count, uint32_t *countRead);

    nsSocketTransport                *mTransport;
    nsrefcnt                          mWriterRefCnt;

    // access to these is protected by mTransport->mLock
    nsresult                          mCondition;
    nsCOMPtr<nsIOutputStreamCallback> mCallback;
    uint32_t                          mCallbackFlags;
    uint64_t                          mByteCount;
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
    nsresult Init(const char **socketTypes, uint32_t typeCount,
                  const nsACString &host, uint16_t port,
                  nsIProxyInfo *proxyInfo);

    // this method instructs the socket transport to use an already connected
    // socket with the given address.
    nsresult InitWithConnectedSocket(PRFileDesc *socketFD,
                                     const mozilla::net::NetAddr *addr);

    // nsASocketHandler methods:
    void OnSocketReady(PRFileDesc *, int16_t outFlags); 
    void OnSocketDetached(PRFileDesc *);
    void IsLocal(bool *aIsLocal);

    // called when a socket event is handled
    void OnSocketEvent(uint32_t type, nsresult status, nsISupports *param);

    uint64_t ByteCountReceived() { return mInput.ByteCount(); }
    uint64_t ByteCountSent() { return mOutput.ByteCount(); }
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
    nsresult PostEvent(uint32_t type, nsresult status = NS_OK, nsISupports *param = nullptr);

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
    uint32_t     mTypeCount;
    nsCString    mHost;
    nsCString    mProxyHost;
    uint16_t     mPort;
    uint16_t     mProxyPort;
    bool mProxyTransparent;
    bool mProxyTransparentResolvesHost;
    uint32_t     mConnectionFlags;
    
    uint16_t         SocketPort() { return (!mProxyHost.IsEmpty() && !mProxyTransparent) ? mProxyPort : mPort; }
    const nsCString &SocketHost() { return (!mProxyHost.IsEmpty() && !mProxyTransparent) ? mProxyHost : mHost; }

    //-------------------------------------------------------------------------
    // members accessible only on the socket transport thread:
    //  (the exception being initialization/shutdown time)
    //-------------------------------------------------------------------------

    // socket state vars:
    uint32_t     mState;     // STATE_??? flags
    bool mAttached;
    bool mInputClosed;
    bool mOutputClosed;

    // this flag is used to determine if the results of a host lookup arrive
    // recursively or not.  this flag is not protected by any lock.
    bool mResolving;

    nsCOMPtr<nsICancelable> mDNSRequest;
    nsCOMPtr<nsIDNSRecord>  mDNSRecord;

    // mNetAddr is valid from GetPeerAddr() once we have
    // reached STATE_TRANSFERRING. It must not change after that.
    mozilla::net::NetAddr   mNetAddr;
    bool                    mNetAddrIsSet;

    // socket methods (these can only be called on the socket thread):

    void     SendStatus(nsresult status);
    nsresult ResolveHost();
    nsresult BuildSocket(PRFileDesc *&, bool &, bool &); 
    nsresult InitiateSocket();
    bool     RecoverFromError();

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
    bool        mFDconnected; // mFD is available to consumer when TRUE.

    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    nsCOMPtr<nsITransportEventSink> mEventSink;
    nsCOMPtr<nsISupports>           mSecInfo;

    nsSocketInputStream  mInput;
    nsSocketOutputStream mOutput;

    friend class nsSocketInputStream;
    friend class nsSocketOutputStream;

    // socket timeouts are not protected by any lock.
    uint16_t mTimeouts[2];

    // QoS setting for socket
    uint8_t mQoSBits;

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
    void TraceInBuf(const char *buf, int32_t n);
    void TraceOutBuf(const char *buf, int32_t n);
#endif
};

#endif // !nsSocketTransport_h__
