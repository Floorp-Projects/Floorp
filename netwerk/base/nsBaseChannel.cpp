/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBaseChannel.h"
#include "nsContentUtils.h"
#include "nsURLHelper.h"
#include "nsNetCID.h"
#include "nsMimeTypes.h"
#include "nsIContentSniffer.h"
#include "nsIScriptSecurityManager.h"
#include "nsMimeTypes.h"
#include "nsIHttpEventSink.h"
#include "nsIHttpChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIStreamConverterService.h"
#include "nsChannelClassifier.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsProxyRelease.h"
#include "nsXULAppAPI.h"
#include "nsContentSecurityManager.h"
#include "LoadInfo.h"
#include "nsServiceManagerUtils.h"

// This class is used to suspend a request across a function scope.
class ScopedRequestSuspender {
public:
  explicit ScopedRequestSuspender(nsIRequest *request)
    : mRequest(request) {
    if (mRequest && NS_FAILED(mRequest->Suspend())) {
      NS_WARNING("Couldn't suspend pump");
      mRequest = nullptr;
    }
  }
  ~ScopedRequestSuspender() {
    if (mRequest)
      mRequest->Resume();
  }
private:
  nsIRequest *mRequest;
};

// Used to suspend data events from mRequest within a function scope.  This is
// usually needed when a function makes callbacks that could process events.
#define SUSPEND_PUMP_FOR_SCOPE() \
  ScopedRequestSuspender pump_suspender__(mRequest)

//-----------------------------------------------------------------------------
// nsBaseChannel

nsBaseChannel::nsBaseChannel()
  : mPumpingData(false)
  , mLoadFlags(LOAD_NORMAL)
  , mQueriedProgressSink(true)
  , mSynthProgressEvents(false)
  , mAllowThreadRetargeting(true)
  , mWaitingOnAsyncRedirect(false)
  , mOpenRedirectChannel(false)
  , mStatus(NS_OK)
  , mContentDispositionHint(UINT32_MAX)
  , mContentLength(-1)
  , mWasOpened(false)
{
  mContentType.AssignLiteral(UNKNOWN_CONTENT_TYPE);
}

nsBaseChannel::~nsBaseChannel()
{
  NS_ReleaseOnMainThread(mLoadInfo.forget());
}

nsresult
nsBaseChannel::Redirect(nsIChannel *newChannel, uint32_t redirectFlags,
                        bool openNewChannel)
{
  SUSPEND_PUMP_FOR_SCOPE();

  // Transfer properties

  newChannel->SetLoadGroup(mLoadGroup);
  newChannel->SetNotificationCallbacks(mCallbacks);
  newChannel->SetLoadFlags(mLoadFlags | LOAD_REPLACE);

  // make a copy of the loadinfo, append to the redirectchain
  // and set it on the new channel
  if (mLoadInfo) {
    nsSecurityFlags secFlags = mLoadInfo->GetSecurityFlags() &
                               ~nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
    nsCOMPtr<nsILoadInfo> newLoadInfo =
      static_cast<mozilla::LoadInfo*>(mLoadInfo.get())->CloneWithNewSecFlags(secFlags);

    nsCOMPtr<nsIPrincipal> uriPrincipal;
    nsIScriptSecurityManager *sm = nsContentUtils::GetSecurityManager();
    sm->GetChannelURIPrincipal(this, getter_AddRefs(uriPrincipal));
    bool isInternalRedirect =
      (redirectFlags & (nsIChannelEventSink::REDIRECT_INTERNAL |
                        nsIChannelEventSink::REDIRECT_STS_UPGRADE));
    newLoadInfo->AppendRedirectedPrincipal(uriPrincipal, isInternalRedirect);
    newChannel->SetLoadInfo(newLoadInfo);
  }
  else {
    // the newChannel was created with a dummy loadInfo, we should clear
    // it in case the original channel does not have a loadInfo
    newChannel->SetLoadInfo(nullptr);
  }

  // Preserve the privacy bit if it has been overridden
  if (mPrivateBrowsingOverriden) {
    nsCOMPtr<nsIPrivateBrowsingChannel> newPBChannel =
      do_QueryInterface(newChannel);
    if (newPBChannel) {
      newPBChannel->SetPrivate(mPrivateBrowsing);
    }
  }

  nsCOMPtr<nsIWritablePropertyBag> bag = ::do_QueryInterface(newChannel);
  if (bag) {
    for (auto iter = mPropertyHash.Iter(); !iter.Done(); iter.Next()) {
      bag->SetProperty(iter.Key(), iter.UserData());
    }
  }

  // Notify consumer, giving chance to cancel redirect.  For backwards compat,
  // we support nsIHttpEventSink if we are an HTTP channel and if this is not
  // an internal redirect.

  RefPtr<nsAsyncRedirectVerifyHelper> redirectCallbackHelper =
      new nsAsyncRedirectVerifyHelper();

  bool checkRedirectSynchronously = !openNewChannel;

  mRedirectChannel = newChannel;
  mRedirectFlags = redirectFlags;
  mOpenRedirectChannel = openNewChannel;
  nsresult rv = redirectCallbackHelper->Init(this, newChannel, redirectFlags,
                                             checkRedirectSynchronously);
  if (NS_FAILED(rv))
    return rv;

  if (checkRedirectSynchronously && NS_FAILED(mStatus))
    return mStatus;

  return NS_OK;
}

