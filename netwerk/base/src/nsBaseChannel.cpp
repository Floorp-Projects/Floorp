/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsBaseChannel.h"
#include "nsChannelProperties.h"
#include "nsURLHelper.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIOService.h"
#include "nsIHttpEventSink.h"
#include "nsIHttpChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIStreamConverterService.h"
#include "nsIContentSniffer.h"

// Determine if this URI is using a safe port.
static nsresult
CheckPort(nsIURI *uri) {
  PRInt32 port;
  nsresult rv = uri->GetPort(&port);
  if (NS_FAILED(rv) || port == -1)  // port undefined or default-valued
    return NS_OK;
  nsCAutoString scheme;
  uri->GetScheme(scheme);
  return NS_CheckPortSafety(port, scheme.get());
}

PR_STATIC_CALLBACK(PLDHashOperator)
CopyProperties(const nsAString &key, nsIVariant *data, void *closure)
{
  nsIWritablePropertyBag *bag =
      NS_STATIC_CAST(nsIWritablePropertyBag *, closure);

  bag->SetProperty(key, data);
  return PL_DHASH_NEXT;
}

// This class is used to suspend a request across a function scope.
class ScopedRequestSuspender {
public:
  ScopedRequestSuspender(nsIRequest *request)
    : mRequest(request) {
    mRequest->Suspend();
  }
  ~ScopedRequestSuspender() {
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
  , mStatus(NS_OK)
  , mQueriedProgressSink(PR_TRUE)
  , mSynthProgressEvents(PR_FALSE)
{
  mContentType.AssignLiteral(UNKNOWN_CONTENT_TYPE);
}

nsresult
nsBaseChannel::Redirect(nsIChannel *newChannel, PRUint32 redirectFlags)
{
  SUSPEND_PUMP_FOR_SCOPE();

  // Transfer properties

  newChannel->SetOriginalURI(OriginalURI());
  newChannel->SetLoadGroup(mLoadGroup);
  newChannel->SetNotificationCallbacks(mCallbacks);
  newChannel->SetLoadFlags(mLoadFlags | LOAD_REPLACE);

  nsCOMPtr<nsIWritablePropertyBag> bag = ::do_QueryInterface(newChannel);
  if (bag)
    mPropertyHash.EnumerateRead(CopyProperties, bag.get());

  // Notify consumer, giving chance to cancel redirect.  For backwards compat,
  // we support nsIHttpEventSink if we are an HTTP channel and if this is not
  // an internal redirect.

  // Global observers. These come first so that other observers don't see
  // redirects that get aborted for security reasons anyway.
  NS_ASSERTION(gIOService, "Must have an IO service");
  nsresult rv = gIOService->OnChannelRedirect(this, newChannel, redirectFlags);
  if (NS_FAILED(rv))
    return rv;

  // Backwards compat for non-internal redirects from a HTTP channel.
  if (!(redirectFlags & nsIChannelEventSink::REDIRECT_INTERNAL)) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface();
    if (httpChannel) {
      nsCOMPtr<nsIHttpEventSink> httpEventSink;
      GetCallback(httpEventSink);
      if (httpEventSink) {
        rv = httpEventSink->OnRedirect(httpChannel, newChannel);
        if (NS_FAILED(rv))
          return rv;
      }
    }
  }

  nsCOMPtr<nsIChannelEventSink> channelEventSink;
  // Give our consumer a chance to observe/block this redirect.
  GetCallback(channelEventSink);
  if (channelEventSink) {
    rv = channelEventSink->OnChannelRedirect(this, newChannel, redirectFlags);
    if (NS_FAILED(rv))
      return rv;
  }

  // If we fail to open the new channel, then we want to leave this channel
  // unaffected, so we defer tearing down our channel until we have succeeded
  // with the redirect.

  rv = newChannel->AsyncOpen(mListener, mListenerContext);
  if (NS_FAILED(rv))
    return rv;

  // close down this channel
  Cancel(NS_BINDING_REDIRECTED);
  mListener = nsnull;
  mListenerContext = nsnull;

  return NS_OK;
}

PRBool
nsBaseChannel::HasContentTypeHint() const
{
  NS_ASSERTION(!IsPending(), "HasContentTypeHint called too late");
  return !mContentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE);
}

void
nsBaseChannel::SetContentLength64(PRInt64 len)
{
  // XXX: Storing the content-length as a property may not be what we want.
  //      It has the drawback of being copied if we redirect this channel.
  //      Maybe it is time for nsIChannel2.
  SetPropertyAsInt64(NS_CHANNEL_PROP_CONTENT_LENGTH, len);
}

PRInt64
nsBaseChannel::ContentLength64()
{
  PRInt64 len;
  nsresult rv = GetPropertyAsInt64(NS_CHANNEL_PROP_CONTENT_LENGTH, &len);
  return NS_SUCCEEDED(rv) ? len : -1;
}

