/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/PNeckoParent.h"
#include "mozilla/net/NeckoCommon.h"

#ifndef mozilla_net_NeckoParent_h
#define mozilla_net_NeckoParent_h

namespace mozilla {
namespace net {

// Used to override channel Private Browsing status if needed.
enum PBOverrideStatus {
  kPBOverride_Unset = 0,
  kPBOverride_Private,
  kPBOverride_NotPrivate
};

// Header file contents
class NeckoParent :
  public PNeckoParent
{
public:
  NeckoParent();
  virtual ~NeckoParent();

  MOZ_WARN_UNUSED_RESULT
  static const char *
  GetValidatedAppInfo(const SerializedLoadContext& aSerialized,
                      PContentParent* aBrowser,
                      uint32_t* aAppId,
                      bool* aInBrowserElement);

  /*
   * Creates LoadContext for parent-side of an e10s channel.
   *
   * PContentParent corresponds to the process that is requesting the load.
   *
   * Returns null if successful, or an error string if failed.
   */
  MOZ_WARN_UNUSED_RESULT
  static const char*
  CreateChannelLoadContext(const PBrowserOrId& aBrowser,
                           PContentParent* aContent,
                           const SerializedLoadContext& aSerialized,
                           nsCOMPtr<nsILoadContext> &aResult);

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual void
  CloneManagees(ProtocolBase* aSource,
              mozilla::ipc::ProtocolCloneContext* aCtx) MOZ_OVERRIDE;
  virtual PCookieServiceParent* AllocPCookieServiceParent() MOZ_OVERRIDE;
  virtual bool
  RecvPCookieServiceConstructor(PCookieServiceParent* aActor) MOZ_OVERRIDE
  {
    return PNeckoParent::RecvPCookieServiceConstructor(aActor);
  }

  /*
   * This implementation of nsIAuthPrompt2 is used for nested remote iframes that
   * want an auth prompt.  This class lives in the parent process and informs the
   * NeckoChild that we want an auth prompt, which forwards the request to the
   * TabParent in the remote iframe that contains the nested iframe
   */
  class NestedFrameAuthPrompt MOZ_FINAL : public nsIAuthPrompt2
  {
    ~NestedFrameAuthPrompt() {}

  public:
    NS_DECL_ISUPPORTS

    NestedFrameAuthPrompt(PNeckoParent* aParent, uint64_t aNestedFrameId);

    NS_IMETHOD PromptAuth(nsIChannel*, uint32_t, nsIAuthInformation*, bool*)
    {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD AsyncPromptAuth(nsIChannel* aChannel, nsIAuthPromptCallback* callback,
                               nsISupports*, uint32_t,
                               nsIAuthInformation* aInfo, nsICancelable**);

    NS_IMETHOD AsyncPromptAuth2(nsIChannel*, nsIDOMElement*,
                                nsIAuthPromptCallback*, nsISupports*,
                                uint32_t, nsIAuthInformation*, nsICancelable**)
    {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

  protected:
    PNeckoParent* mNeckoParent;
    uint64_t mNestedFrameId;
  };

protected:
  virtual PHttpChannelParent*
    AllocPHttpChannelParent(const PBrowserOrId&, const SerializedLoadContext&,
                            const HttpChannelCreationArgs& aOpenArgs) MOZ_OVERRIDE;
  virtual bool
    RecvPHttpChannelConstructor(
                      PHttpChannelParent* aActor,
                      const PBrowserOrId& aBrowser,
                      const SerializedLoadContext& aSerialized,
                      const HttpChannelCreationArgs& aOpenArgs) MOZ_OVERRIDE;
  virtual bool DeallocPHttpChannelParent(PHttpChannelParent*) MOZ_OVERRIDE;
  virtual bool DeallocPCookieServiceParent(PCookieServiceParent*) MOZ_OVERRIDE;
  virtual PWyciwygChannelParent* AllocPWyciwygChannelParent() MOZ_OVERRIDE;
  virtual bool DeallocPWyciwygChannelParent(PWyciwygChannelParent*) MOZ_OVERRIDE;
  virtual PFTPChannelParent*
    AllocPFTPChannelParent(const PBrowserOrId& aBrowser,
                           const SerializedLoadContext& aSerialized,
                           const FTPChannelCreationArgs& aOpenArgs) MOZ_OVERRIDE;
  virtual bool
    RecvPFTPChannelConstructor(
                      PFTPChannelParent* aActor,
                      const PBrowserOrId& aBrowser,
                      const SerializedLoadContext& aSerialized,
                      const FTPChannelCreationArgs& aOpenArgs) MOZ_OVERRIDE;
  virtual bool DeallocPFTPChannelParent(PFTPChannelParent*) MOZ_OVERRIDE;
  virtual PWebSocketParent*
    AllocPWebSocketParent(const PBrowserOrId& browser,
                          const SerializedLoadContext& aSerialized) MOZ_OVERRIDE;
  virtual bool DeallocPWebSocketParent(PWebSocketParent*) MOZ_OVERRIDE;
  virtual PTCPSocketParent* AllocPTCPSocketParent(const nsString& host,
                                                  const uint16_t& port) MOZ_OVERRIDE;

  virtual PRemoteOpenFileParent*
    AllocPRemoteOpenFileParent(const SerializedLoadContext& aSerialized,
                               const URIParams& aFileURI,
                               const OptionalURIParams& aAppURI) MOZ_OVERRIDE;
  virtual bool
    RecvPRemoteOpenFileConstructor(PRemoteOpenFileParent* aActor,
                                   const SerializedLoadContext& aSerialized,
                                   const URIParams& aFileURI,
                                   const OptionalURIParams& aAppURI)
                                   MOZ_OVERRIDE;
  virtual bool DeallocPRemoteOpenFileParent(PRemoteOpenFileParent* aActor)
                                            MOZ_OVERRIDE;

  virtual bool DeallocPTCPSocketParent(PTCPSocketParent*) MOZ_OVERRIDE;
  virtual PTCPServerSocketParent*
    AllocPTCPServerSocketParent(const uint16_t& aLocalPort,
                                const uint16_t& aBacklog,
                                const nsString& aBinaryType) MOZ_OVERRIDE;
  virtual bool RecvPTCPServerSocketConstructor(PTCPServerSocketParent*,
                                               const uint16_t& aLocalPort,
                                               const uint16_t& aBacklog,
                                               const nsString& aBinaryType) MOZ_OVERRIDE;
  virtual bool DeallocPTCPServerSocketParent(PTCPServerSocketParent*) MOZ_OVERRIDE;
  virtual PUDPSocketParent* AllocPUDPSocketParent(const nsCString& aHost,
                                                  const uint16_t& aPort,
                                                  const nsCString& aFilter) MOZ_OVERRIDE;
  virtual bool RecvPUDPSocketConstructor(PUDPSocketParent*,
                                         const nsCString& aHost,
                                         const uint16_t& aPort,
                                         const nsCString& aFilter) MOZ_OVERRIDE;
  virtual bool DeallocPUDPSocketParent(PUDPSocketParent*) MOZ_OVERRIDE;
  virtual PDNSRequestParent* AllocPDNSRequestParent(const nsCString& aHost,
                                                    const uint32_t& aFlags) MOZ_OVERRIDE;
  virtual bool RecvPDNSRequestConstructor(PDNSRequestParent* actor,
                                          const nsCString& hostName,
                                          const uint32_t& flags) MOZ_OVERRIDE;
  virtual bool DeallocPDNSRequestParent(PDNSRequestParent*) MOZ_OVERRIDE;
  virtual bool RecvHTMLDNSPrefetch(const nsString& hostname,
                                   const uint16_t& flags) MOZ_OVERRIDE;
  virtual bool RecvCancelHTMLDNSPrefetch(const nsString& hostname,
                                         const uint16_t& flags,
                                         const nsresult& reason) MOZ_OVERRIDE;

  virtual mozilla::ipc::IProtocol*
  CloneProtocol(Channel* aChannel,
                mozilla::ipc::ProtocolCloneContext* aCtx) MOZ_OVERRIDE;
  virtual PRtspControllerParent* AllocPRtspControllerParent() MOZ_OVERRIDE;
  virtual bool DeallocPRtspControllerParent(PRtspControllerParent*) MOZ_OVERRIDE;

  virtual PRtspChannelParent*
    AllocPRtspChannelParent(const RtspChannelConnectArgs& aArgs)
                            MOZ_OVERRIDE;
  virtual bool
    RecvPRtspChannelConstructor(PRtspChannelParent* aActor,
                                const RtspChannelConnectArgs& aArgs)
                                MOZ_OVERRIDE;
  virtual bool DeallocPRtspChannelParent(PRtspChannelParent*) MOZ_OVERRIDE;

  virtual PChannelDiverterParent*
  AllocPChannelDiverterParent(const ChannelDiverterArgs& channel) MOZ_OVERRIDE;
  virtual bool
  RecvPChannelDiverterConstructor(PChannelDiverterParent* actor,
                                  const ChannelDiverterArgs& channel) MOZ_OVERRIDE;
  virtual bool DeallocPChannelDiverterParent(PChannelDiverterParent* actor)
                                                                MOZ_OVERRIDE;

  virtual bool RecvOnAuthAvailable(const uint64_t& aCallbackId,
                                   const nsString& aUser,
                                   const nsString& aPassword,
                                   const nsString& aDomain) MOZ_OVERRIDE;
  virtual bool RecvOnAuthCancelled(const uint64_t& aCallbackId,
                                   const bool& aUserCancel) MOZ_OVERRIDE;

private:
  nsCString mCoreAppsBasePath;
  nsCString mWebAppsBasePath;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_NeckoParent_h