nsresult
nsBaseChannel::ContinueRedirect()
{
  // Backwards compat for non-internal redirects from a HTTP channel.
  // XXX Is our http channel implementation going to derive from nsBaseChannel?
  //     If not, this code can be removed.
  if (!(mRedirectFlags & nsIChannelEventSink::REDIRECT_INTERNAL)) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface();
    if (httpChannel) {
      nsCOMPtr<nsIHttpEventSink> httpEventSink;
      GetCallback(httpEventSink);
      if (httpEventSink) {
        nsresult rv = httpEventSink->OnRedirect(httpChannel, mRedirectChannel);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
    }
  }

  // Make sure to do this _after_ making all the OnChannelRedirect calls
  mRedirectChannel->SetOriginalURI(OriginalURI());

  // If we fail to open the new channel, then we want to leave this channel
  // unaffected, so we defer tearing down our channel until we have succeeded
  // with the redirect.

  if (mOpenRedirectChannel) {
    nsresult rv = NS_OK;
    if (mLoadInfo && mLoadInfo->GetEnforceSecurity()) {
      MOZ_ASSERT(!mListenerContext, "mListenerContext should be null!");
      rv = mRedirectChannel->AsyncOpen2(mListener);
    }
    else {
      rv = mRedirectChannel->AsyncOpen(mListener, mListenerContext);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mRedirectChannel = nullptr;

  // close down this channel
  Cancel(NS_BINDING_REDIRECTED);
  ChannelDone();

  return NS_OK;
}

bool
nsBaseChannel::HasContentTypeHint() const
{
  NS_ASSERTION(!Pending(), "HasContentTypeHint called too late");
  return !mContentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE);
}

nsresult
nsBaseChannel::PushStreamConverter(const char *fromType,
                                   const char *toType,
                                   bool invalidatesContentLength,
                                   nsIStreamListener **result)
{
  NS_ASSERTION(mListener, "no listener");

  nsresult rv;
  nsCOMPtr<nsIStreamConverterService> scs =
      do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIStreamListener> converter;
  rv = scs->AsyncConvertData(fromType, toType, mListener, mListenerContext,
                             getter_AddRefs(converter));
  if (NS_SUCCEEDED(rv)) {
    mListener = converter;
    if (invalidatesContentLength)
      mContentLength = -1;
    if (result) {
      *result = nullptr;
      converter.swap(*result);
    }
  }
  return rv;
}

nsresult
nsBaseChannel::BeginPumpingData()
{
  nsresult rv;

  rv = BeginAsyncRead(this, getter_AddRefs(mRequest));
  if (NS_SUCCEEDED(rv)) {
    mPumpingData = true;
    return NS_OK;
  }
  if (rv != NS_ERROR_NOT_IMPLEMENTED) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> stream;
  nsCOMPtr<nsIChannel> channel;
  rv = OpenContentStream(true, getter_AddRefs(stream),
                         getter_AddRefs(channel));
  if (NS_FAILED(rv))
    return rv;

  NS_ASSERTION(!stream || !channel, "Got both a channel and a stream?");

  if (channel) {
      rv = NS_DispatchToCurrentThread(new RedirectRunnable(this, channel));
      if (NS_SUCCEEDED(rv))
          mWaitingOnAsyncRedirect = true;
      return rv;
  }

  // By assigning mPump, we flag this channel as pending (see Pending).  It's
  // important that the pending flag is set when we call into the stream (the
  // call to AsyncRead results in the stream's AsyncWait method being called)
  // and especially when we call into the loadgroup.  Our caller takes care to
  // release mPump if we return an error.
 
  rv = nsInputStreamPump::Create(getter_AddRefs(mPump), stream, -1, -1, 0, 0,
                                 true);
  if (NS_SUCCEEDED(rv)) {
    mPumpingData = true;
    mRequest = mPump;
    rv = mPump->AsyncRead(this, nullptr);
  }

  return rv;
}

void
nsBaseChannel::HandleAsyncRedirect(nsIChannel* newChannel)
{
  NS_ASSERTION(!mPumpingData, "Shouldn't have gotten here");

  nsresult rv = mStatus;
  if (NS_SUCCEEDED(mStatus)) {
    rv = Redirect(newChannel,
                  nsIChannelEventSink::REDIRECT_TEMPORARY,
                  true);
    if (NS_SUCCEEDED(rv)) {
      // OnRedirectVerifyCallback will be called asynchronously
      return;
    }
  }

  ContinueHandleAsyncRedirect(rv);
}

void
nsBaseChannel::ContinueHandleAsyncRedirect(nsresult result)
{
  mWaitingOnAsyncRedirect = false;

  if (NS_FAILED(result))
    Cancel(result);

  if (NS_FAILED(result) && mListener) {
    // Notify our consumer ourselves
    mListener->OnStartRequest(this, mListenerContext);
    mListener->OnStopRequest(this, mListenerContext, mStatus);
    ChannelDone();
  }

  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nullptr, mStatus);

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nullptr;
  CallbacksChanged();
}

