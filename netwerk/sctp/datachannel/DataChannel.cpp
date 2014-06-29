/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#if !defined(__Userspace_os_Windows)
#include <arpa/inet.h>
#endif
// usrsctp.h expects to have errno definitions prior to its inclusion.
#include <errno.h>

#define SCTP_DEBUG 1
#define SCTP_STDINT_INCLUDE <stdint.h>

#ifdef _MSC_VER
// Disable "warning C4200: nonstandard extension used : zero-sized array in
//          struct/union"
// ...which the third-party file usrsctp.h runs afoul of.
#pragma warning(push)
#pragma warning(disable:4200)
#endif

#include "usrsctp.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "DataChannelLog.h"

#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "mozilla/Services.h"
#include "nsProxyRelease.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "nsNetUtil.h"
#include "mozilla/StaticPtr.h"
#ifdef MOZ_PEERCONNECTION
#include "mtransport/runnable_utils.h"
#endif

#define DATACHANNEL_LOG(args) LOG(args)
#include "DataChannel.h"
#include "DataChannelProtocol.h"

#ifdef PR_LOGGING
PRLogModuleInfo*
GetDataChannelLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog)
    sLog = PR_NewLogModule("DataChannel");
  return sLog;
}

PRLogModuleInfo*
GetSCTPLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog)
    sLog = PR_NewLogModule("SCTP");
  return sLog;
}
#endif

// Let us turn on and off important assertions in non-debug builds
#ifdef DEBUG
#define ASSERT_WEBRTC(x) MOZ_ASSERT((x))
#elif defined(MOZ_WEBRTC_ASSERT_ALWAYS)
#define ASSERT_WEBRTC(x) do { if (!(x)) { MOZ_CRASH(); } } while (0)
#endif

static bool sctp_initialized;

namespace mozilla {

class DataChannelShutdown;
StaticRefPtr<DataChannelShutdown> gDataChannelShutdown;

class DataChannelShutdown : public nsIObserver
{
public:
  // This needs to be tied to some form object that is guaranteed to be
  // around (singleton likely) unless we want to shutdown sctp whenever
  // we're not using it (and in which case we'd keep a refcnt'd object
  // ref'd by each DataChannelConnection to release the SCTP usrlib via
  // sctp_finish)

  NS_DECL_ISUPPORTS

  DataChannelShutdown() {}

  void Init()
    {
      nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
      if (!observerService)
        return;

      nsresult rv = observerService->AddObserver(this,
                                                 "profile-change-net-teardown",
                                                 false);
      MOZ_ASSERT(rv == NS_OK);
      (void) rv;
    }

private:
  virtual ~DataChannelShutdown()
    {
      nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
      if (observerService)
        observerService->RemoveObserver(this, "profile-change-net-teardown");
    }

public:
  NS_IMETHODIMP Observe(nsISupports* aSubject, const char* aTopic,
                        const char16_t* aData) {
    if (strcmp(aTopic, "profile-change-net-teardown") == 0) {
      LOG(("Shutting down SCTP"));
      if (sctp_initialized) {
        usrsctp_finish();
        sctp_initialized = false;
      }
      nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
      if (!observerService)
        return NS_ERROR_FAILURE;

      nsresult rv = observerService->RemoveObserver(this,
                                                    "profile-change-net-teardown");
      MOZ_ASSERT(rv == NS_OK);
      (void) rv;

      nsRefPtr<DataChannelShutdown> kungFuDeathGrip(this);
      gDataChannelShutdown = nullptr;
    }
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(DataChannelShutdown, nsIObserver);


BufferedMsg::BufferedMsg(struct sctp_sendv_spa &spa, const char *data,
                         uint32_t length) : mLength(length)
{
  mSpa = new sctp_sendv_spa;
  *mSpa = spa;
  char *tmp = new char[length]; // infallible malloc!
  memcpy(tmp, data, length);
  mData = tmp;
}

BufferedMsg::~BufferedMsg()
{
  delete mSpa;
  delete mData;
}

static int
receive_cb(struct socket* sock, union sctp_sockstore addr,
           void *data, size_t datalen,
           struct sctp_rcvinfo rcv, int flags, void *ulp_info)
{
  DataChannelConnection *connection = static_cast<DataChannelConnection*>(ulp_info);
  return connection->ReceiveCallback(sock, data, datalen, rcv, flags);
}

#ifdef PR_LOGGING
static void
debug_printf(const char *format, ...)
{
  va_list ap;
  char buffer[1024];

  if (PR_LOG_TEST(GetSCTPLog(), PR_LOG_ALWAYS)) {
    va_start(ap, format);
#ifdef _WIN32
    if (vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, ap) > 0) {
#else
    if (vsnprintf(buffer, sizeof(buffer), format, ap) > 0) {
#endif
      PR_LogPrint("%s", buffer);
    }
    va_end(ap);
  }
}
#endif

DataChannelConnection::DataChannelConnection(DataConnectionListener *listener) :
   mLock("netwerk::sctp::DataChannelConnection")
{
  mState = CLOSED;
  mSocket = nullptr;
  mMasterSocket = nullptr;
  mListener = listener->asWeakPtr();
  mLocalPort = 0;
  mRemotePort = 0;
  mDeferTimeout = 10;
  mTimerRunning = false;
  LOG(("Constructor DataChannelConnection=%p, listener=%p", this, mListener.get()));
  mInternalIOThread = nullptr;
}

DataChannelConnection::~DataChannelConnection()
{
  LOG(("Deleting DataChannelConnection %p", (void *) this));
  // This may die on the MainThread, or on the STS thread
  ASSERT_WEBRTC(mState == CLOSED);
  MOZ_ASSERT(!mMasterSocket);
  MOZ_ASSERT(mPending.GetSize() == 0);

  // Already disconnected from sigslot/mTransportFlow
  // TransportFlows must be released from the STS thread
  if (!IsSTSThread()) {
    ASSERT_WEBRTC(NS_IsMainThread());
    if (mTransportFlow) {
      ASSERT_WEBRTC(mSTS);
      NS_ProxyRelease(mSTS, mTransportFlow);
    }

    if (mInternalIOThread) {
      // Avoid spinning the event thread from here (which if we're mainthread
      // is in the event loop already)
      NS_DispatchToMainThread(WrapRunnable(nsCOMPtr<nsIThread>(mInternalIOThread),
                                           &nsIThread::Shutdown),
                              NS_DISPATCH_NORMAL);
    }
  } else {
    // on STS, safe to call shutdown
    if (mInternalIOThread) {
      mInternalIOThread->Shutdown();
    }
  }
}

void
DataChannelConnection::Destroy()
{
  // Though it's probably ok to do this and close the sockets;
  // if we really want it to do true clean shutdowns it can
  // create a dependant Internal object that would remain around
  // until the network shut down the association or timed out.
  LOG(("Destroying DataChannelConnection %p", (void *) this));
  ASSERT_WEBRTC(NS_IsMainThread());
  CloseAll();

  MutexAutoLock lock(mLock);
  // If we had a pending reset, we aren't waiting for it - clear the list so
  // we can deregister this DataChannelConnection without leaking.
  ClearResets();

  MOZ_ASSERT(mSTS);
  ASSERT_WEBRTC(NS_IsMainThread());
  // Finish Destroy on STS thread to avoid bug 876167 - once that's fixed,
  // the usrsctp_close() calls can move back here (and just proxy the
  // disconnect_all())
  RUN_ON_THREAD(mSTS, WrapRunnable(nsRefPtr<DataChannelConnection>(this),
                                   &DataChannelConnection::DestroyOnSTS,
                                   mSocket, mMasterSocket),
                NS_DISPATCH_NORMAL);

  // These will be released on STS
  mSocket = nullptr;
  mMasterSocket = nullptr; // also a flag that we've Destroyed this connection

  // Must do this in Destroy() since we may then delete this object
  if (mUsingDtls) {
    usrsctp_deregister_address(static_cast<void *>(this));
    LOG(("Deregistered %p from the SCTP stack.", static_cast<void *>(this)));
  }

  // We can't get any more new callbacks from the SCTP library
  // All existing callbacks have refs to DataChannelConnection

  // nsDOMDataChannel objects have refs to DataChannels that have refs to us
}

void DataChannelConnection::DestroyOnSTS(struct socket *aMasterSocket,
                                         struct socket *aSocket)
{
  if (aSocket && aSocket != aMasterSocket)
    usrsctp_close(aSocket);
  if (aMasterSocket)
    usrsctp_close(aMasterSocket);

  disconnect_all();
}

NS_IMPL_ISUPPORTS(DataChannelConnection,
                  nsITimerCallback)

bool
DataChannelConnection::Init(unsigned short aPort, uint16_t aNumStreams, bool aUsingDtls)
{
  struct sctp_initmsg initmsg;
  struct sctp_udpencaps encaps;
  struct sctp_assoc_value av;
  struct sctp_event event;
  socklen_t len;

  uint16_t event_types[] = {SCTP_ASSOC_CHANGE,
                            SCTP_PEER_ADDR_CHANGE,
                            SCTP_REMOTE_ERROR,
                            SCTP_SHUTDOWN_EVENT,
                            SCTP_ADAPTATION_INDICATION,
                            SCTP_SEND_FAILED_EVENT,
                            SCTP_STREAM_RESET_EVENT,
                            SCTP_STREAM_CHANGE_EVENT};
  {
    ASSERT_WEBRTC(NS_IsMainThread());

    // MutexAutoLock lock(mLock); Not needed since we're on mainthread always
    if (!sctp_initialized) {
      if (aUsingDtls) {
        LOG(("sctp_init(DTLS)"));
#ifdef MOZ_PEERCONNECTION
        usrsctp_init(0,
                     DataChannelConnection::SctpDtlsOutput,
#ifdef PR_LOGGING
                     debug_printf
#else
                     nullptr
#endif
                    );
#else
        NS_ASSERTION(!aUsingDtls, "Trying to use SCTP/DTLS without mtransport");
#endif
      } else {
        LOG(("sctp_init(%u)", aPort));
        usrsctp_init(aPort,
                     nullptr,
#ifdef PR_LOGGING
                     debug_printf
#else
                     nullptr
#endif
                    );
      }

#ifdef PR_LOGGING
      // Set logging to SCTP:PR_LOG_DEBUG to get SCTP debugs
      if (PR_LOG_TEST(GetSCTPLog(), PR_LOG_ALWAYS)) {
        usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);
      }
#endif
      usrsctp_sysctl_set_sctp_blackhole(2);
      // ECN is currently not supported by the Firefox code
      usrsctp_sysctl_set_sctp_ecn_enable(0);
      sctp_initialized = true;

      gDataChannelShutdown = new DataChannelShutdown();
      gDataChannelShutdown->Init();
    }
  }

  // XXX FIX! make this a global we get once
  // Find the STS thread
  nsresult rv;
  mSTS = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Open sctp with a callback
  if ((mMasterSocket = usrsctp_socket(
         aUsingDtls ? AF_CONN : AF_INET,
         SOCK_STREAM, IPPROTO_SCTP, receive_cb, nullptr, 0, this)) == nullptr) {
    return false;
  }

  // Make non-blocking for bind/connect.  SCTP over UDP defaults to non-blocking
  // in associations for normal IO
  if (usrsctp_set_non_blocking(mMasterSocket, 1) < 0) {
    LOG(("Couldn't set non_blocking on SCTP socket"));
    // We can't handle connect() safely if it will block, not that this will
    // even happen.
    goto error_cleanup;
  }

  // Make sure when we close the socket, make sure it doesn't call us back again!
  // This would cause it try to use an invalid DataChannelConnection pointer
  struct linger l;
  l.l_onoff = 1;
  l.l_linger = 0;
  if (usrsctp_setsockopt(mMasterSocket, SOL_SOCKET, SO_LINGER,
                         (const void *)&l, (socklen_t)sizeof(struct linger)) < 0) {
    LOG(("Couldn't set SO_LINGER on SCTP socket"));
    // unsafe to allow it to continue if this fails
    goto error_cleanup;
  }

  // XXX Consider disabling this when we add proper SDP negotiation.
  // We may want to leave enabled for supporting 'cloning' of SDP offers, which
  // implies re-use of the same pseudo-port number, or forcing a renegotiation.
  {
    uint32_t on = 1;
    if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_REUSE_PORT,
                           (const void *)&on, (socklen_t)sizeof(on)) < 0) {
      LOG(("Couldn't set SCTP_REUSE_PORT on SCTP socket"));
    }
    if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_NODELAY,
                           (const void *)&on, (socklen_t)sizeof(on)) < 0) {
      LOG(("Couldn't set SCTP_NODELAY on SCTP socket"));
    }
  }

  if (!aUsingDtls) {
    memset(&encaps, 0, sizeof(encaps));
    encaps.sue_address.ss_family = AF_INET;
    encaps.sue_port = htons(aPort);
    if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_REMOTE_UDP_ENCAPS_PORT,
                           (const void*)&encaps,
                           (socklen_t)sizeof(struct sctp_udpencaps)) < 0) {
      LOG(("*** failed encaps errno %d", errno));
      goto error_cleanup;
    }
    LOG(("SCTP encapsulation local port %d", aPort));
  }

  av.assoc_id = SCTP_ALL_ASSOC;
  av.assoc_value = SCTP_ENABLE_RESET_STREAM_REQ | SCTP_ENABLE_CHANGE_ASSOC_REQ;
  if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET, &av,
                         (socklen_t)sizeof(struct sctp_assoc_value)) < 0) {
    LOG(("*** failed enable stream reset errno %d", errno));
    goto error_cleanup;
  }

  /* Enable the events of interest. */
  memset(&event, 0, sizeof(event));
  event.se_assoc_id = SCTP_ALL_ASSOC;
  event.se_on = 1;
  for (uint32_t i = 0; i < sizeof(event_types)/sizeof(event_types[0]); ++i) {
    event.se_type = event_types[i];
    if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(event)) < 0) {
      LOG(("*** failed setsockopt SCTP_EVENT errno %d", errno));
      goto error_cleanup;
    }
  }

  // Update number of streams
  mStreams.AppendElements(aNumStreams);
  for (uint32_t i = 0; i < aNumStreams; ++i) {
    mStreams[i] = nullptr;
  }
  memset(&initmsg, 0, sizeof(initmsg));
  len = sizeof(initmsg);
  if (usrsctp_getsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, &len) < 0) {
    LOG(("*** failed getsockopt SCTP_INITMSG"));
    goto error_cleanup;
  }
  LOG(("Setting number of SCTP streams to %u, was %u/%u", aNumStreams,
       initmsg.sinit_num_ostreams, initmsg.sinit_max_instreams));
  initmsg.sinit_num_ostreams  = aNumStreams;
  initmsg.sinit_max_instreams = MAX_NUM_STREAMS;
  if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg,
                         (socklen_t)sizeof(initmsg)) < 0) {
    LOG(("*** failed setsockopt SCTP_INITMSG, errno %d", errno));
    goto error_cleanup;
  }

  mSocket = nullptr;
  if (aUsingDtls) {
    mUsingDtls = true;
    usrsctp_register_address(static_cast<void *>(this));
    LOG(("Registered %p within the SCTP stack.", static_cast<void *>(this)));
  } else {
    mUsingDtls = false;
  }
  return true;

