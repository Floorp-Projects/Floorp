/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef mediapipeline_h__
#define mediapipeline_h__

#include <map>

#include "sigslot.h"

#include "signaling/src/media-conduit/MediaConduitInterface.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Atomics.h"
#include "SrtpFlow.h"
#include "databuffer.h"
#include "mtransport/runnable_utils.h"
#include "mtransport/transportflow.h"
#include "AudioPacketizer.h"
#include "StreamTracks.h"
#include "signaling/src/peerconnection/PacketDumper.h"

#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"

// Should come from MediaEngine.h, but that's a pain to include here
// because of the MOZILLA_EXTERNAL_LINKAGE stuff.
#define WEBRTC_DEFAULT_SAMPLE_RATE 32000

class nsIPrincipal;

namespace mozilla {
class MediaPipelineFilter;
class PeerIdentity;
class AudioProxyThread;
class VideoFrameConverter;

namespace dom {
  class MediaStreamTrack;
  struct RTCRTPContributingSourceStats;
} // namespace dom

class SourceMediaStream;

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
// MediaStreamGraph
//   * Receives outgoing data from the MediaStreamGraph
//   * Receives pull requests for more data from the
//     MediaStreamGraph
// One or another GIPS threads
//   * Receives RTCP messages to send to the other side
//   * Processes video frames GIPS wants to render
//
// For a transmitting conduit, "output" is RTP and "input" is RTCP.
// For a receiving conduit, "input" is RTP and "output" is RTCP.
//

class MediaPipeline : public sigslot::has_slots<> {
 public:
  enum Direction { TRANSMIT, RECEIVE };
  enum State { MP_CONNECTING, MP_OPEN, MP_CLOSED };
  MediaPipeline(const std::string& pc,
                Direction direction,
                nsCOMPtr<nsIEventTarget> main_thread,
                nsCOMPtr<nsIEventTarget> sts_thread,
                RefPtr<MediaSessionConduit> conduit);

  virtual void Start() = 0;
  virtual void Stop() = 0;
  virtual void DetachMedia() {}

  void SetLevel(size_t level) { level_ = level; }

  // Must be called on the main thread.
  void Shutdown_m();

  void UpdateTransport_m(RefPtr<TransportFlow> rtp_transport,
                         RefPtr<TransportFlow> rtcp_transport,
                         nsAutoPtr<MediaPipelineFilter> filter);

  void UpdateTransport_s(RefPtr<TransportFlow> rtp_transport,
                         RefPtr<TransportFlow> rtcp_transport,
                         nsAutoPtr<MediaPipelineFilter> filter);

  // Used only for testing; adds RTP header extension for RTP Stream Id with
  // the given id.
  void AddRIDExtension_m(size_t extension_id);
  void AddRIDExtension_s(size_t extension_id);
  // Used only for testing; installs a MediaPipelineFilter that filters
  // everything but the given RID
  void AddRIDFilter_m(const std::string& rid);
  void AddRIDFilter_s(const std::string& rid);

  virtual Direction direction() const { return direction_; }
  int level() const { return level_; }
  virtual bool IsVideo() const = 0;

  bool IsDoingRtcpMux() const {
    return (rtp_.type_ == MUX);
  }

  class RtpCSRCStats {
  public:
    // Gets an expiration time for CRC info given a reference time,
    //   this reference time would normally be the time of calling.
    //   This value can then be used to check if a RtpCSRCStats
    //   has expired via Expired(...)
    static DOMHighResTimeStamp
    GetExpiryFromTime(const DOMHighResTimeStamp aTime);

    RtpCSRCStats(const uint32_t aCsrc,
                 const DOMHighResTimeStamp aTime);
    ~RtpCSRCStats() {};
    // Initialize a webidl representation suitable for adding to a report.
    //   This assumes that the webidl object is empty.
    // @param aWebidlObj the webidl binding object to popluate
    // @param aRtpInboundStreamId the associated RTCInboundRTPStreamStats.id
    void
    GetWebidlInstance(dom::RTCRTPContributingSourceStats& aWebidlObj,
                             const nsString &aInboundRtpStreamId) const;
    void SetTimestamp(const DOMHighResTimeStamp aTime) { mTimestamp = aTime; }
    // Check if the RtpCSRCStats has expired, checks against a
    //   given expiration time.
    bool Expired(const DOMHighResTimeStamp aExpiry) const {
      return mTimestamp < aExpiry;
    }
  private:
    static const double constexpr EXPIRY_TIME_MILLISECONDS = 10 * 1000;
    uint32_t mCsrc;
    DOMHighResTimeStamp mTimestamp;
  };

  // Gets the gathered contributing source stats for the last expiration period.
  // @param aId the stream id to use for populating inboundRtpStreamId field
  // @param aArr the array to append the stats objects to
  void
  GetContributingSourceStats(
      const nsString& aInboundStreamId,
      FallibleTArray<dom::RTCRTPContributingSourceStats>& aArr) const;

