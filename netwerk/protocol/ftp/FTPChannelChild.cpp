/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/ChannelDiverterChild.h"
#include "mozilla/net/FTPChannelChild.h"
#include "mozilla/dom/TabChild.h"
#include "nsFtpProtocolHandler.h"
#include "nsITabChild.h"
#include "nsStringStream.h"
#include "nsNetUtil.h"
#include "base/compiler_specific.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "SerializedLoadContext.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "nsIPrompt.h"

using namespace mozilla::ipc;

#undef LOG
#define LOG(args) MOZ_LOG(gFTPLog, mozilla::LogLevel::Debug, args)

namespace mozilla {
namespace net {

FTPChannelChild::FTPChannelChild(nsIURI* uri)
: mIPCOpen(false)
, mUnknownDecoderInvolved(false)
, mCanceled(false)
, mSuspendCount(0)
, mIsPending(false)
, mLastModifiedTime(0)
, mStartPos(0)
, mDivertingToParent(false)
, mFlushedForDiversion(false)
, mSuspendSent(false)
{
  LOG(("Creating FTPChannelChild @%x\n", this));
  // grab a reference to the handler to ensure that it doesn't go away.
  NS_ADDREF(gFtpHandler);
  SetURI(uri);
  mEventQ = new ChannelEventQueue(static_cast<nsIFTPChannel*>(this));

  // We could support thread retargeting, but as long as we're being driven by
  // IPDL on the main thread it doesn't buy us anything.
  DisallowThreadRetargeting();
}

FTPChannelChild::~FTPChannelChild()
{
  LOG(("Destroying FTPChannelChild @%x\n", this));
  gFtpHandler->Release();
}

void
FTPChannelChild::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen, "Attempt to retain more than one IPDL reference");
  mIPCOpen = true;
  AddRef();
}

void
FTPChannelChild::ReleaseIPDLReference()
{
  MOZ_ASSERT(mIPCOpen, "Attempt to release nonexistent IPDL reference");
  mIPCOpen = false;
  Release();
}

//-----------------------------------------------------------------------------
// FTPChannelChild::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS_INHERITED(FTPChannelChild,
                            nsBaseChannel,
                            nsIFTPChannel,
                            nsIUploadChannel,
                            nsIResumableChannel,
                            nsIProxiedChannel,
                            nsIChildChannel,
                            nsIDivertableChannel)

