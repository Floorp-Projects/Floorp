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
#include "nsISocketProvider.h"
#include "nsISocketProviderService.h"
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

namespace mozilla {
namespace net {

static PRDescIdentity sLayerIdentity;
static PRIOMethods sLayerMethods;
static PRIOMethods *sLayerMethodsPtr = nullptr;

TLSFilterTransaction::TLSFilterTransaction(nsAHttpTransaction *aWrapped,
                                           const char *aTLSHost,
                                           int32_t aTLSPort,
                                           nsAHttpSegmentReader *aReader,
                                           nsAHttpSegmentWriter *aWriter)
  : mTransaction(aWrapped)
  , mEncryptedTextUsed(0)
  , mEncryptedTextSize(0)
  , mSegmentReader(aReader)
  , mSegmentWriter(aWriter)
  , mForce(false)
  , mNudgeCounter(0)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG(("TLSFilterTransaction ctor %p\n", this));

  nsCOMPtr<nsISocketProvider> provider;
  nsCOMPtr<nsISocketProviderService> spserv =
    do_GetService(NS_SOCKETPROVIDERSERVICE_CONTRACTID);

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

  if (provider && mFD) {
    mFD->secret = reinterpret_cast<PRFilePrivate *>(this);
    provider->AddToSocket(PR_AF_INET, aTLSHost, aTLSPort, nullptr,
                          NeckoOriginAttributes(), 0, mFD,
                          getter_AddRefs(mSecInfo));
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

TLSFilterTransaction::~TLSFilterTransaction()
{
  LOG(("TLSFilterTransaction dtor %p\n", this));
  Cleanup();
}

void
TLSFilterTransaction::Cleanup()
{
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

void
TLSFilterTransaction::Close(nsresult aReason)
{
  if (!mTransaction) {
    return;
  }

  mTransaction->Close(aReason);
  mTransaction = nullptr;
}

nsresult
TLSFilterTransaction::OnReadSegment(const char *aData,
                                    uint32_t aCount,
                                    uint32_t *outCountRead)
{
  LOG(("TLSFilterTransaction %p OnReadSegment %d (buffered %d)\n",
       this, aCount, mEncryptedTextUsed));

  mReadSegmentBlocked = false;
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
    rv = mSegmentReader->OnReadSegment(mEncryptedText.get(), mEncryptedTextUsed, &amt);
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

  EnsureBuffer(mEncryptedText, aCount + 4096,
               0, mEncryptedTextSize);

  while (aCount > 0) {
    int32_t written = PR_Write(mFD, aData, aCount);
    LOG(("TLSFilterTransaction %p OnReadSegment PRWrite(%d) = %d %d\n",
         this, aCount, written,
         PR_GetError() == PR_WOULD_BLOCK_ERROR));

    if (written < 1) {
      if (*outCountRead) {
        return NS_OK;
      }
      // mTransaction ReadSegments actually obscures this code, so
      // keep it in a member var for this::ReadSegments to insepct. Similar
      // to nsHttpConnection::mSocketOutCondition
      mReadSegmentBlocked = (PR_GetError() == PR_WOULD_BLOCK_ERROR);
      return mReadSegmentBlocked ? NS_BASE_STREAM_WOULD_BLOCK : NS_ERROR_FAILURE;
    }
    aCount -= written;
    aData += written;
    *outCountRead += written;
    mNudgeCounter = 0;
  }

  LOG(("TLSFilterTransaction %p OnReadSegment2 (buffered %d)\n",
       this, mEncryptedTextUsed));

  uint32_t amt = 0;
  if (mEncryptedTextUsed) {
    // If we are tunneled on spdy CommitToSegmentSize will prevent partial
    // writes that could interfere with multiplexing. H1 is fine with
    // partial writes.
    rv = mSegmentReader->CommitToSegmentSize(mEncryptedTextUsed, mForce);
    if (rv != NS_BASE_STREAM_WOULD_BLOCK) {
      rv = mSegmentReader->OnReadSegment(mEncryptedText.get(), mEncryptedTextUsed, &amt);
    }

    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      // return OK because all the data was consumed and stored in this buffer
      Connection()->TransactionHasDataToWrite(this);
      return NS_OK;
    } else if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (amt == mEncryptedTextUsed) {
    mEncryptedText = nullptr;
    mEncryptedTextUsed = 0;
    mEncryptedTextSize = 0;
  } else {
    memmove(mEncryptedText.get(), &mEncryptedText[amt], mEncryptedTextUsed - amt);
    mEncryptedTextUsed -= amt;
  }
  return NS_OK;
}

int32_t
TLSFilterTransaction::FilterOutput(const char *aBuf, int32_t aAmount)
{
  EnsureBuffer(mEncryptedText, mEncryptedTextUsed + aAmount,
               mEncryptedTextUsed, mEncryptedTextSize);
  memcpy(&mEncryptedText[mEncryptedTextUsed], aBuf, aAmount);
  mEncryptedTextUsed += aAmount;
  return aAmount;
}

nsresult
TLSFilterTransaction::CommitToSegmentSize(uint32_t size, bool forceCommitment)
{
  if (!mSegmentReader) {
      return NS_ERROR_FAILURE;
  }

  // pad the commit by a little bit to leave room for encryption overhead
  // this isn't foolproof and we may still have to buffer, but its a good start
  mForce = forceCommitment;
  return mSegmentReader->CommitToSegmentSize(size + 1024, forceCommitment);
}

nsresult
TLSFilterTransaction::OnWriteSegment(char *aData,
                                     uint32_t aCount,
                                     uint32_t *outCountRead)
{

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mSegmentWriter);
  LOG(("TLSFilterTransaction::OnWriteSegment %p max=%d\n", this, aCount));
  if (!mSecInfo) {
    return NS_ERROR_FAILURE;
  }

  // this will call through to FilterInput to get data from the higher
  // level connection before removing the local TLS layer
  mFilterReadCode = NS_OK;
  int32_t bytesRead = PR_Read(mFD, aData, aCount);
  if (bytesRead == -1) {
    if (PR_GetError() == PR_WOULD_BLOCK_ERROR) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }
    return NS_ERROR_FAILURE;
  }
  *outCountRead = bytesRead;

  if (NS_SUCCEEDED(mFilterReadCode) && !bytesRead) {
    LOG(("TLSFilterTransaction::OnWriteSegment %p "
         "Second layer of TLS stripping results in STREAM_CLOSED\n", this));
    mFilterReadCode = NS_BASE_STREAM_CLOSED;
  }

  LOG(("TLSFilterTransaction::OnWriteSegment %p rv=%x didread=%d "
        "2 layers of ssl stripped to plaintext\n", this, mFilterReadCode, bytesRead));
  return mFilterReadCode;
}

int32_t
TLSFilterTransaction::FilterInput(char *aBuf, int32_t aAmount)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mSegmentWriter);
  LOG(("TLSFilterTransaction::FilterInput max=%d\n", aAmount));

