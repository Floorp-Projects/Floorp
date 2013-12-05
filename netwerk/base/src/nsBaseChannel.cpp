/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBaseChannel.h"
#include "nsURLHelper.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIHttpEventSink.h"
#include "nsIHttpChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIStreamConverterService.h"
#include "nsChannelClassifier.h"
#include "nsAsyncRedirectVerifyHelper.h"

static PLDHashOperator
CopyProperties(const nsAString &key, nsIVariant *data, void *closure)
{
  nsIWritablePropertyBag *bag =
      static_cast<nsIWritablePropertyBag *>(closure);

  bag->SetProperty(key, data);
  return PL_DHASH_NEXT;
}

// This class is used to suspend a request across a function scope.
class ScopedRequestSuspender {
public:
  ScopedRequestSuspender(nsIRequest *request)
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

// Used to suspend data events from mPump within a function scope.  This is
// usually needed when a function makes callbacks that could process events.
#define SUSPEND_PUMP_FOR_SCOPE() \
  ScopedRequestSuspender pump_suspender__(mPump)

//-----------------------------------------------------------------------------
// nsBaseChannel

nsBaseChannel::nsBaseChannel()
  : mLoadFlags(LOAD_NORMAL)
  , mQueriedProgressSink(true)
  , mSynthProgressEvents(false)
  , mWasOpened(false)
  , mWaitingOnAsyncRedirect(false)
  , mStatus(NS_OK)
  , mContentDispositionHint(UINT32_MAX)
  , mContentLength(-1)
{
  mContentType.AssignLiteral(UNKNOWN_CONTENT_TYPE);
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

  // Try to preserve the privacy bit if it has been overridden
  if (mPrivateBrowsingOverriden) {
    nsCOMPtr<nsIPrivateBrowsingChannel> newPBChannel =
      do_QueryInterface(newChannel);
    if (newPBChannel) {
      newPBChannel->SetPrivate(mPrivateBrowsing);
    }
  }

  nsCOMPtr<nsIWritablePropertyBag> bag = ::do_QueryInterface(newChannel);
  if (bag)
    mPropertyHash.EnumerateRead(CopyProperties, bag.get());

  // Notify consumer, giving chance to cancel redirect.  For backwards compat,
  // we support nsIHttpEventSink if we are an HTTP channel and if this is not
  // an internal redirect.

  nsRefPtr<nsAsyncRedirectVerifyHelper> redirectCallbackHelper =
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
    nsresult rv = mRedirectChannel->AsyncOpen(mListener, mListenerContext);
    if (NS_FAILED(rv))
      return rv;
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
  NS_ASSERTION(!IsPending(), "HasContentTypeHint called too late");
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
  nsCOMPtr<nsIInputStream> stream;
  nsCOMPtr<nsIChannel> channel;
  nsresult rv = OpenContentStream(true, getter_AddRefs(stream),
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

  // By assigning mPump, we flag this channel as pending (see IsPending).  It's
  // important that the pending flag is set when we call into the stream (the
  // call to AsyncRead results in the stream's AsyncWait method being called)
  // and especially when we call into the loadgroup.  Our caller takes care to
  // release mPump if we return an error.
 
  rv = nsInputStreamPump::Create(getter_AddRefs(mPump), stream, -1, -1, 0, 0,
                                 true);
  if (NS_SUCCEEDED(rv))
    rv = mPump->AsyncRead(this, nullptr);

  return rv;
}

void
nsBaseChannel::HandleAsyncRedirect(nsIChannel* newChannel)
{
  NS_ASSERTION(!mPump, "Shouldn't have gotten here");

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
  nsresult rv;

  if (mLoadFlags & LOAD_CLASSIFY_URI) {
    nsRefPtr<nsChannelClassifier> classifier = new nsChannelClassifier();
    if (classifier) {
      rv = classifier->Start(this);
      if (NS_FAILED(rv)) {
        Cancel(rv);
      }
    } else {
      Cancel(NS_ERROR_OUT_OF_MEMORY);
    }
  }
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsISupports

NS_IMPL_ISUPPORTS_INHERITED8(nsBaseChannel,
                             nsHashPropertyBag,
                             nsIRequest,
                             nsIChannel,
                             nsIInterfaceRequestor,
                             nsITransportEventSink,
                             nsIRequestObserver,
                             nsIStreamListener,
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
  *result = IsPending();
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetStatus(nsresult *status)
{
  if (mPump && NS_SUCCEEDED(mStatus)) {
    mPump->GetStatus(status);
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

  if (mPump)
    mPump->Cancel(status);

  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::Suspend()
{
  NS_ENSURE_TRUE(mPump, NS_ERROR_NOT_INITIALIZED);
  return mPump->Suspend();
}

NS_IMETHODIMP
nsBaseChannel::Resume()
{
  NS_ENSURE_TRUE(mPump, NS_ERROR_NOT_INITIALIZED);
  return mPump->Resume();
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
  NS_ENSURE_TRUE(!mPump, NS_ERROR_IN_PROGRESS);
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
nsBaseChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
  NS_ENSURE_TRUE(mURI, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(!mPump, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);
  NS_ENSURE_ARG(listener);

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

//-----------------------------------------------------------------------------
// nsBaseChannel::nsITransportEventSink

NS_IMETHODIMP
nsBaseChannel::OnTransportStatus(nsITransport *transport, nsresult status,
                                 uint64_t progress, uint64_t progressMax)
{
  // In some cases, we may wish to suppress transport-layer status events.

  if (!mPump || NS_FAILED(mStatus) || HasLoadFlag(LOAD_BACKGROUND))
    return NS_OK;

  SUSPEND_PUMP_FOR_SCOPE();

  // Lazily fetch mProgressSink
  if (!mProgressSink) {
    if (mQueriedProgressSink)
      return NS_OK;
    GetCallback(mProgressSink);
    mQueriedProgressSink = true;
    if (!mProgressSink)
      return NS_OK;
  }

  nsAutoString statusArg;
  if (GetStatusArg(status, statusArg))
    mProgressSink->OnStatus(this, mListenerContext, status, statusArg.get());

  if (progress)
    mProgressSink->OnProgress(this, mListenerContext, progress, progressMax);

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
  // If our content type is unknown or if the content type is
  // application/octet-stream and the caller requested it, use the content type
  // sniffer. If the sniffer is not available for some reason, then we just keep
  // going as-is.
  bool shouldSniff = mContentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE) ||
            ((mLoadFlags & LOAD_TREAT_APPLICATION_OCTET_STREAM_AS_UNKNOWN) &&
            mContentType.EqualsLiteral(APPLICATION_OCTET_STREAM));

  if (NS_SUCCEEDED(mStatus) && shouldSniff) {
    mPump->PeekStream(CallUnknownTypeSniffer, static_cast<nsIChannel*>(this));
  }

  // Now, the general type sniffers. Skip this if we have none.
  if (mLoadFlags & LOAD_CALL_CONTENT_SNIFFERS)
    mPump->PeekStream(CallTypeSniffers, static_cast<nsIChannel*>(this));

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

  // Cause IsPending to return false.
  mPump = nullptr;

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
    uint64_t prog = offset + count;
    OnTransportStatus(nullptr, NS_NET_STATUS_READING, prog, mContentLength);
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
