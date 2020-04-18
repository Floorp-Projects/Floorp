/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "Http2Session.h"
#include "nsHttp.h"
#include "nsHttpHandler.h"
#include "nsHttpRequestHead.h"
#include "TCPFastOpen.h"
#include "nsISocketProvider.h"
#include "nsSocketProviderService.h"
#include "nsISSLSocketControl.h"
#include "nsISocketTransport.h"
#include "nsISupportsPriority.h"
#include "nsNetAddr.h"
#include "prerror.h"
#include "prio.h"
#include "TunnelUtils.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsSocketTransport2.h"
#include "nsSocketTransportService2.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Mutex.h"

#if defined(FUZZING)
#  include "FuzzySecurityInfo.h"
#  include "mozilla/StaticPrefs_fuzzing.h"
#endif

namespace mozilla {
namespace net {

static PRDescIdentity sLayerIdentity;
static PRIOMethods sLayerMethods;
static PRIOMethods* sLayerMethodsPtr = nullptr;

TLSFilterTransaction::TLSFilterTransaction(nsAHttpTransaction* aWrapped,
                                           const char* aTLSHost,
                                           int32_t aTLSPort,
                                           nsAHttpSegmentReader* aReader,
                                           nsAHttpSegmentWriter* aWriter)
    : mTransaction(aWrapped),
      mEncryptedTextUsed(0),
      mEncryptedTextSize(0),
      mSegmentReader(aReader),
      mSegmentWriter(aWriter),
      mFilterReadCode(NS_ERROR_NOT_INITIALIZED),
      mFilterReadAmount(0),
      mInOnReadSegment(false),
      mForce(false),
      mReadSegmentReturnValue(NS_OK),
      mCloseReason(NS_ERROR_UNEXPECTED),
      mNudgeCounter(0) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("TLSFilterTransaction ctor %p\n", this));

  nsCOMPtr<nsISocketProvider> provider;
  nsCOMPtr<nsISocketProviderService> spserv =
      nsSocketProviderService::GetOrCreate();

  if (spserv) {
    spserv->GetSocketProvider("ssl", getter_AddRefs(provider));
  }

  // Install an NSPR layer to handle getpeername() with a failure. This is kind
  // of silly, but the default one used by the pipe asserts when called and the
  // nss code calls it to see if we are connected to a real socket or not.
  if (!sLayerMethodsPtr) {
    // one time initialization
    sLayerIdentity = PR_GetUniqueIdentity("TLSFilterTransaction Layer");
    sLayerMethods = *PR_GetDefaultIOMethods();
    sLayerMethods.getpeername = GetPeerName;
    sLayerMethods.getsocketoption = GetSocketOption;
    sLayerMethods.setsocketoption = SetSocketOption;
    sLayerMethods.read = FilterRead;
    sLayerMethods.write = FilterWrite;
    sLayerMethods.send = FilterSend;
    sLayerMethods.recv = FilterRecv;
    sLayerMethods.close = FilterClose;
    sLayerMethodsPtr = &sLayerMethods;
  }

  mFD = PR_CreateIOLayerStub(sLayerIdentity, &sLayerMethods);

  bool addTLSLayer = true;
#ifdef FUZZING
  addTLSLayer = !StaticPrefs::fuzzing_necko_enabled();
  if (!addTLSLayer) {
    SOCKET_LOG(("Skipping TLS layer in TLSFilterTransaction for fuzzing.\n"));

    mSecInfo = static_cast<nsISupports*>(
        static_cast<nsISSLSocketControl*>(new FuzzySecurityInfo()));
  }
#endif

  if (provider && mFD) {
    mFD->secret = reinterpret_cast<PRFilePrivate*>(this);

    if (addTLSLayer) {
      provider->AddToSocket(PR_AF_INET, aTLSHost, aTLSPort, nullptr,
                            OriginAttributes(), 0, 0, mFD,
                            getter_AddRefs(mSecInfo));
    }
  }

  if (mTransaction) {
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    mTransaction->GetSecurityCallbacks(getter_AddRefs(callbacks));
    nsCOMPtr<nsISSLSocketControl> secCtrl(do_QueryInterface(mSecInfo));
    if (secCtrl) {
      secCtrl->SetNotificationCallbacks(callbacks);
    }
  }
}

TLSFilterTransaction::~TLSFilterTransaction() {
  LOG(("TLSFilterTransaction dtor %p\n", this));

  // Prevent call to OnReadSegment from FilterOutput, our mSegmentReader is now
  // an invalid pointer.
  mInOnReadSegment = true;

  Cleanup();
}

void TLSFilterTransaction::Cleanup() {
  LOG(("TLSFilterTransaction::Cleanup %p", this));

  if (mTransaction) {
    mTransaction->Close(NS_ERROR_ABORT);
    mTransaction = nullptr;
  }

  if (mFD) {
    PR_Close(mFD);
    mFD = nullptr;
  }
  mSecInfo = nullptr;
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

void TLSFilterTransaction::Close(nsresult aReason) {
  LOG(("TLSFilterTransaction::Close %p %" PRIx32, this,
       static_cast<uint32_t>(aReason)));

  if (!mTransaction) {
    return;
  }

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
  mTransaction->Close(aReason);
  mTransaction = nullptr;

  if (!gHttpHandler->Bug1563695()) {
    RefPtr<NullHttpTransaction> baseTrans(do_QueryReferent(mWeakTrans));
    SpdyConnectTransaction* trans =
        baseTrans ? baseTrans->QuerySpdyConnectTransaction() : nullptr;

    LOG(("TLSFilterTransaction::Close %p aReason=%" PRIx32 " trans=%p\n", this,
         static_cast<uint32_t>(aReason), trans));

    if (trans) {
      trans->Close(aReason);
      trans = nullptr;
    }
  }

  if (gHttpHandler->Bug1563538()) {
    if (NS_FAILED(aReason)) {
      mCloseReason = aReason;
    } else {
      mCloseReason = NS_BASE_STREAM_CLOSED;
    }
  } else {
    MOZ_ASSERT(NS_ERROR_UNEXPECTED == mCloseReason);
  }
}

nsresult TLSFilterTransaction::OnReadSegment(const char* aData, uint32_t aCount,
                                             uint32_t* outCountRead) {
  LOG(("TLSFilterTransaction %p OnReadSegment %d (buffered %d)\n", this, aCount,
       mEncryptedTextUsed));

  mReadSegmentReturnValue = NS_OK;
  MOZ_ASSERT(mSegmentReader);
  if (!mSecInfo) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  *outCountRead = 0;

  // get rid of buffer first
  if (mEncryptedTextUsed) {
    rv = mSegmentReader->CommitToSegmentSize(mEncryptedTextUsed, mForce);
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      return rv;
    }

    uint32_t amt;
    rv = mSegmentReader->OnReadSegment(mEncryptedText.get(), mEncryptedTextUsed,
                                       &amt);
    if (NS_FAILED(rv)) {
      return rv;
    }

    mEncryptedTextUsed -= amt;
    if (mEncryptedTextUsed) {
      memmove(mEncryptedText.get(), &mEncryptedText[amt], mEncryptedTextUsed);
      return NS_OK;
    }
  }

  // encrypt for network write
  // write aData down the SSL layer into the FilterWrite() method where it will
  // be queued into mEncryptedText. We need to copy it like this in order to
  // guarantee atomic writes

  EnsureBuffer(mEncryptedText, aCount + 4096, 0, mEncryptedTextSize);

  // Prevents call to OnReadSegment from inside FilterOutput, as we handle it
  // here.
  AutoRestore<bool> inOnReadSegment(mInOnReadSegment);
  mInOnReadSegment = true;

  while (aCount > 0) {
    int32_t written = PR_Write(mFD, aData, aCount);
    LOG(("TLSFilterTransaction %p OnReadSegment PRWrite(%d) = %d %d\n", this,
         aCount, written, PR_GetError() == PR_WOULD_BLOCK_ERROR));

    if (written < 1) {
      if (*outCountRead) {
        return NS_OK;
      }
      // mTransaction ReadSegments actually obscures this code, so
      // keep it in a member var for this::ReadSegments to inspect. Similar
      // to nsHttpConnection::mSocketOutCondition
      PRErrorCode code = PR_GetError();
      mReadSegmentReturnValue = ErrorAccordingToNSPR(code);

      return mReadSegmentReturnValue;
    }
    aCount -= written;
    aData += written;
    *outCountRead += written;
    mNudgeCounter = 0;
  }

  LOG(("TLSFilterTransaction %p OnReadSegment2 (buffered %d)\n", this,
       mEncryptedTextUsed));

  uint32_t amt = 0;
  if (mEncryptedTextUsed) {
    // If we are tunneled on spdy CommitToSegmentSize will prevent partial
    // writes that could interfere with multiplexing. H1 is fine with
    // partial writes.
    rv = mSegmentReader->CommitToSegmentSize(mEncryptedTextUsed, mForce);
    if (rv != NS_BASE_STREAM_WOULD_BLOCK) {
      rv = mSegmentReader->OnReadSegment(mEncryptedText.get(),
                                         mEncryptedTextUsed, &amt);
    }

    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      // return OK because all the data was consumed and stored in this buffer
      // It is fine if the connection is null.  We are likely a websocket and
      // thus writing push is ensured by the caller.
      if (Connection()) {
        Connection()->TransactionHasDataToWrite(this);
      }
      return NS_OK;
    }
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (amt == mEncryptedTextUsed) {
    mEncryptedText = nullptr;
    mEncryptedTextUsed = 0;
    mEncryptedTextSize = 0;
  } else {
    memmove(mEncryptedText.get(), &mEncryptedText[amt],
            mEncryptedTextUsed - amt);
    mEncryptedTextUsed -= amt;
  }
  return NS_OK;
}

