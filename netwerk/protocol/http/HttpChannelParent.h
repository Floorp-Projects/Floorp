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

#ifndef mozilla_net_HttpChannelParent_h
#define mozilla_net_HttpChannelParent_h

#include "nsHttp.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/net/PHttpChannelParent.h"
#include "mozilla/net/NeckoCommon.h"
#include "nsIProgressEventSink.h"
#include "nsITabParent.h"

using namespace mozilla::dom;

class nsICacheEntryDescriptor;

namespace mozilla {
namespace net {

class HttpChannelParentListener;

class HttpChannelParent : public PHttpChannelParent
                        , public nsIProgressEventSink
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROGRESSEVENTSINK

  // Make these non-virtual for a little performance benefit
  nsresult OnStartRequest(nsIRequest *aRequest, 
                          nsISupports *aContext);
  nsresult OnStopRequest(nsIRequest *aRequest, 
                         nsISupports *aContext, 
                         nsresult aStatusCode);
  nsresult OnDataAvailable(nsIRequest *aRequest, 
                           nsISupports *aContext, 
                           nsIInputStream *aInputStream, 
                           PRUint32 aOffset, 
                           PRUint32 aCount);
  
  HttpChannelParent(PBrowserParent* iframeEmbedding);
  virtual ~HttpChannelParent();

protected:
  virtual bool RecvAsyncOpen(const IPC::URI&            uri,
                             const IPC::URI&            originalUri,
                             const IPC::URI&            docUri,
                             const IPC::URI&            referrerUri,
                             const PRUint32&            loadFlags,
                             const RequestHeaderTuples& requestHeaders,
                             const nsHttpAtom&          requestMethod,
                             const nsCString&           uploadStreamData,
                             const PRInt32&             uploadStreamInfo,
                             const PRUint16&            priority,
                             const PRUint8&             redirectionLimit,
                             const PRBool&              allowPipelining,
                             const PRBool&              forceAllowThirdPartyCookie,
                             const bool&                doResumeAt,
                             const PRUint64&            startPos,
                             const nsCString&           entityID,
                             const bool&                chooseApplicationCache,
                             const nsCString&           appCacheClientID);

  virtual bool RecvSetPriority(const PRUint16& priority);
  virtual bool RecvSetCacheTokenCachedCharset(const nsCString& charset);
  virtual bool RecvSuspend();
  virtual bool RecvResume();
  virtual bool RecvCancel(const nsresult& status);
  virtual bool RecvRedirect2Result(const nsresult& result,
                                   const RequestHeaderTuples& changedHeaders);
  virtual bool RecvUpdateAssociatedContentSecurity(const PRInt32& high,
                                                   const PRInt32& low,
                                                   const PRInt32& broken,
                                                   const PRInt32& no);
  virtual bool RecvDocumentChannelCleanup();
  virtual bool RecvMarkOfflineCacheEntryAsForeign();

  virtual void ActorDestroy(ActorDestroyReason why);

protected:
  friend class mozilla::net::HttpChannelParentListener;
  nsCOMPtr<nsITabParent> mTabParent;

private:
  nsCOMPtr<nsIChannel> mChannel;
  nsRefPtr<HttpChannelParentListener> mChannelListener;
  nsCOMPtr<nsICacheEntryDescriptor> mCacheDescriptor;
  bool mIPCClosed;                // PHttpChannel actor has been Closed()
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_HttpChannelParent_h
