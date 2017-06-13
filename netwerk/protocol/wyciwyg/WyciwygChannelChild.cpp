/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWyciwyg.h"

#include "base/compiler_specific.h"

#include "mozilla/net/ChannelEventQueue.h"
#include "WyciwygChannelChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/net/NeckoChild.h"

#include "nsCharsetSource.h"
#include "nsContentUtils.h"
#include "nsStringStream.h"
#include "nsNetUtil.h"
#include "nsISerializable.h"
#include "nsSerializationHelper.h"
#include "nsIProgressEventSink.h"
#include "mozilla/ipc/URIUtils.h"
#include "SerializedLoadContext.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "nsProxyRelease.h"
#include "nsContentSecurityManager.h"

using namespace mozilla::ipc;
using namespace mozilla::dom;

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(WyciwygChannelChild,
                  nsIRequest,
                  nsIChannel,
                  nsIWyciwygChannel,
                  nsIPrivateBrowsingChannel)


WyciwygChannelChild::WyciwygChannelChild(nsIEventTarget *aNeckoTarget)
  : NeckoTargetHolder(aNeckoTarget)
  , mStatus(NS_OK)
  , mIsPending(false)
  , mCanceled(false)
  , mLoadFlags(LOAD_NORMAL)
  , mContentLength(-1)
  , mCharsetSource(kCharsetUninitialized)
  , mState(WCC_NEW)
  , mIPCOpen(false)
  , mSentAppData(false)
{
  LOG(("Creating WyciwygChannelChild @%p\n", this));
  mEventQ = new ChannelEventQueue(NS_ISUPPORTS_CAST(nsIWyciwygChannel*, this));

  if (mNeckoTarget) {
    gNeckoChild->SetEventTargetForActor(this, mNeckoTarget);
  }

  gNeckoChild->SendPWyciwygChannelConstructor(this);
  // IPDL holds a reference until IPDL channel gets destroyed
  AddIPDLReference();
}

WyciwygChannelChild::~WyciwygChannelChild()
{
  LOG(("Destroying WyciwygChannelChild @%p\n", this));
  if (mLoadInfo) {
    NS_ReleaseOnMainThread(mLoadInfo.forget());
  }
}

void
WyciwygChannelChild::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen, "Attempt to retain more than one IPDL reference");
  mIPCOpen = true;
  AddRef();
}