int32_t TLSFilterTransaction::FilterOutput(const char* aBuf, int32_t aAmount) {
  EnsureBuffer(mEncryptedText, mEncryptedTextUsed + aAmount, mEncryptedTextUsed,
               mEncryptedTextSize);
  memcpy(&mEncryptedText[mEncryptedTextUsed], aBuf, aAmount);
  mEncryptedTextUsed += aAmount;

  LOG(("TLSFilterTransaction::FilterOutput %p %d buffered=%u mSegmentReader=%p",
       this, aAmount, mEncryptedTextUsed, mSegmentReader));

  if (!mInOnReadSegment) {
    // When called externally, we must make sure any newly written data is
    // actually sent to the higher level connection.
    // This also covers the case when PR_Read() wrote a re-negotioation
    // response.
    uint32_t notUsed;
    Unused << OnReadSegment("", 0, &notUsed);
  }

  return aAmount;
}

nsresult TLSFilterTransaction::CommitToSegmentSize(uint32_t size,
                                                   bool forceCommitment) {
  if (!mSegmentReader) {
    return NS_ERROR_FAILURE;
  }

  // pad the commit by a little bit to leave room for encryption overhead
  // this isn't foolproof and we may still have to buffer, but its a good start
  mForce = forceCommitment;
  return mSegmentReader->CommitToSegmentSize(size + 1024, forceCommitment);
}

nsresult TLSFilterTransaction::OnWriteSegment(char* aData, uint32_t aCount,
                                              uint32_t* outCountRead) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(mSegmentWriter);
  LOG(("TLSFilterTransaction::OnWriteSegment %p max=%d\n", this, aCount));
  if (!mSecInfo) {
    return NS_ERROR_FAILURE;
  }

  // this will call through to FilterInput to get data from the higher
  // level connection before removing the local TLS layer
  mFilterReadCode = NS_OK;
  mFilterReadAmount = 0;
  int32_t bytesRead = PR_Read(mFD, aData, aCount);
  if (bytesRead == -1) {
    PRErrorCode code = PR_GetError();
    if (code == PR_WOULD_BLOCK_ERROR) {
      LOG(
          ("TLSFilterTransaction::OnWriteSegment %p PR_Read would block, "
           "actual read: %d\n",
           this, mFilterReadAmount));

      if (mFilterReadAmount == 0 && NS_SUCCEEDED(mFilterReadCode)) {
        // No reading happened, but also no error occured, hence there is no
        // condition to break the `again` loop, propagate WOULD_BLOCK through
        // mFilterReadCode to break it and poll the socket again for reading.
        mFilterReadCode = NS_BASE_STREAM_WOULD_BLOCK;
      }
      return NS_BASE_STREAM_WOULD_BLOCK;
    }
    // If reading from the socket succeeded (NS_SUCCEEDED(mFilterReadCode)),
    // but the nss layer encountered an error remember the error.
    if (NS_SUCCEEDED(mFilterReadCode)) {
      mFilterReadCode = ErrorAccordingToNSPR(code);
      LOG(("TLSFilterTransaction::OnWriteSegment %p nss error %" PRIx32 ".\n",
           this, static_cast<uint32_t>(mFilterReadCode)));
    }
    return mFilterReadCode;
  }
  *outCountRead = bytesRead;

  if (NS_SUCCEEDED(mFilterReadCode) && !bytesRead) {
    LOG(
        ("TLSFilterTransaction::OnWriteSegment %p "
         "Second layer of TLS stripping results in STREAM_CLOSED\n",
         this));
    mFilterReadCode = NS_BASE_STREAM_CLOSED;
  }

  LOG(("TLSFilterTransaction::OnWriteSegment %p rv=%" PRIx32 " didread=%d "
       "2 layers of ssl stripped to plaintext\n",
       this, static_cast<uint32_t>(mFilterReadCode), bytesRead));
  return mFilterReadCode;
}

int32_t TLSFilterTransaction::FilterInput(char* aBuf, int32_t aAmount) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(mSegmentWriter);
  LOG(("TLSFilterTransaction::FilterInput max=%d\n", aAmount));

  uint32_t outCountRead = 0;
  mFilterReadCode =
      mSegmentWriter->OnWriteSegment(aBuf, aAmount, &outCountRead);
  if (NS_SUCCEEDED(mFilterReadCode) && outCountRead) {
    LOG(("TLSFilterTransaction::FilterInput rv=%" PRIx32
         " read=%d input from net "
         "1 layer stripped, 1 still on\n",
         static_cast<uint32_t>(mFilterReadCode), outCountRead));
    if (mReadSegmentReturnValue == NS_BASE_STREAM_WOULD_BLOCK) {
      mNudgeCounter = 0;
    }

    mFilterReadAmount += outCountRead;
  }
  if (mFilterReadCode == NS_BASE_STREAM_WOULD_BLOCK) {
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  }
  return outCountRead;
}

nsresult TLSFilterTransaction::ReadSegments(nsAHttpSegmentReader* aReader,
                                            uint32_t aCount,
                                            uint32_t* outCountRead) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("TLSFilterTransaction::ReadSegments %p max=%d\n", this, aCount));

  if (!mTransaction) {
    return mCloseReason;
  }

  mReadSegmentReturnValue = NS_OK;
  mSegmentReader = aReader;
  nsresult rv = mTransaction->ReadSegments(this, aCount, outCountRead);

  // mSegmentReader is left assigned (not nullified) because we want to be able
  // to call OnReadSegment directly, it expects mSegmentReader be non-null.

  LOG(("TLSFilterTransaction %p called trans->ReadSegments rv=%" PRIx32 " %d\n",
       this, static_cast<uint32_t>(rv), *outCountRead));
  if (NS_SUCCEEDED(rv) &&
      (mReadSegmentReturnValue == NS_BASE_STREAM_WOULD_BLOCK)) {
    LOG(("TLSFilterTransaction %p read segment blocked found rv=%" PRIx32 "\n",
         this, static_cast<uint32_t>(rv)));
    if (Connection()) {
      Unused << Connection()->ForceSend();
    }
  }

  return NS_SUCCEEDED(rv) ? mReadSegmentReturnValue : rv;
}

nsresult TLSFilterTransaction::WriteSegmentsAgain(nsAHttpSegmentWriter* aWriter,
                                                  uint32_t aCount,
                                                  uint32_t* outCountWritten,
                                                  bool* again) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("TLSFilterTransaction::WriteSegmentsAgain %p max=%d\n", this, aCount));

  if (!mTransaction) {
    return mCloseReason;
  }

  mSegmentWriter = aWriter;

  nsresult rv =
      mTransaction->WriteSegmentsAgain(this, aCount, outCountWritten, again);

  if (NS_SUCCEEDED(rv) && !(*outCountWritten) && NS_FAILED(mFilterReadCode)) {
    // nsPipe turns failures into silent OK.. undo that!
    rv = mFilterReadCode;
    if (Connection() && (mFilterReadCode == NS_BASE_STREAM_WOULD_BLOCK)) {
      Unused << Connection()->ResumeRecv();
    }
  }
  LOG(("TLSFilterTransaction %p called trans->WriteSegments rv=%" PRIx32
       " %d\n",
       this, static_cast<uint32_t>(rv), *outCountWritten));
  return rv;
}

nsresult TLSFilterTransaction::WriteSegments(nsAHttpSegmentWriter* aWriter,
                                             uint32_t aCount,
                                             uint32_t* outCountWritten) {
  bool again = false;
  return WriteSegmentsAgain(aWriter, aCount, outCountWritten, &again);
}

nsresult TLSFilterTransaction::GetTransactionSecurityInfo(
    nsISupports** outSecInfo) {
  if (!mSecInfo) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISupports> temp(mSecInfo);
  temp.forget(outSecInfo);
  return NS_OK;
}

