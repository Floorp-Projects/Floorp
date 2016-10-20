/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasePrincipal.h"
#include "mozilla/net/PNeckoParent.h"
#include "mozilla/net/NeckoCommon.h"
#include "nsIAuthPrompt2.h"
#include "nsINetworkPredictor.h"
#include "nsNetUtil.h"

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
class NeckoParent
  : public PNeckoParent
{
public:
  NeckoParent();
  virtual ~NeckoParent();

  MOZ_MUST_USE
  static const char *
  GetValidatedAppInfo(const SerializedLoadContext& aSerialized,
                      PContentParent* aBrowser,
                      mozilla::DocShellOriginAttributes& aAttrs);

  /*
   * Creates LoadContext for parent-side of an e10s channel.
   *
   * PContentParent corresponds to the process that is requesting the load.
   *
   * Returns null if successful, or an error string if failed.
   */
  MOZ_MUST_USE
  static const char*
  CreateChannelLoadContext(const PBrowserOrId& aBrowser,
                           PContentParent* aContent,
                           const SerializedLoadContext& aSerialized,
                           nsCOMPtr<nsILoadContext> &aResult);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual PCookieServiceParent* AllocPCookieServiceParent() override;
  virtual bool
  RecvPCookieServiceConstructor(PCookieServiceParent* aActor) override
  {
    return PNeckoParent::RecvPCookieServiceConstructor(aActor);
  }

  /*
   * This implementation of nsIAuthPrompt2 is used for nested remote iframes that
   * want an auth prompt.  This class lives in the parent process and informs the
   * NeckoChild that we want an auth prompt, which forwards the request to the
   * TabParent in the remote iframe that contains the nested iframe
   */
  class NestedFrameAuthPrompt final : public nsIAuthPrompt2
  {
    ~NestedFrameAuthPrompt() {}

  public:
    NS_DECL_ISUPPORTS

    NestedFrameAuthPrompt(PNeckoParent* aParent, TabId aNestedFrameId);

    NS_IMETHOD PromptAuth(nsIChannel*, uint32_t, nsIAuthInformation*, bool*) override
    {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD AsyncPromptAuth(nsIChannel* aChannel, nsIAuthPromptCallback* callback,
                               nsISupports*, uint32_t,
                               nsIAuthInformation* aInfo, nsICancelable**) override;

  protected:
    PNeckoParent* mNeckoParent;
    TabId mNestedFrameId;
  };

protected:
  virtual PHttpChannelParent*
    AllocPHttpChannelParent(const PBrowserOrId&, const SerializedLoadContext&,
                            const HttpChannelCreationArgs& aOpenArgs) override;
  virtual bool
    RecvPHttpChannelConstructor(
                      PHttpChannelParent* aActor,
                      const PBrowserOrId& aBrowser,
                      const SerializedLoadContext& aSerialized,
                      const HttpChannelCreationArgs& aOpenArgs) override;
  virtual bool DeallocPHttpChannelParent(PHttpChannelParent*) override;

  virtual PAltDataOutputStreamParent* AllocPAltDataOutputStreamParent(
    const nsCString& type, PHttpChannelParent* channel) override;
  virtual bool DeallocPAltDataOutputStreamParent(
    PAltDataOutputStreamParent* aActor) override;

  virtual bool DeallocPCookieServiceParent(PCookieServiceParent*) override;
  virtual PWyciwygChannelParent* AllocPWyciwygChannelParent() override;
  virtual bool DeallocPWyciwygChannelParent(PWyciwygChannelParent*) override;
  virtual PFTPChannelParent*
    AllocPFTPChannelParent(const PBrowserOrId& aBrowser,
                           const SerializedLoadContext& aSerialized,
                           const FTPChannelCreationArgs& aOpenArgs) override;
  virtual bool
    RecvPFTPChannelConstructor(
                      PFTPChannelParent* aActor,
                      const PBrowserOrId& aBrowser,
                      const SerializedLoadContext& aSerialized,
                      const FTPChannelCreationArgs& aOpenArgs) override;
  virtual bool DeallocPFTPChannelParent(PFTPChannelParent*) override;
  virtual PWebSocketParent*
    AllocPWebSocketParent(const PBrowserOrId& browser,
                          const SerializedLoadContext& aSerialized,
                          const uint32_t& aSerial) override;
  virtual bool DeallocPWebSocketParent(PWebSocketParent*) override;
  virtual PTCPSocketParent* AllocPTCPSocketParent(const nsString& host,
                                                  const uint16_t& port) override;

  virtual bool DeallocPTCPSocketParent(PTCPSocketParent*) override;
  virtual PTCPServerSocketParent*
    AllocPTCPServerSocketParent(const uint16_t& aLocalPort,
                                const uint16_t& aBacklog,
                                const bool& aUseArrayBuffers) override;
  virtual bool RecvPTCPServerSocketConstructor(PTCPServerSocketParent*,
                                               const uint16_t& aLocalPort,
                                               const uint16_t& aBacklog,
                                               const bool& aUseArrayBuffers) override;
  virtual bool DeallocPTCPServerSocketParent(PTCPServerSocketParent*) override;
  virtual PUDPSocketParent* AllocPUDPSocketParent(const Principal& aPrincipal,
                                                  const nsCString& aFilter) override;
  virtual bool RecvPUDPSocketConstructor(PUDPSocketParent*,
                                         const Principal& aPrincipal,
                                         const nsCString& aFilter) override;
  virtual bool DeallocPUDPSocketParent(PUDPSocketParent*) override;
  virtual PDNSRequestParent* AllocPDNSRequestParent(const nsCString& aHost,
                                                    const uint32_t& aFlags,
                                                    const nsCString& aNetworkInterface) override;
  virtual bool RecvPDNSRequestConstructor(PDNSRequestParent* actor,
                                          const nsCString& hostName,
                                          const uint32_t& flags,
                                          const nsCString& aNetworkInterface) override;
  virtual bool DeallocPDNSRequestParent(PDNSRequestParent*) override;
  virtual bool RecvSpeculativeConnect(const URIParams& aURI, const bool& aAnonymous) override;
  virtual bool RecvHTMLDNSPrefetch(const nsString& hostname,
                                   const uint16_t& flags) override;
  virtual bool RecvCancelHTMLDNSPrefetch(const nsString& hostname,
                                         const uint16_t& flags,
                                         const nsresult& reason) override;
  virtual PWebSocketEventListenerParent*
    AllocPWebSocketEventListenerParent(const uint64_t& aInnerWindowID) override;
  virtual bool DeallocPWebSocketEventListenerParent(PWebSocketEventListenerParent*) override;

  virtual mozilla::ipc::IProtocol*
  CloneProtocol(Channel* aChannel,
                mozilla::ipc::ProtocolCloneContext* aCtx) override;

  virtual PDataChannelParent*
    AllocPDataChannelParent(const uint32_t& channelId) override;
  virtual bool DeallocPDataChannelParent(PDataChannelParent* parent) override;

  virtual bool RecvPDataChannelConstructor(PDataChannelParent* aActor,
                                           const uint32_t& channelId) override;

  virtual PRtspControllerParent* AllocPRtspControllerParent() override;
  virtual bool DeallocPRtspControllerParent(PRtspControllerParent*) override;

  virtual PRtspChannelParent*
    AllocPRtspChannelParent(const RtspChannelConnectArgs& aArgs)
                            override;
  virtual bool
    RecvPRtspChannelConstructor(PRtspChannelParent* aActor,
                                const RtspChannelConnectArgs& aArgs)
                                override;
  virtual bool DeallocPRtspChannelParent(PRtspChannelParent*) override;

  virtual PChannelDiverterParent*
  AllocPChannelDiverterParent(const ChannelDiverterArgs& channel) override;
  virtual bool
  RecvPChannelDiverterConstructor(PChannelDiverterParent* actor,
                                  const ChannelDiverterArgs& channel) override;
  virtual bool DeallocPChannelDiverterParent(PChannelDiverterParent* actor)
                                                                override;
  virtual PTransportProviderParent*
  AllocPTransportProviderParent() override;
  virtual bool
  DeallocPTransportProviderParent(PTransportProviderParent* aActor) override;

  virtual bool RecvOnAuthAvailable(const uint64_t& aCallbackId,
                                   const nsString& aUser,
                                   const nsString& aPassword,
                                   const nsString& aDomain) override;
  virtual bool RecvOnAuthCancelled(const uint64_t& aCallbackId,
                                   const bool& aUserCancel) override;

  /* Predictor Messages */
  virtual bool RecvPredPredict(const ipc::OptionalURIParams& aTargetURI,
                               const ipc::OptionalURIParams& aSourceURI,
                               const PredictorPredictReason& aReason,
                               const IPC::SerializedLoadContext& aLoadContext,
                               const bool& hasVerifier) override;

  virtual bool RecvPredLearn(const ipc::URIParams& aTargetURI,
                             const ipc::OptionalURIParams& aSourceURI,
                             const PredictorPredictReason& aReason,
                             const IPC::SerializedLoadContext& aLoadContext) override;
  virtual bool RecvPredReset() override;

  virtual bool RecvRemoveRequestContext(const nsCString& rcid) override;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_NeckoParent_h