error_cleanup:
  usrsctp_close(mMasterSocket);
  mMasterSocket = nullptr;
  mUsingDtls = false;
  return false;
}

void
DataChannelConnection::StartDefer()
{
  nsresult rv;
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                            DataChannelOnMessageAvailable::START_DEFER,
                            this, (DataChannel *) nullptr));
    return;
  }

  ASSERT_WEBRTC(NS_IsMainThread());
  if (!mDeferredTimer) {
    mDeferredTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    MOZ_ASSERT(mDeferredTimer);
  }

  if (!mTimerRunning) {
    rv = mDeferredTimer->InitWithCallback(this, mDeferTimeout,
                                          nsITimer::TYPE_ONE_SHOT);
    NS_ENSURE_TRUE_VOID(rv == NS_OK);

    mTimerRunning = true;
  }
}

// nsITimerCallback

NS_IMETHODIMP
DataChannelConnection::Notify(nsITimer *timer)
{
  ASSERT_WEBRTC(NS_IsMainThread());
  LOG(("%s: %p [%p] (%dms), sending deferred messages", __FUNCTION__, this, timer, mDeferTimeout));

  if (timer == mDeferredTimer) {
    if (SendDeferredMessages()) {
      // Still blocked
      // we don't need a lock, since this must be main thread...
      nsresult rv = mDeferredTimer->InitWithCallback(this, mDeferTimeout,
                                                     nsITimer::TYPE_ONE_SHOT);
      if (NS_FAILED(rv)) {
        LOG(("%s: cannot initialize open timer", __FUNCTION__));
        // XXX and do....?
        return rv;
      }
      mTimerRunning = true;
    } else {
      LOG(("Turned off deferred send timer"));
      mTimerRunning = false;
    }
  }
  return NS_OK;
}

#ifdef MOZ_PEERCONNECTION
void
DataChannelConnection::SetEvenOdd()
{
  ASSERT_WEBRTC(IsSTSThread());

  TransportLayerDtls *dtls = static_cast<TransportLayerDtls *>(
      mTransportFlow->GetLayer(TransportLayerDtls::ID()));
  MOZ_ASSERT(dtls);  // DTLS is mandatory
  mAllocateEven = (dtls->role() == TransportLayerDtls::CLIENT);
}

bool
DataChannelConnection::ConnectViaTransportFlow(TransportFlow *aFlow, uint16_t localport, uint16_t remoteport)
{
  LOG(("Connect DTLS local %u, remote %u", localport, remoteport));

  NS_PRECONDITION(mMasterSocket, "SCTP wasn't initialized before ConnectViaTransportFlow!");
  NS_ENSURE_TRUE(aFlow, false);

  mTransportFlow = aFlow;
  mLocalPort = localport;
  mRemotePort = remoteport;
  mState = CONNECTING;

  RUN_ON_THREAD(mSTS, WrapRunnable(nsRefPtr<DataChannelConnection>(this),
                                   &DataChannelConnection::SetSignals),
                NS_DISPATCH_NORMAL);
  return true;
}

void
DataChannelConnection::SetSignals()
{
  ASSERT_WEBRTC(IsSTSThread());
  ASSERT_WEBRTC(mTransportFlow);
  LOG(("Setting transport signals, state: %d", mTransportFlow->state()));
  mTransportFlow->SignalPacketReceived.connect(this, &DataChannelConnection::SctpDtlsInput);
  // SignalStateChange() doesn't call you with the initial state
  mTransportFlow->SignalStateChange.connect(this, &DataChannelConnection::CompleteConnect);
  CompleteConnect(mTransportFlow, mTransportFlow->state());
}

void
DataChannelConnection::CompleteConnect(TransportFlow *flow, TransportLayer::State state)
{
  LOG(("Data transport state: %d", state));
  MutexAutoLock lock(mLock);
  ASSERT_WEBRTC(IsSTSThread());
  // We should abort connection on TS_ERROR.
  // Note however that the association will also fail (perhaps with a delay) and
  // notify us in that way
  if (state != TransportLayer::TS_OPEN || !mMasterSocket)
    return;

  struct sockaddr_conn addr;
  memset(&addr, 0, sizeof(addr));
  addr.sconn_family = AF_CONN;
#if defined(__Userspace_os_Darwin)
  addr.sconn_len = sizeof(addr);
#endif
  addr.sconn_port = htons(mLocalPort);
  addr.sconn_addr = static_cast<void *>(this);

  LOG(("Calling usrsctp_bind"));
  int r = usrsctp_bind(mMasterSocket, reinterpret_cast<struct sockaddr *>(&addr),
                       sizeof(addr));
  if (r < 0) {
    LOG(("usrsctp_bind failed: %d", r));
  } else {
    // This is the remote addr
    addr.sconn_port = htons(mRemotePort);
    LOG(("Calling usrsctp_connect"));
    r = usrsctp_connect(mMasterSocket, reinterpret_cast<struct sockaddr *>(&addr),
                        sizeof(addr));
    if (r < 0) {
      if (errno == EINPROGRESS) {
        // non-blocking
        return;
      } else {
        LOG(("usrsctp_connect failed: %d", errno));
        mState = CLOSED;
      }
    } else {
      // We set Even/Odd and fire ON_CONNECTION via SCTP_COMM_UP when we get that
      // This also avoids issues with calling TransportFlow stuff on Mainthread
      return;
    }
  }
  // Note: currently this doesn't actually notify the application
  NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                            DataChannelOnMessageAvailable::ON_CONNECTION,
                            this));
  return;
}

// Process any pending Opens
void
DataChannelConnection::ProcessQueuedOpens()
{
  // The nsDeque holds channels with an AddRef applied.  Another reference
  // (may) be held by the DOMDataChannel, unless it's been GC'd.  No other
  // references should exist.

  // Can't copy nsDeque's.  Move into temp array since any that fail will
  // go back to mPending
  nsDeque temp;
  DataChannel *temp_channel; // really already_AddRefed<>
  while (nullptr != (temp_channel = static_cast<DataChannel *>(mPending.PopFront()))) {
    temp.Push(static_cast<void *>(temp_channel));
  }

  nsRefPtr<DataChannel> channel;
  // All these entries have an AddRef(); make that explicit now via the dont_AddRef()
  while (nullptr != (channel = dont_AddRef(static_cast<DataChannel *>(temp.PopFront())))) {
    if (channel->mFlags & DATA_CHANNEL_FLAGS_FINISH_OPEN) {
      LOG(("Processing queued open for %p (%u)", channel.get(), channel->mStream));
      channel->mFlags &= ~DATA_CHANNEL_FLAGS_FINISH_OPEN;
      // OpenFinish returns a reference itself, so we need to take it can Release it
      channel = OpenFinish(channel.forget()); // may reset the flag and re-push
    } else {
      NS_ASSERTION(false, "How did a DataChannel get queued without the FINISH_OPEN flag?");
    }
  }

}
void
DataChannelConnection::SctpDtlsInput(TransportFlow *flow,
                                     const unsigned char *data, size_t len)
{
#ifdef PR_LOGGING
  if (PR_LOG_TEST(GetSCTPLog(), PR_LOG_DEBUG)) {
    char *buf;

    if ((buf = usrsctp_dumppacket((void *)data, len, SCTP_DUMP_INBOUND)) != nullptr) {
      PR_LogPrint("%s", buf);
      usrsctp_freedumpbuffer(buf);
    }
  }
#endif
  // Pass the data to SCTP
  usrsctp_conninput(static_cast<void *>(this), data, len, 0);
}

int
DataChannelConnection::SendPacket(unsigned char data[], size_t len, bool release)
{
  //LOG(("%p: SCTP/DTLS sent %ld bytes", this, len));
  int res = mTransportFlow->SendPacket(data, len) < 0 ? 1 : 0;
  if (release)
    delete [] data;
  return res;
}

