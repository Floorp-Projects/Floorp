/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#if !defined(__Userspace_os_Windows)
#  include <arpa/inet.h>
#endif
// usrsctp.h expects to have errno definitions prior to its inclusion.
#include <errno.h>

#define SCTP_DEBUG 1
#define SCTP_STDINT_INCLUDE <stdint.h>

#ifdef _MSC_VER
// Disable "warning C4200: nonstandard extension used : zero-sized array in
//          struct/union"
// ...which the third-party file usrsctp.h runs afoul of.
#  pragma warning(push)
#  pragma warning(disable : 4200)
#endif

#include "usrsctp.h"

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#include "nsServiceManagerUtils.h"
#include "nsIInputStream.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "mozilla/Services.h"
#include "mozilla/Sprintf.h"
#include "nsProxyRelease.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/Unused.h"
#ifdef MOZ_PEERCONNECTION
#  include "mtransport/runnable_utils.h"
#  include "signaling/src/peerconnection/MediaTransportHandler.h"
#  include "mediapacket.h"
#endif

#include "DataChannel.h"
#include "DataChannelProtocol.h"

// Let us turn on and off important assertions in non-debug builds
#ifdef DEBUG
#  define ASSERT_WEBRTC(x) MOZ_ASSERT((x))
#elif defined(MOZ_WEBRTC_ASSERT_ALWAYS)
#  define ASSERT_WEBRTC(x) \
    do {                   \
      if (!(x)) {          \
        MOZ_CRASH();       \
      }                    \
    } while (0)
#endif

static bool sctp_initialized;

namespace mozilla {

LazyLogModule gDataChannelLog("DataChannel");
static LazyLogModule gSCTPLog("SCTP");

#define SCTP_LOG(args) \
  MOZ_LOG(mozilla::gSCTPLog, mozilla::LogLevel::Debug, args)

class DataChannelConnectionShutdown : public nsITimerCallback {
 public:
  explicit DataChannelConnectionShutdown(DataChannelConnection* aConnection)
      : mConnection(aConnection) {
    mTimer = NS_NewTimer();  // we'll crash if this fails
    mTimer->InitWithCallback(this, 30 * 1000, nsITimer::TYPE_ONE_SHOT);
  }

  NS_IMETHODIMP Notify(nsITimer* aTimer) override;

  NS_DECL_THREADSAFE_ISUPPORTS

 private:
  virtual ~DataChannelConnectionShutdown() { mTimer->Cancel(); }

  RefPtr<DataChannelConnection> mConnection;
  nsCOMPtr<nsITimer> mTimer;
};

class DataChannelShutdown;

StaticRefPtr<DataChannelShutdown> sDataChannelShutdown;

class DataChannelShutdown : public nsIObserver {
 public:
  // This needs to be tied to some object that is guaranteed to be
  // around (singleton likely) unless we want to shutdown sctp whenever
  // we're not using it (and in which case we'd keep a refcnt'd object
  // ref'd by each DataChannelConnection to release the SCTP usrlib via
  // sctp_finish). Right now, the single instance of this class is
  // owned by the observer service and a StaticRefPtr.

  NS_DECL_ISUPPORTS

  DataChannelShutdown() = default;

  void Init() {
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (!observerService) return;

    nsresult rv =
        observerService->AddObserver(this, "xpcom-will-shutdown", false);
    MOZ_ASSERT(rv == NS_OK);
    (void)rv;
  }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    // Note: MainThread
    if (strcmp(aTopic, "xpcom-will-shutdown") == 0) {
      DC_DEBUG(("Shutting down SCTP"));
      if (sctp_initialized) {
        usrsctp_finish();
        sctp_initialized = false;
      }
      nsCOMPtr<nsIObserverService> observerService =
          mozilla::services::GetObserverService();
      if (!observerService) return NS_ERROR_FAILURE;

      nsresult rv =
          observerService->RemoveObserver(this, "xpcom-will-shutdown");
      MOZ_ASSERT(rv == NS_OK);
      (void)rv;

      {
        StaticMutexAutoLock lock(sLock);
        sConnections = nullptr;  // clears as well
      }
      sDataChannelShutdown = nullptr;
    }
    return NS_OK;
  }

  void CreateConnectionShutdown(DataChannelConnection* aConnection) {
    StaticMutexAutoLock lock(sLock);
    if (!sConnections) {
      sConnections = new nsTArray<RefPtr<DataChannelConnectionShutdown>>();
    }
    sConnections->AppendElement(new DataChannelConnectionShutdown(aConnection));
  }

  void RemoveConnectionShutdown(
      DataChannelConnectionShutdown* aConnectionShutdown) {
    StaticMutexAutoLock lock(sLock);
    if (sConnections) {
      sConnections->RemoveElement(aConnectionShutdown);
    }
  }

 private:
  // The only instance of DataChannelShutdown is owned by the observer
  // service, so there is no need to call RemoveObserver here.
  virtual ~DataChannelShutdown() = default;

  // protects sConnections
  static StaticMutex sLock;
  static StaticAutoPtr<nsTArray<RefPtr<DataChannelConnectionShutdown>>>
      sConnections;
};

StaticMutex DataChannelShutdown::sLock;
StaticAutoPtr<nsTArray<RefPtr<DataChannelConnectionShutdown>>>
    DataChannelShutdown::sConnections;

NS_IMPL_ISUPPORTS(DataChannelShutdown, nsIObserver);

NS_IMPL_ISUPPORTS(DataChannelConnectionShutdown, nsITimerCallback)

NS_IMETHODIMP
DataChannelConnectionShutdown::Notify(nsITimer* aTimer) {
  // safely release reference to ourself
  RefPtr<DataChannelConnectionShutdown> grip(this);
  // Might not be set. We don't actually use the |this| pointer in
  // RemoveConnectionShutdown right now, which makes this a bit gratuitous
  // anyway...
  if (sDataChannelShutdown) {
    sDataChannelShutdown->RemoveConnectionShutdown(this);
  }
  return NS_OK;
}

OutgoingMsg::OutgoingMsg(struct sctp_sendv_spa& info, const uint8_t* data,
                         size_t length)
    : mLength(length), mData(data) {
  mInfo = &info;
  mPos = 0;
}

void OutgoingMsg::Advance(size_t offset) {
  mPos += offset;
  if (mPos > mLength) {
    mPos = mLength;
  }
}

BufferedOutgoingMsg::BufferedOutgoingMsg(OutgoingMsg& msg) {
  size_t length = msg.GetLeft();
  auto* tmp = new uint8_t[length];  // infallible malloc!
  memcpy(tmp, msg.GetData(), length);
  mLength = length;
  mData = tmp;
  mInfo = new sctp_sendv_spa;
  *mInfo = msg.GetInfo();
  mPos = 0;
}

BufferedOutgoingMsg::~BufferedOutgoingMsg() {
  delete mInfo;
  delete[] mData;
}

static int receive_cb(struct socket* sock, union sctp_sockstore addr,
                      void* data, size_t datalen, struct sctp_rcvinfo rcv,
                      int flags, void* ulp_info) {
  DataChannelConnection* connection =
      static_cast<DataChannelConnection*>(ulp_info);
  return connection->ReceiveCallback(sock, data, datalen, rcv, flags);
}

static DataChannelConnection* GetConnectionFromSocket(struct socket* sock) {
  struct sockaddr* addrs = nullptr;
  int naddrs = usrsctp_getladdrs(sock, 0, &addrs);
  if (naddrs <= 0 || addrs[0].sa_family != AF_CONN) {
    return nullptr;
  }
  // usrsctp_getladdrs() returns the addresses bound to this socket, which
  // contains the SctpDataMediaChannel* as sconn_addr.  Read the pointer,
  // then free the list of addresses once we have the pointer.  We only open
  // AF_CONN sockets, and they should all have the sconn_addr set to the
  // pointer that created them, so [0] is as good as any other.
  struct sockaddr_conn* sconn =
      reinterpret_cast<struct sockaddr_conn*>(&addrs[0]);
  DataChannelConnection* connection =
      reinterpret_cast<DataChannelConnection*>(sconn->sconn_addr);
  usrsctp_freeladdrs(addrs);

  return connection;
}

// called when the buffer empties to the threshold value
static int threshold_event(struct socket* sock, uint32_t sb_free) {
  DataChannelConnection* connection = GetConnectionFromSocket(sock);
  if (connection) {
    connection->SendDeferredMessages();
  } else {
    DC_ERROR(("Can't find connection for socket %p", sock));
  }
  return 0;
}

static void debug_printf(const char* format, ...) {
  va_list ap;
  char buffer[1024];

  if (MOZ_LOG_TEST(gSCTPLog, LogLevel::Debug)) {
    va_start(ap, format);
#ifdef _WIN32
    if (vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, ap) > 0) {
#else
    if (VsprintfLiteral(buffer, format, ap) > 0) {
#endif
      SCTP_LOG(("%s", buffer));
    }
    va_end(ap);
  }
}

DataChannelConnection::~DataChannelConnection() {
  DC_DEBUG(("Deleting DataChannelConnection %p", (void*)this));
  // This may die on the MainThread, or on the STS thread
  ASSERT_WEBRTC(mState == CLOSED);
  MOZ_ASSERT(!mMasterSocket);
  MOZ_ASSERT(mPending.GetSize() == 0);

  if (!IsSTSThread()) {
    ASSERT_WEBRTC(NS_IsMainThread());

    if (mInternalIOThread) {
      // Avoid spinning the event thread from here (which if we're mainthread
      // is in the event loop already)
      nsCOMPtr<nsIRunnable> r = WrapRunnable(
          nsCOMPtr<nsIThread>(mInternalIOThread), &nsIThread::AsyncShutdown);
      Dispatch(r.forget());
    }
  } else {
    // on STS, safe to call shutdown
    if (mInternalIOThread) {
      mInternalIOThread->Shutdown();
    }
  }
}

void DataChannelConnection::Destroy() {
  // Though it's probably ok to do this and close the sockets;
  // if we really want it to do true clean shutdowns it can
  // create a dependant Internal object that would remain around
  // until the network shut down the association or timed out.
  DC_DEBUG(("Destroying DataChannelConnection %p", (void*)this));
  ASSERT_WEBRTC(NS_IsMainThread());
  CloseAll();

  MutexAutoLock lock(mLock);
  // If we had a pending reset, we aren't waiting for it - clear the list so
  // we can deregister this DataChannelConnection without leaking.
  ClearResets();

  MOZ_ASSERT(mSTS);
  ASSERT_WEBRTC(NS_IsMainThread());
  mListener = nullptr;
  // Finish Destroy on STS thread to avoid bug 876167 - once that's fixed,
  // the usrsctp_close() calls can move back here (and just proxy the
  // disconnect_all())
  RUN_ON_THREAD(mSTS,
                WrapRunnable(RefPtr<DataChannelConnection>(this),
                             &DataChannelConnection::DestroyOnSTS, mSocket,
                             mMasterSocket),
                NS_DISPATCH_NORMAL);

  // These will be released on STS
  mSocket = nullptr;
  mMasterSocket = nullptr;  // also a flag that we've Destroyed this connection

  // We can't get any more new callbacks from the SCTP library
  // All existing callbacks have refs to DataChannelConnection

  // nsDOMDataChannel objects have refs to DataChannels that have refs to us
}

void DataChannelConnection::DestroyOnSTS(struct socket* aMasterSocket,
                                         struct socket* aSocket) {
  if (aSocket && aSocket != aMasterSocket) usrsctp_close(aSocket);
  if (aMasterSocket) usrsctp_close(aMasterSocket);

  usrsctp_deregister_address(static_cast<void*>(this));
  DC_DEBUG(("Deregistered %p from the SCTP stack.", static_cast<void*>(this)));
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  mShutdown = true;
#endif

  disconnect_all();
  mTransportHandler = nullptr;

  // we may have queued packet sends on STS after this; dispatch to ourselves
  // before finishing here so we can be sure there aren't anymore runnables
  // active that can try to touch the flow. DON'T use RUN_ON_THREAD, it
  // queue-jumps!
  mSTS->Dispatch(WrapRunnable(RefPtr<DataChannelConnection>(this),
                              &DataChannelConnection::DestroyOnSTSFinal),
                 NS_DISPATCH_NORMAL);
}

void DataChannelConnection::DestroyOnSTSFinal() {
  sDataChannelShutdown->CreateConnectionShutdown(this);
}

Maybe<RefPtr<DataChannelConnection>> DataChannelConnection::Create(
    DataChannelConnection::DataConnectionListener* aListener,
    nsIEventTarget* aTarget, MediaTransportHandler* aHandler,
    const uint16_t aLocalPort, const uint16_t aNumStreams,
    const Maybe<uint64_t>& aMaxMessageSize) {
  ASSERT_WEBRTC(NS_IsMainThread());

  RefPtr<DataChannelConnection> connection = new DataChannelConnection(
      aListener, aTarget, aHandler);  // Walks into a bar
  return connection->Init(aLocalPort, aNumStreams, aMaxMessageSize)
             ? Some(connection)
             : Nothing();
}

DataChannelConnection::DataChannelConnection(
    DataChannelConnection::DataConnectionListener* aListener,
    nsIEventTarget* aTarget, MediaTransportHandler* aHandler)
    : NeckoTargetHolder(aTarget),
      mLock("netwerk::sctp::DataChannelConnection"),
      mListener(aListener),
      mTransportHandler(aHandler) {
  DC_VERBOSE(("Constructor DataChannelConnection=%p, listener=%p", this,
              mListener.get()));
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  mShutdown = false;
#endif
}

