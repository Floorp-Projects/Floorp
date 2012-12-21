/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef mediapipeline_h__
#define mediapipeline_h__

#include "sigslot.h"

#ifdef USE_FAKE_MEDIA_STREAMS
#include "FakeMediaStreams.h"
#else
#include "nsDOMMediaStream.h"
#endif
#include "MediaConduitInterface.h"
#include "AudioSegment.h"
#include "SrtpFlow.h"
#include "databuffer.h"
#include "runnable_utils.h"
#include "transportflow.h"

#ifdef MOZILLA_INTERNAL_API
#include "VideoSegment.h"
#endif

namespace mozilla {

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
                MediaStream *stream,
                RefPtr<MediaSessionConduit> conduit,
                RefPtr<TransportFlow> rtp_transport,
                RefPtr<TransportFlow> rtcp_transport)
      : direction_(direction),
        stream_(stream),
        conduit_(conduit),
        rtp_transport_(rtp_transport),
        rtp_state_(MP_CONNECTING),
        rtcp_transport_(rtcp_transport),
        rtcp_state_(MP_CONNECTING),
        main_thread_(main_thread),
        sts_thread_(sts_thread),
        transport_(new PipelineTransport(this)),
        rtp_send_srtp_(),
        rtcp_send_srtp_(),
        rtp_recv_srtp_(),
        rtcp_recv_srtp_(),
        rtp_packets_sent_(0),
        rtcp_packets_sent_(0),
        rtp_packets_received_(0),
        rtcp_packets_received_(0),
        muxed_((rtcp_transport_ == NULL) || (rtp_transport_ == rtcp_transport_)),
        pc_(pc),
        description_() {
  }

  virtual ~MediaPipeline() {
    MOZ_ASSERT(!stream_);  // Check that we have shut down already.
  }

  void Shutdown() {
    ASSERT_ON_THREAD(main_thread_);
    // First shut down networking and then disconnect from
    // the media streams. DetachTransport() is sync so
    // we are sure that the transport is shut down before
    // we touch stream_ or conduit_.
    DetachTransport();
    if (stream_) {
      DetachMediaStream();
    }
  }

  virtual nsresult Init();

  virtual Direction direction() const { return direction_; }

  int rtp_packets_sent() const { return rtp_packets_sent_; }
  int rtcp_packets_sent() const { return rtp_packets_sent_; }
  int rtp_packets_received() const { return rtp_packets_received_; }
  int rtcp_packets_received() const { return rtp_packets_received_; }

  // Thread counting
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaPipeline)

 protected:
  virtual void DetachMediaStream() {}

  // Separate class to allow ref counting
  class PipelineTransport : public TransportInterface {
   public:
    // Implement the TransportInterface functions
    PipelineTransport(MediaPipeline *pipeline)
        : pipeline_(pipeline),
	  sts_thread_(pipeline->sts_thread_) {}

    void Detach() { pipeline_ = NULL; }
    MediaPipeline *pipeline() const { return pipeline_; }

    virtual nsresult SendRtpPacket(const void* data, int len);
    virtual nsresult SendRtcpPacket(const void* data, int len);

   private:
    virtual nsresult SendRtpPacket_s(nsAutoPtr<DataBuffer> data);
    virtual nsresult SendRtcpPacket_s(nsAutoPtr<DataBuffer> data);

    MediaPipeline *pipeline_;  // Raw pointer to avoid cycles
    nsCOMPtr<nsIEventTarget> sts_thread_;
  };
  friend class PipelineTransport;

  virtual nsresult TransportReady(TransportFlow *flow); // The transport is ready
  virtual nsresult TransportFailed(TransportFlow *flow);  // The transport is down

  void increment_rtp_packets_sent();
  void increment_rtcp_packets_sent();
  void increment_rtp_packets_received();
  void increment_rtcp_packets_received();

  virtual nsresult SendPacket(TransportFlow *flow, const void* data, int len);

  // Process slots on transports
  void StateChange(TransportFlow *flow, TransportLayer::State);
  void RtpPacketReceived(TransportLayer *layer, const unsigned char *data,
                         size_t len);
  void RtcpPacketReceived(TransportLayer *layer, const unsigned char *data,
                          size_t len);
  void PacketReceived(TransportLayer *layer, const unsigned char *data,
                      size_t len);

  Direction direction_;
  RefPtr<MediaStream> stream_;  // A pointer to the stream we are servicing.
  		      		// Written on the main thread.
  		      		// Used on STS and MediaStreamGraph threads.
  RefPtr<MediaSessionConduit> conduit_;  // Our conduit. Written on the main
  			      		 // thread. Read on STS thread.

  // The transport objects. Read/written on STS thread.
  RefPtr<TransportFlow> rtp_transport_;
  State rtp_state_;
  RefPtr<TransportFlow> rtcp_transport_;
  State rtcp_state_;

  // Pointers to the threads we need. Initialized at creation
  // and used all over the place.
  nsCOMPtr<nsIEventTarget> main_thread_;
  nsCOMPtr<nsIEventTarget> sts_thread_;

  // Created on Init. Referenced by the conduit and eventually
  // destroyed on the STS thread.
  RefPtr<PipelineTransport> transport_;

  // Used only on STS thread.
  RefPtr<SrtpFlow> rtp_send_srtp_;
  RefPtr<SrtpFlow> rtcp_send_srtp_;
  RefPtr<SrtpFlow> rtp_recv_srtp_;
  RefPtr<SrtpFlow> rtcp_recv_srtp_;

  // Written only on STS thread. May be read on other
  // threads but since there is no mutex, the values
  // will only be approximate.
  int rtp_packets_sent_;
  int rtcp_packets_sent_;
  int rtp_packets_received_;
  int rtcp_packets_received_;

  // Written on Init. Read on STS thread.
  bool muxed_;
  std::string pc_;
  std::string description_;

 private:
  void DetachTransport();
  void DetachTransport_s();

  nsresult TransportReadyInt(TransportFlow *flow);

  bool IsRtp(const unsigned char *data, size_t len);
};