/* static */
int
DataChannelConnection::SctpDtlsOutput(void *addr, void *buffer, size_t length,
                                      uint8_t tos, uint8_t set_df)
{
  DataChannelConnection *peer = static_cast<DataChannelConnection *>(addr);
  int res;

#ifdef PR_LOGGING
  if (PR_LOG_TEST(GetSCTPLog(), PR_LOG_DEBUG)) {
    char *buf;

    if ((buf = usrsctp_dumppacket(buffer, length, SCTP_DUMP_OUTBOUND)) != nullptr) {
      PR_LogPrint("%s", buf);
      usrsctp_freedumpbuffer(buf);
    }
  }
#endif
  // We're async proxying even if on the STSThread because this is called
  // with internal SCTP locks held in some cases (such as in usrsctp_connect()).
  // SCTP has an option for Apple, on IP connections only, to release at least
  // one of the locks before calling a packet output routine; with changes to
  // the underlying SCTP stack this might remove the need to use an async proxy.
  if (0 /*peer->IsSTSThread()*/) {
    res = peer->SendPacket(static_cast<unsigned char *>(buffer), length, false);
  } else {
    unsigned char *data = new unsigned char[length];
    memcpy(data, buffer, length);
    // Commented out since we have to Dispatch SendPacket to avoid deadlock"
    // res = -1;

    // XXX It might be worthwhile to add an assertion against the thread
    // somehow getting into the DataChannel/SCTP code again, as
    // DISPATCH_SYNC is not fully blocking.  This may be tricky, as it
    // needs to be a per-thread check, not a global.
    peer->mSTS->Dispatch(WrapRunnable(
                           nsRefPtr<DataChannelConnection>(peer),
                           &DataChannelConnection::SendPacket, data, length, true),
                         NS_DISPATCH_NORMAL);
    res = 0; // cheat!  Packets can always be dropped later anyways
  }
  return res;
}
#endif

#ifdef ALLOW_DIRECT_SCTP_LISTEN_CONNECT
// listen for incoming associations
// Blocks! - Don't call this from main thread!

#error This code will not work as-is since SetEvenOdd() runs on Mainthread

bool
DataChannelConnection::Listen(unsigned short port)
{
  struct sockaddr_in addr;
  socklen_t addr_len;

  NS_WARN_IF_FALSE(!NS_IsMainThread(), "Blocks, do not call from main thread!!!");

  /* Acting as the 'server' */
  memset((void *)&addr, 0, sizeof(addr));
#ifdef HAVE_SIN_LEN
  addr.sin_len = sizeof(struct sockaddr_in);
#endif
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  LOG(("Waiting for connections on port %u", ntohs(addr.sin_port)));
  mState = CONNECTING;
  if (usrsctp_bind(mMasterSocket, reinterpret_cast<struct sockaddr *>(&addr), sizeof(struct sockaddr_in)) < 0) {
    LOG(("***Failed userspace_bind"));
    return false;
  }
  if (usrsctp_listen(mMasterSocket, 1) < 0) {
    LOG(("***Failed userspace_listen"));
    return false;
  }

  LOG(("Accepting connection"));
  addr_len = 0;
  if ((mSocket = usrsctp_accept(mMasterSocket, nullptr, &addr_len)) == nullptr) {
    LOG(("***Failed accept"));
    return false;
  }
  mState = OPEN;

  struct linger l;
  l.l_onoff = 1;
  l.l_linger = 0;
  if (usrsctp_setsockopt(mSocket, SOL_SOCKET, SO_LINGER,
                         (const void *)&l, (socklen_t)sizeof(struct linger)) < 0) {
    LOG(("Couldn't set SO_LINGER on SCTP socket"));
  }

  SetEvenOdd();

  // Notify Connection open
  // XXX We need to make sure connection sticks around until the message is delivered
  LOG(("%s: sending ON_CONNECTION for %p", __FUNCTION__, this));
  NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                            DataChannelOnMessageAvailable::ON_CONNECTION,
                            this, (DataChannel *) nullptr));
  return true;
}

// Blocks! - Don't call this from main thread!
bool
DataChannelConnection::Connect(const char *addr, unsigned short port)
{
  struct sockaddr_in addr4;
  struct sockaddr_in6 addr6;

  NS_WARN_IF_FALSE(!NS_IsMainThread(), "Blocks, do not call from main thread!!!");

  /* Acting as the connector */
  LOG(("Connecting to %s, port %u", addr, port));
  memset((void *)&addr4, 0, sizeof(struct sockaddr_in));
  memset((void *)&addr6, 0, sizeof(struct sockaddr_in6));
#ifdef HAVE_SIN_LEN
  addr4.sin_len = sizeof(struct sockaddr_in);
#endif
#ifdef HAVE_SIN6_LEN
  addr6.sin6_len = sizeof(struct sockaddr_in6);
#endif
  addr4.sin_family = AF_INET;
  addr6.sin6_family = AF_INET6;
  addr4.sin_port = htons(port);
  addr6.sin6_port = htons(port);
  mState = CONNECTING;

#if !defined(__Userspace_os_Windows)
  if (inet_pton(AF_INET6, addr, &addr6.sin6_addr) == 1) {
    if (usrsctp_connect(mMasterSocket, reinterpret_cast<struct sockaddr *>(&addr6), sizeof(struct sockaddr_in6)) < 0) {
      LOG(("*** Failed userspace_connect"));
      return false;
    }
  } else if (inet_pton(AF_INET, addr, &addr4.sin_addr) == 1) {
    if (usrsctp_connect(mMasterSocket, reinterpret_cast<struct sockaddr *>(&addr4), sizeof(struct sockaddr_in)) < 0) {
      LOG(("*** Failed userspace_connect"));
      return false;
    }
  } else {
    LOG(("*** Illegal destination address."));
  }
#else
  {
    struct sockaddr_storage ss;
    int sslen = sizeof(ss);

    if (!WSAStringToAddressA(const_cast<char *>(addr), AF_INET6, nullptr, (struct sockaddr*)&ss, &sslen)) {
      addr6.sin6_addr = (reinterpret_cast<struct sockaddr_in6 *>(&ss))->sin6_addr;
      if (usrsctp_connect(mMasterSocket, reinterpret_cast<struct sockaddr *>(&addr6), sizeof(struct sockaddr_in6)) < 0) {
        LOG(("*** Failed userspace_connect"));
        return false;
      }
    } else if (!WSAStringToAddressA(const_cast<char *>(addr), AF_INET, nullptr, (struct sockaddr*)&ss, &sslen)) {
      addr4.sin_addr = (reinterpret_cast<struct sockaddr_in *>(&ss))->sin_addr;
      if (usrsctp_connect(mMasterSocket, reinterpret_cast<struct sockaddr *>(&addr4), sizeof(struct sockaddr_in)) < 0) {
        LOG(("*** Failed userspace_connect"));
        return false;
      }
    } else {
      LOG(("*** Illegal destination address."));
    }
  }
#endif

  mSocket = mMasterSocket;

  LOG(("connect() succeeded!  Entering connected mode"));
  mState = OPEN;

  SetEvenOdd();

  // Notify Connection open
  // XXX We need to make sure connection sticks around until the message is delivered
  LOG(("%s: sending ON_CONNECTION for %p", __FUNCTION__, this));
  NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                            DataChannelOnMessageAvailable::ON_CONNECTION,
                            this, (DataChannel *) nullptr));
  return true;
}
#endif

DataChannel *
DataChannelConnection::FindChannelByStream(uint16_t stream)
{
  return mStreams.SafeElementAt(stream);
}

uint16_t
DataChannelConnection::FindFreeStream()
{
  uint32_t i, j, limit;

  limit = mStreams.Length();
  if (limit > MAX_NUM_STREAMS)
    limit = MAX_NUM_STREAMS;

  for (i = (mAllocateEven ? 0 : 1); i < limit; i += 2) {
    if (!mStreams[i]) {
      // Verify it's not still in the process of closing
      for (j = 0; j < mStreamsResetting.Length(); ++j) {
        if (mStreamsResetting[j] == i) {
          break;
        }
      }
      if (j == mStreamsResetting.Length())
        break;
    }
  }
  if (i >= limit) {
    return INVALID_STREAM;
  }
  return i;
}

bool
DataChannelConnection::RequestMoreStreams(int32_t aNeeded)
{
  struct sctp_status status;
  struct sctp_add_streams sas;
  uint32_t outStreamsNeeded;
  socklen_t len;

  if (aNeeded + mStreams.Length() > MAX_NUM_STREAMS)
    aNeeded = MAX_NUM_STREAMS - mStreams.Length();
  if (aNeeded <= 0)
    return false;

  len = (socklen_t)sizeof(struct sctp_status);
  if (usrsctp_getsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_STATUS, &status, &len) < 0) {
    LOG(("***failed: getsockopt SCTP_STATUS"));
    return false;
  }
  outStreamsNeeded = aNeeded; // number to add

  memset(&sas, 0, sizeof(struct sctp_add_streams));
  sas.sas_instrms = 0;
  sas.sas_outstrms = (uint16_t)outStreamsNeeded; /* XXX error handling */
  // Doesn't block, we get an event when it succeeds or fails
  if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_ADD_STREAMS, &sas,
                         (socklen_t) sizeof(struct sctp_add_streams)) < 0) {
    if (errno == EALREADY)
      return true;

    LOG(("***failed: setsockopt ADD errno=%d", errno));
    return false;
  }
  LOG(("Requested %u more streams", outStreamsNeeded));
  return true;
}

int32_t
DataChannelConnection::SendControlMessage(void *msg, uint32_t len, uint16_t stream)
{
  struct sctp_sndinfo sndinfo;

  // Note: Main-thread IO, but doesn't block
  memset(&sndinfo, 0, sizeof(struct sctp_sndinfo));
  sndinfo.snd_sid = stream;
  sndinfo.snd_ppid = htonl(DATA_CHANNEL_PPID_CONTROL);
  if (usrsctp_sendv(mSocket, msg, len, nullptr, 0,
                    &sndinfo, (socklen_t)sizeof(struct sctp_sndinfo),
                    SCTP_SENDV_SNDINFO, 0) < 0) {
    //LOG(("***failed: sctp_sendv")); don't log because errno is a return!
    return (0);
  }
  return (1);
}

int32_t
DataChannelConnection::SendOpenAckMessage(uint16_t stream)
{
  struct rtcweb_datachannel_ack ack;

  memset(&ack, 0, sizeof(struct rtcweb_datachannel_ack));
  ack.msg_type = DATA_CHANNEL_ACK;

  return SendControlMessage(&ack, sizeof(ack), stream);
}

int32_t
DataChannelConnection::SendOpenRequestMessage(const nsACString& label,
                                              const nsACString& protocol,
                                              uint16_t stream, bool unordered,
                                              uint16_t prPolicy, uint32_t prValue)
{
  int label_len = label.Length(); // not including nul
  int proto_len = protocol.Length(); // not including nul
  struct rtcweb_datachannel_open_request *req =
    (struct rtcweb_datachannel_open_request*) moz_xmalloc((sizeof(*req)-1) + label_len + proto_len);
   // careful - request includes 1 char label

  memset(req, 0, sizeof(struct rtcweb_datachannel_open_request));
  req->msg_type = DATA_CHANNEL_OPEN_REQUEST;
  switch (prPolicy) {
  case SCTP_PR_SCTP_NONE:
    req->channel_type = DATA_CHANNEL_RELIABLE;
    break;
  case SCTP_PR_SCTP_TTL:
    req->channel_type = DATA_CHANNEL_PARTIAL_RELIABLE_TIMED;
    break;
  case SCTP_PR_SCTP_RTX:
    req->channel_type = DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT;
    break;
  default:
    // FIX! need to set errno!  Or make all these SendXxxx() funcs return 0 or errno!
    moz_free(req);
    return (0);
  }
  if (unordered) {
    // Per the current types, all differ by 0x80 between ordered and unordered
    req->channel_type |= 0x80; // NOTE: be careful if new types are added in the future
  }

  req->reliability_param = htonl(prValue);
  req->priority = htons(0); /* XXX: add support */
  req->label_length = htons(label_len);
  req->protocol_length = htons(proto_len);
  memcpy(&req->label[0], PromiseFlatCString(label).get(), label_len);
  memcpy(&req->label[label_len], PromiseFlatCString(protocol).get(), proto_len);

  // sizeof(*req) already includes +1 byte for label, need nul for both strings
  int32_t result = SendControlMessage(req, (sizeof(*req)-1) + label_len + proto_len, stream);

  moz_free(req);
  return result;
}

