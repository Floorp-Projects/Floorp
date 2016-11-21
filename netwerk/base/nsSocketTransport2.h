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

#include "nsIInterfaceRequestor.h"
#include "nsISocketTransport.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIDNSListener.h"
#include "nsIClassInfo.h"
#include "mozilla/net/DNS.h"
#include "nsASocketHandler.h"
#include "mozilla/Telemetry.h"

#include "prerror.h"
#include "nsAutoPtr.h"

class nsICancelable;
class nsIDNSRecord;
class nsIInterfaceRequestor;

//-----------------------------------------------------------------------------

// after this short interval, we will return to PR_Poll
#define NS_SOCKET_CONNECT_TIMEOUT PR_MillisecondsToInterval(20)

//-----------------------------------------------------------------------------

namespace mozilla {
namespace net {

nsresult
ErrorAccordingToNSPR(PRErrorCode errorCode);

class nsSocketTransport;

class nsSocketInputStream : public nsIAsyncInputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIASYNCINPUTSTREAM

    explicit nsSocketInputStream(nsSocketTransport *);
    virtual ~nsSocketInputStream();

    bool     IsReferenced() { return mReaderRefCnt > 0; }
    nsresult Condition()    { return mCondition; }
    uint64_t ByteCount()    { return mByteCount; }

    // called by the socket transport on the socket thread...
    void OnSocketReady(nsresult condition);

private:
    nsSocketTransport               *mTransport;
    ThreadSafeAutoRefCnt             mReaderRefCnt;

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

    explicit nsSocketOutputStream(nsSocketTransport *);
    virtual ~nsSocketOutputStream();

    bool     IsReferenced() { return mWriterRefCnt > 0; }
    nsresult Condition()    { return mCondition; }
    uint64_t ByteCount()    { return mByteCount; }

    // called by the socket transport on the socket thread...
    void OnSocketReady(nsresult condition); 

private:
    static nsresult WriteFromSegments(nsIInputStream *, void *,
                                      const char *, uint32_t offset,
                                      uint32_t count, uint32_t *countRead);

    nsSocketTransport                *mTransport;
    ThreadSafeAutoRefCnt              mWriterRefCnt;

    // access to these is protected by mTransport->mLock
    nsresult                          mCondition;
    nsCOMPtr<nsIOutputStreamCallback> mCallback;
    uint32_t                          mCallbackFlags;
    uint64_t                          mByteCount;
};

//-----------------------------------------------------------------------------

class nsSocketTransport final : public nsASocketHandler
                              , public nsISocketTransport
                              , public nsIDNSListener
                              , public nsIClassInfo
                              , public nsIInterfaceRequestor
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSITRANSPORT
    NS_DECL_NSISOCKETTRANSPORT
    NS_DECL_NSIDNSLISTENER
    NS_DECL_NSICLASSINFO
    NS_DECL_NSIINTERFACEREQUESTOR

    nsSocketTransport();

    // this method instructs the socket transport to open a socket of the
    // given type(s) to the given host or proxy.
    nsresult Init(const char **socketTypes, uint32_t typeCount,
                  const nsACString &host, uint16_t port,
                  const nsACString &hostRoute, uint16_t portRoute,
                  nsIProxyInfo *proxyInfo);

    // Alternative Init method for when the IP-address of the host
    // has been pre-resolved using a alternative means (e.g. FlyWeb service
    // info).
    nsresult InitPreResolved(const char **socketTypes, uint32_t typeCount,
                             const nsACString &host, uint16_t port,
                             const nsACString &hostRoute, uint16_t portRoute,
                             nsIProxyInfo *proxyInfo,
                             const mozilla::net::NetAddr* addr);

    // this method instructs the socket transport to use an already connected
    // socket with the given address.
    nsresult InitWithConnectedSocket(PRFileDesc *socketFD,
                                     const NetAddr *addr);

    // this method instructs the socket transport to use an already connected
    // socket with the given address, and additionally supplies security info.
    nsresult InitWithConnectedSocket(PRFileDesc* aSocketFD,
                                     const NetAddr* aAddr,
                                     nsISupports* aSecInfo);

    // This method instructs the socket transport to open a socket
    // connected to the given Unix domain address. We can only create
    // unlayered, simple, stream sockets.
    nsresult InitWithFilename(const char *filename);

    // nsASocketHandler methods:
    void OnSocketReady(PRFileDesc *, int16_t outFlags) override;
    void OnSocketDetached(PRFileDesc *) override;
    void IsLocal(bool *aIsLocal) override;
    void OnKeepaliveEnabledPrefChange(bool aEnabled) override final;

    // called when a socket event is handled
    void OnSocketEvent(uint32_t type, nsresult status, nsISupports *param);

    uint64_t ByteCountReceived() override { return mInput.ByteCount(); }
    uint64_t ByteCountSent() override { return mOutput.ByteCount(); }
    static void CloseSocket(PRFileDesc *aFd, bool aTelemetryEnabled);
    static void SendPRBlockingTelemetry(PRIntervalTime aStart,
        Telemetry::ID aIDNormal,
        Telemetry::ID aIDShutdown,
        Telemetry::ID aIDConnectivityChange,
        Telemetry::ID aIDLinkChange,
        Telemetry::ID aIDOffline);
protected:

    virtual ~nsSocketTransport();
    void     CleanupTypes();

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

