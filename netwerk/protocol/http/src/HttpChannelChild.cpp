/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

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
 * The Initial Developer of the Original Code is
 *  The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Duell <jduell.mcbugs@gmail.com>
 *   Daniel Witte <dwitte@mozilla.com>
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

#include "nsHttp.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/HttpChannelChild.h"

#include "nsStringStream.h"
#include "nsHttpHandler.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

// C++ file contents
HttpChannelChild::HttpChannelChild()
  : mState(HCC_NEW)
{
  LOG(("Creating HttpChannelChild @%x\n", this));
}

HttpChannelChild::~HttpChannelChild()
{
  LOG(("Destroying HttpChannelChild @%x\n", this));
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF_INHERITED(HttpChannelChild, HttpBaseChannel)
NS_IMPL_RELEASE_INHERITED(HttpChannelChild, HttpBaseChannel)

NS_INTERFACE_MAP_BEGIN(HttpChannelChild)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsIHttpChannel)
  NS_INTERFACE_MAP_ENTRY(nsIHttpChannelInternal)
  NS_INTERFACE_MAP_ENTRY(nsICachingChannel)
  NS_INTERFACE_MAP_ENTRY(nsIUploadChannel)
  NS_INTERFACE_MAP_ENTRY(nsIUploadChannel2)
  NS_INTERFACE_MAP_ENTRY(nsIEncodedChannel)
  NS_INTERFACE_MAP_ENTRY(nsIResumableChannel)
  NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
  NS_INTERFACE_MAP_ENTRY(nsIProxiedChannel)
  NS_INTERFACE_MAP_ENTRY(nsITraceableChannel)
  NS_INTERFACE_MAP_ENTRY(nsIApplicationCacheContainer)
  NS_INTERFACE_MAP_ENTRY(nsIApplicationCacheChannel)
NS_INTERFACE_MAP_END_INHERITING(HttpBaseChannel)

//-----------------------------------------------------------------------------
// HttpChannelChild::PHttpChannelChild
//-----------------------------------------------------------------------------

bool 
HttpChannelChild::RecvOnStartRequest(const nsHttpResponseHead& responseHead)
{
  LOG(("HttpChannelChild::RecvOnStartRequest [this=%x]\n", this));

  mState = HCC_ONSTART;

  mResponseHead = new nsHttpResponseHead(responseHead);

  nsresult rv = mListener->OnStartRequest(this, mListenerContext);
  if (!NS_SUCCEEDED(rv)) {
    // TODO: Cancel request:
    //  - Send Cancel msg to parent 
    //  - drop any in flight OnDataAvail msgs we receive
    //  - make sure we do call OnStopRequest eventually
    //  - return true here, not false
    return false;  
  }
  return true;
}

bool 
HttpChannelChild::RecvOnDataAvailable(const nsCString& data,
                                      const PRUint32& offset,
                                      const PRUint32& count)
{
  LOG(("HttpChannelChild::RecvOnDataAvailable [this=%x]\n", this));

  mState = HCC_ONDATA;

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
  if (!NS_SUCCEEDED(rv)) {
    // TODO:  what to do here?  Cancel request?  Very unlikely to fail.
    return false;  
  }
  rv = mListener->OnDataAvailable(this, mListenerContext,
                                  stringStream, offset, count);
  stringStream->Close();
  if (!NS_SUCCEEDED(rv)) {
    // TODO: Cancel request: see notes in OnStartRequest
    return false; 
  }
  return true;
}

