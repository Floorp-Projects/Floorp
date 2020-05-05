/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTransportHandlerIPC.h"
#include "mozilla/dom/MediaTransportChild.h"
#include "nsThreadUtils.h"
#include "mozilla/net/SocketProcessBridgeChild.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "CSFLog.h"

namespace mozilla {

static const char* mthipcLogTag = "MediaTransportHandler";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG mthipcLogTag

MediaTransportHandlerIPC::MediaTransportHandlerIPC(
    nsISerialEventTarget* aCallbackThread)
    : MediaTransportHandler(aCallbackThread) {
  mInitPromise = net::SocketProcessBridgeChild::GetSocketProcessBridge()->Then(
      mCallbackThread, __func__,
      [this, self = RefPtr<MediaTransportHandlerIPC>(this)](
          const RefPtr<net::SocketProcessBridgeChild>& aBridge) {
        ipc::PBackgroundChild* actor =
            ipc::BackgroundChild::GetOrCreateSocketActorForCurrentThread();
        if (!actor) {
          NS_WARNING(
              "MediaTransportHandlerIPC async init failed! Webrtc networking "
              "will not work!");
          return InitPromise::CreateAndReject(
              nsCString("GetOrCreateSocketActorForCurrentThread failed!"),
              __func__);
        }
        MediaTransportChild* child = new MediaTransportChild(this);
        actor->SetEventTargetForActor(child, mCallbackThread);
        // PBackgroungChild owns mChild! When it is done with it,
        // mChild will let us know it it going away.
        mChild = actor->SendPMediaTransportConstructor(child);
        CSFLogDebug(LOGTAG, "%s Init done", __func__);
        return InitPromise::CreateAndResolve(true, __func__);
      },
      [=](const nsCString& aError) {
        CSFLogError(LOGTAG,
                    "MediaTransportHandlerIPC async init failed! Webrtc "
                    "networking will not work! Error was %s",
                    aError.get());
        NS_WARNING(
            "MediaTransportHandlerIPC async init failed! Webrtc networking "
            "will not work!");
        return InitPromise::CreateAndReject(aError, __func__);
      });
}

RefPtr<MediaTransportHandler::IceLogPromise>
MediaTransportHandlerIPC::GetIceLog(const nsCString& aPattern) {
  return mInitPromise->Then(
      mCallbackThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerIPC>(this)](bool /* dummy */) {
        if (!mChild) {
          return IceLogPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
        }
        // Compiler has trouble deducing the return type here for some reason,
        // so we use a temp variable as a hint.
        // SendGetIceLog _almost_ returns an IceLogPromise; the reject value
        // differs (ipc::ResponseRejectReason vs nsresult) so we need to
        // convert.
        RefPtr<IceLogPromise> promise = mChild->SendGetIceLog(aPattern)->Then(
            mCallbackThread, __func__,
            [](WebrtcGlobalLog&& aLogLines) {
              return IceLogPromise::CreateAndResolve(std::move(aLogLines),
                                                     __func__);
            },
            [](ipc::ResponseRejectReason aReason) {
              return IceLogPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
            });
        return promise;
      },
      [](const nsCString& aError) {
        return IceLogPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
      });
}

void MediaTransportHandlerIPC::ClearIceLog() {
  mInitPromise->Then(
      mCallbackThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (mChild) {
          mChild->SendClearIceLog();
        }
      },
      [](const nsCString& aError) {});
}

void MediaTransportHandlerIPC::EnterPrivateMode() {
  mInitPromise->Then(
      mCallbackThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (mChild) {
          mChild->SendEnterPrivateMode();
        }
      },
      [](const nsCString& aError) {});
}

void MediaTransportHandlerIPC::ExitPrivateMode() {
  mInitPromise->Then(
      mCallbackThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (mChild) {
          mChild->SendExitPrivateMode();
        }
      },
      [](const nsCString& aError) {});
}

nsresult MediaTransportHandlerIPC::CreateIceCtx(
    const std::string& aName, const nsTArray<dom::RTCIceServer>& aIceServers,
    dom::RTCIceTransportPolicy aIcePolicy) {
  CSFLogDebug(LOGTAG, "MediaTransportHandlerIPC::CreateIceCtx start");
  // Run some validation on this side of the IPC boundary so we can return
  // errors synchronously. We don't actually use the results. It might make
  // sense to move this check to PeerConnection and have this API take the
  // converted form, but we would need to write IPC serialization code for
  // the NrIce*Server types.
  std::vector<NrIceStunServer> stunServers;
  std::vector<NrIceTurnServer> turnServers;
  nsresult rv = ConvertIceServers(aIceServers, &stunServers, &turnServers);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mInitPromise->Then(
      mCallbackThread, __func__,
      [=, iceServers = aIceServers.Clone(),
       self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (mChild) {
          CSFLogDebug(LOGTAG, "%s starting", __func__);
          if (!mChild->SendCreateIceCtx(aName, std::move(iceServers),
                                        aIcePolicy)) {
            CSFLogError(LOGTAG, "%s failed!", __func__);
          }
        }
      },
      [](const nsCString& aError) {});

  return NS_OK;
}