  uint32_t outCountRead = 0;
  mFilterReadCode = mSegmentWriter->OnWriteSegment(aBuf, aAmount, &outCountRead);
  if (NS_SUCCEEDED(mFilterReadCode) && outCountRead) {
    LOG(("TLSFilterTransaction::FilterInput rv=%x read=%d input from net "
         "1 layer stripped, 1 still on\n", mFilterReadCode, outCountRead));
    if (mReadSegmentBlocked) {
      mNudgeCounter = 0;
    }
  }
  if (mFilterReadCode == NS_BASE_STREAM_WOULD_BLOCK) {
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  }
  return outCountRead;
}

nsresult
TLSFilterTransaction::ReadSegments(nsAHttpSegmentReader *aReader,
                                   uint32_t aCount, uint32_t *outCountRead)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG(("TLSFilterTransaction::ReadSegments %p max=%d\n", this, aCount));

  if (!mTransaction) {
    return NS_ERROR_UNEXPECTED;
  }

  mReadSegmentBlocked = false;
  mSegmentReader = aReader;
  nsresult rv = mTransaction->ReadSegments(this, aCount, outCountRead);
  LOG(("TLSFilterTransaction %p called trans->ReadSegments rv=%x %d\n",
       this, rv, *outCountRead));
  if (NS_SUCCEEDED(rv) && mReadSegmentBlocked) {
    rv = NS_BASE_STREAM_WOULD_BLOCK;
    LOG(("TLSFilterTransaction %p read segment blocked found rv=%x\n",
         this, rv));
    Connection()->ForceSend();
  }

  return rv;
}

nsresult
TLSFilterTransaction::WriteSegments(nsAHttpSegmentWriter *aWriter,
                                    uint32_t aCount, uint32_t *outCountWritten)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG(("TLSFilterTransaction::WriteSegments %p max=%d\n", this, aCount));

  if (!mTransaction) {
    return NS_ERROR_UNEXPECTED;
  }

  mSegmentWriter = aWriter;
  nsresult rv = mTransaction->WriteSegments(this, aCount, outCountWritten);
  if (NS_SUCCEEDED(rv) && NS_FAILED(mFilterReadCode) && !(*outCountWritten)) {
    // nsPipe turns failures into silent OK.. undo that!
    rv = mFilterReadCode;
    if (Connection() && (mFilterReadCode == NS_BASE_STREAM_WOULD_BLOCK)) {
      Connection()->ResumeRecv();
    }
  }
  LOG(("TLSFilterTransaction %p called trans->WriteSegments rv=%x %d\n",
       this, rv, *outCountWritten));
  return rv;
}

nsresult
TLSFilterTransaction::GetTransactionSecurityInfo(nsISupports **outSecInfo)
{
  if (!mSecInfo) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISupports> temp(mSecInfo);
  temp.forget(outSecInfo);
  return NS_OK;
}

nsresult
TLSFilterTransaction::NudgeTunnel(NudgeTunnelCallback *aCallback)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG(("TLSFilterTransaction %p NudgeTunnel\n", this));
  mNudgeCallback = nullptr;

  if (!mSecInfo) {
    return NS_ERROR_FAILURE;
  }

  uint32_t notUsed;
  int32_t written = PR_Write(mFD, "", 0);
  if ((written < 0) && (PR_GetError() != PR_WOULD_BLOCK_ERROR)) {
    // fatal handshake failure
    LOG(("TLSFilterTransaction %p Fatal Handshake Failure: %d\n", this, PR_GetError()));
    return NS_ERROR_FAILURE;
  }

  OnReadSegment("", 0, &notUsed);

  // The SSL Layer does some unusual things with PR_Poll that makes it a bad
  // match for multiplexed SSL sessions. We work around this by manually polling for
  // the moment during the brief handshake phase or otherwise blocked on write.
  // Thankfully this is a pretty unusual state. NSPR doesn't help us here -
  // asserting when polling without the NSPR IO layer on the bottom of
  // the stack. As a follow-on we can do some NSPR and maybe libssl changes
  // to make this more event driven, but this is acceptable for getting started.

  uint32_t counter = mNudgeCounter++;
  uint32_t delay;

  if (!counter) {
    delay = 0;
  } else if (counter < 8) { // up to 48ms at 6
    delay = 6;
  } else if (counter < 34) { // up to 499 ms at 17ms
    delay = 17;
  } else { // after that at 51ms (3 old windows ticks)
    delay = 51;
  }

  if(!mTimer) {
    mTimer = do_CreateInstance("@mozilla.org/timer;1");
  }

  mNudgeCallback = aCallback;
  if (!mTimer ||
      NS_FAILED(mTimer->InitWithCallback(this, delay, nsITimer::TYPE_ONE_SHOT))) {
    return StartTimerCallback();
  }

  LOG(("TLSFilterTransaction %p NudgeTunnel timer started\n", this));
  return NS_OK;
}

