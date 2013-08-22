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
#include "DOMMediaStream.h"
#include "MediaStreamGraph.h"
#include "VideoUtils.h"
#endif
#include "MediaConduitInterface.h"
#include "AudioSegment.h"
#include "mozilla/ReentrantMonitor.h"
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
                TrackID track_id,
                RefPtr<MediaSessionConduit> conduit,
                RefPtr<TransportFlow> rtp_transport,
                RefPtr<TransportFlow> rtcp_transport)
      : direction_(direction),
        stream_(stream),
        track_id_(track_id),
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
        pc_(pc),
        description_() {
      // To indicate rtcp-mux rtcp_transport should be NULL.
      // Therefore it's an error to send in the same flow for
      // both rtp and rtcp.
      MOZ_ASSERT(rtp_transport_ != rtcp_transport_);

      if (!rtcp_transport_) {
        rtcp_transport_ = rtp_transport;
      }
  }

  virtual ~MediaPipeline();

  // Must be called on the STS thread.  Must be called after ShutdownMedia_m().
  void ShutdownTransport_s();

  // Must be called on the main thread.
  void ShutdownMedia_m() {
    ASSERT_ON_THREAD(main_thread_);

    if (stream_) {
      DetachMediaStream();
    }
  }

  virtual nsresult Init();

  virtual Direction direction() const { return direction_; }

  bool IsDoingRtcpMux() const {
    return (rtp_transport_ == rtcp_transport_);
  }

  int rtp_packets_sent() const { return rtp_packets_sent_; }
  int rtcp_packets_sent() const { return rtcp_packets_sent_; }
  int rtp_packets_received() const { return rtp_packets_received_; }
  int rtcp_packets_received() const { return rtcp_packets_received_; }

  MediaSessionConduit *Conduit() { return conduit_; }

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

  virtual nsresult TransportFailed_s(TransportFlow *flow);  // The transport is down
  virtual nsresult TransportReady_s(TransportFlow *flow);   // The transport is ready

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
  TrackID track_id_;            // The track on the stream.
                                // Written and used as the stream_;
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
  std::string pc_;
  std::string description_;

 private:
  nsresult Init_s();

  bool IsRtp(const unsigned char *data, size_t len);
};

class GenericReceiveListener : public MediaStreamListener
{
 public:
  GenericReceiveListener(SourceMediaStream *source, TrackID track_id,
                         TrackRate track_rate)
    : source_(source),
      track_id_(track_id),
      track_rate_(track_rate),
      played_ticks_(0) {}

  virtual ~GenericReceiveListener() {}

  void AddSelf(MediaSegment* segment);

  void SetPlayedTicks(TrackTicks time) {
    played_ticks_ = time;
  }

  void EndTrack() {
    source_->EndTrack(track_id_);
  }

 protected:
  SourceMediaStream *source_;
  TrackID track_id_;
  TrackRate track_rate_;
  TrackTicks played_ticks_;
};

class TrackAddedCallback {
 public:
  virtual void TrackAdded(TrackTicks current_ticks) = 0;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TrackAddedCallback);

 protected:
  virtual ~TrackAddedCallback() {}
};

class GenericReceiveListener;

class GenericReceiveCallback : public TrackAddedCallback
{
 public:
  GenericReceiveCallback(GenericReceiveListener* listener)
    : listener_(listener) {}

  void TrackAdded(TrackTicks time) {
    listener_->SetPlayedTicks(time);
  }

 private:
  RefPtr<GenericReceiveListener> listener_;
};

class ConduitDeleteEvent: public nsRunnable
{
public:
  ConduitDeleteEvent(TemporaryRef<MediaSessionConduit> aConduit) :
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
  // Set rtcp_transport to NULL to use rtcp-mux
  MediaPipelineTransmit(const std::string& pc,
                        nsCOMPtr<nsIEventTarget> main_thread,
                        nsCOMPtr<nsIEventTarget> sts_thread,
                        MediaStream *stream,
                        TrackID track_id,
                        RefPtr<MediaSessionConduit> conduit,
                        RefPtr<TransportFlow> rtp_transport,
                        RefPtr<TransportFlow> rtcp_transport) :
      MediaPipeline(pc, TRANSMIT, main_thread, sts_thread,
                    stream, track_id, conduit, rtp_transport,
                    rtcp_transport),
      listener_(new PipelineListener(conduit)) {}

  // Initialize (stuff here may fail)
  virtual nsresult Init();

