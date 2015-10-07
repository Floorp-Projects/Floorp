/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _MEDIAPIPELINEFACTORY_H_
#define _MEDIAPIPELINEFACTORY_H_

#include "MediaConduitInterface.h"
#include "PeerConnectionMedia.h"
#include "transportflow.h"

#include "signaling/src/jsep/JsepTrack.h"
#include "mozilla/nsRefPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class MediaPipelineFactory
{
public:
  explicit MediaPipelineFactory(PeerConnectionMedia* aPCMedia)
      : mPCMedia(aPCMedia), mPC(aPCMedia->GetPC())
  {
  }

  nsresult CreateOrUpdateMediaPipeline(const JsepTrackPair& aTrackPair,
                                       const JsepTrack& aTrack);

private:
  nsresult CreateMediaPipelineReceiving(
      const JsepTrackPair& aTrackPair,
      const JsepTrack& aTrack,
      size_t level,
      nsRefPtr<TransportFlow> aRtpFlow,
      nsRefPtr<TransportFlow> aRtcpFlow,
      nsAutoPtr<MediaPipelineFilter> filter,
      const nsRefPtr<MediaSessionConduit>& aConduit);

  nsresult CreateMediaPipelineSending(
      const JsepTrackPair& aTrackPair,
      const JsepTrack& aTrack,
      size_t level,
      nsRefPtr<TransportFlow> aRtpFlow,
      nsRefPtr<TransportFlow> aRtcpFlow,
      nsAutoPtr<MediaPipelineFilter> filter,
      const nsRefPtr<MediaSessionConduit>& aConduit);

  nsresult GetOrCreateAudioConduit(const JsepTrackPair& aTrackPair,
                                   const JsepTrack& aTrack,
                                   nsRefPtr<MediaSessionConduit>* aConduitp);

  nsresult GetOrCreateVideoConduit(const JsepTrackPair& aTrackPair,
                                   const JsepTrack& aTrack,
                                   nsRefPtr<MediaSessionConduit>* aConduitp);

  MediaConduitErrorCode EnsureExternalCodec(VideoSessionConduit& aConduit,
                                            VideoCodecConfig* aConfig,
                                            bool aIsSend);

  nsresult CreateOrGetTransportFlow(size_t aLevel, bool aIsRtcp,
                                    const JsepTransport& transport,
                                    nsRefPtr<TransportFlow>* out);

  nsresult GetTransportParameters(const JsepTrackPair& aTrackPair,
                                  const JsepTrack& aTrack,
                                  size_t* aLevelOut,
                                  nsRefPtr<TransportFlow>* aRtpOut,
                                  nsRefPtr<TransportFlow>* aRtcpOut,
                                  nsAutoPtr<MediaPipelineFilter>* aFilterOut);

  nsresult ConfigureVideoCodecMode(const JsepTrack& aTrack,
                                   VideoSessionConduit& aConduit);

private:
  // Not owned, and assumed to exist as long as the factory.
  // The factory is a transient object, so this is fairly easy.
  PeerConnectionMedia* mPCMedia;
  PeerConnectionImpl* mPC;
};

} // namespace mozilla

#endif