bool DataChannelConnection::Init(const uint16_t aLocalPort,
                                 const uint16_t aNumStreams,
                                 const Maybe<uint64_t>& aMaxMessageSize) {
  ASSERT_WEBRTC(NS_IsMainThread());

  struct sctp_initmsg initmsg;
  struct sctp_assoc_value av;
  struct sctp_event event;
  socklen_t len;

  uint16_t event_types[] = {
      SCTP_ASSOC_CHANGE,          SCTP_PEER_ADDR_CHANGE,
      SCTP_REMOTE_ERROR,          SCTP_SHUTDOWN_EVENT,
      SCTP_ADAPTATION_INDICATION, SCTP_PARTIAL_DELIVERY_EVENT,
      SCTP_SEND_FAILED_EVENT,     SCTP_STREAM_RESET_EVENT,
      SCTP_STREAM_CHANGE_EVENT};
  {
    // MutexAutoLock lock(mLock); Not needed since we're on mainthread always
    mLocalPort = aLocalPort;
    SetMaxMessageSize(aMaxMessageSize.isSome(), aMaxMessageSize.valueOr(0));

    if (!sctp_initialized) {
      DC_DEBUG(("sctp_init"));
#ifdef MOZ_PEERCONNECTION
      usrsctp_init(0, DataChannelConnection::SctpDtlsOutput, debug_printf);
#else
      MOZ_CRASH("Trying to use SCTP/DTLS without mtransport");
#endif

      // Set logging to SCTP:LogLevel::Debug to get SCTP debugs
      if (MOZ_LOG_TEST(gSCTPLog, LogLevel::Debug)) {
        usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);
      }

      // Do not send ABORTs in response to INITs (1).
      // Do not send ABORTs for received Out of the Blue packets (2).
      usrsctp_sysctl_set_sctp_blackhole(2);

      // Disable the Explicit Congestion Notification extension (currently not
      // supported by the Firefox code)
      usrsctp_sysctl_set_sctp_ecn_enable(0);

      // Enable interleaving messages for different streams (incoming)
      // See: https://tools.ietf.org/html/rfc6458#section-8.1.20
      usrsctp_sysctl_set_sctp_default_frag_interleave(2);

      sctp_initialized = true;

      sDataChannelShutdown = new DataChannelShutdown();
      sDataChannelShutdown->Init();
    }
  }

  // XXX FIX! make this a global we get once
  // Find the STS thread
  nsresult rv;
  mSTS = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Open sctp with a callback
  if ((mMasterSocket = usrsctp_socket(
           AF_CONN, SOCK_STREAM, IPPROTO_SCTP, receive_cb, threshold_event,
           usrsctp_sysctl_get_sctp_sendspace() / 2, this)) == nullptr) {
    return false;
  }

  int buf_size = 1024 * 1024;

  if (usrsctp_setsockopt(mMasterSocket, SOL_SOCKET, SO_RCVBUF,
                         (const void*)&buf_size, sizeof(buf_size)) < 0) {
    DC_ERROR(("Couldn't change receive buffer size on SCTP socket"));
    goto error_cleanup;
  }
  if (usrsctp_setsockopt(mMasterSocket, SOL_SOCKET, SO_SNDBUF,
                         (const void*)&buf_size, sizeof(buf_size)) < 0) {
    DC_ERROR(("Couldn't change send buffer size on SCTP socket"));
    goto error_cleanup;
  }

  // Make non-blocking for bind/connect.  SCTP over UDP defaults to non-blocking
  // in associations for normal IO
  if (usrsctp_set_non_blocking(mMasterSocket, 1) < 0) {
    DC_ERROR(("Couldn't set non_blocking on SCTP socket"));
    // We can't handle connect() safely if it will block, not that this will
    // even happen.
    goto error_cleanup;
  }

  // Make sure when we close the socket, make sure it doesn't call us back
  // again! This would cause it try to use an invalid DataChannelConnection
  // pointer
  struct linger l;
  l.l_onoff = 1;
  l.l_linger = 0;
  if (usrsctp_setsockopt(mMasterSocket, SOL_SOCKET, SO_LINGER, (const void*)&l,
                         (socklen_t)sizeof(struct linger)) < 0) {
    DC_ERROR(("Couldn't set SO_LINGER on SCTP socket"));
    // unsafe to allow it to continue if this fails
    goto error_cleanup;
  }

  // XXX Consider disabling this when we add proper SDP negotiation.
  // We may want to leave enabled for supporting 'cloning' of SDP offers, which
  // implies re-use of the same pseudo-port number, or forcing a renegotiation.
  {
    const int option_value = 1;
    if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_REUSE_PORT,
                           (const void*)&option_value,
                           (socklen_t)sizeof(option_value)) < 0) {
      DC_WARN(("Couldn't set SCTP_REUSE_PORT on SCTP socket"));
    }
    if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_NODELAY,
                           (const void*)&option_value,
                           (socklen_t)sizeof(option_value)) < 0) {
      DC_WARN(("Couldn't set SCTP_NODELAY on SCTP socket"));
    }
  }

  // Set explicit EOR
  {
    const int option_value = 1;
    if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_EXPLICIT_EOR,
                           (const void*)&option_value,
                           (socklen_t)sizeof(option_value)) < 0) {
      DC_ERROR(("*** failed to enable explicit EOR mode %d", errno));
      goto error_cleanup;
    }
  }

  // Enable ndata
  // TODO: Bug 1381145, enable this once ndata has been deployed
#if 0
  av.assoc_id = SCTP_FUTURE_ASSOC;
  av.assoc_value = 1;
  if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_INTERLEAVING_SUPPORTED, &av,
                         (socklen_t)sizeof(struct sctp_assoc_value)) < 0) {
    DC_ERROR(("*** failed enable ndata errno %d", errno));
    goto error_cleanup;
  }
#endif

  av.assoc_id = SCTP_ALL_ASSOC;
  av.assoc_value = SCTP_ENABLE_RESET_STREAM_REQ | SCTP_ENABLE_CHANGE_ASSOC_REQ;
  if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET,
                         &av, (socklen_t)sizeof(struct sctp_assoc_value)) < 0) {
    DC_ERROR(("*** failed enable stream reset errno %d", errno));
    goto error_cleanup;
  }

  /* Enable the events of interest. */
  memset(&event, 0, sizeof(event));
  event.se_assoc_id = SCTP_ALL_ASSOC;
  event.se_on = 1;
  for (unsigned short event_type : event_types) {
    event.se_type = event_type;
    if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_EVENT, &event,
                           sizeof(event)) < 0) {
      DC_ERROR(("*** failed setsockopt SCTP_EVENT errno %d", errno));
      goto error_cleanup;
    }
  }

  memset(&initmsg, 0, sizeof(initmsg));
  len = sizeof(initmsg);
  if (usrsctp_getsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg,
                         &len) < 0) {
    DC_ERROR(("*** failed getsockopt SCTP_INITMSG"));
    goto error_cleanup;
  }
  DC_DEBUG(("Setting number of SCTP streams to %u, was %u/%u", aNumStreams,
            initmsg.sinit_num_ostreams, initmsg.sinit_max_instreams));
  initmsg.sinit_num_ostreams = aNumStreams;
  initmsg.sinit_max_instreams = MAX_NUM_STREAMS;
  if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg,
                         (socklen_t)sizeof(initmsg)) < 0) {
    DC_ERROR(("*** failed setsockopt SCTP_INITMSG, errno %d", errno));
    goto error_cleanup;
  }

  mSocket = nullptr;
  usrsctp_register_address(static_cast<void*>(this));
  DC_DEBUG(("Registered %p within the SCTP stack.", static_cast<void*>(this)));
  return true;

error_cleanup:
  usrsctp_close(mMasterSocket);
  mMasterSocket = nullptr;
  return false;
}

void DataChannelConnection::SetMaxMessageSize(bool aMaxMessageSizeSet,
                                              uint64_t aMaxMessageSize) {
  MutexAutoLock lock(mLock);  // TODO: Needed?

  if (mMaxMessageSizeSet && !aMaxMessageSizeSet) {
    // Don't overwrite already set MMS with default values
    return;
  }

  mMaxMessageSizeSet = aMaxMessageSizeSet;
  mMaxMessageSize = aMaxMessageSize;

  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs =
      do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (!NS_WARN_IF(NS_FAILED(rv))) {
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);

    if (branch) {
      int32_t temp;
      if (!NS_FAILED(branch->GetIntPref(
              "media.peerconnection.sctp.force_maximum_message_size", &temp))) {
        if (temp >= 0) {
          mMaxMessageSize = (uint64_t)temp;
        }
      }
    }
  }

  // Fix remote MMS. This code exists, so future implementations of
  // RTCSctpTransport.maxMessageSize can simply provide that value from
  // GetMaxMessageSize.

  // TODO: Bug 1382779, once resolved, can be increased to
  // min(Uint8ArrayMaxSize, UINT32_MAX)
  // TODO: Bug 1381146, once resolved, can be increased to whatever we support
  // then (hopefully
  //       SIZE_MAX)
  if (mMaxMessageSize == 0 ||
      mMaxMessageSize > WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_REMOTE) {
    mMaxMessageSize = WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_REMOTE;
  }

  DC_DEBUG(("Maximum message size (outgoing data): %" PRIu64
            " (set=%s, enforced=%s)",
            mMaxMessageSize, mMaxMessageSizeSet ? "yes" : "no",
            aMaxMessageSize != mMaxMessageSize ? "yes" : "no"));
}

uint64_t DataChannelConnection::GetMaxMessageSize() { return mMaxMessageSize; }

#ifdef MOZ_PEERCONNECTION

bool DataChannelConnection::ConnectToTransport(const std::string& aTransportId,
                                               const bool aClient,
                                               const uint16_t aLocalPort,
                                               const uint16_t aRemotePort) {
  MutexAutoLock lock(mLock);

  MOZ_ASSERT(mMasterSocket,
             "SCTP wasn't initialized before ConnectToTransport!");
  static const auto paramString =
      [](const std::string& tId, const Maybe<bool>& client,
         const uint16_t localPort, const uint16_t remotePort) -> std::string {
    std::ostringstream stream;
    stream << "Transport ID: '" << tId << "', Role: '"
           << (client ? (client.value() ? "client" : "server") : "")
           << "', Local Port: '" << localPort << "', Remote Port: '"
           << remotePort << "'";
    return stream.str();
  };

  const auto params =
      paramString(aTransportId, Some(aClient), aLocalPort, aRemotePort);
  DC_DEBUG(("ConnectToTransport connecting DTLS transport with parameters: %s",
            params.c_str()));

  const auto currentReadyState = GetReadyState();
  if (currentReadyState == OPEN) {
    if (aTransportId == mTransportId && mAllocateEven.isSome() &&
        mAllocateEven.value() == aClient && mLocalPort == aLocalPort &&
        mRemotePort == aRemotePort) {
      DC_WARN(
          ("Skipping attempt to connect to an already OPEN transport with "
           "identical parameters."));
      return true;
    }
    DC_WARN(
        ("Attempting to connect to an already OPEN transport, because "
         "different parameters were provided."));
    DC_WARN(("Original transport parameters: %s",
             paramString(mTransportId, mAllocateEven, mLocalPort, aRemotePort)
                 .c_str()));
    DC_WARN(("New transport parameters: %s", params.c_str()));
  }
  if (NS_WARN_IF(aTransportId.empty())) {
    return false;
  }

  mLocalPort = aLocalPort;
  mRemotePort = aRemotePort;
  SetReadyState(CONNECTING);
  mAllocateEven = Some(aClient);

  // Could be faster. Probably doesn't matter.
  while (auto channel = mChannels.Get(INVALID_STREAM)) {
    mChannels.Remove(channel);
    channel->mStream = FindFreeStream();
    if (channel->mStream != INVALID_STREAM) {
      mChannels.Insert(channel);
    }
  }
  RUN_ON_THREAD(mSTS,
                WrapRunnable(RefPtr<DataChannelConnection>(this),
                             &DataChannelConnection::SetSignals, aTransportId),
                NS_DISPATCH_NORMAL);
  return true;
}

void DataChannelConnection::SetSignals(const std::string& aTransportId) {
  ASSERT_WEBRTC(IsSTSThread());
  mTransportId = aTransportId;
  mTransportHandler->SignalPacketReceived.connect(
      this, &DataChannelConnection::SctpDtlsInput);
  mTransportHandler->SignalStateChange.connect(
      this, &DataChannelConnection::TransportStateChange);
  // SignalStateChange() doesn't call you with the initial state
  if (mTransportHandler->GetState(mTransportId, false) ==
      TransportLayer::TS_OPEN) {
    DC_DEBUG(("Setting transport signals, dtls already open"));
    CompleteConnect();
  } else {
    DC_DEBUG(("Setting transport signals, dtls not open yet"));
  }
}

void DataChannelConnection::TransportStateChange(
    const std::string& aTransportId, TransportLayer::State aState) {
  if (aTransportId == mTransportId) {
    if (aState == TransportLayer::TS_OPEN) {
      DC_DEBUG(("Transport is open!"));
      CompleteConnect();
    } else if (aState == TransportLayer::TS_CLOSED ||
               aState == TransportLayer::TS_NONE ||
               aState == TransportLayer::TS_ERROR) {
      DC_DEBUG(("Transport is closed!"));
      Stop();
    }
  }
}

void DataChannelConnection::CompleteConnect() {
  MutexAutoLock lock(mLock);

  DC_DEBUG(("dtls open"));
  ASSERT_WEBRTC(IsSTSThread());
  if (!mMasterSocket) {
    return;
  }

  struct sockaddr_conn addr;
  memset(&addr, 0, sizeof(addr));
  addr.sconn_family = AF_CONN;
#  if defined(__Userspace_os_Darwin)
  addr.sconn_len = sizeof(addr);
#  endif
  addr.sconn_port = htons(mLocalPort);
  addr.sconn_addr = static_cast<void*>(this);

  DC_DEBUG(("Calling usrsctp_bind"));
  int r = usrsctp_bind(mMasterSocket, reinterpret_cast<struct sockaddr*>(&addr),
                       sizeof(addr));
  if (r < 0) {
    DC_ERROR(("usrsctp_bind failed: %d", r));
  } else {
    // This is the remote addr
    addr.sconn_port = htons(mRemotePort);
    DC_DEBUG(("Calling usrsctp_connect"));
    r = usrsctp_connect(
        mMasterSocket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (r >= 0 || errno == EINPROGRESS) {
      struct sctp_paddrparams paddrparams;
      socklen_t opt_len;

      memset(&paddrparams, 0, sizeof(struct sctp_paddrparams));
      memcpy(&paddrparams.spp_address, &addr, sizeof(struct sockaddr_conn));
      opt_len = (socklen_t)sizeof(struct sctp_paddrparams);
      r = usrsctp_getsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
                             &paddrparams, &opt_len);
      if (r < 0) {
        DC_ERROR(("usrsctp_getsockopt failed: %d", r));
      } else {
        // draft-ietf-rtcweb-data-channel-13 section 5: max initial MTU IPV4
        // 1200, IPV6 1280
        paddrparams.spp_pathmtu = 1200;  // safe for either
        paddrparams.spp_flags &= ~SPP_PMTUD_ENABLE;
        paddrparams.spp_flags |= SPP_PMTUD_DISABLE;
        opt_len = (socklen_t)sizeof(struct sctp_paddrparams);
        r = usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP,
                               SCTP_PEER_ADDR_PARAMS, &paddrparams, opt_len);
        if (r < 0) {
          DC_ERROR(("usrsctp_getsockopt failed: %d", r));
        } else {
          DC_ERROR(("usrsctp: PMTUD disabled, MTU set to %u",
                    paddrparams.spp_pathmtu));
        }
      }
    }
    if (r < 0) {
      if (errno == EINPROGRESS) {
        // non-blocking
        return;
      }
      DC_ERROR(("usrsctp_connect failed: %d", errno));
      SetReadyState(CLOSED);
    } else {
      // We fire ON_CONNECTION via SCTP_COMM_UP when we get that
      return;
    }
  }
  // Note: currently this doesn't actually notify the application
  Dispatch(do_AddRef(new DataChannelOnMessageAvailable(
      DataChannelOnMessageAvailable::ON_CONNECTION, this)));
}

