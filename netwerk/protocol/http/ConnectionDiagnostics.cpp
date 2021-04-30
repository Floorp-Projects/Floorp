/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttpConnectionMgr.h"
#include "nsHttpConnection.h"
#include "HttpConnectionUDP.h"
#include "Http2Session.h"
#include "nsHttpHandler.h"
#include "nsIConsoleService.h"
#include "nsHttpRequestHead.h"
#include "nsServiceManagerUtils.h"
#include "nsSocketTransportService2.h"

#include "mozilla/IntegerPrintfMacros.h"

namespace mozilla {
namespace net {

void nsHttpConnectionMgr::PrintDiagnostics() {
  nsresult rv =
      PostEvent(&nsHttpConnectionMgr::OnMsgPrintDiagnostics, 0, nullptr);
  if (NS_FAILED(rv)) {
    LOG(
        ("nsHttpConnectionMgr::PrintDiagnostics\n"
         "  failed to post OnMsgPrintDiagnostics event"));
  }
}

void nsHttpConnectionMgr::OnMsgPrintDiagnostics(int32_t, ARefBase*) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (!consoleService) return;

  mLogData.AppendPrintf("HTTP Connection Diagnostics\n---------------------\n");
  mLogData.AppendPrintf("IsSpdyEnabled() = %d\n",
                        gHttpHandler->IsSpdyEnabled());
  mLogData.AppendPrintf("MaxSocketCount() = %d\n",
                        gHttpHandler->MaxSocketCount());
  mLogData.AppendPrintf("mNumActiveConns = %d\n", mNumActiveConns);
  mLogData.AppendPrintf("mNumIdleConns = %d\n", mNumIdleConns);

  for (RefPtr<ConnectionEntry> ent : mCT.Values()) {
    mLogData.AppendPrintf(
        "   AtActiveConnectionLimit = %d\n",
        AtActiveConnectionLimit(ent, NS_HTTP_ALLOW_KEEPALIVE));

    ent->PrintDiagnostics(mLogData, MaxPersistConnections(ent));
  }

