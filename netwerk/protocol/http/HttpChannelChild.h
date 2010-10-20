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
 *   Honza Bambas <honzab@firemni.cz>
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

#ifndef mozilla_net_HttpChannelChild_h
#define mozilla_net_HttpChannelChild_h

#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/net/PHttpChannelChild.h"

#include "nsIStreamListener.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsICacheInfoChannel.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIUploadChannel2.h"
#include "nsIResumableChannel.h"
#include "nsIProxiedChannel.h"
#include "nsITraceableChannel.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIAssociatedContentSecurity.h"

namespace mozilla {
namespace net {

class ChildChannelEvent;

class HttpChannelChild : public PHttpChannelChild
                       , public HttpBaseChannel
                       , public nsICacheInfoChannel
                       , public nsIProxiedChannel
                       , public nsITraceableChannel
                       , public nsIApplicationCacheChannel
                       , public nsIAsyncVerifyRedirectCallback
                       , public nsIAssociatedContentSecurity
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICACHEINFOCHANNEL
  NS_DECL_NSIPROXIEDCHANNEL
  NS_DECL_NSITRACEABLECHANNEL
  NS_DECL_NSIAPPLICATIONCACHECONTAINER
  NS_DECL_NSIAPPLICATIONCACHECHANNEL
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK
  NS_DECL_NSIASSOCIATEDCONTENTSECURITY

  HttpChannelChild();
  virtual ~HttpChannelChild();

  // Methods HttpBaseChannel didn't implement for us or that we override.
  //
  // nsIRequest
  NS_IMETHOD Cancel(nsresult status);
  NS_IMETHOD Suspend();
  NS_IMETHOD Resume();
  // nsIChannel
  NS_IMETHOD GetSecurityInfo(nsISupports **aSecurityInfo);
  NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *aContext);
  // HttpBaseChannel::nsIHttpChannel
  NS_IMETHOD SetRequestHeader(const nsACString& aHeader, 
                              const nsACString& aValue, 
                              PRBool aMerge);
  // nsIHttpChannelInternal
  NS_IMETHOD SetupFallbackChannel(const char *aFallbackKey);
  // nsISupportsPriority
  NS_IMETHOD SetPriority(PRInt32 value);
  // nsIResumableChannel
  NS_IMETHOD ResumeAt(PRUint64 startPos, const nsACString& entityID);

  // Final setup when redirect has proceeded successfully in chrome
  nsresult CompleteRedirectSetup(nsIStreamListener *listener, 
                                 nsISupports *aContext);

  // IPDL holds a reference while the PHttpChannel protocol is live (starting at
  // AsyncOpen, and ending at either OnStopRequest or any IPDL error, either of
  // which call NeckoChild::DeallocPHttpChannel()).
  void AddIPDLReference();
  void ReleaseIPDLReference();

protected:
  bool RecvOnStartRequest(const nsHttpResponseHead& responseHead,
                          const PRBool& useResponseHead,
                          const RequestHeaderTuples& requestHeaders,
                          const PRBool& isFromCache,
                          const PRBool& cacheEntryAvailable,
                          const PRUint32& cacheExpirationTime,
                          const nsCString& cachedCharset,
                          const nsCString& securityInfoSerialization);
  bool RecvOnDataAvailable(const nsCString& data, 
                           const PRUint32& offset,
                           const PRUint32& count);
  bool RecvOnStopRequest(const nsresult& statusCode);
  bool RecvOnProgress(const PRUint64& progress, const PRUint64& progressMax);
  bool RecvOnStatus(const nsresult& status, const nsString& statusArg);
  bool RecvCancelEarly(const nsresult& status);
  bool RecvRedirect1Begin(PHttpChannelChild* newChannel,
                          const URI& newURI,
                          const PRUint32& redirectFlags,
                          const nsHttpResponseHead& responseHead);
  bool RecvRedirect3Complete();
  bool RecvAssociateApplicationCache(const nsCString& groupID,
                                     const nsCString& clientID);