nsresult TLSFilterTransaction::NudgeTunnel(NudgeTunnelCallback* aCallback) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("TLSFilterTransaction %p NudgeTunnel\n", this));
  mNudgeCallback = nullptr;

  if (!mSecInfo) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISSLSocketControl> ssl(do_QueryInterface(mSecInfo));
  nsresult rv = ssl ? ssl->DriveHandshake() : NS_ERROR_FAILURE;
  if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK) {
    // fatal handshake failure
    LOG(("TLSFilterTransaction %p Fatal Handshake Failure: %d\n", this,
         PR_GetError()));
    return NS_ERROR_FAILURE;
  }

  uint32_t notUsed;
  Unused << OnReadSegment("", 0, &notUsed);

  // The SSL Layer does some unusual things with PR_Poll that makes it a bad
  // match for multiplexed SSL sessions. We work around this by manually polling
  // for the moment during the brief handshake phase or otherwise blocked on
  // write. Thankfully this is a pretty unusual state. NSPR doesn't help us here
  // - asserting when polling without the NSPR IO layer on the bottom of the
  // stack. As a follow-on we can do some NSPR and maybe libssl changes to make
  // this more event driven, but this is acceptable for getting started.

  uint32_t counter = mNudgeCounter++;
  uint32_t delay;

  if (!counter) {
    delay = 0;
  } else if (counter < 8) {  // up to 48ms at 6
    delay = 6;
  } else if (counter < 34) {  // up to 499 ms at 17ms
    delay = 17;
  } else {  // after that at 51ms (3 old windows ticks)
    delay = 51;
  }

  if (!mTimer) {
    mTimer = NS_NewTimer();
  }

  mNudgeCallback = aCallback;
  if (!mTimer || NS_FAILED(mTimer->InitWithCallback(this, delay,
                                                    nsITimer::TYPE_ONE_SHOT))) {
    return StartTimerCallback();
  }

  LOG(("TLSFilterTransaction %p NudgeTunnel timer started\n", this));
  return NS_OK;
}

NS_IMETHODIMP
TLSFilterTransaction::Notify(nsITimer* timer) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("TLSFilterTransaction %p NudgeTunnel notify\n", this));

  if (timer != mTimer) {
    return NS_ERROR_UNEXPECTED;
  }
  nsresult rv = StartTimerCallback();
  if (NS_FAILED(rv)) {
    Close(rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
TLSFilterTransaction::GetName(nsACString& aName) {
  aName.AssignLiteral("TLSFilterTransaction");
  return NS_OK;
}

nsresult TLSFilterTransaction::StartTimerCallback() {
  LOG(("TLSFilterTransaction %p NudgeTunnel StartTimerCallback %p\n", this,
       mNudgeCallback.get()));

  if (mNudgeCallback) {
    // This class can be called re-entrantly, so cleanup m* before ->on()
    RefPtr<NudgeTunnelCallback> cb(mNudgeCallback);
    mNudgeCallback = nullptr;
    return cb->OnTunnelNudged(this);
  }
  return NS_OK;
}

bool TLSFilterTransaction::HasDataToRecv() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  if (!mFD) {
    return false;
  }
  int32_t n = 0;
  char c;
  n = PR_Recv(mFD, &c, 1, PR_MSG_PEEK, 0);
  return n > 0;
}

PRStatus TLSFilterTransaction::GetPeerName(PRFileDesc* aFD, PRNetAddr* addr) {
  NetAddr peeraddr;
  TLSFilterTransaction* self =
      reinterpret_cast<TLSFilterTransaction*>(aFD->secret);

  if (!self->mTransaction ||
      NS_FAILED(self->mTransaction->Connection()->Transport()->GetPeerAddr(
          &peeraddr))) {
    return PR_FAILURE;
  }
  NetAddrToPRNetAddr(&peeraddr, addr);
  return PR_SUCCESS;
}

PRStatus TLSFilterTransaction::GetSocketOption(PRFileDesc* aFD,
                                               PRSocketOptionData* aOpt) {
  if (aOpt->option == PR_SockOpt_Nonblocking) {
    aOpt->value.non_blocking = PR_TRUE;
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

PRStatus TLSFilterTransaction::SetSocketOption(PRFileDesc* aFD,
                                               const PRSocketOptionData* aOpt) {
  return PR_FAILURE;
}

PRStatus TLSFilterTransaction::FilterClose(PRFileDesc* aFD) {
  return PR_SUCCESS;
}

int32_t TLSFilterTransaction::FilterWrite(PRFileDesc* aFD, const void* aBuf,
                                          int32_t aAmount) {
  TLSFilterTransaction* self =
      reinterpret_cast<TLSFilterTransaction*>(aFD->secret);
  return self->FilterOutput(static_cast<const char*>(aBuf), aAmount);
}

int32_t TLSFilterTransaction::FilterSend(PRFileDesc* aFD, const void* aBuf,
                                         int32_t aAmount, int, PRIntervalTime) {
  return FilterWrite(aFD, aBuf, aAmount);
}

int32_t TLSFilterTransaction::FilterRead(PRFileDesc* aFD, void* aBuf,
                                         int32_t aAmount) {
  TLSFilterTransaction* self =
      reinterpret_cast<TLSFilterTransaction*>(aFD->secret);
  return self->FilterInput(static_cast<char*>(aBuf), aAmount);
}

int32_t TLSFilterTransaction::FilterRecv(PRFileDesc* aFD, void* aBuf,
                                         int32_t aAmount, int, PRIntervalTime) {
  return FilterRead(aFD, aBuf, aAmount);
}

/////
// The other methods of TLSFilterTransaction just call mTransaction->method
/////

void TLSFilterTransaction::SetConnection(nsAHttpConnection* aConnection) {
  if (!mTransaction) {
    return;
  }

  mTransaction->SetConnection(aConnection);
}

nsAHttpConnection* TLSFilterTransaction::Connection() {
  if (!mTransaction) {
    return nullptr;
  }
  return mTransaction->Connection();
}

void TLSFilterTransaction::GetSecurityCallbacks(nsIInterfaceRequestor** outCB) {
  if (!mTransaction) {
    return;
  }
  mTransaction->GetSecurityCallbacks(outCB);
}

void TLSFilterTransaction::OnTransportStatus(nsITransport* aTransport,
                                             nsresult aStatus,
                                             int64_t aProgress) {
  if (!mTransaction) {
    return;
  }
  mTransaction->OnTransportStatus(aTransport, aStatus, aProgress);
}

nsHttpConnectionInfo* TLSFilterTransaction::ConnectionInfo() {
  if (!mTransaction) {
    return nullptr;
  }
  return mTransaction->ConnectionInfo();
}

bool TLSFilterTransaction::IsDone() {
  if (!mTransaction) {
    return true;
  }
  return mTransaction->IsDone();
}

nsresult TLSFilterTransaction::Status() {
  if (!mTransaction) {
    return NS_ERROR_UNEXPECTED;
  }

  return mTransaction->Status();
}

uint32_t TLSFilterTransaction::Caps() {
  if (!mTransaction) {
    return 0;
  }

  return mTransaction->Caps();
}

void TLSFilterTransaction::SetProxyConnectFailed() {
  if (!mTransaction) {
    return;
  }

  mTransaction->SetProxyConnectFailed();
}

nsHttpRequestHead* TLSFilterTransaction::RequestHead() {
  if (!mTransaction) {
    return nullptr;
  }

  return mTransaction->RequestHead();
}

uint32_t TLSFilterTransaction::Http1xTransactionCount() {
  if (!mTransaction) {
    return 0;
  }

  return mTransaction->Http1xTransactionCount();
}

nsresult TLSFilterTransaction::TakeSubTransactions(
    nsTArray<RefPtr<nsAHttpTransaction> >& outTransactions) {
  LOG(("TLSFilterTransaction::TakeSubTransactions [this=%p] mTransaction %p\n",
       this, mTransaction.get()));

  if (!mTransaction) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mTransaction->TakeSubTransactions(outTransactions) ==
      NS_ERROR_NOT_IMPLEMENTED) {
    outTransactions.AppendElement(mTransaction);
  }
  mTransaction = nullptr;

  return NS_OK;
}

nsresult TLSFilterTransaction::SetProxiedTransaction(
    nsAHttpTransaction* aTrans, nsAHttpTransaction* aSpdyConnectTransaction) {
  LOG(
      ("TLSFilterTransaction::SetProxiedTransaction [this=%p] aTrans=%p, "
       "aSpdyConnectTransaction=%p\n",
       this, aTrans, aSpdyConnectTransaction));

  mTransaction = aTrans;

  // Reverting mCloseReason to the default value for consistency to indicate we
  // are no longer in closed state.
  mCloseReason = NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  mTransaction->GetSecurityCallbacks(getter_AddRefs(callbacks));
  nsCOMPtr<nsISSLSocketControl> secCtrl(do_QueryInterface(mSecInfo));
  if (secCtrl && callbacks) {
    secCtrl->SetNotificationCallbacks(callbacks);
  }

  mWeakTrans = do_GetWeakReference(aSpdyConnectTransaction);

  return NS_OK;
}

bool TLSFilterTransaction::IsNullTransaction() {
  if (!mTransaction) {
    return false;
  }
  return mTransaction->IsNullTransaction();
}

NullHttpTransaction* TLSFilterTransaction::QueryNullTransaction() {
  if (!mTransaction) {
    return nullptr;
  }
  return mTransaction->QueryNullTransaction();
}

nsHttpTransaction* TLSFilterTransaction::QueryHttpTransaction() {
  if (!mTransaction) {
    return nullptr;
  }
  return mTransaction->QueryHttpTransaction();
}

class SocketInWrapper : public nsIAsyncInputStream,
                        public nsAHttpSegmentWriter {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_NSIASYNCINPUTSTREAM(mStream->)

  SocketInWrapper(nsIAsyncInputStream* aWrapped, TLSFilterTransaction* aFilter)
      : mStream(aWrapped), mTLSFilter(aFilter) {}

  NS_IMETHOD Close() override {
    mTLSFilter = nullptr;
    return mStream->Close();
  }

  NS_IMETHOD Available(uint64_t* _retval) override {
    return mStream->Available(_retval);
  }

  NS_IMETHOD IsNonBlocking(bool* _retval) override {
    return mStream->IsNonBlocking(_retval);
  }

  NS_IMETHOD ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                          uint32_t aCount, uint32_t* _retval) override {
    return mStream->ReadSegments(aWriter, aClosure, aCount, _retval);
  }

  // finally, ones that don't get forwarded :)
  NS_IMETHOD Read(char* aBuf, uint32_t aCount, uint32_t* _retval) override;
  virtual nsresult OnWriteSegment(char* segment, uint32_t count,
                                  uint32_t* countWritten) override;

 private:
  virtual ~SocketInWrapper() = default;
  ;

  nsCOMPtr<nsIAsyncInputStream> mStream;
  RefPtr<TLSFilterTransaction> mTLSFilter;
};

nsresult SocketInWrapper::OnWriteSegment(char* segment, uint32_t count,
                                         uint32_t* countWritten) {
  LOG(("SocketInWrapper OnWriteSegment %d %p filter=%p\n", count, this,
       mTLSFilter.get()));

  nsresult rv = mStream->Read(segment, count, countWritten);
  LOG(("SocketInWrapper OnWriteSegment %p wrapped read %" PRIx32 " %d\n", this,
       static_cast<uint32_t>(rv), *countWritten));
  return rv;
}

NS_IMETHODIMP
SocketInWrapper::Read(char* aBuf, uint32_t aCount, uint32_t* _retval) {
  LOG(("SocketInWrapper Read %d %p filter=%p\n", aCount, this,
       mTLSFilter.get()));

  if (!mTLSFilter) {
    return NS_ERROR_UNEXPECTED;  // protect potentially dangling mTLSFilter
  }

  // mTLSFilter->mSegmentWriter MUST be this at ctor time
  return mTLSFilter->OnWriteSegment(aBuf, aCount, _retval);
}

class SocketOutWrapper : public nsIAsyncOutputStream,
                         public nsAHttpSegmentReader {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_NSIASYNCOUTPUTSTREAM(mStream->)

  SocketOutWrapper(nsIAsyncOutputStream* aWrapped,
                   TLSFilterTransaction* aFilter)
      : mStream(aWrapped), mTLSFilter(aFilter) {}

  NS_IMETHOD Close() override {
    mTLSFilter = nullptr;
    return mStream->Close();
  }

  NS_IMETHOD Flush() override { return mStream->Flush(); }

  NS_IMETHOD IsNonBlocking(bool* _retval) override {
    return mStream->IsNonBlocking(_retval);
  }

  NS_IMETHOD WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                           uint32_t aCount, uint32_t* _retval) override {
    return mStream->WriteSegments(aReader, aClosure, aCount, _retval);
  }

  NS_IMETHOD WriteFrom(nsIInputStream* aFromStream, uint32_t aCount,
                       uint32_t* _retval) override {
    return mStream->WriteFrom(aFromStream, aCount, _retval);
  }

  // finally, ones that don't get forwarded :)
  NS_IMETHOD Write(const char* aBuf, uint32_t aCount,
                   uint32_t* _retval) override;
  virtual nsresult OnReadSegment(const char* segment, uint32_t count,
                                 uint32_t* countRead) override;

 private:
  virtual ~SocketOutWrapper() = default;
  ;

  nsCOMPtr<nsIAsyncOutputStream> mStream;
  RefPtr<TLSFilterTransaction> mTLSFilter;
};