// Process any pending Opens
void DataChannelConnection::ProcessQueuedOpens() {
  // The nsDeque holds channels with an AddRef applied.  Another reference
  // (may) be held by the DOMDataChannel, unless it's been GC'd.  No other
  // references should exist.

  // Can't copy nsDeque's.  Move into temp array since any that fail will
  // go back to mPending
  nsDeque temp;
  DataChannel* temp_channel;  // really already_AddRefed<>
  while (nullptr !=
         (temp_channel = static_cast<DataChannel*>(mPending.PopFront()))) {
    temp.Push(static_cast<void*>(temp_channel));
  }

  RefPtr<DataChannel> channel;
  // All these entries have an AddRef(); make that explicit now via the
  // dont_AddRef()
  while (nullptr !=
         (channel = dont_AddRef(static_cast<DataChannel*>(temp.PopFront())))) {
    if (channel->mFlags & DATA_CHANNEL_FLAGS_FINISH_OPEN) {
      DC_DEBUG(("Processing queued open for %p (%u)", channel.get(),
                channel->mStream));
      channel->mFlags &= ~DATA_CHANNEL_FLAGS_FINISH_OPEN;
      // OpenFinish returns a reference itself, so we need to take it can
      // Release it
      channel = OpenFinish(channel.forget());  // may reset the flag and re-push
    } else {
      NS_ASSERTION(
          false,
          "How did a DataChannel get queued without the FINISH_OPEN flag?");
    }
  }
}

void DataChannelConnection::SctpDtlsInput(const std::string& aTransportId,
                                          const MediaPacket& packet) {
  if ((packet.type() != MediaPacket::SCTP) || (mTransportId != aTransportId)) {
    return;
  }

  if (MOZ_LOG_TEST(gSCTPLog, LogLevel::Debug)) {
    char* buf;

    if ((buf = usrsctp_dumppacket((void*)packet.data(), packet.len(),
                                  SCTP_DUMP_INBOUND)) != nullptr) {
      SCTP_LOG(("%s", buf));
      usrsctp_freedumpbuffer(buf);
    }
  }
  // Pass the data to SCTP
  MutexAutoLock lock(mLock);
  usrsctp_conninput(static_cast<void*>(this), packet.data(), packet.len(), 0);
}

void DataChannelConnection::SendPacket(std::unique_ptr<MediaPacket>&& packet) {
  mSTS->Dispatch(NS_NewRunnableFunction(
      "DataChannelConnection::SendPacket",
      [this, self = RefPtr<DataChannelConnection>(this),
       packet = std::move(packet)]() mutable {
        // DC_DEBUG(("%p: SCTP/DTLS sent %ld bytes", this, len));
        if (!mTransportId.empty() && mTransportHandler) {
          mTransportHandler->SendPacket(mTransportId, std::move(*packet));
        }
      }));
}

/* static */
int DataChannelConnection::SctpDtlsOutput(void* addr, void* buffer,
                                          size_t length, uint8_t tos,
                                          uint8_t set_df) {
  DataChannelConnection* peer = static_cast<DataChannelConnection*>(addr);
  MOZ_DIAGNOSTIC_ASSERT(!peer->mShutdown);

  if (MOZ_LOG_TEST(gSCTPLog, LogLevel::Debug)) {
    char* buf;

    if ((buf = usrsctp_dumppacket(buffer, length, SCTP_DUMP_OUTBOUND)) !=
        nullptr) {
      SCTP_LOG(("%s", buf));
      usrsctp_freedumpbuffer(buf);
    }
  }

  // We're async proxying even if on the STSThread because this is called
  // with internal SCTP locks held in some cases (such as in usrsctp_connect()).
  // SCTP has an option for Apple, on IP connections only, to release at least
  // one of the locks before calling a packet output routine; with changes to
  // the underlying SCTP stack this might remove the need to use an async proxy.
  std::unique_ptr<MediaPacket> packet(new MediaPacket);
  packet->SetType(MediaPacket::SCTP);
  packet->Copy(static_cast<const uint8_t*>(buffer), length);

  if (NS_IsMainThread() && peer->mDeferSend) {
    peer->mDeferredSend.emplace_back(std::move(packet));
    return 0;
  }

  peer->SendPacket(std::move(packet));
  return 0;  // cheat!  Packets can always be dropped later anyways
}
#endif

#ifdef ALLOW_DIRECT_SCTP_LISTEN_CONNECT
// listen for incoming associations
// Blocks! - Don't call this from main thread!

bool DataChannelConnection::Listen(unsigned short port) {
  struct sockaddr_in addr;
  socklen_t addr_len;

  NS_WARNING_ASSERTION(!NS_IsMainThread(),
                       "Blocks, do not call from main thread!!!");

  /* Acting as the 'server' */
  memset((void*)&addr, 0, sizeof(addr));
#  ifdef HAVE_SIN_LEN
  addr.sin_len = sizeof(struct sockaddr_in);
#  endif
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  DC_DEBUG(("Waiting for connections on port %u", ntohs(addr.sin_port)));
  {
    MutexAutoLock lock(mLock);
    SetReadyState(CONNECTING);
  }
  if (usrsctp_bind(mMasterSocket, reinterpret_cast<struct sockaddr*>(&addr),
                   sizeof(struct sockaddr_in)) < 0) {
    DC_ERROR(("***Failed userspace_bind"));
    return false;
  }
  if (usrsctp_listen(mMasterSocket, 1) < 0) {
    DC_ERROR(("***Failed userspace_listen"));
    return false;
  }

  DC_DEBUG(("Accepting connection"));
  addr_len = 0;
  if ((mSocket = usrsctp_accept(mMasterSocket, nullptr, &addr_len)) ==
      nullptr) {
    DC_ERROR(("***Failed accept"));
    return false;
  }

  {
    MutexAutoLock lock(mLock);
    SetReadyState(OPEN);
  }

  struct linger l;
  l.l_onoff = 1;
  l.l_linger = 0;
  if (usrsctp_setsockopt(mSocket, SOL_SOCKET, SO_LINGER, (const void*)&l,
                         (socklen_t)sizeof(struct linger)) < 0) {
    DC_WARN(("Couldn't set SO_LINGER on SCTP socket"));
  }

  // Notify Connection open
  // XXX We need to make sure connection sticks around until the message is
  // delivered
  DC_DEBUG(("%s: sending ON_CONNECTION for %p", __FUNCTION__, this));
  Dispatch(do_AddRef(new DataChannelOnMessageAvailable(
      DataChannelOnMessageAvailable::ON_CONNECTION, this,
      (DataChannel*)nullptr)));
  return true;
}

// Blocks! - Don't call this from main thread!
bool DataChannelConnection::Connect(const char* addr, unsigned short port) {
  struct sockaddr_in addr4;
  struct sockaddr_in6 addr6;

  NS_WARNING_ASSERTION(!NS_IsMainThread(),
                       "Blocks, do not call from main thread!!!");

  /* Acting as the connector */
  DC_DEBUG(("Connecting to %s, port %u", addr, port));
  memset((void*)&addr4, 0, sizeof(struct sockaddr_in));
  memset((void*)&addr6, 0, sizeof(struct sockaddr_in6));
#  ifdef HAVE_SIN_LEN
  addr4.sin_len = sizeof(struct sockaddr_in);
#  endif
#  ifdef HAVE_SIN6_LEN
  addr6.sin6_len = sizeof(struct sockaddr_in6);
#  endif
  addr4.sin_family = AF_INET;
  addr6.sin6_family = AF_INET6;
  addr4.sin_port = htons(port);
  addr6.sin6_port = htons(port);
  {
    MutexAutoLock lock(mLock);
    SetReadyState(CONNECTING);
  }
#  if !defined(__Userspace_os_Windows)
  if (inet_pton(AF_INET6, addr, &addr6.sin6_addr) == 1) {
    if (usrsctp_connect(mMasterSocket,
                        reinterpret_cast<struct sockaddr*>(&addr6),
                        sizeof(struct sockaddr_in6)) < 0) {
      DC_ERROR(("*** Failed userspace_connect"));
      return false;
    }
  } else if (inet_pton(AF_INET, addr, &addr4.sin_addr) == 1) {
    if (usrsctp_connect(mMasterSocket,
                        reinterpret_cast<struct sockaddr*>(&addr4),
                        sizeof(struct sockaddr_in)) < 0) {
      DC_ERROR(("*** Failed userspace_connect"));
      return false;
    }
  } else {
    DC_ERROR(("*** Illegal destination address."));
  }
#  else
  {
    struct sockaddr_storage ss;
    int sslen = sizeof(ss);

    if (!WSAStringToAddressA(const_cast<char*>(addr), AF_INET6, nullptr,
                             (struct sockaddr*)&ss, &sslen)) {
      addr6.sin6_addr =
          (reinterpret_cast<struct sockaddr_in6*>(&ss))->sin6_addr;
      if (usrsctp_connect(mMasterSocket,
                          reinterpret_cast<struct sockaddr*>(&addr6),
                          sizeof(struct sockaddr_in6)) < 0) {
        DC_ERROR(("*** Failed userspace_connect"));
        return false;
      }
    } else if (!WSAStringToAddressA(const_cast<char*>(addr), AF_INET, nullptr,
                                    (struct sockaddr*)&ss, &sslen)) {
      addr4.sin_addr = (reinterpret_cast<struct sockaddr_in*>(&ss))->sin_addr;
      if (usrsctp_connect(mMasterSocket,
                          reinterpret_cast<struct sockaddr*>(&addr4),
                          sizeof(struct sockaddr_in)) < 0) {
        DC_ERROR(("*** Failed userspace_connect"));
        return false;
      }
    } else {
      DC_ERROR(("*** Illegal destination address."));
    }
  }
#  endif

  mSocket = mMasterSocket;

  DC_DEBUG(("connect() succeeded!  Entering connected mode"));
  {
    MutexAutoLock lock(mLock);
    SetReadyState(OPEN);
  }
  // Notify Connection open
  // XXX We need to make sure connection sticks around until the message is
  // delivered
  DC_DEBUG(("%s: sending ON_CONNECTION for %p", __FUNCTION__, this));
  Dispatch(do_AddRef(new DataChannelOnMessageAvailable(
      DataChannelOnMessageAvailable::ON_CONNECTION, this,
      (DataChannel*)nullptr)));
  return true;
}
#endif

DataChannel* DataChannelConnection::FindChannelByStream(uint16_t stream) {
  return mChannels.Get(stream).get();
}

uint16_t DataChannelConnection::FindFreeStream() {
  ASSERT_WEBRTC(NS_IsMainThread());
  uint16_t i, limit;

  limit = MAX_NUM_STREAMS;

  MOZ_ASSERT(mAllocateEven.isSome());
  for (i = (*mAllocateEven ? 0 : 1); i < limit; i += 2) {
    if (mChannels.Get(i)) {
      continue;
    }

    // Verify it's not still in the process of closing
    size_t j;
    for (j = 0; j < mStreamsResetting.Length(); ++j) {
      if (mStreamsResetting[j] == i) {
        break;
      }
    }

    if (j == mStreamsResetting.Length()) {
      return i;
    }
  }
  return INVALID_STREAM;
}

uint32_t DataChannelConnection::UpdateCurrentStreamIndex() {
  RefPtr<DataChannel> channel = mChannels.GetNextChannel(mCurrentStream);
  if (!channel) {
    mCurrentStream = 0;
  } else {
    mCurrentStream = channel->mStream;
  }
  return mCurrentStream;
}

uint32_t DataChannelConnection::GetCurrentStreamIndex() {
  return mCurrentStream;
}

bool DataChannelConnection::RequestMoreStreams(int32_t aNeeded) {
  struct sctp_status status;
  struct sctp_add_streams sas;
  uint32_t outStreamsNeeded;
  socklen_t len;

  if (aNeeded + mNegotiatedIdLimit > MAX_NUM_STREAMS) {
    aNeeded = MAX_NUM_STREAMS - mNegotiatedIdLimit;
  }
  if (aNeeded <= 0) {
    return false;
  }

  len = (socklen_t)sizeof(struct sctp_status);
  if (usrsctp_getsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_STATUS, &status,
                         &len) < 0) {
    DC_ERROR(("***failed: getsockopt SCTP_STATUS"));
    return false;
  }
  outStreamsNeeded = aNeeded;  // number to add

  // Note: if multiple channel opens happen when we don't have enough space,
  // we'll call RequestMoreStreams() multiple times
  memset(&sas, 0, sizeof(sas));
  sas.sas_instrms = 0;
  sas.sas_outstrms = (uint16_t)outStreamsNeeded; /* XXX error handling */
  // Doesn't block, we get an event when it succeeds or fails
  if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_ADD_STREAMS, &sas,
                         (socklen_t)sizeof(struct sctp_add_streams)) < 0) {
    if (errno == EALREADY) {
      DC_DEBUG(("Already have %u output streams", outStreamsNeeded));
      return true;
    }

    DC_ERROR(("***failed: setsockopt ADD errno=%d", errno));
    return false;
  }
  DC_DEBUG(("Requested %u more streams", outStreamsNeeded));
  // We add to mNegotiatedIdLimit when we get a SCTP_STREAM_CHANGE_EVENT and the
  // values are larger than mNegotiatedIdLimit
  return true;
}

// Returns a POSIX error code.
int DataChannelConnection::SendControlMessage(const uint8_t* data, uint32_t len,
                                              uint16_t stream) {
  struct sctp_sendv_spa info = {0};

  // General flags
  info.sendv_flags = SCTP_SEND_SNDINFO_VALID;

  // Set stream identifier, protocol identifier and flags
  info.sendv_sndinfo.snd_sid = stream;
  info.sendv_sndinfo.snd_flags = SCTP_EOR;
  info.sendv_sndinfo.snd_ppid = htonl(DATA_CHANNEL_PPID_CONTROL);

  // Create message instance and send
  // Note: Main-thread IO, but doesn't block
#if (UINT32_MAX > SIZE_MAX)
  if (len > SIZE_MAX) {
    return EMSGSIZE;
  }
#endif
  OutgoingMsg msg(info, data, (size_t)len);
  bool buffered;
  int error = SendMsgInternalOrBuffer(mBufferedControl, msg, buffered, nullptr);

  // Set pending type (if buffered)
  if (!error && buffered && !mPendingType) {
    mPendingType = PENDING_DCEP;
  }
  return error;
}