// XXX This should use a separate thread (outbound queue) which should
// select() to know when to *try* to send data to the socket again.
// Alternatively, it can use a timeout, but that's guaranteed to be wrong
// (just not sure in what direction).  We could re-implement NSPR's
// PR_POLL_WRITE/etc handling... with a lot of work.

// Better yet, use the SCTP stack's notifications on buffer state to avoid
// filling the SCTP's buffers.

// returns if we're still blocked or not
bool
DataChannelConnection::SendDeferredMessages()
{
  uint32_t i;
  nsRefPtr<DataChannel> channel; // we may null out the refs to this
  bool still_blocked = false;
  bool sent = false;

  // This may block while something is modifying channels, but should not block for IO
  MutexAutoLock lock(mLock);

  // XXX For total fairness, on a still_blocked we'd start next time at the
  // same index.  Sorry, not going to bother for now.
  for (i = 0; i < mStreams.Length(); ++i) {
    channel = mStreams[i];
    if (!channel)
      continue;

    // Only one of these should be set....
    if (channel->mFlags & DATA_CHANNEL_FLAGS_SEND_REQ) {
      if (SendOpenRequestMessage(channel->mLabel, channel->mProtocol,
                                 channel->mStream,
                                 channel->mFlags & DATA_CHANNEL_FLAGS_OUT_OF_ORDER_ALLOWED,
                                 channel->mPrPolicy, channel->mPrValue)) {
        channel->mFlags &= ~DATA_CHANNEL_FLAGS_SEND_REQ;
        sent = true;
      } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          still_blocked = true;
        } else {
          // Close the channel, inform the user
          mStreams[channel->mStream] = nullptr;
          channel->mState = CLOSED;
          // Don't need to reset; we didn't open it
          NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                                    DataChannelOnMessageAvailable::ON_CHANNEL_CLOSED, this,
                                    channel));
        }
      }
    }
    if (still_blocked)
      break;

    if (channel->mFlags & DATA_CHANNEL_FLAGS_SEND_ACK) {
      if (SendOpenAckMessage(channel->mStream)) {
        channel->mFlags &= ~DATA_CHANNEL_FLAGS_SEND_ACK;
        sent = true;
      } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          still_blocked = true;
        } else {
          // Close the channel, inform the user
          CloseInt(channel);
          // XXX send error via DataChannelOnMessageAvailable (bug 843625)
        }
      }
    }
    if (still_blocked)
      break;

    if (channel->mFlags & DATA_CHANNEL_FLAGS_SEND_DATA) {
      bool failed_send = false;
      int32_t result;

      if (channel->mState == CLOSED || channel->mState == CLOSING) {
        channel->mBufferedData.Clear();
      }
      while (!channel->mBufferedData.IsEmpty() &&
             !failed_send) {
        struct sctp_sendv_spa *spa = channel->mBufferedData[0]->mSpa;
        const char *data           = channel->mBufferedData[0]->mData;
        uint32_t len               = channel->mBufferedData[0]->mLength;

        // SCTP will return EMSGSIZE if the message is bigger than the buffer
        // size (or EAGAIN if there isn't space)
        if ((result = usrsctp_sendv(mSocket, data, len,
                                    nullptr, 0,
                                    (void *)spa, (socklen_t)sizeof(struct sctp_sendv_spa),
                                    SCTP_SENDV_SPA,
                                    0) < 0)) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // leave queued for resend
            failed_send = true;
            LOG(("queue full again when resending %d bytes (%d)", len, result));
          } else {
            LOG(("error %d re-sending string", errno));
            failed_send = true;
          }
        } else {
          LOG(("Resent buffer of %d bytes (%d)", len, result));
          sent = true;
          channel->mBufferedData.RemoveElementAt(0);
        }
      }
      if (channel->mBufferedData.IsEmpty())
        channel->mFlags &= ~DATA_CHANNEL_FLAGS_SEND_DATA;
      else
        still_blocked = true;
    }
    if (still_blocked)
      break;
  }

  if (!still_blocked) {
    // mDeferTimeout becomes an estimate of how long we need to wait next time we block
    return false;
  }
  // adjust time?  More time for next wait if we didn't send anything, less if did
  // Pretty crude, but better than nothing; just to keep CPU use down
  if (!sent && mDeferTimeout < 50)
    mDeferTimeout++;
  else if (sent && mDeferTimeout > 10)
    mDeferTimeout--;

  return true;
}

void
DataChannelConnection::HandleOpenRequestMessage(const struct rtcweb_datachannel_open_request *req,
                                                size_t length,
                                                uint16_t stream)
{
  nsRefPtr<DataChannel> channel;
  uint32_t prValue;
  uint16_t prPolicy;
  uint32_t flags;

  mLock.AssertCurrentThreadOwns();

  if (length != (sizeof(*req) - 1) + ntohs(req->label_length) + ntohs(req->protocol_length)) {
    LOG(("%s: Inconsistent length: %u, should be %u", __FUNCTION__, length,
         (sizeof(*req) - 1) + ntohs(req->label_length) + ntohs(req->protocol_length)));
    if (length < (sizeof(*req) - 1) + ntohs(req->label_length) + ntohs(req->protocol_length))
      return;
  }

  LOG(("%s: length %u, sizeof(*req) = %u", __FUNCTION__, length, sizeof(*req)));

  switch (req->channel_type) {
    case DATA_CHANNEL_RELIABLE:
    case DATA_CHANNEL_RELIABLE_UNORDERED:
      prPolicy = SCTP_PR_SCTP_NONE;
      break;
    case DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT:
    case DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT_UNORDERED:
      prPolicy = SCTP_PR_SCTP_RTX;
      break;
    case DATA_CHANNEL_PARTIAL_RELIABLE_TIMED:
    case DATA_CHANNEL_PARTIAL_RELIABLE_TIMED_UNORDERED:
      prPolicy = SCTP_PR_SCTP_TTL;
      break;
    default:
      /* XXX error handling */
      return;
  }
  prValue = ntohl(req->reliability_param);
  flags = (req->channel_type & 0x80) ? DATA_CHANNEL_FLAGS_OUT_OF_ORDER_ALLOWED : 0;

  if ((channel = FindChannelByStream(stream))) {
    if (!(channel->mFlags & DATA_CHANNEL_FLAGS_EXTERNAL_NEGOTIATED)) {
      LOG(("ERROR: HandleOpenRequestMessage: channel for stream %u is in state %d instead of CLOSED.",
           stream, channel->mState));
     /* XXX: some error handling */
    } else {
      LOG(("Open for externally negotiated channel %u", stream));
      // XXX should also check protocol, maybe label
      if (prPolicy != channel->mPrPolicy ||
          prValue != channel->mPrValue ||
          flags != (channel->mFlags & DATA_CHANNEL_FLAGS_OUT_OF_ORDER_ALLOWED))
      {
        LOG(("WARNING: external negotiation mismatch with OpenRequest:"
             "channel %u, policy %u/%u, value %u/%u, flags %x/%x",
             stream, prPolicy, channel->mPrPolicy,
             prValue, channel->mPrValue, flags, channel->mFlags));
      }
    }
    return;
  }

  nsCString label(nsDependentCSubstring(&req->label[0], ntohs(req->label_length)));
  nsCString protocol(nsDependentCSubstring(&req->label[ntohs(req->label_length)],
                                           ntohs(req->protocol_length)));

  channel = new DataChannel(this,
                            stream,
                            DataChannel::CONNECTING,
                            label,
                            protocol,
                            prPolicy, prValue,
                            flags,
                            nullptr, nullptr);
  mStreams[stream] = channel;

  channel->mState = DataChannel::WAITING_TO_OPEN;

  LOG(("%s: sending ON_CHANNEL_CREATED for %s/%s: %u", __FUNCTION__,
       channel->mLabel.get(), channel->mProtocol.get(), stream));
  NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                            DataChannelOnMessageAvailable::ON_CHANNEL_CREATED,
                            this, channel));

  LOG(("%s: deferring sending ON_CHANNEL_OPEN for %p", __FUNCTION__, channel.get()));

  if (!SendOpenAckMessage(stream)) {
    // XXX Only on EAGAIN!?  And if not, then close the channel??
    channel->mFlags |= DATA_CHANNEL_FLAGS_SEND_ACK;
    StartDefer();
  }

  // Now process any queued data messages for the channel (which will
  // themselves likely get queued until we leave WAITING_TO_OPEN, plus any
  // more that come in before that happens)
  DeliverQueuedData(stream);
}

// NOTE: the updated spec from the IETF says we should set in-order until we receive an ACK.
// That would make this code moot.  Keep it for now for backwards compatibility.
void
DataChannelConnection::DeliverQueuedData(uint16_t stream)
{
  mLock.AssertCurrentThreadOwns();

  uint32_t i = 0;
  while (i < mQueuedData.Length()) {
    // Careful! we may modify the array length from within the loop!
    if (mQueuedData[i]->mStream == stream) {
      LOG(("Delivering queued data for stream %u, length %u",
           stream, mQueuedData[i]->mLength));
      // Deliver the queued data
      HandleDataMessage(mQueuedData[i]->mPpid,
                        mQueuedData[i]->mData, mQueuedData[i]->mLength,
                        mQueuedData[i]->mStream);
      mQueuedData.RemoveElementAt(i);
      continue; // don't bump index since we removed the element
    }
    i++;
  }
}

void
DataChannelConnection::HandleOpenAckMessage(const struct rtcweb_datachannel_ack *ack,
                                            size_t length, uint16_t stream)
{
  DataChannel *channel;

  mLock.AssertCurrentThreadOwns();

  channel = FindChannelByStream(stream);
  NS_ENSURE_TRUE_VOID(channel);

  LOG(("OpenAck received for stream %u, waiting=%d", stream,
       (channel->mFlags & DATA_CHANNEL_FLAGS_WAITING_ACK) ? 1 : 0));

  channel->mFlags &= ~DATA_CHANNEL_FLAGS_WAITING_ACK;
}

void
DataChannelConnection::HandleUnknownMessage(uint32_t ppid, size_t length, uint16_t stream)
{
  /* XXX: Send an error message? */
  LOG(("unknown DataChannel message received: %u, len %ld on stream %lu", ppid, length, stream));
  // XXX Log to JS error console if possible
}