//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelChild::GetLastModifiedTime(PRTime* lastModifiedTime)
{
  *lastModifiedTime = mLastModifiedTime;
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::SetLastModifiedTime(PRTime lastModifiedTime)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
FTPChannelChild::ResumeAt(uint64_t aStartPos, const nsACString& aEntityID)
{
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  mStartPos = aStartPos;
  mEntityID = aEntityID;
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::GetEntityID(nsACString& entityID)
{
  entityID = mEntityID;
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::GetProxyInfo(nsIProxyInfo** aProxyInfo)
{
  DROP_DEAD();
}

NS_IMETHODIMP
FTPChannelChild::SetUploadStream(nsIInputStream* stream,
                                 const nsACString& contentType,
                                 int64_t contentLength)
{
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  mUploadStream = stream;
  // NOTE: contentLength is intentionally ignored here.
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::GetUploadStream(nsIInputStream** stream)
{
  NS_ENSURE_ARG_POINTER(stream);
  *stream = mUploadStream;
  NS_IF_ADDREF(*stream);
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::AsyncOpen(::nsIStreamListener* listener, nsISupports* aContext)
{
  LOG(("FTPChannelChild::AsyncOpen [this=%p]\n", this));

  NS_ENSURE_TRUE((gNeckoChild), NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  // Port checked in parent, but duplicate here so we can return with error
  // immediately, as we've done since before e10s.
  nsresult rv;
  rv = NS_CheckPortSafety(nsBaseChannel::URI()); // Need to disambiguate,
                                                 // because in the child ipdl,
                                                 // a typedef URI is defined...
  if (NS_FAILED(rv))
    return rv;

  mozilla::dom::TabChild* tabChild = nullptr;
  nsCOMPtr<nsITabChild> iTabChild;
  NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup,
                                NS_GET_IID(nsITabChild),
                                getter_AddRefs(iTabChild));
  GetCallback(iTabChild);
  if (iTabChild) {
    tabChild = static_cast<mozilla::dom::TabChild*>(iTabChild.get());
  }
  if (MissingRequiredTabChild(tabChild, "ftp")) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  mListener = listener;
  mListenerContext = aContext;

  // add ourselves to the load group. 
  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nullptr);

  OptionalInputStreamParams uploadStream;
  nsTArray<mozilla::ipc::FileDescriptor> fds;
  SerializeInputStream(mUploadStream, uploadStream, fds);

  MOZ_ASSERT(fds.IsEmpty());

  FTPChannelOpenArgs openArgs;
  SerializeURI(nsBaseChannel::URI(), openArgs.uri());
  openArgs.startPos() = mStartPos;
  openArgs.entityID() = mEntityID;
  openArgs.uploadStream() = uploadStream;

  nsCOMPtr<nsILoadInfo> loadInfo;
  GetLoadInfo(getter_AddRefs(loadInfo));
  rv = mozilla::ipc::LoadInfoToLoadInfoArgs(loadInfo, &openArgs.loadInfo());
  NS_ENSURE_SUCCESS(rv, rv);

  gNeckoChild->
    SendPFTPChannelConstructor(this, tabChild, IPC::SerializedLoadContext(this),
                               openArgs);

  // The socket transport layer in the chrome process now has a logical ref to
  // us until OnStopRequest is called.
  AddIPDLReference();

  mIsPending = true;
  mWasOpened = true;

  return rv;
}

NS_IMETHODIMP
FTPChannelChild::IsPending(bool* result)
{
  *result = mIsPending;
  return NS_OK;
}

nsresult
FTPChannelChild::OpenContentStream(bool async,
                                   nsIInputStream** stream,
                                   nsIChannel** channel)
{
  NS_RUNTIMEABORT("FTPChannel*Child* should never have OpenContentStream called!");
  return NS_OK;
}
  
//-----------------------------------------------------------------------------
// FTPChannelChild::PFTPChannelChild
//-----------------------------------------------------------------------------

class FTPStartRequestEvent : public ChannelEvent
{
public:
  FTPStartRequestEvent(FTPChannelChild* aChild,
                       const nsresult& aChannelStatus,
                       const int64_t& aContentLength,
                       const nsCString& aContentType,
                       const PRTime& aLastModified,
                       const nsCString& aEntityID,
                       const URIParams& aURI)
    : mChild(aChild)
    , mChannelStatus(aChannelStatus)
    , mContentLength(aContentLength)
    , mContentType(aContentType)
    , mLastModified(aLastModified)
    , mEntityID(aEntityID)
    , mURI(aURI)
  {
  }
  void Run()
  {
    mChild->DoOnStartRequest(mChannelStatus, mContentLength, mContentType,
                             mLastModified, mEntityID, mURI);
  }

private:
  FTPChannelChild* mChild;
  nsresult mChannelStatus;
  int64_t mContentLength;
  nsCString mContentType;
  PRTime mLastModified;
  nsCString mEntityID;
  URIParams mURI;
};

bool
FTPChannelChild::RecvOnStartRequest(const nsresult& aChannelStatus,
                                    const int64_t& aContentLength,
                                    const nsCString& aContentType,
                                    const PRTime& aLastModified,
                                    const nsCString& aEntityID,
                                    const URIParams& aURI)
{
  // mFlushedForDiversion and mDivertingToParent should NEVER be set at this
  // stage, as they are set in the listener's OnStartRequest.
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
    "mFlushedForDiversion should be unset before OnStartRequest!");
  MOZ_RELEASE_ASSERT(!mDivertingToParent,
    "mDivertingToParent should be unset before OnStartRequest!");

  LOG(("FTPChannelChild::RecvOnStartRequest [this=%p]\n", this));

  mEventQ->RunOrEnqueue(new FTPStartRequestEvent(this, aChannelStatus,
                                                 aContentLength, aContentType,
                                                 aLastModified, aEntityID,
                                                 aURI));
  return true;
}

void
FTPChannelChild::DoOnStartRequest(const nsresult& aChannelStatus,
                                  const int64_t& aContentLength,
                                  const nsCString& aContentType,
                                  const PRTime& aLastModified,
                                  const nsCString& aEntityID,
                                  const URIParams& aURI)
{
  LOG(("FTPChannelChild::DoOnStartRequest [this=%p]\n", this));

  // mFlushedForDiversion and mDivertingToParent should NEVER be set at this
  // stage, as they are set in the listener's OnStartRequest.
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
    "mFlushedForDiversion should be unset before OnStartRequest!");
  MOZ_RELEASE_ASSERT(!mDivertingToParent,
    "mDivertingToParent should be unset before OnStartRequest!");

  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = aChannelStatus;
  }

  mContentLength = aContentLength;
  SetContentType(aContentType);
  mLastModifiedTime = aLastModified;
  mEntityID = aEntityID;

  nsCString spec;
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  uri->GetSpec(spec);
  nsBaseChannel::URI()->SetSpec(spec);

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
  nsresult rv = mListener->OnStartRequest(this, mListenerContext);
  if (NS_FAILED(rv))
    Cancel(rv);

  if (mDivertingToParent) {
    mListener = nullptr;
    mListenerContext = nullptr;
    if (mLoadGroup) {
      mLoadGroup->RemoveRequest(this, nullptr, mStatus);
    }
  }
}

class FTPDataAvailableEvent : public ChannelEvent
{
public:
  FTPDataAvailableEvent(FTPChannelChild* aChild,
                        const nsresult& aChannelStatus,
                        const nsCString& aData,
                        const uint64_t& aOffset,
                        const uint32_t& aCount)
    : mChild(aChild)
    , mChannelStatus(aChannelStatus)
    , mData(aData)
    , mOffset(aOffset)
    , mCount(aCount)
  {
  }
  void Run()
  {
    mChild->DoOnDataAvailable(mChannelStatus, mData, mOffset, mCount);
  }

private:
  FTPChannelChild* mChild;
  nsresult mChannelStatus;
  nsCString mData;
  uint64_t mOffset;
  uint32_t mCount;
};

bool
FTPChannelChild::RecvOnDataAvailable(const nsresult& channelStatus,
                                     const nsCString& data,
                                     const uint64_t& offset,
                                     const uint32_t& count)
{
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
                     "Should not be receiving any more callbacks from parent!");

  LOG(("FTPChannelChild::RecvOnDataAvailable [this=%p]\n", this));

  mEventQ->RunOrEnqueue(new FTPDataAvailableEvent(this, channelStatus, data,
                                                  offset, count),
                        mDivertingToParent);

  return true;
}

