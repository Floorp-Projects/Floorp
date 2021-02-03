/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "ConnectionHandle.h"
#include "HalfOpenSocket.h"
#include "nsHttpConnection.h"
#include "nsIDNSRecord.h"
#include "nsIDNSService.h"
#include "nsQueryObject.h"

// Log on level :5, instead of default :4.
#undef LOG
#define LOG(args) LOG5(args)
#undef LOG_ENABLED
#define LOG_ENABLED() LOG5_ENABLED()

namespace mozilla {
namespace net {

//////////////////////// HalfOpenSocket
NS_IMPL_ADDREF(HalfOpenSocket)
NS_IMPL_RELEASE(HalfOpenSocket)

NS_INTERFACE_MAP_BEGIN(HalfOpenSocket)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIOutputStreamCallback)
  NS_INTERFACE_MAP_ENTRY(nsITransportEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(HalfOpenSocket)
NS_INTERFACE_MAP_END

HalfOpenSocket::HalfOpenSocket(ConnectionEntry* ent, nsAHttpTransaction* trans,
                               uint32_t caps, bool speculative,
                               bool isFromPredictor, bool urgentStart)
    : mTransaction(trans),
      mDispatchedMTransaction(false),
      mCaps(caps),
      mSpeculative(speculative),
      mUrgentStart(urgentStart),
      mIsFromPredictor(isFromPredictor),
      mAllow1918(true),
      mHasConnected(false),
      mPrimaryConnectedOK(false),
      mBackupConnectedOK(false),
      mBackupConnStatsSet(false),
      mFreeToUse(true),
      mPrimaryStreamStatus(NS_OK),
      mEnt(ent) {
  MOZ_ASSERT(ent && trans, "constructor with null arguments");
  LOG(("Creating HalfOpenSocket [this=%p trans=%p ent=%s key=%s]\n", this,
       trans, ent->mConnInfo->Origin(), ent->mConnInfo->HashKey().get()));

  mIsHttp3 = mEnt->mConnInfo->IsHttp3();
  if (speculative) {
    Telemetry::AutoCounter<Telemetry::HTTPCONNMGR_TOTAL_SPECULATIVE_CONN>
        totalSpeculativeConn;
    ++totalSpeculativeConn;

    if (isFromPredictor) {
      Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PRECONNECTS_CREATED>
          totalPreconnectsCreated;
      ++totalPreconnectsCreated;
    }
  }

  MOZ_ASSERT(mEnt);
}

HalfOpenSocket::~HalfOpenSocket() {
  MOZ_ASSERT(!mStreamOut);
  MOZ_ASSERT(!mBackupStreamOut);
  LOG(("Destroying HalfOpenSocket [this=%p]\n", this));

  if (mEnt) {
    bool inqueue = mEnt->RemoveHalfOpen(this);
    LOG(("Destroying HalfOpenSocket was in the HalfOpenList=%d [this=%p]\n",
         inqueue, this));
  }
}

nsresult HalfOpenSocket::SetupStreams(nsISocketTransport** transport,
                                      nsIAsyncInputStream** instream,
                                      nsIAsyncOutputStream** outstream,
                                      bool isBackup) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  MOZ_ASSERT(mEnt);
  nsresult rv;
  nsTArray<nsCString> socketTypes;
  const nsHttpConnectionInfo* ci = mEnt->mConnInfo;
  if (mIsHttp3) {
    socketTypes.AppendElement("quic"_ns);
  } else {
    if (ci->FirstHopSSL()) {
      socketTypes.AppendElement("ssl"_ns);
    } else {
      const nsCString& defaultType = gHttpHandler->DefaultSocketType();
      if (!defaultType.IsVoid()) {
        socketTypes.AppendElement(defaultType);
      }
    }
  }

  nsCOMPtr<nsISocketTransport> socketTransport;
  nsCOMPtr<nsISocketTransportService> sts;

  sts = services::GetSocketTransportService();
  if (!sts) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  LOG(
      ("HalfOpenSocket::SetupStreams [this=%p ent=%s] "
       "setup routed transport to origin %s:%d via %s:%d\n",
       this, ci->HashKey().get(), ci->Origin(), ci->OriginPort(),
       ci->RoutedHost(), ci->RoutedPort()));

  nsCOMPtr<nsIRoutedSocketTransportService> routedSTS(do_QueryInterface(sts));
  if (routedSTS) {
    rv = routedSTS->CreateRoutedTransport(
        socketTypes, ci->GetOrigin(), ci->OriginPort(), ci->GetRoutedHost(),
        ci->RoutedPort(), ci->ProxyInfo(), getter_AddRefs(socketTransport));
  } else {
    if (!ci->GetRoutedHost().IsEmpty()) {
      // There is a route requested, but the legacy nsISocketTransportService
      // can't handle it.
      // Origin should be reachable on origin host name, so this should
      // not be a problem - but log it.
      LOG(
          ("HalfOpenSocket this=%p using legacy nsISocketTransportService "
           "means explicit route %s:%d will be ignored.\n",
           this, ci->RoutedHost(), ci->RoutedPort()));
    }

    rv = sts->CreateTransport(socketTypes, ci->GetOrigin(), ci->OriginPort(),
                              ci->ProxyInfo(), getter_AddRefs(socketTransport));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t tmpFlags = 0;
  if (mCaps & NS_HTTP_REFRESH_DNS) {
    tmpFlags = nsISocketTransport::BYPASS_CACHE;
  }

  tmpFlags |= nsISocketTransport::GetFlagsFromTRRMode(
      NS_HTTP_TRR_MODE_FROM_FLAGS(mCaps));

  if (mCaps & NS_HTTP_LOAD_ANONYMOUS) {
    tmpFlags |= nsISocketTransport::ANONYMOUS_CONNECT;
  }

  if (ci->GetPrivate()) {
    tmpFlags |= nsISocketTransport::NO_PERMANENT_STORAGE;
  }

  Unused << socketTransport->SetIsPrivate(ci->GetPrivate());

  if (ci->GetLessThanTls13()) {
    tmpFlags |= nsISocketTransport::DONT_TRY_ECH;
  }

  if (((mCaps & NS_HTTP_BE_CONSERVATIVE) || ci->GetBeConservative()) &&
      gHttpHandler->ConnMgr()->BeConservativeIfProxied(ci->ProxyInfo())) {
    LOG(("Setting Socket to BE_CONSERVATIVE"));
    tmpFlags |= nsISocketTransport::BE_CONSERVATIVE;
  }

  if (ci->HasIPHintAddress()) {
    nsCOMPtr<nsIDNSService> dns =
        do_GetService("@mozilla.org/network/dns-service;1", &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // The spec says: "If A and AAAA records for TargetName are locally
    // available, the client SHOULD ignore these hints.", so we check if the DNS
    // record is in cache before setting USE_IP_HINT_ADDRESS.
    nsCOMPtr<nsIDNSRecord> record;
    rv = dns->ResolveNative(ci->GetRoutedHost(), nsIDNSService::RESOLVE_OFFLINE,
                            mEnt->mConnInfo->GetOriginAttributes(),
                            getter_AddRefs(record));
    if (NS_FAILED(rv) || !record) {
      LOG(("Setting Socket to use IP hint address"));
      tmpFlags |= nsISocketTransport::USE_IP_HINT_ADDRESS;
    }
  }

  if (mCaps & NS_HTTP_DISABLE_IPV4) {
    tmpFlags |= nsISocketTransport::DISABLE_IPV4;
  } else if (mCaps & NS_HTTP_DISABLE_IPV6) {
    tmpFlags |= nsISocketTransport::DISABLE_IPV6;
  } else if (mEnt->PreferenceKnown()) {
    if (mEnt->mPreferIPv6) {
      tmpFlags |= nsISocketTransport::DISABLE_IPV4;
    } else if (mEnt->mPreferIPv4) {
      tmpFlags |= nsISocketTransport::DISABLE_IPV6;
    }

    // In case the host is no longer accessible via the preferred IP family,
    // try the opposite one and potentially restate the preference.
    tmpFlags |= nsISocketTransport::RETRY_WITH_DIFFERENT_IP_FAMILY;

    // From the same reason, let the backup socket fail faster to try the other
    // family.
    uint16_t fallbackTimeout =
        isBackup ? gHttpHandler->GetFallbackSynTimeout() : 0;
    if (fallbackTimeout) {
      socketTransport->SetTimeout(nsISocketTransport::TIMEOUT_CONNECT,
                                  fallbackTimeout);
    }
  } else if (isBackup && gHttpHandler->FastFallbackToIPv4()) {
    // For backup connections, we disable IPv6. That's because some users have
    // broken IPv6 connectivity (leading to very long timeouts), and disabling
    // IPv6 on the backup connection gives them a much better user experience
    // with dual-stack hosts, though they still pay the 250ms delay for each new
    // connection. This strategy is also known as "happy eyeballs".
    tmpFlags |= nsISocketTransport::DISABLE_IPV6;
  }

  if (!Allow1918()) {
    tmpFlags |= nsISocketTransport::DISABLE_RFC1918;
  }

  MOZ_ASSERT(!(tmpFlags & nsISocketTransport::DISABLE_IPV4) ||
                 !(tmpFlags & nsISocketTransport::DISABLE_IPV6),
             "Both types should not be disabled at the same time.");
  socketTransport->SetConnectionFlags(tmpFlags);
  socketTransport->SetTlsFlags(ci->GetTlsFlags());

  const OriginAttributes& originAttributes =
      mEnt->mConnInfo->GetOriginAttributes();
  if (originAttributes != OriginAttributes()) {
    socketTransport->SetOriginAttributes(originAttributes);
  }

  socketTransport->SetQoSBits(gHttpHandler->GetQoSBits());

  rv = socketTransport->SetEventSink(this, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = socketTransport->SetSecurityCallbacks(this);
  NS_ENSURE_SUCCESS(rv, rv);

  if (gHttpHandler->EchConfigEnabled()) {
    rv = socketTransport->SetEchConfig(ci->GetEchConfig());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  Telemetry::Accumulate(Telemetry::HTTP_CONNECTION_ENTRY_CACHE_HIT_1,
                        mEnt->mUsedForConnection);
  mEnt->mUsedForConnection = true;

  nsCOMPtr<nsIOutputStream> sout;
  rv = socketTransport->OpenOutputStream(nsITransport::OPEN_UNBUFFERED, 0, 0,
                                         getter_AddRefs(sout));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> sin;
  rv = socketTransport->OpenInputStream(nsITransport::OPEN_UNBUFFERED, 0, 0,
                                        getter_AddRefs(sin));
  NS_ENSURE_SUCCESS(rv, rv);

  socketTransport.forget(transport);
  CallQueryInterface(sin, instream);
  CallQueryInterface(sout, outstream);

  rv = (*outstream)->AsyncWait(this, 0, 0, nullptr);
  if (NS_SUCCEEDED(rv)) {
    gHttpHandler->ConnMgr()->StartedConnect();
  }

  return rv;
}

nsresult HalfOpenSocket::SetupPrimaryStreams() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  nsresult rv;

  mPrimarySynStarted = TimeStamp::Now();
  rv = SetupStreams(getter_AddRefs(mSocketTransport), getter_AddRefs(mStreamIn),
                    getter_AddRefs(mStreamOut), false);

  LOG(("HalfOpenSocket::SetupPrimaryStream [this=%p ent=%s rv=%" PRIx32 "]",
       this, mEnt->mConnInfo->Origin(), static_cast<uint32_t>(rv)));
  if (NS_FAILED(rv)) {
    if (mStreamOut) {
      mStreamOut->AsyncWait(nullptr, 0, 0, nullptr);
    }

    mStreamOut = nullptr;
    mStreamIn = nullptr;
    mSocketTransport = nullptr;
  }
  return rv;
}

nsresult HalfOpenSocket::SetupBackupStreams() {
  MOZ_ASSERT(mTransaction);

  mBackupSynStarted = TimeStamp::Now();
  nsresult rv = SetupStreams(getter_AddRefs(mBackupTransport),
                             getter_AddRefs(mBackupStreamIn),
                             getter_AddRefs(mBackupStreamOut), true);

  LOG(("HalfOpenSocket::SetupBackupStream [this=%p ent=%s rv=%" PRIx32 "]",
       this, mEnt->mConnInfo->Origin(), static_cast<uint32_t>(rv)));
  if (NS_FAILED(rv)) {
    if (mBackupStreamOut) {
      mBackupStreamOut->AsyncWait(nullptr, 0, 0, nullptr);
    }
    mBackupStreamOut = nullptr;
    mBackupStreamIn = nullptr;
    mBackupTransport = nullptr;
  }
  return rv;
}

void HalfOpenSocket::SetupBackupTimer() {
  MOZ_ASSERT(mEnt);
  uint16_t timeout = gHttpHandler->GetIdleSynTimeout();
  MOZ_ASSERT(!mSynTimer, "timer already initd");

  // When using Fast Open the correct transport will be setup for sure (it is
  // guaranteed), but it can be that it will happened a bit later.
  if (timeout && !mSpeculative) {
    // Setup the timer that will establish a backup socket
    // if we do not get a writable event on the main one.
    // We do this because a lost SYN takes a very long time
    // to repair at the TCP level.
    //
    // Failure to setup the timer is something we can live with,
    // so don't return an error in that case.
    NS_NewTimerWithCallback(getter_AddRefs(mSynTimer), this, timeout,
                            nsITimer::TYPE_ONE_SHOT);
    LOG(("HalfOpenSocket::SetupBackupTimer() [this=%p]", this));
  } else if (timeout) {
    LOG(("HalfOpenSocket::SetupBackupTimer() [this=%p], did not arm\n", this));
  }
}

void HalfOpenSocket::CancelBackupTimer() {
  // If the syntimer is still armed, we can cancel it because no backup
  // socket should be formed at this point
  if (!mSynTimer) {
    return;
  }

  LOG(("HalfOpenSocket::CancelBackupTimer()"));
  mSynTimer->Cancel();

  // Keeping the reference to the timer to remember we have already
  // performed the backup connection.
}

void HalfOpenSocket::Abandon() {
  LOG(("HalfOpenSocket::Abandon [this=%p ent=%s] %p %p %p %p", this,
       mEnt->mConnInfo->Origin(), mSocketTransport.get(),
       mBackupTransport.get(), mStreamOut.get(), mBackupStreamOut.get()));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  RefPtr<HalfOpenSocket> deleteProtector(this);

  // Tell socket (and backup socket) to forget the half open socket.
  if (mSocketTransport) {
    mSocketTransport->SetEventSink(nullptr, nullptr);
    mSocketTransport->SetSecurityCallbacks(nullptr);
    mSocketTransport = nullptr;
  }
  if (mBackupTransport) {
    mBackupTransport->SetEventSink(nullptr, nullptr);
    mBackupTransport->SetSecurityCallbacks(nullptr);
    mBackupTransport = nullptr;
  }

  // Tell output stream (and backup) to forget the half open socket.
  if (mStreamOut) {
    gHttpHandler->ConnMgr()->RecvdConnect();
    mStreamOut->AsyncWait(nullptr, 0, 0, nullptr);
    mStreamOut = nullptr;
  }
  if (mBackupStreamOut) {
    gHttpHandler->ConnMgr()->RecvdConnect();
    mBackupStreamOut->AsyncWait(nullptr, 0, 0, nullptr);
    mBackupStreamOut = nullptr;
  }

  // Lose references to input stream (and backup).
  if (mStreamIn) {
    mStreamIn->AsyncWait(nullptr, 0, 0, nullptr);
    mStreamIn = nullptr;
  }
  if (mBackupStreamIn) {
    mBackupStreamIn->AsyncWait(nullptr, 0, 0, nullptr);
    mBackupStreamIn = nullptr;
  }

  // Stop the timer - we don't want any new backups.
  CancelBackupTimer();

  // Remove the half open from the connection entry.
  if (mEnt) {
    mEnt->mDoNotDestroy = false;
    mEnt->RemoveHalfOpen(this);
  }
  mEnt = nullptr;
}

double HalfOpenSocket::Duration(TimeStamp epoch) {
  if (mPrimarySynStarted.IsNull()) {
    return 0;
  }

  return (epoch - mPrimarySynStarted).ToMilliseconds();
}

NS_IMETHODIMP  // method for nsITimerCallback
HalfOpenSocket::Notify(nsITimer* timer) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(timer == mSynTimer, "wrong timer");

  MOZ_ASSERT(!mBackupTransport);
  MOZ_ASSERT(mSynTimer);
  MOZ_ASSERT(mEnt);

  DebugOnly<nsresult> rv = SetupBackupStreams();
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Keeping the reference to the timer to remember we have already
  // performed the backup connection.

  return NS_OK;
}

NS_IMETHODIMP  // method for nsINamed
HalfOpenSocket::GetName(nsACString& aName) {
  aName.AssignLiteral("HalfOpenSocket");
  return NS_OK;
}

// method for nsIAsyncOutputStreamCallback
NS_IMETHODIMP
HalfOpenSocket::OnOutputStreamReady(nsIAsyncOutputStream* out) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(mStreamOut || mBackupStreamOut);
  MOZ_ASSERT(out == mStreamOut || out == mBackupStreamOut, "stream mismatch");
  MOZ_ASSERT(mEnt);

  LOG(("HalfOpenSocket::OnOutputStreamReady [this=%p ent=%s %s]\n", this,
       mEnt->mConnInfo->Origin(), out == mStreamOut ? "primary" : "backup"));

  mEnt->mDoNotDestroy = true;
  gHttpHandler->ConnMgr()->RecvdConnect();

  CancelBackupTimer();

  nsresult rv = SetupConn(out);
  if (mEnt) {
    mEnt->mDoNotDestroy = false;
  }
  return rv;
}

nsresult HalfOpenSocket::SetupConn(nsIAsyncOutputStream* out) {
  // assign the new socket to the http connection
  RefPtr<HttpConnectionBase> conn;
  if (!mIsHttp3) {
    conn = new nsHttpConnection();
  } else {
    conn = new HttpConnectionUDP();
  }

  LOG(
      ("HalfOpenSocket::SetupConn "
       "Created new nshttpconnection %p %s\n",
       conn.get(), mIsHttp3 ? "using http3" : ""));

  NullHttpTransaction* nullTrans = mTransaction->QueryNullTransaction();
  if (nullTrans) {
    conn->BootstrapTimings(nullTrans->Timings());
  }

  // Some capabilities are needed before a transaciton actually gets
  // scheduled (e.g. how to negotiate false start)
  conn->SetTransactionCaps(mTransaction->Caps());

  NetAddr peeraddr;
  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  mTransaction->GetSecurityCallbacks(getter_AddRefs(callbacks));
  nsresult rv;
  if (out == mStreamOut) {
    TimeDuration rtt = TimeStamp::Now() - mPrimarySynStarted;
    rv = conn->Init(
        mEnt->mConnInfo, gHttpHandler->ConnMgr()->mMaxRequestDelay,
        mSocketTransport, mStreamIn, mStreamOut, mPrimaryConnectedOK, callbacks,
        PR_MillisecondsToInterval(static_cast<uint32_t>(rtt.ToMilliseconds())));

    bool resetPreference = false;
    mSocketTransport->GetResetIPFamilyPreference(&resetPreference);
    if (resetPreference) {
      mEnt->ResetIPFamilyPreference();
    }

    if (NS_SUCCEEDED(mSocketTransport->GetPeerAddr(&peeraddr))) {
      mEnt->RecordIPFamilyPreference(peeraddr.raw.family);
    }

    // The nsHttpConnection object now owns these streams and sockets
    mStreamOut = nullptr;
    mStreamIn = nullptr;
    mSocketTransport = nullptr;

  } else if (out == mBackupStreamOut) {
    TimeDuration rtt = TimeStamp::Now() - mBackupSynStarted;
    rv = conn->Init(
        mEnt->mConnInfo, gHttpHandler->ConnMgr()->mMaxRequestDelay,
        mBackupTransport, mBackupStreamIn, mBackupStreamOut, mBackupConnectedOK,
        callbacks,
        PR_MillisecondsToInterval(static_cast<uint32_t>(rtt.ToMilliseconds())));

    bool resetPreference = false;
    mBackupTransport->GetResetIPFamilyPreference(&resetPreference);
    if (resetPreference) {
      mEnt->ResetIPFamilyPreference();
    }

    if (NS_SUCCEEDED(mBackupTransport->GetPeerAddr(&peeraddr))) {
      mEnt->RecordIPFamilyPreference(peeraddr.raw.family);
    }

    // The nsHttpConnection object now owns these streams and sockets
    mBackupStreamOut = nullptr;
    mBackupStreamIn = nullptr;
    mBackupTransport = nullptr;
  } else {
    MOZ_ASSERT(false, "unexpected stream");
    rv = NS_ERROR_UNEXPECTED;
  }

  if (NS_FAILED(rv)) {
    LOG(
        ("HalfOpenSocket::SetupConn "
         "conn->init (%p) failed %" PRIx32 "\n",
         conn.get(), static_cast<uint32_t>(rv)));

    if (nsHttpTransaction* trans = mTransaction->QueryHttpTransaction()) {
      if (mIsHttp3) {
        trans->DisableHttp3();
        gHttpHandler->ExcludeHttp3(mEnt->mConnInfo);
      }
      // The transaction's connection info is changed after DisableHttp3(), so
      // this is the only point we can remove this transaction from its conn
      // entry.
      mEnt->RemoveTransFromPendingQ(trans);
    }
    mTransaction->Close(rv);

    return rv;
  }

  // This half-open socket has created a connection.  This flag excludes it
  // from counter of actual connections used for checking limits.
  mHasConnected = true;

  // if this is still in the pending list, remove it and dispatch it
  RefPtr<PendingTransactionInfo> pendingTransInfo =
      gHttpHandler->ConnMgr()->FindTransactionHelper(true, mEnt, mTransaction);
  if (pendingTransInfo) {
    MOZ_ASSERT(!mSpeculative, "Speculative Half Open found mTransaction");

    mEnt->InsertIntoActiveConns(conn);
    if (mIsHttp3) {
      // Each connection must have a ConnectionHandle wrapper.
      // In case of Http < 2 the a ConnectionHandle is created for each
      // transaction in DispatchAbstractTransaction.
      // In case of Http2/ and Http3, ConnectionHandle is created only once.
      // Http2 connection always starts as http1 connection and the first
      // transaction use DispatchAbstractTransaction to be dispatched and
      // a ConnectionHandle is created. All consecutive transactions for
      // Http2 use a short-cut in DispatchTransaction and call
      // HttpConnectionBase::Activate (DispatchAbstractTransaction is never
      // called).
      // In case of Http3 the short-cut HttpConnectionBase::Activate is always
      // used also for the first transaction, therefore we need to create
      // ConnectionHandle here.
      RefPtr<ConnectionHandle> handle = new ConnectionHandle(conn);
      pendingTransInfo->Transaction()->SetConnection(handle);
    }
    rv = gHttpHandler->ConnMgr()->DispatchTransaction(
        mEnt, pendingTransInfo->Transaction(), conn);
  } else {
    // this transaction was dispatched off the pending q before all the
    // sockets established themselves.

    // After about 1 second allow for the possibility of restarting a
    // transaction due to server close. Keep at sub 1 second as that is the
    // minimum granularity we can expect a server to be timing out with.
    RefPtr<nsHttpConnection> connTCP = do_QueryObject(conn);
    if (connTCP) {
      connTCP->SetIsReusedAfter(950);
    }

    // if we are using ssl and no other transactions are waiting right now,
    // then form a null transaction to drive the SSL handshake to
    // completion. Afterwards the connection will be 100% ready for the next
    // transaction to use it. Make an exception for SSL tunneled HTTP proxy as
    // the NullHttpTransaction does not know how to drive Connect
    // Http3 cannot be dispatched using OnMsgReclaimConnection (see below),
    // therefore we need to use a Nulltransaction.
    if (!connTCP ||
        (mEnt->mConnInfo->FirstHopSSL() && !mEnt->UrgentStartQueueLength() &&
         !mEnt->PendingQueueLength() && !mEnt->mConnInfo->UsingConnect())) {
      LOG(
          ("HalfOpenSocket::SetupConn null transaction will "
           "be used to finish SSL handshake on conn %p\n",
           conn.get()));
      RefPtr<nsAHttpTransaction> trans;
      if (mTransaction->IsNullTransaction() && !mDispatchedMTransaction) {
        // null transactions cannot be put in the entry queue, so that
        // explains why it is not present.
        mDispatchedMTransaction = true;
        trans = mTransaction;
      } else {
        trans = new NullHttpTransaction(mEnt->mConnInfo, callbacks, mCaps);
      }

      mEnt->InsertIntoActiveConns(conn);
      rv = gHttpHandler->ConnMgr()->DispatchAbstractTransaction(mEnt, trans,
                                                                mCaps, conn, 0);
    } else {
      // otherwise just put this in the persistent connection pool
      LOG(
          ("HalfOpenSocket::SetupConn no transaction match "
           "returning conn %p to pool\n",
           conn.get()));
      gHttpHandler->ConnMgr()->OnMsgReclaimConnection(conn);

      // We expect that there is at least one tranasction in the pending
      // queue that can take this connection, but it can happened that
      // all transactions are blocked or they have took other idle
      // connections. In that case the connection has been added to the
      // idle queue.
      // If the connection is in the idle queue but it is using ssl, make
      // a nulltransaction for it to finish ssl handshake!

      // !!! It can be that mEnt is null after OnMsgReclaimConnection.!!!
      if (mEnt && mEnt->mConnInfo->FirstHopSSL() &&
          !mEnt->mConnInfo->UsingConnect()) {
        RefPtr<nsHttpConnection> connTCP = do_QueryObject(conn);
        // If RemoveIdleConnection succeeds that means that conn is in the
        // idle queue.
        if (connTCP && NS_SUCCEEDED(mEnt->RemoveIdleConnection(connTCP))) {
          RefPtr<nsAHttpTransaction> trans;
          if (mTransaction->IsNullTransaction() && !mDispatchedMTransaction) {
            mDispatchedMTransaction = true;
            trans = mTransaction;
          } else {
            trans = new NullHttpTransaction(mEnt->mConnInfo, callbacks, mCaps);
          }
          mEnt->InsertIntoActiveConns(conn);
          rv = gHttpHandler->ConnMgr()->DispatchAbstractTransaction(
              mEnt, trans, mCaps, conn, 0);
        }
      }
    }
  }

  // If this halfOpenConn was speculative, but at the end the conn got a
  // non-null transaction than this halfOpen is not speculative anymore!
  if (conn->Transaction() && !conn->Transaction()->IsNullTransaction()) {
    Claim();
  }

  return rv;
}

// method for nsITransportEventSink
NS_IMETHODIMP
HalfOpenSocket::OnTransportStatus(nsITransport* trans, nsresult status,
                                  int64_t progress, int64_t progressMax) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  MOZ_ASSERT((trans == mSocketTransport) || (trans == mBackupTransport));
  MOZ_ASSERT(mEnt);
  if (mTransaction) {
    if ((trans == mSocketTransport) ||
        ((trans == mBackupTransport) &&
         (status == NS_NET_STATUS_CONNECTED_TO) && mSocketTransport)) {
      // Send this status event only if the transaction is still pending,
      // i.e. it has not found a free already connected socket.
      // Sockets in halfOpen state can only get following events:
      // NS_NET_STATUS_RESOLVING_HOST, NS_NET_STATUS_RESOLVED_HOST,
      // NS_NET_STATUS_CONNECTING_TO and NS_NET_STATUS_CONNECTED_TO.
      // mBackupTransport is only started after
      // NS_NET_STATUS_CONNECTING_TO of mSocketTransport, so ignore all
      // mBackupTransport events until NS_NET_STATUS_CONNECTED_TO.
      // mBackupTransport must be connected before mSocketTransport.
      mTransaction->OnTransportStatus(trans, status, progress);
    }
  }

  MOZ_ASSERT(trans == mSocketTransport || trans == mBackupTransport);
  if (status == NS_NET_STATUS_CONNECTED_TO) {
    if (trans == mSocketTransport) {
      mPrimaryConnectedOK = true;
    } else {
      mBackupConnectedOK = true;
    }
  }

  // The rest of this method only applies to the primary transport
  if (trans != mSocketTransport) {
    return NS_OK;
  }

  mPrimaryStreamStatus = status;

  // if we are doing spdy coalescing and haven't recorded the ip address
  // for this entry before then make the hash key if our dns lookup
  // just completed. We can't do coalescing if using a proxy because the
  // ip addresses are not available to the client.

  if (status == NS_NET_STATUS_CONNECTING_TO && gHttpHandler->IsSpdyEnabled() &&
      gHttpHandler->CoalesceSpdy() && mEnt && mEnt->mConnInfo &&
      mEnt->mConnInfo->EndToEndSSL() && mEnt->AllowHttp2() &&
      !mEnt->mConnInfo->UsingProxy() && mEnt->mCoalescingKeys.IsEmpty()) {
    nsCOMPtr<nsIDNSAddrRecord> dnsRecord(do_GetInterface(mSocketTransport));
    nsTArray<NetAddr> addressSet;
    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    if (dnsRecord) {
      rv = dnsRecord->GetAddresses(addressSet);
    }

    if (NS_SUCCEEDED(rv) && !addressSet.IsEmpty()) {
      for (uint32_t i = 0; i < addressSet.Length(); ++i) {
        if ((addressSet[i].raw.family == AF_INET &&
             addressSet[i].inet.ip == 0) ||
            (addressSet[i].raw.family == AF_INET6 &&
             addressSet[i].inet6.ip.u64[0] == 0 &&
             addressSet[i].inet6.ip.u64[1] == 0)) {
          // Bug 1680249 - Don't create the coalescing key if the ip address is
          // `0.0.0.0` or `::`.
          LOG(("HalfOpenSocket: skip creating Coalescing Key for host [%s]",
               mEnt->mConnInfo->Origin()));
          continue;
        }
        nsCString* newKey = mEnt->mCoalescingKeys.AppendElement(nsCString());
        newKey->SetLength(kIPv6CStrBufSize + 26);
        addressSet[i].ToStringBuffer(newKey->BeginWriting(), kIPv6CStrBufSize);
        newKey->SetLength(strlen(newKey->BeginReading()));
        if (mEnt->mConnInfo->GetAnonymous()) {
          newKey->AppendLiteral("~A:");
        } else {
          newKey->AppendLiteral("~.:");
        }
        newKey->AppendInt(mEnt->mConnInfo->OriginPort());
        newKey->AppendLiteral("/[");
        nsAutoCString suffix;
        mEnt->mConnInfo->GetOriginAttributes().CreateSuffix(suffix);
        newKey->Append(suffix);
        newKey->AppendLiteral("]viaDNS");
        LOG((
            "HalfOpenSocket::OnTransportStatus "
            "STATUS_CONNECTING_TO Established New Coalescing Key # %d for host "
            "%s [%s]",
            i, mEnt->mConnInfo->Origin(), newKey->get()));
      }
      gHttpHandler->ConnMgr()->ProcessSpdyPendingQ(mEnt);
    }
  }

  switch (status) {
    case NS_NET_STATUS_CONNECTING_TO:
      // Passed DNS resolution, now trying to connect, start the backup timer
      // only prevent creating another backup transport.
      // We also check for mEnt presence to not instantiate the timer after
      // this half open socket has already been abandoned.  It may happen
      // when we get this notification right between main-thread calls to
      // nsHttpConnectionMgr::Shutdown and nsSocketTransportService::Shutdown
      // where the first abandons all half open socket instances and only
      // after that the second stops the socket thread.
      // Http3 has its own syn-retransmission, therefore it does not need a
      // backup connection.
      if (mEnt && !mBackupTransport && !mSynTimer && !mIsHttp3) {
        SetupBackupTimer();
      }
      break;

    case NS_NET_STATUS_CONNECTED_TO:
      // TCP connection's up, now transfer or SSL negotiantion starts,
      // no need for backup socket
      CancelBackupTimer();
      break;

    default:
      break;
  }

  return NS_OK;
}

// method for nsIInterfaceRequestor
NS_IMETHODIMP
HalfOpenSocket::GetInterface(const nsIID& iid, void** result) {
  if (mTransaction) {
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    mTransaction->GetSecurityCallbacks(getter_AddRefs(callbacks));
    if (callbacks) {
      return callbacks->GetInterface(iid, result);
    }
  }
  return NS_ERROR_NO_INTERFACE;
}

bool HalfOpenSocket::AcceptsTransaction(nsHttpTransaction* trans) {
  // When marked as urgent start, only accept urgent start marked transactions.
  // Otherwise, accept any kind of transaction.
  return !mUrgentStart || (trans->Caps() & nsIClassOfService::UrgentStart);
}

bool HalfOpenSocket::Claim() {
  if (mSpeculative) {
    mSpeculative = false;
    uint32_t flags;
    if (mSocketTransport &&
        NS_SUCCEEDED(mSocketTransport->GetConnectionFlags(&flags))) {
      flags &= ~nsISocketTransport::DISABLE_RFC1918;
      mSocketTransport->SetConnectionFlags(flags);
    }

    Telemetry::AutoCounter<Telemetry::HTTPCONNMGR_USED_SPECULATIVE_CONN>
        usedSpeculativeConn;
    ++usedSpeculativeConn;

    if (mIsFromPredictor) {
      Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PRECONNECTS_USED>
          totalPreconnectsUsed;
      ++totalPreconnectsUsed;
    }

    // Http3 has its own syn-retransmission, therefore it does not need a
    // backup connection.
    if ((mPrimaryStreamStatus == NS_NET_STATUS_CONNECTING_TO) && mEnt &&
        !mBackupTransport && !mSynTimer && !mIsHttp3) {
      SetupBackupTimer();
    }
  }

  if (mFreeToUse) {
    mFreeToUse = false;
    return true;
  }
  return false;
}

void HalfOpenSocket::Unclaim() {
  MOZ_ASSERT(!mSpeculative && !mFreeToUse);
  // We will keep the backup-timer running. Most probably this halfOpen will
  // be used by a transaction from which this transaction took the halfOpen.
  // (this is happening because of the transaction priority.)
  mFreeToUse = true;
}

}  // namespace net
}  // namespace mozilla
