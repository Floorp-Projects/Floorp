/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWyciwyg.h"

#include "mozilla/net/NeckoChild.h"
#include "WyciwygChannelChild.h"
#include "mozilla/dom/TabChild.h"

#include "nsCharsetSource.h"
#include "nsStringStream.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsISerializable.h"
#include "nsSerializationHelper.h"
#include "nsILoadContext.h"
#include "mozilla/ipc/URIUtils.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS4(WyciwygChannelChild,
                   nsIRequest,
                   nsIChannel,
                   nsIWyciwygChannel,
                   nsIPrivateBrowsingChannel)


WyciwygChannelChild::WyciwygChannelChild()
  : mStatus(NS_OK)
  , mIsPending(false)
  , mCanceled(false)
  , mLoadFlags(LOAD_NORMAL)
  , mContentLength(-1)
  , mCharsetSource(kCharsetUninitialized)
  , mState(WCC_NEW)
  , mIPCOpen(false)
  , mSentAppData(false)
  , mEventQ(NS_ISUPPORTS_CAST(nsIWyciwygChannel*, this))
{
  LOG(("Creating WyciwygChannelChild @%x\n", this));
}

WyciwygChannelChild::~WyciwygChannelChild()
{
  LOG(("Destroying WyciwygChannelChild @%x\n", this));
}

void
WyciwygChannelChild::AddIPDLReference()
{
  NS_ABORT_IF_FALSE(!mIPCOpen, "Attempt to retain more than one IPDL reference");
  mIPCOpen = true;
  AddRef();
}

void
WyciwygChannelChild::ReleaseIPDLReference()
{
  NS_ABORT_IF_FALSE(mIPCOpen, "Attempt to release nonexistent IPDL reference");
  mIPCOpen = false;
  Release();
}