class MaybeDivertOnDataFTPEvent : public ChannelEvent
{
 public:
  MaybeDivertOnDataFTPEvent(FTPChannelChild* child,
                            const nsCString& data,
                            const uint64_t& offset,
                            const uint32_t& count)
  : mChild(child)
  , mData(data)
  , mOffset(offset)
  , mCount(count) {}

  void Run()
  {
    mChild->MaybeDivertOnData(mData, mOffset, mCount);
  }

 private:
  FTPChannelChild* mChild;
  nsCString mData;
  uint64_t mOffset;
  uint32_t mCount;
};

void
FTPChannelChild::MaybeDivertOnData(const nsCString& data,
                                   const uint64_t& offset,
                                   const uint32_t& count)
{
  if (mDivertingToParent) {
    SendDivertOnDataAvailable(data, offset, count);
  }
}

void
FTPChannelChild::DoOnDataAvailable(const nsresult& channelStatus,
                                   const nsCString& data,
                                   const uint64_t& offset,
                                   const uint32_t& count)
{
  LOG(("FTPChannelChild::DoOnDataAvailable [this=%p]\n", this));

  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = channelStatus;
  }

  if (mDivertingToParent) {
    MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
      "Should not be processing any more callbacks from parent!");

    SendDivertOnDataAvailable(data, offset, count);
    return;
  }

  if (mCanceled)
    return;

  if (mUnknownDecoderInvolved) {
    mUnknownDecoderEventQ.AppendElement(
      MakeUnique<MaybeDivertOnDataFTPEvent>(this, data, offset, count));
  }

  // NOTE: the OnDataAvailable contract requires the client to read all the data
  // in the inputstream.  This code relies on that ('data' will go away after
  // this function).  Apparently the previous, non-e10s behavior was to actually
  // support only reading part of the data, allowing later calls to read the
  // rest.
  nsCOMPtr<nsIInputStream> stringStream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stringStream),
                                      data.get(),
                                      count,
                                      NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
  rv = mListener->OnDataAvailable(this, mListenerContext,
                                  stringStream, offset, count);
  if (NS_FAILED(rv))
    Cancel(rv);
  stringStream->Close();
}