nsresult SocketOutWrapper::OnReadSegment(const char* segment, uint32_t count,
                                         uint32_t* countWritten) {
  return mStream->Write(segment, count, countWritten);
}

NS_IMETHODIMP
SocketOutWrapper::Write(const char* aBuf, uint32_t aCount, uint32_t* _retval) {
  LOG(("SocketOutWrapper Write %d %p filter=%p\n", aCount, this,
       mTLSFilter.get()));

  // mTLSFilter->mSegmentReader MUST be this at ctor time
  if (!mTLSFilter) {
    return NS_ERROR_UNEXPECTED;  // protect potentially dangling mTLSFilter
  }

  return mTLSFilter->OnReadSegment(aBuf, aCount, _retval);
}

void TLSFilterTransaction::newIODriver(nsIAsyncInputStream* aSocketIn,
                                       nsIAsyncOutputStream* aSocketOut,
                                       nsIAsyncInputStream** outSocketIn,
                                       nsIAsyncOutputStream** outSocketOut) {
  SocketInWrapper* inputWrapper = new SocketInWrapper(aSocketIn, this);
  mSegmentWriter = inputWrapper;
  nsCOMPtr<nsIAsyncInputStream> newIn(inputWrapper);
  newIn.forget(outSocketIn);

  SocketOutWrapper* outputWrapper = new SocketOutWrapper(aSocketOut, this);
  mSegmentReader = outputWrapper;
  nsCOMPtr<nsIAsyncOutputStream> newOut(outputWrapper);
  newOut.forget(outSocketOut);
}

SpdyConnectTransaction* TLSFilterTransaction::QuerySpdyConnectTransaction() {
  if (!mTransaction) {
    return nullptr;
  }
  return mTransaction->QuerySpdyConnectTransaction();
}

class WeakTransProxy final : public nsISupports {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit WeakTransProxy(SpdyConnectTransaction* aTrans) {
    MOZ_ASSERT(OnSocketThread());
    mWeakTrans = do_GetWeakReference(aTrans);
  }

  already_AddRefed<NullHttpTransaction> QueryTransaction() {
    MOZ_ASSERT(OnSocketThread());
    RefPtr<NullHttpTransaction> trans = do_QueryReferent(mWeakTrans);
    return trans.forget();
  }

 private:
  ~WeakTransProxy() { MOZ_ASSERT(OnSocketThread()); }

  nsWeakPtr mWeakTrans;
};

NS_IMPL_ISUPPORTS(WeakTransProxy, nsISupports)

class WeakTransFreeProxy final : public Runnable {
 public:
  explicit WeakTransFreeProxy(WeakTransProxy* proxy)
      : Runnable("WeakTransFreeProxy"), mProxy(proxy) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(OnSocketThread());
    mProxy = nullptr;
    return NS_OK;
  }

  void Dispatch() {
    MOZ_ASSERT(!OnSocketThread());
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService("@mozilla.org/network/socket-transport-service;1");
    Unused << sts->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
  }

 private:
  RefPtr<WeakTransProxy> mProxy;
};

class SocketTransportShim : public nsISocketTransport {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITRANSPORT
  NS_DECL_NSISOCKETTRANSPORT

  explicit SocketTransportShim(SpdyConnectTransaction* aTrans,
                               nsISocketTransport* aWrapped, bool aIsWebsocket)
      : mWrapped(aWrapped), mIsWebsocket(aIsWebsocket) {
    mWeakTrans = new WeakTransProxy(aTrans);
  }

 private:
  virtual ~SocketTransportShim() {
    if (!OnSocketThread()) {
      RefPtr<WeakTransFreeProxy> p = new WeakTransFreeProxy(mWeakTrans);
      mWeakTrans = nullptr;
      p->Dispatch();
    }
  }

  nsCOMPtr<nsISocketTransport> mWrapped;
  bool mIsWebsocket;
  nsCOMPtr<nsIInterfaceRequestor> mSecurityCallbacks;
  RefPtr<WeakTransProxy> mWeakTrans;  // SpdyConnectTransaction *
};