void
DataChannelConnection::HandleDataMessage(uint32_t ppid,
                                         const void *data, size_t length,
                                         uint16_t stream)
{
  DataChannel *channel;
  const char *buffer = (const char *) data;

  mLock.AssertCurrentThreadOwns();

  channel = FindChannelByStream(stream);

  // XXX A closed channel may trip this... check
  // NOTE: the updated spec from the IETF says we should set in-order until we receive an ACK.
  // That would make this code moot.  Keep it for now for backwards compatibility.
  if (!channel) {
    // In the updated 0-RTT open case, the sender can send data immediately
    // after Open, and doesn't set the in-order bit (since we don't have a
    // response or ack).  Also, with external negotiation, data can come in
    // before we're told about the external negotiation.  We need to buffer
    // data until either a) Open comes in, if the ordering get messed up,
    // or b) the app tells us this channel was externally negotiated.  When
    // these occur, we deliver the data.

    // Since this is rare and non-performance, keep a single list of queued
    // data messages to deliver once the channel opens.
    LOG(("Queuing data for stream %u, length %u", stream, length));
    // Copies data
    mQueuedData.AppendElement(new QueuedDataMessage(stream, ppid, data, length));
    return;
  }

  // XXX should this be a simple if, no warnings/debugbreaks?
  NS_ENSURE_TRUE_VOID(channel->mState != CLOSED);

  {
    nsAutoCString recvData(buffer, length); // copies (<64) or allocates
    bool is_binary = true;

    if (ppid == DATA_CHANNEL_PPID_DOMSTRING ||
        ppid == DATA_CHANNEL_PPID_DOMSTRING_LAST) {
      is_binary = false;
    }
    if (is_binary != channel->mIsRecvBinary && !channel->mRecvBuffer.IsEmpty()) {
      NS_WARNING("DataChannel message aborted by fragment type change!");
      channel->mRecvBuffer.Truncate(0);
    }
    channel->mIsRecvBinary = is_binary;

    switch (ppid) {
      case DATA_CHANNEL_PPID_DOMSTRING:
      case DATA_CHANNEL_PPID_BINARY:
        channel->mRecvBuffer += recvData;
        LOG(("DataChannel: Partial %s message of length %lu (total %u) on channel id %u",
             is_binary ? "binary" : "string", length, channel->mRecvBuffer.Length(),
             channel->mStream));
        return; // Not ready to notify application

      case DATA_CHANNEL_PPID_DOMSTRING_LAST:
        LOG(("DataChannel: String message received of length %lu on channel %u",
             length, channel->mStream));
        if (!channel->mRecvBuffer.IsEmpty()) {
          channel->mRecvBuffer += recvData;
          LOG(("%s: sending ON_DATA (string fragmented) for %p", __FUNCTION__, channel));
          channel->SendOrQueue(new DataChannelOnMessageAvailable(
                                 DataChannelOnMessageAvailable::ON_DATA, this,
                                 channel, channel->mRecvBuffer, -1));
          channel->mRecvBuffer.Truncate(0);
          return;
        }
        // else send using recvData normally
        length = -1; // Flag for DOMString

        // WebSockets checks IsUTF8() here; we can try to deliver it
        break;

      case DATA_CHANNEL_PPID_BINARY_LAST:
        LOG(("DataChannel: Received binary message of length %lu on channel id %u",
             length, channel->mStream));
        if (!channel->mRecvBuffer.IsEmpty()) {
          channel->mRecvBuffer += recvData;
          LOG(("%s: sending ON_DATA (binary fragmented) for %p", __FUNCTION__, channel));
          channel->SendOrQueue(new DataChannelOnMessageAvailable(
                                 DataChannelOnMessageAvailable::ON_DATA, this,
                                 channel, channel->mRecvBuffer,
                                 channel->mRecvBuffer.Length()));
          channel->mRecvBuffer.Truncate(0);
          return;
        }
        // else send using recvData normally
        break;

      default:
        NS_ERROR("Unknown data PPID");
        return;
    }
    /* Notify onmessage */
    LOG(("%s: sending ON_DATA for %p", __FUNCTION__, channel));
    channel->SendOrQueue(new DataChannelOnMessageAvailable(
                           DataChannelOnMessageAvailable::ON_DATA, this,
                           channel, recvData, length));
  }
}

// Called with mLock locked!
void
DataChannelConnection::HandleMessage(const void *buffer, size_t length, uint32_t ppid, uint16_t stream)
{
  const struct rtcweb_datachannel_open_request *req;
  const struct rtcweb_datachannel_ack *ack;

  mLock.AssertCurrentThreadOwns();

  switch (ppid) {
    case DATA_CHANNEL_PPID_CONTROL:
      req = static_cast<const struct rtcweb_datachannel_open_request *>(buffer);

      NS_ENSURE_TRUE_VOID(length >= sizeof(*ack)); // smallest message
      switch (req->msg_type) {
        case DATA_CHANNEL_OPEN_REQUEST:
          // structure includes a possibly-unused char label[1] (in a packed structure)
          NS_ENSURE_TRUE_VOID(length >= sizeof(*req) - 1);

          HandleOpenRequestMessage(req, length, stream);
          break;
        case DATA_CHANNEL_ACK:
          // >= sizeof(*ack) checked above

          ack = static_cast<const struct rtcweb_datachannel_ack *>(buffer);
          HandleOpenAckMessage(ack, length, stream);
          break;
        default:
          HandleUnknownMessage(ppid, length, stream);
          break;
      }
      break;
    case DATA_CHANNEL_PPID_DOMSTRING:
    case DATA_CHANNEL_PPID_DOMSTRING_LAST:
    case DATA_CHANNEL_PPID_BINARY:
    case DATA_CHANNEL_PPID_BINARY_LAST:
      HandleDataMessage(ppid, buffer, length, stream);
      break;
    default:
      LOG(("Message of length %lu, PPID %u on stream %u received.",
           length, ppid, stream));
      break;
  }
}

void
DataChannelConnection::HandleAssociationChangeEvent(const struct sctp_assoc_change *sac)
{
  uint32_t i, n;

  switch (sac->sac_state) {
  case SCTP_COMM_UP:
    LOG(("Association change: SCTP_COMM_UP"));
    if (mState == CONNECTING) {
      mSocket = mMasterSocket;
      mState = OPEN;

      SetEvenOdd();

      NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                                DataChannelOnMessageAvailable::ON_CONNECTION,
                                this));
      LOG(("DTLS connect() succeeded!  Entering connected mode"));

      // Open any streams pending...
      ProcessQueuedOpens();

    } else if (mState == OPEN) {
      LOG(("DataConnection Already OPEN"));
    } else {
      LOG(("Unexpected state: %d", mState));
    }
    break;
  case SCTP_COMM_LOST:
    LOG(("Association change: SCTP_COMM_LOST"));
    // This association is toast, so also close all the channels -- from mainthread!
    NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                              DataChannelOnMessageAvailable::ON_DISCONNECTED,
                              this));
    break;
  case SCTP_RESTART:
    LOG(("Association change: SCTP_RESTART"));
    break;
  case SCTP_SHUTDOWN_COMP:
    LOG(("Association change: SCTP_SHUTDOWN_COMP"));
    NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                              DataChannelOnMessageAvailable::ON_DISCONNECTED,
                              this));
    break;
  case SCTP_CANT_STR_ASSOC:
    LOG(("Association change: SCTP_CANT_STR_ASSOC"));
    break;
  default:
    LOG(("Association change: UNKNOWN"));
    break;
  }
  LOG(("Association change: streams (in/out) = (%u/%u)",
       sac->sac_inbound_streams, sac->sac_outbound_streams));

  NS_ENSURE_TRUE_VOID(sac);
  n = sac->sac_length - sizeof(*sac);
  if (((sac->sac_state == SCTP_COMM_UP) ||
        (sac->sac_state == SCTP_RESTART)) && (n > 0)) {
    for (i = 0; i < n; ++i) {
      switch (sac->sac_info[i]) {
      case SCTP_ASSOC_SUPPORTS_PR:
        LOG(("Supports: PR"));
        break;
      case SCTP_ASSOC_SUPPORTS_AUTH:
        LOG(("Supports: AUTH"));
        break;
      case SCTP_ASSOC_SUPPORTS_ASCONF:
        LOG(("Supports: ASCONF"));
        break;
      case SCTP_ASSOC_SUPPORTS_MULTIBUF:
        LOG(("Supports: MULTIBUF"));
        break;
      case SCTP_ASSOC_SUPPORTS_RE_CONFIG:
        LOG(("Supports: RE-CONFIG"));
        break;
      default:
        LOG(("Supports: UNKNOWN(0x%02x)", sac->sac_info[i]));
        break;
      }
    }
  } else if (((sac->sac_state == SCTP_COMM_LOST) ||
              (sac->sac_state == SCTP_CANT_STR_ASSOC)) && (n > 0)) {
    LOG(("Association: ABORT ="));
    for (i = 0; i < n; ++i) {
      LOG((" 0x%02x", sac->sac_info[i]));
    }
  }
  if ((sac->sac_state == SCTP_CANT_STR_ASSOC) ||
      (sac->sac_state == SCTP_SHUTDOWN_COMP) ||
      (sac->sac_state == SCTP_COMM_LOST)) {
    return;
  }
}

void
DataChannelConnection::HandlePeerAddressChangeEvent(const struct sctp_paddr_change *spc)
{
  char addr_buf[INET6_ADDRSTRLEN];
  const char *addr = "";
  struct sockaddr_in *sin;
  struct sockaddr_in6 *sin6;
#if defined(__Userspace_os_Windows)
  DWORD addr_len = INET6_ADDRSTRLEN;
#endif

  switch (spc->spc_aaddr.ss_family) {
  case AF_INET:
    sin = (struct sockaddr_in *)&spc->spc_aaddr;
#if !defined(__Userspace_os_Windows)
    addr = inet_ntop(AF_INET, &sin->sin_addr, addr_buf, INET6_ADDRSTRLEN);
#else
    if (WSAAddressToStringA((LPSOCKADDR)sin, sizeof(sin->sin_addr), nullptr,
                            addr_buf, &addr_len)) {
      return;
    }
#endif
    break;
  case AF_INET6:
    sin6 = (struct sockaddr_in6 *)&spc->spc_aaddr;
#if !defined(__Userspace_os_Windows)
    addr = inet_ntop(AF_INET6, &sin6->sin6_addr, addr_buf, INET6_ADDRSTRLEN);
#else
    if (WSAAddressToStringA((LPSOCKADDR)sin6, sizeof(sin6), nullptr,
                            addr_buf, &addr_len)) {
      return;
    }
#endif
  case AF_CONN:
    addr = "DTLS connection";
    break;
  default:
    break;
  }
  LOG(("Peer address %s is now ", addr));
  switch (spc->spc_state) {
  case SCTP_ADDR_AVAILABLE:
    LOG(("SCTP_ADDR_AVAILABLE"));
    break;
  case SCTP_ADDR_UNREACHABLE:
    LOG(("SCTP_ADDR_UNREACHABLE"));
    break;
  case SCTP_ADDR_REMOVED:
    LOG(("SCTP_ADDR_REMOVED"));
    break;
  case SCTP_ADDR_ADDED:
    LOG(("SCTP_ADDR_ADDED"));
    break;
  case SCTP_ADDR_MADE_PRIM:
    LOG(("SCTP_ADDR_MADE_PRIM"));
    break;
  case SCTP_ADDR_CONFIRMED:
    LOG(("SCTP_ADDR_CONFIRMED"));
    break;
  default:
    LOG(("UNKNOWN"));
    break;
  }
  LOG((" (error = 0x%08x).\n", spc->spc_error));
}

void
DataChannelConnection::HandleRemoteErrorEvent(const struct sctp_remote_error *sre)
{
  size_t i, n;

  n = sre->sre_length - sizeof(struct sctp_remote_error);
  LOG(("Remote Error (error = 0x%04x): ", sre->sre_error));
  for (i = 0; i < n; ++i) {
    LOG((" 0x%02x", sre-> sre_data[i]));
  }
}

void
DataChannelConnection::HandleShutdownEvent(const struct sctp_shutdown_event *sse)
{
  LOG(("Shutdown event."));
  /* XXX: notify all channels. */
  // Attempts to actually send anything will fail
}

void
DataChannelConnection::HandleAdaptationIndication(const struct sctp_adaptation_event *sai)
{
  LOG(("Adaptation indication: %x.", sai-> sai_adaptation_ind));
}

void
DataChannelConnection::HandleSendFailedEvent(const struct sctp_send_failed_event *ssfe)
{
  size_t i, n;

  if (ssfe->ssfe_flags & SCTP_DATA_UNSENT) {
    LOG(("Unsent "));
  }
   if (ssfe->ssfe_flags & SCTP_DATA_SENT) {
    LOG(("Sent "));
  }
  if (ssfe->ssfe_flags & ~(SCTP_DATA_SENT | SCTP_DATA_UNSENT)) {
    LOG(("(flags = %x) ", ssfe->ssfe_flags));
  }
  LOG(("message with PPID = %u, SID = %d, flags: 0x%04x due to error = 0x%08x",
       ntohl(ssfe->ssfe_info.snd_ppid), ssfe->ssfe_info.snd_sid,
       ssfe->ssfe_info.snd_flags, ssfe->ssfe_error));
  n = ssfe->ssfe_length - sizeof(struct sctp_send_failed_event);
  for (i = 0; i < n; ++i) {
    LOG((" 0x%02x", ssfe->ssfe_data[i]));
  }
}

