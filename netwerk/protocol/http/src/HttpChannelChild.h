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

#include "mozilla/net/PHttpChannelChild.h"
#include "mozilla/net/NeckoCommon.h"

#include "nsHttpRequestHead.h"
#include "nsHashPropertyBag.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsICachingChannel.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIEncodedChannel.h"
#include "nsIUploadChannel.h"
#include "nsIUploadChannel2.h"
#include "nsIResumableChannel.h"
#include "nsISupportsPriority.h"
#include "nsIProxiedChannel.h"
#include "nsITraceableChannel.h"


namespace mozilla {
namespace net {

// TODO: replace with IPDL states
enum HttpChannelChildState {
  HCC_NEW,
  HCC_OPENED,
  HCC_ONSTART,
  HCC_ONDATA,
  HCC_ONSTOP
};

// Header file contents
class HttpChannelChild : public PHttpChannelChild
                       , public nsIHttpChannel
                       , public nsHashPropertyBag
                       , public nsIHttpChannelInternal
                       , public nsICachingChannel
                       , public nsIUploadChannel
                       , public nsIUploadChannel2
                       , public nsIEncodedChannel
                       , public nsIResumableChannel
                       , public nsISupportsPriority
                       , public nsIProxiedChannel
                       , public nsITraceableChannel
                       , public nsIApplicationCacheChannel
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSIHTTPCHANNEL
  NS_DECL_NSIHTTPCHANNELINTERNAL
  NS_DECL_NSICACHINGCHANNEL
  NS_DECL_NSIUPLOADCHANNEL
  NS_DECL_NSIUPLOADCHANNEL2
  NS_DECL_NSIENCODEDCHANNEL
  NS_DECL_NSIRESUMABLECHANNEL
  NS_DECL_NSISUPPORTSPRIORITY
  NS_DECL_NSIPROXIEDCHANNEL
  NS_DECL_NSITRACEABLECHANNEL
  NS_DECL_NSIAPPLICATIONCACHECONTAINER
  NS_DECL_NSIAPPLICATIONCACHECHANNEL

  HttpChannelChild();
  virtual ~HttpChannelChild();

  nsresult Init(nsIURI *uri);

protected:
  bool RecvOnStartRequest(const PRInt32& HACK_ContentLength,
                          const nsCString& HACK_ContentType,
                          const PRUint32& HACK_Status,
                          const nsCString& HACK_StatusText);
  bool RecvOnDataAvailable(const nsCString& data, 
                           const PRUint32& offset,
                           const PRUint32& count);
  bool RecvOnStopRequest(const nsresult& statusCode);

private:
  nsCOMPtr<nsIStreamListener>         mChildListener;
  nsCOMPtr<nsISupports>               mChildListenerContext;

  // FIXME: copy full ResponseHead (bug 536283)
  PRInt32                             mContentLength_HACK;
  nsCString                           mContentType_HACK;
  PRUint32                            mResponseStatus_HACK;
  nsCString                           mResponseStatusText_HACK;

  // FIXME: replace with IPDL states (bug 536319) 
  enum HttpChannelChildState mState;

  /**
   * fields copied from nsHttpChannel.h
   */
  nsCOMPtr<nsIURI>                  mOriginalURI;
  nsCOMPtr<nsIURI>                  mURI;
  nsCOMPtr<nsIURI>                  mDocumentURI;

  nsCOMPtr<nsIInterfaceRequestor>   mCallbacks;
  nsCOMPtr<nsIProgressEventSink>    mProgressSink;

  nsHttpRequestHead                 mRequestHead;

  nsCString                         mSpec; // ASCII encoded URL spec

  PRUint32                          mLoadFlags;
  PRUint32                          mStatus;

  // state flags
  PRUint32                          mIsPending                : 1;
  PRUint32                          mWasOpened                : 1;

};


} // namespace net
} // namespace mozilla

#endif // mozilla_net_HttpChannelChild_h