class OutputStreamShim : public nsIAsyncOutputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  NS_DECL_NSIASYNCOUTPUTSTREAM

  friend class SpdyConnectTransaction;
  friend class WebsocketHasDataToWrite;
  friend class OutputCloseTransaction;

  OutputStreamShim(SpdyConnectTransaction* aTrans, bool aIsWebsocket)
      : mCallback(nullptr),
        mStatus(NS_OK),
        mMutex("OutputStreamShim"),
        mIsWebsocket(aIsWebsocket) {
    mWeakTrans = new WeakTransProxy(aTrans);
  }

  already_AddRefed<nsIOutputStreamCallback> TakeCallback();

 private:
  virtual ~OutputStreamShim() {
    if (!OnSocketThread()) {
      RefPtr<WeakTransFreeProxy> p = new WeakTransFreeProxy(mWeakTrans);
      mWeakTrans = nullptr;
      p->Dispatch();
    }
  }

  RefPtr<WeakTransProxy> mWeakTrans;  // SpdyConnectTransaction *
  nsCOMPtr<nsIOutputStreamCallback> mCallback;
  nsresult mStatus;
  mozilla::Mutex mMutex;

  // Websockets
  bool mIsWebsocket;
  nsresult CallTransactionHasDataToWrite();
  nsresult CloseTransaction(nsresult reason);
};

class InputStreamShim : public nsIAsyncInputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  friend class SpdyConnectTransaction;
  friend class InputCloseTransaction;

  InputStreamShim(SpdyConnectTransaction* aTrans, bool aIsWebsocket)
      : mCallback(nullptr),
        mStatus(NS_OK),
        mMutex("InputStreamShim"),
        mIsWebsocket(aIsWebsocket) {
    mWeakTrans = new WeakTransProxy(aTrans);
  }

  already_AddRefed<nsIInputStreamCallback> TakeCallback();
  bool HasCallback();

 private:
  virtual ~InputStreamShim() {
    if (!OnSocketThread()) {
      RefPtr<WeakTransFreeProxy> p = new WeakTransFreeProxy(mWeakTrans);
      mWeakTrans = nullptr;
      p->Dispatch();
    }
  }

  RefPtr<WeakTransProxy> mWeakTrans;  // SpdyConnectTransaction *
  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsresult mStatus;
  mozilla::Mutex mMutex;

  // Websockets
  bool mIsWebsocket;
  nsresult CloseTransaction(nsresult reason);
};

SpdyConnectTransaction::SpdyConnectTransaction(
    nsHttpConnectionInfo* ci, nsIInterfaceRequestor* callbacks, uint32_t caps,
    nsHttpTransaction* trans, nsAHttpConnection* session, bool isWebsocket)
    : NullHttpTransaction(ci, callbacks, caps | NS_HTTP_ALLOW_KEEPALIVE),
      mConnectStringOffset(0),
      mSession(session),
      mSegmentReader(nullptr),
      mInputDataSize(0),
      mInputDataUsed(0),
      mInputDataOffset(0),
      mOutputDataSize(0),
      mOutputDataUsed(0),
      mOutputDataOffset(0),
      mForcePlainText(false),
      mIsWebsocket(isWebsocket),
      mConnRefTaken(false),
      mCreateShimErrorCalled(false) {
  LOG(("SpdyConnectTransaction ctor %p\n", this));

  mTimestampSyn = TimeStamp::Now();
  mRequestHead = new nsHttpRequestHead();
  if (mIsWebsocket) {
    // Ensure our request head has all the websocket headers duplicated from the
    // original transaction before calling the boilerplate stuff to create the
    // rest of the CONNECT headers.
    trans->RequestHead()->Enter();
    mRequestHead->SetHeaders(trans->RequestHead()->Headers());
    trans->RequestHead()->Exit();
  }
  DebugOnly<nsresult> rv = nsHttpConnection::MakeConnectString(
      trans, mRequestHead, mConnectString, mIsWebsocket);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  mDrivingTransaction = trans;
}

SpdyConnectTransaction::~SpdyConnectTransaction() {
  LOG(("SpdyConnectTransaction dtor %p\n", this));

  MOZ_ASSERT(OnSocketThread());

  if (mDrivingTransaction) {
    // requeue it I guess. This should be gone.
    mDrivingTransaction->SetH2WSTransaction(nullptr);
    Unused << gHttpHandler->InitiateTransaction(
        mDrivingTransaction, mDrivingTransaction->Priority());
  }
}

void SpdyConnectTransaction::ForcePlainText() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!mInputDataUsed && !mInputDataSize && !mInputDataOffset);
  MOZ_ASSERT(!mForcePlainText);
  MOZ_ASSERT(!mTunnelTransport, "call before mapstreamtohttpconnection");
  MOZ_ASSERT(!mIsWebsocket);

  mForcePlainText = true;
}

void SpdyConnectTransaction::MapStreamToHttpConnection(
    nsISocketTransport* aTransport, nsHttpConnectionInfo* aConnInfo,
    int32_t httpResponseCode) {
  MOZ_ASSERT(OnSocketThread());

  mConnInfo = aConnInfo;

  mTunnelTransport = new SocketTransportShim(this, aTransport, mIsWebsocket);
  mTunnelStreamIn = new InputStreamShim(this, mIsWebsocket);
  mTunnelStreamOut = new OutputStreamShim(this, mIsWebsocket);
  mTunneledConn = new nsHttpConnection();

  // If httpResponseCode is -1, it means that proxy connect is not used. We
  // should not call HttpProxyResponseToErrorCode(), since this will create a
  // shim error.
  if (httpResponseCode > 0 && httpResponseCode != 200) {
    nsresult err = HttpProxyResponseToErrorCode(httpResponseCode);
    if (NS_FAILED(err)) {
      CreateShimError(err);
    }
  }

  // this new http connection has a specific hashkey (i.e. to a particular
  // host via the tunnel) and is associated with the tunnel streams
  LOG(("SpdyConnectTransaction %p new httpconnection %p %s\n", this,
       mTunneledConn.get(), aConnInfo->HashKey().get()));

  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  GetSecurityCallbacks(getter_AddRefs(callbacks));
  mTunneledConn->SetTransactionCaps(Caps());
  MOZ_ASSERT(aConnInfo->UsingHttpsProxy() || mIsWebsocket);
  TimeDuration rtt = TimeStamp::Now() - mTimestampSyn;
  DebugOnly<nsresult> rv = mTunneledConn->Init(
      aConnInfo, gHttpHandler->ConnMgr()->MaxRequestDelay(), mTunnelTransport,
      mTunnelStreamIn, mTunnelStreamOut, true, callbacks,
      PR_MillisecondsToInterval(static_cast<uint32_t>(rtt.ToMilliseconds())));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (mForcePlainText) {
    mTunneledConn->ForcePlainText();
  } else if (mIsWebsocket) {
    LOG(("SpdyConnectTransaction::MapStreamToHttpConnection %p websocket",
         this));
    // Let the root transaction know about us, so it can pass our own conn
    // to the websocket.
    mDrivingTransaction->SetH2WSTransaction(this);
  } else {
    mTunneledConn->SetupSecondaryTLS(this);
    mTunneledConn->SetInSpdyTunnel(true);
  }

  // make the originating transaction stick to the tunneled conn
  RefPtr<nsAHttpConnection> wrappedConn =
      gHttpHandler->ConnMgr()->MakeConnectionHandle(mTunneledConn);
  mDrivingTransaction->SetConnection(wrappedConn);
  mDrivingTransaction->MakeSticky();
  mDrivingTransaction->OnProxyConnectComplete(httpResponseCode);

  if (!mIsWebsocket) {
    // jump the priority and start the dispatcher
    Unused << gHttpHandler->InitiateTransaction(
        mDrivingTransaction, nsISupportsPriority::PRIORITY_HIGHEST - 60);
    mDrivingTransaction = nullptr;
  }
}

nsresult SpdyConnectTransaction::Flush(uint32_t count, uint32_t* countRead) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("SpdyConnectTransaction::Flush %p count %d avail %d\n", this, count,
       mOutputDataUsed - mOutputDataOffset));

  if (!mSegmentReader) {
    return NS_ERROR_UNEXPECTED;
  }

  *countRead = 0;
  count = std::min(count, (mOutputDataUsed - mOutputDataOffset));
  if (count) {
    nsresult rv;
    rv = mSegmentReader->OnReadSegment(&mOutputData[mOutputDataOffset], count,
                                       countRead);
    if (NS_FAILED(rv) && (rv != NS_BASE_STREAM_WOULD_BLOCK)) {
      LOG(("SpdyConnectTransaction::Flush %p Error %" PRIx32 "\n", this,
           static_cast<uint32_t>(rv)));
      CreateShimError(rv);
      return rv;
    }
  }

  mOutputDataOffset += *countRead;
  if (mOutputDataOffset == mOutputDataUsed) {
    mOutputDataOffset = mOutputDataUsed = 0;
  }
  if (!(*countRead)) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  if (mOutputDataUsed != mOutputDataOffset) {
    LOG(("SpdyConnectTransaction::Flush %p Incomplete %d\n", this,
         mOutputDataUsed - mOutputDataOffset));
    mSession->TransactionHasDataToWrite(this);
  }

  return NS_OK;
}