void
WyciwygChannelChild::ReleaseIPDLReference()
{
  MOZ_ASSERT(mIPCOpen, "Attempt to release nonexistent IPDL reference");
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

  // propagate loadInfo
  mozilla::ipc::PrincipalInfo requestingPrincipalInfo;
  mozilla::ipc::PrincipalInfo triggeringPrincipalInfo;
  mozilla::ipc::PrincipalInfo principalToInheritInfo;
  uint32_t securityFlags;
  uint32_t policyType;
  if (mLoadInfo) {
    mozilla::ipc::PrincipalToPrincipalInfo(mLoadInfo->LoadingPrincipal(),
                                           &requestingPrincipalInfo);
    mozilla::ipc::PrincipalToPrincipalInfo(mLoadInfo->TriggeringPrincipal(),
                                           &triggeringPrincipalInfo);
    mozilla::ipc::PrincipalToPrincipalInfo(mLoadInfo->PrincipalToInherit(),
                                           &principalToInheritInfo);
    securityFlags = mLoadInfo->GetSecurityFlags();
    policyType = mLoadInfo->InternalContentPolicyType();
  }
  else {
    // use default values if no loadInfo is provided
    mozilla::ipc::PrincipalToPrincipalInfo(nsContentUtils::GetSystemPrincipal(),
                                           &requestingPrincipalInfo);
    mozilla::ipc::PrincipalToPrincipalInfo(nsContentUtils::GetSystemPrincipal(),
                                           &triggeringPrincipalInfo);
    mozilla::ipc::PrincipalToPrincipalInfo(nsContentUtils::GetSystemPrincipal(),
                                           &principalToInheritInfo);
    securityFlags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL;
    policyType = nsIContentPolicy::TYPE_OTHER;
  }

  SendInit(serializedUri,
           requestingPrincipalInfo,
           triggeringPrincipalInfo,
           principalToInheritInfo,
           securityFlags,
           policyType);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// WyciwygChannelChild::PWyciwygChannelChild
//-----------------------------------------------------------------------------

class WyciwygStartRequestEvent
  : public NeckoTargetChannelEvent<WyciwygChannelChild>
{
public:
  WyciwygStartRequestEvent(WyciwygChannelChild* child,
                           const nsresult& statusCode,
                           const int64_t& contentLength,
                           const int32_t& source,
                           const nsCString& charset,
                           const nsCString& securityInfo)
  : NeckoTargetChannelEvent<WyciwygChannelChild>(child)
  , mStatusCode(statusCode)
  , mContentLength(contentLength)
  , mSource(source)
  , mCharset(charset)
  , mSecurityInfo(securityInfo) {}
  void Run() { mChild->OnStartRequest(mStatusCode, mContentLength, mSource,
                                     mCharset, mSecurityInfo); }
private:
  nsresult mStatusCode;
  int64_t mContentLength;
  int32_t mSource;
  nsCString mCharset;
  nsCString mSecurityInfo;
};

mozilla::ipc::IPCResult
WyciwygChannelChild::RecvOnStartRequest(const nsresult& statusCode,
                                        const int64_t& contentLength,
                                        const int32_t& source,
                                        const nsCString& charset,
                                        const nsCString& securityInfo)
{
  mEventQ->RunOrEnqueue(new WyciwygStartRequestEvent(this, statusCode,
                                                     contentLength, source,
                                                     charset, securityInfo));
  return IPC_OK();
}

void
WyciwygChannelChild::OnStartRequest(const nsresult& statusCode,
                                    const int64_t& contentLength,
                                    const int32_t& source,
                                    const nsCString& charset,
                                    const nsCString& securityInfo)
{
  LOG(("WyciwygChannelChild::RecvOnStartRequest [this=%p]\n", this));

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

class WyciwygDataAvailableEvent
  : public NeckoTargetChannelEvent<WyciwygChannelChild>
{
public:
  WyciwygDataAvailableEvent(WyciwygChannelChild* child,
                            const nsCString& data,
                            const uint64_t& offset)
  : NeckoTargetChannelEvent<WyciwygChannelChild>(child)
  , mData(data)
  , mOffset(offset) {}
  void Run() { mChild->OnDataAvailable(mData, mOffset); }
private:
  nsCString mData;
  uint64_t mOffset;
};

mozilla::ipc::IPCResult
WyciwygChannelChild::RecvOnDataAvailable(const nsCString& data,
                                         const uint64_t& offset)
{
  mEventQ->RunOrEnqueue(new WyciwygDataAvailableEvent(this, data, offset));
  return IPC_OK();
}

void
WyciwygChannelChild::OnDataAvailable(const nsCString& data,
                                     const uint64_t& offset)
{
  LOG(("WyciwygChannelChild::RecvOnDataAvailable [this=%p]\n", this));

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

  if (mProgressSink && NS_SUCCEEDED(rv)) {
    mProgressSink->OnProgress(this, nullptr, offset + data.Length(),
                              mContentLength);
  }
}

class WyciwygStopRequestEvent
  : public NeckoTargetChannelEvent<WyciwygChannelChild>
{
public:
  WyciwygStopRequestEvent(WyciwygChannelChild* child,
                          const nsresult& statusCode)
  : NeckoTargetChannelEvent<WyciwygChannelChild>(child)
  , mStatusCode(statusCode) {}
  void Run() { mChild->OnStopRequest(mStatusCode); }
private:
  nsresult mStatusCode;
};

mozilla::ipc::IPCResult
WyciwygChannelChild::RecvOnStopRequest(const nsresult& statusCode)
{
  mEventQ->RunOrEnqueue(new WyciwygStopRequestEvent(this, statusCode));
  return IPC_OK();
}

void
WyciwygChannelChild::OnStopRequest(const nsresult& statusCode)
{
  LOG(("WyciwygChannelChild::RecvOnStopRequest [this=%p status=%" PRIu32 "]\n",
       this, static_cast<uint32_t>(statusCode)));

  { // We need to ensure that all IPDL message dispatching occurs
    // before we delete the protocol below
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);

    mState = WCC_ONSTOP;

    mIsPending = false;

    if (!mCanceled)
      mStatus = statusCode;

    mListener->OnStopRequest(this, mListenerContext, statusCode);

    mListener = nullptr;
    mListenerContext = nullptr;

    if (mLoadGroup)
      mLoadGroup->RemoveRequest(this, nullptr, mStatus);

    mCallbacks = nullptr;
    mProgressSink = nullptr;
  }

  if (mIPCOpen)
    PWyciwygChannelChild::Send__delete__(this);
}

class WyciwygCancelEvent : public NeckoTargetChannelEvent<WyciwygChannelChild>
{
 public:
  WyciwygCancelEvent(WyciwygChannelChild* child, const nsresult& status)
  : NeckoTargetChannelEvent<WyciwygChannelChild>(child)
  , mStatus(status) {}

  void Run() { mChild->CancelEarly(mStatus); }
 private:
  nsresult mStatus;
};

mozilla::ipc::IPCResult
WyciwygChannelChild::RecvCancelEarly(const nsresult& statusCode)
{
  mEventQ->RunOrEnqueue(new WyciwygCancelEvent(this, statusCode));
  return IPC_OK();
}

void WyciwygChannelChild::CancelEarly(const nsresult& statusCode)
{
  LOG(("WyciwygChannelChild::CancelEarly [this=%p]\n", this));
  
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

NS_IMETHODIMP
WyciwygChannelChild::GetName(nsACString & aName)
{
  return mURI->GetSpec(aName);
}

NS_IMETHODIMP
WyciwygChannelChild::IsPending(bool *aIsPending)
{
  *aIsPending = mIsPending;
  return NS_OK;
}

NS_IMETHODIMP
WyciwygChannelChild::GetStatus(nsresult *aStatus)
{
  *aStatus = mStatus;
  return NS_OK;
}

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

NS_IMETHODIMP
WyciwygChannelChild::Suspend()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WyciwygChannelChild::Resume()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

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

  UpdatePrivateBrowsing();
  return NS_OK;
}

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

NS_IMETHODIMP
WyciwygChannelChild::GetURI(nsIURI * *aURI)
{
  *aURI = mURI;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
WyciwygChannelChild::GetOwner(nsISupports * *aOwner)
{
  NS_IF_ADDREF(*aOwner = mOwner);
  return NS_OK;
}
NS_IMETHODIMP
WyciwygChannelChild::SetOwner(nsISupports * aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
WyciwygChannelChild::GetLoadInfo(nsILoadInfo **aLoadInfo)
{
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP
WyciwygChannelChild::SetLoadInfo(nsILoadInfo* aLoadInfo)
{
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

NS_IMETHODIMP
WyciwygChannelChild::GetIsDocument(bool *aIsDocument)
{
  return NS_GetIsDocumentChannel(this, aIsDocument);
}

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
  UpdatePrivateBrowsing();
  return NS_OK;
}

NS_IMETHODIMP
WyciwygChannelChild::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);

  return NS_OK;
}

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

NS_IMETHODIMP
WyciwygChannelChild::GetContentCharset(nsACString & aContentCharset)
{
  aContentCharset.AssignLiteral("UTF-16");
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

NS_IMETHODIMP
WyciwygChannelChild::Open(nsIInputStream **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WyciwygChannelChild::Open2(nsIInputStream** aStream)
{
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return Open(aStream);
}

static mozilla::dom::TabChild*
GetTabChild(nsIChannel* aChannel)
{
  nsCOMPtr<nsITabChild> iTabChild;
  NS_QueryNotificationCallbacks(aChannel, iTabChild);
  return iTabChild ? static_cast<mozilla::dom::TabChild*>(iTabChild.get()) : nullptr;
}

NS_IMETHODIMP
WyciwygChannelChild::AsyncOpen(nsIStreamListener *aListener, nsISupports *aContext)
{
  MOZ_ASSERT(!mLoadInfo ||
             mLoadInfo->GetSecurityMode() == 0 ||
             mLoadInfo->GetInitialSecurityCheckDone() ||
             (mLoadInfo->GetSecurityMode() == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL &&
              nsContentUtils::IsSystemPrincipal(mLoadInfo->LoadingPrincipal())),
             "security flags in loadInfo but asyncOpen2() not called");

  LOG(("WyciwygChannelChild::AsyncOpen [this=%p]\n", this));

  // The only places creating wyciwyg: channels should be
  // HTMLDocument::OpenCommon and session history.  Both should be setting an
  // owner or loadinfo.
  NS_PRECONDITION(mOwner || mLoadInfo, "Must have a principal");
  NS_ENSURE_STATE(mOwner || mLoadInfo);

  NS_ENSURE_ARG_POINTER(aListener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);

  mListener = aListener;
  mListenerContext = aContext;
  mIsPending = true;

  if (mLoadGroup) {
    mLoadGroup->AddRequest(this, nullptr);
  }

  URIParams originalURI;
  SerializeURI(mOriginalURI, originalURI);

  mozilla::dom::TabChild* tabChild = GetTabChild(this);
  if (MissingRequiredTabChild(tabChild, "wyciwyg")) {
    mCallbacks = nullptr;
    return NS_ERROR_ILLEGAL_VALUE;
  }

  PBrowserOrId browser = static_cast<ContentChild*>(Manager()->Manager())
                         ->GetBrowserOrId(tabChild);

  SendAsyncOpen(originalURI, mLoadFlags, IPC::SerializedLoadContext(this), browser);

  mSentAppData = true;
  mState = WCC_OPENED;

  return NS_OK;
}

NS_IMETHODIMP
WyciwygChannelChild::AsyncOpen2(nsIStreamListener *aListener)
{
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  if (NS_FAILED(rv)) {
    mCallbacks = nullptr;
    return rv;
  }
  return AsyncOpen(listener, nullptr);
}

//-----------------------------------------------------------------------------
// nsIWyciwygChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WyciwygChannelChild::WriteToCacheEntry(const nsAString & aData)
{
  NS_ENSURE_TRUE((mState == WCC_INIT) ||
                 (mState == WCC_ONWRITE), NS_ERROR_UNEXPECTED);

  if (!mSentAppData) {
    mozilla::dom::TabChild* tabChild = GetTabChild(this);

    PBrowserOrId browser = static_cast<ContentChild*>(Manager()->Manager())
                           ->GetBrowserOrId(tabChild);

    SendAppData(IPC::SerializedLoadContext(this), browser);
    mSentAppData = true;
  }

  mState = WCC_ONWRITE;

  // Give ourselves a megabyte of headroom for the message size. Convert bytes
  // to wide chars.
  static const size_t kMaxMessageSize = (IPC::Channel::kMaximumMessageSize - 1024) / 2;

  size_t curIndex = 0;
  size_t charsRemaining = aData.Length();
  do {
    size_t chunkSize = std::min(charsRemaining, kMaxMessageSize);
    SendWriteToCacheEntry(Substring(aData, curIndex, chunkSize));

    charsRemaining -= chunkSize;
    curIndex += chunkSize;
  } while (charsRemaining != 0);

  return NS_OK;
}

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
} // namespace net
} // namespace mozilla