nsresult
WyciwygChannelChild::Init(nsIURI* uri)
{
  NS_ENSURE_ARG_POINTER(uri);

  mState = WCC_INIT;

  mURI = uri;
  mOriginalURI = uri;

  URIParams serializedUri;
  SerializeURI(uri, serializedUri);

  SendInit(serializedUri);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// WyciwygChannelChild::PWyciwygChannelChild
//-----------------------------------------------------------------------------

class WyciwygStartRequestEvent : public ChannelEvent
{
public:
  WyciwygStartRequestEvent(WyciwygChannelChild* child,
                           const nsresult& statusCode,
                           const int64_t& contentLength,
                           const int32_t& source,
                           const nsCString& charset,
                           const nsCString& securityInfo)
  : mChild(child), mStatusCode(statusCode), mContentLength(contentLength),
    mSource(source), mCharset(charset), mSecurityInfo(securityInfo) {}
  void Run() { mChild->OnStartRequest(mStatusCode, mContentLength, mSource,
                                     mCharset, mSecurityInfo); }
private:
  WyciwygChannelChild* mChild;
  nsresult mStatusCode;
  int64_t mContentLength;
  int32_t mSource;
  nsCString mCharset;
  nsCString mSecurityInfo;
};

bool
WyciwygChannelChild::RecvOnStartRequest(const nsresult& statusCode,
                                        const int64_t& contentLength,
                                        const int32_t& source,
                                        const nsCString& charset,
                                        const nsCString& securityInfo)
{
  if (mEventQ.ShouldEnqueue()) {
    mEventQ.Enqueue(new WyciwygStartRequestEvent(this, statusCode,
                                                 contentLength, source,
                                                 charset, securityInfo));
  } else {
    OnStartRequest(statusCode, contentLength, source, charset, securityInfo);
  }
  return true;
}

void
WyciwygChannelChild::OnStartRequest(const nsresult& statusCode,
                                    const int64_t& contentLength,
                                    const int32_t& source,
                                    const nsCString& charset,
                                    const nsCString& securityInfo)
{
  LOG(("WyciwygChannelChild::RecvOnStartRequest [this=%x]\n", this));

  mState = WCC_ONSTART;

  mStatus = statusCode;
  mContentLength = contentLength;
  mCharsetSource = source;
  mCharset = charset;

  if (!securityInfo.IsEmpty()) {
    NS_DeserializeObject(securityInfo, getter_AddRefs(mSecurityInfo));
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);

  nsresult rv = mListener->OnStartRequest(this, mListenerContext);
  if (NS_FAILED(rv))
    Cancel(rv);
}

class WyciwygDataAvailableEvent : public ChannelEvent
{
public:
  WyciwygDataAvailableEvent(WyciwygChannelChild* child,
                            const nsCString& data,
                            const uint64_t& offset)
  : mChild(child), mData(data), mOffset(offset) {}
  void Run() { mChild->OnDataAvailable(mData, mOffset); }
private:
  WyciwygChannelChild* mChild;
  nsCString mData;
  uint64_t mOffset;
};

bool
WyciwygChannelChild::RecvOnDataAvailable(const nsCString& data,
                                         const uint64_t& offset)
{
  if (mEventQ.ShouldEnqueue()) {
    mEventQ.Enqueue(new WyciwygDataAvailableEvent(this, data, offset));
  } else {
    OnDataAvailable(data, offset);
  }
  return true;
}

void
WyciwygChannelChild::OnDataAvailable(const nsCString& data,
                                     const uint64_t& offset)
{
  LOG(("WyciwygChannelChild::RecvOnDataAvailable [this=%x]\n", this));

  if (mCanceled)
    return;

  mState = WCC_ONDATA;

  // NOTE: the OnDataAvailable contract requires the client to read all the data
  // in the inputstream.  This code relies on that ('data' will go away after
  // this function).  Apparently the previous, non-e10s behavior was to actually
  // support only reading part of the data, allowing later calls to read the
  // rest.
  nsCOMPtr<nsIInputStream> stringStream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stringStream),
                                      data.get(),
                                      data.Length(),
                                      NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
  
  rv = mListener->OnDataAvailable(this, mListenerContext,
                                  stringStream, offset, data.Length());
  if (NS_FAILED(rv))
    Cancel(rv);

  if (mProgressSink && NS_SUCCEEDED(rv) && !(mLoadFlags & LOAD_BACKGROUND))
    mProgressSink->OnProgress(this, nullptr, offset + data.Length(),
                              uint64_t(mContentLength));
}

class WyciwygStopRequestEvent : public ChannelEvent
{
public:
  WyciwygStopRequestEvent(WyciwygChannelChild* child,
                          const nsresult& statusCode)
  : mChild(child), mStatusCode(statusCode) {}
  void Run() { mChild->OnStopRequest(mStatusCode); }
private:
  WyciwygChannelChild* mChild;
  nsresult mStatusCode;
};

bool
WyciwygChannelChild::RecvOnStopRequest(const nsresult& statusCode)
{
  if (mEventQ.ShouldEnqueue()) {
    mEventQ.Enqueue(new WyciwygStopRequestEvent(this, statusCode));
  } else {
    OnStopRequest(statusCode);
  }
  return true;
}

void
WyciwygChannelChild::OnStopRequest(const nsresult& statusCode)
{
  LOG(("WyciwygChannelChild::RecvOnStopRequest [this=%x status=%u]\n",
           this, statusCode));

  { // We need to ensure that all IPDL message dispatching occurs
    // before we delete the protocol below
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);

    mState = WCC_ONSTOP;

    mIsPending = false;

    if (!mCanceled)
      mStatus = statusCode;

    mListener->OnStopRequest(this, mListenerContext, statusCode);

    mListener = 0;
    mListenerContext = 0;

    if (mLoadGroup)
      mLoadGroup->RemoveRequest(this, nullptr, mStatus);

    mCallbacks = 0;
    mProgressSink = 0;
  }

  if (mIPCOpen)
    PWyciwygChannelChild::Send__delete__(this);
}