void
DataChannelConnection::ClearResets()
{
  // Clear all pending resets
  if (!mStreamsResetting.IsEmpty()) {
    LOG(("Clearing resets for %d streams", mStreamsResetting.Length()));
  }

  for (uint32_t i = 0; i < mStreamsResetting.Length(); ++i) {
    nsRefPtr<DataChannel> channel;
    channel = FindChannelByStream(mStreamsResetting[i]);
    if (channel) {
      LOG(("Forgetting channel %u (%p) with pending reset",channel->mStream, channel.get()));
      mStreams[channel->mStream] = nullptr;
    }
  }
  mStreamsResetting.Clear();
}

void
DataChannelConnection::ResetOutgoingStream(uint16_t stream)
{
  uint32_t i;

  mLock.AssertCurrentThreadOwns();
  LOG(("Connection %p: Resetting outgoing stream %u",
       (void *) this, stream));
  // Rarely has more than a couple items and only for a short time
  for (i = 0; i < mStreamsResetting.Length(); ++i) {
    if (mStreamsResetting[i] == stream) {
      return;
    }
  }
  mStreamsResetting.AppendElement(stream);
}

void
DataChannelConnection::SendOutgoingStreamReset()
{
  struct sctp_reset_streams *srs;
  uint32_t i;
  size_t len;

  LOG(("Connection %p: Sending outgoing stream reset for %d streams",
       (void *) this, mStreamsResetting.Length()));
  mLock.AssertCurrentThreadOwns();
  if (mStreamsResetting.IsEmpty()) {
    LOG(("No streams to reset"));
    return;
  }
  len = sizeof(sctp_assoc_t) + (2 + mStreamsResetting.Length()) * sizeof(uint16_t);
  srs = static_cast<struct sctp_reset_streams *> (moz_xmalloc(len)); // infallible malloc
  memset(srs, 0, len);
  srs->srs_flags = SCTP_STREAM_RESET_OUTGOING;
  srs->srs_number_streams = mStreamsResetting.Length();
  for (i = 0; i < mStreamsResetting.Length(); ++i) {
    srs->srs_stream_list[i] = mStreamsResetting[i];
  }
  if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_RESET_STREAMS, srs, (socklen_t)len) < 0) {
    LOG(("***failed: setsockopt RESET, errno %d", errno));
    // if errno == EALREADY, this is normal - we can't send another reset
    // with one pending.
    // When we get an incoming reset (which may be a response to our
    // outstanding one), see if we have any pending outgoing resets and
    // send them
  } else {
    mStreamsResetting.Clear();
  }
  moz_free(srs);
}

void
DataChannelConnection::HandleStreamResetEvent(const struct sctp_stream_reset_event *strrst)
{
  uint32_t n, i;
  nsRefPtr<DataChannel> channel; // since we may null out the ref to the channel

  if (!(strrst->strreset_flags & SCTP_STREAM_RESET_DENIED) &&
      !(strrst->strreset_flags & SCTP_STREAM_RESET_FAILED)) {
    n = (strrst->strreset_length - sizeof(struct sctp_stream_reset_event)) / sizeof(uint16_t);
    for (i = 0; i < n; ++i) {
      if (strrst->strreset_flags & SCTP_STREAM_RESET_INCOMING_SSN) {
        channel = FindChannelByStream(strrst->strreset_stream_list[i]);
        if (channel) {
          // The other side closed the channel
          // We could be in three states:
          // 1. Normal state (input and output streams (OPEN)
          //    Notify application, send a RESET in response on our
          //    outbound channel.  Go to CLOSED
          // 2. We sent our own reset (CLOSING); either they crossed on the
          //    wire, or this is a response to our Reset.
          //    Go to CLOSED
          // 3. We've sent a open but haven't gotten a response yet (OPENING)
          //    I believe this is impossible, as we don't have an input stream yet.

          LOG(("Incoming: Channel %u  closed, state %d",
               channel->mStream, channel->mState));
          ASSERT_WEBRTC(channel->mState == DataChannel::OPEN ||
                        channel->mState == DataChannel::CLOSING ||
                        channel->mState == DataChannel::WAITING_TO_OPEN);
          if (channel->mState == DataChannel::OPEN ||
              channel->mState == DataChannel::WAITING_TO_OPEN) {
            ResetOutgoingStream(channel->mStream);
            SendOutgoingStreamReset();
            NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                                      DataChannelOnMessageAvailable::ON_CHANNEL_CLOSED, this,
                                      channel));
          }
          mStreams[channel->mStream] = nullptr;

          LOG(("Disconnected DataChannel %p from connection %p",
               (void *) channel.get(), (void *) channel->mConnection.get()));
          channel->Destroy();
          // At this point when we leave here, the object is a zombie held alive only by the DOM object
        } else {
          LOG(("Can't find incoming channel %d",i));
        }
      }
    }
  }

  // In case we failed to send a RESET due to having one outstanding, process any pending resets now:
  if (!mStreamsResetting.IsEmpty()) {
    LOG(("Sending %d pending resets", mStreamsResetting.Length()));
    SendOutgoingStreamReset();
  }
}

void
DataChannelConnection::HandleStreamChangeEvent(const struct sctp_stream_change_event *strchg)
{
  uint16_t stream;
  uint32_t i;
  nsRefPtr<DataChannel> channel;

  if (strchg->strchange_flags == SCTP_STREAM_CHANGE_DENIED) {
    LOG(("*** Failed increasing number of streams from %u (%u/%u)",
         mStreams.Length(),
         strchg->strchange_instrms,
         strchg->strchange_outstrms));
    // XXX FIX! notify pending opens of failure
    return;
  } else {
    if (strchg->strchange_instrms > mStreams.Length()) {
      LOG(("Other side increased streamds from %u to %u",
           mStreams.Length(), strchg->strchange_instrms));
    }
    if (strchg->strchange_outstrms > mStreams.Length()) {
      uint16_t old_len = mStreams.Length();
      LOG(("Increasing number of streams from %u to %u - adding %u (in: %u)",
           old_len,
           strchg->strchange_outstrms,
           strchg->strchange_outstrms - old_len,
           strchg->strchange_instrms));
      // make sure both are the same length
      mStreams.AppendElements(strchg->strchange_outstrms - old_len);
      LOG(("New length = %d (was %d)", mStreams.Length(), old_len));
      for (uint32_t i = old_len; i < mStreams.Length(); ++i) {
        mStreams[i] = nullptr;
      }
      // Re-process any channels waiting for streams.
      // Linear search, but we don't increase channels often and
      // the array would only get long in case of an app error normally

      // Make sure we request enough streams if there's a big jump in streams
      // Could make a more complex API for OpenXxxFinish() and avoid this loop
      int32_t num_needed = mPending.GetSize();
      LOG(("%d of %d new streams already needed", num_needed,
           strchg->strchange_outstrms - old_len));
      num_needed -= (strchg->strchange_outstrms - old_len); // number we added
      if (num_needed > 0) {
        if (num_needed < 16)
          num_needed = 16;
        LOG(("Not enough new streams, asking for %d more", num_needed));
        RequestMoreStreams(num_needed);
      }

      ProcessQueuedOpens();
    }
    // else probably not a change in # of streams
  }

  for (i = 0; i < mStreams.Length(); ++i) {
    channel = mStreams[i];
    if (!channel)
      continue;

    if ((channel->mState == CONNECTING) &&
        (channel->mStream == INVALID_STREAM)) {
      if ((strchg->strchange_flags & SCTP_STREAM_CHANGE_DENIED) ||
          (strchg->strchange_flags & SCTP_STREAM_CHANGE_FAILED)) {
        /* XXX: Signal to the other end. */
        channel->mState = CLOSED;
        NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                                  DataChannelOnMessageAvailable::ON_CHANNEL_CLOSED, this,
                                  channel));
        // maybe fire onError (bug 843625)
      } else {
        stream = FindFreeStream();
        if (stream != INVALID_STREAM) {
          channel->mStream = stream;
          mStreams[stream] = channel;
          channel->mFlags |= DATA_CHANNEL_FLAGS_SEND_REQ;
          /// XXX fix
          StartDefer();
        } else {
          /* We will not find more ... */
          break;
        }
      }
    }
  }
}


// Called with mLock locked!
void
DataChannelConnection::HandleNotification(const union sctp_notification *notif, size_t n)
{
  mLock.AssertCurrentThreadOwns();
  if (notif->sn_header.sn_length != (uint32_t)n) {
    return;
  }
  switch (notif->sn_header.sn_type) {
  case SCTP_ASSOC_CHANGE:
    HandleAssociationChangeEvent(&(notif->sn_assoc_change));
    break;
  case SCTP_PEER_ADDR_CHANGE:
    HandlePeerAddressChangeEvent(&(notif->sn_paddr_change));
    break;
  case SCTP_REMOTE_ERROR:
    HandleRemoteErrorEvent(&(notif->sn_remote_error));
    break;
  case SCTP_SHUTDOWN_EVENT:
    HandleShutdownEvent(&(notif->sn_shutdown_event));
    break;
  case SCTP_ADAPTATION_INDICATION:
    HandleAdaptationIndication(&(notif->sn_adaptation_event));
    break;
  case SCTP_PARTIAL_DELIVERY_EVENT:
    LOG(("SCTP_PARTIAL_DELIVERY_EVENT"));
    break;
  case SCTP_AUTHENTICATION_EVENT:
    LOG(("SCTP_AUTHENTICATION_EVENT"));
    break;
  case SCTP_SENDER_DRY_EVENT:
    //LOG(("SCTP_SENDER_DRY_EVENT"));
    break;
  case SCTP_NOTIFICATIONS_STOPPED_EVENT:
    LOG(("SCTP_NOTIFICATIONS_STOPPED_EVENT"));
    break;
  case SCTP_SEND_FAILED_EVENT:
    HandleSendFailedEvent(&(notif->sn_send_failed_event));
    break;
  case SCTP_STREAM_RESET_EVENT:
    HandleStreamResetEvent(&(notif->sn_strreset_event));
    break;
  case SCTP_ASSOC_RESET_EVENT:
    LOG(("SCTP_ASSOC_RESET_EVENT"));
    break;
  case SCTP_STREAM_CHANGE_EVENT:
    HandleStreamChangeEvent(&(notif->sn_strchange_event));
    break;
  default:
    LOG(("unknown SCTP event: %u", (uint32_t)notif->sn_header.sn_type));
    break;
   }
 }

int
DataChannelConnection::ReceiveCallback(struct socket* sock, void *data, size_t datalen,
                                       struct sctp_rcvinfo rcv, int32_t flags)
{
  ASSERT_WEBRTC(!NS_IsMainThread());

  if (!data) {
    usrsctp_close(sock); // SCTP has finished shutting down
  } else {
    MutexAutoLock lock(mLock);
    if (flags & MSG_NOTIFICATION) {
      HandleNotification(static_cast<union sctp_notification *>(data), datalen);
    } else {
      HandleMessage(data, datalen, ntohl(rcv.rcv_ppid), rcv.rcv_sid);
    }
  }
  // sctp allocates 'data' with malloc(), and expects the receiver to free
  // it (presumably with free).
  // XXX future optimization: try to deliver messages without an internal
  // alloc/copy, and if so delay the free until later.
  free(data);
  // usrsctp defines the callback as returning an int, but doesn't use it
  return 1;
}