nsresult SpdyConnectTransaction::ReadSegments(nsAHttpSegmentReader* reader,
                                              uint32_t count,
                                              uint32_t* countRead) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("SpdyConnectTransaction::ReadSegments %p count %d conn %p\n", this,
       count, mTunneledConn.get()));

  mSegmentReader = reader;

  // spdy stream carrying tunnel is not setup yet.
  if (!mTunneledConn) {
    uint32_t toWrite = mConnectString.Length() - mConnectStringOffset;
    toWrite = std::min(toWrite, count);
    *countRead = toWrite;
    if (toWrite) {
      nsresult rv = mSegmentReader->OnReadSegment(
          mConnectString.BeginReading() + mConnectStringOffset, toWrite,
          countRead);
      if (NS_FAILED(rv) && (rv != NS_BASE_STREAM_WOULD_BLOCK)) {
        LOG(
            ("SpdyConnectTransaction::ReadSegments %p OnReadSegmentError "
             "%" PRIx32 "\n",
             this, static_cast<uint32_t>(rv)));
        CreateShimError(rv);
      } else {
        mConnectStringOffset += toWrite;
        if (mConnectString.Length() == mConnectStringOffset) {
          mConnectString.Truncate();
          mConnectStringOffset = 0;
        }
      }
      return rv;
    }

    LOG(("SpdyConnectTransaciton::ReadSegments %p connect request consumed",
         this));
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  if (mForcePlainText) {
    // this path just ignores sending the request so that we can
    // send a synthetic reply in writesegments()
    LOG(
        ("SpdyConnectTransaciton::ReadSegments %p dropping %d output bytes "
         "due to synthetic reply\n",
         this, mOutputDataUsed - mOutputDataOffset));
    *countRead = mOutputDataUsed - mOutputDataOffset;
    mOutputDataOffset = mOutputDataUsed = 0;
    mTunneledConn->DontReuse();
    return NS_OK;
  }

  *countRead = 0;
  nsresult rv = Flush(count, countRead);
  if (!(*countRead)) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  nsCOMPtr<nsIOutputStreamCallback> cb = mTunnelStreamOut->TakeCallback();
  if (!cb) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  // See if there is any more data available
  rv = cb->OnOutputStreamReady(mTunnelStreamOut);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Write out anything that may have come out of the stream just above
  uint32_t subtotal;
  count -= *countRead;
  rv = Flush(count, &subtotal);
  *countRead += subtotal;

  return rv;
}

void SpdyConnectTransaction::CreateShimError(nsresult code) {
  LOG(("SpdyConnectTransaction::CreateShimError %p 0x%08" PRIx32, this,
       static_cast<uint32_t>(code)));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(NS_FAILED(code));

  MOZ_ASSERT(!mCreateShimErrorCalled);
  if (mCreateShimErrorCalled) {
    return;
  }
  mCreateShimErrorCalled = true;

  if (mTunnelStreamOut && NS_SUCCEEDED(mTunnelStreamOut->mStatus)) {
    mTunnelStreamOut->mStatus = code;
  }

  if (mTunnelStreamIn && NS_SUCCEEDED(mTunnelStreamIn->mStatus)) {
    mTunnelStreamIn->mStatus = code;
  }

  if (mTunnelStreamIn) {
    nsCOMPtr<nsIInputStreamCallback> cb = mTunnelStreamIn->TakeCallback();
    if (cb) {
      cb->OnInputStreamReady(mTunnelStreamIn);
    }
  }

  if (mTunnelStreamOut) {
    nsCOMPtr<nsIOutputStreamCallback> cb = mTunnelStreamOut->TakeCallback();
    if (cb) {
      cb->OnOutputStreamReady(mTunnelStreamOut);
    }
  }
  mCreateShimErrorCalled = false;
}

nsresult SpdyConnectTransaction::WriteDataToBuffer(nsAHttpSegmentWriter* writer,
                                                   uint32_t count,
                                                   uint32_t* countWritten) {
  EnsureBuffer(mInputData, mInputDataUsed + count, mInputDataUsed,
               mInputDataSize);
  nsresult rv =
      writer->OnWriteSegment(&mInputData[mInputDataUsed], count, countWritten);
  if (NS_FAILED(rv)) {
    if (rv != NS_BASE_STREAM_WOULD_BLOCK) {
      LOG(
          ("SpdyConnectTransaction::WriteSegments wrapped writer %p Error "
           "%" PRIx32 "\n",
           this, static_cast<uint32_t>(rv)));
      CreateShimError(rv);
    }
    return rv;
  }
  mInputDataUsed += *countWritten;
  LOG(
      ("SpdyConnectTransaction %p %d new bytes [%d total] of ciphered data "
       "buffered\n",
       this, *countWritten, mInputDataUsed - mInputDataOffset));

  return rv;
}

nsresult SpdyConnectTransaction::WriteSegments(nsAHttpSegmentWriter* writer,
                                               uint32_t count,
                                               uint32_t* countWritten) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("SpdyConnectTransaction::WriteSegments %p max=%d", this, count));

  // For websockets, we need to forward the initial response through to the base
  // transaction so the normal websocket plumbing can do all the things it needs
  // to do.
  if (mIsWebsocket) {
    return WebsocketWriteSegments(writer, count, countWritten);
  }

  // first call into the tunnel stream to get the demux'd data out of the
  // spdy session.
  nsresult rv = WriteDataToBuffer(writer, count, countWritten);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIInputStreamCallback> cb =
      mTunneledConn ? mTunnelStreamIn->TakeCallback() : nullptr;
  LOG(("SpdyConnectTransaction::WriteSegments %p cb=%p", this, cb.get()));

  if (!cb) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  rv = cb->OnInputStreamReady(mTunnelStreamIn);
  LOG(
      ("SpdyConnectTransaction::WriteSegments %p "
       "after InputStreamReady callback %d total of ciphered data buffered "
       "rv=%" PRIx32 "\n",
       this, mInputDataUsed - mInputDataOffset, static_cast<uint32_t>(rv)));
  LOG(
      ("SpdyConnectTransaction::WriteSegments %p "
       "goodput %p out %" PRId64 "\n",
       this, mTunneledConn.get(), mTunneledConn->ContentBytesWritten()));
  if (NS_SUCCEEDED(rv) && !mTunneledConn->ContentBytesWritten()) {
    nsCOMPtr<nsIOutputStreamCallback> ocb = mTunnelStreamOut->TakeCallback();
    mTunnelStreamOut->AsyncWait(ocb, 0, 0, nullptr);
  }
  return rv;
}

nsresult SpdyConnectTransaction::WebsocketWriteSegments(
    nsAHttpSegmentWriter* writer, uint32_t count, uint32_t* countWritten) {
  MOZ_ASSERT(OnSocketThread());
  MOZ_ASSERT(mIsWebsocket);
  LOG(("SpdyConnectTransaction::WebsocketWriteSegments %p max=%d", this,
       count));

  if (mDrivingTransaction && !mDrivingTransaction->IsDone()) {
    // Transaction hasn't received end of headers yet, so keep passing data to
    // it until it has. Then we can take over.
    nsresult rv =
        mDrivingTransaction->WriteSegments(writer, count, countWritten);
    if (NS_SUCCEEDED(rv) && mDrivingTransaction->IsDone() && !mConnRefTaken) {
      mDrivingTransaction->Close(NS_OK);
    }
  }

  if (!mConnRefTaken) {
    // Force driving transaction to finish so the websocket channel can get its
    // notifications correctly and start driving.
    MOZ_ASSERT(mDrivingTransaction);
    mDrivingTransaction->Close(NS_OK);
  }

  nsresult rv = WriteDataToBuffer(writer, count, countWritten);
  if (NS_SUCCEEDED(rv)) {
    if (!mTunneledConn) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }
    nsCOMPtr<nsIInputStreamCallback> cb = mTunnelStreamIn->TakeCallback();
    if (!cb) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }
    rv = cb->OnInputStreamReady(mTunnelStreamIn);
  }

  return rv;
}

bool SpdyConnectTransaction::ConnectedReadyForInput() {
  return mTunneledConn && mTunnelStreamIn->HasCallback();
}

nsHttpRequestHead* SpdyConnectTransaction::RequestHead() {
  return mRequestHead;
}

void SpdyConnectTransaction::Close(nsresult code) {
  LOG(("SpdyConnectTransaction close %p %" PRIx32 "\n", this,
       static_cast<uint32_t>(code)));

  MOZ_ASSERT(OnSocketThread());

  if (mIsWebsocket && mDrivingTransaction) {
    mDrivingTransaction->SetH2WSTransaction(nullptr);
    if (!mConnRefTaken) {
      // This indicates that the websocket failed to set up, so just close down
      // the transaction as usual.
      mDrivingTransaction->Close(code);
      mDrivingTransaction = nullptr;
    }
  }
  NullHttpTransaction::Close(code);
  if (NS_FAILED(code) && (code != NS_BASE_STREAM_WOULD_BLOCK)) {
    CreateShimError(code);
  } else {
    CreateShimError(NS_BASE_STREAM_CLOSED);
  }
}

void SpdyConnectTransaction::SetConnRefTaken() {
  MOZ_ASSERT(OnSocketThread());

  mConnRefTaken = true;
  mDrivingTransaction = nullptr;  // Just in case
}