class WyciwygCancelEvent : public ChannelEvent
{
 public:
  WyciwygCancelEvent(WyciwygChannelChild* child, const nsresult& status)
  : mChild(child)
  , mStatus(status) {}

  void Run() { mChild->CancelEarly(mStatus); }
 private:
  WyciwygChannelChild* mChild;
  nsresult mStatus;
};

bool
WyciwygChannelChild::RecvCancelEarly(const nsresult& statusCode)
{
  if (mEventQ.ShouldEnqueue()) {
    mEventQ.Enqueue(new WyciwygCancelEvent(this, statusCode));
  } else {
    CancelEarly(statusCode);
  }
  return true;
}

void WyciwygChannelChild::CancelEarly(const nsresult& statusCode)
{
  LOG(("WyciwygChannelChild::CancelEarly [this=%x]\n", this));
  
  if (mCanceled)
    return;

  mCanceled = true;
  mStatus = statusCode;
  
  mIsPending = false;
  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nullptr, mStatus);

  if (mListener) {
    mListener->OnStartRequest(this, mListenerContext);
    mListener->OnStopRequest(this, mListenerContext, mStatus);
  }
  mListener = nullptr;
  mListenerContext = nullptr;

  if (mIPCOpen)
    PWyciwygChannelChild::Send__delete__(this);
}

//-----------------------------------------------------------------------------
// nsIRequest
//-----------------------------------------------------------------------------

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP
WyciwygChannelChild::GetName(nsACString & aName)
{
  return mURI->GetSpec(aName);
}

/* boolean isPending (); */
NS_IMETHODIMP
WyciwygChannelChild::IsPending(bool *aIsPending)
{
  *aIsPending = mIsPending;
  return NS_OK;
}

/* readonly attribute nsresult status; */
NS_IMETHODIMP
WyciwygChannelChild::GetStatus(nsresult *aStatus)
{
  *aStatus = mStatus;
  return NS_OK;
}

/* void cancel (in nsresult aStatus); */
NS_IMETHODIMP
WyciwygChannelChild::Cancel(nsresult aStatus)
{
  if (mCanceled)
    return NS_OK;

  mCanceled = true;
  mStatus = aStatus;
  if (mIPCOpen)
    SendCancel(aStatus);
  return NS_OK;
}

/* void suspend (); */
NS_IMETHODIMP
WyciwygChannelChild::Suspend()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void resume (); */
NS_IMETHODIMP
WyciwygChannelChild::Resume()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsILoadGroup loadGroup; */
NS_IMETHODIMP
WyciwygChannelChild::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}
NS_IMETHODIMP
WyciwygChannelChild::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
  if (!CanSetLoadGroup(aLoadGroup)) {
    return NS_ERROR_FAILURE;
  }

  mLoadGroup = aLoadGroup;
  NS_QueryNotificationCallbacks(mCallbacks,
                                mLoadGroup,
                                NS_GET_IID(nsIProgressEventSink),
                                getter_AddRefs(mProgressSink));
  return NS_OK;
}

/* attribute nsLoadFlags loadFlags; */
NS_IMETHODIMP
WyciwygChannelChild::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}
NS_IMETHODIMP
WyciwygChannelChild::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  mLoadFlags = aLoadFlags;
  return NS_OK;
}


//-----------------------------------------------------------------------------
// nsIChannel
//-----------------------------------------------------------------------------

/* attribute nsIURI originalURI; */
NS_IMETHODIMP
WyciwygChannelChild::GetOriginalURI(nsIURI * *aOriginalURI)
{
  *aOriginalURI = mOriginalURI;
  NS_ADDREF(*aOriginalURI);
  return NS_OK;
}
NS_IMETHODIMP
WyciwygChannelChild::SetOriginalURI(nsIURI * aOriginalURI)
{
  NS_ENSURE_TRUE(mState == WCC_INIT, NS_ERROR_UNEXPECTED);

  NS_ENSURE_ARG_POINTER(aOriginalURI);
  mOriginalURI = aOriginalURI;
  return NS_OK;
}

