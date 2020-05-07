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
#  define mozilla_net_NeckoParent_h

namespace mozilla {
namespace net {

// Used to override channel Private Browsing status if needed.
enum PBOverrideStatus {
  kPBOverride_Unset = 0,
  kPBOverride_Private,
  kPBOverride_NotPrivate
};

// Header file contents
class NeckoParent : public PNeckoParent {
  friend class PNeckoParent;

 public:
  NeckoParent();
  virtual ~NeckoParent() = default;

  [[nodiscard]] static const char* GetValidatedOriginAttributes(
      const SerializedLoadContext& aSerialized, PContentParent* aBrowser,
      nsIPrincipal* aRequestingPrincipal, mozilla::OriginAttributes& aAttrs);

  /*
   * Creates LoadContext for parent-side of an e10s channel.
   *
   * PContentParent corresponds to the process that is requesting the load.
   *
   * Returns null if successful, or an error string if failed.
   */
  [[nodiscard]] static const char* CreateChannelLoadContext(
      PBrowserParent* aBrowser, PContentParent* aContent,
      const SerializedLoadContext& aSerialized,
      nsIPrincipal* aRequestingPrincipal, nsCOMPtr<nsILoadContext>& aResult);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  PCookieServiceParent* AllocPCookieServiceParent();
  virtual mozilla::ipc::IPCResult RecvPCookieServiceConstructor(
      PCookieServiceParent* aActor) override {
    return PNeckoParent::RecvPCookieServiceConstructor(aActor);
  }

  /*
   * This implementation of nsIAuthPrompt2 is used for nested remote iframes
   * that want an auth prompt.  This class lives in the parent process and
   * informs the NeckoChild that we want an auth prompt, which forwards the
   * request to the BrowserParent in the remote iframe that contains the nested
   * iframe
   */
  class NestedFrameAuthPrompt final : public nsIAuthPrompt2 {
    ~NestedFrameAuthPrompt() {}

   public:
    NS_DECL_ISUPPORTS

    NestedFrameAuthPrompt(PNeckoParent* aParent, TabId aNestedFrameId);

    NS_IMETHOD PromptAuth(nsIChannel*, uint32_t, nsIAuthInformation*,
                          bool*) override {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD AsyncPromptAuth(nsIChannel* aChannel,
                               nsIAuthPromptCallback* callback, nsISupports*,
                               uint32_t, nsIAuthInformation* aInfo,
                               nsICancelable**) override;

   protected:
    PNeckoParent* mNeckoParent;
    TabId mNestedFrameId;
  };

 protected:
  bool mSocketProcessBridgeInited;

  already_AddRefed<PHttpChannelParent> AllocPHttpChannelParent(
      PBrowserParent*, const SerializedLoadContext&,
      const HttpChannelCreationArgs& aOpenArgs);
  virtual mozilla::ipc::IPCResult RecvPHttpChannelConstructor(
      PHttpChannelParent* aActor, PBrowserParent* aBrowser,
      const SerializedLoadContext& aSerialized,
      const HttpChannelCreationArgs& aOpenArgs) override;

  PStunAddrsRequestParent* AllocPStunAddrsRequestParent();
  bool DeallocPStunAddrsRequestParent(PStunAddrsRequestParent* aActor);

  PWebrtcTCPSocketParent* AllocPWebrtcTCPSocketParent(
      const Maybe<TabId>& aTabId);
  bool DeallocPWebrtcTCPSocketParent(PWebrtcTCPSocketParent* aActor);

  PAltDataOutputStreamParent* AllocPAltDataOutputStreamParent(
      const nsCString& type, const int64_t& predictedSize,
      PHttpChannelParent* channel);
  bool DeallocPAltDataOutputStreamParent(PAltDataOutputStreamParent* aActor);