NS_IMETHODIMP
TLSFilterTransaction::Notify(nsITimer *timer)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG(("TLSFilterTransaction %p NudgeTunnel notify\n", this));

  if (timer != mTimer) {
    return NS_ERROR_UNEXPECTED;
  }
  StartTimerCallback();
  return NS_OK;
}

nsresult
TLSFilterTransaction::StartTimerCallback()
{
  LOG(("TLSFilterTransaction %p NudgeTunnel StartTimerCallback %p\n",
       this, mNudgeCallback.get()));

  if (mNudgeCallback) {
    // This class can be called re-entrantly, so cleanup m* before ->on()
    RefPtr<NudgeTunnelCallback> cb(mNudgeCallback);
    mNudgeCallback = nullptr;
    cb->OnTunnelNudged(this);
  }
  return NS_OK;
}

PRStatus
TLSFilterTransaction::GetPeerName(PRFileDesc *aFD, PRNetAddr*addr)
{
  NetAddr peeraddr;
  TLSFilterTransaction *self = reinterpret_cast<TLSFilterTransaction *>(aFD->secret);

  if (!self->mTransaction ||
      NS_FAILED(self->mTransaction->Connection()->Transport()->GetPeerAddr(&peeraddr))) {
    return PR_FAILURE;
  }
  NetAddrToPRNetAddr(&peeraddr, addr);
  return PR_SUCCESS;
}

PRStatus
TLSFilterTransaction::GetSocketOption(PRFileDesc *aFD, PRSocketOptionData *aOpt)
{
  if (aOpt->option == PR_SockOpt_Nonblocking) {
    aOpt->value.non_blocking = PR_TRUE;
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

PRStatus
TLSFilterTransaction::SetSocketOption(PRFileDesc *aFD, const PRSocketOptionData *aOpt)
{
  return PR_FAILURE;
}

PRStatus
TLSFilterTransaction::FilterClose(PRFileDesc *aFD)
{
  return PR_SUCCESS;
}

int32_t
TLSFilterTransaction::FilterWrite(PRFileDesc *aFD, const void *aBuf, int32_t aAmount)
{
  TLSFilterTransaction *self = reinterpret_cast<TLSFilterTransaction *>(aFD->secret);
  return self->FilterOutput(static_cast<const char *>(aBuf), aAmount);
}

int32_t
TLSFilterTransaction::FilterSend(PRFileDesc *aFD, const void *aBuf, int32_t aAmount,
                                  int , PRIntervalTime)
{
  return FilterWrite(aFD, aBuf, aAmount);
}

int32_t
TLSFilterTransaction::FilterRead(PRFileDesc *aFD, void *aBuf, int32_t aAmount)
{
  TLSFilterTransaction *self = reinterpret_cast<TLSFilterTransaction *>(aFD->secret);
  return self->FilterInput(static_cast<char *>(aBuf), aAmount);
}

int32_t
TLSFilterTransaction::FilterRecv(PRFileDesc *aFD, void *aBuf, int32_t aAmount,
                                  int , PRIntervalTime)
{
  return FilterRead(aFD, aBuf, aAmount);
}

/////
// The other methods of TLSFilterTransaction just call mTransaction->method
/////

void
TLSFilterTransaction::SetConnection(nsAHttpConnection *aConnection)
{
  if (!mTransaction) {
    return;
  }

  mTransaction->SetConnection(aConnection);
}

nsAHttpConnection *
TLSFilterTransaction::Connection()
{
  if (!mTransaction) {
    return nullptr;
  }
  return mTransaction->Connection();
}

void
TLSFilterTransaction::GetSecurityCallbacks(nsIInterfaceRequestor **outCB)
{
  if (!mTransaction) {
    return;
  }
  mTransaction->GetSecurityCallbacks(outCB);
}

void
TLSFilterTransaction::OnTransportStatus(nsITransport* aTransport,
                                        nsresult aStatus, int64_t aProgress)
{
  if (!mTransaction) {
    return;
  }
  mTransaction->OnTransportStatus(aTransport, aStatus, aProgress);
}

nsHttpConnectionInfo *
TLSFilterTransaction::ConnectionInfo()
{
  if (!mTransaction) {
    return nullptr;
  }
  return mTransaction->ConnectionInfo();
}

bool
TLSFilterTransaction::IsDone()
{
  if (!mTransaction) {
    return true;
  }
  return mTransaction->IsDone();
}

nsresult
TLSFilterTransaction::Status()
{
  if (!mTransaction) {
    return NS_ERROR_UNEXPECTED;
  }

  return mTransaction->Status();
}

uint32_t
TLSFilterTransaction::Caps()
{
  if (!mTransaction) {
    return 0;
  }

  return mTransaction->Caps();
}

void
TLSFilterTransaction::SetDNSWasRefreshed()
{
  if (!mTransaction) {
    return;
  }

  mTransaction->SetDNSWasRefreshed();
}

uint64_t
TLSFilterTransaction::Available()
{
  if (!mTransaction) {
    return 0;
  }

  return mTransaction->Available();
}

void
TLSFilterTransaction::SetProxyConnectFailed()
{
  if (!mTransaction) {
    return;
  }

  mTransaction->SetProxyConnectFailed();
}

nsHttpRequestHead *
TLSFilterTransaction::RequestHead()
{
  if (!mTransaction) {
    return nullptr;
  }

  return mTransaction->RequestHead();
}

uint32_t
TLSFilterTransaction::Http1xTransactionCount()
{
  if (!mTransaction) {
    return 0;
  }

  return mTransaction->Http1xTransactionCount();
}

nsresult
TLSFilterTransaction::TakeSubTransactions(
  nsTArray<RefPtr<nsAHttpTransaction> > &outTransactions)
{
  LOG(("TLSFilterTransaction::TakeSubTransactions [this=%p] mTransaction %p\n",
       this, mTransaction.get()));

  if (!mTransaction) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mTransaction->TakeSubTransactions(outTransactions) == NS_ERROR_NOT_IMPLEMENTED) {
    outTransactions.AppendElement(mTransaction);
  }
  mTransaction = nullptr;

  return NS_OK;
}

nsresult
TLSFilterTransaction::SetProxiedTransaction(nsAHttpTransaction *aTrans)
{
  LOG(("TLSFilterTransaction::SetProxiedTransaction [this=%p] aTrans=%p\n",
       this, aTrans));

  mTransaction = aTrans;
  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  mTransaction->GetSecurityCallbacks(getter_AddRefs(callbacks));
  nsCOMPtr<nsISSLSocketControl> secCtrl(do_QueryInterface(mSecInfo));
  if (secCtrl && callbacks) {
    secCtrl->SetNotificationCallbacks(callbacks);
  }

  return NS_OK;
}

// AddTransaction is for adding pipelined subtransactions
nsresult
TLSFilterTransaction::AddTransaction(nsAHttpTransaction *aTrans)
{
  LOG(("TLSFilterTransaction::AddTransaction passing on subtransaction "
       "[this=%p] aTrans=%p ,mTransaction=%p\n", this, aTrans, mTransaction.get()));

  if (!mTransaction) {
    return NS_ERROR_FAILURE;
  }

  return mTransaction->AddTransaction(aTrans);
}

uint32_t
TLSFilterTransaction::PipelineDepth()
{
  if (!mTransaction) {
    return 0;
  }

  return mTransaction->PipelineDepth();
}

nsresult
TLSFilterTransaction::SetPipelinePosition(int32_t aPosition)
{
  if (!mTransaction) {
    return NS_OK;
  }

  return mTransaction->SetPipelinePosition(aPosition);
}

int32_t
TLSFilterTransaction::PipelinePosition()
{
  if (!mTransaction) {
    return 1;
  }

  return mTransaction->PipelinePosition();
}

nsHttpPipeline *
TLSFilterTransaction::QueryPipeline()
{
  if (!mTransaction) {
    return nullptr;
  }
  return mTransaction->QueryPipeline();
}

bool
TLSFilterTransaction::IsNullTransaction()
{
  if (!mTransaction) {
    return false;
  }
  return mTransaction->IsNullTransaction();
}

NullHttpTransaction *
TLSFilterTransaction::QueryNullTransaction()
{
  if (!mTransaction) {
    return nullptr;
  }
  return mTransaction->QueryNullTransaction();
}

nsHttpTransaction *
TLSFilterTransaction::QueryHttpTransaction()
{
  if (!mTransaction) {
    return nullptr;
  }
  return mTransaction->QueryHttpTransaction();
}


class SocketInWrapper : public nsIAsyncInputStream
                      , public nsAHttpSegmentWriter
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_NSIASYNCINPUTSTREAM(mStream->)

  SocketInWrapper(nsIAsyncInputStream *aWrapped, TLSFilterTransaction *aFilter)
    : mStream(aWrapped)
    , mTLSFilter(aFilter)
  { }

  NS_IMETHOD Close() override
  {
    mTLSFilter = nullptr;
    return mStream->Close();
  }

  NS_IMETHOD Available(uint64_t *_retval) override
  {
    return mStream->Available(_retval);
  }

  NS_IMETHOD IsNonBlocking(bool *_retval) override
  {
    return mStream->IsNonBlocking(_retval);
  }

  NS_IMETHOD ReadSegments(nsWriteSegmentFun aWriter, void *aClosure, uint32_t aCount, uint32_t *_retval) override
  {
    return mStream->ReadSegments(aWriter, aClosure, aCount, _retval);
  }

  // finally, ones that don't get forwarded :)
  NS_IMETHOD Read(char *aBuf, uint32_t aCount, uint32_t *_retval) override;
  virtual nsresult OnWriteSegment(char *segment, uint32_t count, uint32_t *countWritten) override;

private:
  virtual ~SocketInWrapper() {};

  nsCOMPtr<nsIAsyncInputStream> mStream;
  RefPtr<TLSFilterTransaction> mTLSFilter;
};