void MediaTransportHandlerIPC::Destroy() {
  mInitPromise->Then(
      mCallbackThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (mChild) {
          MediaTransportChild::Send__delete__(mChild);
          mChild = nullptr;
        }
      },
      [](const nsCString& aError) {});
}

// We will probably be able to move the proxy lookup stuff into
// this class once we move mtransport to its own process.
void MediaTransportHandlerIPC::SetProxyConfig(
    NrSocketProxyConfig&& aProxyConfig) {
  mInitPromise->Then(
      mCallbackThread, __func__,
      [aProxyConfig = std::move(aProxyConfig), this,
       self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) mutable {
        if (mChild) {
          mChild->SendSetProxyConfig(aProxyConfig.GetConfig());
        }
      },
      [](const nsCString& aError) {});
}

void MediaTransportHandlerIPC::EnsureProvisionalTransport(
    const std::string& aTransportId, const std::string& aLocalUfrag,
    const std::string& aLocalPwd, size_t aComponentCount) {
  mInitPromise->Then(
      mCallbackThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (mChild) {
          mChild->SendEnsureProvisionalTransport(aTransportId, aLocalUfrag,
                                                 aLocalPwd, aComponentCount);
        }
      },
      [](const nsCString& aError) {});
}

void MediaTransportHandlerIPC::SetTargetForDefaultLocalAddressLookup(
    const std::string& aTargetIp, uint16_t aTargetPort) {
  mInitPromise->Then(
      mCallbackThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (mChild) {
          mChild->SendSetTargetForDefaultLocalAddressLookup(aTargetIp,
                                                            aTargetPort);
        }
      },
      [](const nsCString& aError) {});
}

// We set default-route-only as late as possible because it depends on what
// capture permissions have been granted on the window, which could easily
// change between Init (ie; when the PC is created) and StartIceGathering
// (ie; when we set the local description).
void MediaTransportHandlerIPC::StartIceGathering(
    bool aDefaultRouteOnly, bool aObfuscateHostAddresses,
    // TODO(bug 1522205): It probably makes sense to look this up internally
    const nsTArray<NrIceStunAddr>& aStunAddrs) {
  mInitPromise->Then(
      mCallbackThread, __func__,
      [=, stunAddrs = aStunAddrs.Clone(),
       self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (mChild) {
          mChild->SendStartIceGathering(aDefaultRouteOnly,
                                        aObfuscateHostAddresses, stunAddrs);
        }
      },
      [](const nsCString& aError) {});
}

void MediaTransportHandlerIPC::ActivateTransport(
    const std::string& aTransportId, const std::string& aLocalUfrag,
    const std::string& aLocalPwd, size_t aComponentCount,
    const std::string& aUfrag, const std::string& aPassword,
    const nsTArray<uint8_t>& aKeyDer, const nsTArray<uint8_t>& aCertDer,
    SSLKEAType aAuthType, bool aDtlsClient, const DtlsDigestList& aDigests,
    bool aPrivacyRequested) {
  mInitPromise->Then(
      mCallbackThread, __func__,
      [=, keyDer = aKeyDer.Clone(), certDer = aCertDer.Clone(),
       self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (mChild) {
          mChild->SendActivateTransport(aTransportId, aLocalUfrag, aLocalPwd,
                                        aComponentCount, aUfrag, aPassword,
                                        keyDer, certDer, aAuthType, aDtlsClient,
                                        aDigests, aPrivacyRequested);
        }
      },
      [](const nsCString& aError) {});
}

void MediaTransportHandlerIPC::RemoveTransportsExcept(
    const std::set<std::string>& aTransportIds) {
  std::vector<std::string> transportIds(aTransportIds.begin(),
                                        aTransportIds.end());
  mInitPromise->Then(
      mCallbackThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (mChild) {
          mChild->SendRemoveTransportsExcept(transportIds);
        }
      },
      [](const nsCString& aError) {});
}