// Returns a POSIX error code.
int DataChannelConnection::SendOpenAckMessage(uint16_t stream) {
  struct rtcweb_datachannel_ack ack;

  memset(&ack, 0, sizeof(struct rtcweb_datachannel_ack));
  ack.msg_type = DATA_CHANNEL_ACK;

  return SendControlMessage((const uint8_t*)&ack, sizeof(ack), stream);
}

// Returns a POSIX error code.
int DataChannelConnection::SendOpenRequestMessage(
    const nsACString& label, const nsACString& protocol, uint16_t stream,
    bool unordered, uint16_t prPolicy, uint32_t prValue) {
  const int label_len = label.Length();     // not including nul
  const int proto_len = protocol.Length();  // not including nul
  // careful - request struct include one char for the label
  const int req_size = sizeof(struct rtcweb_datachannel_open_request) - 1 +
                       label_len + proto_len;
  UniqueFreePtr<struct rtcweb_datachannel_open_request> req(
      (struct rtcweb_datachannel_open_request*)moz_xmalloc(req_size));

  memset(req.get(), 0, req_size);
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
      return EINVAL;
  }
  if (unordered) {
    // Per the current types, all differ by 0x80 between ordered and unordered
    req->channel_type |=
        0x80;  // NOTE: be careful if new types are added in the future
  }

  req->reliability_param = htonl(prValue);
  req->priority = htons(0); /* XXX: add support */
  req->label_length = htons(label_len);
  req->protocol_length = htons(proto_len);
  memcpy(&req->label[0], PromiseFlatCString(label).get(), label_len);
  memcpy(&req->label[label_len], PromiseFlatCString(protocol).get(), proto_len);

  // TODO: req_size is an int... that looks hairy
  int error = SendControlMessage((const uint8_t*)req.get(), req_size, stream);
  return error;
}

// XXX This should use a separate thread (outbound queue) which should
// select() to know when to *try* to send data to the socket again.
// Alternatively, it can use a timeout, but that's guaranteed to be wrong
// (just not sure in what direction).  We could re-implement NSPR's
// PR_POLL_WRITE/etc handling... with a lot of work.

// Better yet, use the SCTP stack's notifications on buffer state to avoid
// filling the SCTP's buffers.

// returns if we're still blocked (true)
bool DataChannelConnection::SendDeferredMessages() {
  RefPtr<DataChannel> channel;  // we may null out the refs to this

  // This may block while something is modifying channels, but should not block
  // for IO
  ASSERT_WEBRTC(!NS_IsMainThread());
  mLock.AssertCurrentThreadOwns();

  DC_DEBUG(("SendDeferredMessages called, pending type: %d", mPendingType));
  if (!mPendingType) {
    return false;
  }

  // Send pending control messages
  // Note: If ndata is not active, check if DCEP messages are currently
  // outstanding. These need to
  //       be sent first before other streams can be used for sending.
  if (!mBufferedControl.IsEmpty() &&
      (mSendInterleaved || mPendingType == PENDING_DCEP)) {
    if (SendBufferedMessages(mBufferedControl, nullptr)) {
      return true;
    }

    // Note: There may or may not be pending data messages
    mPendingType = PENDING_DATA;
  }

  bool blocked = false;
  uint32_t i = GetCurrentStreamIndex();
  uint32_t end = i;
  do {
    channel = mChannels.Get(i);
    // Should already be cleared if closing/closed
    if (!channel || channel->mBufferedData.IsEmpty()) {
      i = UpdateCurrentStreamIndex();
      continue;
    }

    // Send buffered data messages
    // Warning: This will fail in case ndata is inactive and a previously
    //          deallocated data channel has not been closed properly. If you
    //          ever see that no messages can be sent on any channel, this is
    //          likely the cause (an explicit EOR message partially sent whose
    //          remaining chunks are still being waited for).
    size_t written = 0;
    mDeferSend = true;
    blocked = SendBufferedMessages(channel->mBufferedData, &written);
    mDeferSend = false;
    if (written) {
      channel->DecrementBufferedAmount(written);
    }

    for (auto&& packet : mDeferredSend) {
      MOZ_ASSERT(written);
      SendPacket(std::move(packet));
    }
    mDeferredSend.clear();

    // Update current stream index
    // Note: If ndata is not active, the outstanding data messages on this
    //       stream need to be sent first before other streams can be used for
    //       sending.
    if (mSendInterleaved || !blocked) {
      i = UpdateCurrentStreamIndex();
    }
  } while (!blocked && i != end);

  if (!blocked) {
    mPendingType = mBufferedControl.IsEmpty() ? PENDING_NONE : PENDING_DCEP;
  }
  return blocked;
}

// Called with mLock locked!
// buffer MUST have at least one item!
// returns if we're still blocked (true)
bool DataChannelConnection::SendBufferedMessages(
    nsTArray<UniquePtr<BufferedOutgoingMsg>>& buffer, size_t* aWritten) {
  do {
    // Re-send message
    int error = SendMsgInternal(*buffer[0], aWritten);
    switch (error) {
      case 0:
        buffer.RemoveElementAt(0);
        break;
      case EAGAIN:
#if (EAGAIN != EWOULDBLOCK)
      case EWOULDBLOCK:
#endif
        return true;
      default:
        buffer.RemoveElementAt(0);
        DC_ERROR(("error on sending: %d", error));
        break;
    }
  } while (!buffer.IsEmpty());

  return false;
}

// Caller must ensure that length <= SIZE_MAX
void DataChannelConnection::HandleOpenRequestMessage(
    const struct rtcweb_datachannel_open_request* req, uint32_t length,
    uint16_t stream) {
  RefPtr<DataChannel> channel;
  uint32_t prValue;
  uint16_t prPolicy;

  ASSERT_WEBRTC(!NS_IsMainThread());
  mLock.AssertCurrentThreadOwns();

  const size_t requiredLength = (sizeof(*req) - 1) + ntohs(req->label_length) +
                                ntohs(req->protocol_length);
  if (((size_t)length) != requiredLength) {
    if (((size_t)length) < requiredLength) {
      DC_ERROR(
          ("%s: insufficient length: %u, should be %zu. Unable to continue.",
           __FUNCTION__, length, requiredLength));
      return;
    }
    DC_WARN(("%s: Inconsistent length: %u, should be %zu", __FUNCTION__, length,
             requiredLength));
  }

  DC_DEBUG(("%s: length %u, sizeof(*req) = %zu", __FUNCTION__, length,
            sizeof(*req)));

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
      DC_ERROR(("Unknown channel type %d", req->channel_type));
      /* XXX error handling */
      return;
  }
  prValue = ntohl(req->reliability_param);
  bool ordered = !(req->channel_type & 0x80);

  if ((channel = FindChannelByStream(stream))) {
    if (!channel->mNegotiated) {
      DC_ERROR(
          ("HandleOpenRequestMessage: channel for pre-existing stream "
           "%u that was not externally negotiated. JS is lying to us, or "
           "there's an id collision.",
           stream));
      /* XXX: some error handling */
    } else {
      DC_DEBUG(("Open for externally negotiated channel %u", stream));
      // XXX should also check protocol, maybe label
      if (prPolicy != channel->mPrPolicy || prValue != channel->mPrValue ||
          ordered != channel->mOrdered) {
        DC_WARN(
            ("external negotiation mismatch with OpenRequest:"
             "channel %u, policy %u/%u, value %u/%u, ordered %d/%d",
             stream, prPolicy, channel->mPrPolicy, prValue, channel->mPrValue,
             static_cast<int>(ordered), static_cast<int>(channel->mOrdered)));
      }
    }
    return;
  }
  if (stream >= mNegotiatedIdLimit) {
    DC_ERROR(("%s: stream %u out of bounds (%zu)", __FUNCTION__, stream,
              mNegotiatedIdLimit));
    return;
  }

  nsCString label(
      nsDependentCSubstring(&req->label[0], ntohs(req->label_length)));
  nsCString protocol(nsDependentCSubstring(
      &req->label[ntohs(req->label_length)], ntohs(req->protocol_length)));

  channel =
      new DataChannel(this, stream, DataChannel::OPEN, label, protocol,
                      prPolicy, prValue, ordered, false, nullptr, nullptr);
  mChannels.Insert(channel);

  DC_DEBUG(("%s: sending ON_CHANNEL_CREATED for %s/%s: %u", __FUNCTION__,
            channel->mLabel.get(), channel->mProtocol.get(), stream));
  Dispatch(do_AddRef(new DataChannelOnMessageAvailable(
      DataChannelOnMessageAvailable::ON_CHANNEL_CREATED, this, channel)));

  DC_DEBUG(("%s: deferring sending ON_CHANNEL_OPEN for %p", __FUNCTION__,
            channel.get()));
  channel->AnnounceOpen();

  // Note that any message can be buffered; SendOpenAckMessage may
  // error later than this check.
  const auto error = SendOpenAckMessage(channel->mStream);
  if (error) {
    DC_ERROR(("SendOpenRequest failed, error = %d", error));
    Dispatch(NS_NewRunnableFunction(
        "DataChannelConnection::HandleOpenRequestMessage",
        [channel, connection = RefPtr<DataChannelConnection>(this)]() {
          MutexAutoLock mLock(connection->mLock);
          // Close the channel on failure
          connection->CloseInt(channel);
        }));
    return;
  }
  DeliverQueuedData(channel->mStream);
}

// NOTE: the updated spec from the IETF says we should set in-order until we
// receive an ACK. That would make this code moot.  Keep it for now for
// backwards compatibility.
void DataChannelConnection::DeliverQueuedData(uint16_t stream) {
  mLock.AssertCurrentThreadOwns();

  uint32_t i = 0;
  while (i < mQueuedData.Length()) {
    // Careful! we may modify the array length from within the loop!
    if (mQueuedData[i]->mStream == stream) {
      DC_DEBUG(("Delivering queued data for stream %u, length %u", stream,
                mQueuedData[i]->mLength));
      // Deliver the queued data
      HandleDataMessage(mQueuedData[i]->mData, mQueuedData[i]->mLength,
                        mQueuedData[i]->mPpid, mQueuedData[i]->mStream,
                        mQueuedData[i]->mFlags);
      mQueuedData.RemoveElementAt(i);
      continue;  // don't bump index since we removed the element
    }
    i++;
  }
}

// Caller must ensure that length <= SIZE_MAX
void DataChannelConnection::HandleOpenAckMessage(
    const struct rtcweb_datachannel_ack* ack, uint32_t length,
    uint16_t stream) {
  DataChannel* channel;

  mLock.AssertCurrentThreadOwns();

  channel = FindChannelByStream(stream);
  if (NS_WARN_IF(!channel)) {
    return;
  }

  DC_DEBUG(("OpenAck received for stream %u, waiting=%d", stream,
            (channel->mFlags & DATA_CHANNEL_FLAGS_WAITING_ACK) ? 1 : 0));

  channel->mFlags &= ~DATA_CHANNEL_FLAGS_WAITING_ACK;
}

// Caller must ensure that length <= SIZE_MAX
void DataChannelConnection::HandleUnknownMessage(uint32_t ppid, uint32_t length,
                                                 uint16_t stream) {
  /* XXX: Send an error message? */
  DC_ERROR(("unknown DataChannel message received: %u, len %u on stream %d",
            ppid, length, stream));
  // XXX Log to JS error console if possible
}

uint8_t DataChannelConnection::BufferMessage(nsACString& recvBuffer,
                                             const void* data, uint32_t length,
                                             uint32_t ppid, int flags) {
  const char* buffer = (const char*)data;
  uint8_t bufferFlags = 0;

  if ((flags & MSG_EOR) && ppid != DATA_CHANNEL_PPID_BINARY_PARTIAL &&
      ppid != DATA_CHANNEL_PPID_DOMSTRING_PARTIAL) {
    bufferFlags |= DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_COMPLETE;

    // Return directly if nothing has been buffered
    if (recvBuffer.IsEmpty()) {
      return bufferFlags;
    }
  }

  // Ensure it doesn't blow up our buffer
  // TODO: Change 'WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_LOCAL' to whatever the
  //       new buffer is capable of holding.
  if (((uint64_t)recvBuffer.Length()) + ((uint64_t)length) >
      WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_LOCAL) {
    bufferFlags |= DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_TOO_LARGE;
    return bufferFlags;
  }

  // Copy & add to receive buffer
  recvBuffer.Append(buffer, length);
  bufferFlags |= DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_BUFFERED;
  return bufferFlags;
}