nsresult
SocketInWrapper::OnWriteSegment(char *segment, uint32_t count, uint32_t *countWritten)
{
  LOG(("SocketInWrapper OnWriteSegment %d %p filter=%p\n", count, this, mTLSFilter.get()));

  nsresult rv = mStream->Read(segment, count, countWritten);
  LOG(("SocketInWrapper OnWriteSegment %p wrapped read %x %d\n",
       this, rv, *countWritten));
  return rv;
}

NS_IMETHODIMP
SocketInWrapper::Read(char *aBuf, uint32_t aCount, uint32_t *_retval)
{
  LOG(("SocketInWrapper Read %d %p filter=%p\n", aCount, this, mTLSFilter.get()));

  if (!mTLSFilter) {
    return NS_ERROR_UNEXPECTED; // protect potentially dangling mTLSFilter
  }

  // mTLSFilter->mSegmentWriter MUST be this at ctor time
  return mTLSFilter->OnWriteSegment(aBuf, aCount, _retval);
}

class SocketOutWrapper : public nsIAsyncOutputStream
                       , public nsAHttpSegmentReader
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_NSIASYNCOUTPUTSTREAM(mStream->)

  SocketOutWrapper(nsIAsyncOutputStream *aWrapped, TLSFilterTransaction *aFilter)
    : mStream(aWrapped)
    , mTLSFilter(aFilter)
  { }

  NS_IMETHOD Close() override
  {
    mTLSFilter = nullptr;
    return mStream->Close();
  }

  NS_IMETHOD Flush() override
  {
    return mStream->Flush();
  }

  NS_IMETHOD IsNonBlocking(bool *_retval) override
  {
    return mStream->IsNonBlocking(_retval);
  }

  NS_IMETHOD WriteSegments(nsReadSegmentFun aReader, void *aClosure, uint32_t aCount, uint32_t *_retval) override
  {
    return mStream->WriteSegments(aReader, aClosure, aCount, _retval);
  }

  NS_IMETHOD WriteFrom(nsIInputStream *aFromStream, uint32_t aCount, uint32_t *_retval) override
  {
    return mStream->WriteFrom(aFromStream, aCount, _retval);
  }

  // finally, ones that don't get forwarded :)
  NS_IMETHOD Write(const char *aBuf, uint32_t aCount, uint32_t *_retval) override;
  virtual nsresult OnReadSegment(const char *segment, uint32_t count, uint32_t *countRead) override;