  bool GetAssociatedContentSecurity(nsIAssociatedContentSecurity** res = nsnull);

private:
  RequestHeaderTuples mRequestHeaders;
  nsRefPtr<HttpChannelChild> mRedirectChannelChild;
  nsCOMPtr<nsIURI> mRedirectOriginalURI;
  nsCOMPtr<nsISupports> mSecurityInfo;

  PRPackedBool mIsFromCache;
  PRPackedBool mCacheEntryAvailable;
  PRUint32     mCacheExpirationTime;
  nsCString    mCachedCharset;

  // If ResumeAt is called before AsyncOpen, we need to send extra data upstream
  bool mSendResumeAt;
  // Current suspension depth for this channel object
  PRUint32 mSuspendCount;

  bool mIPCOpen;
  bool mKeptAlive;

  // Workaround for Necko re-entrancy dangers. We buffer IPDL messages in a
  // queue if still dispatching previous one(s) to listeners/observers.
  // Otherwise synchronous XMLHttpRequests and/or other code that spins the
  // event loop (ex: IPDL rpc) could cause listener->OnDataAvailable (for
  // instance) to be called before mListener->OnStartRequest has completed.
  void BeginEventQueueing();
  void EndEventQueueing();
  void FlushEventQueue();
  void EnqueueEvent(ChildChannelEvent* callback);
  bool ShouldEnqueue();

  nsTArray<nsAutoPtr<ChildChannelEvent> > mEventQueue;
  enum {
    PHASE_UNQUEUED,
    PHASE_QUEUEING,
    PHASE_FINISHED_QUEUEING,
    PHASE_FLUSHING
  } mQueuePhase;

  void OnStartRequest(const nsHttpResponseHead& responseHead,
                          const PRBool& useResponseHead,
                          const RequestHeaderTuples& requestHeaders,
                          const PRBool& isFromCache,
                          const PRBool& cacheEntryAvailable,
                          const PRUint32& cacheExpirationTime,
                          const nsCString& cachedCharset,
                          const nsCString& securityInfoSerialization);
  void OnDataAvailable(const nsCString& data, 
                       const PRUint32& offset,
                       const PRUint32& count);
  void OnStopRequest(const nsresult& statusCode);
  void OnProgress(const PRUint64& progress, const PRUint64& progressMax);
  void OnStatus(const nsresult& status, const nsString& statusArg);
  void OnCancel(const nsresult& status);
  void Redirect1Begin(PHttpChannelChild* newChannel, const URI& newURI,
                      const PRUint32& redirectFlags,
                      const nsHttpResponseHead& responseHead);
  void Redirect3Complete();

  friend class AutoEventEnqueuer;
  friend class StartRequestEvent;
  friend class StopRequestEvent;
  friend class DataAvailableEvent;
  friend class ProgressEvent;
  friend class StatusEvent;
  friend class CancelEvent;
  friend class Redirect1Event;
  friend class Redirect3Event;
};

//-----------------------------------------------------------------------------
// inline functions
//-----------------------------------------------------------------------------

inline void
HttpChannelChild::BeginEventQueueing()
{
  if (mQueuePhase != PHASE_UNQUEUED)
    return;
  // Store incoming IPDL messages for later.
  mQueuePhase = PHASE_QUEUEING;
}

inline void
HttpChannelChild::EndEventQueueing()
{
  if (mQueuePhase != PHASE_QUEUEING)
    return;

  mQueuePhase = PHASE_FINISHED_QUEUEING;
}


inline bool
HttpChannelChild::ShouldEnqueue()
{
  return mQueuePhase != PHASE_UNQUEUED || mSuspendCount;
}

inline void
HttpChannelChild::EnqueueEvent(ChildChannelEvent* callback)
{
  mEventQueue.AppendElement(callback);
}


} // namespace net
} // namespace mozilla

#endif // mozilla_net_HttpChannelChild_h
