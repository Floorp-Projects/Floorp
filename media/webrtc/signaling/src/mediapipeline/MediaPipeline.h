/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef mediapipeline_h__
#define mediapipeline_h__

#include <map>

#include "sigslot.h"
#include "transportlayer.h"  // For TransportLayer::State

#include "signaling/src/media-conduit/MediaConduitInterface.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Atomics.h"
#include "SrtpFlow.h"  // For SRTP_MAX_EXPANSION
#include "mediapacket.h"
#include "mtransport/runnable_utils.h"
#include "AudioPacketizer.h"
#include "MediaPipelineFilter.h"
#include "MediaSegment.h"
#include "signaling/src/peerconnection/PacketDumper.h"

#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"

// Should come from MediaEngine.h, but that's a pain to include here
// because of the MOZILLA_EXTERNAL_LINKAGE stuff.
#define WEBRTC_MAX_SAMPLE_RATE 48000

class nsIPrincipal;

namespace mozilla {
class AudioProxyThread;
class MediaInputPort;
class MediaPipelineFilter;
class MediaTransportHandler;
class PeerIdentity;
class ProcessedMediaTrack;
class SourceMediaTrack;
class VideoFrameConverter;

namespace dom {
class MediaStreamTrack;
struct RTCRTPContributingSourceStats;
}  // namespace dom

// A class that represents the pipeline of audio and video
// The dataflow looks like:
//
// TRANSMIT
// CaptureDevice -> stream -> [us] -> conduit -> [us] -> transport -> network
//
// RECEIVE
// network -> transport -> [us] -> conduit -> [us] -> stream -> Playout
//
// The boxes labeled [us] are just bridge logic implemented in this class
//
// We have to deal with a number of threads:
//
// GSM:
//   * Assembles the pipeline
// SocketTransportService
//   * Receives notification that ICE and DTLS have completed
//   * Processes incoming network data and passes it to the conduit
//   * Processes outgoing RTP and RTCP
// MediaTrackGraph
//   * Receives outgoing data from the MediaTrackGraph
//   * Receives pull requests for more data from the
//     MediaTrackGraph
// One or another GIPS threads
//   * Receives RTCP messages to send to the other side
//   * Processes video frames GIPS wants to render
//
// For a transmitting conduit, "output" is RTP and "input" is RTCP.
// For a receiving conduit, "input" is RTP and "output" is RTCP.
//

class MediaPipeline : public sigslot::has_slots<> {
 public:
  enum class DirectionType { TRANSMIT, RECEIVE };
  MediaPipeline(const std::string& aPc,
                RefPtr<MediaTransportHandler> aTransportHandler,
                DirectionType aDirection,
                RefPtr<nsISerialEventTarget> aMainThread,
                RefPtr<nsISerialEventTarget> aStsThread,
                RefPtr<MediaSessionConduit> aConduit);

  virtual void Start() = 0;
  virtual RefPtr<GenericPromise> Stop() = 0;
  virtual void DetachMedia() {}

  void SetLevel(size_t aLevel) { mLevel = aLevel; }

  // Must be called on the main thread.
  void Shutdown_m();

  void UpdateTransport_m(const std::string& aTransportId,
                         UniquePtr<MediaPipelineFilter>&& aFilter);

  void UpdateTransport_s(const std::string& aTransportId,
                         UniquePtr<MediaPipelineFilter>&& aFilter);

  // Used only for testing; adds RTP header extension for RTP Stream Id with
  // the given id.
  void AddRIDExtension_m(size_t aExtensionId);
  void AddRIDExtension_s(size_t aExtensionId);
  // Used only for testing; installs a MediaPipelineFilter that filters
  // everything but the given RID
  void AddRIDFilter_m(const std::string& aRid);
  void AddRIDFilter_s(const std::string& aRid);

  virtual DirectionType Direction() const { return mDirection; }
  int Level() const { return mLevel; }
  virtual bool IsVideo() const = 0;