private:
  virtual ~SocketOutWrapper() {};

  nsCOMPtr<nsIAsyncOutputStream> mStream;
  RefPtr<TLSFilterTransaction> mTLSFilter;
};

nsresult
SocketOutWrapper::OnReadSegment(const char *segment, uint32_t count, uint32_t *countWritten)
{
  return mStream->Write(segment, count, countWritten);
}

NS_IMETHODIMP
SocketOutWrapper::Write(const char *aBuf, uint32_t aCount, uint32_t *_retval)
{
  LOG(("SocketOutWrapper Write %d %p filter=%p\n", aCount, this, mTLSFilter.get()));

  // mTLSFilter->mSegmentReader MUST be this at ctor time
  if (!mTLSFilter) {
    return NS_ERROR_UNEXPECTED; // protect potentially dangling mTLSFilter
  }

  return mTLSFilter->OnReadSegment(aBuf, aCount, _retval);
}

void
TLSFilterTransaction::newIODriver(nsIAsyncInputStream *aSocketIn,
                                  nsIAsyncOutputStream *aSocketOut,
                                  nsIAsyncInputStream **outSocketIn,
                                  nsIAsyncOutputStream **outSocketOut)
{
  SocketInWrapper *inputWrapper = new SocketInWrapper(aSocketIn, this);
  mSegmentWriter = inputWrapper;
  nsCOMPtr<nsIAsyncInputStream> newIn(inputWrapper);
  newIn.forget(outSocketIn);

  SocketOutWrapper *outputWrapper = new SocketOutWrapper(aSocketOut, this);
  mSegmentReader = outputWrapper;
  nsCOMPtr<nsIAsyncOutputStream> newOut(outputWrapper);
  newOut.forget(outSocketOut);
}

SpdyConnectTransaction *
TLSFilterTransaction::QuerySpdyConnectTransaction()
{
  if (!mTransaction) {
    return nullptr;
  }
  return mTransaction->QuerySpdyConnectTransaction();
}

class SocketTransportShim : public nsISocketTransport
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITRANSPORT
  NS_DECL_NSISOCKETTRANSPORT

  explicit SocketTransportShim(nsISocketTransport *aWrapped)
    : mWrapped(aWrapped)
  {};

private:
  virtual ~SocketTransportShim() {};

  nsCOMPtr<nsISocketTransport> mWrapped;
};

class OutputStreamShim : public nsIAsyncOutputStream
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  NS_DECL_NSIASYNCOUTPUTSTREAM

  friend class SpdyConnectTransaction;

  explicit OutputStreamShim(SpdyConnectTransaction *aTrans)
    : mCallback(nullptr)
    , mStatus(NS_OK)
  {
    mWeakTrans = do_GetWeakReference(aTrans);
  }

private:
  virtual ~OutputStreamShim() {};

  nsWeakPtr mWeakTrans; // SpdyConnectTransaction *
  nsIOutputStreamCallback *mCallback;
  nsresult mStatus;
};

class InputStreamShim : public nsIAsyncInputStream
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  friend class SpdyConnectTransaction;

  explicit InputStreamShim(SpdyConnectTransaction *aTrans)
    : mCallback(nullptr)
    , mStatus(NS_OK)
  {
    mWeakTrans = do_GetWeakReference(aTrans);
  }

private:
  virtual ~InputStreamShim() {};

  nsWeakPtr mWeakTrans; // SpdyConnectTransaction *
  nsIInputStreamCallback *mCallback;
  nsresult mStatus;
};

SpdyConnectTransaction::SpdyConnectTransaction(nsHttpConnectionInfo *ci,
                                               nsIInterfaceRequestor *callbacks,
                                               uint32_t caps,
                                               nsHttpTransaction *trans,
                                               nsAHttpConnection *session)
  : NullHttpTransaction(ci, callbacks, caps | NS_HTTP_ALLOW_KEEPALIVE)
  , mConnectStringOffset(0)
  , mSession(session)
  , mSegmentReader(nullptr)
  , mInputDataSize(0)
  , mInputDataUsed(0)
  , mInputDataOffset(0)
  , mOutputDataSize(0)
  , mOutputDataUsed(0)
  , mOutputDataOffset(0)
  , mForcePlainText(false)
{
  LOG(("SpdyConnectTransaction ctor %p\n", this));

  mTimestampSyn = TimeStamp::Now();
  mRequestHead = new nsHttpRequestHead();
  nsHttpConnection::MakeConnectString(trans, mRequestHead, mConnectString);
  mDrivingTransaction = trans;
}

SpdyConnectTransaction::~SpdyConnectTransaction()
{
  LOG(("SpdyConnectTransaction dtor %p\n", this));

  if (mDrivingTransaction) {
    // requeue it I guess. This should be gone.
    gHttpHandler->InitiateTransaction(mDrivingTransaction,
                                      mDrivingTransaction->Priority());
  }
}

void
SpdyConnectTransaction::ForcePlainText()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(!mInputDataUsed && !mInputDataSize && !mInputDataOffset);
  MOZ_ASSERT(!mForcePlainText);
  MOZ_ASSERT(!mTunnelTransport, "call before mapstreamtohttpconnection");

  mForcePlainText = true;
  return;
}

