/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSocketTransportService2_h__
#define nsSocketTransportService2_h__

#include "nsPISocketTransportService.h"
#include "nsIThreadInternal.h"
#include "nsIRunnable.h"
#include "nsEventQueue.h"
#include "nsCOMPtr.h"
#include "prinrval.h"
#include "prlog.h"
#include "prinit.h"
#include "nsIObserver.h"
#include "mozilla/Mutex.h"
#include "mozilla/net/DashboardTypes.h"

class nsASocketHandler;
struct PRPollDesc;

//-----------------------------------------------------------------------------

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=nsSocketTransport:5
//
extern PRLogModuleInfo *gSocketTransportLog;
#endif
#define SOCKET_LOG(args)     PR_LOG(gSocketTransportLog, PR_LOG_DEBUG, args)
#define SOCKET_LOG_ENABLED() PR_LOG_TEST(gSocketTransportLog, PR_LOG_DEBUG)

//-----------------------------------------------------------------------------

#define NS_SOCKET_POLL_TIMEOUT PR_INTERVAL_NO_TIMEOUT

//-----------------------------------------------------------------------------

namespace mozilla {
namespace net {
// These maximums are borrowed from the linux kernel.
static const int32_t kMaxTCPKeepIdle  = 32767; // ~9 hours.
static const int32_t kMaxTCPKeepIntvl = 32767;
static const int32_t kMaxTCPKeepCount   = 127;
static const int32_t kDefaultTCPKeepCount =
#if defined (XP_WIN)
                                              10; // Hardcoded in Windows.
#elif defined (XP_MACOSX)
                                              8;  // Hardcoded in OSX.
#else
                                              4;  // Specifiable in Linux.
#endif
}
}

//-----------------------------------------------------------------------------