    // Safer way to get and automatically release PRFileDesc objects.
    class MOZ_STACK_CLASS PRFileDescAutoLock
    {
    public:
      explicit PRFileDescAutoLock(nsSocketTransport *aSocketTransport,
                                  nsresult *aConditionWhileLocked = nullptr)
        : mSocketTransport(aSocketTransport)
        , mFd(nullptr)
      {
        MOZ_ASSERT(aSocketTransport);
        MutexAutoLock lock(mSocketTransport->mLock);
        if (aConditionWhileLocked) {
          *aConditionWhileLocked = mSocketTransport->mCondition;
          if (NS_FAILED(mSocketTransport->mCondition)) {
            return;
          }
        }
        mFd = mSocketTransport->GetFD_Locked();
      }
      ~PRFileDescAutoLock() {
        MutexAutoLock lock(mSocketTransport->mLock);
        if (mFd) {
          mSocketTransport->ReleaseFD_Locked(mFd);
        }
      }
      bool IsInitialized() {
        return mFd;
      }
      operator PRFileDesc*() {
        return mFd;
      }
      nsresult SetKeepaliveEnabled(bool aEnable);
      nsresult SetKeepaliveVals(bool aEnabled, int aIdleTime,
                                int aRetryInterval, int aProbeCount);
    private:
      operator PRFileDescAutoLock*() { return nullptr; }

      // Weak ptr to nsSocketTransport since this is a stack class only.
      nsSocketTransport *mSocketTransport;
      PRFileDesc        *mFd;
    };
    friend class PRFileDescAutoLock;

    class LockedPRFileDesc
    {
    public:
      explicit LockedPRFileDesc(nsSocketTransport *aSocketTransport)
        : mSocketTransport(aSocketTransport)
        , mFd(nullptr)
      {
        MOZ_ASSERT(aSocketTransport);
      }
      ~LockedPRFileDesc() {}
      bool IsInitialized() {
        return mFd;
      }
      LockedPRFileDesc& operator=(PRFileDesc *aFd) {
        mSocketTransport->mLock.AssertCurrentThreadOwns();
        mFd = aFd;
        return *this;
      }
      operator PRFileDesc*() {
        if (mSocketTransport->mAttached) {
          mSocketTransport->mLock.AssertCurrentThreadOwns();
        }
        return mFd;
      }
      bool operator==(PRFileDesc *aFd) {
        mSocketTransport->mLock.AssertCurrentThreadOwns();
        return mFd == aFd;
      }
    private:
      operator LockedPRFileDesc*() { return nullptr; }
      // Weak ptr to nsSocketTransport since it owns this class.
      nsSocketTransport *mSocketTransport;
      PRFileDesc        *mFd;
    };
    friend class LockedPRFileDesc;

    //-------------------------------------------------------------------------
    // these members are "set" at initialization time and are never modified
    // afterwards.  this allows them to be safely accessed from any thread.
    //-------------------------------------------------------------------------

    // socket type info:
    char       **mTypes;
    uint32_t     mTypeCount;
    nsCString    mHost;
    nsCString    mProxyHost;
    nsCString    mOriginHost;
    uint16_t     mPort;
    nsCOMPtr<nsIProxyInfo> mProxyInfo;
    uint16_t     mProxyPort;
    uint16_t     mOriginPort;
    bool mProxyTransparent;
    bool mProxyTransparentResolvesHost;
    bool mHttpsProxy;
    uint32_t     mConnectionFlags;

    // The origin attributes are used to create sockets.  The first party domain
    // will eventually be used to isolate OCSP cache and is only non-empty when
    // "privacy.firstparty.isolate" is enabled.  Setting this is the only way to
    // carry origin attributes down to NSPR layers which are final consumers.
    // It must be set before the socket transport is built.
    NeckoOriginAttributes mOriginAttributes;
    
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

    // The platform-specific network interface id that this socket
    // associated with.
    nsCString mNetworkInterfaceId;

    // this flag is used to determine if the results of a host lookup arrive
    // recursively or not.  this flag is not protected by any lock.
    bool mResolving;

    nsCOMPtr<nsICancelable> mDNSRequest;
    nsCOMPtr<nsIDNSRecord>  mDNSRecord;

    // mNetAddr/mSelfAddr is valid from GetPeerAddr()/GetSelfAddr() once we have
    // reached STATE_TRANSFERRING. It must not change after that.
    void                    SetSocketName(PRFileDesc *fd);
    NetAddr                 mNetAddr;
    NetAddr                 mSelfAddr; // getsockname()
    Atomic<bool, Relaxed>   mNetAddrIsSet;
    Atomic<bool, Relaxed>   mSelfAddrIsSet;
    Atomic<bool, Relaxed>   mNetAddrPreResolved;

    nsAutoPtr<NetAddr>      mBindAddr;

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

    Mutex            mLock;  // protects members in this section.
    LockedPRFileDesc mFD;
    nsrefcnt         mFDref;       // mFD is closed when mFDref goes to zero.
    bool             mFDconnected; // mFD is available to consumer when TRUE.

    // A delete protector reference to gSocketTransportService held for lifetime
    // of 'this'. Sometimes used interchangably with gSocketTransportService due
    // to scoping.
    RefPtr<nsSocketTransportService> mSocketTransportService;

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

    // Reads prefs to get default keepalive config.
    nsresult EnsureKeepaliveValsAreInitialized();

    // Groups calls to fd.SetKeepaliveEnabled and fd.SetKeepaliveVals.
    nsresult SetKeepaliveEnabledInternal(bool aEnable);

    // True if keepalive has been enabled by the socket owner. Note: Keepalive
    // must also be enabled globally for it to be enabled in TCP.
    bool mKeepaliveEnabled;

    // Keepalive config (support varies by platform).
    int32_t mKeepaliveIdleTimeS;
    int32_t mKeepaliveRetryIntervalS;
    int32_t mKeepaliveProbeCount;
};

} // namespace net
} // namespace mozilla

#endif // !nsSocketTransport_h__
