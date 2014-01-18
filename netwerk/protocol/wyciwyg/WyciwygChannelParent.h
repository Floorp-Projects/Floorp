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
namespace dom {
  class PBrowserParent;
}

namespace net {

class WyciwygChannelParent : public PWyciwygChannelParent
                           , public nsIStreamListener
                           , public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR

  WyciwygChannelParent();
  virtual ~WyciwygChannelParent();

protected:
  virtual bool RecvInit(const URIParams& uri) MOZ_OVERRIDE;
  virtual bool RecvAsyncOpen(const URIParams& original,
                             const uint32_t& loadFlags,
                             const IPC::SerializedLoadContext& loadContext,
                             PBrowserParent* parent) MOZ_OVERRIDE;
  virtual bool RecvWriteToCacheEntry(const nsString& data) MOZ_OVERRIDE;
  virtual bool RecvCloseCacheEntry(const nsresult& reason) MOZ_OVERRIDE;
  virtual bool RecvSetCharsetAndSource(const int32_t& source,
                                       const nsCString& charset) MOZ_OVERRIDE;
  virtual bool RecvSetSecurityInfo(const nsCString& securityInfo) MOZ_OVERRIDE;
  virtual bool RecvCancel(const nsresult& statusCode) MOZ_OVERRIDE;
  virtual bool RecvAppData(const IPC::SerializedLoadContext& loadContext,
                           PBrowserParent* parent) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  bool SetupAppData(const IPC::SerializedLoadContext& loadContext,
                    PBrowserParent* aParent);

  nsCOMPtr<nsIWyciwygChannel> mChannel;
  bool mIPCClosed;
  bool mReceivedAppData;
  nsCOMPtr<nsILoadContext> mLoadContext;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_WyciwygChannelParent_h