already_AddRefed<nsIOutputStreamCallback> OutputStreamShim::TakeCallback() {
  mozilla::MutexAutoLock lock(mMutex);
  return mCallback.forget();
}

class WebsocketHasDataToWrite final : public Runnable {
 public:
  explicit WebsocketHasDataToWrite(OutputStreamShim* shim)
      : Runnable("WebsocketHasDataToWrite"), mShim(shim) {}

  ~WebsocketHasDataToWrite() = default;

  NS_IMETHOD Run() override { return mShim->CallTransactionHasDataToWrite(); }

  [[nodiscard]] nsresult Dispatch() {
    if (OnSocketThread()) {
      return Run();
    }

    nsCOMPtr<nsIEventTarget> sts =
        do_GetService("@mozilla.org/network/socket-transport-service;1");
    return sts->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
  }

 private:
  RefPtr<OutputStreamShim> mShim;
};

NS_IMETHODIMP
OutputStreamShim::AsyncWait(nsIOutputStreamCallback* callback,
                            unsigned int flags, unsigned int requestedCount,
                            nsIEventTarget* target) {
  if (mIsWebsocket) {
    // With websockets, AsyncWait may be called from the main thread, but the
    // target is on the socket thread. That's all we really care about.
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
    MOZ_ASSERT((!target && !callback) || (target == sts));
    if (target && (target != sts)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");
    bool currentThread;

    if (target && (NS_FAILED(target->IsOnCurrentThread(&currentThread)) ||
                   !currentThread)) {
      return NS_ERROR_FAILURE;
    }
  }

  LOG(("OutputStreamShim::AsyncWait %p callback %p\n", this, callback));

  {
    mozilla::MutexAutoLock lock(mMutex);
    mCallback = callback;
  }

  RefPtr<WebsocketHasDataToWrite> wsdw = new WebsocketHasDataToWrite(this);
  Unused << wsdw->Dispatch();

  return NS_OK;
}

class OutputCloseTransaction final : public Runnable {
 public:
  OutputCloseTransaction(OutputStreamShim* shim, nsresult reason)
      : Runnable("OutputCloseTransaction"), mShim(shim), mReason(reason) {}

  ~OutputCloseTransaction() = default;

  NS_IMETHOD Run() override { return mShim->CloseTransaction(mReason); }

 private:
  RefPtr<OutputStreamShim> mShim;
  nsresult mReason;
};

NS_IMETHODIMP
OutputStreamShim::CloseWithStatus(nsresult reason) {
  if (!OnSocketThread()) {
    RefPtr<OutputCloseTransaction> oct =
        new OutputCloseTransaction(this, reason);
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService("@mozilla.org/network/socket-transport-service;1");
    return sts->Dispatch(oct, nsIEventTarget::DISPATCH_NORMAL);
  }

  return CloseTransaction(reason);
}

nsresult OutputStreamShim::CloseTransaction(nsresult reason) {
  MOZ_ASSERT(OnSocketThread());
  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction* trans = baseTrans->QuerySpdyConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  trans->mSession->CloseTransaction(trans, reason);
  return NS_OK;
}

NS_IMETHODIMP
OutputStreamShim::Close() { return CloseWithStatus(NS_OK); }

NS_IMETHODIMP
OutputStreamShim::Flush() {
  MOZ_ASSERT(OnSocketThread());
  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction* trans = baseTrans->QuerySpdyConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  uint32_t count = trans->mOutputDataUsed - trans->mOutputDataOffset;
  if (!count) {
    return NS_OK;
  }

  uint32_t countRead;
  nsresult rv = trans->Flush(count, &countRead);
  LOG(("OutputStreamShim::Flush %p before %d after %d\n", this, count,
       trans->mOutputDataUsed - trans->mOutputDataOffset));
  return rv;
}

nsresult OutputStreamShim::CallTransactionHasDataToWrite() {
  MOZ_ASSERT(OnSocketThread());
  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction* trans = baseTrans->QuerySpdyConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }
  trans->mSession->TransactionHasDataToWrite(trans);
  return NS_OK;
}

NS_IMETHODIMP
OutputStreamShim::Write(const char* aBuf, uint32_t aCount, uint32_t* _retval) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction* trans = baseTrans->QuerySpdyConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  if ((trans->mOutputDataUsed + aCount) >= 512000) {
    *_retval = 0;
    // time for some flow control;
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  EnsureBuffer(trans->mOutputData, trans->mOutputDataUsed + aCount,
               trans->mOutputDataUsed, trans->mOutputDataSize);
  memcpy(&trans->mOutputData[trans->mOutputDataUsed], aBuf, aCount);
  trans->mOutputDataUsed += aCount;
  *_retval = aCount;
  LOG(("OutputStreamShim::Write %p new %d total %d\n", this, aCount,
       trans->mOutputDataUsed));

  trans->mSession->TransactionHasDataToWrite(trans);

  return NS_OK;
}