void
SpdyConnectTransaction::MapStreamToHttpConnection(nsISocketTransport *aTransport,
                                                  nsHttpConnectionInfo *aConnInfo)
{
  mConnInfo = aConnInfo;

  mTunnelTransport = new SocketTransportShim(aTransport);
  mTunnelStreamIn = new InputStreamShim(this);
  mTunnelStreamOut = new OutputStreamShim(this);
  mTunneledConn = new nsHttpConnection();

  // this new http connection has a specific hashkey (i.e. to a particular
  // host via the tunnel) and is associated with the tunnel streams
  LOG(("SpdyConnectTransaction new httpconnection %p %s\n",
       mTunneledConn.get(), aConnInfo->HashKey().get()));

  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  GetSecurityCallbacks(getter_AddRefs(callbacks));
  mTunneledConn->SetTransactionCaps(Caps());
  MOZ_ASSERT(aConnInfo->UsingHttpsProxy());
  TimeDuration rtt = TimeStamp::Now() - mTimestampSyn;
  mTunneledConn->Init(aConnInfo,
                      gHttpHandler->ConnMgr()->MaxRequestDelay(),
                      mTunnelTransport, mTunnelStreamIn, mTunnelStreamOut,
                      true, callbacks,
                      PR_MillisecondsToInterval(
                        static_cast<uint32_t>(rtt.ToMilliseconds())));
  if (mForcePlainText) {
      mTunneledConn->ForcePlainText();
  } else {
    mTunneledConn->SetupSecondaryTLS();
    mTunneledConn->SetInSpdyTunnel(true);
  }

  // make the originating transaction stick to the tunneled conn
  RefPtr<nsAHttpConnection> wrappedConn =
    gHttpHandler->ConnMgr()->MakeConnectionHandle(mTunneledConn);
  mDrivingTransaction->SetConnection(wrappedConn);
  mDrivingTransaction->MakeSticky();

  // jump the priority and start the dispatcher
  gHttpHandler->InitiateTransaction(
    mDrivingTransaction, nsISupportsPriority::PRIORITY_HIGHEST - 60);
  mDrivingTransaction = nullptr;
}

nsresult
SpdyConnectTransaction::Flush(uint32_t count, uint32_t *countRead)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG(("SpdyConnectTransaction::Flush %p count %d avail %d\n",
       this, count, mOutputDataUsed - mOutputDataOffset));

  if (!mSegmentReader) {
    return NS_ERROR_UNEXPECTED;
  }

  *countRead = 0;
  count = std::min(count, (mOutputDataUsed - mOutputDataOffset));
  if (count) {
    nsresult rv;
    rv = mSegmentReader->OnReadSegment(&mOutputData[mOutputDataOffset],
                                       count, countRead);
    if (NS_FAILED(rv) && (rv != NS_BASE_STREAM_WOULD_BLOCK)) {
      LOG(("SpdyConnectTransaction::Flush %p Error %x\n", this, rv));
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
    LOG(("SpdyConnectTransaction::Flush %p Incomplete %d\n",
         this, mOutputDataUsed - mOutputDataOffset));
    mSession->TransactionHasDataToWrite(this);
  }

  return NS_OK;
}

nsresult
SpdyConnectTransaction::ReadSegments(nsAHttpSegmentReader *reader,
                                     uint32_t count,
                                     uint32_t *countRead)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG(("SpdyConnectTransaction::ReadSegments %p count %d conn %p\n",
       this, count, mTunneledConn.get()));

  mSegmentReader = reader;

  // spdy stream carrying tunnel is not setup yet.
  if (!mTunneledConn) {
    uint32_t toWrite = mConnectString.Length() - mConnectStringOffset;
    toWrite = std::min(toWrite, count);
    *countRead = toWrite;
    if (toWrite) {
      nsresult rv = mSegmentReader->
        OnReadSegment(mConnectString.BeginReading() + mConnectStringOffset,
                      toWrite, countRead);
      if (NS_FAILED(rv) && (rv != NS_BASE_STREAM_WOULD_BLOCK)) {
        LOG(("SpdyConnectTransaction::ReadSegments %p OnReadSegmentError %x\n",
             this, rv));
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
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  if (mForcePlainText) {
    // this path just ignores sending the request so that we can
    // send a synthetic reply in writesegments()
    LOG(("SpdyConnectTransaciton::ReadSegments %p dropping %d output bytes "
         "due to synthetic reply\n", this, mOutputDataUsed - mOutputDataOffset));
    *countRead = mOutputDataUsed - mOutputDataOffset;
    mOutputDataOffset = mOutputDataUsed = 0;
    mTunneledConn->DontReuse();
    return NS_OK;
  }

  *countRead = 0;
  Flush(count, countRead);
  if (!mTunnelStreamOut->mCallback) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  nsresult rv =
    mTunnelStreamOut->mCallback->OnOutputStreamReady(mTunnelStreamOut);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t subtotal;
  count -= *countRead;
  rv = Flush(count, &subtotal);
  *countRead += subtotal;
  return rv;
}

void
SpdyConnectTransaction::CreateShimError(nsresult code)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(NS_FAILED(code));

  if (mTunnelStreamOut && NS_SUCCEEDED(mTunnelStreamOut->mStatus)) {
    mTunnelStreamOut->mStatus = code;
  }

  if (mTunnelStreamIn && NS_SUCCEEDED(mTunnelStreamIn->mStatus)) {
    mTunnelStreamIn->mStatus = code;
  }

  if (mTunnelStreamIn && mTunnelStreamIn->mCallback) {
    mTunnelStreamIn->mCallback->OnInputStreamReady(mTunnelStreamIn);
  }

  if (mTunnelStreamOut && mTunnelStreamOut->mCallback) {
    mTunnelStreamOut->mCallback->OnOutputStreamReady(mTunnelStreamOut);
  }
}

nsresult
SpdyConnectTransaction::WriteSegments(nsAHttpSegmentWriter *writer,
                                      uint32_t count,
                                      uint32_t *countWritten)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG(("SpdyConnectTransaction::WriteSegments %p max=%d cb=%p\n",
       this, count, mTunneledConn ? mTunnelStreamIn->mCallback : nullptr));

  // first call into the tunnel stream to get the demux'd data out of the
  // spdy session.
  EnsureBuffer(mInputData, mInputDataUsed + count, mInputDataUsed, mInputDataSize);
  nsresult rv = writer->OnWriteSegment(&mInputData[mInputDataUsed],
                                       count, countWritten);
  if (NS_FAILED(rv)) {
    if (rv != NS_BASE_STREAM_WOULD_BLOCK) {
      LOG(("SpdyConnectTransaction::WriteSegments wrapped writer %p Error %x\n", this, rv));
      CreateShimError(rv);
    }
    return rv;
  }
  mInputDataUsed += *countWritten;
  LOG(("SpdyConnectTransaction %p %d new bytes [%d total] of ciphered data buffered\n",
       this, *countWritten, mInputDataUsed - mInputDataOffset));

  if (!mTunneledConn || !mTunnelStreamIn->mCallback) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  rv = mTunnelStreamIn->mCallback->OnInputStreamReady(mTunnelStreamIn);
  LOG(("SpdyConnectTransaction::WriteSegments %p "
       "after InputStreamReady callback %d total of ciphered data buffered rv=%x\n",
       this, mInputDataUsed - mInputDataOffset, rv));
  LOG(("SpdyConnectTransaction::WriteSegments %p "
       "goodput %p out %llu\n", this, mTunneledConn.get(),
       mTunneledConn->ContentBytesWritten()));
  if (NS_SUCCEEDED(rv) && !mTunneledConn->ContentBytesWritten()) {
    mTunnelStreamOut->AsyncWait(mTunnelStreamOut->mCallback, 0, 0, nullptr);
  }
  return rv;
}