  class RtpCSRCStats {
   public:
    // Gets an expiration time for CRC info given a reference time,
    //   this reference time would normally be the time of calling.
    //   This value can then be used to check if a RtpCSRCStats
    //   has expired via Expired(...)
    static DOMHighResTimeStamp GetExpiryFromTime(
        const DOMHighResTimeStamp aTime);

    RtpCSRCStats(const uint32_t aCsrc, const DOMHighResTimeStamp aTime);
    ~RtpCSRCStats() = default;
    // Initialize a webidl representation suitable for adding to a report.
    //   This assumes that the webidl object is empty.
    // @param aWebidlObj the webidl binding object to popluate
    // @param aInboundRtpStreamId the associated RTCInboundRTPStreamStats.id
    void GetWebidlInstance(dom::RTCRTPContributingSourceStats& aWebidlObj,
                           const nsString& aInboundRtpStreamId) const;
    void SetTimestamp(const DOMHighResTimeStamp aTime) { mTimestamp = aTime; }
    // Check if the RtpCSRCStats has expired, checks against a
    //   given expiration time.
    bool Expired(const DOMHighResTimeStamp aExpiry) const {
      return mTimestamp < aExpiry;
    }

   private:
    static const double constexpr EXPIRY_TIME_MILLISECONDS = 10 * 1000;
    const uint32_t mCsrc;
    DOMHighResTimeStamp mTimestamp;
  };

  // Gets the gathered contributing source stats for the last expiration period.
  // @param aId the stream id to use for populating inboundRtpStreamId field
  // @param aArr the array to append the stats objects to
  void GetContributingSourceStats(
      const nsString& aInboundRtpStreamId,
      FallibleTArray<dom::RTCRTPContributingSourceStats>& aArr) const;

  int32_t RtpPacketsSent() const { return mRtpPacketsSent; }
  int64_t RtpBytesSent() const { return mRtpBytesSent; }
  int32_t RtcpPacketsSent() const { return mRtcpPacketsSent; }
  int32_t RtpPacketsReceived() const { return mRtpPacketsReceived; }
  int64_t RtpBytesReceived() const { return mRtpBytesReceived; }
  int32_t RtcpPacketsReceived() const { return mRtcpPacketsReceived; }
  // Gets the current time as a DOMHighResTimeStamp
  DOMHighResTimeStamp GetNow() const;

  MediaSessionConduit* Conduit() const { return mConduit; }

  // Thread counting
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaPipeline)

  // Separate class to allow ref counting
  class PipelineTransport : public TransportInterface {
   public:
    // Implement the TransportInterface functions
    explicit PipelineTransport(RefPtr<nsISerialEventTarget> aStsThread)
        : mPipeline(nullptr), mStsThread(std::move(aStsThread)) {}

    void Attach(MediaPipeline* pipeline) { mPipeline = pipeline; }
    void Detach() { mPipeline = nullptr; }
    MediaPipeline* Pipeline() const { return mPipeline; }

    virtual nsresult SendRtpPacket(const uint8_t* aData, size_t aLen) override;
    virtual nsresult SendRtcpPacket(const uint8_t* aData, size_t aLen) override;

   private:
    void SendRtpRtcpPacket_s(MediaPacket&& aPacket);

    // Creates a cycle, which we break with Detach
    RefPtr<MediaPipeline> mPipeline;
    const RefPtr<nsISerialEventTarget> mStsThread;
  };

 protected:
  virtual ~MediaPipeline();
  friend class PipelineTransport;

  // The transport is ready
  virtual void TransportReady_s() {}

  void IncrementRtpPacketsSent(int aBytes);
  void IncrementRtcpPacketsSent();
  void IncrementRtpPacketsReceived(int aBytes);
  virtual void OnRtpPacketReceived() {}
  void IncrementRtcpPacketsReceived();

  virtual void SendPacket(MediaPacket&& packet);

  // Process slots on transports
  void RtpStateChange(const std::string& aTransportId, TransportLayer::State);
  void RtcpStateChange(const std::string& aTransportId, TransportLayer::State);
  virtual void CheckTransportStates();
  void PacketReceived(const std::string& aTransportId,
                      const MediaPacket& packet);
  void AlpnNegotiated(const std::string& aAlpn, bool aPrivacyRequested);