  int32_t rtp_packets_sent() const { return rtp_packets_sent_; }
  int64_t rtp_bytes_sent() const { return rtp_bytes_sent_; }
  int32_t rtcp_packets_sent() const { return rtcp_packets_sent_; }
  int32_t rtp_packets_received() const { return rtp_packets_received_; }
  int64_t rtp_bytes_received() const { return rtp_bytes_received_; }
  int32_t rtcp_packets_received() const { return rtcp_packets_received_; }

  MediaSessionConduit *Conduit() const { return conduit_; }

  // Thread counting
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaPipeline)

  typedef enum {
    RTP,
    RTCP,
    MUX,
    MAX_RTP_TYPE
  } RtpType;

  // Separate class to allow ref counting
  class PipelineTransport : public TransportInterface {
   public:
    // Implement the TransportInterface functions
    explicit PipelineTransport(MediaPipeline *pipeline)
        : pipeline_(pipeline),
          sts_thread_(pipeline->sts_thread_) {}

    void Attach(MediaPipeline *pipeline) { pipeline_ = pipeline; }
    void Detach() { pipeline_ = nullptr; }
    MediaPipeline *pipeline() const { return pipeline_; }

    virtual nsresult SendRtpPacket(const uint8_t* data, size_t len);
    virtual nsresult SendRtcpPacket(const uint8_t* data, size_t len);

   private:
    nsresult SendRtpRtcpPacket_s(nsAutoPtr<DataBuffer> data,
                                 bool is_rtp);

    // Creates a cycle, which we break with Detach
    RefPtr<MediaPipeline> pipeline_;
    nsCOMPtr<nsIEventTarget> sts_thread_;
  };

 protected:
  virtual ~MediaPipeline();
  nsresult AttachTransport_s();
  friend class PipelineTransport;

  class TransportInfo {
    public:
      TransportInfo(RefPtr<TransportFlow> flow, RtpType type) :
        transport_(flow),
        state_(MP_CONNECTING),
        type_(type) {
      }

      void Detach()
      {
        transport_ = nullptr;
        send_srtp_ = nullptr;
        recv_srtp_ = nullptr;
      }

      RefPtr<TransportFlow> transport_;
      State state_;
      RefPtr<SrtpFlow> send_srtp_;
      RefPtr<SrtpFlow> recv_srtp_;
      RtpType type_;
  };

  // The transport is down
  virtual nsresult TransportFailed_s(TransportInfo &info);
  // The transport is ready
  virtual nsresult TransportReady_s(TransportInfo &info);
  void UpdateRtcpMuxState(TransportInfo &info);

  nsresult ConnectTransport_s(TransportInfo &info);

  TransportInfo* GetTransportInfo_s(TransportFlow *flow);

  void increment_rtp_packets_sent(int bytes);
  void increment_rtcp_packets_sent();
  void increment_rtp_packets_received(int bytes);
  virtual void OnRtpPacketReceived() {};
  void increment_rtcp_packets_received();

  virtual nsresult SendPacket(TransportFlow *flow, const void *data, int len);

  // Process slots on transports
  void StateChange(TransportFlow *flow, TransportLayer::State);
  void RtpPacketReceived(TransportLayer *layer, const unsigned char *data,
                         size_t len);
  void RtcpPacketReceived(TransportLayer *layer, const unsigned char *data,
                          size_t len);
  void PacketReceived(TransportLayer *layer, const unsigned char *data,
                      size_t len);

  Direction direction_;
  size_t level_;
  RefPtr<MediaSessionConduit> conduit_;  // Our conduit. Written on the main
                                         // thread. Read on STS thread.

  // The transport objects. Read/written on STS thread.
  TransportInfo rtp_;
  TransportInfo rtcp_;

  // Pointers to the threads we need. Initialized at creation
  // and used all over the place.
  nsCOMPtr<nsIEventTarget> main_thread_;
  nsCOMPtr<nsIEventTarget> sts_thread_;

  // Created in c'tor. Referenced by the conduit.
  RefPtr<PipelineTransport> transport_;

  // Only safe to access from STS thread.
  // Build into TransportInfo?
  int32_t rtp_packets_sent_;
  int32_t rtcp_packets_sent_;
  int32_t rtp_packets_received_;
  int32_t rtcp_packets_received_;
  int64_t rtp_bytes_sent_;
  int64_t rtp_bytes_received_;

  // Only safe to access from STS thread.
  std::map<uint32_t, RtpCSRCStats> csrc_stats_;

  // Written in c'tor. Read on STS thread.
  std::string pc_;
  std::string description_;

  // Written in c'tor, all following accesses are on the STS thread.
  nsAutoPtr<MediaPipelineFilter> filter_;
  nsAutoPtr<webrtc::RtpHeaderParser> rtp_parser_;

  nsAutoPtr<PacketDumper> packet_dumper_;

 private:
  // Gets the current time as a DOMHighResTimeStamp
  static DOMHighResTimeStamp GetNow();

  bool IsRtp(const unsigned char *data, size_t len);
  // Must be called on the STS thread.  Must be called after DetachMedia().
  void DetachTransport_s();
};

