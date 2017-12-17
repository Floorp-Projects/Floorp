/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _TRANSCEIVERIMPL_H_
#define _TRANSCEIVERIMPL_H_

#include <string>
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIEventTarget.h"
#include "nsTArray.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "ErrorList.h"
#include "mtransport/transportflow.h"
#include "signaling/src/jsep/JsepTransceiver.h"

class nsIPrincipal;

namespace mozilla {
class PeerIdentity;
class PeerConnectionMedia;
class JsepTransceiver;
class MediaSessionConduit;
class VideoSessionConduit;
class MediaPipelineReceive;
class MediaPipelineTransmit;
class MediaPipeline;
class MediaPipelineFilter;
class WebRtcCallWrapper;
class JsepTrackNegotiatedDetails;

namespace dom {
class RTCRtpTransceiver;
struct RTCRtpSourceEntry;
}

/**
 * This is what ties all the various pieces that make up a transceiver
 * together. This includes:
 * MediaStreamTrack for rendering and capture
 * TransportFlow for RTP transmission/reception
 * Audio/VideoConduit for feeding RTP/RTCP into webrtc.org for decoding, and
 * feeding audio/video frames into webrtc.org for encoding into RTP/RTCP.
*/
class TransceiverImpl : public nsISupports {
public:
  /**
   * |aReceiveStream| is always set; this holds even if the remote end has not
   * negotiated one for this transceiver. |aSendTrack| might or might not be
   * set.
   */
  TransceiverImpl(const std::string& aPCHandle,
                  JsepTransceiver* aJsepTransceiver,
                  nsIEventTarget* aMainThread,
                  nsIEventTarget* aStsThread,
                  dom::MediaStreamTrack* aReceiveTrack,
                  dom::MediaStreamTrack* aSendTrack,
                  WebRtcCallWrapper* aCallWrapper);

  bool IsValid() const
  {
    return !!mConduit;
  }

  nsresult UpdateSendTrack(dom::MediaStreamTrack* aSendTrack);

  nsresult UpdateSinkIdentity(const dom::MediaStreamTrack* aTrack,
                              nsIPrincipal* aPrincipal,
                              const PeerIdentity* aSinkIdentity);

  nsresult UpdateTransport(PeerConnectionMedia& aTransportManager);

  nsresult UpdateConduit();

  nsresult UpdatePrincipal(nsIPrincipal* aPrincipal);

  // TODO: We probably need to de-Sync when transceivers are stopped.
  nsresult SyncWithMatchingVideoConduits(
      std::vector<RefPtr<TransceiverImpl>>& transceivers);

  void Shutdown_m();

  bool ConduitHasPluginID(uint64_t aPluginID);

  bool HasSendTrack(const dom::MediaStreamTrack* aSendTrack) const;

  // This is so PCImpl can unregister from PrincipalChanged callbacks; maybe we
  // should have TransceiverImpl handle these callbacks instead? It would need
  // to be able to get a ref to PCImpl though.
  RefPtr<dom::MediaStreamTrack> GetSendTrack()
  {
    return mSendTrack;
  }

  // for webidl
  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector);
  already_AddRefed<dom::MediaStreamTrack> GetReceiveTrack();
  void SyncWithJS(dom::RTCRtpTransceiver& aJsTransceiver, ErrorResult& aRv);

  void InsertDTMFTone(int tone, uint32_t duration);

  bool HasReceiveTrack(const dom::MediaStreamTrack* aReceiveTrack) const;

  // TODO: These are for stats; try to find a cleaner way.
  RefPtr<MediaPipeline> GetSendPipeline();

  RefPtr<MediaPipeline> GetReceivePipeline();

  void AddRIDExtension(unsigned short aExtensionId);

  void AddRIDFilter(const nsAString& aRid);

  bool IsVideo() const;

  void GetRtpSources(const int64_t aTimeNow,
                     nsTArray<dom::RTCRtpSourceEntry>& outSources) const;

  // test-only: insert fake CSRCs and audio levels for testing
  void InsertAudioLevelForContributingSource(uint32_t aSource,
                                             int64_t aTimestamp,
                                             bool aHasLevel,
                                             uint8_t aLevel);

  NS_DECL_THREADSAFE_ISUPPORTS

private:
  virtual ~TransceiverImpl();
  void InitAudio();
  void InitVideo();
  nsresult UpdateAudioConduit();
  nsresult UpdateVideoConduit();
  nsresult ConfigureVideoCodecMode(VideoSessionConduit& aConduit);
  // This will eventually update audio extmap too
  void UpdateVideoExtmap(const JsepTrackNegotiatedDetails& aDetails,
                         bool aSending);
  void StartReceiveStream();
  void Stop();

  const std::string mPCHandle;
  RefPtr<JsepTransceiver> mJsepTransceiver;
  std::string mMid;
  bool mHaveStartedReceiving;
  bool mHaveSetupTransport;
  nsCOMPtr<nsIEventTarget> mMainThread;
  nsCOMPtr<nsIEventTarget> mStsThread;
  RefPtr<dom::MediaStreamTrack> mReceiveTrack;
  RefPtr<dom::MediaStreamTrack> mSendTrack;
  // state for webrtc.org that is shared between all transceivers
  RefPtr<WebRtcCallWrapper> mCallWrapper;
  RefPtr<TransportFlow> mRtpFlow;
  RefPtr<TransportFlow> mRtcpFlow;
  RefPtr<MediaSessionConduit> mConduit;
  RefPtr<MediaPipelineReceive> mReceivePipeline;
  RefPtr<MediaPipelineTransmit> mTransmitPipeline;
};

} // namespace mozilla

#endif // _TRANSCEIVERIMPL_H_