  // Called on the main thread.
  virtual void DetachMediaStream() {
    ASSERT_ON_THREAD(main_thread_);
    stream_->RemoveListener(listener_);
    // Let the listener be destroyed with the pipeline (or later).
    stream_ = nullptr;
  }

  // Override MediaPipeline::TransportReady.
  virtual nsresult TransportReady_s(TransportFlow *flow);

  // Separate class to allow ref counting
  class PipelineListener : public MediaStreamListener {
   public:
    PipelineListener(const RefPtr<MediaSessionConduit>& conduit)
      : conduit_(conduit),
        active_(false),
        last_img_(-1),
        samples_10ms_buffer_(nullptr),
        buffer_current_(0),
        samplenum_10ms_(0) {}

    ~PipelineListener()
    {
      // release conduit on mainthread.  Must use forget()!
      nsresult rv = NS_DispatchToMainThread(new
        ConduitDeleteEvent(conduit_.forget()), NS_DISPATCH_NORMAL);
      MOZ_ASSERT(!NS_FAILED(rv),"Could not dispatch conduit shutdown to main");
      if (NS_FAILED(rv)) {
        MOZ_CRASH();
      }
    }


    // XXX. This is not thread-safe but the hazard is just
    // that active_ = true takes a while to propagate. Revisit
    // when 823600 lands.
    void SetActive(bool active) { active_ = active; }

    // Implement MediaStreamListener
    virtual void NotifyQueuedTrackChanges(MediaStreamGraph* graph, TrackID tid,
                                          TrackRate rate,
                                          TrackTicks offset,
                                          uint32_t events,
                                          const MediaSegment& queued_media) MOZ_OVERRIDE;
    virtual void NotifyPull(MediaStreamGraph* aGraph, StreamTime aDesiredTime) MOZ_OVERRIDE {}

   private:
    virtual void ProcessAudioChunk(AudioSessionConduit *conduit,
				   TrackRate rate, AudioChunk& chunk);
#ifdef MOZILLA_INTERNAL_API
    virtual void ProcessVideoChunk(VideoSessionConduit *conduit,
				   TrackRate rate, VideoChunk& chunk);
#endif
    RefPtr<MediaSessionConduit> conduit_;
    volatile bool active_;

    int32_t last_img_; // serial number of last Image

    // These vars handle breaking audio samples into exact 10ms chunks:
    // The buffer of 10ms audio samples that we will send once full
    // (can be carried over from one call to another).
    nsAutoArrayPtr<int16_t> samples_10ms_buffer_;
    // The location of the pointer within that buffer (in units of samples).
    int64_t buffer_current_;
    // The number of samples in a 10ms audio chunk.
    int64_t samplenum_10ms_;
  };

 private:
  RefPtr<PipelineListener> listener_;
};


// A specialization of pipeline for reading from the network and
// rendering video.
class MediaPipelineReceive : public MediaPipeline {
 public:
  // Set rtcp_transport to NULL to use rtcp-mux
  MediaPipelineReceive(const std::string& pc,
                       nsCOMPtr<nsIEventTarget> main_thread,
                       nsCOMPtr<nsIEventTarget> sts_thread,
                       MediaStream *stream,
                       TrackID track_id,
                       RefPtr<MediaSessionConduit> conduit,
                       RefPtr<TransportFlow> rtp_transport,
                       RefPtr<TransportFlow> rtcp_transport) :
      MediaPipeline(pc, RECEIVE, main_thread, sts_thread,
                    stream, track_id, conduit, rtp_transport,
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
                            TrackID track_id,
                            RefPtr<AudioSessionConduit> conduit,
                            RefPtr<TransportFlow> rtp_transport,
                            RefPtr<TransportFlow> rtcp_transport) :
      MediaPipelineReceive(pc, main_thread, sts_thread,
                           stream, track_id, conduit, rtp_transport,
                           rtcp_transport),
      listener_(new PipelineListener(stream->AsSourceStream(),
                                     track_id, conduit)) {
  }

  virtual void DetachMediaStream() {
    ASSERT_ON_THREAD(main_thread_);
    listener_->EndTrack();
    stream_->RemoveListener(listener_);
    stream_ = nullptr;
  }

  virtual nsresult Init();

 private:
  // Separate class to allow ref counting
  class PipelineListener : public GenericReceiveListener {
   public:
    PipelineListener(SourceMediaStream * source, TrackID track_id,
                     const RefPtr<MediaSessionConduit>& conduit);