void DataChannelConnection::HandleDataMessage(const void* data, size_t length,
                                              uint32_t ppid, uint16_t stream,
                                              int flags) {
  DataChannel* channel;
  const char* buffer = (const char*)data;

  mLock.AssertCurrentThreadOwns();
  channel = FindChannelByStream(stream);

  // Note: Until we support SIZE_MAX sized messages, we need this check
#if (SIZE_MAX > UINT32_MAX)
  if (length > UINT32_MAX) {
    DC_ERROR(("DataChannel: Cannot handle message of size %zu (max=%" PRIu32
              ")",
              length, UINT32_MAX));
    CloseInt(channel);
    return;
  }
#endif
  uint32_t data_length = (uint32_t)length;

  // XXX A closed channel may trip this... check
  // NOTE: the updated spec from the IETF says we should set in-order until we
  // receive an ACK. That would make this code moot.  Keep it for now for
  // backwards compatibility.
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
    DC_DEBUG(("Queuing data for stream %u, length %u", stream, data_length));
    // Copies data
    mQueuedData.AppendElement(
        new QueuedDataMessage(stream, ppid, flags, data, data_length));
    return;
  }

  bool is_binary = true;
  uint8_t bufferFlags;
  int32_t type;
  const char* info = "";

  if (ppid == DATA_CHANNEL_PPID_DOMSTRING_PARTIAL ||
      ppid == DATA_CHANNEL_PPID_DOMSTRING) {
    is_binary = false;
  }
  if (is_binary != channel->mIsRecvBinary && !channel->mRecvBuffer.IsEmpty()) {
    NS_WARNING("DataChannel message aborted by fragment type change!");
    // TODO: Maybe closing would be better as this is a hard to detect protocol
    // violation?
    channel->mRecvBuffer.Truncate(0);
  }
  channel->mIsRecvBinary = is_binary;

  // Remaining chunks of previously truncated message (due to the buffer being
  // full)?
  if (channel->mFlags & DATA_CHANNEL_FLAGS_CLOSING_TOO_LARGE) {
    DC_ERROR(
        ("DataChannel: Ignoring partial message of length %u, buffer full and "
         "closing",
         data_length));
    // Only unblock if unordered
    if (!channel->mOrdered && (flags & MSG_EOR)) {
      channel->mFlags &= ~DATA_CHANNEL_FLAGS_CLOSING_TOO_LARGE;
    }
  }

  // Buffer message until complete
  bufferFlags =
      BufferMessage(channel->mRecvBuffer, buffer, data_length, ppid, flags);
  if (bufferFlags & DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_TOO_LARGE) {
    DC_ERROR(
        ("DataChannel: Buffered message would become too large to handle, "
         "closing channel"));
    channel->mRecvBuffer.Truncate(0);
    channel->mFlags |= DATA_CHANNEL_FLAGS_CLOSING_TOO_LARGE;
    CloseInt(channel);
    return;
  }
  if (!(bufferFlags & DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_COMPLETE)) {
    DC_DEBUG(
        ("DataChannel: Partial %s message of length %u (total %u) on channel "
         "id %u",
         is_binary ? "binary" : "string", data_length,
         channel->mRecvBuffer.Length(), channel->mStream));
    return;  // Not ready to notify application
  }
  if (bufferFlags & DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_BUFFERED) {
    data_length = channel->mRecvBuffer.Length();
  }

  // Complain about large messages (only complain - we can handle it)
  if (data_length > WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_LOCAL) {
    DC_WARN(
        ("DataChannel: Received message of length %u is > announced maximum "
         "message size (%u)",
         data_length, WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_LOCAL));
  }

  switch (ppid) {
    case DATA_CHANNEL_PPID_DOMSTRING:
      DC_DEBUG(
          ("DataChannel: Received string message of length %u on channel %u",
           data_length, channel->mStream));
      type = DataChannelOnMessageAvailable::ON_DATA_STRING;
      if (bufferFlags & DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_BUFFERED) {
        info = " (string fragmented)";
      }
      // else send using recvData normally

      // WebSockets checks IsUTF8() here; we can try to deliver it
      break;

    case DATA_CHANNEL_PPID_BINARY:
      DC_DEBUG(
          ("DataChannel: Received binary message of length %u on channel id %u",
           data_length, channel->mStream));
      type = DataChannelOnMessageAvailable::ON_DATA_BINARY;
      if (bufferFlags & DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_BUFFERED) {
        info = " (binary fragmented)";
      }

      // else send using recvData normally
      break;

    default:
      NS_ERROR("Unknown data PPID");
      DC_ERROR(("Unknown data PPID %" PRIu32, ppid));
      return;
  }

  // Notify onmessage
  DC_DEBUG(("%s: sending ON_DATA_%s%s for %p", __FUNCTION__,
            (type == DataChannelOnMessageAvailable::ON_DATA_STRING) ? "STRING"
                                                                    : "BINARY",
            info, channel));
  if (bufferFlags & DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_BUFFERED) {
    channel->SendOrQueue(new DataChannelOnMessageAvailable(
        type, this, channel, channel->mRecvBuffer));
    channel->mRecvBuffer.Truncate(0);
  } else {
    nsAutoCString recvData(buffer, data_length);  // copies (<64) or allocates
    channel->SendOrQueue(
        new DataChannelOnMessageAvailable(type, this, channel, recvData));
  }
}

void DataChannelConnection::HandleDCEPMessage(const void* buffer, size_t length,
                                              uint32_t ppid, uint16_t stream,
                                              int flags) {
  const struct rtcweb_datachannel_open_request* req;
  const struct rtcweb_datachannel_ack* ack;

  // Note: Until we support SIZE_MAX sized messages, we need this check
#if (SIZE_MAX > UINT32_MAX)
  if (length > UINT32_MAX) {
    DC_ERROR(("DataChannel: Cannot handle message of size %zu (max=%u)", length,
              UINT32_MAX));
    Stop();
    return;
  }
#endif
  uint32_t data_length = (uint32_t)length;

  mLock.AssertCurrentThreadOwns();

  // Buffer message until complete
  const uint8_t bufferFlags =
      BufferMessage(mRecvBuffer, buffer, data_length, ppid, flags);
  if (bufferFlags & DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_TOO_LARGE) {
    DC_ERROR(
        ("DataChannel: Buffered message would become too large to handle, "
         "closing connection"));
    mRecvBuffer.Truncate(0);
    Stop();
    return;
  }
  if (!(bufferFlags & DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_COMPLETE)) {
    DC_DEBUG(("Buffered partial DCEP message of length %u", data_length));
    return;
  }
  if (bufferFlags & DATA_CHANNEL_BUFFER_MESSAGE_FLAGS_BUFFERED) {
    buffer = reinterpret_cast<const void*>(mRecvBuffer.BeginReading());
    data_length = mRecvBuffer.Length();
  }

  req = static_cast<const struct rtcweb_datachannel_open_request*>(buffer);
  DC_DEBUG(("Handling DCEP message of length %u", data_length));

  // Ensure minimum message size (ack is the smallest DCEP message)
  if ((size_t)data_length < sizeof(*ack)) {
    DC_WARN(("Ignored invalid DCEP message (too short)"));
    return;
  }

  switch (req->msg_type) {
    case DATA_CHANNEL_OPEN_REQUEST:
      // structure includes a possibly-unused char label[1] (in a packed
      // structure)
      if (NS_WARN_IF((size_t)data_length < sizeof(*req) - 1)) {
        return;
      }

      HandleOpenRequestMessage(req, data_length, stream);
      break;
    case DATA_CHANNEL_ACK:
      // >= sizeof(*ack) checked above

      ack = static_cast<const struct rtcweb_datachannel_ack*>(buffer);
      HandleOpenAckMessage(ack, data_length, stream);
      break;
    default:
      HandleUnknownMessage(ppid, data_length, stream);
      break;
  }

  // Reset buffer
  mRecvBuffer.Truncate(0);
}

// Called with mLock locked!
void DataChannelConnection::HandleMessage(const void* buffer, size_t length,
                                          uint32_t ppid, uint16_t stream,
                                          int flags) {
  mLock.AssertCurrentThreadOwns();

  switch (ppid) {
    case DATA_CHANNEL_PPID_CONTROL:
      HandleDCEPMessage(buffer, length, ppid, stream, flags);
      break;
    case DATA_CHANNEL_PPID_DOMSTRING_PARTIAL:
    case DATA_CHANNEL_PPID_DOMSTRING:
    case DATA_CHANNEL_PPID_BINARY_PARTIAL:
    case DATA_CHANNEL_PPID_BINARY:
      HandleDataMessage(buffer, length, ppid, stream, flags);
      break;
    default:
      DC_ERROR((
          "Unhandled message of length %zu PPID %u on stream %u received (%s).",
          length, ppid, stream, (flags & MSG_EOR) ? "complete" : "partial"));
      break;
  }
}

void DataChannelConnection::HandleAssociationChangeEvent(
    const struct sctp_assoc_change* sac) {
  mLock.AssertCurrentThreadOwns();

  uint32_t i, n;
  const auto readyState = GetReadyState();
  switch (sac->sac_state) {
    case SCTP_COMM_UP:
      DC_DEBUG(("Association change: SCTP_COMM_UP"));
      if (readyState == CONNECTING) {
        mSocket = mMasterSocket;
        SetReadyState(OPEN);

        DC_DEBUG(("Negotiated number of incoming streams: %" PRIu16,
                  sac->sac_inbound_streams));
        DC_DEBUG(("Negotiated number of outgoing streams: %" PRIu16,
                  sac->sac_outbound_streams));
        mNegotiatedIdLimit =
            std::max(mNegotiatedIdLimit,
                     static_cast<size_t>(std::max(sac->sac_outbound_streams,
                                                  sac->sac_inbound_streams)));

        Dispatch(do_AddRef(new DataChannelOnMessageAvailable(
            DataChannelOnMessageAvailable::ON_CONNECTION, this)));
        DC_DEBUG(("DTLS connect() succeeded!  Entering connected mode"));

        // Open any streams pending...
        ProcessQueuedOpens();

      } else if (readyState == OPEN) {
        DC_DEBUG(("DataConnection Already OPEN"));
      } else {
        DC_ERROR(("Unexpected state: %d", readyState));
      }
      break;
    case SCTP_COMM_LOST:
      DC_DEBUG(("Association change: SCTP_COMM_LOST"));
      // This association is toast, so also close all the channels -- from
      // mainthread!
      Stop();
      break;
    case SCTP_RESTART:
      DC_DEBUG(("Association change: SCTP_RESTART"));
      break;
    case SCTP_SHUTDOWN_COMP:
      DC_DEBUG(("Association change: SCTP_SHUTDOWN_COMP"));
      Stop();
      break;
    case SCTP_CANT_STR_ASSOC:
      DC_DEBUG(("Association change: SCTP_CANT_STR_ASSOC"));
      break;
    default:
      DC_DEBUG(("Association change: UNKNOWN"));
      break;
  }
  DC_DEBUG(("Association change: streams (in/out) = (%u/%u)",
            sac->sac_inbound_streams, sac->sac_outbound_streams));

  if (NS_WARN_IF(!sac)) {
    return;
  }

  n = sac->sac_length - sizeof(*sac);
  if ((sac->sac_state == SCTP_COMM_UP) || (sac->sac_state == SCTP_RESTART)) {
    if (n > 0) {
      for (i = 0; i < n; ++i) {
        switch (sac->sac_info[i]) {
          case SCTP_ASSOC_SUPPORTS_PR:
            DC_DEBUG(("Supports: PR"));
            break;
          case SCTP_ASSOC_SUPPORTS_AUTH:
            DC_DEBUG(("Supports: AUTH"));
            break;
          case SCTP_ASSOC_SUPPORTS_ASCONF:
            DC_DEBUG(("Supports: ASCONF"));
            break;
          case SCTP_ASSOC_SUPPORTS_MULTIBUF:
            DC_DEBUG(("Supports: MULTIBUF"));
            break;
          case SCTP_ASSOC_SUPPORTS_RE_CONFIG:
            DC_DEBUG(("Supports: RE-CONFIG"));
            break;
#if defined(SCTP_ASSOC_SUPPORTS_INTERLEAVING)
          case SCTP_ASSOC_SUPPORTS_INTERLEAVING:
            DC_DEBUG(("Supports: NDATA"));
            // TODO: This should probably be set earlier above in 'case
            //       SCTP_COMM_UP' but we also need this for 'SCTP_RESTART'.
            mSendInterleaved = true;
            break;
#endif
          default:
            DC_ERROR(("Supports: UNKNOWN(0x%02x)", sac->sac_info[i]));
            break;
        }
      }
    }
  } else if (((sac->sac_state == SCTP_COMM_LOST) ||
              (sac->sac_state == SCTP_CANT_STR_ASSOC)) &&
             (n > 0)) {
    DC_DEBUG(("Association: ABORT ="));
    for (i = 0; i < n; ++i) {
      DC_DEBUG((" 0x%02x", sac->sac_info[i]));
    }
  }
  if ((sac->sac_state == SCTP_CANT_STR_ASSOC) ||
      (sac->sac_state == SCTP_SHUTDOWN_COMP) ||
      (sac->sac_state == SCTP_COMM_LOST)) {
    return;
  }
}

void DataChannelConnection::HandlePeerAddressChangeEvent(
    const struct sctp_paddr_change* spc) {
  const char* addr = "";
#if !defined(__Userspace_os_Windows)
  char addr_buf[INET6_ADDRSTRLEN];
  struct sockaddr_in* sin;
  struct sockaddr_in6* sin6;
#endif

  switch (spc->spc_aaddr.ss_family) {
    case AF_INET:
#if !defined(__Userspace_os_Windows)
      sin = (struct sockaddr_in*)&spc->spc_aaddr;
      addr = inet_ntop(AF_INET, &sin->sin_addr, addr_buf, INET6_ADDRSTRLEN);
#endif
      break;
    case AF_INET6:
#if !defined(__Userspace_os_Windows)
      sin6 = (struct sockaddr_in6*)&spc->spc_aaddr;
      addr = inet_ntop(AF_INET6, &sin6->sin6_addr, addr_buf, INET6_ADDRSTRLEN);
#endif
      break;
    case AF_CONN:
      addr = "DTLS connection";
      break;
    default:
      break;
  }
  DC_DEBUG(("Peer address %s is now ", addr));
  switch (spc->spc_state) {
    case SCTP_ADDR_AVAILABLE:
      DC_DEBUG(("SCTP_ADDR_AVAILABLE"));
      break;
    case SCTP_ADDR_UNREACHABLE:
      DC_DEBUG(("SCTP_ADDR_UNREACHABLE"));
      break;
    case SCTP_ADDR_REMOVED:
      DC_DEBUG(("SCTP_ADDR_REMOVED"));
      break;
    case SCTP_ADDR_ADDED:
      DC_DEBUG(("SCTP_ADDR_ADDED"));
      break;
    case SCTP_ADDR_MADE_PRIM:
      DC_DEBUG(("SCTP_ADDR_MADE_PRIM"));
      break;
    case SCTP_ADDR_CONFIRMED:
      DC_DEBUG(("SCTP_ADDR_CONFIRMED"));
      break;
    default:
      DC_ERROR(("UNKNOWN SCP STATE"));
      break;
  }
  if (spc->spc_error) {
    DC_ERROR((" (error = 0x%08x).\n", spc->spc_error));
  }
}

void DataChannelConnection::HandleRemoteErrorEvent(
    const struct sctp_remote_error* sre) {
  size_t i, n;

  n = sre->sre_length - sizeof(struct sctp_remote_error);
  DC_WARN(("Remote Error (error = 0x%04x): ", sre->sre_error));
  for (i = 0; i < n; ++i) {
    DC_WARN((" 0x%02x", sre->sre_data[i]));
  }
}

void DataChannelConnection::HandleShutdownEvent(
    const struct sctp_shutdown_event* sse) {
  DC_DEBUG(("Shutdown event."));
  /* XXX: notify all channels. */
  // Attempts to actually send anything will fail
}

void DataChannelConnection::HandleAdaptationIndication(
    const struct sctp_adaptation_event* sai) {
  DC_DEBUG(("Adaptation indication: %x.", sai->sai_adaptation_ind));
}

void DataChannelConnection::HandlePartialDeliveryEvent(
    const struct sctp_pdapi_event* spde) {
  // Note: Be aware that stream and sequence number being u32 instead of u16 is
  //       a bug in the SCTP API. This may change in the future.

  DC_DEBUG(("Partial delivery event: "));
  switch (spde->pdapi_indication) {
    case SCTP_PARTIAL_DELIVERY_ABORTED:
      DC_DEBUG(("delivery aborted "));
      break;
    default:
      DC_ERROR(("??? "));
      break;
  }
  DC_DEBUG(("(flags = %x), stream = %" PRIu32 ", sn = %" PRIu32,
            spde->pdapi_flags, spde->pdapi_stream, spde->pdapi_seq));

  // Validate stream ID
  if (spde->pdapi_stream >= UINT16_MAX) {
    DC_ERROR(("Invalid stream id in partial delivery event: %" PRIu32 "\n",
              spde->pdapi_stream));
    return;
  }

  // Find channel and reset buffer
  DataChannel* channel = FindChannelByStream((uint16_t)spde->pdapi_stream);
  if (channel) {
    DC_WARN(("Abort partially delivered message of %u bytes\n",
             channel->mRecvBuffer.Length()));
    channel->mRecvBuffer.Truncate(0);
  }
}