  bool DeallocPCookieServiceParent(PCookieServiceParent*);
  PFTPChannelParent* AllocPFTPChannelParent(
      PBrowserParent* aBrowser, const SerializedLoadContext& aSerialized,
      const FTPChannelCreationArgs& aOpenArgs);
  virtual mozilla::ipc::IPCResult RecvPFTPChannelConstructor(
      PFTPChannelParent* aActor, PBrowserParent* aBrowser,
      const SerializedLoadContext& aSerialized,
      const FTPChannelCreationArgs& aOpenArgs) override;
  bool DeallocPFTPChannelParent(PFTPChannelParent*);
  PWebSocketParent* AllocPWebSocketParent(
      PBrowserParent* browser, const SerializedLoadContext& aSerialized,
      const uint32_t& aSerial);
  bool DeallocPWebSocketParent(PWebSocketParent*);
  PTCPSocketParent* AllocPTCPSocketParent(const nsString& host,
                                          const uint16_t& port);

  already_AddRefed<PDocumentChannelParent> AllocPDocumentChannelParent(
      const dom::MaybeDiscarded<dom::BrowsingContext>& aContext,
      const DocumentChannelCreationArgs& args);
  virtual mozilla::ipc::IPCResult RecvPDocumentChannelConstructor(
      PDocumentChannelParent* aActor,
      const dom::MaybeDiscarded<dom::BrowsingContext>& aContext,
      const DocumentChannelCreationArgs& aArgs) override;
  bool DeallocPDocumentChannelParent(PDocumentChannelParent* channel);

  bool DeallocPTCPSocketParent(PTCPSocketParent*);
  PTCPServerSocketParent* AllocPTCPServerSocketParent(
      const uint16_t& aLocalPort, const uint16_t& aBacklog,
      const bool& aUseArrayBuffers);
  virtual mozilla::ipc::IPCResult RecvPTCPServerSocketConstructor(
      PTCPServerSocketParent*, const uint16_t& aLocalPort,
      const uint16_t& aBacklog, const bool& aUseArrayBuffers) override;
  bool DeallocPTCPServerSocketParent(PTCPServerSocketParent*);
  PUDPSocketParent* AllocPUDPSocketParent(nsIPrincipal* aPrincipal,
                                          const nsCString& aFilter);
  virtual mozilla::ipc::IPCResult RecvPUDPSocketConstructor(
      PUDPSocketParent*, nsIPrincipal* aPrincipal,
      const nsCString& aFilter) override;
  bool DeallocPUDPSocketParent(PUDPSocketParent*);
  already_AddRefed<PDNSRequestParent> AllocPDNSRequestParent(
      const nsCString& aHost, const nsCString& aTrrServer,
      const uint16_t& aType, const OriginAttributes& aOriginAttributes,
      const uint32_t& aFlags);
  virtual mozilla::ipc::IPCResult RecvPDNSRequestConstructor(
      PDNSRequestParent* actor, const nsCString& hostName,
      const nsCString& trrServer, const uint16_t& type,
      const OriginAttributes& aOriginAttributes,
      const uint32_t& flags) override;
  mozilla::ipc::IPCResult RecvSpeculativeConnect(nsIURI* aURI,
                                                 nsIPrincipal* aPrincipal,
                                                 const bool& aAnonymous);
  mozilla::ipc::IPCResult RecvHTMLDNSPrefetch(
      const nsString& hostname, const bool& isHttps,
      const OriginAttributes& aOriginAttributes, const uint32_t& flags);
  mozilla::ipc::IPCResult RecvCancelHTMLDNSPrefetch(
      const nsString& hostname, const bool& isHttps,
      const OriginAttributes& aOriginAttributes, const uint32_t& flags,
      const nsresult& reason);
  PWebSocketEventListenerParent* AllocPWebSocketEventListenerParent(
      const uint64_t& aInnerWindowID);
  bool DeallocPWebSocketEventListenerParent(PWebSocketEventListenerParent*);

  already_AddRefed<PDataChannelParent> AllocPDataChannelParent(
      const uint32_t& channelId);

  virtual mozilla::ipc::IPCResult RecvPDataChannelConstructor(
      PDataChannelParent* aActor, const uint32_t& channelId) override;

