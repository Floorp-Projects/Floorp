/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WyciwygChannelParent_h
#define mozilla_net_WyciwygChannelParent_h

#include "mozilla/net/PWyciwygChannelParent.h"
#include "mozilla/net/NeckoCommon.h"
#include "nsIStreamListener.h"

#include "nsIWyciwygChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"

namespace mozilla {
namespace net {

class WyciwygChannelParent : public PWyciwygChannelParent
                           , public nsIStreamListener
                           , public nsIInterfaceRequestor
                           , public nsILoadContext
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSILOADCONTEXT

  WyciwygChannelParent();
  virtual ~WyciwygChannelParent();

protected:
  virtual bool RecvInit(const IPC::URI& uri);
  virtual bool RecvAsyncOpen(const IPC::URI& original,
                             const PRUint32& loadFlags,
                             const bool& haveLoadContext,
                             const bool& isContent,
                             const bool& usingPrivateBrowsing,
                             const bool& isInBrowserElement,
                             const PRUint32& appId,
                             const nsCString& extendedOrigin);
  virtual bool RecvWriteToCacheEntry(const nsString& data);
  virtual bool RecvCloseCacheEntry(const nsresult& reason);
  virtual bool RecvSetCharsetAndSource(const PRInt32& source,
                                       const nsCString& charset);
  virtual bool RecvSetSecurityInfo(const nsCString& securityInfo);
  virtual bool RecvCancel(const nsresult& statusCode);

  virtual void ActorDestroy(ActorDestroyReason why);

  nsCOMPtr<nsIWyciwygChannel> mChannel;
  bool mIPCClosed;

  // fields for impersonating nsILoadContext
  bool mHaveLoadContext             : 1;
  bool mIsContent                   : 1;
  bool mUsePrivateBrowsing          : 1;
  bool mIsInBrowserElement          : 1;

  PRUint32 mAppId;
  nsCString mExtendedOrigin;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_WyciwygChannelParent_h