bool
SpdyConnectTransaction::ConnectedReadyForInput()
{
  return mTunneledConn && mTunnelStreamIn->mCallback;
}

nsHttpRequestHead *
SpdyConnectTransaction::RequestHead()
{
  return mRequestHead;
}

void
SpdyConnectTransaction::Close(nsresult code)
{
  LOG(("SpdyConnectTransaction close %p %x\n", this, code));

  NullHttpTransaction::Close(code);
  if (NS_FAILED(code) && (code != NS_BASE_STREAM_WOULD_BLOCK)) {
    CreateShimError(code);
  } else {
    CreateShimError(NS_BASE_STREAM_CLOSED);
  }
}

NS_IMETHODIMP
OutputStreamShim::AsyncWait(nsIOutputStreamCallback *callback,
                            unsigned int, unsigned int, nsIEventTarget *target)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  bool currentThread;

  if (target &&
      (NS_FAILED(target->IsOnCurrentThread(&currentThread)) || !currentThread)) {
    return NS_ERROR_FAILURE;
  }

  LOG(("OutputStreamShim::AsyncWait %p callback %p\n", this, callback));
  mCallback = callback;

  RefPtr<NullHttpTransaction> baseTrans(do_QueryReferent(mWeakTrans));
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction *trans = baseTrans->QuerySpdyConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  trans->mSession->TransactionHasDataToWrite(trans);

  return NS_OK;
}

NS_IMETHODIMP
OutputStreamShim::CloseWithStatus(nsresult reason)
{
  RefPtr<NullHttpTransaction> baseTrans(do_QueryReferent(mWeakTrans));
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction *trans = baseTrans->QuerySpdyConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  trans->mSession->CloseTransaction(trans, reason);
  return NS_OK;
}

NS_IMETHODIMP
OutputStreamShim::Close()
{
  return CloseWithStatus(NS_OK);
}

NS_IMETHODIMP
OutputStreamShim::Flush()
{
  RefPtr<NullHttpTransaction> baseTrans(do_QueryReferent(mWeakTrans));
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction *trans = baseTrans->QuerySpdyConnectTransaction();
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
  LOG(("OutputStreamShim::Flush %p before %d after %d\n",
       this, count, trans->mOutputDataUsed - trans->mOutputDataOffset));
  return rv;
}

NS_IMETHODIMP
OutputStreamShim::Write(const char * aBuf, uint32_t aCount, uint32_t *_retval)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  RefPtr<NullHttpTransaction> baseTrans(do_QueryReferent(mWeakTrans));
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction *trans = baseTrans->QuerySpdyConnectTransaction();
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
  LOG(("OutputStreamShim::Write %p new %d total %d\n", this, aCount, trans->mOutputDataUsed));

  trans->mSession->TransactionHasDataToWrite(trans);

  return NS_OK;
}

