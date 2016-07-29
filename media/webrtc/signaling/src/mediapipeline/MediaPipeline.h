/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef mediapipeline_h__
#define mediapipeline_h__

#include "sigslot.h"

#ifdef USE_FAKE_MEDIA_STREAMS
#include "FakeMediaStreams.h"
#endif
#include "MediaConduitInterface.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Atomics.h"
#include "SrtpFlow.h"
#include "databuffer.h"
#include "runnable_utils.h"
#include "transportflow.h"
#include "AudioPacketizer.h"
#include "StreamTracks.h"

#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"

// Should come from MediaEngine.h, but that's a pain to include here
// because of the MOZILLA_EXTERNAL_LINKAGE stuff.
#define WEBRTC_DEFAULT_SAMPLE_RATE 32000

class nsIPrincipal;

namespace mozilla {
class MediaPipelineFilter;
class PeerIdentity;
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
class VideoFrameConverter;
#endif

#ifndef USE_FAKE_MEDIA_STREAMS
namespace dom {
  class MediaStreamTrack;
} // namespace dom

class SourceMediaStream;
#endif // USE_FAKE_MEDIA_STREAMS

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
                const std::string& track_id,
                int level,
                RefPtr<MediaSessionConduit> conduit,
                RefPtr<TransportFlow> rtp_transport,
                RefPtr<TransportFlow> rtcp_transport,
                nsAutoPtr<MediaPipelineFilter> filter);

  // Must be called on the STS thread.  Must be called after ShutdownMedia_m().
  void DetachTransport_s();

  // Must be called on the main thread.
  void ShutdownMedia_m()
  {
    ASSERT_ON_THREAD(main_thread_);

    if (direction_ == RECEIVE) {
      conduit_->StopReceiving();
    } else {
      conduit_->StopTransmitting();
    }
    DetachMedia();
  }

  virtual nsresult Init();

  void UpdateTransport_m(int level,
                         RefPtr<TransportFlow> rtp_transport,
                         RefPtr<TransportFlow> rtcp_transport,
                         nsAutoPtr<MediaPipelineFilter> filter);

  void UpdateTransport_s(int level,
                         RefPtr<TransportFlow> rtp_transport,
                         RefPtr<TransportFlow> rtcp_transport,
                         nsAutoPtr<MediaPipelineFilter> filter);

  // Used only for testing; installs a MediaPipelineFilter that filters
  // everything but the nth ssrc
  void SelectSsrc_m(size_t ssrc_index);
  void SelectSsrc_s(size_t ssrc_index);

  virtual Direction direction() const { return direction_; }
  virtual const std::string& trackid() const { return track_id_; }
  virtual int level() const { return level_; }
  virtual bool IsVideo() const = 0;

  bool IsDoingRtcpMux() const {
    return (rtp_.type_ == MUX);
  }

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

 protected:
  virtual ~MediaPipeline();
  virtual void DetachMedia() {}
  nsresult AttachTransport_s();

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

    virtual nsresult SendRtpPacket(const void* data, int len);
    virtual nsresult SendRtcpPacket(const void* data, int len);

   private:
    nsresult SendRtpRtcpPacket_s(nsAutoPtr<DataBuffer> data,
                                 bool is_rtp);

    MediaPipeline *pipeline_;  // Raw pointer to avoid cycles
    nsCOMPtr<nsIEventTarget> sts_thread_;
  };
  friend class PipelineTransport;

  class TransportInfo {
    public:
      TransportInfo(RefPtr<TransportFlow> flow, RtpType type) :
        transport_(flow),
        state_(MP_CONNECTING),
        type_(type) {
        MOZ_ASSERT(flow);
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

  // Unhooks from signals
  void DisconnectTransport_s(TransportInfo &info);
  nsresult ConnectTransport_s(TransportInfo &info);

  TransportInfo* GetTransportInfo_s(TransportFlow *flow);

  void increment_rtp_packets_sent(int bytes);
  void increment_rtcp_packets_sent();
  void increment_rtp_packets_received(int bytes);
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
  std::string track_id_;        // The track on the stream.
                                // Written on the main thread.
                                // Used on STS and MediaStreamGraph threads.
                                // Not used outside initialization in MediaPipelineTransmit
  // The m-line index (starting at 0, to match convention) Atomic because
  // this value is updated from STS, but read on main, and we don't want to
  // bother with dispatches just to get an int occasionally.
  Atomic<int> level_;
  RefPtr<MediaSessionConduit> conduit_;  // Our conduit. Written on the main
                                         // thread. Read on STS thread.

  // The transport objects. Read/written on STS thread.
  TransportInfo rtp_;
  TransportInfo rtcp_;

  // Pointers to the threads we need. Initialized at creation
  // and used all over the place.
  nsCOMPtr<nsIEventTarget> main_thread_;
  nsCOMPtr<nsIEventTarget> sts_thread_;

  // Created on Init. Referenced by the conduit and eventually
  // destroyed on the STS thread.
  RefPtr<PipelineTransport> transport_;

  // Only safe to access from STS thread.
  // Build into TransportInfo?
  int32_t rtp_packets_sent_;
  int32_t rtcp_packets_sent_;
  int32_t rtp_packets_received_;
  int32_t rtcp_packets_received_;
  int64_t rtp_bytes_sent_;
  int64_t rtp_bytes_received_;

  std::vector<uint32_t> ssrcs_received_;

  // Written on Init. Read on STS thread.
  std::string pc_;
  std::string description_;

  // Written on Init, all following accesses are on the STS thread.
  nsAutoPtr<MediaPipelineFilter> filter_;
  nsAutoPtr<webrtc::RtpHeaderParser> rtp_parser_;

 private:
  nsresult Init_s();

  bool IsRtp(const unsigned char *data, size_t len);
};

class ConduitDeleteEvent: public Runnable
{
public:
  explicit ConduitDeleteEvent(already_AddRefed<MediaSessionConduit> aConduit) :
    mConduit(aConduit) {}

  /* we exist solely to proxy release of the conduit */
  NS_IMETHOD Run() { return NS_OK; }
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
                        dom::MediaStreamTrack* domtrack,
                        const std::string& track_id,
                        int level,
                        RefPtr<MediaSessionConduit> conduit,
                        RefPtr<TransportFlow> rtp_transport,
                        RefPtr<TransportFlow> rtcp_transport,
                        nsAutoPtr<MediaPipelineFilter> filter);

  // Initialize (stuff here may fail)
  nsresult Init() override;

  virtual void AttachToTrack(const std::string& track_id);

  // written and used from MainThread
  bool IsVideo() const override;

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // When the principal of the domtrack changes, it calls through to here
  // so that we can determine whether to enable track transmission.
  // `track` has to be null or equal `domtrack_` for us to apply the update.
  virtual void UpdateSinkIdentity_m(dom::MediaStreamTrack* track,
                                    nsIPrincipal* principal,
                                    const PeerIdentity* sinkIdentity);
#endif

  // Called on the main thread.
  void DetachMedia() override;

  // Override MediaPipeline::TransportReady.
  nsresult TransportReady_s(TransportInfo &info) override;

  // Replace a track with a different one
  // In non-compliance with the likely final spec, allow the new
  // track to be part of a different stream (since we don't support
  // multiple tracks of a type in a stream yet).  bug 1056650
  virtual nsresult ReplaceTrack(dom::MediaStreamTrack& domtrack);

  // Separate classes to allow ref counting
  class PipelineListener;
  class VideoFrameFeeder;

 protected:
  ~MediaPipelineTransmit();

 private:
  RefPtr<PipelineListener> listener_;
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  RefPtr<VideoFrameFeeder> feeder_;
  RefPtr<VideoFrameConverter> converter_;
#endif
  dom::MediaStreamTrack* domtrack_;
};