/* readonly attribute nsIURI URI; */
NS_IMETHODIMP
WyciwygChannelChild::GetURI(nsIURI * *aURI)
{
  *aURI = mURI;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

/* attribute nsISupports owner; */
NS_IMETHODIMP
WyciwygChannelChild::GetOwner(nsISupports * *aOwner)
{
  NS_PRECONDITION(mOwner, "Must have a principal!");
  NS_ENSURE_STATE(mOwner);

  NS_ADDREF(*aOwner = mOwner);
  return NS_OK;
}
NS_IMETHODIMP
WyciwygChannelChild::SetOwner(nsISupports * aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

/* attribute nsIInterfaceRequestor notificationCallbacks; */
NS_IMETHODIMP
WyciwygChannelChild::GetNotificationCallbacks(nsIInterfaceRequestor * *aCallbacks)
{
  *aCallbacks = mCallbacks;
  NS_IF_ADDREF(*aCallbacks);
  return NS_OK;
}
NS_IMETHODIMP
WyciwygChannelChild::SetNotificationCallbacks(nsIInterfaceRequestor * aCallbacks)
{
  if (!CanSetCallbacks(aCallbacks)) {
    return NS_ERROR_FAILURE;
  }

  mCallbacks = aCallbacks;
  NS_QueryNotificationCallbacks(mCallbacks,
                                mLoadGroup,
                                NS_GET_IID(nsIProgressEventSink),
                                getter_AddRefs(mProgressSink));
  return NS_OK;
}

/* readonly attribute nsISupports securityInfo; */
NS_IMETHODIMP
WyciwygChannelChild::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);

  return NS_OK;
}