already_AddRefed<DataChannel>
DataChannelConnection::Open(const nsACString& label, const nsACString& protocol,
                            Type type, bool inOrder,
                            uint32_t prValue, DataChannelListener *aListener,
                            nsISupports *aContext, bool aExternalNegotiated,
                            uint16_t aStream)
{
  // aStream == INVALID_STREAM to have the protocol allocate
  uint16_t prPolicy = SCTP_PR_SCTP_NONE;
  uint32_t flags;

  LOG(("DC Open: label %s/%s, type %u, inorder %d, prValue %u, listener %p, context %p, external: %s, stream %u",
       PromiseFlatCString(label).get(), PromiseFlatCString(protocol).get(),
       type, inOrder, prValue, aListener, aContext,
       aExternalNegotiated ? "true" : "false", aStream));
  switch (type) {
    case DATA_CHANNEL_RELIABLE:
      prPolicy = SCTP_PR_SCTP_NONE;
      break;
    case DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT:
      prPolicy = SCTP_PR_SCTP_RTX;
      break;
    case DATA_CHANNEL_PARTIAL_RELIABLE_TIMED:
      prPolicy = SCTP_PR_SCTP_TTL;
      break;
  }
  if ((prPolicy == SCTP_PR_SCTP_NONE) && (prValue != 0)) {
    return nullptr;
  }

  // Don't look past currently-negotiated streams
  if (aStream != INVALID_STREAM && aStream < mStreams.Length() && mStreams[aStream]) {
    LOG(("ERROR: external negotiation of already-open channel %u", aStream));
    // XXX How do we indicate this up to the application?  Probably the
    // caller's job, but we may need to return an error code.
    return nullptr;
  }

  flags = !inOrder ? DATA_CHANNEL_FLAGS_OUT_OF_ORDER_ALLOWED : 0;
  nsRefPtr<DataChannel> channel(new DataChannel(this,
                                                aStream,
                                                DataChannel::CONNECTING,
                                                label, protocol,
                                                type, prValue,
                                                flags,
                                                aListener, aContext));
  if (aExternalNegotiated) {
    channel->mFlags |= DATA_CHANNEL_FLAGS_EXTERNAL_NEGOTIATED;
  }

  MutexAutoLock lock(mLock); // OpenFinish assumes this
  return OpenFinish(channel.forget());
}

// Separate routine so we can also call it to finish up from pending opens
already_AddRefed<DataChannel>
DataChannelConnection::OpenFinish(already_AddRefed<DataChannel>&& aChannel)
{
  nsRefPtr<DataChannel> channel(aChannel); // takes the reference passed in
  // Normally 1 reference if called from ::Open(), or 2 if called from
  // ProcessQueuedOpens() unless the DOMDataChannel was gc'd
  uint16_t stream = channel->mStream;
  bool queue = false;

  mLock.AssertCurrentThreadOwns();

  // Cases we care about:
  // Pre-negotiated:
  //    Not Open:
  //      Doesn't fit:
  //         -> change initial ask or renegotiate after open
  //      -> queue open
  //    Open:
  //      Doesn't fit:
  //         -> RequestMoreStreams && queue
  //      Does fit:
  //         -> open
  // Not negotiated:
  //    Not Open:
  //      -> queue open
  //    Open:
  //      -> Try to get a stream
  //      Doesn't fit:
  //         -> RequestMoreStreams && queue
  //      Does fit:
  //         -> open
  // So the Open cases are basically the same
  // Not Open cases are simply queue for non-negotiated, and
  // either change the initial ask or possibly renegotiate after open.

  if (mState == OPEN) {
    if (stream == INVALID_STREAM) {
      stream = FindFreeStream(); // may be INVALID_STREAM if we need more
    }
    if (stream == INVALID_STREAM || stream >= mStreams.Length()) {
      // RequestMoreStreams() limits to MAX_NUM_STREAMS -- allocate extra streams
      // to avoid going back immediately for more if the ask to N, N+1, etc
      int32_t more_needed = (stream == INVALID_STREAM) ? 16 :
                            (stream-((int32_t)mStreams.Length())) + 16;
      if (!RequestMoreStreams(more_needed)) {
        // Something bad happened... we're done
        goto request_error_cleanup;
      }
      queue = true;
    }
  } else {
    // not OPEN
    if (stream != INVALID_STREAM && stream >= mStreams.Length() &&
        mState == CLOSED) {
      // Update number of streams for init message
      struct sctp_initmsg initmsg;
      socklen_t len = sizeof(initmsg);
      int32_t total_needed = stream+16;

      memset(&initmsg, 0, sizeof(initmsg));
      if (usrsctp_getsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, &len) < 0) {
        LOG(("*** failed getsockopt SCTP_INITMSG"));
        goto request_error_cleanup;
      }
      LOG(("Setting number of SCTP streams to %u, was %u/%u", total_needed,
           initmsg.sinit_num_ostreams, initmsg.sinit_max_instreams));
      initmsg.sinit_num_ostreams  = total_needed;
      initmsg.sinit_max_instreams = MAX_NUM_STREAMS;
      if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg,
                             (socklen_t)sizeof(initmsg)) < 0) {
        LOG(("*** failed setsockopt SCTP_INITMSG, errno %d", errno));
        goto request_error_cleanup;
      }

      int32_t old_len = mStreams.Length();
      mStreams.AppendElements(total_needed - old_len);
      for (int32_t i = old_len; i < total_needed; ++i) {
        mStreams[i] = nullptr;
      }
    }
    // else if state is CONNECTING, we'll just re-negotiate when OpenFinish
    // is called, if needed
    queue = true;
  }
  if (queue) {
    LOG(("Queuing channel %p (%u) to finish open", channel.get(), stream));
    // Also serves to mark we told the app
    channel->mFlags |= DATA_CHANNEL_FLAGS_FINISH_OPEN;
    channel->AddRef(); // we need a ref for the nsDeQue and one to return
    mPending.Push(channel);
    return channel.forget();
  }

  MOZ_ASSERT(stream != INVALID_STREAM);
  // just allocated (& OPEN), or externally negotiated
  mStreams[stream] = channel; // holds a reference
  channel->mStream = stream;

#ifdef TEST_QUEUED_DATA
  // It's painful to write a test for this...
  channel->mState = OPEN;
  channel->mReady = true;
  SendMsgInternal(channel, "Help me!", 8, DATA_CHANNEL_PPID_DOMSTRING_LAST);
#endif

  if (channel->mFlags & DATA_CHANNEL_FLAGS_OUT_OF_ORDER_ALLOWED) {
    // Don't send unordered until this gets cleared
    channel->mFlags |= DATA_CHANNEL_FLAGS_WAITING_ACK;
  }

  if (!(channel->mFlags & DATA_CHANNEL_FLAGS_EXTERNAL_NEGOTIATED)) {
    if (!SendOpenRequestMessage(channel->mLabel, channel->mProtocol,
                                stream,
                                !!(channel->mFlags & DATA_CHANNEL_FLAGS_OUT_OF_ORDER_ALLOWED),
                                channel->mPrPolicy, channel->mPrValue)) {
      LOG(("SendOpenRequest failed, errno = %d", errno));
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        channel->mFlags |= DATA_CHANNEL_FLAGS_SEND_REQ;
        StartDefer();

        return channel.forget();
      } else {
        if (channel->mFlags & DATA_CHANNEL_FLAGS_FINISH_OPEN) {
          // We already returned the channel to the app.
          NS_ERROR("Failed to send open request");
          NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                                    DataChannelOnMessageAvailable::ON_CHANNEL_CLOSED, this,
                                    channel));
        }
        // If we haven't returned the channel yet, it will get destroyed when we exit
        // this function.
        mStreams[stream] = nullptr;
        channel->mStream = INVALID_STREAM;
        // we'll be destroying the channel
        channel->mState = CLOSED;
        return nullptr;
      }
      /* NOTREACHED */
    }
  }
  // Either externally negotiated or we sent Open
  channel->mState = OPEN;
  channel->mReady = true;
  // FIX?  Move into DOMDataChannel?  I don't think we can send it yet here
  LOG(("%s: sending ON_CHANNEL_OPEN for %p", __FUNCTION__, channel.get()));
  NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                            DataChannelOnMessageAvailable::ON_CHANNEL_OPEN, this,
                            channel));

  return channel.forget();

request_error_cleanup:
  channel->mState = CLOSED;
  if (channel->mFlags & DATA_CHANNEL_FLAGS_FINISH_OPEN) {
    // We already returned the channel to the app.
    NS_ERROR("Failed to request more streams");
    NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                              DataChannelOnMessageAvailable::ON_CHANNEL_CLOSED, this,
                              channel));
    return channel.forget();
  }
  // we'll be destroying the channel, but it never really got set up
  // Alternative would be to RUN_ON_THREAD(channel.forget(),::Destroy,...) and
  // Dispatch it to ourselves
  return nullptr;
}

int32_t
DataChannelConnection::SendMsgInternal(DataChannel *channel, const char *data,
                                       uint32_t length, uint32_t ppid)
{
  uint16_t flags;
  struct sctp_sendv_spa spa;
  int32_t result;

  NS_ENSURE_TRUE(channel->mState == OPEN || channel->mState == CONNECTING, 0);
  NS_WARN_IF_FALSE(length > 0, "Length is 0?!");

  // To avoid problems where an in-order OPEN is lost and an
  // out-of-order data message "beats" it, require data to be in-order
  // until we get an ACK.
  if ((channel->mFlags & DATA_CHANNEL_FLAGS_OUT_OF_ORDER_ALLOWED) &&
      !(channel->mFlags & DATA_CHANNEL_FLAGS_WAITING_ACK)) {
    flags = SCTP_UNORDERED;
  } else {
    flags = 0;
  }

  spa.sendv_sndinfo.snd_ppid = htonl(ppid);
  spa.sendv_sndinfo.snd_sid = channel->mStream;
  spa.sendv_sndinfo.snd_flags = flags;
  spa.sendv_sndinfo.snd_context = 0;
  spa.sendv_sndinfo.snd_assoc_id = 0;
  spa.sendv_flags = SCTP_SEND_SNDINFO_VALID;

  if (channel->mPrPolicy != SCTP_PR_SCTP_NONE) {
    spa.sendv_prinfo.pr_policy = channel->mPrPolicy;
    spa.sendv_prinfo.pr_value = channel->mPrValue;
    spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
  }

  // Note: Main-thread IO, but doesn't block!
  // XXX FIX!  to deal with heavy overruns of JS trying to pass data in
  // (more than the buffersize) queue data onto another thread to do the
  // actual sends.  See netwerk/protocol/websocket/WebSocketChannel.cpp

  // SCTP will return EMSGSIZE if the message is bigger than the buffer
  // size (or EAGAIN if there isn't space)
  if (channel->mBufferedData.IsEmpty()) {
    result = usrsctp_sendv(mSocket, data, length,
                           nullptr, 0,
                           (void *)&spa, (socklen_t)sizeof(struct sctp_sendv_spa),
                           SCTP_SENDV_SPA, 0);
    LOG(("Sent buffer (len=%u), result=%d", length, result));
  } else {
    // Fake EAGAIN if we're already buffering data
    result = -1;
    errno = EAGAIN;
  }
  if (result < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // queue data for resend!  And queue any further data for the stream until it is...
      BufferedMsg *buffered = new BufferedMsg(spa, data, length); // infallible malloc
      channel->mBufferedData.AppendElement(buffered); // owned by mBufferedData array
      channel->mFlags |= DATA_CHANNEL_FLAGS_SEND_DATA;
      LOG(("Queued %u buffers (len=%u)", channel->mBufferedData.Length(), length));
      StartDefer();
      return 0;
    }
    LOG(("error %d sending string", errno));
  }
  return result;
}