// A specialization of pipeline for reading from the network and
// rendering video.
class MediaPipelineReceive : public MediaPipeline {
 public:
  // Set rtcp_transport to nullptr to use rtcp-mux
  MediaPipelineReceive(const std::string& pc,
                       nsCOMPtr<nsIEventTarget> main_thread,
                       nsCOMPtr<nsIEventTarget> sts_thread,
                       SourceMediaStream *stream,
                       const std::string& track_id,
                       int level,
                       RefPtr<MediaSessionConduit> conduit,
                       RefPtr<TransportFlow> rtp_transport,
                       RefPtr<TransportFlow> rtcp_transport,
                       nsAutoPtr<MediaPipelineFilter> filter);

  int segments_added() const { return segments_added_; }

#ifndef USE_FAKE_MEDIA_STREAMS
  // Sets the PrincipalHandle we set on the media chunks produced by this
  // pipeline. Must be called on the main thread.
  virtual void SetPrincipalHandle_m(const PrincipalHandle& principal_handle) = 0;
#endif // USE_FAKE_MEDIA_STREAMS
 protected:
  ~MediaPipelineReceive();

  RefPtr<SourceMediaStream> stream_;
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
                            SourceMediaStream* stream,
                            // This comes from an msid attribute. Everywhere
                            // but MediaStreamGraph uses this.
                            const std::string& media_stream_track_id,
                            // This is an integer identifier that is only
                            // unique within a single DOMMediaStream, which is
                            // used by MediaStreamGraph
                            TrackID numeric_track_id,
                            int level,
                            RefPtr<AudioSessionConduit> conduit,
                            RefPtr<TransportFlow> rtp_transport,
                            RefPtr<TransportFlow> rtcp_transport,
                            nsAutoPtr<MediaPipelineFilter> filter);

  void DetachMedia() override;

  nsresult Init() override;
  bool IsVideo() const override { return false; }

#ifndef USE_FAKE_MEDIA_STREAMS
  void SetPrincipalHandle_m(const PrincipalHandle& principal_handle) override;
#endif // USE_FAKE_MEDIA_STREAMS

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
                            SourceMediaStream *stream,
                            // This comes from an msid attribute. Everywhere
                            // but MediaStreamGraph uses this.
                            const std::string& media_stream_track_id,
                            // This is an integer identifier that is only
                            // unique within a single DOMMediaStream, which is
                            // used by MediaStreamGraph
                            TrackID numeric_track_id,
                            int level,
                            RefPtr<VideoSessionConduit> conduit,
                            RefPtr<TransportFlow> rtp_transport,
                            RefPtr<TransportFlow> rtcp_transport,
                            nsAutoPtr<MediaPipelineFilter> filter);

  // Called on the main thread.
  void DetachMedia() override;

  nsresult Init() override;
  bool IsVideo() const override { return true; }

#ifndef USE_FAKE_MEDIA_STREAMS
  void SetPrincipalHandle_m(const PrincipalHandle& principal_handle) override;
#endif // USE_FAKE_MEDIA_STREAMS

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