// A specialization of pipeline for reading from an input device
// and transmitting to the network.
class MediaPipelineTransmit : public MediaPipeline {
 public:
  MediaPipelineTransmit(const std::string& pc,
                        nsCOMPtr<nsIEventTarget> main_thread,
                        nsCOMPtr<nsIEventTarget> sts_thread,
                        MediaStream *stream,
                        RefPtr<MediaSessionConduit> conduit,
                        RefPtr<TransportFlow> rtp_transport,
                        RefPtr<TransportFlow> rtcp_transport) :
      MediaPipeline(pc, TRANSMIT, main_thread, sts_thread,
                    stream, conduit, rtp_transport,
                    rtcp_transport),
      listener_(new PipelineListener(conduit)) {}

  // Initialize (stuff here may fail)
  virtual nsresult Init();

  // Called on the main thread.
  virtual void DetachMediaStream() {
    ASSERT_ON_THREAD(main_thread_);
    stream_->RemoveListener(listener_);
    // Remove our reference so that when the MediaStreamGraph
    // releases the listener, it will be destroyed.
    listener_ = nullptr;
    stream_ = nullptr;
  }

  // Override MediaPipeline::TransportReady.
  virtual nsresult TransportReady(TransportFlow *flow);

  // Separate class to allow ref counting
  class PipelineListener : public MediaStreamListener {
   public:
    PipelineListener(const RefPtr<MediaSessionConduit>& conduit)
        : conduit_(conduit), active_(false) {}

    // XXX. This is not thread-safe but the hazard is just 
    // that active_ = true takes a while to propagate. Revisit
    // when 823600 lands.
    void SetActive(bool active) { active_ = active; }

    // Implement MediaStreamListener
    virtual void NotifyQueuedTrackChanges(MediaStreamGraph* graph, TrackID tid,
                                          TrackRate rate,
                                          TrackTicks offset,
                                          uint32_t events,
                                          const MediaSegment& queued_media);
    virtual void NotifyPull(MediaStreamGraph* aGraph, StreamTime aDesiredTime) {}

   private:
    virtual void ProcessAudioChunk(AudioSessionConduit *conduit,
				   TrackRate rate, AudioChunk& chunk);
#ifdef MOZILLA_INTERNAL_API
    virtual void ProcessVideoChunk(VideoSessionConduit *conduit,
				   TrackRate rate, VideoChunk& chunk);
#endif
    RefPtr<MediaSessionConduit> conduit_;
    volatile bool active_;
  };

 private:
RefPtr<PipelineListener> listener_;

};