nsresult
nsBaseChannel::PushStreamConverter(const char *fromType,
                                   const char *toType,
                                   PRBool invalidatesContentLength,
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
      SetContentLength64(-1);
    if (result) {
      *result = nsnull;
      converter.swap(*result);
    }
  }
  return rv;
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsISupports

NS_IMPL_ISUPPORTS_INHERITED6(nsBaseChannel,
                             nsHashPropertyBag,
                             nsIRequest,
                             nsIChannel,
                             nsIInterfaceRequestor,
                             nsITransportEventSink,
                             nsIRequestObserver,
                             nsIStreamListener)

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
nsBaseChannel::IsPending(PRBool *result)
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
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetOriginalURI(nsIURI *aURI)
{
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
  PRBool dummy;
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
nsBaseChannel::GetContentLength(PRInt32 *aContentLength)
{
  PRInt64 len = ContentLength64();
  if (len > PR_INT32_MAX || len < 0)
    *aContentLength = -1;
  else
    *aContentLength = (PRInt32) len;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetContentLength(PRInt32 aContentLength)
{
  SetContentLength64(aContentLength);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::Open(nsIInputStream **result)
{
  NS_ENSURE_TRUE(mURI, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(!mPump, NS_ERROR_IN_PROGRESS);

  nsresult rv = OpenContentStream(PR_FALSE, result);
  if (rv == NS_ERROR_NOT_IMPLEMENTED)
    return NS_ImplementChannelOpen(this, result);

  return rv;
}

NS_IMETHODIMP
nsBaseChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
  NS_ENSURE_TRUE(mURI, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(!mPump, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_ARG(listener);

  // Ensure that this is an allowed port before proceeding.
  nsresult rv = CheckPort(mURI);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIInputStream> stream;
  rv = OpenContentStream(PR_TRUE, getter_AddRefs(stream));
  if (NS_FAILED(rv))
    return rv;

  mPump = new nsInputStreamPump();
  if (!mPump)
    return NS_ERROR_OUT_OF_MEMORY;
  rv = mPump->Init(stream, -1, -1, 0, 0, PR_TRUE);
  if (NS_FAILED(rv)) {
    mPump = nsnull;
    return rv;
  }

  rv = mPump->AsyncRead(this, nsnull);
  if (NS_FAILED(rv)) {
    mPump = nsnull;
    return rv;
  }

  mListener = listener;
  mListenerContext = ctxt;

  SUSPEND_PUMP_FOR_SCOPE();

  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nsnull);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsITransportEventSink

NS_IMETHODIMP
nsBaseChannel::OnTransportStatus(nsITransport *transport, nsresult status,
                                 PRUint64 progress, PRUint64 progressMax)
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
    mQueriedProgressSink = PR_TRUE;
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
CallTypeSniffers(void *aClosure, const PRUint8 *aData, PRUint32 aCount)
{
  nsIChannel *chan = NS_STATIC_CAST(nsIChannel*, aClosure);

  nsCOMPtr<nsIContentSniffer> sniffer =
    do_CreateInstance(NS_GENERIC_CONTENT_SNIFFER);
  if (!sniffer)
    return;

  nsCAutoString detected;
  nsresult rv = sniffer->GetMIMETypeFromContent(chan, aData, aCount, detected);
  if (NS_SUCCEEDED(rv))
    chan->SetContentType(detected);
}

NS_IMETHODIMP
nsBaseChannel::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  // If our content type is unknown, then use the content type sniffer.  If the
  // sniffer is not available for some reason, then we just keep going as-is.
  if (NS_SUCCEEDED(mStatus) && mContentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE)) {
    mPump->PeekStream(CallTypeSniffers, NS_STATIC_CAST(nsIChannel*, this));
  }

  return mListener->OnStartRequest(this, mListenerContext);
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
  mPump = nsnull;

  mListener->OnStopRequest(this, mListenerContext, mStatus);
  mListener = nsnull;
  mListenerContext = nsnull;

  // No need to suspend pump in this scope since we will not be receiving
  // any more events from it.

  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nsnull, mStatus);

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nsnull;
  CallbacksChanged();

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIStreamListener

NS_IMETHODIMP
nsBaseChannel::OnDataAvailable(nsIRequest *request, nsISupports *ctxt,
                               nsIInputStream *stream, PRUint32 offset,
                               PRUint32 count)
{
  nsresult rv = mListener->OnDataAvailable(this, mListenerContext, stream,
                                           offset, count);
  if (mSynthProgressEvents && NS_SUCCEEDED(rv)) {
    PRUint64 prog = PRUint64(offset) + count;
    PRUint64 progMax = ContentLength64();
    OnTransportStatus(nsnull, nsITransport::STATUS_READING, prog, progMax);
  }

  return rv;
}