/* attribute ACString contentType; */
NS_IMETHODIMP
WyciwygChannelChild::GetContentType(nsACString & aContentType)
{
  aContentType.AssignLiteral(WYCIWYG_TYPE);
  return NS_OK;
}
NS_IMETHODIMP
WyciwygChannelChild::SetContentType(const nsACString & aContentType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute ACString contentCharset; */
NS_IMETHODIMP
WyciwygChannelChild::GetContentCharset(nsACString & aContentCharset)
{
  aContentCharset.Assign("UTF-16");
  return NS_OK;
}
NS_IMETHODIMP
WyciwygChannelChild::SetContentCharset(const nsACString & aContentCharset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WyciwygChannelChild::GetContentDisposition(uint32_t *aContentDisposition)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
WyciwygChannelChild::SetContentDisposition(uint32_t aContentDisposition)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
WyciwygChannelChild::GetContentDispositionFilename(nsAString &aContentDispositionFilename)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
WyciwygChannelChild::SetContentDispositionFilename(const nsAString &aContentDispositionFilename)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
WyciwygChannelChild::GetContentDispositionHeader(nsACString &aContentDispositionHeader)
{
  return NS_ERROR_NOT_AVAILABLE;
}

/* attribute int64_t contentLength; */
NS_IMETHODIMP
WyciwygChannelChild::GetContentLength(int64_t *aContentLength)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
WyciwygChannelChild::SetContentLength(int64_t aContentLength)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIInputStream open (); */
NS_IMETHODIMP
WyciwygChannelChild::Open(nsIInputStream **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

static mozilla::dom::TabChild*
GetTabChild(nsIChannel* aChannel)
{
  nsCOMPtr<nsITabChild> iTabChild;
  NS_QueryNotificationCallbacks(aChannel, iTabChild);
  return iTabChild ? static_cast<mozilla::dom::TabChild*>(iTabChild.get()) : nullptr;
}

/* void asyncOpen (in nsIStreamListener aListener, in nsISupports aContext); */
NS_IMETHODIMP
WyciwygChannelChild::AsyncOpen(nsIStreamListener *aListener, nsISupports *aContext)
{
  LOG(("WyciwygChannelChild::AsyncOpen [this=%x]\n", this));

  // The only places creating wyciwyg: channels should be
  // HTMLDocument::OpenCommon and session history.  Both should be setting an
  // owner.
  NS_PRECONDITION(mOwner, "Must have a principal");
  NS_ENSURE_STATE(mOwner);

  NS_ENSURE_ARG_POINTER(aListener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);

  mListener = aListener;
  mListenerContext = aContext;
  mIsPending = true;

  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nullptr);

  URIParams originalURI;
  SerializeURI(mOriginalURI, originalURI);

  mozilla::dom::TabChild* tabChild = GetTabChild(this);
  SendAsyncOpen(originalURI, mLoadFlags, IPC::SerializedLoadContext(this), tabChild);

  mSentAppData = true;
  mState = WCC_OPENED;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIWyciwygChannel
//-----------------------------------------------------------------------------

/* void writeToCacheEntry (in AString aData); */
NS_IMETHODIMP
WyciwygChannelChild::WriteToCacheEntry(const nsAString & aData)
{
  NS_ENSURE_TRUE((mState == WCC_INIT) ||
                 (mState == WCC_ONWRITE), NS_ERROR_UNEXPECTED);

  if (!mSentAppData) {
    mozilla::dom::TabChild* tabChild = GetTabChild(this);
    SendAppData(IPC::SerializedLoadContext(this), tabChild);
    mSentAppData = true;
  }

  SendWriteToCacheEntry(PromiseFlatString(aData));
  mState = WCC_ONWRITE;
  return NS_OK;
}

/* void closeCacheEntry (in nsresult reason); */
NS_IMETHODIMP
WyciwygChannelChild::CloseCacheEntry(nsresult reason)
{
  NS_ENSURE_TRUE(mState == WCC_ONWRITE, NS_ERROR_UNEXPECTED);

  SendCloseCacheEntry(reason);
  mState = WCC_ONCLOSED;

  if (mIPCOpen)
    PWyciwygChannelChild::Send__delete__(this);

  return NS_OK;
}

/* void setSecurityInfo (in nsISupports aSecurityInfo); */
NS_IMETHODIMP
WyciwygChannelChild::SetSecurityInfo(nsISupports *aSecurityInfo)
{
  mSecurityInfo = aSecurityInfo;

  if (mSecurityInfo) {
    nsCOMPtr<nsISerializable> serializable = do_QueryInterface(mSecurityInfo);
    if (serializable) {
      nsCString secInfoStr;
      NS_SerializeToString(serializable, secInfoStr);
      SendSetSecurityInfo(secInfoStr);
    }
    else {
      NS_WARNING("Can't serialize security info");
    }
  }

  return NS_OK;
}

/* void setCharsetAndSource (in long aSource, in ACString aCharset); */
NS_IMETHODIMP
WyciwygChannelChild::SetCharsetAndSource(int32_t aSource, const nsACString & aCharset)
{
  // mState == WCC_ONSTART when reading from the channel
  // mState == WCC_INIT when writing to the cache
  NS_ENSURE_TRUE((mState == WCC_ONSTART) ||
                 (mState == WCC_INIT), NS_ERROR_UNEXPECTED);

  mCharsetSource = aSource;
  mCharset = aCharset;

  // TODO ensure that nsWyciwygChannel in the parent has still the cache entry
  SendSetCharsetAndSource(mCharsetSource, mCharset);
  return NS_OK;
}

/* ACString getCharsetAndSource (out long aSource); */
NS_IMETHODIMP
WyciwygChannelChild::GetCharsetAndSource(int32_t *aSource, nsACString & _retval)
{
  NS_ENSURE_TRUE((mState == WCC_ONSTART) ||
                 (mState == WCC_ONDATA) ||
                 (mState == WCC_ONSTOP), NS_ERROR_NOT_AVAILABLE);

  if (mCharsetSource == kCharsetUninitialized)
    return NS_ERROR_NOT_AVAILABLE;

  *aSource = mCharsetSource;
  _retval = mCharset;
  return NS_OK;
}

//------------------------------------------------------------------------------
}} // mozilla::net
