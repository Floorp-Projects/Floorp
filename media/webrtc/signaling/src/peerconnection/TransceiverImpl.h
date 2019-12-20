/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _TRANSCEIVERIMPL_H_
#define _TRANSCEIVERIMPL_H_

#include <string>
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsISerialEventTarget.h"
#include "nsTArray.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "ErrorList.h"
#include "signaling/src/jsep/JsepTransceiver.h"
#include "signaling/src/media-conduit/RtcpEventObserver.h"

class nsIPrincipal;

namespace mozilla {
class PeerIdentity;
class JsepTransceiver;
enum class MediaSessionConduitLocalDirection : int;
class MediaSessionConduit;
class VideoSessionConduit;
class AudioSessionConduit;
class MediaPipelineReceive;
class MediaPipelineTransmit;
class MediaPipeline;
class MediaPipelineFilter;
class MediaTransportHandler;
class WebRtcCallWrapper;
class JsepTrackNegotiatedDetails;

namespace dom {
class RTCRtpTransceiver;
struct RTCRtpSourceEntry;
}  // namespace dom

/**
 * This is what ties all the various pieces that make up a transceiver
 * together. This includes:
 * MediaStreamTrack for rendering and capture
 * MediaTransportHandler for RTP transmission/reception
 * Audio/VideoConduit for feeding RTP/RTCP into webrtc.org for decoding, and
 * feeding audio/video frames into webrtc.org for encoding into RTP/RTCP.
 */
class TransceiverImpl : public nsISupports, public RtcpEventObserver {
 public:
  /**
   * |aReceiveTrack| is always set; this holds even if the remote end has not
   * negotiated one for this transceiver. |aSendTrack| might or might not be
   * set.
   */
  TransceiverImpl(
      const std::string& aPCHandle, MediaTransportHandler* aTransportHandler,
      JsepTransceiver* aJsepTransceiver, nsISerialEventTarget* aMainThread,
      nsISerialEventTarget* aStsThread, dom::MediaStreamTrack* aReceiveTrack,
      dom::MediaStreamTrack* aSendTrack, WebRtcCallWrapper* aCallWrapper,
      const PrincipalHandle& aPrincipalHandle);

  bool IsValid() const { return !!mConduit; }

  nsresult UpdateSendTrack(dom::MediaStreamTrack* aSendTrack);

  nsresult UpdateSinkIdentity(const dom::MediaStreamTrack* aTrack,
                              nsIPrincipal* aPrincipal,
                              const PeerIdentity* aSinkIdentity);

  nsresult UpdateTransport();

  nsresult UpdateConduit();

  void ResetSync();

  nsresult SyncWithMatchingVideoConduits(
      std::vector<RefPtr<TransceiverImpl>>& transceivers);

  void Shutdown_m();

  bool ConduitHasPluginID(uint64_t aPluginID);

  bool HasSendTrack(const dom::MediaStreamTrack* aSendTrack) const;

  // This is so PCImpl can unregister from PrincipalChanged callbacks; maybe we
  // should have TransceiverImpl handle these callbacks instead? It would need
  // to be able to get a ref to PCImpl though.
  RefPtr<dom::MediaStreamTrack> GetSendTrack() { return mSendTrack; }

  // for webidl
  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector);
  already_AddRefed<dom::MediaStreamTrack> GetReceiveTrack();
  void SetReceiveTrackMuted(bool aMuted);
  void SyncWithJS(dom::RTCRtpTransceiver& aJsTransceiver, ErrorResult& aRv);

  void InsertDTMFTone(int tone, uint32_t duration);

  bool HasReceiveTrack(const dom::MediaStreamTrack* aReceiveTrack) const;

  // TODO: These are for stats; try to find a cleaner way.
  RefPtr<MediaPipelineTransmit> GetSendPipeline();

  RefPtr<MediaPipelineReceive> GetReceivePipeline();

  std::string GetTransportId() const {
    return mJsepTransceiver->mTransport.mTransportId;
  }

  void AddRIDExtension(unsigned short aExtensionId);

  void AddRIDFilter(const nsAString& aRid);

  bool IsVideo() const;

  bool IsSending() const {
    return !mJsepTransceiver->IsStopped() &&
           mJsepTransceiver->mSendTrack.GetActive();
  }

  void GetRtpSources(const int64_t aTimeNow,
                     nsTArray<dom::RTCRtpSourceEntry>& outSources) const;

  void OnRtcpBye() override;

  void OnRtcpTimeout() override;

  // test-only: insert fake CSRCs and audio levels for testing
  void InsertAudioLevelForContributingSource(const uint32_t aSource,
                                             const int64_t aTimestamp,
                                             const uint32_t aRtpTimestamp,
                                             const bool aHasLevel,
                                             const uint8_t aLevel);

  NS_DECL_THREADSAFE_ISUPPORTS

 private:
  virtual ~TransceiverImpl();
  void InitAudio(const PrincipalHandle& aPrincipalHandle);
  void InitVideo(const PrincipalHandle& aPrincipalHandle);
  nsresult UpdateAudioConduit();
  nsresult UpdateVideoConduit();
  nsresult ConfigureVideoCodecMode(VideoSessionConduit& aConduit);
  void UpdateConduitRtpExtmap(const JsepTrackNegotiatedDetails& aDetails,
                              const MediaSessionConduitLocalDirection aDir);
  void Stop();

  const std::string mPCHandle;
  RefPtr<MediaTransportHandler> mTransportHandler;
  RefPtr<JsepTransceiver> mJsepTransceiver;
  std::string mMid;
  bool mHaveStartedReceiving;
  bool mHaveSetupTransport;
  nsCOMPtr<nsISerialEventTarget> mMainThread;
  nsCOMPtr<nsISerialEventTarget> mStsThread;
  RefPtr<dom::MediaStreamTrack> mReceiveTrack;
  RefPtr<dom::MediaStreamTrack> mSendTrack;
  // state for webrtc.org that is shared between all transceivers
  RefPtr<WebRtcCallWrapper> mCallWrapper;
  RefPtr<MediaSessionConduit> mConduit;
  RefPtr<MediaPipelineReceive> mReceivePipeline;
  RefPtr<MediaPipelineTransmit> mTransmitPipeline;
};

}  // namespace mozilla

#endif  // _TRANSCEIVERIMPL_H_