class ConduitDeleteEvent: public Runnable
{
public:
  explicit ConduitDeleteEvent(already_AddRefed<MediaSessionConduit> aConduit) :
    Runnable("ConduitDeleteEvent"),
    mConduit(aConduit) {}

  /* we exist solely to proxy release of the conduit */
  NS_IMETHOD Run() override { return NS_OK; }
private:
  RefPtr<MediaSessionConduit> mConduit;
};

// A specialization of pipeline for reading from an input device
// and transmitting to the network.
class MediaPipelineTransmit : public MediaPipeline {
public:
  // Set rtcp_transport to nullptr to use rtcp-mux
  MediaPipelineTransmit(const std::string& pc,
                        nsCOMPtr<nsIEventTarget> main_thread,
                        nsCOMPtr<nsIEventTarget> sts_thread,
                        bool is_video,
                        dom::MediaStreamTrack* domtrack,
                        RefPtr<MediaSessionConduit> conduit);

  void Start() override;
  void Stop() override;

  // written and used from MainThread
  bool IsVideo() const override;

  // When the principal of the domtrack changes, it calls through to here
  // so that we can determine whether to enable track transmission.
  // `track` has to be null or equal `domtrack_` for us to apply the update.
  virtual void UpdateSinkIdentity_m(dom::MediaStreamTrack* track,
                                    nsIPrincipal* principal,
                                    const PeerIdentity* sinkIdentity);

  // Called on the main thread.
  void DetachMedia() override;

  // Override MediaPipeline::TransportReady.
  nsresult TransportReady_s(TransportInfo &info) override;

  // Replace a track with a different one
  // In non-compliance with the likely final spec, allow the new
  // track to be part of a different stream (since we don't support
  // multiple tracks of a type in a stream yet).  bug 1056650
  virtual nsresult ReplaceTrack(RefPtr<dom::MediaStreamTrack>& domtrack);

  // Separate classes to allow ref counting
  class PipelineListener;
  class VideoFrameFeeder;

 protected:
  ~MediaPipelineTransmit();

  void SetDescription();

 private:
  RefPtr<PipelineListener> listener_;
  RefPtr<AudioProxyThread> audio_processing_;
  RefPtr<VideoFrameFeeder> feeder_;
  RefPtr<VideoFrameConverter> converter_;
  bool is_video_;
  RefPtr<dom::MediaStreamTrack> domtrack_;
  bool transmitting_;
};


// A specialization of pipeline for reading from the network and
// rendering media.
class MediaPipelineReceive : public MediaPipeline {
 public:
  // Set rtcp_transport to nullptr to use rtcp-mux
  MediaPipelineReceive(const std::string& pc,
                       nsCOMPtr<nsIEventTarget> main_thread,
                       nsCOMPtr<nsIEventTarget> sts_thread,
                       RefPtr<MediaSessionConduit> conduit);

  int segments_added() const { return segments_added_; }

  // Sets the PrincipalHandle we set on the media chunks produced by this
  // pipeline. Must be called on the main thread.
  virtual void SetPrincipalHandle_m(const PrincipalHandle& principal_handle) = 0;

 protected:
  ~MediaPipelineReceive();

  int segments_added_;

 private:
};


// A specialization of pipeline for reading from the network and
// rendering audio.
class MediaPipelineReceiveAudio : public MediaPipelineReceive {
 public:
  MediaPipelineReceiveAudio(const std::string& pc,
                            nsCOMPtr<nsIEventTarget> main_thread,
                            nsCOMPtr<nsIEventTarget> sts_thread,
                            RefPtr<AudioSessionConduit> conduit,
                            dom::MediaStreamTrack* aTrack);

  void DetachMedia() override;

  bool IsVideo() const override { return false; }

  void SetPrincipalHandle_m(const PrincipalHandle& principal_handle) override;

  void Start() override;
  void Stop() override;

  void OnRtpPacketReceived() override;

 private:
  // Separate class to allow ref counting
  class PipelineListener;

  RefPtr<PipelineListener> listener_;
};


// A specialization of pipeline for reading from the network and
// rendering video.
class MediaPipelineReceiveVideo : public MediaPipelineReceive {
 public:
  MediaPipelineReceiveVideo(const std::string& pc,
                            nsCOMPtr<nsIEventTarget> main_thread,
                            nsCOMPtr<nsIEventTarget> sts_thread,
                            RefPtr<VideoSessionConduit> conduit,
                            dom::MediaStreamTrack* aTrack);

  // Called on the main thread.
  void DetachMedia() override;

  bool IsVideo() const override { return true; }

  void SetPrincipalHandle_m(const PrincipalHandle& principal_handle) override;

  void Start() override;
  void Stop() override;

  void OnRtpPacketReceived() override;

 private:
  class PipelineRenderer;
  friend class PipelineRenderer;

  // Separate class to allow ref counting
  class PipelineListener;

  RefPtr<PipelineRenderer> renderer_;
  RefPtr<PipelineListener> listener_;
};


}  // namespace mozilla
#endif