class nsSocketTransportService MOZ_FINAL : public nsPISocketTransportService
                                         , public nsIEventTarget
                                         , public nsIThreadObserver
                                         , public nsIRunnable
                                         , public nsIObserver
{
    typedef mozilla::Mutex Mutex;

public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSPISOCKETTRANSPORTSERVICE
    NS_DECL_NSISOCKETTRANSPORTSERVICE
    NS_DECL_NSIEVENTTARGET
    NS_DECL_NSITHREADOBSERVER
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSIOBSERVER 

    nsSocketTransportService();

    // Max Socket count may need to get initialized/used by nsHttpHandler
    // before this class is initialized.
    static uint32_t gMaxCount;
    static PRCallOnceType gMaxCountInitOnce;
    static PRStatus DiscoverMaxCount();

    //
    // the number of sockets that can be attached at any given time is
    // limited.  this is done because some operating systems (e.g., Win9x)
    // limit the number of sockets that can be created by an application.
    // AttachSocket will fail if the limit is exceeded.  consumers should
    // call CanAttachSocket and check the result before creating a socket.
    //
    bool CanAttachSocket() {
        return mActiveCount + mIdleCount < gMaxCount;
    }

    // Called by the networking dashboard on the socket thread only
    // Fills the passed array with socket information
    void GetSocketConnections(nsTArray<mozilla::net::SocketInfo> *);
    uint64_t GetSentBytes() { return mSentBytesCount; }
    uint64_t GetReceivedBytes() { return mReceivedBytesCount; }

    // Returns true if keepalives are enabled in prefs.
    bool IsKeepaliveEnabled() { return mKeepaliveEnabledPref; }
protected:

    virtual ~nsSocketTransportService();

private:

    //-------------------------------------------------------------------------
    // misc (any thread)
    //-------------------------------------------------------------------------

    nsCOMPtr<nsIThread> mThread;    // protected by mLock
    PRFileDesc *mThreadEvent;
                            // protected by mLock.  mThreadEvent may change
                            // if the old pollable event is broken.  only
                            // the socket thread may change mThreadEvent;
                            // it needs to lock mLock only when it changes
                            // mThreadEvent.  other threads don't change
                            // mThreadEvent; they need to lock mLock
                            // whenever they access mThreadEvent.
    bool        mAutodialEnabled;
                            // pref to control autodial code

    // Returns mThread, protecting the get-and-addref with mLock
    already_AddRefed<nsIThread> GetThreadSafely();

    //-------------------------------------------------------------------------
    // initialization and shutdown (any thread)
    //-------------------------------------------------------------------------

    Mutex         mLock;
    bool          mInitialized;
    bool          mShuttingDown;
                            // indicates whether we are currently in the
                            // process of shutting down
    bool          mOffline;
    bool          mGoingOffline;

    // Detaches all sockets.
    void Reset(bool aGuardLocals);

    //-------------------------------------------------------------------------
    // socket lists (socket thread only)
    //
    // only "active" sockets are on the poll list.  the active list is kept
    // in sync with the poll list such that:
    //
    //   mActiveList[k].mFD == mPollList[k+1].fd
    //
    // where k=0,1,2,...
    //-------------------------------------------------------------------------

    struct SocketContext
    {
        PRFileDesc       *mFD;
        nsASocketHandler *mHandler;
        uint16_t          mElapsedTime;  // time elapsed w/o activity
    };

    SocketContext *mActiveList;                   /* mListSize entries */
    SocketContext *mIdleList;                     /* mListSize entries */

    uint32_t mActiveListSize;
    uint32_t mIdleListSize;
    uint32_t mActiveCount;
    uint32_t mIdleCount;

    nsresult DetachSocket(SocketContext *, SocketContext *);
    nsresult AddToIdleList(SocketContext *);
    nsresult AddToPollList(SocketContext *);
    void RemoveFromIdleList(SocketContext *);
    void RemoveFromPollList(SocketContext *);
    void MoveToIdleList(SocketContext *sock);
    void MoveToPollList(SocketContext *sock);

    bool GrowActiveList();
    bool GrowIdleList();
    void   InitMaxCount();

    // Total bytes number transfered through all the sockets except active ones
    uint64_t mSentBytesCount;
    uint64_t mReceivedBytesCount;
    //-------------------------------------------------------------------------
    // poll list (socket thread only)
    //
    // first element of the poll list is mThreadEvent (or null if the pollable
    // event cannot be created).
    //-------------------------------------------------------------------------

    PRPollDesc *mPollList;                        /* mListSize + 1 entries */

    PRIntervalTime PollTimeout();            // computes ideal poll timeout
    nsresult       DoPollIteration(bool wait);
                                             // perfoms a single poll iteration
    int32_t        Poll(bool wait, uint32_t *interval);
                                             // calls PR_Poll.  the out param
                                             // interval indicates the poll
                                             // duration in seconds.

    //-------------------------------------------------------------------------
    // pending socket queue - see NotifyWhenCanAttachSocket
    //-------------------------------------------------------------------------

    nsEventQueue mPendingSocketQ; // queue of nsIRunnable objects

    // Preference Monitor for SendBufferSize and Keepalive prefs.
    nsresult    UpdatePrefs();
    int32_t     mSendBufferSize;
    // Number of seconds of connection is idle before first keepalive ping.
    int32_t     mKeepaliveIdleTimeS;
    // Number of seconds between retries should keepalive pings fail.
    int32_t     mKeepaliveRetryIntervalS;
    // Number of keepalive probes to send.
    int32_t     mKeepaliveProbeCount;
    // True if TCP keepalive is enabled globally.
    bool        mKeepaliveEnabledPref;

    void OnKeepaliveEnabledPrefChange();
    void NotifyKeepaliveEnabledPrefChange(SocketContext *sock);

    // Socket thread only for dynamically adjusting max socket size
#if defined(XP_WIN)
    void ProbeMaxCount();
#endif
    bool mProbedMaxCount;

    void AnalyzeConnection(nsTArray<mozilla::net::SocketInfo> *data,
                           SocketContext *context, bool aActive);

    void ClosePrivateConnections();
    void DetachSocketWithGuard(bool aGuardLocals,
                               SocketContext *socketList,
                               int32_t index);
};

extern nsSocketTransportService *gSocketTransportService;
extern PRThread                 *gSocketThread;

#endif // !nsSocketTransportService_h__