    ~PipelineListener()
    {
      // release conduit on mainthread.  Must use forget()!
      nsresult rv = NS_DispatchToMainThread(new
        ConduitDeleteEvent(conduit_.forget()), NS_DISPATCH_NORMAL);
      MOZ_ASSERT(!NS_FAILED(rv),"Could not dispatch conduit shutdown to main");
      if (NS_FAILED(rv)) {
        MOZ_CRASH();
      }
    }

    // Implement MediaStreamListener
    virtual void NotifyQueuedTrackChanges(MediaStreamGraph* graph, TrackID tid,
                                          TrackRate rate,
                                          TrackTicks offset,
                                          uint32_t events,
                                          const MediaSegment& queued_media) MOZ_OVERRIDE {}
    virtual void NotifyPull(MediaStreamGraph* graph, StreamTime desired_time) MOZ_OVERRIDE;

   private:
    RefPtr<MediaSessionConduit> conduit_;
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
                            TrackID track_id,
                            RefPtr<VideoSessionConduit> conduit,
                            RefPtr<TransportFlow> rtp_transport,
                            RefPtr<TransportFlow> rtcp_transport) :
      MediaPipelineReceive(pc, main_thread, sts_thread,
                           stream, track_id, conduit, rtp_transport,
                           rtcp_transport),
      renderer_(new PipelineRenderer(this)),
      listener_(new PipelineListener(stream->AsSourceStream(), track_id)) {
  }

  // Called on the main thread.
  virtual void DetachMediaStream() {
    ASSERT_ON_THREAD(main_thread_);

    listener_->EndTrack();
    // stop generating video and thus stop invoking the PipelineRenderer
    // and PipelineListener - the renderer has a raw ptr to the Pipeline to
    // avoid cycles, and the render callbacks are invoked from a different
    // thread so simple null-checks would cause TSAN bugs without locks.
    static_cast<VideoSessionConduit*>(conduit_.get())->DetachRenderer();
    stream_->RemoveListener(listener_);
    stream_ = nullptr;
  }

  virtual nsresult Init();

 private:
  class PipelineRenderer : public VideoRenderer {
   public:
    PipelineRenderer(MediaPipelineReceiveVideo *pipeline) :
      pipeline_(pipeline) {}

    void Detach() { pipeline_ = NULL; }

    // Implement VideoRenderer
    virtual void FrameSizeChange(unsigned int width,
                                 unsigned int height,
                                 unsigned int number_of_streams) {
      pipeline_->listener_->FrameSizeChange(width, height, number_of_streams);
    }

    virtual void RenderVideoFrame(const unsigned char* buffer,
                                  unsigned int buffer_size,
                                  uint32_t time_stamp,
                                  int64_t render_time) {
      pipeline_->listener_->RenderVideoFrame(buffer, buffer_size, time_stamp,
                                            render_time);
    }

   private:
    MediaPipelineReceiveVideo *pipeline_;  // Raw pointer to avoid cycles
  };

  // Separate class to allow ref counting
  class PipelineListener : public GenericReceiveListener {
   public:
    PipelineListener(SourceMediaStream * source, TrackID track_id);

    // Implement MediaStreamListener
    virtual void NotifyQueuedTrackChanges(MediaStreamGraph* graph, TrackID tid,
                                          TrackRate rate,
                                          TrackTicks offset,
                                          uint32_t events,
                                          const MediaSegment& queued_media) MOZ_OVERRIDE {}
    virtual void NotifyPull(MediaStreamGraph* graph, StreamTime desired_time) MOZ_OVERRIDE;

    // Accessors for external writes from the renderer
    void FrameSizeChange(unsigned int width,
                         unsigned int height,
                         unsigned int number_of_streams) {
      ReentrantMonitorAutoEnter enter(monitor_);

      width_ = width;
      height_ = height;
    }

    void RenderVideoFrame(const unsigned char* buffer,
                          unsigned int buffer_size,
                          uint32_t time_stamp,
                          int64_t render_time);


   private:
    int width_;
    int height_;
#ifdef MOZILLA_INTERNAL_API
    nsRefPtr<layers::ImageContainer> image_container_;
    nsRefPtr<layers::Image> image_;
#endif
    mozilla::ReentrantMonitor monitor_; // Monitor for processing WebRTC frames.
                                        // Protects image_ against:
                                        // - Writing from the GIPS thread
                                        // - Reading from the MSG thread
  };

  friend class PipelineRenderer;

  RefPtr<PipelineRenderer> renderer_;
  RefPtr<PipelineListener> listener_;
};


}  // end namespace
#endif