void
nsBaseChannel::ClassifyURI()
{
  // For channels created in the child process, delegate to the parent to
  // classify URIs.
  if (!XRE_IsParentProcess()) {
    return;
  }

  if (mLoadFlags & LOAD_CLASSIFY_URI) {
    RefPtr<nsChannelClassifier> classifier = new nsChannelClassifier(this);
    if (classifier) {
      classifier->Start();
    } else {
      Cancel(NS_ERROR_OUT_OF_MEMORY);
    }
  }
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsISupports

NS_IMPL_ISUPPORTS_INHERITED(nsBaseChannel,
                            nsHashPropertyBag,
                            nsIRequest,
                            nsIChannel,
                            nsIThreadRetargetableRequest,
                            nsIInterfaceRequestor,
                            nsITransportEventSink,
                            nsIRequestObserver,
                            nsIStreamListener,
                            nsIThreadRetargetableStreamListener,
                            nsIAsyncVerifyRedirectCallback,
                            nsIPrivateBrowsingChannel)

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIRequest

NS_IMETHODIMP
nsBaseChannel::GetName(nsACString &result)
{
  if (!mURI) {
    result.Truncate();
    return NS_OK;
  }
  return mURI->GetSpec(result);
}

NS_IMETHODIMP
nsBaseChannel::IsPending(bool *result)
{
  *result = Pending();
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetStatus(nsresult *status)
{
  if (mRequest && NS_SUCCEEDED(mStatus)) {
    mRequest->GetStatus(status);
  } else {
    *status = mStatus;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::Cancel(nsresult status)
{
  // Ignore redundant cancelation
  if (NS_FAILED(mStatus))
    return NS_OK;

  mStatus = status;

  if (mRequest)
    mRequest->Cancel(status);

  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::Suspend()
{
  NS_ENSURE_TRUE(mPumpingData, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mRequest, NS_ERROR_NOT_IMPLEMENTED);
  return mRequest->Suspend();
}

NS_IMETHODIMP
nsBaseChannel::Resume()
{
  NS_ENSURE_TRUE(mPumpingData, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mRequest, NS_ERROR_NOT_IMPLEMENTED);
  return mRequest->Resume();
}

NS_IMETHODIMP
nsBaseChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
  NS_IF_ADDREF(*aLoadGroup = mLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
  if (!CanSetLoadGroup(aLoadGroup)) {
    return NS_ERROR_FAILURE;
  }

  mLoadGroup = aLoadGroup;
  CallbacksChanged();
  UpdatePrivateBrowsing();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIChannel

NS_IMETHODIMP
nsBaseChannel::GetOriginalURI(nsIURI **aURI)
{
  *aURI = OriginalURI();
  NS_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetOriginalURI(nsIURI *aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  mOriginalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetURI(nsIURI **aURI)
{
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetOwner(nsISupports **aOwner)
{
  NS_IF_ADDREF(*aOwner = mOwner);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetOwner(nsISupports *aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetLoadInfo(nsILoadInfo* aLoadInfo)
{
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetLoadInfo(nsILoadInfo** aLoadInfo)
{
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetIsDocument(bool *aIsDocument)
{
  return NS_GetIsDocumentChannel(this, aIsDocument);
}

NS_IMETHODIMP
nsBaseChannel::GetNotificationCallbacks(nsIInterfaceRequestor **aCallbacks)
{
  NS_IF_ADDREF(*aCallbacks = mCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetNotificationCallbacks(nsIInterfaceRequestor *aCallbacks)
{
  if (!CanSetCallbacks(aCallbacks)) {
    return NS_ERROR_FAILURE;
  }

  mCallbacks = aCallbacks;
  CallbacksChanged();
  UpdatePrivateBrowsing();
  return NS_OK;
}

NS_IMETHODIMP 
nsBaseChannel::GetSecurityInfo(nsISupports **aSecurityInfo)
{
  NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetContentType(nsACString &aContentType)
{
  aContentType = mContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetContentType(const nsACString &aContentType)
{
  // mContentCharset is unchanged if not parsed
  bool dummy;
  net_ParseContentType(aContentType, mContentType, mContentCharset, &dummy);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetContentCharset(nsACString &aContentCharset)
{
  aContentCharset = mContentCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetContentCharset(const nsACString &aContentCharset)
{
  mContentCharset = aContentCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetContentDisposition(uint32_t *aContentDisposition)
{
  // preserve old behavior, fail unless explicitly set.
  if (mContentDispositionHint == UINT32_MAX) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aContentDisposition = mContentDispositionHint;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetContentDisposition(uint32_t aContentDisposition)
{
  mContentDispositionHint = aContentDisposition;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetContentDispositionFilename(nsAString &aContentDispositionFilename)
{
  if (!mContentDispositionFilename) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aContentDispositionFilename = *mContentDispositionFilename;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetContentDispositionFilename(const nsAString &aContentDispositionFilename)
{
  mContentDispositionFilename = new nsString(aContentDispositionFilename);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetContentDispositionHeader(nsACString &aContentDispositionHeader)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsBaseChannel::GetContentLength(int64_t *aContentLength)
{
  *aContentLength = mContentLength;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetContentLength(int64_t aContentLength)
{
  mContentLength = aContentLength;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::Open(nsIInputStream **result)
{
  NS_ENSURE_TRUE(mURI, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(!mPumpingData, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_IN_PROGRESS);

  nsCOMPtr<nsIChannel> chan;
  nsresult rv = OpenContentStream(false, result, getter_AddRefs(chan));
  NS_ASSERTION(!chan || !*result, "Got both a channel and a stream?");
  if (NS_SUCCEEDED(rv) && chan) {
      rv = Redirect(chan, nsIChannelEventSink::REDIRECT_INTERNAL, false);
      if (NS_FAILED(rv))
          return rv;
      rv = chan->Open(result);
  } else if (rv == NS_ERROR_NOT_IMPLEMENTED)
    return NS_ImplementChannelOpen(this, result);

  if (NS_SUCCEEDED(rv)) {
    mWasOpened = true;
    ClassifyURI();
  }

  return rv;
}

NS_IMETHODIMP
nsBaseChannel::Open2(nsIInputStream** aStream)
{
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return Open(aStream);
}

NS_IMETHODIMP
nsBaseChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
  MOZ_ASSERT(!mLoadInfo ||
             mLoadInfo->GetSecurityMode() == 0 ||
             mLoadInfo->GetInitialSecurityCheckDone() ||
             (mLoadInfo->GetSecurityMode() == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL &&
              nsContentUtils::IsSystemPrincipal(mLoadInfo->LoadingPrincipal())),
             "security flags in loadInfo but asyncOpen2() not called");

  NS_ENSURE_TRUE(mURI, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(!mPumpingData, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);
  NS_ENSURE_ARG(listener);

  // Skip checking for chrome:// sub-resources.
  nsAutoCString scheme;
  mURI->GetScheme(scheme);
  if (!scheme.EqualsLiteral("file")) {
    NS_CompareLoadInfoAndLoadContext(this);
  }

  // Ensure that this is an allowed port before proceeding.
  nsresult rv = NS_CheckPortSafety(mURI);
  if (NS_FAILED(rv)) {
    mCallbacks = nullptr;
    return rv;
  }

  // Store the listener and context early so that OpenContentStream and the
  // stream's AsyncWait method (called by AsyncRead) can have access to them
  // via PushStreamConverter and the StreamListener methods.  However, since
  // this typically introduces a reference cycle between this and the listener,
  // we need to be sure to break the reference if this method does not succeed.
  mListener = listener;
  mListenerContext = ctxt;

  // This method assigns mPump as a side-effect.  We need to clear mPump if
  // this method fails.
  rv = BeginPumpingData();
  if (NS_FAILED(rv)) {
    mPump = nullptr;
    mRequest = nullptr;
    mPumpingData = false;
    ChannelDone();
    mCallbacks = nullptr;
    return rv;
  }

  // At this point, we are going to return success no matter what.

  mWasOpened = true;

  SUSPEND_PUMP_FOR_SCOPE();

  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nullptr);

  ClassifyURI();

  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::AsyncOpen2(nsIStreamListener *aListener)
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
// nsBaseChannel::nsITransportEventSink

NS_IMETHODIMP
nsBaseChannel::OnTransportStatus(nsITransport *transport, nsresult status,
                                 int64_t progress, int64_t progressMax)
{
  // In some cases, we may wish to suppress transport-layer status events.

  if (!mPumpingData || NS_FAILED(mStatus)) {
    return NS_OK;
  }

  SUSPEND_PUMP_FOR_SCOPE();

  // Lazily fetch mProgressSink
  if (!mProgressSink) {
    if (mQueriedProgressSink) {
      return NS_OK;
    }
    GetCallback(mProgressSink);
    mQueriedProgressSink = true;
    if (!mProgressSink) {
      return NS_OK;
    }
  }

  if (!HasLoadFlag(LOAD_BACKGROUND)) {
    nsAutoString statusArg;
    if (GetStatusArg(status, statusArg)) {
      mProgressSink->OnStatus(this, mListenerContext, status, statusArg.get());
    }
  }

  if (progress) {
    mProgressSink->OnProgress(this, mListenerContext, progress, progressMax);
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIInterfaceRequestor

NS_IMETHODIMP
nsBaseChannel::GetInterface(const nsIID &iid, void **result)
{
  NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup, iid, result);
  return *result ? NS_OK : NS_ERROR_NO_INTERFACE; 
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIRequestObserver

static void
CallTypeSniffers(void *aClosure, const uint8_t *aData, uint32_t aCount)
{
  nsIChannel *chan = static_cast<nsIChannel*>(aClosure);

  nsAutoCString newType;
  NS_SniffContent(NS_CONTENT_SNIFFER_CATEGORY, chan, aData, aCount, newType);
  if (!newType.IsEmpty()) {
    chan->SetContentType(newType);
  }
}

static void
CallUnknownTypeSniffer(void *aClosure, const uint8_t *aData, uint32_t aCount)
{
  nsIChannel *chan = static_cast<nsIChannel*>(aClosure);

  nsCOMPtr<nsIContentSniffer> sniffer =
    do_CreateInstance(NS_GENERIC_CONTENT_SNIFFER);
  if (!sniffer)
    return;

  nsAutoCString detected;
  nsresult rv = sniffer->GetMIMETypeFromContent(chan, aData, aCount, detected);
  if (NS_SUCCEEDED(rv))
    chan->SetContentType(detected);
}

NS_IMETHODIMP
nsBaseChannel::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  MOZ_ASSERT_IF(mRequest, request == mRequest);

  if (mPump) {
    // If our content type is unknown, use the content type
    // sniffer. If the sniffer is not available for some reason, then we just keep
    // going as-is.
    if (NS_SUCCEEDED(mStatus) &&
        mContentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE)) {
      mPump->PeekStream(CallUnknownTypeSniffer, static_cast<nsIChannel*>(this));
    }

    // Now, the general type sniffers. Skip this if we have none.
    if (mLoadFlags & LOAD_CALL_CONTENT_SNIFFERS)
      mPump->PeekStream(CallTypeSniffers, static_cast<nsIChannel*>(this));
  }

  SUSPEND_PUMP_FOR_SCOPE();

  if (mListener) // null in case of redirect
      return mListener->OnStartRequest(this, mListenerContext);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                             nsresult status)
{
  // If both mStatus and status are failure codes, we keep mStatus as-is since
  // that is consistent with our GetStatus and Cancel methods.
  if (NS_SUCCEEDED(mStatus))
    mStatus = status;

  // Cause Pending to return false.
  mPump = nullptr;
  mRequest = nullptr;
  mPumpingData = false;

  if (mListener) // null in case of redirect
      mListener->OnStopRequest(this, mListenerContext, mStatus);
  ChannelDone();

  // No need to suspend pump in this scope since we will not be receiving
  // any more events from it.

  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nullptr, mStatus);

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nullptr;
  CallbacksChanged();

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIStreamListener

NS_IMETHODIMP
nsBaseChannel::OnDataAvailable(nsIRequest *request, nsISupports *ctxt,
                               nsIInputStream *stream, uint64_t offset,
                               uint32_t count)
{
  SUSPEND_PUMP_FOR_SCOPE();

  nsresult rv = mListener->OnDataAvailable(this, mListenerContext, stream,
                                           offset, count);
  if (mSynthProgressEvents && NS_SUCCEEDED(rv)) {
    int64_t prog = offset + count;
    if (NS_IsMainThread()) {
      OnTransportStatus(nullptr, NS_NET_STATUS_READING, prog, mContentLength);
    } else {
      class OnTransportStatusAsyncEvent : public mozilla::Runnable
      {
        RefPtr<nsBaseChannel> mChannel;
        int64_t mProgress;
        int64_t mContentLength;
      public:
        OnTransportStatusAsyncEvent(nsBaseChannel* aChannel,
                                    int64_t aProgress,
                                    int64_t aContentLength)
          : mChannel(aChannel),
            mProgress(aProgress),
            mContentLength(aContentLength)
        { }

        NS_IMETHOD Run() override
        {
          return mChannel->OnTransportStatus(nullptr, NS_NET_STATUS_READING,
                                             mProgress, mContentLength);
        }
      };

      nsCOMPtr<nsIRunnable> runnable =
        new OnTransportStatusAsyncEvent(this, prog, mContentLength);
      NS_DispatchToMainThread(runnable);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsBaseChannel::OnRedirectVerifyCallback(nsresult result)
{
  if (NS_SUCCEEDED(result))
    result = ContinueRedirect();

  if (NS_FAILED(result) && !mWaitingOnAsyncRedirect) {
    if (NS_SUCCEEDED(mStatus))
      mStatus = result;
    return NS_OK;
  }

  if (mWaitingOnAsyncRedirect)
    ContinueHandleAsyncRedirect(result);

  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::RetargetDeliveryTo(nsIEventTarget* aEventTarget)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE(mRequest, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIThreadRetargetableRequest> req;
  if (mAllowThreadRetargeting) {
    req = do_QueryInterface(mRequest);
  }

  NS_ENSURE_TRUE(req, NS_ERROR_NOT_IMPLEMENTED);

  return req->RetargetDeliveryTo(aEventTarget);
}

NS_IMETHODIMP
nsBaseChannel::CheckListenerChain()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mAllowThreadRetargeting) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsIThreadRetargetableStreamListener> listener =
    do_QueryInterface(mListener);
  if (!listener) {
    return NS_ERROR_NO_INTERFACE;
  }

  return listener->CheckListenerChain();
}