NS_IMETHODIMP
OutputStreamShim::WriteFrom(nsIInputStream* aFromStream, uint32_t aCount,
                            uint32_t* _retval) {
  if (mIsWebsocket) {
    LOG3(("WARNING: OutputStreamShim::WriteFrom %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OutputStreamShim::WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                                uint32_t aCount, uint32_t* _retval) {
  if (mIsWebsocket) {
    LOG3(("WARNING: OutputStreamShim::WriteSegments %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OutputStreamShim::IsNonBlocking(bool* _retval) {
  *_retval = true;
  return NS_OK;
}

already_AddRefed<nsIInputStreamCallback> InputStreamShim::TakeCallback() {
  mozilla::MutexAutoLock lock(mMutex);
  return mCallback.forget();
}

bool InputStreamShim::HasCallback() {
  mozilla::MutexAutoLock lock(mMutex);
  return mCallback != nullptr;
}

class CheckAvailData final : public Runnable {
 public:
  explicit CheckAvailData(InputStreamShim* shim)
      : Runnable("CheckAvailData"), mShim(shim) {}

  ~CheckAvailData() = default;

  NS_IMETHOD Run() override {
    uint64_t avail = 0;
    if (NS_SUCCEEDED(mShim->Available(&avail)) && avail) {
      nsCOMPtr<nsIInputStreamCallback> cb = mShim->TakeCallback();
      if (cb) {
        cb->OnInputStreamReady(mShim);
      }
    }
    return NS_OK;
  }

  [[nodiscard]] nsresult Dispatch() {
    // Dispatch the event even if we're on socket thread to avoid closing and
    // destructing Http2Session in case this call is comming from
    // Http2Session::ReadSegments() and the callback closes the transaction in
    // OnInputStreamRead().
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService("@mozilla.org/network/socket-transport-service;1");
    return sts->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
  }

 private:
  RefPtr<InputStreamShim> mShim;
};

NS_IMETHODIMP
InputStreamShim::AsyncWait(nsIInputStreamCallback* callback, unsigned int flags,
                           unsigned int requestedCount,
                           nsIEventTarget* target) {
  if (mIsWebsocket) {
    // With websockets, AsyncWait may be called from the main thread, but the
    // target is on the socket thread. That's all we really care about.
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
    MOZ_ASSERT((!target && !callback) || (target == sts));
    if (target && (target != sts)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");
    bool currentThread;

    if (target && (NS_FAILED(target->IsOnCurrentThread(&currentThread)) ||
                   !currentThread)) {
      return NS_ERROR_FAILURE;
    }
  }

  LOG(("InputStreamShim::AsyncWait %p callback %p\n", this, callback));

  {
    mozilla::MutexAutoLock lock(mMutex);
    mCallback = callback;
  }

  if (callback) {
    RefPtr<CheckAvailData> cad = new CheckAvailData(this);
    Unused << cad->Dispatch();
  }

  return NS_OK;
}

class InputCloseTransaction final : public Runnable {
 public:
  InputCloseTransaction(InputStreamShim* shim, nsresult reason)
      : Runnable("InputCloseTransaction"), mShim(shim), mReason(reason) {}

  ~InputCloseTransaction() = default;

  NS_IMETHOD Run() override { return mShim->CloseTransaction(mReason); }

 private:
  RefPtr<InputStreamShim> mShim;
  nsresult mReason;
};

NS_IMETHODIMP
InputStreamShim::CloseWithStatus(nsresult reason) {
  if (!OnSocketThread()) {
    RefPtr<InputCloseTransaction> ict = new InputCloseTransaction(this, reason);
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService("@mozilla.org/network/socket-transport-service;1");
    return sts->Dispatch(ict, nsIEventTarget::DISPATCH_NORMAL);
  }

  return CloseTransaction(reason);
}

nsresult InputStreamShim::CloseTransaction(nsresult reason) {
  MOZ_ASSERT(OnSocketThread());
  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction* trans = baseTrans->QuerySpdyConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  trans->mSession->CloseTransaction(trans, reason);
  return NS_OK;
}

NS_IMETHODIMP
InputStreamShim::Close() { return CloseWithStatus(NS_OK); }

NS_IMETHODIMP
InputStreamShim::Available(uint64_t* _retval) {
  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction* trans = baseTrans->QuerySpdyConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  *_retval = trans->mInputDataUsed - trans->mInputDataOffset;
  return NS_OK;
}

NS_IMETHODIMP
InputStreamShim::Read(char* aBuf, uint32_t aCount, uint32_t* _retval) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction* trans = baseTrans->QuerySpdyConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  uint32_t avail = trans->mInputDataUsed - trans->mInputDataOffset;
  uint32_t tocopy = std::min(aCount, avail);
  *_retval = tocopy;
  memcpy(aBuf, &trans->mInputData[trans->mInputDataOffset], tocopy);
  trans->mInputDataOffset += tocopy;
  if (trans->mInputDataOffset == trans->mInputDataUsed) {
    trans->mInputDataOffset = trans->mInputDataUsed = 0;
  }

  return tocopy ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK;
}

NS_IMETHODIMP
InputStreamShim::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                              uint32_t aCount, uint32_t* _retval) {
  if (mIsWebsocket) {
    LOG3(("WARNING: InputStreamShim::ReadSegments %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InputStreamShim::IsNonBlocking(bool* _retval) {
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
SocketTransportShim::SetKeepaliveEnabled(bool aKeepaliveEnabled) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::SetKeepaliveEnabled %p called", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::SetKeepaliveVals(int32_t keepaliveIdleTime,
                                      int32_t keepaliveRetryInterval) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::SetKeepaliveVals %p called", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::GetSecurityCallbacks(
    nsIInterfaceRequestor** aSecurityCallbacks) {
  if (mIsWebsocket) {
    nsCOMPtr<nsIInterfaceRequestor> out(mSecurityCallbacks);
    *aSecurityCallbacks = out.forget().take();
    return NS_OK;
  }

  return mWrapped->GetSecurityCallbacks(aSecurityCallbacks);
}

NS_IMETHODIMP
SocketTransportShim::SetSecurityCallbacks(
    nsIInterfaceRequestor* aSecurityCallbacks) {
  if (mIsWebsocket) {
    mSecurityCallbacks = aSecurityCallbacks;
    return NS_OK;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::OpenInputStream(uint32_t aFlags, uint32_t aSegmentSize,
                                     uint32_t aSegmentCount,
                                     nsIInputStream** _retval) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::OpenInputStream %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::OpenOutputStream(uint32_t aFlags, uint32_t aSegmentSize,
                                      uint32_t aSegmentCount,
                                      nsIOutputStream** _retval) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::OpenOutputStream %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::Close(nsresult aReason) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::Close %p", this));
  } else {
    LOG(("SocketTransportShim::Close %p", this));
  }

  if (gHttpHandler->Bug1563538()) {
    // Must always post, because mSession->CloseTransaction releases the
    // Http2Stream which is still on stack.
    RefPtr<SocketTransportShim> self(this);

    nsCOMPtr<nsIEventTarget> sts =
        do_GetService("@mozilla.org/network/socket-transport-service;1");
    Unused << sts->Dispatch(NS_NewRunnableFunction(
        "SocketTransportShim::Close", [self = std::move(self), aReason]() {
          RefPtr<NullHttpTransaction> baseTrans =
              self->mWeakTrans->QueryTransaction();
          if (!baseTrans) {
            return;
          }
          SpdyConnectTransaction* trans =
              baseTrans->QuerySpdyConnectTransaction();
          MOZ_ASSERT(trans);
          if (!trans) {
            return;
          }

          trans->mSession->CloseTransaction(trans, aReason);
        }));
    return NS_OK;
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::SetEventSink(nsITransportEventSink* aSink,
                                  nsIEventTarget* aEventTarget) {
  if (mIsWebsocket) {
    // Need to pretend, since websockets expect this to work
    return NS_OK;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::Bind(NetAddr* aLocalAddr) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::Bind %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::GetFirstRetryError(nsresult* aFirstRetryError) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::GetFirstRetryError %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::GetEsniUsed(bool* aEsniUsed) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::GetEsniUsed %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::ResolvedByTRR(bool* aResolvedByTRR) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::IsTRR %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

#define FWD_TS_PTR(fx, ts) \
  NS_IMETHODIMP            \
  SocketTransportShim::fx(ts* arg) { return mWrapped->fx(arg); }

#define FWD_TS_ADDREF(fx, ts) \
  NS_IMETHODIMP               \
  SocketTransportShim::fx(ts** arg) { return mWrapped->fx(arg); }

#define FWD_TS(fx, ts) \
  NS_IMETHODIMP        \
  SocketTransportShim::fx(ts arg) { return mWrapped->fx(arg); }

FWD_TS_PTR(GetKeepaliveEnabled, bool);
FWD_TS_PTR(GetSendBufferSize, uint32_t);
FWD_TS(SetSendBufferSize, uint32_t);
FWD_TS_PTR(GetPort, int32_t);
FWD_TS_PTR(GetPeerAddr, mozilla::net::NetAddr);
FWD_TS_PTR(GetSelfAddr, mozilla::net::NetAddr);
FWD_TS_ADDREF(GetScriptablePeerAddr, nsINetAddr);
FWD_TS_ADDREF(GetScriptableSelfAddr, nsINetAddr);
FWD_TS_ADDREF(GetSecurityInfo, nsISupports);
FWD_TS_PTR(IsAlive, bool);
FWD_TS_PTR(GetConnectionFlags, uint32_t);
FWD_TS(SetConnectionFlags, uint32_t);
FWD_TS_PTR(GetTlsFlags, uint32_t);
FWD_TS(SetTlsFlags, uint32_t);
FWD_TS_PTR(GetRecvBufferSize, uint32_t);
FWD_TS(SetRecvBufferSize, uint32_t);
FWD_TS_PTR(GetResetIPFamilyPreference, bool);

nsresult SocketTransportShim::GetOriginAttributes(
    mozilla::OriginAttributes* aOriginAttributes) {
  return mWrapped->GetOriginAttributes(aOriginAttributes);
}

nsresult SocketTransportShim::SetOriginAttributes(
    const mozilla::OriginAttributes& aOriginAttributes) {
  return mWrapped->SetOriginAttributes(aOriginAttributes);
}

NS_IMETHODIMP
SocketTransportShim::GetScriptableOriginAttributes(
    JSContext* aCx, JS::MutableHandle<JS::Value> aOriginAttributes) {
  return mWrapped->GetScriptableOriginAttributes(aCx, aOriginAttributes);
}

NS_IMETHODIMP
SocketTransportShim::SetScriptableOriginAttributes(
    JSContext* aCx, JS::Handle<JS::Value> aOriginAttributes) {
  return mWrapped->SetScriptableOriginAttributes(aCx, aOriginAttributes);
}

NS_IMETHODIMP
SocketTransportShim::GetHost(nsACString& aHost) {
  return mWrapped->GetHost(aHost);
}

NS_IMETHODIMP
SocketTransportShim::GetTimeout(uint32_t aType, uint32_t* _retval) {
  return mWrapped->GetTimeout(aType, _retval);
}

NS_IMETHODIMP
SocketTransportShim::SetTimeout(uint32_t aType, uint32_t aValue) {
  return mWrapped->SetTimeout(aType, aValue);
}

NS_IMETHODIMP
SocketTransportShim::SetReuseAddrPort(bool aReuseAddrPort) {
  return mWrapped->SetReuseAddrPort(aReuseAddrPort);
}

NS_IMETHODIMP
SocketTransportShim::SetLinger(bool aPolarity, int16_t aTimeout) {
  return mWrapped->SetLinger(aPolarity, aTimeout);
}

NS_IMETHODIMP
SocketTransportShim::GetQoSBits(uint8_t* aQoSBits) {
  return mWrapped->GetQoSBits(aQoSBits);
}

NS_IMETHODIMP
SocketTransportShim::SetQoSBits(uint8_t aQoSBits) {
  return mWrapped->SetQoSBits(aQoSBits);
}

NS_IMETHODIMP
SocketTransportShim::SetFastOpenCallback(TCPFastOpen* aFastOpen) {
  return mWrapped->SetFastOpenCallback(aFastOpen);
}

NS_IMPL_ISUPPORTS(TLSFilterTransaction, nsITimerCallback, nsINamed)
NS_IMPL_ISUPPORTS(SocketTransportShim, nsISocketTransport, nsITransport)
NS_IMPL_ISUPPORTS(InputStreamShim, nsIInputStream, nsIAsyncInputStream)
NS_IMPL_ISUPPORTS(OutputStreamShim, nsIOutputStream, nsIAsyncOutputStream)
NS_IMPL_ISUPPORTS(SocketInWrapper, nsIAsyncInputStream)
NS_IMPL_ISUPPORTS(SocketOutWrapper, nsIAsyncOutputStream)

}  // namespace net
}  // namespace mozilla