void MediaTransportHandlerIPC::StartIceChecks(
    bool aIsControlling, const std::vector<std::string>& aIceOptions) {
  mInitPromise->Then(
      mCallbackThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (mChild) {
          mChild->SendStartIceChecks(aIsControlling, aIceOptions);
        }
      },
      [](const nsCString& aError) {});
}

void MediaTransportHandlerIPC::SendPacket(const std::string& aTransportId,
                                          MediaPacket&& aPacket) {
  mInitPromise->Then(
      mCallbackThread, __func__,
      [this, self = RefPtr<MediaTransportHandlerIPC>(this), aTransportId,
       aPacket = std::move(aPacket)](bool /*dummy*/) mutable {
        if (mChild) {
          mChild->SendSendPacket(aTransportId, aPacket);
        }
      },
      [](const nsCString& aError) {});
}

void MediaTransportHandlerIPC::AddIceCandidate(
    const std::string& aTransportId, const std::string& aCandidate,
    const std::string& aUfrag, const std::string& aObfuscatedAddress) {
  mInitPromise->Then(
      mCallbackThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (mChild) {
          mChild->SendAddIceCandidate(aTransportId, aCandidate, aUfrag,
                                      aObfuscatedAddress);
        }
      },
      [](const nsCString& aError) {});
}

void MediaTransportHandlerIPC::UpdateNetworkState(bool aOnline) {
  mInitPromise->Then(
      mCallbackThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (mChild) {
          mChild->SendUpdateNetworkState(aOnline);
        }
      },
      [](const nsCString& aError) {});
}

RefPtr<dom::RTCStatsPromise> MediaTransportHandlerIPC::GetIceStats(
    const std::string& aTransportId, DOMHighResTimeStamp aNow) {
  return mInitPromise->Then(
      mCallbackThread, __func__,
      [aTransportId, aNow, this,
       self = RefPtr<MediaTransportHandlerIPC>(this)](bool /*dummy*/) {
        if (!mChild) {
          return dom::RTCStatsPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
        }
        RefPtr<dom::RTCStatsPromise> promise =
            mChild->SendGetIceStats(aTransportId, aNow)
                ->Then(
                    mCallbackThread, __func__,
                    [](const dom::RTCStatsCollection& aStats) {
                      UniquePtr<dom::RTCStatsCollection> stats(
                          new dom::RTCStatsCollection(aStats));
                      return dom::RTCStatsPromise::CreateAndResolve(
                          std::move(stats), __func__);
                    },
                    [](ipc::ResponseRejectReason aReason) {
                      return dom::RTCStatsPromise::CreateAndReject(
                          NS_ERROR_FAILURE, __func__);
                    });
        return promise;
      },
      [](const nsCString& aError) {
        return dom::RTCStatsPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                     __func__);
      });
}

MediaTransportChild::MediaTransportChild(MediaTransportHandlerIPC* aUser)
    : mUser(aUser) {}

MediaTransportChild::~MediaTransportChild() { mUser->mChild = nullptr; }

mozilla::ipc::IPCResult MediaTransportChild::RecvOnCandidate(
    const string& transportId, const CandidateInfo& candidateInfo) {
  mUser->OnCandidate(transportId, candidateInfo);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportChild::RecvOnAlpnNegotiated(
    const string& alpn) {
  mUser->OnAlpnNegotiated(alpn);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportChild::RecvOnGatheringStateChange(
    const int& state) {
  mUser->OnGatheringStateChange(static_cast<dom::RTCIceGatheringState>(state));
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportChild::RecvOnConnectionStateChange(
    const int& state) {
  mUser->OnConnectionStateChange(
      static_cast<dom::RTCIceConnectionState>(state));
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportChild::RecvOnPacketReceived(
    const string& transportId, const MediaPacket& packet) {
  MediaPacket copy(packet);  // Laaaaaame! Might be safe to const_cast?
  mUser->OnPacketReceived(transportId, copy);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportChild::RecvOnEncryptedSending(
    const string& transportId, const MediaPacket& packet) {
  MediaPacket copy(packet);  // Laaaaaame! Might be safe to const_cast?
  mUser->OnEncryptedSending(transportId, copy);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportChild::RecvOnStateChange(
    const string& transportId, const int& state) {
  mUser->OnStateChange(transportId, static_cast<TransportLayer::State>(state));
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportChild::RecvOnRtcpStateChange(
    const string& transportId, const int& state) {
  mUser->OnRtcpStateChange(transportId,
                           static_cast<TransportLayer::State>(state));
  return ipc::IPCResult::Ok();
}

}  // namespace mozilla