  void RtpPacketReceived(const MediaPacket& packet);
  void RtcpPacketReceived(const MediaPacket& packet);

  void EncryptedPacketSending(const std::string& aTransportId,
                              const MediaPacket& aPacket);

  void SetDescription_s(const std::string& description);

  // Called when ALPN is negotiated and is requesting privacy, so receive
  // pipelines do not enter data into the graph under a content principal.
  virtual void MakePrincipalPrivate_s() {}

  const DirectionType mDirection;
  size_t mLevel;
  std::string mTransportId;
  const RefPtr<MediaTransportHandler> mTransportHandler;
  RefPtr<MediaSessionConduit> mConduit;  // Our conduit. Written on the main
                                         // thread. Read on STS thread.

  TransportLayer::State mRtpState = TransportLayer::TS_NONE;
  TransportLayer::State mRtcpState = TransportLayer::TS_NONE;
  bool mSignalsConnected = false;

  // Pointers to the threads we need. Initialized at creation
  // and used all over the place.
  const RefPtr<nsISerialEventTarget> mMainThread;
  const RefPtr<nsISerialEventTarget> mStsThread;

  // Created in c'tor. Referenced by the conduit.
  const RefPtr<PipelineTransport> mTransport;

  // Only safe to access from STS thread.
  int32_t mRtpPacketsSent;
  int32_t mRtcpPacketsSent;
  int32_t mRtpPacketsReceived;
  int32_t mRtcpPacketsReceived;
  int64_t mRtpBytesSent;
  int64_t mRtpBytesReceived;

  // Only safe to access from STS thread.
  std::map<uint32_t, RtpCSRCStats> mCsrcStats;

  // Written in c'tor. Read on STS thread.
  const std::string mPc;
  std::string mDescription;

  // Written in c'tor, all following accesses are on the STS thread.
  UniquePtr<MediaPipelineFilter> mFilter;
  const UniquePtr<webrtc::RtpHeaderParser> mRtpParser;

  UniquePtr<PacketDumper> mPacketDumper;

 private:
  bool IsRtp(const unsigned char* aData, size_t aLen) const;
  // Must be called on the STS thread.  Must be called after DetachMedia().
  void DetachTransport_s();
};

// A specialization of pipeline for reading from an input device
// and transmitting to the network.
class MediaPipelineTransmit : public MediaPipeline {
 public:
  // Set aRtcpTransport to nullptr to use rtcp-mux
  MediaPipelineTransmit(const std::string& aPc,
                        RefPtr<MediaTransportHandler> aTransportHandler,
                        RefPtr<nsISerialEventTarget> aMainThread,
                        RefPtr<nsISerialEventTarget> aStsThread, bool aIsVideo,
                        RefPtr<MediaSessionConduit> aConduit);

  bool Transmitting() const;

  void Start() override;
  RefPtr<GenericPromise> Stop() override;

  // written and used from MainThread
  bool IsVideo() const override;

  // When the principal of the domtrack changes, it calls through to here
  // so that we can determine whether to enable track transmission.
  // `track` has to be null or equal `mDomTrack` for us to apply the update.
  virtual void UpdateSinkIdentity_m(const dom::MediaStreamTrack* aTrack,
                                    nsIPrincipal* aPrincipal,
                                    const PeerIdentity* aSinkIdentity);

  // Called on the main thread.
  void DetachMedia() override;

  // Override MediaPipeline::TransportReady_s.
  void TransportReady_s() override;

  // Replace a track with a different one.
  nsresult SetTrack(RefPtr<dom::MediaStreamTrack> aDomTrack);

  // Set the track whose data we will transmit. For internal and test use.
  void SetSendTrack(RefPtr<ProcessedMediaTrack> aSendTrack);

  // Separate classes to allow ref counting
  class PipelineListener;
  class PipelineListenerTrackConsumer;
  class VideoFrameFeeder;