  consoleService->LogStringMessage(NS_ConvertUTF8toUTF16(mLogData).Data());
  mLogData.Truncate();
}

void ConnectionEntry::PrintDiagnostics(nsCString& log,
                                       uint32_t aMaxPersistConns) {
  log.AppendPrintf(" ent host = %s hashkey = %s\n", mConnInfo->Origin(),
                   mConnInfo->HashKey().get());

  log.AppendPrintf("   RestrictConnections = %d\n", RestrictConnections());
  log.AppendPrintf("   Pending Q Length = %zu\n", PendingQueueLength());
  log.AppendPrintf("   Active Conns Length = %zu\n", mActiveConns.Length());
  log.AppendPrintf("   Idle Conns Length = %zu\n", mIdleConns.Length());
  log.AppendPrintf("   DnsAndSock Length = %zu\n",
                   mDnsAndConnectSockets.Length());
  log.AppendPrintf("   Coalescing Keys Length = %zu\n",
                   mCoalescingKeys.Length());
  log.AppendPrintf("   Spdy using = %d\n", mUsingSpdy);

  uint32_t i;
  for (i = 0; i < mActiveConns.Length(); ++i) {
    log.AppendPrintf("   :: Active Connection #%u\n", i);
    mActiveConns[i]->PrintDiagnostics(log);
  }
  for (i = 0; i < mIdleConns.Length(); ++i) {
    log.AppendPrintf("   :: Idle Connection #%u\n", i);
    mIdleConns[i]->PrintDiagnostics(log);
  }
  for (i = 0; i < mDnsAndConnectSockets.Length(); ++i) {
    log.AppendPrintf("   :: Half Open #%u\n", i);
    mDnsAndConnectSockets[i]->PrintDiagnostics(log);
  }

  mPendingQ.PrintDiagnostics(log);

  for (i = 0; i < mCoalescingKeys.Length(); ++i) {
    log.AppendPrintf("   :: Coalescing Key #%u %s\n", i,
                     mCoalescingKeys[i].get());
  }
}

void PendingTransactionQueue::PrintDiagnostics(nsCString& log) {
  uint32_t i = 0;
  for (const auto& entry : mPendingTransactionTable) {
    log.AppendPrintf("   :: Pending Transactions with Window ID = %" PRIu64
                     "\n",
                     entry.GetKey());
    for (uint32_t j = 0; j < entry.GetData()->Length(); ++j) {
      log.AppendPrintf("     ::: Pending Transaction #%u\n", i);
      entry.GetData()->ElementAt(j)->PrintDiagnostics(log);
      ++i;
    }
  }
}

void DnsAndConnectSocket::PrintDiagnostics(nsCString& log) {
  log.AppendPrintf("     has connected = %d, isSpeculative = %d\n",
                   HasConnected(), IsSpeculative());

  TimeStamp now = TimeStamp::Now();

  if (mPrimaryTransport.mSynStarted.IsNull()) {
    log.AppendPrintf("    primary not started\n");
  } else {
    log.AppendPrintf("    primary started %.2fms ago\n",
                     (now - mPrimaryTransport.mSynStarted).ToMilliseconds());
  }

  if (mBackupTransport.mSynStarted.IsNull()) {
    log.AppendPrintf("    backup not started\n");
  } else {
    log.AppendPrintf("    backup started %.2f ago\n",
                     (now - mBackupTransport.mSynStarted).ToMilliseconds());
  }

  log.AppendPrintf("    primary transport %d, backup transport %d\n",
                   !!mPrimaryTransport.mSocketTransport,
                   !!mBackupTransport.mSocketTransport);
}

void nsHttpConnection::PrintDiagnostics(nsCString& log) {
  log.AppendPrintf("    CanDirectlyActivate = %d\n", CanDirectlyActivate());

  log.AppendPrintf("    npncomplete = %d  setupSSLCalled = %d\n", mNPNComplete,
                   mSetupSSLCalled);

  log.AppendPrintf("    spdyVersion = %d  reportedSpdy = %d everspdy = %d\n",
                   static_cast<int32_t>(mUsingSpdyVersion), mReportedSpdy,
                   mEverUsedSpdy);

  log.AppendPrintf("    iskeepalive = %d  dontReuse = %d isReused = %d\n",
                   IsKeepAlive(), mDontReuse, mIsReused);

  log.AppendPrintf("    mTransaction = %d mSpdySession = %d\n",
                   !!mTransaction.get(), !!mSpdySession.get());

  PRIntervalTime now = PR_IntervalNow();
  log.AppendPrintf("    time since last read = %ums\n",
                   PR_IntervalToMilliseconds(now - mLastReadTime));

  log.AppendPrintf("    max-read/read/written %" PRId64 "/%" PRId64 "/%" PRId64
                   "\n",
                   mMaxBytesRead, mTotalBytesRead, mTotalBytesWritten);

  log.AppendPrintf("    rtt = %ums\n", PR_IntervalToMilliseconds(mRtt));

  log.AppendPrintf("    idlemonitoring = %d transactionCount=%d\n",
                   mIdleMonitoring, mHttp1xTransactionCount);

  if (mSpdySession) mSpdySession->PrintDiagnostics(log);
}

void HttpConnectionUDP::PrintDiagnostics(nsCString& log) {
  log.AppendPrintf("    CanDirectlyActivate = %d\n", CanDirectlyActivate());

  log.AppendPrintf("    dontReuse = %d isReused = %d\n", mDontReuse, mIsReused);

  log.AppendPrintf("    read/written %" PRId64 "/%" PRId64 "\n",
                   mHttp3Session ? mHttp3Session->BytesRead() : 0,
                   mHttp3Session ? mHttp3Session->GetBytesWritten() : 0);

  log.AppendPrintf("    rtt = %ums\n", PR_IntervalToMilliseconds(mRtt));
}

void Http2Session::PrintDiagnostics(nsCString& log) {
  log.AppendPrintf("     ::: HTTP2\n");
  log.AppendPrintf(
      "     shouldgoaway = %d mClosed = %d CanReuse = %d nextID=0x%X\n",
      mShouldGoAway, mClosed, CanReuse(), mNextStreamID);

  log.AppendPrintf("     concurrent = %d maxconcurrent = %d\n", mConcurrent,
                   mMaxConcurrent);

  log.AppendPrintf("     roomformorestreams = %d roomformoreconcurrent = %d\n",
                   RoomForMoreStreams(), RoomForMoreConcurrent());

  log.AppendPrintf("     transactionHashCount = %d streamIDHashCount = %d\n",
                   mStreamTransactionHash.Count(), mStreamIDHash.Count());

  log.AppendPrintf("     Queued Stream Size = %zu\n", mQueuedStreams.Length());

  PRIntervalTime now = PR_IntervalNow();
  log.AppendPrintf("     Ping Threshold = %ums\n",
                   PR_IntervalToMilliseconds(mPingThreshold));
  log.AppendPrintf("     Ping Timeout = %ums\n",
                   PR_IntervalToMilliseconds(gHttpHandler->SpdyPingTimeout()));
  log.AppendPrintf("     Idle for Any Activity (ping) = %ums\n",
                   PR_IntervalToMilliseconds(now - mLastReadEpoch));
  log.AppendPrintf("     Idle for Data Activity = %ums\n",
                   PR_IntervalToMilliseconds(now - mLastDataReadEpoch));
  if (mPingSentEpoch)
    log.AppendPrintf("     Ping Outstanding (ping) = %ums, expired = %d\n",
                     PR_IntervalToMilliseconds(now - mPingSentEpoch),
                     now - mPingSentEpoch >= gHttpHandler->SpdyPingTimeout());
  else
    log.AppendPrintf("     No Ping Outstanding\n");
}

void nsHttpTransaction::PrintDiagnostics(nsCString& log) {
  if (!mRequestHead) return;

  nsAutoCString requestURI;
  mRequestHead->RequestURI(requestURI);
  log.AppendPrintf("       :::: uri = %s\n", requestURI.get());
  log.AppendPrintf("       caps = 0x%x\n", mCaps);
  log.AppendPrintf("       priority = %d\n", mPriority);
  log.AppendPrintf("       restart count = %u\n", mRestartCount);
}

void PendingTransactionInfo::PrintDiagnostics(nsCString& log) {
  log.AppendPrintf("     ::: Pending transaction\n");
  mTransaction->PrintDiagnostics(log);
  RefPtr<DnsAndConnectSocket> dnsAndSock = do_QueryReferent(mDnsAndSock);
  log.AppendPrintf("     Waiting for half open sock: %p or connection: %p\n",
                   dnsAndSock.get(), mActiveConn.get());
}

}  // namespace net
}  // namespace mozilla
