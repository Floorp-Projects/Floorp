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

class nsIPrincipal;

namespace mozilla {
class PeerIdentity;
class JsepTransceiver;
enum class MediaSessionConduitLocalDirection : int;
class MediaSessionConduit;
class VideoSessionConduit;
class AudioSessionConduit;
struct AudioCodecConfig;
class VideoCodecConfig;  // Why is this a class, but AudioCodecConfig a struct?
class MediaPipelineTransmit;
class MediaPipeline;
class MediaPipelineFilter;
class MediaTransportHandler;
class WebRtcCallWrapper;
class JsepTrackNegotiatedDetails;

namespace dom {
class RTCDTMFSender;
class RTCRtpTransceiver;
struct RTCRtpSourceEntry;
class RTCRtpReceiver;
}  // namespace dom

/**
 * This is what ties all the various pieces that make up a transceiver
 * together. This includes:
 * MediaStreamTrack for rendering and capture
 * MediaTransportHandler for RTP transmission/reception
 * Audio/VideoConduit for feeding RTP/RTCP into webrtc.org for decoding, and
 * feeding audio/video frames into webrtc.org for encoding into RTP/RTCP.
 */
class TransceiverImpl : public nsISupports, public nsWrapperCache {
 public:
  /**
   * |aSendTrack| might or might not be set.
   */
  TransceiverImpl(nsPIDOMWindowInner* aWindow, bool aPrivacyNeeded,
                  const std::string& aPCHandle,
                  MediaTransportHandler* aTransportHandler,
                  JsepTransceiver* aJsepTransceiver,
                  nsISerialEventTarget* aMainThread,
                  nsISerialEventTarget* aStsThread,
                  dom::MediaStreamTrack* aSendTrack,
                  WebRtcCallWrapper* aCallWrapper);

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
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsPIDOMWindowInner* GetParentObject() const;
  void SyncWithJS(dom::RTCRtpTransceiver& aJsTransceiver, ErrorResult& aRv);
  dom::RTCRtpReceiver* Receiver() const { return mReceiver; }
  dom::RTCDTMFSender* GetDtmf() const { return mDtmf; }

  bool CanSendDTMF() const;

  // TODO: These are for stats; try to find a cleaner way.
  RefPtr<MediaPipelineTransmit> GetSendPipeline();

  std::string GetTransportId() const {
    return mJsepTransceiver->mTransport.mTransportId;
  }

  bool IsVideo() const;

  bool IsSending() const {
    return !mJsepTransceiver->IsStopped() &&
           mJsepTransceiver->mSendTrack.GetActive();
  }

  bool IsReceiving() const {
    return !mJsepTransceiver->IsStopped() &&
           mJsepTransceiver->mRecvTrack.GetActive();
  }

  void GetRtpSources(const int64_t aTimeNow,
                     nsTArray<dom::RTCRtpSourceEntry>& outSources) const;

  MediaSessionConduit* GetConduit() const { return mConduit; }

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TransceiverImpl)

  static nsresult NegotiatedDetailsToAudioCodecConfigs(
      const JsepTrackNegotiatedDetails& aDetails,
      std::vector<UniquePtr<AudioCodecConfig>>* aConfigs);

  static nsresult NegotiatedDetailsToVideoCodecConfigs(
      const JsepTrackNegotiatedDetails& aDetails,
      std::vector<UniquePtr<VideoCodecConfig>>* aConfigs);

  static void UpdateConduitRtpExtmap(
      MediaSessionConduit& aConduit, const JsepTrackNegotiatedDetails& aDetails,
      const MediaSessionConduitLocalDirection aDir);

 private:
  virtual ~TransceiverImpl();
  void InitAudio();
  void InitVideo();
  nsresult UpdateAudioConduit();
  nsresult UpdateVideoConduit();
  nsresult ConfigureVideoCodecMode(VideoSessionConduit& aConduit);
  void Stop();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  const std::string mPCHandle;
  RefPtr<MediaTransportHandler> mTransportHandler;
  RefPtr<JsepTransceiver> mJsepTransceiver;
  std::string mMid;
  bool mHaveSetupTransport;
  nsCOMPtr<nsISerialEventTarget> mMainThread;
  nsCOMPtr<nsISerialEventTarget> mStsThread;
  RefPtr<dom::MediaStreamTrack> mSendTrack;
  // state for webrtc.org that is shared between all transceivers
  RefPtr<WebRtcCallWrapper> mCallWrapper;
  RefPtr<MediaSessionConduit> mConduit;
  RefPtr<MediaPipelineTransmit> mTransmitPipeline;
  RefPtr<dom::RTCRtpReceiver> mReceiver;
  // TODO(bug 1616937): Move this to RTCRtpSender
  RefPtr<dom::RTCDTMFSender> mDtmf;
};

}  // namespace mozilla

#endif  // _TRANSCEIVERIMPL_H_