class FTPStopRequestEvent : public ChannelEvent
{
public:
  FTPStopRequestEvent(FTPChannelChild* aChild,
                      const nsresult& aChannelStatus,
                      const nsCString &aErrorMsg,
                      bool aUseUTF8)
    : mChild(aChild)
    , mChannelStatus(aChannelStatus)
    , mErrorMsg(aErrorMsg)
    , mUseUTF8(aUseUTF8)
  {
  }
  void Run()
  {
    mChild->DoOnStopRequest(mChannelStatus, mErrorMsg, mUseUTF8);
  }

private:
  FTPChannelChild* mChild;
  nsresult mChannelStatus;
  nsCString mErrorMsg;
  bool mUseUTF8;
};

bool
FTPChannelChild::RecvOnStopRequest(const nsresult& aChannelStatus,
                                   const nsCString &aErrorMsg,
                                   const bool &aUseUTF8)
{
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
    "Should not be receiving any more callbacks from parent!");

  LOG(("FTPChannelChild::RecvOnStopRequest [this=%p status=%x]\n",
       this, aChannelStatus));

  mEventQ->RunOrEnqueue(new FTPStopRequestEvent(this, aChannelStatus, aErrorMsg,
                                                aUseUTF8));
  return true;
}

class nsFtpChildAsyncAlert : public Runnable
{
public:
  nsFtpChildAsyncAlert(nsIPrompt *aPrompter, nsString aResponseMsg)
    : mPrompter(aPrompter)
    , mResponseMsg(aResponseMsg)
  {
    MOZ_COUNT_CTOR(nsFtpChildAsyncAlert);
  }
protected:
  virtual ~nsFtpChildAsyncAlert()
  {
    MOZ_COUNT_DTOR(nsFtpChildAsyncAlert);
  }
public:
  NS_IMETHOD Run() override
  {
    if (mPrompter) {
      mPrompter->Alert(nullptr, mResponseMsg.get());
    }
    return NS_OK;
  }
private:
  nsCOMPtr<nsIPrompt> mPrompter;
  nsString mResponseMsg;
};

class MaybeDivertOnStopFTPEvent : public ChannelEvent
{
 public:
  MaybeDivertOnStopFTPEvent(FTPChannelChild* child,
                            const nsresult& aChannelStatus)
  : mChild(child)
  , mChannelStatus(aChannelStatus) {}

  void Run()
  {
    mChild->MaybeDivertOnStop(mChannelStatus);
  }

 private:
  FTPChannelChild* mChild;
  nsresult mChannelStatus;
};

void
FTPChannelChild::MaybeDivertOnStop(const nsresult& aChannelStatus)
{
  if (mDivertingToParent) {
    SendDivertOnStopRequest(aChannelStatus);
  }
}

void
FTPChannelChild::DoOnStopRequest(const nsresult& aChannelStatus,
                                 const nsCString &aErrorMsg,
                                 bool aUseUTF8)
{
  LOG(("FTPChannelChild::DoOnStopRequest [this=%p status=%x]\n",
       this, aChannelStatus));

  if (mDivertingToParent) {
    MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
      "Should not be processing any more callbacks from parent!");

    SendDivertOnStopRequest(aChannelStatus);
    return;
  }

  if (!mCanceled)
    mStatus = aChannelStatus;

  if (mUnknownDecoderInvolved) {
    mUnknownDecoderEventQ.AppendElement(
      MakeUnique<MaybeDivertOnStopFTPEvent>(this, aChannelStatus));
  }

  { // Ensure that all queued ipdl events are dispatched before
    // we initiate protocol deletion below.
    mIsPending = false;
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);
    (void)mListener->OnStopRequest(this, mListenerContext, aChannelStatus);

    if (NS_FAILED(aChannelStatus) && !aErrorMsg.IsEmpty()) {
      nsCOMPtr<nsIPrompt> prompter;
      GetCallback(prompter);
      if (prompter) {
        nsCOMPtr<nsIRunnable> alertEvent;
        if (aUseUTF8) {
          alertEvent = new nsFtpChildAsyncAlert(prompter,
                             NS_ConvertUTF8toUTF16(aErrorMsg));
        } else {
          alertEvent = new nsFtpChildAsyncAlert(prompter,
                             NS_ConvertASCIItoUTF16(aErrorMsg));
        }
        NS_DispatchToMainThread(alertEvent);
      }
    }

    mListener = nullptr;
    mListenerContext = nullptr;

    if (mLoadGroup)
      mLoadGroup->RemoveRequest(this, nullptr, aChannelStatus);
  }

  // This calls NeckoChild::DeallocPFTPChannelChild(), which deletes |this| if IPDL
  // holds the last reference.  Don't rely on |this| existing after here!
  Send__delete__(this);
}