NS_IMETHODIMP
OutputStreamShim::WriteFrom(nsIInputStream *aFromStream, uint32_t aCount, uint32_t *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OutputStreamShim::WriteSegments(nsReadSegmentFun aReader, void *aClosure, uint32_t aCount, uint32_t *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OutputStreamShim::IsNonBlocking(bool *_retval)
{
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
InputStreamShim::AsyncWait(nsIInputStreamCallback *callback,
                           unsigned int, unsigned int, nsIEventTarget *target)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  bool currentThread;

  if (target &&
      (NS_FAILED(target->IsOnCurrentThread(&currentThread)) || !currentThread)) {
    return NS_ERROR_FAILURE;
  }

  LOG(("InputStreamShim::AsyncWait %p callback %p\n", this, callback));
  mCallback = callback;
  return NS_OK;
}

NS_IMETHODIMP
InputStreamShim::CloseWithStatus(nsresult reason)
{
  RefPtr<NullHttpTransaction> baseTrans(do_QueryReferent(mWeakTrans));
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction *trans = baseTrans->QuerySpdyConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  trans->mSession->CloseTransaction(trans, reason);
  return NS_OK;
}

NS_IMETHODIMP
InputStreamShim::Close()
{
  return CloseWithStatus(NS_OK);
}

NS_IMETHODIMP
InputStreamShim::Available(uint64_t *_retval)
{
  RefPtr<NullHttpTransaction> baseTrans(do_QueryReferent(mWeakTrans));
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction *trans = baseTrans->QuerySpdyConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  *_retval = trans->mInputDataUsed - trans->mInputDataOffset;
  return NS_OK;
}

NS_IMETHODIMP
InputStreamShim::Read(char *aBuf, uint32_t aCount, uint32_t *_retval)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  RefPtr<NullHttpTransaction> baseTrans(do_QueryReferent(mWeakTrans));
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  SpdyConnectTransaction *trans = baseTrans->QuerySpdyConnectTransaction();
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
InputStreamShim::ReadSegments(nsWriteSegmentFun aWriter, void *aClosure,
                              uint32_t aCount, uint32_t *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InputStreamShim::IsNonBlocking(bool *_retval)
{
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
SocketTransportShim::SetKeepaliveEnabled(bool aKeepaliveEnabled)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::SetKeepaliveVals(int32_t keepaliveIdleTime, int32_t keepaliveRetryInterval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::SetSecurityCallbacks(nsIInterfaceRequestor *aSecurityCallbacks)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::OpenInputStream(uint32_t aFlags, uint32_t aSegmentSize,
                                     uint32_t aSegmentCount, nsIInputStream * *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::OpenOutputStream(uint32_t aFlags, uint32_t aSegmentSize,
                                      uint32_t aSegmentCount, nsIOutputStream * *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::Close(nsresult aReason)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::SetEventSink(nsITransportEventSink *aSink, nsIEventTarget *aEventTarget)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::Bind(NetAddr *aLocalAddr)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

#define FWD_TS_PTR(fx, ts) NS_IMETHODIMP \
SocketTransportShim::fx(ts *arg) { return mWrapped->fx(arg); }

#define FWD_TS_ADDREF(fx, ts) NS_IMETHODIMP \
SocketTransportShim::fx(ts **arg) { return mWrapped->fx(arg); }

#define FWD_TS(fx, ts) NS_IMETHODIMP \
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
FWD_TS_ADDREF(GetSecurityCallbacks, nsIInterfaceRequestor);
FWD_TS_PTR(IsAlive, bool);
FWD_TS_PTR(GetConnectionFlags, uint32_t);
FWD_TS(SetConnectionFlags, uint32_t);
FWD_TS_PTR(GetRecvBufferSize, uint32_t);
FWD_TS(SetRecvBufferSize, uint32_t);

nsresult
SocketTransportShim::GetOriginAttributes(mozilla::NeckoOriginAttributes* aOriginAttributes)
{
  return mWrapped->GetOriginAttributes(aOriginAttributes);
}

nsresult
SocketTransportShim::SetOriginAttributes(const mozilla::NeckoOriginAttributes& aOriginAttributes)
{
  return mWrapped->SetOriginAttributes(aOriginAttributes);
}

NS_IMETHODIMP
SocketTransportShim::GetScriptableOriginAttributes(JSContext* aCx,
  JS::MutableHandle<JS::Value> aOriginAttributes)
{
  return mWrapped->GetScriptableOriginAttributes(aCx, aOriginAttributes);
}

NS_IMETHODIMP
SocketTransportShim::SetScriptableOriginAttributes(JSContext* aCx,
  JS::Handle<JS::Value> aOriginAttributes)
{
  return mWrapped->SetScriptableOriginAttributes(aCx, aOriginAttributes);
}

NS_IMETHODIMP
SocketTransportShim::GetHost(nsACString & aHost)
{
  return mWrapped->GetHost(aHost);
}

NS_IMETHODIMP
SocketTransportShim::GetTimeout(uint32_t aType, uint32_t *_retval)
{
  return mWrapped->GetTimeout(aType, _retval);
}

NS_IMETHODIMP
SocketTransportShim::GetNetworkInterfaceId(nsACString_internal &aNetworkInterfaceId)
{
  return mWrapped->GetNetworkInterfaceId(aNetworkInterfaceId);
}

NS_IMETHODIMP
SocketTransportShim::SetNetworkInterfaceId(const nsACString_internal &aNetworkInterfaceId)
{
  return mWrapped->SetNetworkInterfaceId(aNetworkInterfaceId);
}

NS_IMETHODIMP
SocketTransportShim::SetTimeout(uint32_t aType, uint32_t aValue)
{
  return mWrapped->SetTimeout(aType, aValue);
}

NS_IMETHODIMP
SocketTransportShim::SetReuseAddrPort(bool aReuseAddrPort)
{
  return mWrapped->SetReuseAddrPort(aReuseAddrPort);
}

NS_IMETHODIMP
SocketTransportShim::GetQoSBits(uint8_t *aQoSBits)
{
  return mWrapped->GetQoSBits(aQoSBits);
}

NS_IMETHODIMP
SocketTransportShim::SetQoSBits(uint8_t aQoSBits)
{
  return mWrapped->SetQoSBits(aQoSBits);
}

NS_IMPL_ISUPPORTS(TLSFilterTransaction, nsITimerCallback)
NS_IMPL_ISUPPORTS(SocketTransportShim, nsISocketTransport, nsITransport)
NS_IMPL_ISUPPORTS(InputStreamShim, nsIInputStream, nsIAsyncInputStream)
NS_IMPL_ISUPPORTS(OutputStreamShim, nsIOutputStream, nsIAsyncOutputStream)
NS_IMPL_ISUPPORTS(SocketInWrapper, nsIAsyncInputStream)
NS_IMPL_ISUPPORTS(SocketOutWrapper, nsIAsyncOutputStream)

} // namespace net
} // namespace mozilla