bool 
HttpChannelChild::RecvOnStopRequest(const nsresult& statusCode)
{
  LOG(("HttpChannelChild::RecvOnStopRequest [this=%x status=%u]\n", 
           this, statusCode));

  mState = HCC_ONSTOP;

  mIsPending = PR_FALSE;
  mStatus = statusCode;
  nsresult rv = mListener->OnStopRequest(this, mListenerContext, statusCode);
  mListener = 0;
  mListenerContext = 0;
  if (!NS_SUCCEEDED(rv)) {
    // TODO: Cancel request: see notes in OnStartRequest
    return false;  
  }
  return true;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::Cancel(nsresult status)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::Suspend()
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::Resume()
{
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIChannel
//-----------------------------------------------------------------------------
NS_IMETHODIMP
HttpChannelChild::GetOwner(nsISupports **aOwner)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::SetOwner(nsISupports *aOwner)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::GetSecurityInfo(nsISupports **aSecurityInfo)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::AsyncOpen(nsIStreamListener *listener, nsISupports *aContext)
{
  LOG(("HttpChannelChild::AsyncOpen [this=%x uri=%s]\n", this, mSpec.get()));

  NS_ENSURE_TRUE(gNeckoChild != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  // Port checked in parent, but duplicate here so we can return with error
  // immediately, as we've done since before e10s.
  nsresult rv;
  rv = NS_CheckPortSafety(mURI);
  if (NS_FAILED(rv))
    return rv;

  // This corresponds to Release() in DeallocPHttpChannel 
  this->AddRef();

  // TODO: Combine constructor and AsyncOpen to save one IPC msg
  if (!gNeckoChild->SendPHttpChannelConstructor(this)) {
    // TODO: currently means "this" has been deleted! bug 529693
    DROP_DEAD();
  }
  mListener = listener;
  mListenerContext = aContext;

  // TODO: serialize mConnectionInfo across to the parent, and set it on
  // the new channel somehow?

  // TODO: serialize mCaps across to the parent, and set it on
  // the new channel somehow?

  // TODO: need to dupe cookies logic from nsHttpChannel.cpp?

  // TODO: need to notify (child-side) http-on-modify-req observers 

  // TODO: add self to loadgroup? 

  // TODO: smartest way to pass nsURI == (spec, charset)? 
  nsCAutoString charset;
  mURI->GetOriginCharset(charset);
  nsCAutoString originalSpec;
  mOriginalURI->GetSpec(originalSpec);
  nsCAutoString originalCharset;
  mOriginalURI->GetOriginCharset(originalCharset);

  nsCAutoString docSpec;
  nsCAutoString docCharset;
  if (mDocumentURI) {
    mDocumentURI->GetSpec(docSpec);
    mDocumentURI->GetOriginCharset(docCharset);
  }

  if (!SendAsyncOpen(mSpec, charset, originalSpec, originalCharset, 
                     docSpec, docCharset, mLoadFlags, mRequestHeaders,
                     mRequestHead.Method(), mRedirectionLimit, mAllowPipelining,
                     mForceAllowThirdPartyCookie)) {
    // IPDL error: our destructor will be called automatically
    // -- TODO: verify that that's the case :)
    mListener = 0;
    mListenerContext = 0;
    return NS_ERROR_FAILURE;
  }

  mIsPending = PR_TRUE;
  mWasOpened = PR_TRUE;
  mState = HCC_OPENED;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIHttpChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::SetRequestHeader(const nsACString& aHeader, 
                                   const nsACString& aValue, 
                                   PRBool aMerge)
{
  nsresult rv = HttpBaseChannel::SetRequestHeader(aHeader, aValue, aMerge);
  if (NS_FAILED(rv))
    return rv;

  RequestHeaderTuple* tuple = mRequestHeaders.AppendElement();
  if (!tuple)
    return NS_ERROR_OUT_OF_MEMORY;

  tuple->mHeader = aHeader;
  tuple->mValue = aValue;
  tuple->mMerge = aMerge;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIHttpChannelInternal
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::SetupFallbackChannel(const char *aFallbackKey)
{
  DROP_DEAD();
}


//-----------------------------------------------------------------------------
// HttpChannelChild::nsICachingChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetCacheToken(nsISupports **aCacheToken)
{
  DROP_DEAD();
}
NS_IMETHODIMP
HttpChannelChild::SetCacheToken(nsISupports *aCacheToken)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::GetOfflineCacheToken(nsISupports **aOfflineCacheToken)
{
  DROP_DEAD();
}
NS_IMETHODIMP
HttpChannelChild::SetOfflineCacheToken(nsISupports *aOfflineCacheToken)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::GetCacheKey(nsISupports **aCacheKey)
{
  DROP_DEAD();
}
NS_IMETHODIMP
HttpChannelChild::SetCacheKey(nsISupports *aCacheKey)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::GetCacheAsFile(PRBool *aCacheAsFile)
{
  DROP_DEAD();
}
NS_IMETHODIMP
HttpChannelChild::SetCacheAsFile(PRBool aCacheAsFile)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::GetCacheForOfflineUse(PRBool *aCacheForOfflineUse)
{
  DROP_DEAD();
}
NS_IMETHODIMP
HttpChannelChild::SetCacheForOfflineUse(PRBool aCacheForOfflineUse)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::GetOfflineCacheClientID(nsACString& id)
{
  DROP_DEAD();
}
NS_IMETHODIMP
HttpChannelChild::SetOfflineCacheClientID(const nsACString& id)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::GetCacheFile(nsIFile **aCacheFile)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::IsFromCache(PRBool *retval)
{
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIUploadChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::SetUploadStream(nsIInputStream *aStream, 
                                  const nsACString& aContentType, 
                                  PRInt32 aContentLength)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::GetUploadStream(nsIInputStream **aUploadStream)
{
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIUploadChannel2
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::ExplicitSetUploadStream(nsIInputStream *aStream, 
                                          const nsACString& aContentType, 
                                          PRInt64 aContentLength, 
                                          const nsACString& aMethod, 
                                          PRBool aStreamHasHeaders)
{
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIEncodedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetContentEncodings(nsIUTF8StringEnumerator **result)
{
  DROP_DEAD();
}

/* attribute boolean applyConversion; */
NS_IMETHODIMP
HttpChannelChild::GetApplyConversion(PRBool *aApplyConversion)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::SetApplyConversion(PRBool aApplyConversion)
{
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIResumableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::ResumeAt(PRUint64 startPos, const nsACString& entityID)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::GetEntityID(nsACString& aEntityID)
{
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsISupportsPriority
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetPriority(PRInt32 *aPriority)
{
  DROP_DEAD();
}
NS_IMETHODIMP
HttpChannelChild::SetPriority(PRInt32 aPriority)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::AdjustPriority(PRInt32 delta)
{
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIProxiedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetProxyInfo(nsIProxyInfo **aProxyInfo)
{
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsITraceableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::SetNewListener(nsIStreamListener *listener, 
                                 nsIStreamListener **oldListener)
{
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIApplicationCacheContainer
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetApplicationCache(nsIApplicationCache **aApplicationCache)
{
  DROP_DEAD();
}
NS_IMETHODIMP
HttpChannelChild::SetApplicationCache(nsIApplicationCache *aApplicationCache)
{
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIApplicationCacheChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetLoadedFromApplicationCache(PRBool *retval)
{
  DROP_DEAD();
}

NS_IMETHODIMP
HttpChannelChild::GetInheritApplicationCache(PRBool *aInheritApplicationCache)
{
  DROP_DEAD();
}
NS_IMETHODIMP
HttpChannelChild::SetInheritApplicationCache(PRBool aInheritApplicationCache)
{
  // FIXME: Browser calls this early, so stub OK for now. Fix in bug 536295.  
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetChooseApplicationCache(PRBool *aChooseApplicationCache)
{
  DROP_DEAD();
}
NS_IMETHODIMP
HttpChannelChild::SetChooseApplicationCache(PRBool aChooseApplicationCache)
{
  // FIXME: Browser calls this early, so stub OK for now. Fix in bug 536295.  
  return NS_OK;
}


//------------------------------------------------------------------------------
}} // mozilla::net