class FTPFailedAsyncOpenEvent : public ChannelEvent
{
 public:
  FTPFailedAsyncOpenEvent(FTPChannelChild* aChild, nsresult aStatus)
  : mChild(aChild), mStatus(aStatus) {}
  void Run() { mChild->DoFailedAsyncOpen(mStatus); }
 private:
  FTPChannelChild* mChild;
  nsresult mStatus;
};

bool
FTPChannelChild::RecvFailedAsyncOpen(const nsresult& statusCode)
{
  LOG(("FTPChannelChild::RecvFailedAsyncOpen [this=%p status=%x]\n",
       this, statusCode));
  mEventQ->RunOrEnqueue(new FTPFailedAsyncOpenEvent(this, statusCode));
  return true;
}

void
FTPChannelChild::DoFailedAsyncOpen(const nsresult& statusCode)
{
  LOG(("FTPChannelChild::DoFailedAsyncOpen [this=%p status=%x]\n",
       this, statusCode));
  mStatus = statusCode;

  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nullptr, statusCode);

  if (mListener) {
    mListener->OnStartRequest(this, mListenerContext);
    mIsPending = false;
    mListener->OnStopRequest(this, mListenerContext, statusCode);
  } else {
    mIsPending = false;
  }

  mListener = nullptr;
  mListenerContext = nullptr;

  if (mIPCOpen)
    Send__delete__(this);
}

class FTPFlushedForDiversionEvent : public ChannelEvent
{
 public:
  explicit FTPFlushedForDiversionEvent(FTPChannelChild* aChild)
  : mChild(aChild)
  {
    MOZ_RELEASE_ASSERT(aChild);
  }

  void Run()
  {
    mChild->FlushedForDiversion();
  }
 private:
  FTPChannelChild* mChild;
};

bool
FTPChannelChild::RecvFlushedForDiversion()
{
  LOG(("FTPChannelChild::RecvFlushedForDiversion [this=%p]\n", this));
  MOZ_ASSERT(mDivertingToParent);

  mEventQ->RunOrEnqueue(new FTPFlushedForDiversionEvent(this), true);
  return true;
}

void
FTPChannelChild::FlushedForDiversion()
{
  LOG(("FTPChannelChild::FlushedForDiversion [this=%p]\n", this));
  MOZ_RELEASE_ASSERT(mDivertingToParent);

  // Once this is set, it should not be unset before FTPChannelChild is taken
  // down. After it is set, no OnStart/OnData/OnStop callbacks should be
  // received from the parent channel, nor dequeued from the ChannelEventQueue.
  mFlushedForDiversion = true;

  SendDivertComplete();
}

bool
FTPChannelChild::RecvDivertMessages()
{
  LOG(("FTPChannelChild::RecvDivertMessages [this=%p]\n", this));
  MOZ_RELEASE_ASSERT(mDivertingToParent);
  MOZ_RELEASE_ASSERT(mSuspendCount > 0);

  // DivertTo() has been called on parent, so we can now start sending queued
  // IPDL messages back to parent listener.
  if (NS_WARN_IF(NS_FAILED(Resume()))) {
    return false;
  }
  return true;
}

class FTPDeleteSelfEvent : public ChannelEvent
{
 public:
  explicit FTPDeleteSelfEvent(FTPChannelChild* aChild)
  : mChild(aChild) {}
  void Run() { mChild->DoDeleteSelf(); }
 private:
  FTPChannelChild* mChild;
};

bool
FTPChannelChild::RecvDeleteSelf()
{
  mEventQ->RunOrEnqueue(new FTPDeleteSelfEvent(this));
  return true;
}

void
FTPChannelChild::DoDeleteSelf()
{
  if (mIPCOpen)
    Send__delete__(this);
}