void DataChannelConnection::HandleSendFailedEvent(
    const struct sctp_send_failed_event* ssfe) {
  size_t i, n;

  if (ssfe->ssfe_flags & SCTP_DATA_UNSENT) {
    DC_DEBUG(("Unsent "));
  }
  if (ssfe->ssfe_flags & SCTP_DATA_SENT) {
    DC_DEBUG(("Sent "));
  }
  if (ssfe->ssfe_flags & ~(SCTP_DATA_SENT | SCTP_DATA_UNSENT)) {
    DC_DEBUG(("(flags = %x) ", ssfe->ssfe_flags));
  }
  DC_DEBUG(
      ("message with PPID = %u, SID = %d, flags: 0x%04x due to error = 0x%08x",
       ntohl(ssfe->ssfe_info.snd_ppid), ssfe->ssfe_info.snd_sid,
       ssfe->ssfe_info.snd_flags, ssfe->ssfe_error));
  n = ssfe->ssfe_length - sizeof(struct sctp_send_failed_event);
  for (i = 0; i < n; ++i) {
    DC_DEBUG((" 0x%02x", ssfe->ssfe_data[i]));
  }
}

void DataChannelConnection::ClearResets() {
  // Clear all pending resets
  if (!mStreamsResetting.IsEmpty()) {
    DC_DEBUG(("Clearing resets for %zu streams", mStreamsResetting.Length()));
  }

  for (uint32_t i = 0; i < mStreamsResetting.Length(); ++i) {
    RefPtr<DataChannel> channel;
    channel = FindChannelByStream(mStreamsResetting[i]);
    if (channel) {
      DC_DEBUG(("Forgetting channel %u (%p) with pending reset",
                channel->mStream, channel.get()));
      // TODO: Do we _really_ want to remove this? Are we allowed to reuse the
      // id?
      mChannels.Remove(channel);
    }
  }
  mStreamsResetting.Clear();
}

void DataChannelConnection::ResetOutgoingStream(uint16_t stream) {
  uint32_t i;

  mLock.AssertCurrentThreadOwns();
  DC_DEBUG(
      ("Connection %p: Resetting outgoing stream %u", (void*)this, stream));
  // Rarely has more than a couple items and only for a short time
  for (i = 0; i < mStreamsResetting.Length(); ++i) {
    if (mStreamsResetting[i] == stream) {
      return;
    }
  }
  mStreamsResetting.AppendElement(stream);
}

void DataChannelConnection::SendOutgoingStreamReset() {
  struct sctp_reset_streams* srs;
  uint32_t i;
  size_t len;

  DC_DEBUG(("Connection %p: Sending outgoing stream reset for %zu streams",
            (void*)this, mStreamsResetting.Length()));
  mLock.AssertCurrentThreadOwns();
  if (mStreamsResetting.IsEmpty()) {
    DC_DEBUG(("No streams to reset"));
    return;
  }
  len = sizeof(sctp_assoc_t) +
        (2 + mStreamsResetting.Length()) * sizeof(uint16_t);
  srs = static_cast<struct sctp_reset_streams*>(
      moz_xmalloc(len));  // infallible malloc
  memset(srs, 0, len);
  srs->srs_flags = SCTP_STREAM_RESET_OUTGOING;
  srs->srs_number_streams = mStreamsResetting.Length();
  for (i = 0; i < mStreamsResetting.Length(); ++i) {
    srs->srs_stream_list[i] = mStreamsResetting[i];
  }
  if (usrsctp_setsockopt(mMasterSocket, IPPROTO_SCTP, SCTP_RESET_STREAMS, srs,
                         (socklen_t)len) < 0) {
    DC_ERROR(("***failed: setsockopt RESET, errno %d", errno));
    // if errno == EALREADY, this is normal - we can't send another reset
    // with one pending.
    // When we get an incoming reset (which may be a response to our
    // outstanding one), see if we have any pending outgoing resets and
    // send them
  } else {
    mStreamsResetting.Clear();
  }
  free(srs);
}

void DataChannelConnection::HandleStreamResetEvent(
    const struct sctp_stream_reset_event* strrst) {
  uint32_t n, i;
  RefPtr<DataChannel> channel;  // since we may null out the ref to the channel

  if (!(strrst->strreset_flags & SCTP_STREAM_RESET_DENIED) &&
      !(strrst->strreset_flags & SCTP_STREAM_RESET_FAILED)) {
    n = (strrst->strreset_length - sizeof(struct sctp_stream_reset_event)) /
        sizeof(uint16_t);
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
          // 3. We've sent a open but haven't gotten a response yet (CONNECTING)
          //    I believe this is impossible, as we don't have an input stream
          //    yet.

          DC_DEBUG(("Incoming: Channel %u  closed", channel->mStream));
          if (mChannels.Remove(channel)) {
            // Mark the stream for reset (the reset is sent below)
            ResetOutgoingStream(channel->mStream);
          }

          DC_DEBUG(("Disconnected DataChannel %p from connection %p",
                    (void*)channel.get(), (void*)channel->mConnection.get()));
          channel->StreamClosedLocked();
        } else {
          DC_WARN(("Can't find incoming channel %d", i));
        }
      }
    }
  }

  // Process any pending resets now:
  if (!mStreamsResetting.IsEmpty()) {
    DC_DEBUG(("Sending %zu pending resets", mStreamsResetting.Length()));
    SendOutgoingStreamReset();
  }
}

void DataChannelConnection::HandleStreamChangeEvent(
    const struct sctp_stream_change_event* strchg) {
  ASSERT_WEBRTC(!NS_IsMainThread());
  if (strchg->strchange_flags == SCTP_STREAM_CHANGE_DENIED) {
    DC_ERROR(("*** Failed increasing number of streams from %zu (%u/%u)",
              mNegotiatedIdLimit, strchg->strchange_instrms,
              strchg->strchange_outstrms));
    // XXX FIX! notify pending opens of failure
    return;
  }
  if (strchg->strchange_instrms > mNegotiatedIdLimit) {
    DC_DEBUG(("Other side increased streams from %zu to %u", mNegotiatedIdLimit,
              strchg->strchange_instrms));
  }
  uint16_t old_limit = mNegotiatedIdLimit;
  uint16_t new_limit =
      std::max(strchg->strchange_outstrms, strchg->strchange_instrms);
  if (new_limit > mNegotiatedIdLimit) {
    DC_DEBUG(("Increasing number of streams from %u to %u - adding %u (in: %u)",
              old_limit, new_limit, new_limit - old_limit,
              strchg->strchange_instrms));
    // make sure both are the same length
    mNegotiatedIdLimit = new_limit;
    DC_DEBUG(("New length = %zu (was %d)", mNegotiatedIdLimit, old_limit));
    // Re-process any channels waiting for streams.
    // Linear search, but we don't increase channels often and
    // the array would only get long in case of an app error normally

    // Make sure we request enough streams if there's a big jump in streams
    // Could make a more complex API for OpenXxxFinish() and avoid this loop
    auto channels = mChannels.GetAll();
    size_t num_needed =
        channels.Length() ? (channels.LastElement()->mStream + 1) : 0;
    MOZ_ASSERT(num_needed != INVALID_STREAM);
    if (num_needed > new_limit) {
      int32_t more_needed = num_needed - ((int32_t)mNegotiatedIdLimit) + 16;
      DC_DEBUG(("Not enough new streams, asking for %d more", more_needed));
      // TODO: parameter is an int32_t but we pass size_t
      RequestMoreStreams(more_needed);
    } else if (strchg->strchange_outstrms < strchg->strchange_instrms) {
      DC_DEBUG(("Requesting %d output streams to match partner",
                strchg->strchange_instrms - strchg->strchange_outstrms));
      RequestMoreStreams(strchg->strchange_instrms -
                         strchg->strchange_outstrms);
    }

    ProcessQueuedOpens();
  }
  // else probably not a change in # of streams

  if ((strchg->strchange_flags & SCTP_STREAM_CHANGE_DENIED) ||
      (strchg->strchange_flags & SCTP_STREAM_CHANGE_FAILED)) {
    // Other side denied our request. Need to AnnounceClosed some stuff.
    for (auto& channel : mChannels.GetAll()) {
      if (channel->mStream >= mNegotiatedIdLimit) {
        /* XXX: Signal to the other end. */
        channel->AnnounceClosed();
        // maybe fire onError (bug 843625)
      }
    }
  }
}

// Called with mLock locked!
void DataChannelConnection::HandleNotification(
    const union sctp_notification* notif, size_t n) {
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
    case SCTP_AUTHENTICATION_EVENT:
      DC_DEBUG(("SCTP_AUTHENTICATION_EVENT"));
      break;
    case SCTP_SENDER_DRY_EVENT:
      // DC_DEBUG(("SCTP_SENDER_DRY_EVENT"));
      break;
    case SCTP_NOTIFICATIONS_STOPPED_EVENT:
      DC_DEBUG(("SCTP_NOTIFICATIONS_STOPPED_EVENT"));
      break;
    case SCTP_PARTIAL_DELIVERY_EVENT:
      HandlePartialDeliveryEvent(&(notif->sn_pdapi_event));
      break;
    case SCTP_SEND_FAILED_EVENT:
      HandleSendFailedEvent(&(notif->sn_send_failed_event));
      break;
    case SCTP_STREAM_RESET_EVENT:
      HandleStreamResetEvent(&(notif->sn_strreset_event));
      break;
    case SCTP_ASSOC_RESET_EVENT:
      DC_DEBUG(("SCTP_ASSOC_RESET_EVENT"));
      break;
    case SCTP_STREAM_CHANGE_EVENT:
      HandleStreamChangeEvent(&(notif->sn_strchange_event));
      break;
    default:
      DC_ERROR(("unknown SCTP event: %u", (uint32_t)notif->sn_header.sn_type));
      break;
  }
}