// Handles fragmenting binary messages
int32_t
DataChannelConnection::SendBinary(DataChannel *channel, const char *data,
                                  uint32_t len,
                                  uint32_t ppid_partial, uint32_t ppid_final)
{
  // Since there's a limit on network buffer size and no limits on message
  // size, and we don't want to use EOR mode (multiple writes for a
  // message, but all other streams are blocked until you finish sending
  // this message), we need to add application-level fragmentation of large
  // messages.  On a reliable channel, these can be simply rebuilt into a
  // large message.  On an unreliable channel, we can't and don't know how
  // long to wait, and there are no retransmissions, and no easy way to
  // tell the user "this part is missing", so on unreliable channels we
  // need to return an error if sending more bytes than the network buffers
  // can hold, and perhaps a lower number.

  // We *really* don't want to do this from main thread! - and SendMsgInternal
  // avoids blocking.
  // This MUST be reliable and in-order for the reassembly to work
  if (len > DATA_CHANNEL_MAX_BINARY_FRAGMENT &&
      channel->mPrPolicy == DATA_CHANNEL_RELIABLE &&
      !(channel->mFlags & DATA_CHANNEL_FLAGS_OUT_OF_ORDER_ALLOWED)) {
    int32_t sent=0;
    uint32_t origlen = len;
    LOG(("Sending binary message length %u in chunks", len));
    // XXX check flags for out-of-order, or force in-order for large binary messages
    while (len > 0) {
      uint32_t sendlen = PR_MIN(len, DATA_CHANNEL_MAX_BINARY_FRAGMENT);
      uint32_t ppid;
      len -= sendlen;
      ppid = len > 0 ? ppid_partial : ppid_final;
      LOG(("Send chunk of %u bytes, ppid %u", sendlen, ppid));
      // Note that these might end up being deferred and queued.
      sent += SendMsgInternal(channel, data, sendlen, ppid);
      data += sendlen;
    }
    LOG(("Sent %d buffers for %u bytes, %d sent immediately, %d buffers queued",
         (origlen+DATA_CHANNEL_MAX_BINARY_FRAGMENT-1)/DATA_CHANNEL_MAX_BINARY_FRAGMENT,
         origlen, sent,
         channel->mBufferedData.Length()));
    return sent;
  }
  NS_WARN_IF_FALSE(len <= DATA_CHANNEL_MAX_BINARY_FRAGMENT,
                   "Sending too-large data on unreliable channel!");

  // This will fail if the message is too large (default 256K)
  return SendMsgInternal(channel, data, len, ppid_final);
}

class ReadBlobRunnable : public nsRunnable {
public:
  ReadBlobRunnable(DataChannelConnection* aConnection, uint16_t aStream,
    nsIInputStream* aBlob) :
    mConnection(aConnection),
    mStream(aStream),
    mBlob(aBlob)
  { }

  NS_IMETHODIMP Run() {
    // ReadBlob() is responsible to releasing the reference
    DataChannelConnection *self = mConnection;
    self->ReadBlob(mConnection.forget(), mStream, mBlob);
    return NS_OK;
  }

private:
  // Make sure the Connection doesn't die while there are jobs outstanding.
  // Let it die (if released by PeerConnectionImpl while we're running)
  // when we send our runnable back to MainThread.  Then ~DataChannelConnection
  // can send the IOThread to MainThread to die in a runnable, avoiding
  // unsafe event loop recursion.  Evil.
  nsRefPtr<DataChannelConnection> mConnection;
  uint16_t mStream;
  // Use RefCount for preventing the object is deleted when SendBlob returns.
  nsRefPtr<nsIInputStream> mBlob;
};

int32_t
DataChannelConnection::SendBlob(uint16_t stream, nsIInputStream *aBlob)
{
  DataChannel *channel = mStreams[stream];
  NS_ENSURE_TRUE(channel, 0);
  // Spawn a thread to send the data
  if (!mInternalIOThread) {
    nsresult res = NS_NewThread(getter_AddRefs(mInternalIOThread));
    if (NS_FAILED(res)) {
      return -1;
    }
  }

  nsCOMPtr<nsIRunnable> runnable = new ReadBlobRunnable(this, stream, aBlob);
  mInternalIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  return 0;
}

void
DataChannelConnection::ReadBlob(already_AddRefed<DataChannelConnection> aThis,
                                uint16_t aStream, nsIInputStream* aBlob)
{
  // NOTE: 'aThis' has been forgotten by the caller to avoid releasing
  // it off mainthread; if PeerConnectionImpl has released then we want
  // ~DataChannelConnection() to run on MainThread

  // XXX to do this safely, we must enqueue these atomically onto the
  // output socket.  We need a sender thread(s?) to enque data into the
  // socket and to avoid main-thread IO that might block.  Even on a
  // background thread, we may not want to block on one stream's data.
  // I.e. run non-blocking and service multiple channels.

  // For now as a hack, send as a single blast of queued packets which may
  // be deferred until buffer space is available.
  nsCString temp;
  uint64_t len;
  nsCOMPtr<nsIThread> mainThread;
  NS_GetMainThread(getter_AddRefs(mainThread));

  if (NS_FAILED(aBlob->Available(&len)) ||
      NS_FAILED(NS_ReadInputStreamToString(aBlob, temp, len))) {
    // Bug 966602:  Doesn't return an error to the caller via onerror.
    // We must release DataChannelConnection on MainThread to avoid issues (bug 876167)
    NS_ProxyRelease(mainThread, aThis.take());
    return;
  }
  aBlob->Close();
  RUN_ON_THREAD(mainThread, WrapRunnable(nsRefPtr<DataChannelConnection>(aThis),
                               &DataChannelConnection::SendBinaryMsg,
                               aStream, temp),
                NS_DISPATCH_NORMAL);
}

void
DataChannelConnection::GetStreamIds(std::vector<uint16_t>* aStreamList)
{
  ASSERT_WEBRTC(NS_IsMainThread());
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    if (mStreams[i]) {
      aStreamList->push_back(mStreams[i]->mStream);
    }
  }
}

int32_t
DataChannelConnection::SendMsgCommon(uint16_t stream, const nsACString &aMsg,
                                     bool isBinary)
{
  ASSERT_WEBRTC(NS_IsMainThread());
  // We really could allow this from other threads, so long as we deal with
  // asynchronosity issues with channels closing, in particular access to
  // mStreams, and issues with the association closing (access to mSocket).

  const char *data = aMsg.BeginReading();
  uint32_t len     = aMsg.Length();
  DataChannel *channel;

  LOG(("Sending %sto stream %u: %u bytes", isBinary ? "binary " : "", stream, len));
  // XXX if we want more efficiency, translate flags once at open time
  channel = mStreams[stream];
  NS_ENSURE_TRUE(channel, 0);

  if (isBinary)
    return SendBinary(channel, data, len,
                      DATA_CHANNEL_PPID_BINARY, DATA_CHANNEL_PPID_BINARY_LAST);
  return SendBinary(channel, data, len,
                    DATA_CHANNEL_PPID_DOMSTRING, DATA_CHANNEL_PPID_DOMSTRING_LAST);
}

void
DataChannelConnection::Close(DataChannel *aChannel)
{
  MutexAutoLock lock(mLock);
  CloseInt(aChannel);
}

// So we can call Close() with the lock already held
// Called from someone who holds a ref via ::Close(), or from ~DataChannel
void
DataChannelConnection::CloseInt(DataChannel *aChannel)
{
  MOZ_ASSERT(aChannel);
  nsRefPtr<DataChannel> channel(aChannel); // make sure it doesn't go away on us

  mLock.AssertCurrentThreadOwns();
  LOG(("Connection %p/Channel %p: Closing stream %u",
       channel->mConnection.get(), channel.get(), channel->mStream));
  // re-test since it may have closed before the lock was grabbed
  if (aChannel->mState == CLOSED || aChannel->mState == CLOSING) {
    LOG(("Channel already closing/closed (%u)", aChannel->mState));
    if (mState == CLOSED && channel->mStream != INVALID_STREAM) {
      // called from CloseAll()
      // we're not going to hang around waiting any more
      mStreams[channel->mStream] = nullptr;
    }
    return;
  }
  aChannel->mBufferedData.Clear();
  if (channel->mStream != INVALID_STREAM) {
    ResetOutgoingStream(channel->mStream);
    if (mState == CLOSED) { // called from CloseAll()
      // Let resets accumulate then send all at once in CloseAll()
      // we're not going to hang around waiting
      mStreams[channel->mStream] = nullptr;
    } else {
      SendOutgoingStreamReset();
    }
  }
  aChannel->mState = CLOSING;
  if (mState == CLOSED) {
    // we're not going to hang around waiting
    channel->Destroy();
  }
  // At this point when we leave here, the object is a zombie held alive only by the DOM object
}

void DataChannelConnection::CloseAll()
{
  LOG(("Closing all channels (connection %p)", (void*) this));
  // Don't need to lock here

  // Make sure no more channels will be opened
  {
    MutexAutoLock lock(mLock);
    mState = CLOSED;
  }

  // Close current channels
  // If there are runnables, they hold a strong ref and keep the channel
  // and/or connection alive (even if in a CLOSED state)
  bool closed_some = false;
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    if (mStreams[i]) {
      mStreams[i]->Close();
      closed_some = true;
    }
  }

  // Clean up any pending opens for channels
  nsRefPtr<DataChannel> channel;
  while (nullptr != (channel = dont_AddRef(static_cast<DataChannel *>(mPending.PopFront())))) {
    LOG(("closing pending channel %p, stream %u", channel.get(), channel->mStream));
    channel->Close(); // also releases the ref on each iteration
    closed_some = true;
  }
  // It's more efficient to let the Resets queue in shutdown and then
  // SendOutgoingStreamReset() here.
  if (closed_some) {
    MutexAutoLock lock(mLock);
    SendOutgoingStreamReset();
  }
}

DataChannel::~DataChannel()
{
  // NS_ASSERTION since this is more "I think I caught all the cases that
  // can cause this" than a true kill-the-program assertion.  If this is
  // wrong, nothing bad happens.  A worst it's a leak.
  NS_ASSERTION(mState == CLOSED || mState == CLOSING, "unexpected state in ~DataChannel");
}

void
DataChannel::Close()
{
  ENSURE_DATACONNECTION;
  mConnection->Close(this);
}

// Used when disconnecting from the DataChannelConnection
void
DataChannel::Destroy()
{
  ENSURE_DATACONNECTION;

  LOG(("Destroying Data channel %u", mStream));
  MOZ_ASSERT_IF(mStream != INVALID_STREAM,
                !mConnection->FindChannelByStream(mStream));
  mStream = INVALID_STREAM;
  mState = CLOSED;
  mConnection = nullptr;
}

void
DataChannel::SetListener(DataChannelListener *aListener, nsISupports *aContext)
{
  MutexAutoLock mLock(mListenerLock);
  mContext = aContext;
  mListener = aListener;
}

// May be called from another (i.e. Main) thread!
void
DataChannel::AppReady()
{
  ENSURE_DATACONNECTION;

  MutexAutoLock lock(mConnection->mLock);

  mReady = true;
  if (mState == WAITING_TO_OPEN) {
    mState = OPEN;
    NS_DispatchToMainThread(new DataChannelOnMessageAvailable(
                              DataChannelOnMessageAvailable::ON_CHANNEL_OPEN, mConnection,
                              this));
    for (uint32_t i = 0; i < mQueuedMessages.Length(); ++i) {
      nsCOMPtr<nsIRunnable> runnable = mQueuedMessages[i];
      MOZ_ASSERT(runnable);
      NS_DispatchToMainThread(runnable);
    }
  } else {
    NS_ASSERTION(mQueuedMessages.IsEmpty(), "Shouldn't have queued messages if not WAITING_TO_OPEN");
  }
  mQueuedMessages.Clear();
  mQueuedMessages.Compact();
  // We never use it again...  We could even allocate the array in the odd
  // cases we need it.
}

uint32_t
DataChannel::GetBufferedAmount()
{
  uint32_t buffered = 0;
  for (uint32_t i = 0; i < mBufferedData.Length(); ++i) {
    buffered += mBufferedData[i]->mLength;
  }
  return buffered;
}

// Called with mLock locked!
void
DataChannel::SendOrQueue(DataChannelOnMessageAvailable *aMessage)
{
  if (!mReady &&
      (mState == CONNECTING || mState == WAITING_TO_OPEN)) {
    mQueuedMessages.AppendElement(aMessage);
  } else {
    NS_DispatchToMainThread(aMessage);
  }
}

} // namespace mozilla