  PSimpleChannelParent* AllocPSimpleChannelParent(const uint32_t& channelId);
  bool DeallocPSimpleChannelParent(PSimpleChannelParent* parent);

  virtual mozilla::ipc::IPCResult RecvPSimpleChannelConstructor(
      PSimpleChannelParent* aActor, const uint32_t& channelId) override;

  already_AddRefed<PFileChannelParent> AllocPFileChannelParent(
      const uint32_t& channelId);

  virtual mozilla::ipc::IPCResult RecvPFileChannelConstructor(
      PFileChannelParent* aActor, const uint32_t& channelId) override;

  PChannelDiverterParent* AllocPChannelDiverterParent(
      const ChannelDiverterArgs& channel);
  virtual mozilla::ipc::IPCResult RecvPChannelDiverterConstructor(
      PChannelDiverterParent* actor,
      const ChannelDiverterArgs& channel) override;
  bool DeallocPChannelDiverterParent(PChannelDiverterParent* actor);
  PTransportProviderParent* AllocPTransportProviderParent();
  bool DeallocPTransportProviderParent(PTransportProviderParent* aActor);

  mozilla::ipc::IPCResult RecvOnAuthAvailable(const uint64_t& aCallbackId,
                                              const nsString& aUser,
                                              const nsString& aPassword,
                                              const nsString& aDomain);
  mozilla::ipc::IPCResult RecvOnAuthCancelled(const uint64_t& aCallbackId,
                                              const bool& aUserCancel);

  /* Predictor Messages */
  mozilla::ipc::IPCResult RecvPredPredict(
      nsIURI* aTargetURI, nsIURI* aSourceURI,
      const PredictorPredictReason& aReason,
      const OriginAttributes& aOriginAttributes, const bool& hasVerifier);

  mozilla::ipc::IPCResult RecvPredLearn(
      nsIURI* aTargetURI, nsIURI* aSourceURI,
      const PredictorPredictReason& aReason,
      const OriginAttributes& aOriginAttributes);
  mozilla::ipc::IPCResult RecvPredReset();

  mozilla::ipc::IPCResult RecvRequestContextLoadBegin(const uint64_t& rcid);
  mozilla::ipc::IPCResult RecvRequestContextAfterDOMContentLoaded(
      const uint64_t& rcid);
  mozilla::ipc::IPCResult RecvRemoveRequestContext(const uint64_t& rcid);

  /* WebExtensions */
  mozilla::ipc::IPCResult RecvGetExtensionStream(
      nsIURI* aURI, GetExtensionStreamResolver&& aResolve);

  mozilla::ipc::IPCResult RecvGetExtensionFD(nsIURI* aURI,
                                             GetExtensionFDResolver&& aResolve);

  /* Page thumbnails remote resource loading */
  mozilla::ipc::IPCResult RecvGetPageThumbStream(
      nsIURI* aURI, GetPageThumbStreamResolver&& aResolve);

  PClassifierDummyChannelParent* AllocPClassifierDummyChannelParent(
      nsIURI* aURI, nsIURI* aTopWindowURI, const nsresult& aTopWindowURIResult,
      const Maybe<LoadInfoArgs>& aLoadInfo);

  bool DeallocPClassifierDummyChannelParent(
      PClassifierDummyChannelParent* aParent);

  virtual mozilla::ipc::IPCResult RecvPClassifierDummyChannelConstructor(
      PClassifierDummyChannelParent* aActor, nsIURI* aURI,
      nsIURI* aTopWindowURI, const nsresult& aTopWindowURIResult,
      const Maybe<LoadInfoArgs>& aLoadInfo) override;

  mozilla::ipc::IPCResult RecvInitSocketProcessBridge(
      InitSocketProcessBridgeResolver&& aResolver);

  mozilla::ipc::IPCResult RecvEnsureHSTSData(
      EnsureHSTSDataResolver&& aResolver);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_NeckoParent_h