int DataChannelConnection::ReceiveCallback(struct socket* sock, void* data,
                                           size_t datalen,
                                           struct sctp_rcvinfo rcv, int flags) {
  ASSERT_WEBRTC(!NS_IsMainThread());

  if (!data) {
    DC_DEBUG(("ReceiveCallback: SCTP has finished shutting down"));
  } else {
    bool locked = false;
    if (!IsSTSThread()) {
      mLock.Lock();
      locked = true;
    } else {
      mLock.AssertCurrentThreadOwns();
    }
    if (flags & MSG_NOTIFICATION) {
      HandleNotification(static_cast<union sctp_notification*>(data), datalen);
    } else {
      HandleMessage(data, datalen, ntohl(rcv.rcv_ppid), rcv.rcv_sid, flags);
    }
    if (locked) {
      mLock.Unlock();
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

already_AddRefed<DataChannel> DataChannelConnection::Open(
    const nsACString& label, const nsACString& protocol, Type type,
    bool inOrder, uint32_t prValue, DataChannelListener* aListener,
    nsISupports* aContext, bool aExternalNegotiated, uint16_t aStream) {
  ASSERT_WEBRTC(NS_IsMainThread());
  if (!aExternalNegotiated) {
    if (mAllocateEven.isSome()) {
      aStream = FindFreeStream();
      if (aStream == INVALID_STREAM) {
        return nullptr;
      }
    } else {
      // We do not yet know whether we are client or server, and an id has not
      // been chosen for us. We will need to choose later.
      aStream = INVALID_STREAM;
    }
  }
  uint16_t prPolicy = SCTP_PR_SCTP_NONE;

  DC_DEBUG(
      ("DC Open: label %s/%s, type %u, inorder %d, prValue %u, listener %p, "
       "context %p, external: %s, stream %u",
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
    default:
      DC_ERROR(("unsupported channel type: %u", type));
      MOZ_ASSERT(false);
      return nullptr;
  }
  if ((prPolicy == SCTP_PR_SCTP_NONE) && (prValue != 0)) {
    return nullptr;
  }

  if (aStream != INVALID_STREAM && mChannels.Get(aStream)) {
    DC_ERROR(("external negotiation of already-open channel %u", aStream));
    return nullptr;
  }

  RefPtr<DataChannel> channel(new DataChannel(
      this, aStream, DataChannel::CONNECTING, label, protocol, prPolicy,
      prValue, inOrder, aExternalNegotiated, aListener, aContext));
  mChannels.Insert(channel);

  MutexAutoLock lock(mLock);  // OpenFinish assumes this
  return OpenFinish(channel.forget());
}

// Separate routine so we can also call it to finish up from pending opens
already_AddRefed<DataChannel> DataChannelConnection::OpenFinish(
    already_AddRefed<DataChannel>&& aChannel) {
  RefPtr<DataChannel> channel(aChannel);  // takes the reference passed in
  // Normally 1 reference if called from ::Open(), or 2 if called from
  // ProcessQueuedOpens() unless the DOMDataChannel was gc'd
  const uint16_t stream = channel->mStream;

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
  const auto readyState = GetReadyState();
  if (readyState != OPEN || stream >= mNegotiatedIdLimit) {
    if (readyState == OPEN) {
      MOZ_ASSERT(stream != INVALID_STREAM);
      // RequestMoreStreams() limits to MAX_NUM_STREAMS -- allocate extra
      // streams to avoid going back immediately for more if the ask to N, N+1,
      // etc
      int32_t more_needed = stream - ((int32_t)mNegotiatedIdLimit) + 16;
      if (!RequestMoreStreams(more_needed)) {
        // Something bad happened... we're done
        goto request_error_cleanup;
      }
    }
    DC_DEBUG(("Queuing channel %p (%u) to finish open", channel.get(), stream));
    // Also serves to mark we told the app
    channel->mFlags |= DATA_CHANNEL_FLAGS_FINISH_OPEN;
    // we need a ref for the nsDeQue and one to return
    DataChannel* rawChannel = channel;
    rawChannel->AddRef();
    mPending.Push(rawChannel);
    return channel.forget();
  }

  MOZ_ASSERT(stream != INVALID_STREAM);
  MOZ_ASSERT(stream < mNegotiatedIdLimit);

#ifdef TEST_QUEUED_DATA
  // It's painful to write a test for this...
  channel->AnnounceOpen();
  SendDataMsgInternalOrBuffer(channel, "Help me!", 8,
                              DATA_CHANNEL_PPID_DOMSTRING);
#endif

  if (!channel->mOrdered) {
    // Don't send unordered until this gets cleared
    channel->mFlags |= DATA_CHANNEL_FLAGS_WAITING_ACK;
  }

  if (!channel->mNegotiated) {
    int error = SendOpenRequestMessage(channel->mLabel, channel->mProtocol,
                                       stream, !channel->mOrdered,
                                       channel->mPrPolicy, channel->mPrValue);
    if (error) {
      DC_ERROR(("SendOpenRequest failed, error = %d", error));
      if (channel->mFlags & DATA_CHANNEL_FLAGS_FINISH_OPEN) {
        // We already returned the channel to the app.
        NS_ERROR("Failed to send open request");
        channel->AnnounceClosed();
      }
      // If we haven't returned the channel yet, it will get destroyed when we
      // exit this function.
      mChannels.Remove(channel);
      // we'll be destroying the channel
      return nullptr;
      /* NOTREACHED */
    }
  }

  // Either externally negotiated or we sent Open
  // FIX?  Move into DOMDataChannel?  I don't think we can send it yet here
  channel->AnnounceOpen();

  return channel.forget();

request_error_cleanup:
  if (channel->mFlags & DATA_CHANNEL_FLAGS_FINISH_OPEN) {
    // We already returned the channel to the app.
    NS_ERROR("Failed to request more streams");
    channel->AnnounceClosed();
    return channel.forget();
  }
  // we'll be destroying the channel, but it never really got set up
  // Alternative would be to RUN_ON_THREAD(channel.forget(),::Destroy,...) and
  // Dispatch it to ourselves
  return nullptr;
}

// Requires mLock to be locked!
// Returns a POSIX error code directly instead of setting errno.
int DataChannelConnection::SendMsgInternal(OutgoingMsg& msg, size_t* aWritten) {
  auto& info = msg.GetInfo().sendv_sndinfo;
  int error;

  // EOR set?
  bool eor_set = info.snd_flags & SCTP_EOR ? true : false;

  // Send until buffer is empty
  size_t left = msg.GetLeft();
  do {
    size_t length;

    // Carefully chunk the buffer
    if (left > DATA_CHANNEL_MAX_BINARY_FRAGMENT) {
      length = DATA_CHANNEL_MAX_BINARY_FRAGMENT;

      // Unset EOR flag
      info.snd_flags &= ~SCTP_EOR;
    } else {
      length = left;

      // Set EOR flag
      if (eor_set) {
        info.snd_flags |= SCTP_EOR;
      }
    }

    // Send (or try at least)
    // SCTP will return EMSGSIZE if the message is bigger than the buffer
    // size (or EAGAIN if there isn't space). However, we can avoid EMSGSIZE
    // by carefully crafting small enough message chunks.
    ssize_t written = usrsctp_sendv(
        mSocket, msg.GetData(), length, nullptr, 0, (void*)&msg.GetInfo(),
        (socklen_t)sizeof(struct sctp_sendv_spa), SCTP_SENDV_SPA, 0);

    if (written < 0) {
      error = errno;
      goto out;
    }

    if (aWritten) {
      *aWritten += written;
    }
    DC_DEBUG(("Sent buffer (written=%zu, len=%zu, left=%zu)", (size_t)written,
              length, left - (size_t)written));

    // TODO: Remove once resolved
    // (https://github.com/sctplab/usrsctp/issues/132)
    if (written == 0) {
      DC_ERROR(("@tuexen: usrsctp_sendv returned 0"));
      error = EAGAIN;
      goto out;
    }

    // If not all bytes have been written, this obviously means that usrsctp's
    // buffer is full and we need to try again later.
    if ((size_t)written < length) {
      msg.Advance((size_t)written);
      error = EAGAIN;
      goto out;
    }

    // Update buffer position
    msg.Advance((size_t)written);

    // Get amount of bytes left in the buffer
    left = msg.GetLeft();
  } while (left > 0);

  // Done
  error = 0;

out:
  // Reset EOR flag
  if (eor_set) {
    info.snd_flags |= SCTP_EOR;
  }

  return error;
}

// Requires mLock to be locked!
// Returns a POSIX error code directly instead of setting errno.
// IMPORTANT: Ensure that the buffer passed is guarded by mLock!
int DataChannelConnection::SendMsgInternalOrBuffer(
    nsTArray<UniquePtr<BufferedOutgoingMsg>>& buffer, OutgoingMsg& msg,
    bool& buffered, size_t* aWritten) {
  NS_WARNING_ASSERTION(msg.GetLength() > 0, "Length is 0?!");

  int error = 0;
  bool need_buffering = false;

  // Note: Main-thread IO, but doesn't block!
  // XXX FIX!  to deal with heavy overruns of JS trying to pass data in
  // (more than the buffersize) queue data onto another thread to do the
  // actual sends.  See netwerk/protocol/websocket/WebSocketChannel.cpp

  // Avoid a race between buffer-full-failure (where we have to add the
  // packet to the buffered-data queue) and the buffer-now-only-half-full
  // callback, which happens on a different thread.  Otherwise we might
  // fail here, then before we add it to the queue get the half-full
  // callback, find nothing to do, then on this thread add it to the
  // queue - which would sit there.  Also, if we later send more data, it
  // would arrive ahead of the buffered message, but if the buffer ever
  // got to 1/2 full, the message would get sent - but at a semi-random
  // time, after other data it was supposed to be in front of.

  // Must lock before empty check for similar reasons!
  mLock.AssertCurrentThreadOwns();
  if (buffer.IsEmpty() && (mSendInterleaved || !mPendingType)) {
    error = SendMsgInternal(msg, aWritten);
    switch (error) {
      case 0:
        break;
      case EAGAIN:
#if (EAGAIN != EWOULDBLOCK)
      case EWOULDBLOCK:
#endif
        need_buffering = true;
        break;
      default:
        DC_ERROR(("error %d on sending", error));
        break;
    }
  } else {
    need_buffering = true;
  }

  if (need_buffering) {
    // queue data for resend!  And queue any further data for the stream until
    // it is...
    auto* bufferedMsg = new BufferedOutgoingMsg(msg);  // infallible malloc
    buffer.AppendElement(bufferedMsg);  // owned by mBufferedData array
    DC_DEBUG(("Queued %zu buffers (left=%zu, total=%zu)", buffer.Length(),
              msg.GetLeft(), msg.GetLength()));
    buffered = true;
    return 0;
  }

  buffered = false;
  return error;
}

// Caller must ensure that length <= UINT32_MAX
// Returns a POSIX error code.
int DataChannelConnection::SendDataMsgInternalOrBuffer(DataChannel& channel,
                                                       const uint8_t* data,
                                                       size_t len,
                                                       uint32_t ppid) {
  if (NS_WARN_IF(channel.GetReadyState() != OPEN)) {
    return EINVAL;  // TODO: Find a better error code
  }

  struct sctp_sendv_spa info = {0};

  // General flags
  info.sendv_flags = SCTP_SEND_SNDINFO_VALID;

  // Set stream identifier, protocol identifier and flags
  info.sendv_sndinfo.snd_sid = channel.mStream;
  info.sendv_sndinfo.snd_flags = SCTP_EOR;
  info.sendv_sndinfo.snd_ppid = htonl(ppid);

  MutexAutoLock lock(mLock);  // Need to protect mFlags... :(
  // Unordered?
  // To avoid problems where an in-order OPEN is lost and an
  // out-of-order data message "beats" it, require data to be in-order
  // until we get an ACK.
  if (!channel.mOrdered && !(channel.mFlags & DATA_CHANNEL_FLAGS_WAITING_ACK)) {
    info.sendv_sndinfo.snd_flags |= SCTP_UNORDERED;
  }

  // Partial reliability policy
  if (channel.mPrPolicy != SCTP_PR_SCTP_NONE) {
    info.sendv_prinfo.pr_policy = channel.mPrPolicy;
    info.sendv_prinfo.pr_value = channel.mPrValue;
    info.sendv_flags |= SCTP_SEND_PRINFO_VALID;
  }

  // Create message instance and send
  OutgoingMsg msg(info, data, len);
  bool buffered;
  size_t written = 0;
  mDeferSend = true;
  int error =
      SendMsgInternalOrBuffer(channel.mBufferedData, msg, buffered, &written);
  mDeferSend = false;
  if (written) {
    channel.DecrementBufferedAmount(written);
  }

  for (auto&& packet : mDeferredSend) {
    MOZ_ASSERT(written);
    SendPacket(std::move(packet));
  }
  mDeferredSend.clear();

  // Set pending type and stream index (if buffered)
  if (!error && buffered && !mPendingType) {
    mPendingType = PENDING_DATA;
    mCurrentStream = channel.mStream;
  }
  return error;
}

// Caller must ensure that length <= UINT32_MAX
// Returns a POSIX error code.
int DataChannelConnection::SendDataMsg(DataChannel& channel,
                                       const uint8_t* data, size_t len,
                                       uint32_t ppidPartial,
                                       uint32_t ppidFinal) {
  // We *really* don't want to do this from main thread! - and
  // SendDataMsgInternalOrBuffer avoids blocking.

  if (mMaxMessageSize != 0 && len > mMaxMessageSize) {
    DC_ERROR(("Message rejected, too large (%zu > %" PRIu64 ")", len,
              mMaxMessageSize));
    return EMSGSIZE;
  }

  // This will use EOR-based fragmentation if the message is too large (> 64
  // KiB)
  return SendDataMsgInternalOrBuffer(channel, data, len, ppidFinal);
}

class ReadBlobRunnable : public Runnable {
 public:
  ReadBlobRunnable(DataChannelConnection* aConnection, uint16_t aStream,
                   nsIInputStream* aBlob)
      : Runnable("ReadBlobRunnable"),
        mConnection(aConnection),
        mStream(aStream),
        mBlob(aBlob) {}

  NS_IMETHOD Run() override {
    // ReadBlob() is responsible to releasing the reference
    DataChannelConnection* self = mConnection;
    self->ReadBlob(mConnection.forget(), mStream, mBlob);
    return NS_OK;
  }

 private:
  // Make sure the Connection doesn't die while there are jobs outstanding.
  // Let it die (if released by PeerConnectionImpl while we're running)
  // when we send our runnable back to MainThread.  Then ~DataChannelConnection
  // can send the IOThread to MainThread to die in a runnable, avoiding
  // unsafe event loop recursion.  Evil.
  RefPtr<DataChannelConnection> mConnection;
  uint16_t mStream;
  // Use RefCount for preventing the object is deleted when SendBlob returns.
  RefPtr<nsIInputStream> mBlob;
};

// Returns a POSIX error code.
int DataChannelConnection::SendBlob(uint16_t stream, nsIInputStream* aBlob) {
  RefPtr<DataChannel> channel = mChannels.Get(stream);
  if (NS_WARN_IF(!channel)) {
    return EINVAL;  // TODO: Find a better error code
  }

  // Spawn a thread to send the data
  if (!mInternalIOThread) {
    nsresult rv =
        NS_NewNamedThread("DataChannel IO", getter_AddRefs(mInternalIOThread));
    if (NS_FAILED(rv)) {
      return EINVAL;  // TODO: Find a better error code
    }
  }

  mInternalIOThread->Dispatch(
      do_AddRef(new ReadBlobRunnable(this, stream, aBlob)), NS_DISPATCH_NORMAL);
  return 0;
}

class DataChannelBlobSendRunnable : public Runnable {
 public:
  DataChannelBlobSendRunnable(
      already_AddRefed<DataChannelConnection>& aConnection, uint16_t aStream)
      : Runnable("DataChannelBlobSendRunnable"),
        mConnection(aConnection),
        mStream(aStream) {}

  ~DataChannelBlobSendRunnable() override {
    if (!NS_IsMainThread() && mConnection) {
      MOZ_ASSERT(false);
      // explicitly leak the connection if destroyed off mainthread
      Unused << mConnection.forget().take();
    }
  }

  NS_IMETHOD Run() override {
    ASSERT_WEBRTC(NS_IsMainThread());

    mConnection->SendBinaryMsg(mStream, mData);
    mConnection = nullptr;
    return NS_OK;
  }

  // explicitly public so we can avoid allocating twice and copying
  nsCString mData;

 private:
  // Note: we can be destroyed off the target thread, so be careful not to let
  // this get Released()ed on the temp thread!
  RefPtr<DataChannelConnection> mConnection;
  uint16_t mStream;
};

static auto readyStateToCStr(const uint16_t state) -> const char* {
  switch (state) {
    case DataChannelConnection::CONNECTING:
      return "CONNECTING";
    case DataChannelConnection::OPEN:
      return "OPEN";
    case DataChannelConnection::CLOSING:
      return "CLOSING";
    case DataChannelConnection::CLOSED:
      return "CLOSED";
    default: {
      MOZ_ASSERT(false);
      return "UNKNOWW";
    }
  }
};

void DataChannelConnection::SetReadyState(const uint16_t aState) {
  mLock.AssertCurrentThreadOwns();

  DC_DEBUG(
      ("DataChannelConnection labeled %s (%p) switching connection state %s -> "
       "%s",
       mTransportId.c_str(), this, readyStateToCStr(mState),
       readyStateToCStr(aState)));

  mState = aState;
}

void DataChannelConnection::ReadBlob(
    already_AddRefed<DataChannelConnection> aThis, uint16_t aStream,
    nsIInputStream* aBlob) {
  // NOTE: 'aThis' has been forgotten by the caller to avoid releasing
  // it off mainthread; if PeerConnectionImpl has released then we want
  // ~DataChannelConnection() to run on MainThread

  // XXX to do this safely, we must enqueue these atomically onto the
  // output socket.  We need a sender thread(s?) to enqueue data into the
  // socket and to avoid main-thread IO that might block.  Even on a
  // background thread, we may not want to block on one stream's data.
  // I.e. run non-blocking and service multiple channels.

  // Must not let Dispatching it cause the DataChannelConnection to get
  // released on the wrong thread.  Using
  // WrapRunnable(RefPtr<DataChannelConnection>(aThis),... will occasionally
  // cause aThis to get released on this thread.  Also, an explicit Runnable
  // lets us avoid copying the blob data an extra time.
  RefPtr<DataChannelBlobSendRunnable> runnable =
      new DataChannelBlobSendRunnable(aThis, aStream);
  // avoid copying the blob data by passing the mData from the runnable
  if (NS_FAILED(NS_ReadInputStreamToString(aBlob, runnable->mData, -1))) {
    // Bug 966602:  Doesn't return an error to the caller via onerror.
    // We must release DataChannelConnection on MainThread to avoid issues (bug
    // 876167) aThis is now owned by the runnable; release it there
    NS_ReleaseOnMainThreadSystemGroup("DataChannelBlobSendRunnable",
                                      runnable.forget());
    return;
  }
  aBlob->Close();
  Dispatch(runnable.forget());
}

// Returns a POSIX error code.
int DataChannelConnection::SendDataMsgCommon(uint16_t stream,
                                             const nsACString& aMsg,
                                             bool isBinary) {
  ASSERT_WEBRTC(NS_IsMainThread());
  // We really could allow this from other threads, so long as we deal with
  // asynchronosity issues with channels closing, in particular access to
  // mChannels, and issues with the association closing (access to mSocket).

  const uint8_t* data = (const uint8_t*)aMsg.BeginReading();
  uint32_t len = aMsg.Length();
#if (UINT32_MAX > SIZE_MAX)
  if (len > SIZE_MAX) {
    return EMSGSIZE;
  }
#endif

  DC_DEBUG(("Sending %sto stream %u: %u bytes", isBinary ? "binary " : "",
            stream, len));
  // XXX if we want more efficiency, translate flags once at open time
  RefPtr<DataChannel> channelPtr = mChannels.Get(stream);
  if (NS_WARN_IF(!channelPtr)) {
    return EINVAL;  // TODO: Find a better error code
  }

  auto& channel = *channelPtr;

  if (isBinary) {
    return SendDataMsg(channel, data, len, DATA_CHANNEL_PPID_BINARY_PARTIAL,
                       DATA_CHANNEL_PPID_BINARY);
  }
  return SendDataMsg(channel, data, len, DATA_CHANNEL_PPID_DOMSTRING_PARTIAL,
                     DATA_CHANNEL_PPID_DOMSTRING);
}

void DataChannelConnection::Stop() {
  // Note: This will call 'CloseAll' from the main thread
  Dispatch(do_AddRef(new DataChannelOnMessageAvailable(
      DataChannelOnMessageAvailable::ON_DISCONNECTED, this)));
}

void DataChannelConnection::Close(DataChannel* aChannel) {
  MutexAutoLock lock(mLock);
  CloseInt(aChannel);
}

// So we can call Close() with the lock already held
// Called from someone who holds a ref via ::Close(), or from ~DataChannel
void DataChannelConnection::CloseInt(DataChannel* aChannel) {
  MOZ_ASSERT(aChannel);
  RefPtr<DataChannel> channel(aChannel);  // make sure it doesn't go away on us

  mLock.AssertCurrentThreadOwns();
  DC_DEBUG(("Connection %p/Channel %p: Closing stream %u",
            channel->mConnection.get(), channel.get(), channel->mStream));

  aChannel->mBufferedData.Clear();
  if (GetReadyState() == CLOSED) {
    // If we're CLOSING, we might leave this in place until we can send a
    // reset.
    mChannels.Remove(channel);
  }

  // This is supposed to only be accessed from Main thread, but this has
  // been accessed here from the STS thread for a long time now.
  // See Bug 1586475
  auto channelState = aChannel->mReadyState;
  // re-test since it may have closed before the lock was grabbed
  if (channelState == CLOSED || channelState == CLOSING) {
    DC_DEBUG(("Channel already closing/closed (%u)", channelState));
    return;
  }

  if (channel->mStream != INVALID_STREAM) {
    ResetOutgoingStream(channel->mStream);
    if (GetReadyState() != CLOSED) {
      // Individual channel is being closed, send reset now.
      SendOutgoingStreamReset();
    }
  }
  aChannel->SetReadyState(CLOSING);
  if (GetReadyState() == CLOSED) {
    // we're not going to hang around waiting
    channel->StreamClosedLocked();
  }
  // At this point when we leave here, the object is a zombie held alive only by
  // the DOM object
}

void DataChannelConnection::CloseAll() {
  DC_DEBUG(("Closing all channels (connection %p)", (void*)this));
  // Don't need to lock here

  // Make sure no more channels will be opened
  {
    MutexAutoLock lock(mLock);
    SetReadyState(CLOSED);
  }

  // Close current channels
  // If there are runnables, they hold a strong ref and keep the channel
  // and/or connection alive (even if in a CLOSED state)
  for (auto& channel : mChannels.GetAll()) {
    channel->Close();
  }

  // Clean up any pending opens for channels
  RefPtr<DataChannel> channel;
  while (nullptr != (channel = dont_AddRef(
                         static_cast<DataChannel*>(mPending.PopFront())))) {
    DC_DEBUG(("closing pending channel %p, stream %u", channel.get(),
              channel->mStream));
    channel->Close();  // also releases the ref on each iteration
  }
  // It's more efficient to let the Resets queue in shutdown and then
  // SendOutgoingStreamReset() here.
  MutexAutoLock lock(mLock);
  SendOutgoingStreamReset();
}

bool DataChannelConnection::Channels::IdComparator::Equals(
    const RefPtr<DataChannel>& aChannel, uint16_t aId) const {
  return aChannel->mStream == aId;
}

bool DataChannelConnection::Channels::IdComparator::LessThan(
    const RefPtr<DataChannel>& aChannel, uint16_t aId) const {
  return aChannel->mStream < aId;
}

bool DataChannelConnection::Channels::IdComparator::Equals(
    const RefPtr<DataChannel>& a1, const RefPtr<DataChannel>& a2) const {
  return Equals(a1, a2->mStream);
}

bool DataChannelConnection::Channels::IdComparator::LessThan(
    const RefPtr<DataChannel>& a1, const RefPtr<DataChannel>& a2) const {
  return LessThan(a1, a2->mStream);
}

void DataChannelConnection::Channels::Insert(
    const RefPtr<DataChannel>& aChannel) {
  DC_DEBUG(("Inserting channel %u : %p", aChannel->mStream, aChannel.get()));
  MutexAutoLock lock(mMutex);
  if (aChannel->mStream != INVALID_STREAM) {
    MOZ_ASSERT(!mChannels.ContainsSorted(aChannel, IdComparator()));
  }

  MOZ_ASSERT(!mChannels.Contains(aChannel));

  mChannels.InsertElementSorted(aChannel, IdComparator());
}

bool DataChannelConnection::Channels::Remove(
    const RefPtr<DataChannel>& aChannel) {
  DC_DEBUG(("Removing channel %u : %p", aChannel->mStream, aChannel.get()));
  MutexAutoLock lock(mMutex);
  if (aChannel->mStream == INVALID_STREAM) {
    return mChannels.RemoveElement(aChannel);
  }

  return mChannels.RemoveElementSorted(aChannel, IdComparator());
}

RefPtr<DataChannel> DataChannelConnection::Channels::Get(uint16_t aId) const {
  MutexAutoLock lock(mMutex);
  auto index = mChannels.BinaryIndexOf(aId, IdComparator());
  if (index == ChannelArray::NoIndex) {
    return nullptr;
  }
  return mChannels[index];
}

RefPtr<DataChannel> DataChannelConnection::Channels::GetNextChannel(
    uint16_t aCurrentId) const {
  MutexAutoLock lock(mMutex);
  if (mChannels.IsEmpty()) {
    return nullptr;
  }

  auto index = mChannels.IndexOfFirstElementGt(aCurrentId, IdComparator());
  if (index == mChannels.Length()) {
    index = 0;
  }
  return mChannels[index];
}

DataChannel::~DataChannel() {
  // NS_ASSERTION since this is more "I think I caught all the cases that
  // can cause this" than a true kill-the-program assertion.  If this is
  // wrong, nothing bad happens.  A worst it's a leak.
  NS_ASSERTION(mReadyState == CLOSED || mReadyState == CLOSING,
               "unexpected state in ~DataChannel");
}

void DataChannel::Close() {
  if (mConnection) {
    // ensure we don't get deleted
    RefPtr<DataChannelConnection> connection(mConnection);
    connection->Close(this);
  }
}

// Used when disconnecting from the DataChannelConnection
void DataChannel::StreamClosedLocked() {
  mConnection->mLock.AssertCurrentThreadOwns();
  ENSURE_DATACONNECTION;

  DC_DEBUG(("Destroying Data channel %u", mStream));
  MOZ_ASSERT_IF(mStream != INVALID_STREAM,
                !mConnection->FindChannelByStream(mStream));
  AnnounceClosed();
  // We leave mConnection live until the DOM releases us, to avoid races
}

void DataChannel::ReleaseConnection() {
  ASSERT_WEBRTC(NS_IsMainThread());
  mConnection = nullptr;
}

void DataChannel::SetListener(DataChannelListener* aListener,
                              nsISupports* aContext) {
  ASSERT_WEBRTC(NS_IsMainThread());
  mContext = aContext;
  mListener = aListener;
}

void DataChannel::SendErrnoToErrorResult(int error, size_t aMessageSize,
                                         ErrorResult& aRv) {
  switch (error) {
    case 0:
      break;
    case EMSGSIZE: {
      nsPrintfCString err("Message size (%zu) exceeds maxMessageSize",
                          aMessageSize);
      aRv.ThrowTypeError(err);
      break;
    }
    default:
      aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
      break;
  }
}

void DataChannel::IncrementBufferedAmount(uint32_t aSize, ErrorResult& aRv) {
  ASSERT_WEBRTC(NS_IsMainThread());
  if (mBufferedAmount > UINT32_MAX - aSize) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }

  mBufferedAmount += aSize;
}

void DataChannel::DecrementBufferedAmount(uint32_t aSize) {
  mMainThreadEventTarget->Dispatch(NS_NewRunnableFunction(
      "DataChannel::DecrementBufferedAmount",
      [this, self = RefPtr<DataChannel>(this), aSize] {
        MOZ_ASSERT(aSize <= mBufferedAmount);
        bool wasLow = mBufferedAmount <= mBufferedThreshold;
        mBufferedAmount -= aSize;
        if (!wasLow && mBufferedAmount <= mBufferedThreshold) {
          DC_DEBUG(("%s: sending BUFFER_LOW_THRESHOLD for %s/%s: %u",
                    __FUNCTION__, mLabel.get(), mProtocol.get(), mStream));
          mListener->OnBufferLow(mContext);
        }
        if (mBufferedAmount == 0) {
          DC_DEBUG(("%s: sending NO_LONGER_BUFFERED for %s/%s: %u",
                    __FUNCTION__, mLabel.get(), mProtocol.get(), mStream));
          mListener->NotBuffered(mContext);
        }
      }));
}

void DataChannel::AnnounceOpen() {
  mMainThreadEventTarget->Dispatch(NS_NewRunnableFunction(
      "DataChannel::AnnounceOpen", [this, self = RefPtr<DataChannel>(this)] {
        auto state = GetReadyState();
        // Special-case; spec says to put brand-new remote-created DataChannel
        // in "open", but queue the firing of the "open" event.
        if (state != CLOSING && state != CLOSED && mListener) {
          SetReadyState(OPEN);
          DC_DEBUG(("%s: sending ON_CHANNEL_OPEN for %s/%s: %u", __FUNCTION__,
                    mLabel.get(), mProtocol.get(), mStream));
          mListener->OnChannelConnected(mContext);
        }
      }));
}

void DataChannel::AnnounceClosed() {
  mMainThreadEventTarget->Dispatch(NS_NewRunnableFunction(
      "DataChannel::AnnounceClosed", [this, self = RefPtr<DataChannel>(this)] {
        if (GetReadyState() == CLOSED) {
          return;
        }
        SetReadyState(CLOSED);
        mBufferedData.Clear();
        if (mListener) {
          DC_DEBUG(("%s: sending ON_CHANNEL_CLOSED for %s/%s: %u", __FUNCTION__,
                    mLabel.get(), mProtocol.get(), mStream));
          mListener->OnChannelClosed(mContext);
        }
      }));
}

// Set ready state
void DataChannel::SetReadyState(const uint16_t aState) {
  MOZ_ASSERT(NS_IsMainThread());

  DC_DEBUG(
      ("DataChannelConnection labeled %s(%p) (stream %d) changing ready state "
       "%s -> %s",
       mLabel.get(), this, mStream, readyStateToCStr(mReadyState),
       readyStateToCStr(aState)));

  mReadyState = aState;
}

void DataChannel::SendMsg(const nsACString& aMsg, ErrorResult& aRv) {
  if (!EnsureValidStream(aRv)) {
    return;
  }

  SendErrnoToErrorResult(mConnection->SendMsg(mStream, aMsg), aMsg.Length(),
                         aRv);
  if (!aRv.Failed()) {
    IncrementBufferedAmount(aMsg.Length(), aRv);
  }
}

void DataChannel::SendBinaryMsg(const nsACString& aMsg, ErrorResult& aRv) {
  if (!EnsureValidStream(aRv)) {
    return;
  }

  SendErrnoToErrorResult(mConnection->SendBinaryMsg(mStream, aMsg),
                         aMsg.Length(), aRv);
  if (!aRv.Failed()) {
    IncrementBufferedAmount(aMsg.Length(), aRv);
  }
}

void DataChannel::SendBinaryBlob(dom::Blob& aBlob, ErrorResult& aRv) {
  if (!EnsureValidStream(aRv)) {
    return;
  }

  uint64_t msgLength = aBlob.GetSize(aRv);
  if (aRv.Failed()) {
    return;
  }

  if (msgLength > UINT32_MAX) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }

  // We convert to an nsIInputStream here, because Blob is not threadsafe, and
  // we don't convert it earlier because we need to know how large this is so we
  // can update bufferedAmount.
  nsCOMPtr<nsIInputStream> msgStream;
  aBlob.CreateInputStream(getter_AddRefs(msgStream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  SendErrnoToErrorResult(mConnection->SendBlob(mStream, msgStream), msgLength,
                         aRv);
  if (!aRv.Failed()) {
    IncrementBufferedAmount(msgLength, aRv);
  }
}

dom::Nullable<uint16_t> DataChannel::GetMaxPacketLifeTime() const {
  if (mPrPolicy == SCTP_PR_SCTP_TTL) {
    return dom::Nullable<uint16_t>(mPrValue);
  }
  return dom::Nullable<uint16_t>();
}

dom::Nullable<uint16_t> DataChannel::GetMaxRetransmits() const {
  if (mPrPolicy == SCTP_PR_SCTP_RTX) {
    return dom::Nullable<uint16_t>(mPrValue);
  }
  return dom::Nullable<uint16_t>();
}

uint32_t DataChannel::GetBufferedAmountLowThreshold() {
  return mBufferedThreshold;
}

// Never fire immediately, as it's defined to fire on transitions, not state
void DataChannel::SetBufferedAmountLowThreshold(uint32_t aThreshold) {
  mBufferedThreshold = aThreshold;
}

// Called with mLock locked!
void DataChannel::SendOrQueue(DataChannelOnMessageAvailable* aMessage) {
  nsCOMPtr<nsIRunnable> runnable = aMessage;
  mMainThreadEventTarget->Dispatch(runnable.forget());
}

bool DataChannel::EnsureValidStream(ErrorResult& aRv) {
  MOZ_ASSERT(mConnection);
  if (mConnection && mStream != INVALID_STREAM) {
    return true;
  }
  aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  return false;
}

}  // namespace mozilla
