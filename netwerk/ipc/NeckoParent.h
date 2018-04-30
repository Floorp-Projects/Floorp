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
  virtual ~NeckoParent() = default;

  MOZ_MUST_USE
  static const char *
  GetValidatedOriginAttributes(const SerializedLoadContext& aSerialized,
                               PContentParent* aBrowser,
                               nsIPrincipal* aRequestingPrincipal,
                               mozilla::OriginAttributes& aAttrs);

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
                           nsIPrincipal* aRequestingPrincipal,
                           nsCOMPtr<nsILoadContext> &aResult);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual PCookieServiceParent* AllocPCookieServiceParent() override;
  virtual mozilla::ipc::IPCResult
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
  virtual mozilla::ipc::IPCResult
    RecvPHttpChannelConstructor(
      PHttpChannelParent* aActor,
      const PBrowserOrId& aBrowser,
      const SerializedLoadContext& aSerialized,
      const HttpChannelCreationArgs& aOpenArgs) override;
  virtual bool DeallocPHttpChannelParent(PHttpChannelParent*) override;

  virtual PStunAddrsRequestParent* AllocPStunAddrsRequestParent() override;
  virtual bool
    DeallocPStunAddrsRequestParent(PStunAddrsRequestParent* aActor) override;

  virtual PAltDataOutputStreamParent* AllocPAltDataOutputStreamParent(
    const nsCString& type, const int64_t& predictedSize, PHttpChannelParent* channel) override;
  virtual bool DeallocPAltDataOutputStreamParent(
    PAltDataOutputStreamParent* aActor) override;

  virtual bool DeallocPCookieServiceParent(PCookieServiceParent*) override;
  virtual PWyciwygChannelParent* AllocPWyciwygChannelParent() override;
  virtual bool DeallocPWyciwygChannelParent(PWyciwygChannelParent*) override;
  virtual PFTPChannelParent*
    AllocPFTPChannelParent(const PBrowserOrId& aBrowser,
                           const SerializedLoadContext& aSerialized,
                           const FTPChannelCreationArgs& aOpenArgs) override;
  virtual mozilla::ipc::IPCResult
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
  virtual mozilla::ipc::IPCResult RecvPTCPServerSocketConstructor(PTCPServerSocketParent*,
                                                                  const uint16_t& aLocalPort,
                                                                  const uint16_t& aBacklog,
                                                                  const bool& aUseArrayBuffers) override;
  virtual bool DeallocPTCPServerSocketParent(PTCPServerSocketParent*) override;
  virtual PUDPSocketParent* AllocPUDPSocketParent(const Principal& aPrincipal,
                                                  const nsCString& aFilter) override;
  virtual mozilla::ipc::IPCResult RecvPUDPSocketConstructor(PUDPSocketParent*,
                                                            const Principal& aPrincipal,
                                                            const nsCString& aFilter) override;
  virtual bool DeallocPUDPSocketParent(PUDPSocketParent*) override;
  virtual PDNSRequestParent* AllocPDNSRequestParent(const nsCString& aHost,
                                                    const OriginAttributes& aOriginAttributes,
                                                    const uint32_t& aFlags,
                                                    const nsCString& aNetworkInterface) override;
  virtual mozilla::ipc::IPCResult RecvPDNSRequestConstructor(PDNSRequestParent* actor,
                                                             const nsCString& hostName,
                                                             const OriginAttributes& aOriginAttributes,
                                                             const uint32_t& flags,
                                                             const nsCString& aNetworkInterface) override;
  virtual bool DeallocPDNSRequestParent(PDNSRequestParent*) override;
  virtual mozilla::ipc::IPCResult RecvSpeculativeConnect(const URIParams& aURI,
                                                         const Principal& aPrincipal,
                                                         const bool& aAnonymous) override;
  virtual mozilla::ipc::IPCResult RecvHTMLDNSPrefetch(const nsString& hostname,
                                                      const OriginAttributes& aOriginAttributes,
                                                      const uint16_t& flags) override;
  virtual mozilla::ipc::IPCResult RecvCancelHTMLDNSPrefetch(const nsString& hostname,
                                                            const OriginAttributes& aOriginAttributes,
                                                            const uint16_t& flags,
                                                            const nsresult& reason) override;
  virtual PWebSocketEventListenerParent*
    AllocPWebSocketEventListenerParent(const uint64_t& aInnerWindowID) override;
  virtual bool DeallocPWebSocketEventListenerParent(PWebSocketEventListenerParent*) override;

  virtual PDataChannelParent*
    AllocPDataChannelParent(const uint32_t& channelId) override;
  virtual bool DeallocPDataChannelParent(PDataChannelParent* parent) override;

  virtual mozilla::ipc::IPCResult RecvPDataChannelConstructor(PDataChannelParent* aActor,
                                                              const uint32_t& channelId) override;

  virtual PSimpleChannelParent*
    AllocPSimpleChannelParent(const uint32_t& channelId) override;
  virtual bool DeallocPSimpleChannelParent(PSimpleChannelParent* parent) override;

  virtual mozilla::ipc::IPCResult RecvPSimpleChannelConstructor(PSimpleChannelParent* aActor,
                                                              const uint32_t& channelId) override;

  virtual PFileChannelParent*
    AllocPFileChannelParent(const uint32_t& channelId) override;
  virtual bool DeallocPFileChannelParent(PFileChannelParent* parent) override;

  virtual mozilla::ipc::IPCResult RecvPFileChannelConstructor(PFileChannelParent* aActor,
                                                              const uint32_t& channelId) override;

  virtual PChannelDiverterParent*
  AllocPChannelDiverterParent(const ChannelDiverterArgs& channel) override;
  virtual mozilla::ipc::IPCResult
  RecvPChannelDiverterConstructor(PChannelDiverterParent* actor,
                                  const ChannelDiverterArgs& channel) override;
  virtual bool DeallocPChannelDiverterParent(PChannelDiverterParent* actor)
                                                                override;
  virtual PTransportProviderParent*
  AllocPTransportProviderParent() override;
  virtual bool
  DeallocPTransportProviderParent(PTransportProviderParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvOnAuthAvailable(const uint64_t& aCallbackId,
                                                      const nsString& aUser,
                                                      const nsString& aPassword,
                                                      const nsString& aDomain) override;
  virtual mozilla::ipc::IPCResult RecvOnAuthCancelled(const uint64_t& aCallbackId,
                                                      const bool& aUserCancel) override;

  /* Predictor Messages */
  virtual mozilla::ipc::IPCResult RecvPredPredict(const ipc::OptionalURIParams& aTargetURI,
                                                  const ipc::OptionalURIParams& aSourceURI,
                                                  const PredictorPredictReason& aReason,
                                                  const OriginAttributes& aOriginAttributes,
                                                  const bool& hasVerifier) override;

  virtual mozilla::ipc::IPCResult RecvPredLearn(const ipc::URIParams& aTargetURI,
                                                const ipc::OptionalURIParams& aSourceURI,
                                                const PredictorPredictReason& aReason,
                                                const OriginAttributes& aOriginAttributes) override;
  virtual mozilla::ipc::IPCResult RecvPredReset() override;

  virtual mozilla::ipc::IPCResult RecvRequestContextLoadBegin(const uint64_t& rcid) override;
  virtual mozilla::ipc::IPCResult RecvRequestContextAfterDOMContentLoaded(const uint64_t& rcid) override;
  virtual mozilla::ipc::IPCResult RecvRemoveRequestContext(const uint64_t& rcid) override;

  /* WebExtensions */
  virtual mozilla::ipc::IPCResult
    RecvGetExtensionStream(const URIParams& aURI,
                           GetExtensionStreamResolver&& aResolve) override;

  virtual mozilla::ipc::IPCResult
    RecvGetExtensionFD(const URIParams& aURI,
                       GetExtensionFDResolver&& aResolve) override;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_NeckoParent_h