// A specialization of pipeline for reading from the network and
// rendering video.
class MediaPipelineReceive : public MediaPipeline {
 public:
  MediaPipelineReceive(const std::string& pc,
                       nsCOMPtr<nsIEventTarget> main_thread,
                       nsCOMPtr<nsIEventTarget> sts_thread,
                       MediaStream *stream,
                       RefPtr<MediaSessionConduit> conduit,
                       RefPtr<TransportFlow> rtp_transport,
                       RefPtr<TransportFlow> rtcp_transport) :
      MediaPipeline(pc, RECEIVE, main_thread, sts_thread,
                    stream, conduit, rtp_transport,
                    rtcp_transport),
      segments_added_(0) {
  }

  int segments_added() const { return segments_added_; }

 protected:
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
                            MediaStream *stream,
                            RefPtr<AudioSessionConduit> conduit,
                            RefPtr<TransportFlow> rtp_transport,
                            RefPtr<TransportFlow> rtcp_transport) :
      MediaPipelineReceive(pc, main_thread, sts_thread,
                           stream, conduit, rtp_transport,
                           rtcp_transport),
      listener_(new PipelineListener(stream->AsSourceStream(),
                                     conduit)) {
  }

  virtual void DetachMediaStream() {
    ASSERT_ON_THREAD(main_thread_);
    stream_->RemoveListener(listener_);
    // Remove our reference so that when the MediaStreamGraph
    // releases the listener, it will be destroyed.
    listener_ = nullptr;
    stream_ = nullptr;
  }

  virtual nsresult Init();

 private:
  // Separate class to allow ref counting
  class PipelineListener : public MediaStreamListener {
   public:
    PipelineListener(SourceMediaStream * source,
                     const RefPtr<MediaSessionConduit>& conduit)
        : source_(source),
          conduit_(conduit),
          played_(0) {}

    // Implement MediaStreamListener
    virtual void NotifyQueuedTrackChanges(MediaStreamGraph* graph, TrackID tid,
                                          TrackRate rate,
                                          TrackTicks offset,
                                          uint32_t events,
                                          const MediaSegment& queued_media) {}
    virtual void NotifyPull(MediaStreamGraph* aGraph, StreamTime aDesiredTime);

   private:
    SourceMediaStream *source_;
    RefPtr<MediaSessionConduit> conduit_;
    StreamTime played_;
  };

  RefPtr<PipelineListener> listener_;
};


// A specialization of pipeline for reading from the network and
// rendering video.
class MediaPipelineReceiveVideo : public MediaPipelineReceive {
 public:
  MediaPipelineReceiveVideo(const std::string& pc,
                            nsCOMPtr<nsIEventTarget> main_thread,
                            nsCOMPtr<nsIEventTarget> sts_thread,
                            MediaStream *stream,
                            RefPtr<VideoSessionConduit> conduit,
                            RefPtr<TransportFlow> rtp_transport,
                            RefPtr<TransportFlow> rtcp_transport) :
      MediaPipelineReceive(pc, main_thread, sts_thread,
                           stream, conduit, rtp_transport,
                           rtcp_transport),
      renderer_(new PipelineRenderer(this)) {
  }

  // Called on the main thread.
  virtual void DetachMediaStream() {
    ASSERT_ON_THREAD(main_thread_);
    conduit_ = nullptr;  // Force synchronous destruction so we
                         // stop generating video.
    stream_ = nullptr;
  }

  virtual nsresult Init();

 private:
  class PipelineRenderer : public VideoRenderer {
   public:
    PipelineRenderer(MediaPipelineReceiveVideo *);
    void Detach() { pipeline_ = NULL; }

    // Implement VideoRenderer
    virtual void FrameSizeChange(unsigned int width,
                                 unsigned int height,
                                 unsigned int number_of_streams) {
      width_ = width;
      height_ = height;
    }

    virtual void RenderVideoFrame(const unsigned char* buffer,
                                  unsigned int buffer_size,
                                  uint32_t time_stamp,
                                  int64_t render_time);


   private:
    MediaPipelineReceiveVideo *pipeline_;  // Raw pointer to avoid cycles
#ifdef MOZILLA_INTERNAL_API
    nsRefPtr<layers::ImageContainer> image_container_;
#endif
    int width_;
    int height_;
  };
  friend class PipelineRenderer;

  RefPtr<PipelineRenderer> renderer_;
};


}  // end namespace
#endif