 protected:
  ~MediaPipelineTransmit();

  void SetDescription();

 private:
  void AsyncStart(const RefPtr<GenericPromise>& aPromise);

  const bool mIsVideo;
  const RefPtr<PipelineListener> mListener;
  // Listens for changes in enabled state on the attached MediaStreamTrack, and
  // notifies mListener.
  const nsMainThreadPtrHandle<PipelineListenerTrackConsumer> mTrackConsumer;
  const RefPtr<VideoFrameFeeder> mFeeder;
  RefPtr<AudioProxyThread> mAudioProcessing;
  RefPtr<VideoFrameConverter> mConverter;
  RefPtr<dom::MediaStreamTrack> mDomTrack;
  // Input port connecting mDomTrack's MediaTrack to mSendTrack.
  RefPtr<MediaInputPort> mSendPort;
  // MediaTrack that we send over the network. This allows changing mDomTrack.
  RefPtr<ProcessedMediaTrack> mSendTrack;
  // True if we're actively transmitting data to the network. Main thread only.
  bool mTransmitting;
  // When AsyncStart() is used this flag helps to avoid unexpected starts. One
  // case is that a start has already been scheduled. A second case is that a
  // start has already taken place (from JS for example). A third case is that
  // a stop has taken place so we want to cancel the start. Main thread only.
  bool mAsyncStartRequested;
};

// A specialization of pipeline for reading from the network and
// rendering media.
class MediaPipelineReceive : public MediaPipeline {
 public:
  // Set aRtcpTransport to nullptr to use rtcp-mux
  MediaPipelineReceive(const std::string& aPc,
                       RefPtr<MediaTransportHandler> aTransportHandler,
                       RefPtr<nsISerialEventTarget> aMainThread,
                       RefPtr<nsISerialEventTarget> aStsThread,
                       RefPtr<MediaSessionConduit> aConduit);

 protected:
  ~MediaPipelineReceive();
};

// A specialization of pipeline for reading from the network and
// rendering audio.
class MediaPipelineReceiveAudio : public MediaPipelineReceive {
 public:
  MediaPipelineReceiveAudio(const std::string& aPc,
                            RefPtr<MediaTransportHandler> aTransportHandler,
                            RefPtr<nsISerialEventTarget> aMainThread,
                            RefPtr<nsISerialEventTarget> aStsThread,
                            RefPtr<AudioSessionConduit> aConduit,
                            const RefPtr<dom::MediaStreamTrack>& aTrack,
                            const PrincipalHandle& aPrincipalHandle);

  void DetachMedia() override;

  bool IsVideo() const override { return false; }

  void MakePrincipalPrivate_s() override;

  void Start() override;
  RefPtr<GenericPromise> Stop() override;

  void OnRtpPacketReceived() override;

 private:
  // Separate class to allow ref counting
  class PipelineListener;

  const RefPtr<PipelineListener> mListener;
};

// A specialization of pipeline for reading from the network and
// rendering video.
class MediaPipelineReceiveVideo : public MediaPipelineReceive {
 public:
  MediaPipelineReceiveVideo(const std::string& aPc,
                            RefPtr<MediaTransportHandler> aTransportHandler,
                            RefPtr<nsISerialEventTarget> aMainThread,
                            RefPtr<nsISerialEventTarget> aStsThread,
                            RefPtr<VideoSessionConduit> aConduit,
                            const RefPtr<dom::MediaStreamTrack>& aTrack,
                            const PrincipalHandle& aPrincipalHandle);

  // Called on the main thread.
  void DetachMedia() override;

  bool IsVideo() const override { return true; }

  void MakePrincipalPrivate_s() override;

  void Start() override;
  RefPtr<GenericPromise> Stop() override;

  void OnRtpPacketReceived() override;

 private:
  class PipelineRenderer;
  friend class PipelineRenderer;

  // Separate class to allow ref counting
  class PipelineListener;

  const RefPtr<PipelineRenderer> mRenderer;
  const RefPtr<PipelineListener> mListener;
};

}  // namespace mozilla
#endif