NS_IMETHODIMP
FTPChannelChild::Cancel(nsresult status)
{
  LOG(("FTPChannelChild::Cancel [this=%p]\n", this));
  if (mCanceled)
    return NS_OK;

  mCanceled = true;
  mStatus = status;
  if (mIPCOpen)
    SendCancel(status);
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::Suspend()
{
  NS_ENSURE_TRUE(mIPCOpen, NS_ERROR_NOT_AVAILABLE);

  LOG(("FTPChannelChild::Suspend [this=%p]\n", this));

  // SendSuspend only once, when suspend goes from 0 to 1.
  // Don't SendSuspend at all if we're diverting callbacks to the parent;
  // suspend will be called at the correct time in the parent itself.
  if (!mSuspendCount++ && !mDivertingToParent) {
    SendSuspend();
    mSuspendSent = true;
  }
  mEventQ->Suspend();

  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::Resume()
{
  NS_ENSURE_TRUE(mIPCOpen, NS_ERROR_NOT_AVAILABLE);

  LOG(("FTPChannelChild::Resume [this=%p]\n", this));

  // SendResume only once, when suspend count drops to 0.
  // Don't SendResume at all if we're diverting callbacks to the parent (unless
  // suspend was sent earlier); otherwise, resume will be called at the correct
  // time in the parent itself.
  if (!--mSuspendCount && (!mDivertingToParent || mSuspendSent)) {
    SendResume();
  }
  mEventQ->Resume();

  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelChild::nsIChildChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelChild::ConnectParent(uint32_t id)
{
  LOG(("FTPChannelChild::ConnectParent [this=%p]\n", this));

  mozilla::dom::TabChild* tabChild = nullptr;
  nsCOMPtr<nsITabChild> iTabChild;
  NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup,
                                NS_GET_IID(nsITabChild),
                                getter_AddRefs(iTabChild));
  GetCallback(iTabChild);
  if (iTabChild) {
    tabChild = static_cast<mozilla::dom::TabChild*>(iTabChild.get());
  }

  // The socket transport in the chrome process now holds a logical ref to us
  // until OnStopRequest, or we do a redirect, or we hit an IPDL error.
  AddIPDLReference();

  FTPChannelConnectArgs connectArgs(id);

  if (!gNeckoChild->SendPFTPChannelConstructor(this, tabChild,
                                               IPC::SerializedLoadContext(this),
                                               connectArgs)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::CompleteRedirectSetup(nsIStreamListener *listener,
                                       nsISupports *aContext)
{
  LOG(("FTPChannelChild::CompleteRedirectSetup [this=%p]\n", this));

  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  mIsPending = true;
  mWasOpened = true;
  mListener = listener;
  mListenerContext = aContext;

  // add ourselves to the load group.
  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nullptr);

  // We already have an open IPDL connection to the parent. If on-modify-request
  // listeners or load group observers canceled us, let the parent handle it
  // and send it back to us naturally.
  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelChild::nsIDivertableChannel
//-----------------------------------------------------------------------------
NS_IMETHODIMP
FTPChannelChild::DivertToParent(ChannelDiverterChild **aChild)
{
  MOZ_RELEASE_ASSERT(aChild);
  MOZ_RELEASE_ASSERT(gNeckoChild);
  MOZ_RELEASE_ASSERT(!mDivertingToParent);

  LOG(("FTPChannelChild::DivertToParent [this=%p]\n", this));

  // We must fail DivertToParent() if there's no parent end of the channel (and
  // won't be!) due to early failure.
  if (NS_FAILED(mStatus) && !mIPCOpen) {
    return mStatus;
  }

  nsresult rv = Suspend();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Once this is set, it should not be unset before the child is taken down.
  mDivertingToParent = true;

  PChannelDiverterChild* diverter =
    gNeckoChild->SendPChannelDiverterConstructor(this);
  MOZ_RELEASE_ASSERT(diverter);

  *aChild = static_cast<ChannelDiverterChild*>(diverter);

  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::UnknownDecoderInvolvedKeepData()
{
  mUnknownDecoderInvolved = true;
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelChild::UnknownDecoderInvolvedOnStartRequestCalled()
{
  mUnknownDecoderInvolved = false;

  nsresult rv = NS_OK;

  if (mDivertingToParent) {
    rv = mEventQ->PrependEvents(mUnknownDecoderEventQ);
  }
  mUnknownDecoderEventQ.Clear();

  return rv;
}

NS_IMETHODIMP
FTPChannelChild::GetDivertingToParent(bool* aDiverting)
{
  NS_ENSURE_ARG_POINTER(aDiverting);
  *aDiverting = mDivertingToParent;
  return NS_OK;
}

} // namespace net
} // namespace mozilla

