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
} // namespace dom

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

protected:
  virtual ~WyciwygChannelParent();

  virtual bool RecvInit(const URIParams&          uri,
                        const ipc::PrincipalInfo& aRequestingPrincipalInfo,
                        const ipc::PrincipalInfo& aTriggeringPrincipalInfo,
                        const ipc::PrincipalInfo& aPrincipalToInheritInfo,
                        const uint32_t&           aSecurityFlags,
                        const uint32_t&           aContentPolicyType) override;
  virtual bool RecvAsyncOpen(const URIParams& original,
                             const uint32_t& loadFlags,
                             const IPC::SerializedLoadContext& loadContext,
                             const PBrowserOrId &parent) override;
  virtual bool RecvWriteToCacheEntry(const nsString& data) override;
  virtual bool RecvCloseCacheEntry(const nsresult& reason) override;
  virtual bool RecvSetCharsetAndSource(const int32_t& source,
                                       const nsCString& charset) override;
  virtual bool RecvSetSecurityInfo(const nsCString& securityInfo) override;
  virtual bool RecvCancel(const nsresult& statusCode) override;
  virtual bool RecvAppData(const IPC::SerializedLoadContext& loadContext,
                           const PBrowserOrId &parent) override;

  virtual void ActorDestroy(ActorDestroyReason why) override;

  bool SetupAppData(const IPC::SerializedLoadContext& loadContext,
                    const PBrowserOrId &aParent);

  nsCOMPtr<nsIWyciwygChannel> mChannel;
  bool mIPCClosed;
  bool mReceivedAppData;
  nsCOMPtr<nsILoadContext> mLoadContext;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_WyciwygChannelParent_h
