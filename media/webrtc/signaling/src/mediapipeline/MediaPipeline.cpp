/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include "MediaPipeline.h"

#ifndef USE_FAKE_MEDIA_STREAMS
#include "MediaStreamGraphImpl.h"
#endif

#include <math.h>

#include "nspr.h"
#include "srtp.h"

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#include "VideoSegment.h"
#include "Layers.h"
#include "LayersLogging.h"
#include "ImageTypes.h"
#include "ImageContainer.h"
#include "DOMMediaStream.h"
#include "MediaStreamTrack.h"
#include "MediaStreamListener.h"
#include "MediaStreamVideoSink.h"
#include "VideoUtils.h"
#include "VideoStreamTrack.h"
#ifdef WEBRTC_GONK
#include "GrallocImages.h"
#include "mozilla/layers/GrallocTextureClient.h"
#endif
#endif

#include "nsError.h"
#include "AudioSegment.h"
#include "MediaSegment.h"
#include "MediaPipelineFilter.h"
#include "databuffer.h"
#include "transportflow.h"
#include "transportlayer.h"
#include "transportlayerdtls.h"
#include "transportlayerice.h"
#include "runnable_utils.h"
#include "libyuv/convert.h"
#include "mozilla/SharedThreadPool.h"
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#include "mozilla/PeerIdentity.h"
#include "mozilla/TaskQueue.h"
#endif
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/Sprintf.h"

#include "webrtc/common_types.h"
#include "webrtc/common_video/interface/native_handle.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/video_engine/include/vie_errors.h"

#include "logging.h"

// Max size given stereo is 480*2*2 = 1920 (10ms of 16-bits stereo audio at
// 48KHz)
#define AUDIO_SAMPLE_BUFFER_MAX 480*2*2
static_assert((WEBRTC_DEFAULT_SAMPLE_RATE/100)*sizeof(uint16_t) * 2
               <= AUDIO_SAMPLE_BUFFER_MAX,
               "AUDIO_SAMPLE_BUFFER_MAX is not large enough");

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;

// Logging context
MOZ_MTLOG_MODULE("mediapipeline")

namespace mozilla {

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
class VideoConverterListener
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoConverterListener)

  virtual void OnVideoFrameConverted(unsigned char* aVideoFrame,
                                     unsigned int aVideoFrameLength,
                                     unsigned short aWidth,
                                     unsigned short aHeight,
                                     VideoType aVideoType,
                                     uint64_t aCaptureTime) = 0;

  virtual void OnVideoFrameConverted(webrtc::I420VideoFrame& aVideoFrame) = 0;

protected:
  virtual ~VideoConverterListener() {}
};

// I420 buffer size macros
#define YSIZE(x,y) ((x)*(y))
#define CRSIZE(x,y) ((((x)+1) >> 1) * (((y)+1) >> 1))
#define I420SIZE(x,y) (YSIZE((x),(y)) + 2 * CRSIZE((x),(y)))

// An async video frame format converter.
//
// Input is typically a MediaStream(Track)Listener driven by MediaStreamGraph.
//
// We keep track of the size of the TaskQueue so we can drop frames if
// conversion is taking too long.
//
// Output is passed through to all added VideoConverterListeners on a TaskQueue
// thread whenever a frame is converted.
class VideoFrameConverter
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoFrameConverter)

  VideoFrameConverter()
    : mLength(0)
    , last_img_(-1) // -1 is not a guaranteed invalid serial. See bug 1262134.
    , disabled_frame_sent_(false)
#ifdef DEBUG
    , mThrottleCount(0)
    , mThrottleRecord(0)
#endif
    , mMutex("VideoFrameConverter")
  {
    MOZ_COUNT_CTOR(VideoFrameConverter);

    RefPtr<SharedThreadPool> pool =
      SharedThreadPool::Get(NS_LITERAL_CSTRING("VideoFrameConverter"));

    mTaskQueue = MakeAndAddRef<TaskQueue>(pool.forget());
  }

  void QueueVideoChunk(VideoChunk& aChunk, bool aForceBlack)
  {
    if (aChunk.IsNull()) {
      return;
    }

    // We get passed duplicate frames every ~10ms even with no frame change.
    int32_t serial = aChunk.mFrame.GetImage()->GetSerial();
    if (serial == last_img_) {
      return;
    }
    last_img_ = serial;

    // A throttling limit of 1 allows us to convert 2 frames concurrently.
    // It's short enough to not build up too significant a delay, while
    // giving us a margin to not cause some machines to drop every other frame.
    const int32_t queueThrottlingLimit = 1;
    if (mLength > queueThrottlingLimit) {
      MOZ_MTLOG(ML_DEBUG, "VideoFrameConverter " << this << " queue is full." <<
                          " Throttling by throwing away a frame.");
#ifdef DEBUG
      ++mThrottleCount;
      mThrottleRecord = std::max(mThrottleCount, mThrottleRecord);
#endif
      return;
    }

#ifdef DEBUG
    if (mThrottleCount > 0) {
      auto level = ML_DEBUG;
      if (mThrottleCount > 5) {
        // Log at a higher level when we have large drops.
        level = ML_INFO;
      }
      MOZ_MTLOG(level, "VideoFrameConverter " << this << " stopped" <<
                       " throttling after throwing away " << mThrottleCount <<
                       " frames. Longest throttle so far was " <<
                       mThrottleRecord << " frames.");
      mThrottleCount = 0;
    }
#endif

    bool forceBlack = aForceBlack || aChunk.mFrame.GetForceBlack();

    if (forceBlack) {
      // Reset the last-img check.
      // -1 is not a guaranteed invalid serial. See bug 1262134.
      last_img_ = -1;

      if (disabled_frame_sent_) {
        // After disabling we just pass one black frame to the encoder.
        // Allocating and setting it to black steals some performance
        // that can be avoided. We don't handle resolution changes while
        // disabled for now.
        return;
      }

      disabled_frame_sent_ = true;
    } else {
      disabled_frame_sent_ = false;
    }

    ++mLength; // Atomic

    nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod<StorensRefPtrPassByPtr<Image>, bool>(
        this, &VideoFrameConverter::ProcessVideoFrame,
        aChunk.mFrame.GetImage(), forceBlack);
    mTaskQueue->Dispatch(runnable.forget());
  }

  void AddListener(VideoConverterListener* aListener)
  {
    MutexAutoLock lock(mMutex);

    MOZ_ASSERT(!mListeners.Contains(aListener));
    mListeners.AppendElement(aListener);
  }

  bool RemoveListener(VideoConverterListener* aListener)
  {
    MutexAutoLock lock(mMutex);

    return mListeners.RemoveElement(aListener);
  }

  void Shutdown()
  {
    mTaskQueue->BeginShutdown();
    mTaskQueue->AwaitShutdownAndIdle();
  }

protected:
  virtual ~VideoFrameConverter()
  {
    MOZ_COUNT_DTOR(VideoFrameConverter);
  }

  void VideoFrameConverted(unsigned char* aVideoFrame,
                           unsigned int aVideoFrameLength,
                           unsigned short aWidth,
                           unsigned short aHeight,
                           VideoType aVideoType,
                           uint64_t aCaptureTime)
  {
    MutexAutoLock lock(mMutex);

    for (RefPtr<VideoConverterListener>& listener : mListeners) {
      listener->OnVideoFrameConverted(aVideoFrame, aVideoFrameLength,
                                      aWidth, aHeight, aVideoType, aCaptureTime);
    }
  }

  void VideoFrameConverted(webrtc::I420VideoFrame& aVideoFrame)
  {
    MutexAutoLock lock(mMutex);

    for (RefPtr<VideoConverterListener>& listener : mListeners) {
      listener->OnVideoFrameConverted(aVideoFrame);
    }
  }

  void ProcessVideoFrame(Image* aImage, bool aForceBlack)
  {
    --mLength; // Atomic
    MOZ_ASSERT(mLength >= 0);

    if (aForceBlack) {
      IntSize size = aImage->GetSize();
      uint32_t yPlaneLen = YSIZE(size.width, size.height);
      uint32_t cbcrPlaneLen = 2 * CRSIZE(size.width, size.height);
      uint32_t length = yPlaneLen + cbcrPlaneLen;

      // Send a black image.
      auto pixelData = MakeUniqueFallible<uint8_t[]>(length);
      if (pixelData) {
        // YCrCb black = 0x10 0x80 0x80
        memset(pixelData.get(), 0x10, yPlaneLen);
        // Fill Cb/Cr planes
        memset(pixelData.get() + yPlaneLen, 0x80, cbcrPlaneLen);

        MOZ_MTLOG(ML_DEBUG, "Sending a black video frame");
        VideoFrameConverted(pixelData.get(), length, size.width, size.height,
                            mozilla::kVideoI420, 0);
      }
      return;
    }

    ImageFormat format = aImage->GetFormat();
#ifdef WEBRTC_GONK
    GrallocImage* nativeImage = aImage->AsGrallocImage();
    if (nativeImage) {
      android::sp<android::GraphicBuffer> graphicBuffer = nativeImage->GetGraphicBuffer();
      int pixelFormat = graphicBuffer->getPixelFormat(); /* PixelFormat is an enum == int */
      mozilla::VideoType destFormat;
      switch (pixelFormat) {
        case HAL_PIXEL_FORMAT_YV12:
          // all android must support this
          destFormat = mozilla::kVideoYV12;
          break;
        case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP:
          destFormat = mozilla::kVideoNV21;
          break;
        case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_P:
          destFormat = mozilla::kVideoI420;
          break;
        default:
          // XXX Bug NNNNNNN
          // use http://dxr.mozilla.org/mozilla-central/source/content/media/omx/I420ColorConverterHelper.cpp
          // to convert unknown types (OEM-specific) to I420
          MOZ_MTLOG(ML_ERROR, "Un-handled GRALLOC buffer type:" << pixelFormat);
          MOZ_CRASH();
      }
      void *basePtr;
      graphicBuffer->lock(android::GraphicBuffer::USAGE_SW_READ_MASK, &basePtr);
      uint32_t width = graphicBuffer->getWidth();
      uint32_t height = graphicBuffer->getHeight();
      // XXX gralloc buffer's width and stride could be different depends on implementations.

      if (destFormat != mozilla::kVideoI420) {
        unsigned char *video_frame = static_cast<unsigned char*>(basePtr);
        webrtc::I420VideoFrame i420_frame;
        int stride_y = width;
        int stride_uv = (width + 1) / 2;
        int target_width = width;
        int target_height = height;
        if (i420_frame.CreateEmptyFrame(target_width,
                                        abs(target_height),
                                        stride_y,
                                        stride_uv, stride_uv) < 0) {
          MOZ_ASSERT(false, "Can't allocate empty i420frame");
          return;
        }
        webrtc::VideoType commonVideoType =
          webrtc::RawVideoTypeToCommonVideoVideoType(
            static_cast<webrtc::RawVideoType>((int)destFormat));
        if (ConvertToI420(commonVideoType, video_frame, 0, 0, width, height,
                          I420SIZE(width, height), webrtc::kVideoRotation_0,
                          &i420_frame)) {
          MOZ_ASSERT(false, "Can't convert video type for sending to I420");
          return;
        }
        i420_frame.set_ntp_time_ms(0);
        VideoFrameConverted(i420_frame);
      } else {
        VideoFrameConverted(static_cast<unsigned char*>(basePtr),
                            I420SIZE(width, height),
                            width,
                            height,
                            destFormat, 0);
      }
      graphicBuffer->unlock();
      return;
    } else
#endif
    if (format == ImageFormat::PLANAR_YCBCR) {
      // Cast away constness b/c some of the accessors are non-const
      PlanarYCbCrImage* yuv = const_cast<PlanarYCbCrImage *>(
          static_cast<const PlanarYCbCrImage *>(aImage));

      const PlanarYCbCrData *data = yuv->GetData();
      if (data) {
        uint8_t *y = data->mYChannel;
        uint8_t *cb = data->mCbChannel;
        uint8_t *cr = data->mCrChannel;
        int32_t yStride = data->mYStride;
        int32_t cbCrStride = data->mCbCrStride;
        uint32_t width = yuv->GetSize().width;
        uint32_t height = yuv->GetSize().height;

        webrtc::I420VideoFrame i420_frame;
        int rv = i420_frame.CreateFrame(y, cb, cr, width, height,
                                        yStride, cbCrStride, cbCrStride,
                                        webrtc::kVideoRotation_0);
        if (rv != 0) {
          NS_ERROR("Creating an I420 frame failed");
          return;
        }

        MOZ_MTLOG(ML_DEBUG, "Sending an I420 video frame");
        VideoFrameConverted(i420_frame);
        return;
      }
    }

    RefPtr<SourceSurface> surf = aImage->GetAsSourceSurface();
    if (!surf) {
      MOZ_MTLOG(ML_ERROR, "Getting surface from " << Stringify(format) << " image failed");
      return;
    }

    RefPtr<DataSourceSurface> data = surf->GetDataSurface();
    if (!data) {
      MOZ_MTLOG(ML_ERROR, "Getting data surface from " << Stringify(format)
                          << " image with " << Stringify(surf->GetType()) << "("
                          << Stringify(surf->GetFormat()) << ") surface failed");
      return;
    }

    IntSize size = aImage->GetSize();
    int half_width = (size.width + 1) >> 1;
    int half_height = (size.height + 1) >> 1;
    int c_size = half_width * half_height;
    int buffer_size = YSIZE(size.width, size.height) + 2 * c_size;
    auto yuv_scoped = MakeUniqueFallible<uint8[]>(buffer_size);
    if (!yuv_scoped) {
      return;
    }
    uint8* yuv = yuv_scoped.get();

    DataSourceSurface::ScopedMap map(data, DataSourceSurface::READ);
    if (!map.IsMapped()) {
      MOZ_MTLOG(ML_ERROR, "Reading DataSourceSurface from " << Stringify(format)
                          << " image with " << Stringify(surf->GetType()) << "("
                          << Stringify(surf->GetFormat()) << ") surface failed");
      return;
    }

    int rv;
    int cb_offset = YSIZE(size.width, size.height);
    int cr_offset = cb_offset + c_size;
    switch (surf->GetFormat()) {
      case SurfaceFormat::B8G8R8A8:
      case SurfaceFormat::B8G8R8X8:
        rv = libyuv::ARGBToI420(static_cast<uint8*>(map.GetData()),
                                map.GetStride(),
                                yuv, size.width,
                                yuv + cb_offset, half_width,
                                yuv + cr_offset, half_width,
                                size.width, size.height);
        break;
      case SurfaceFormat::R5G6B5_UINT16:
        rv = libyuv::RGB565ToI420(static_cast<uint8*>(map.GetData()),
                                  map.GetStride(),
                                  yuv, size.width,
                                  yuv + cb_offset, half_width,
                                  yuv + cr_offset, half_width,
                                  size.width, size.height);
        break;
      default:
        MOZ_MTLOG(ML_ERROR, "Unsupported RGB video format" << Stringify(surf->GetFormat()));
        MOZ_ASSERT(PR_FALSE);
        return;
    }
    if (rv != 0) {
      MOZ_MTLOG(ML_ERROR, Stringify(surf->GetFormat()) << " to I420 conversion failed");
      return;
    }
    MOZ_MTLOG(ML_DEBUG, "Sending an I420 video frame converted from " <<
                        Stringify(surf->GetFormat()));
    VideoFrameConverted(yuv, buffer_size, size.width, size.height, mozilla::kVideoI420, 0);
  }

  Atomic<int32_t, Relaxed> mLength;
  RefPtr<TaskQueue> mTaskQueue;

  // Written and read from the queueing thread (normally MSG).
  int32_t last_img_; // serial number of last Image
  bool disabled_frame_sent_; // If a black frame has been sent after disabling.
#ifdef DEBUG
  uint32_t mThrottleCount;
  uint32_t mThrottleRecord;
#endif

  // mMutex guards the below variables.
  Mutex mMutex;
  nsTArray<RefPtr<VideoConverterListener>> mListeners;
};
#endif

// An async inserter for audio data, to avoid running audio codec encoders
// on the MSG/input audio thread.  Basically just bounces all the audio
// data to a single audio processing/input queue.  We could if we wanted to
// use multiple threads and a TaskQueue.
class AudioProxyThread
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioProxyThread)

  explicit AudioProxyThread(AudioSessionConduit *aConduit)
    : mConduit(aConduit)
  {
    MOZ_ASSERT(mConduit);
    MOZ_COUNT_CTOR(AudioProxyThread);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
    // Use only 1 thread; also forces FIFO operation
    // We could use multiple threads, but that may be dicier with the webrtc.org
    // code.  If so we'd need to use TaskQueues like the videoframe converter
    RefPtr<SharedThreadPool> pool =
      SharedThreadPool::Get(NS_LITERAL_CSTRING("AudioProxy"), 1);

    mThread = pool.get();
#else
    nsCOMPtr<nsIThread> thread;
    if (!NS_WARN_IF(NS_FAILED(NS_NewNamedThread("AudioProxy", getter_AddRefs(thread))))) {
      mThread = thread;
    }
#endif
  }

  // called on mThread
  void InternalProcessAudioChunk(
    TrackRate rate,
    AudioChunk& chunk,
    bool enabled) {

    // Convert to interleaved, 16-bits integer audio, with a maximum of two
    // channels (since the WebRTC.org code below makes the assumption that the
    // input audio is either mono or stereo).
    uint32_t outputChannels = chunk.ChannelCount() == 1 ? 1 : 2;
    const int16_t* samples = nullptr;
    UniquePtr<int16_t[]> convertedSamples;

    // We take advantage of the fact that the common case (microphone directly to
    // PeerConnection, that is, a normal call), the samples are already 16-bits
    // mono, so the representation in interleaved and planar is the same, and we
    // can just use that.
    if (enabled && outputChannels == 1 && chunk.mBufferFormat == AUDIO_FORMAT_S16) {
      samples = chunk.ChannelData<int16_t>().Elements()[0];
    } else {
      convertedSamples = MakeUnique<int16_t[]>(chunk.mDuration * outputChannels);

      if (!enabled || chunk.mBufferFormat == AUDIO_FORMAT_SILENCE) {
        PodZero(convertedSamples.get(), chunk.mDuration * outputChannels);
      } else if (chunk.mBufferFormat == AUDIO_FORMAT_FLOAT32) {
        DownmixAndInterleave(chunk.ChannelData<float>(),
                             chunk.mDuration, chunk.mVolume, outputChannels,
                             convertedSamples.get());
      } else if (chunk.mBufferFormat == AUDIO_FORMAT_S16) {
        DownmixAndInterleave(chunk.ChannelData<int16_t>(),
                             chunk.mDuration, chunk.mVolume, outputChannels,
                             convertedSamples.get());
      }
      samples = convertedSamples.get();
    }

    MOZ_ASSERT(!(rate%100)); // rate should be a multiple of 100

    // Check if the rate or the number of channels has changed since the last time
    // we came through. I realize it may be overkill to check if the rate has
    // changed, but I believe it is possible (e.g. if we change sources) and it
    // costs us very little to handle this case.

    uint32_t audio_10ms = rate / 100;

    if (!packetizer_ ||
        packetizer_->PacketSize() != audio_10ms ||
        packetizer_->Channels() != outputChannels) {
      // It's ok to drop the audio still in the packetizer here.
      packetizer_ = new AudioPacketizer<int16_t, int16_t>(audio_10ms, outputChannels);
    }

    packetizer_->Input(samples, chunk.mDuration);

    while (packetizer_->PacketsAvailable()) {
      uint32_t samplesPerPacket = packetizer_->PacketSize() *
                                  packetizer_->Channels();
      // We know that webrtc.org's code going to copy the samples down the line,
      // so we can just use a stack buffer here instead of malloc-ing.
      int16_t packet[AUDIO_SAMPLE_BUFFER_MAX];

      packetizer_->Output(packet);
      mConduit->SendAudioFrame(packet, samplesPerPacket, rate, 0);
    }
  }

  void QueueAudioChunk(TrackRate rate, AudioChunk& chunk, bool enabled)
  {
    RUN_ON_THREAD(mThread,
                  WrapRunnable(RefPtr<AudioProxyThread>(this),
                               &AudioProxyThread::InternalProcessAudioChunk,
                               rate, chunk, enabled),
                  NS_DISPATCH_NORMAL);
  }

protected:
  virtual ~AudioProxyThread()
  {
    // Conduits must be released on MainThread, and we might have the last reference
    // We don't need to worry about runnables still trying to access the conduit, since
    // the runnables hold a ref to AudioProxyThread.
    NS_ReleaseOnMainThread(mConduit.forget());
    MOZ_COUNT_DTOR(AudioProxyThread);
  }

  RefPtr<AudioSessionConduit> mConduit;
  nsCOMPtr<nsIEventTarget> mThread;
  // Only accessed on mThread
  nsAutoPtr<AudioPacketizer<int16_t, int16_t>> packetizer_;
};

static char kDTLSExporterLabel[] = "EXTRACTOR-dtls_srtp";

MediaPipeline::MediaPipeline(const std::string& pc,
                             Direction direction,
                             nsCOMPtr<nsIEventTarget> main_thread,
                             nsCOMPtr<nsIEventTarget> sts_thread,
                             const std::string& track_id,
                             int level,
                             RefPtr<MediaSessionConduit> conduit,
                             RefPtr<TransportFlow> rtp_transport,
                             RefPtr<TransportFlow> rtcp_transport,
                             nsAutoPtr<MediaPipelineFilter> filter)
  : direction_(direction),
    track_id_(track_id),
    level_(level),
    conduit_(conduit),
    rtp_(rtp_transport, rtcp_transport ? RTP : MUX),
    rtcp_(rtcp_transport ? rtcp_transport : rtp_transport,
          rtcp_transport ? RTCP : MUX),
    main_thread_(main_thread),
    sts_thread_(sts_thread),
    rtp_packets_sent_(0),
    rtcp_packets_sent_(0),
    rtp_packets_received_(0),
    rtcp_packets_received_(0),
    rtp_bytes_sent_(0),
    rtp_bytes_received_(0),
    pc_(pc),
    description_(),
    filter_(filter),
    rtp_parser_(webrtc::RtpHeaderParser::Create()) {
  // To indicate rtcp-mux rtcp_transport should be nullptr.
  // Therefore it's an error to send in the same flow for
  // both rtp and rtcp.
  MOZ_ASSERT(rtp_transport != rtcp_transport);

  // PipelineTransport() will access this->sts_thread_; moved here for safety
  transport_ = new PipelineTransport(this);
}

MediaPipeline::~MediaPipeline() {
  ASSERT_ON_THREAD(main_thread_);
  MOZ_MTLOG(ML_INFO, "Destroying MediaPipeline: " << description_);
}

nsresult MediaPipeline::Init() {
  ASSERT_ON_THREAD(main_thread_);

  if (direction_ == RECEIVE) {
    conduit_->SetReceiverTransport(transport_);
  } else {
    conduit_->SetTransmitterTransport(transport_);
  }

  RUN_ON_THREAD(sts_thread_,
                WrapRunnable(
                    RefPtr<MediaPipeline>(this),
                    &MediaPipeline::Init_s),
                NS_DISPATCH_NORMAL);

  return NS_OK;
}

nsresult MediaPipeline::Init_s() {
  ASSERT_ON_THREAD(sts_thread_);

  return AttachTransport_s();
}


// Disconnect us from the transport so that we can cleanly destruct the
// pipeline on the main thread.  ShutdownMedia_m() must have already been
// called
void
MediaPipeline::DetachTransport_s()
{
  ASSERT_ON_THREAD(sts_thread_);

  disconnect_all();
  transport_->Detach();
  rtp_.Detach();
  rtcp_.Detach();
}

nsresult
MediaPipeline::AttachTransport_s()
{
  ASSERT_ON_THREAD(sts_thread_);
  nsresult res;
  MOZ_ASSERT(rtp_.transport_);
  MOZ_ASSERT(rtcp_.transport_);
  res = ConnectTransport_s(rtp_);
  if (NS_FAILED(res)) {
    return res;
  }

  if (rtcp_.transport_ != rtp_.transport_) {
    res = ConnectTransport_s(rtcp_);
    if (NS_FAILED(res)) {
      return res;
    }
  }

  transport_->Attach(this);

  return NS_OK;
}

void
MediaPipeline::UpdateTransport_m(int level,
                                 RefPtr<TransportFlow> rtp_transport,
                                 RefPtr<TransportFlow> rtcp_transport,
                                 nsAutoPtr<MediaPipelineFilter> filter)
{
  RUN_ON_THREAD(sts_thread_,
                WrapRunnable(
                    this,
                    &MediaPipeline::UpdateTransport_s,
                    level,
                    rtp_transport,
                    rtcp_transport,
                    filter),
                NS_DISPATCH_NORMAL);
}

void
MediaPipeline::UpdateTransport_s(int level,
                                 RefPtr<TransportFlow> rtp_transport,
                                 RefPtr<TransportFlow> rtcp_transport,
                                 nsAutoPtr<MediaPipelineFilter> filter)
{
  bool rtcp_mux = false;
  if (!rtcp_transport) {
    rtcp_transport = rtp_transport;
    rtcp_mux = true;
  }

  if ((rtp_transport != rtp_.transport_) ||
      (rtcp_transport != rtcp_.transport_)) {
    DetachTransport_s();
    rtp_ = TransportInfo(rtp_transport, rtcp_mux ? MUX : RTP);
    rtcp_ = TransportInfo(rtcp_transport, rtcp_mux ? MUX : RTCP);
    AttachTransport_s();
  }

  level_ = level;

  if (filter_ && filter) {
    // Use the new filter, but don't forget any remote SSRCs that we've learned
    // by receiving traffic.
    filter_->Update(*filter);
  } else {
    filter_ = filter;
  }
}

void
MediaPipeline::SelectSsrc_m(size_t ssrc_index)
{
  RUN_ON_THREAD(sts_thread_,
                WrapRunnable(
                    this,
                    &MediaPipeline::SelectSsrc_s,
                    ssrc_index),
                NS_DISPATCH_NORMAL);
}

void
MediaPipeline::SelectSsrc_s(size_t ssrc_index)
{
  filter_ = new MediaPipelineFilter;
  if (ssrc_index < ssrcs_received_.size()) {
    filter_->AddRemoteSSRC(ssrcs_received_[ssrc_index]);
  } else {
    MOZ_MTLOG(ML_WARNING, "SelectSsrc called with " << ssrc_index << " but we "
                          << "have only seen " << ssrcs_received_.size()
                          << " ssrcs");
  }
}

void MediaPipeline::StateChange(TransportFlow *flow, TransportLayer::State state) {
  TransportInfo* info = GetTransportInfo_s(flow);
  MOZ_ASSERT(info);

  if (state == TransportLayer::TS_OPEN) {
    MOZ_MTLOG(ML_INFO, "Flow is ready");
    TransportReady_s(*info);
  } else if (state == TransportLayer::TS_CLOSED ||
             state == TransportLayer::TS_ERROR) {
    TransportFailed_s(*info);
  }
}

static bool MakeRtpTypeToStringArray(const char** array) {
  static const char* RTP_str = "RTP";
  static const char* RTCP_str = "RTCP";
  static const char* MUX_str = "RTP/RTCP mux";
  array[MediaPipeline::RTP] = RTP_str;
  array[MediaPipeline::RTCP] = RTCP_str;
  array[MediaPipeline::MUX] = MUX_str;
  return true;
}

static const char* ToString(MediaPipeline::RtpType type) {
  static const char* array[(int)MediaPipeline::MAX_RTP_TYPE] = {nullptr};
  // Dummy variable to cause init to happen only on first call
  static bool dummy = MakeRtpTypeToStringArray(array);
  (void)dummy;
  return array[type];
}

nsresult MediaPipeline::TransportReady_s(TransportInfo &info) {
  MOZ_ASSERT(!description_.empty());

  // TODO(ekr@rtfm.com): implement some kind of notification on
  // failure. bug 852665.
  if (info.state_ != MP_CONNECTING) {
    MOZ_MTLOG(ML_ERROR, "Transport ready for flow in wrong state:" <<
              description_ << ": " << ToString(info.type_));
    return NS_ERROR_FAILURE;
  }

  MOZ_MTLOG(ML_INFO, "Transport ready for pipeline " <<
            static_cast<void *>(this) << " flow " << description_ << ": " <<
            ToString(info.type_));

  // TODO(bcampen@mozilla.com): Should we disconnect from the flow on failure?
  nsresult res;

  // Now instantiate the SRTP objects
  TransportLayerDtls *dtls = static_cast<TransportLayerDtls *>(
      info.transport_->GetLayer(TransportLayerDtls::ID()));
  MOZ_ASSERT(dtls);  // DTLS is mandatory

  uint16_t cipher_suite;
  res = dtls->GetSrtpCipher(&cipher_suite);
  if (NS_FAILED(res)) {
    MOZ_MTLOG(ML_ERROR, "Failed to negotiate DTLS-SRTP. This is an error");
    info.state_ = MP_CLOSED;
    UpdateRtcpMuxState(info);
    return res;
  }

  // SRTP Key Exporter as per RFC 5764 S 4.2
  unsigned char srtp_block[SRTP_TOTAL_KEY_LENGTH * 2];
  res = dtls->ExportKeyingMaterial(kDTLSExporterLabel, false, "",
                                   srtp_block, sizeof(srtp_block));
  if (NS_FAILED(res)) {
    MOZ_MTLOG(ML_ERROR, "Failed to compute DTLS-SRTP keys. This is an error");
    info.state_ = MP_CLOSED;
    UpdateRtcpMuxState(info);
    MOZ_CRASH();  // TODO: Remove once we have enough field experience to
                  // know it doesn't happen. bug 798797. Note that the
                  // code after this never executes.
    return res;
  }

  // Slice and dice as per RFC 5764 S 4.2
  unsigned char client_write_key[SRTP_TOTAL_KEY_LENGTH];
  unsigned char server_write_key[SRTP_TOTAL_KEY_LENGTH];
  int offset = 0;
  memcpy(client_write_key, srtp_block + offset, SRTP_MASTER_KEY_LENGTH);
  offset += SRTP_MASTER_KEY_LENGTH;
  memcpy(server_write_key, srtp_block + offset, SRTP_MASTER_KEY_LENGTH);
  offset += SRTP_MASTER_KEY_LENGTH;
  memcpy(client_write_key + SRTP_MASTER_KEY_LENGTH,
         srtp_block + offset, SRTP_MASTER_SALT_LENGTH);
  offset += SRTP_MASTER_SALT_LENGTH;
  memcpy(server_write_key + SRTP_MASTER_KEY_LENGTH,
         srtp_block + offset, SRTP_MASTER_SALT_LENGTH);
  offset += SRTP_MASTER_SALT_LENGTH;
  MOZ_ASSERT(offset == sizeof(srtp_block));

  unsigned char *write_key;
  unsigned char *read_key;

  if (dtls->role() == TransportLayerDtls::CLIENT) {
    write_key = client_write_key;
    read_key = server_write_key;
  } else {
    write_key = server_write_key;
    read_key = client_write_key;
  }

  MOZ_ASSERT(!info.send_srtp_ && !info.recv_srtp_);
  info.send_srtp_ = SrtpFlow::Create(cipher_suite, false, write_key,
                                     SRTP_TOTAL_KEY_LENGTH);
  info.recv_srtp_ = SrtpFlow::Create(cipher_suite, true, read_key,
                                     SRTP_TOTAL_KEY_LENGTH);
  if (!info.send_srtp_ || !info.recv_srtp_) {
    MOZ_MTLOG(ML_ERROR, "Couldn't create SRTP flow for "
              << ToString(info.type_));
    info.state_ = MP_CLOSED;
    UpdateRtcpMuxState(info);
    return NS_ERROR_FAILURE;
  }

    MOZ_MTLOG(ML_INFO, "Listening for " << ToString(info.type_)
                       << " packets received on " <<
                       static_cast<void *>(dtls->downward()));

  switch (info.type_) {
    case RTP:
      dtls->downward()->SignalPacketReceived.connect(
          this,
          &MediaPipeline::RtpPacketReceived);
      break;
    case RTCP:
      dtls->downward()->SignalPacketReceived.connect(
          this,
          &MediaPipeline::RtcpPacketReceived);
      break;
    case MUX:
      dtls->downward()->SignalPacketReceived.connect(
          this,
          &MediaPipeline::PacketReceived);
      break;
    default:
      MOZ_CRASH();
  }

  info.state_ = MP_OPEN;
  UpdateRtcpMuxState(info);
  return NS_OK;
}

nsresult MediaPipeline::TransportFailed_s(TransportInfo &info) {
  ASSERT_ON_THREAD(sts_thread_);

  info.state_ = MP_CLOSED;
  UpdateRtcpMuxState(info);

  MOZ_MTLOG(ML_INFO, "Transport closed for flow " << ToString(info.type_));

  NS_WARNING(
      "MediaPipeline Transport failed. This is not properly cleaned up yet");

  // TODO(ekr@rtfm.com): SECURITY: Figure out how to clean up if the
  // connection was good and now it is bad.
  // TODO(ekr@rtfm.com): Report up so that the PC knows we
  // have experienced an error.

  return NS_OK;
}

void MediaPipeline::UpdateRtcpMuxState(TransportInfo &info) {
  if (info.type_ == MUX) {
    if (info.transport_ == rtcp_.transport_) {
      rtcp_.state_ = info.state_;
      if (!rtcp_.send_srtp_) {
        rtcp_.send_srtp_ = info.send_srtp_;
        rtcp_.recv_srtp_ = info.recv_srtp_;
      }
    }
  }
}

nsresult MediaPipeline::SendPacket(TransportFlow *flow, const void *data,
                                   int len) {
  ASSERT_ON_THREAD(sts_thread_);

  // Note that we bypass the DTLS layer here
  TransportLayerDtls *dtls = static_cast<TransportLayerDtls *>(
      flow->GetLayer(TransportLayerDtls::ID()));
  MOZ_ASSERT(dtls);

  TransportResult res = dtls->downward()->
      SendPacket(static_cast<const unsigned char *>(data), len);

  if (res != len) {
    // Ignore blocking indications
    if (res == TE_WOULDBLOCK)
      return NS_OK;

    MOZ_MTLOG(ML_ERROR, "Failed write on stream " << description_);
    return NS_BASE_STREAM_CLOSED;
  }

  return NS_OK;
}

void MediaPipeline::increment_rtp_packets_sent(int32_t bytes) {
  ++rtp_packets_sent_;
  rtp_bytes_sent_ += bytes;

  if (!(rtp_packets_sent_ % 100)) {
    MOZ_MTLOG(ML_INFO, "RTP sent packet count for " << description_
              << " Pipeline " << static_cast<void *>(this)
              << " Flow : " << static_cast<void *>(rtp_.transport_)
              << ": " << rtp_packets_sent_
              << " (" << rtp_bytes_sent_ << " bytes)");
  }
}

void MediaPipeline::increment_rtcp_packets_sent() {
  ++rtcp_packets_sent_;
  if (!(rtcp_packets_sent_ % 100)) {
    MOZ_MTLOG(ML_INFO, "RTCP sent packet count for " << description_
              << " Pipeline " << static_cast<void *>(this)
              << " Flow : " << static_cast<void *>(rtcp_.transport_)
              << ": " << rtcp_packets_sent_);
  }
}

void MediaPipeline::increment_rtp_packets_received(int32_t bytes) {
  ++rtp_packets_received_;
  rtp_bytes_received_ += bytes;
  if (!(rtp_packets_received_ % 100)) {
    MOZ_MTLOG(ML_INFO, "RTP received packet count for " << description_
              << " Pipeline " << static_cast<void *>(this)
              << " Flow : " << static_cast<void *>(rtp_.transport_)
              << ": " << rtp_packets_received_
              << " (" << rtp_bytes_received_ << " bytes)");
  }
}

void MediaPipeline::increment_rtcp_packets_received() {
  ++rtcp_packets_received_;
  if (!(rtcp_packets_received_ % 100)) {
    MOZ_MTLOG(ML_INFO, "RTCP received packet count for " << description_
              << " Pipeline " << static_cast<void *>(this)
              << " Flow : " << static_cast<void *>(rtcp_.transport_)
              << ": " << rtcp_packets_received_);
  }
}

void MediaPipeline::RtpPacketReceived(TransportLayer *layer,
                                      const unsigned char *data,
                                      size_t len) {
  if (!transport_->pipeline()) {
    MOZ_MTLOG(ML_ERROR, "Discarding incoming packet; transport disconnected");
    return;
  }

  if (!conduit_) {
    MOZ_MTLOG(ML_DEBUG, "Discarding incoming packet; media disconnected");
    return;
  }

  if (rtp_.state_ != MP_OPEN) {
    MOZ_MTLOG(ML_ERROR, "Discarding incoming packet; pipeline not open");
    return;
  }

  if (rtp_.transport_->state() != TransportLayer::TS_OPEN) {
    MOZ_MTLOG(ML_ERROR, "Discarding incoming packet; transport not open");
    return;
  }

  // This should never happen.
  MOZ_ASSERT(rtp_.recv_srtp_);

  if (direction_ == TRANSMIT) {
    return;
  }

  if (!len) {
    return;
  }

  // Filter out everything but RTP/RTCP
  if (data[0] < 128 || data[0] > 191) {
    return;
  }

  webrtc::RTPHeader header;
  if (!rtp_parser_->Parse(data, len, &header)) {
    return;
  }

  if (std::find(ssrcs_received_.begin(), ssrcs_received_.end(), header.ssrc) ==
      ssrcs_received_.end()) {
    ssrcs_received_.push_back(header.ssrc);
  }

  if (filter_ && !filter_->Filter(header)) {
    return;
  }

  // Make a copy rather than cast away constness
  auto inner_data = MakeUnique<unsigned char[]>(len);
  memcpy(inner_data.get(), data, len);
  int out_len = 0;
  nsresult res = rtp_.recv_srtp_->UnprotectRtp(inner_data.get(),
                                               len, len, &out_len);
  if (!NS_SUCCEEDED(res)) {
    char tmp[16];

    SprintfLiteral(tmp, "%.2x %.2x %.2x %.2x",
                   inner_data[0],
                   inner_data[1],
                   inner_data[2],
                   inner_data[3]);

    MOZ_MTLOG(ML_NOTICE, "Error unprotecting RTP in " << description_
              << "len= " << len << "[" << tmp << "...]");

    return;
  }
  MOZ_MTLOG(ML_DEBUG, description_ << " received RTP packet.");
  increment_rtp_packets_received(out_len);

  (void)conduit_->ReceivedRTPPacket(inner_data.get(), out_len);  // Ignore error codes
}

void MediaPipeline::RtcpPacketReceived(TransportLayer *layer,
                                       const unsigned char *data,
                                       size_t len) {
  if (!transport_->pipeline()) {
    MOZ_MTLOG(ML_DEBUG, "Discarding incoming packet; transport disconnected");
    return;
  }

  if (!conduit_) {
    MOZ_MTLOG(ML_DEBUG, "Discarding incoming packet; media disconnected");
    return;
  }

  if (rtcp_.state_ != MP_OPEN) {
    MOZ_MTLOG(ML_DEBUG, "Discarding incoming packet; pipeline not open");
    return;
  }

  if (rtcp_.transport_->state() != TransportLayer::TS_OPEN) {
    MOZ_MTLOG(ML_ERROR, "Discarding incoming packet; transport not open");
    return;
  }

  if (!len) {
    return;
  }

  // Filter out everything but RTP/RTCP
  if (data[0] < 128 || data[0] > 191) {
    return;
  }

  // We do not filter RTCP for send pipelines, since the webrtc.org code for
  // senders already has logic to ignore RRs that do not apply.
  // TODO bug 1279153: remove SR check for reduced size RTCP
  if (filter_ && direction_ == RECEIVE) {
    if (!filter_->FilterSenderReport(data, len)) {
      MOZ_MTLOG(ML_NOTICE, "Dropping incoming RTCP packet; filtered out");
      return;
    }
  }

  // Make a copy rather than cast away constness
  auto inner_data = MakeUnique<unsigned char[]>(len);
  memcpy(inner_data.get(), data, len);
  int out_len;

  nsresult res = rtcp_.recv_srtp_->UnprotectRtcp(inner_data.get(),
                                                 len,
                                                 len,
                                                 &out_len);

  if (!NS_SUCCEEDED(res))
    return;

  MOZ_MTLOG(ML_DEBUG, description_ << " received RTCP packet.");
  increment_rtcp_packets_received();

  MOZ_ASSERT(rtcp_.recv_srtp_);  // This should never happen

  (void)conduit_->ReceivedRTCPPacket(inner_data.get(), out_len);  // Ignore error codes
}

bool MediaPipeline::IsRtp(const unsigned char *data, size_t len) {
  if (len < 2)
    return false;

  // Check if this is a RTCP packet. Logic based on the types listed in
  // media/webrtc/trunk/src/modules/rtp_rtcp/source/rtp_utility.cc

  // Anything outside this range is RTP.
  if ((data[1] < 192) || (data[1] > 207))
    return true;

  if (data[1] == 192)  // FIR
    return false;

  if (data[1] == 193)  // NACK, but could also be RTP. This makes us sad
    return true;       // but it's how webrtc.org behaves.

  if (data[1] == 194)
    return true;

  if (data[1] == 195)  // IJ.
    return false;

  if ((data[1] > 195) && (data[1] < 200))  // the > 195 is redundant
    return true;

  if ((data[1] >= 200) && (data[1] <= 207))  // SR, RR, SDES, BYE,
    return false;                            // APP, RTPFB, PSFB, XR

  MOZ_ASSERT(false);  // Not reached, belt and suspenders.
  return true;
}

void MediaPipeline::PacketReceived(TransportLayer *layer,
                                   const unsigned char *data,
                                   size_t len) {
  if (!transport_->pipeline()) {
    MOZ_MTLOG(ML_DEBUG, "Discarding incoming packet; transport disconnected");
    return;
  }

  if (IsRtp(data, len)) {
    RtpPacketReceived(layer, data, len);
  } else {
    RtcpPacketReceived(layer, data, len);
  }
}

class MediaPipelineTransmit::PipelineListener
  : public DirectMediaStreamTrackListener
{
friend class MediaPipelineTransmit;
public:
  explicit PipelineListener(const RefPtr<MediaSessionConduit>& conduit)
    : conduit_(conduit),
      track_id_(TRACK_INVALID),
      mMutex("MediaPipelineTransmit::PipelineListener"),
      track_id_external_(TRACK_INVALID),
      active_(false),
      enabled_(false),
      direct_connect_(false)
  {
  }

  ~PipelineListener()
  {
    if (!NS_IsMainThread()) {
      // release conduit on mainthread.  Must use forget()!
      nsresult rv = NS_DispatchToMainThread(new
                                            ConduitDeleteEvent(conduit_.forget()));
      MOZ_ASSERT(!NS_FAILED(rv),"Could not dispatch conduit shutdown to main");
      if (NS_FAILED(rv)) {
        MOZ_CRASH();
      }
    } else {
      conduit_ = nullptr;
    }
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
    if (converter_) {
      converter_->Shutdown();
    }
#endif
  }

  // Dispatches setting the internal TrackID to TRACK_INVALID to the media
  // graph thread to keep it in sync with other MediaStreamGraph operations
  // like RemoveListener() and AddListener(). The TrackID will be updated on
  // the next NewData() callback.
  void UnsetTrackId(MediaStreamGraphImpl* graph);

  void SetActive(bool active) { active_ = active; }
  void SetEnabled(bool enabled) { enabled_ = enabled; }

  // These are needed since nested classes don't have access to any particular
  // instance of the parent
  void SetAudioProxy(const RefPtr<AudioProxyThread>& proxy)
  {
    audio_processing_ = proxy;
  }

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  void SetVideoFrameConverter(const RefPtr<VideoFrameConverter>& converter)
  {
    converter_ = converter;
  }

  void OnVideoFrameConverted(unsigned char* aVideoFrame,
                             unsigned int aVideoFrameLength,
                             unsigned short aWidth,
                             unsigned short aHeight,
                             VideoType aVideoType,
                             uint64_t aCaptureTime)
  {
    MOZ_ASSERT(conduit_->type() == MediaSessionConduit::VIDEO);
    static_cast<VideoSessionConduit*>(conduit_.get())->SendVideoFrame(
      aVideoFrame, aVideoFrameLength, aWidth, aHeight, aVideoType, aCaptureTime);
  }

  void OnVideoFrameConverted(webrtc::I420VideoFrame& aVideoFrame)
  {
    MOZ_ASSERT(conduit_->type() == MediaSessionConduit::VIDEO);
    static_cast<VideoSessionConduit*>(conduit_.get())->SendVideoFrame(aVideoFrame);
  }
#endif

  // Implement MediaStreamTrackListener
  void NotifyQueuedChanges(MediaStreamGraph* aGraph,
                           StreamTime aTrackOffset,
                           const MediaSegment& aQueuedMedia) override;

  // Implement DirectMediaStreamTrackListener
  void NotifyRealtimeTrackData(MediaStreamGraph* aGraph,
                               StreamTime aTrackOffset,
                               const MediaSegment& aMedia) override;
  void NotifyDirectListenerInstalled(InstallationResult aResult) override;
  void NotifyDirectListenerUninstalled() override;

private:
  void UnsetTrackIdImpl() {
    MutexAutoLock lock(mMutex);
    track_id_ = track_id_external_ = TRACK_INVALID;
  }

  void NewData(MediaStreamGraph* graph,
               StreamTime offset,
               const MediaSegment& media);

  RefPtr<MediaSessionConduit> conduit_;
  RefPtr<AudioProxyThread> audio_processing_;
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  RefPtr<VideoFrameConverter> converter_;
#endif

  // May be TRACK_INVALID until we see data from the track
  TrackID track_id_; // this is the current TrackID this listener is attached to
  Mutex mMutex;
  // protected by mMutex
  // May be TRACK_INVALID until we see data from the track
  TrackID track_id_external_; // this is queried from other threads

  // active is true if there is a transport to send on
  mozilla::Atomic<bool> active_;
  // enabled is true if the media access control permits sending
  // actual content; when false you get black/silence
  mozilla::Atomic<bool> enabled_;

  // Written and read on the MediaStreamGraph thread
  bool direct_connect_;
};

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
// Implements VideoConverterListener for MediaPipeline.
//
// We pass converted frames on to MediaPipelineTransmit::PipelineListener
// where they are further forwarded to VideoConduit.
// MediaPipelineTransmit calls Detach() during shutdown to ensure there is
// no cyclic dependencies between us and PipelineListener.
class MediaPipelineTransmit::VideoFrameFeeder
  : public VideoConverterListener
{
public:
  explicit VideoFrameFeeder(const RefPtr<PipelineListener>& listener)
    : listener_(listener),
      mutex_("VideoFrameFeeder")
  {
    MOZ_COUNT_CTOR(VideoFrameFeeder);
  }

  void Detach()
  {
    MutexAutoLock lock(mutex_);

    listener_ = nullptr;
  }

  void OnVideoFrameConverted(unsigned char* aVideoFrame,
                             unsigned int aVideoFrameLength,
                             unsigned short aWidth,
                             unsigned short aHeight,
                             VideoType aVideoType,
                             uint64_t aCaptureTime) override
  {
    MutexAutoLock lock(mutex_);

    if (!listener_) {
      return;
    }

    listener_->OnVideoFrameConverted(aVideoFrame, aVideoFrameLength,
                                     aWidth, aHeight, aVideoType, aCaptureTime);
  }

  void OnVideoFrameConverted(webrtc::I420VideoFrame& aVideoFrame) override
  {
    MutexAutoLock lock(mutex_);

    if (!listener_) {
      return;
    }

    listener_->OnVideoFrameConverted(aVideoFrame);
  }

protected:
  virtual ~VideoFrameFeeder()
  {
    MOZ_COUNT_DTOR(VideoFrameFeeder);
  }

  RefPtr<PipelineListener> listener_;
  Mutex mutex_;
};
#endif

class MediaPipelineTransmit::PipelineVideoSink :
  public MediaStreamVideoSink
{
public:
  explicit PipelineVideoSink(const RefPtr<MediaSessionConduit>& conduit,
                             MediaPipelineTransmit::PipelineListener* listener)
    : conduit_(conduit)
    , pipelineListener_(listener)
  {
  }

  virtual void SetCurrentFrames(const VideoSegment& aSegment) override;
  virtual void ClearFrames() override {}

private:
  ~PipelineVideoSink() {
    // release conduit on mainthread.  Must use forget()!
    nsresult rv = NS_DispatchToMainThread(new
      ConduitDeleteEvent(conduit_.forget()));
    MOZ_ASSERT(!NS_FAILED(rv),"Could not dispatch conduit shutdown to main");
    if (NS_FAILED(rv)) {
      MOZ_CRASH();
    }
  }
  RefPtr<MediaSessionConduit> conduit_;
  MediaPipelineTransmit::PipelineListener* pipelineListener_;
};

MediaPipelineTransmit::MediaPipelineTransmit(
    const std::string& pc,
    nsCOMPtr<nsIEventTarget> main_thread,
    nsCOMPtr<nsIEventTarget> sts_thread,
    dom::MediaStreamTrack* domtrack,
    const std::string& track_id,
    int level,
    RefPtr<MediaSessionConduit> conduit,
    RefPtr<TransportFlow> rtp_transport,
    RefPtr<TransportFlow> rtcp_transport,
    nsAutoPtr<MediaPipelineFilter> filter) :
  MediaPipeline(pc, TRANSMIT, main_thread, sts_thread, track_id, level,
                conduit, rtp_transport, rtcp_transport, filter),
  listener_(new PipelineListener(conduit)),
  video_sink_(new PipelineVideoSink(conduit, listener_)),
  domtrack_(domtrack)
{
  if (!IsVideo()) {
    audio_processing_ = MakeAndAddRef<AudioProxyThread>(static_cast<AudioSessionConduit*>(conduit.get()));
    listener_->SetAudioProxy(audio_processing_);
  }
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  else { // Video
    // For video we send frames to an async VideoFrameConverter that calls
    // back to a VideoFrameFeeder that feeds I420 frames to VideoConduit.

    feeder_ = MakeAndAddRef<VideoFrameFeeder>(listener_);

    converter_ = MakeAndAddRef<VideoFrameConverter>();
    converter_->AddListener(feeder_);

    listener_->SetVideoFrameConverter(converter_);
  }
#endif
}

MediaPipelineTransmit::~MediaPipelineTransmit()
{
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  if (feeder_) {
    feeder_->Detach();
  }
#endif
}

nsresult MediaPipelineTransmit::Init() {
  AttachToTrack(track_id_);

  return MediaPipeline::Init();
}

void MediaPipelineTransmit::AttachToTrack(const std::string& track_id) {
  ASSERT_ON_THREAD(main_thread_);

  description_ = pc_ + "| ";
  description_ += conduit_->type() == MediaSessionConduit::AUDIO ?
      "Transmit audio[" : "Transmit video[";
  description_ += track_id;
  description_ += "]";

  // TODO(ekr@rtfm.com): Check for errors
  MOZ_MTLOG(ML_DEBUG, "Attaching pipeline to track "
            << static_cast<void *>(domtrack_) << " conduit type=" <<
            (conduit_->type() == MediaSessionConduit::AUDIO ?"audio":"video"));

  // Register the Listener directly with the source if we can.
  // We also register it as a non-direct listener so we fall back to that
  // if installing the direct listener fails. As a direct listener we get access
  // to direct unqueued (and not resampled) data.
  domtrack_->AddDirectListener(listener_);
  domtrack_->AddListener(listener_);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  domtrack_->AddDirectListener(video_sink_);
#endif

#ifndef MOZILLA_INTERNAL_API
  // this enables the unit tests that can't fiddle with principals and the like
  listener_->SetEnabled(true);
#endif
}

bool
MediaPipelineTransmit::IsVideo() const
{
  return !!domtrack_->AsVideoStreamTrack();
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
void MediaPipelineTransmit::UpdateSinkIdentity_m(MediaStreamTrack* track,
                                                 nsIPrincipal* principal,
                                                 const PeerIdentity* sinkIdentity) {
  ASSERT_ON_THREAD(main_thread_);

  if (track != nullptr && track != domtrack_) {
    // If a track is specified, then it might not be for this pipeline,
    // since we receive notifications for all tracks on the PC.
    // nullptr means that the PeerIdentity has changed and shall be applied
    // to all tracks of the PC.
    return;
  }

  bool enableTrack = principal->Subsumes(domtrack_->GetPrincipal());
  if (!enableTrack) {
    // first try didn't work, but there's a chance that this is still available
    // if our track is bound to a peerIdentity, and the peer connection (our
    // sink) is bound to the same identity, then we can enable the track.
    const PeerIdentity* trackIdentity = domtrack_->GetPeerIdentity();
    if (sinkIdentity && trackIdentity) {
      enableTrack = (*sinkIdentity == *trackIdentity);
    }
  }

  listener_->SetEnabled(enableTrack);
}
#endif

void
MediaPipelineTransmit::DetachMedia()
{
  ASSERT_ON_THREAD(main_thread_);
  if (domtrack_) {
    domtrack_->RemoveDirectListener(listener_);
    domtrack_->RemoveListener(listener_);
    domtrack_->RemoveDirectListener(video_sink_);
    domtrack_ = nullptr;
  }
  // Let the listener be destroyed with the pipeline (or later).
}

nsresult MediaPipelineTransmit::TransportReady_s(TransportInfo &info) {
  ASSERT_ON_THREAD(sts_thread_);
  // Call base ready function.
  MediaPipeline::TransportReady_s(info);

  // Should not be set for a transmitter
  if (&info == &rtp_) {
    listener_->SetActive(true);
  }

  return NS_OK;
}

nsresult MediaPipelineTransmit::ReplaceTrack(MediaStreamTrack& domtrack) {
  // MainThread, checked in calls we make
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  nsString nsTrackId;
  domtrack.GetId(nsTrackId);
  std::string track_id(NS_ConvertUTF16toUTF8(nsTrackId).get());
#else
  std::string track_id = domtrack.GetId();
#endif
  MOZ_MTLOG(ML_DEBUG, "Reattaching pipeline " << description_ << " to track "
            << static_cast<void *>(&domtrack)
            << " track " << track_id << " conduit type=" <<
            (conduit_->type() == MediaSessionConduit::AUDIO ?"audio":"video"));

  DetachMedia();
  domtrack_ = &domtrack; // Detach clears it
  // Unsets the track id after RemoveListener() takes effect.
  listener_->UnsetTrackId(domtrack_->GraphImpl());
  track_id_ = track_id;
  AttachToTrack(track_id);
  return NS_OK;
}

void MediaPipeline::DisconnectTransport_s(TransportInfo &info) {
  MOZ_ASSERT(info.transport_);
  ASSERT_ON_THREAD(sts_thread_);

  info.transport_->SignalStateChange.disconnect(this);
  // We do this even if we're a transmitter, since we are still possibly
  // registered to receive RTCP.
  TransportLayerDtls *dtls = static_cast<TransportLayerDtls *>(
      info.transport_->GetLayer(TransportLayerDtls::ID()));
  MOZ_ASSERT(dtls);  // DTLS is mandatory
  MOZ_ASSERT(dtls->downward());
  dtls->downward()->SignalPacketReceived.disconnect(this);
}

nsresult MediaPipeline::ConnectTransport_s(TransportInfo &info) {
  MOZ_ASSERT(info.transport_);
  ASSERT_ON_THREAD(sts_thread_);

  // Look to see if the transport is ready
  if (info.transport_->state() == TransportLayer::TS_OPEN) {
    nsresult res = TransportReady_s(info);
    if (NS_FAILED(res)) {
      MOZ_MTLOG(ML_ERROR, "Error calling TransportReady(); res="
                << static_cast<uint32_t>(res) << " in " << __FUNCTION__);
      return res;
    }
  } else if (info.transport_->state() == TransportLayer::TS_ERROR) {
    MOZ_MTLOG(ML_ERROR, ToString(info.type_)
                        << "transport is already in error state");
    TransportFailed_s(info);
    return NS_ERROR_FAILURE;
  }

  info.transport_->SignalStateChange.connect(this,
                                             &MediaPipeline::StateChange);

  return NS_OK;
}

MediaPipeline::TransportInfo* MediaPipeline::GetTransportInfo_s(
    TransportFlow *flow) {
  ASSERT_ON_THREAD(sts_thread_);
  if (flow == rtp_.transport_) {
    return &rtp_;
  }

  if (flow == rtcp_.transport_) {
    return &rtcp_;
  }

  return nullptr;
}

nsresult MediaPipeline::PipelineTransport::SendRtpPacket(
    const void *data, int len) {

    nsAutoPtr<DataBuffer> buf(new DataBuffer(static_cast<const uint8_t *>(data),
                                             len, len + SRTP_MAX_EXPANSION));

    RUN_ON_THREAD(sts_thread_,
                  WrapRunnable(
                      RefPtr<MediaPipeline::PipelineTransport>(this),
                      &MediaPipeline::PipelineTransport::SendRtpRtcpPacket_s,
                      buf, true),
                  NS_DISPATCH_NORMAL);

    return NS_OK;
}

nsresult MediaPipeline::PipelineTransport::SendRtpRtcpPacket_s(
    nsAutoPtr<DataBuffer> data,
    bool is_rtp) {

  ASSERT_ON_THREAD(sts_thread_);
  if (!pipeline_) {
    return NS_OK;  // Detached
  }
  TransportInfo& transport = is_rtp ? pipeline_->rtp_ : pipeline_->rtcp_;

  if (!transport.send_srtp_) {
    MOZ_MTLOG(ML_DEBUG, "Couldn't write RTP/RTCP packet; SRTP not set up yet");
    return NS_OK;
  }

  MOZ_ASSERT(transport.transport_);
  NS_ENSURE_TRUE(transport.transport_, NS_ERROR_NULL_POINTER);

  // libsrtp enciphers in place, so we need a big enough buffer.
  MOZ_ASSERT(data->capacity() >= data->len() + SRTP_MAX_EXPANSION);

  int out_len;
  nsresult res;
  if (is_rtp) {
    res = transport.send_srtp_->ProtectRtp(data->data(),
                                           data->len(),
                                           data->capacity(),
                                           &out_len);
  } else {
    res = transport.send_srtp_->ProtectRtcp(data->data(),
                                            data->len(),
                                            data->capacity(),
                                            &out_len);
  }
  if (!NS_SUCCEEDED(res)) {
    return res;
  }

  // paranoia; don't have uninitialized bytes included in data->len()
  data->SetLength(out_len);

  MOZ_MTLOG(ML_DEBUG, pipeline_->description_ << " sending " <<
            (is_rtp ? "RTP" : "RTCP") << " packet");
  if (is_rtp) {
    pipeline_->increment_rtp_packets_sent(out_len);
  } else {
    pipeline_->increment_rtcp_packets_sent();
  }
  return pipeline_->SendPacket(transport.transport_, data->data(), out_len);
}

nsresult MediaPipeline::PipelineTransport::SendRtcpPacket(
    const void *data, int len) {

    nsAutoPtr<DataBuffer> buf(new DataBuffer(static_cast<const uint8_t *>(data),
                                             len, len + SRTP_MAX_EXPANSION));

    RUN_ON_THREAD(sts_thread_,
                  WrapRunnable(
                      RefPtr<MediaPipeline::PipelineTransport>(this),
                      &MediaPipeline::PipelineTransport::SendRtpRtcpPacket_s,
                      buf, false),
                  NS_DISPATCH_NORMAL);

    return NS_OK;
}

void MediaPipelineTransmit::PipelineListener::
UnsetTrackId(MediaStreamGraphImpl* graph) {
#ifndef USE_FAKE_MEDIA_STREAMS
  class Message : public ControlMessage {
  public:
    explicit Message(PipelineListener* listener) :
      ControlMessage(nullptr), listener_(listener) {}
    virtual void Run() override
    {
      listener_->UnsetTrackIdImpl();
    }
    RefPtr<PipelineListener> listener_;
  };
  graph->AppendMessage(MakeUnique<Message>(this));
#else
  UnsetTrackIdImpl();
#endif
}
// Called if we're attached with AddDirectListener()
void MediaPipelineTransmit::PipelineListener::
NotifyRealtimeTrackData(MediaStreamGraph* graph,
                        StreamTime offset,
                        const MediaSegment& media) {
  MOZ_MTLOG(ML_DEBUG, "MediaPipeline::NotifyRealtimeTrackData() listener=" <<
                      this << ", offset=" << offset <<
                      ", duration=" << media.GetDuration());

  NewData(graph, offset, media);
}

void MediaPipelineTransmit::PipelineListener::
NotifyQueuedChanges(MediaStreamGraph* graph,
                    StreamTime offset,
                    const MediaSegment& queued_media) {
  MOZ_MTLOG(ML_DEBUG, "MediaPipeline::NotifyQueuedChanges()");

  // ignore non-direct data if we're also getting direct data
  if (!direct_connect_) {
    NewData(graph, offset, queued_media);
  }
}

void MediaPipelineTransmit::PipelineListener::
NotifyDirectListenerInstalled(InstallationResult aResult) {
  MOZ_MTLOG(ML_INFO, "MediaPipeline::NotifyDirectListenerInstalled() listener= " <<
                     this << ", result=" << static_cast<int32_t>(aResult));

  direct_connect_ = InstallationResult::SUCCESS == aResult;
}

void MediaPipelineTransmit::PipelineListener::
NotifyDirectListenerUninstalled() {
  MOZ_MTLOG(ML_INFO, "MediaPipeline::NotifyDirectListenerUninstalled() listener=" << this);

  direct_connect_ = false;
}

void MediaPipelineTransmit::PipelineListener::
NewData(MediaStreamGraph* graph,
        StreamTime offset,
        const MediaSegment& media) {
  if (!active_) {
    MOZ_MTLOG(ML_DEBUG, "Discarding packets because transport not ready");
    return;
  }

  if (conduit_->type() !=
      (media.GetType() == MediaSegment::AUDIO ? MediaSessionConduit::AUDIO :
                                                MediaSessionConduit::VIDEO)) {
    MOZ_ASSERT(false, "The media type should always be correct since the "
                      "listener is locked to a specific track");
    return;
  }

  // TODO(ekr@rtfm.com): For now assume that we have only one
  // track type and it's destined for us
  // See bug 784517
  if (media.GetType() == MediaSegment::AUDIO) {
    AudioSegment* audio = const_cast<AudioSegment *>(
        static_cast<const AudioSegment *>(&media));

    AudioSegment::ChunkIterator iter(*audio);
    while(!iter.IsEnded()) {
      TrackRate rate;
#ifdef USE_FAKE_MEDIA_STREAMS
      rate = Fake_MediaStream::GraphRate();
#else
      rate = graph->GraphRate();
#endif
      audio_processing_->QueueAudioChunk(rate, *iter, enabled_);
      iter.Next();
    }
  } else {
    // Ignore
  }
}

void MediaPipelineTransmit::PipelineVideoSink::
SetCurrentFrames(const VideoSegment& aSegment)
{
  MOZ_ASSERT(pipelineListener_);

  if (!pipelineListener_->active_) {
    MOZ_MTLOG(ML_DEBUG, "Discarding packets because transport not ready");
    return;
  }

  if (conduit_->type() != MediaSessionConduit::VIDEO) {
    // Ignore data of wrong kind in case we have a muxed stream
    return;
  }

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
    VideoSegment* video = const_cast<VideoSegment *>(&aSegment);

    VideoSegment::ChunkIterator iter(*video);
    while(!iter.IsEnded()) {
      pipelineListener_->converter_->QueueVideoChunk(*iter, !pipelineListener_->enabled_);
      iter.Next();
    }
#endif
}

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
  explicit GenericReceiveCallback(GenericReceiveListener* listener)
    : listener_(listener) {}

  void TrackAdded(TrackTicks time);

 private:
  RefPtr<GenericReceiveListener> listener_;
};

// Add a listener on the MSG thread using the MSG command queue
static void AddListener(MediaStream* source, MediaStreamListener* listener) {
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  class Message : public ControlMessage {
   public:
    Message(MediaStream* stream, MediaStreamListener* listener)
      : ControlMessage(stream),
        listener_(listener) {}

    virtual void Run() override {
      mStream->AddListenerImpl(listener_.forget());
    }
   private:
    RefPtr<MediaStreamListener> listener_;
  };

  MOZ_ASSERT(listener);

  source->GraphImpl()->AppendMessage(MakeUnique<Message>(source, listener));
#else
  source->AddListener(listener);
#endif
}

class GenericReceiveListener : public MediaStreamListener
{
 public:
  GenericReceiveListener(SourceMediaStream *source, TrackID track_id)
    : source_(source),
      track_id_(track_id),
      played_ticks_(0),
      principal_handle_(PRINCIPAL_HANDLE_NONE) {}

  virtual ~GenericReceiveListener() {}

  void AddSelf()
  {
    AddListener(source_, this);
  }

#ifndef USE_FAKE_MEDIA_STREAMS
  // Must be called on the main thread
  void SetPrincipalHandle_m(const PrincipalHandle& principal_handle)
  {
    class Message : public ControlMessage
    {
    public:
      Message(GenericReceiveListener* listener,
              MediaStream* stream,
              const PrincipalHandle& principal_handle)
        : ControlMessage(stream),
          listener_(listener),
          principal_handle_(principal_handle)
      {}

      void Run() override {
        listener_->SetPrincipalHandle_msg(principal_handle_);
      }

      RefPtr<GenericReceiveListener> listener_;
      PrincipalHandle principal_handle_;
    };

    source_->GraphImpl()->AppendMessage(MakeUnique<Message>(this, source_, principal_handle));
  }

  // Must be called on the MediaStreamGraph thread
  void SetPrincipalHandle_msg(const PrincipalHandle& principal_handle)
  {
    principal_handle_ = principal_handle;
  }
#endif // USE_FAKE_MEDIA_STREAMS

 protected:
  SourceMediaStream *source_;
  TrackID track_id_;
  TrackTicks played_ticks_;
  PrincipalHandle principal_handle_;
};

MediaPipelineReceive::MediaPipelineReceive(
    const std::string& pc,
    nsCOMPtr<nsIEventTarget> main_thread,
    nsCOMPtr<nsIEventTarget> sts_thread,
    SourceMediaStream *stream,
    const std::string& track_id,
    int level,
    RefPtr<MediaSessionConduit> conduit,
    RefPtr<TransportFlow> rtp_transport,
    RefPtr<TransportFlow> rtcp_transport,
    nsAutoPtr<MediaPipelineFilter> filter) :
  MediaPipeline(pc, RECEIVE, main_thread, sts_thread,
                track_id, level, conduit, rtp_transport,
                rtcp_transport, filter),
  stream_(stream),
  segments_added_(0)
{
  MOZ_ASSERT(stream_);
}

MediaPipelineReceive::~MediaPipelineReceive()
{
  MOZ_ASSERT(!stream_);  // Check that we have shut down already.
}

class MediaPipelineReceiveAudio::PipelineListener
  : public GenericReceiveListener
{
public:
  PipelineListener(SourceMediaStream * source, TrackID track_id,
                   const RefPtr<MediaSessionConduit>& conduit)
    : GenericReceiveListener(source, track_id),
      conduit_(conduit)
  {
  }

  ~PipelineListener()
  {
    if (!NS_IsMainThread()) {
      // release conduit on mainthread.  Must use forget()!
      nsresult rv = NS_DispatchToMainThread(new
                                            ConduitDeleteEvent(conduit_.forget()));
      MOZ_ASSERT(!NS_FAILED(rv),"Could not dispatch conduit shutdown to main");
      if (NS_FAILED(rv)) {
        MOZ_CRASH();
      }
    } else {
      conduit_ = nullptr;
    }
  }

  // Implement MediaStreamListener
  void NotifyPull(MediaStreamGraph* graph, StreamTime desired_time) override
  {
    MOZ_ASSERT(source_);
    if (!source_) {
      MOZ_MTLOG(ML_ERROR, "NotifyPull() called from a non-SourceMediaStream");
      return;
    }

    // This comparison is done in total time to avoid accumulated roundoff errors.
    while (source_->TicksToTimeRoundDown(WEBRTC_DEFAULT_SAMPLE_RATE,
                                         played_ticks_) < desired_time) {
      int16_t scratch_buffer[AUDIO_SAMPLE_BUFFER_MAX];

      int samples_length;

      // This fetches 10ms of data, either mono or stereo
      MediaConduitErrorCode err =
          static_cast<AudioSessionConduit*>(conduit_.get())->GetAudioFrame(
              scratch_buffer,
              WEBRTC_DEFAULT_SAMPLE_RATE,
              0,  // TODO(ekr@rtfm.com): better estimate of "capture" (really playout) delay
              samples_length);

      if (err != kMediaConduitNoError) {
        // Insert silence on conduit/GIPS failure (extremely unlikely)
        MOZ_MTLOG(ML_ERROR, "Audio conduit failed (" << err
                  << ") to return data @ " << played_ticks_
                  << " (desired " << desired_time << " -> "
                  << source_->StreamTimeToSeconds(desired_time) << ")");
        // if this is not enough we'll loop and provide more
        samples_length = WEBRTC_DEFAULT_SAMPLE_RATE/100;
        PodArrayZero(scratch_buffer);
      }

      MOZ_ASSERT(samples_length * sizeof(uint16_t) < AUDIO_SAMPLE_BUFFER_MAX);

      MOZ_MTLOG(ML_DEBUG, "Audio conduit returned buffer of length "
                << samples_length);

      RefPtr<SharedBuffer> samples = SharedBuffer::Create(samples_length * sizeof(uint16_t));
      int16_t *samples_data = static_cast<int16_t *>(samples->Data());
      AudioSegment segment;
      // We derive the number of channels of the stream from the number of samples
      // the AudioConduit gives us, considering it gives us packets of 10ms and we
      // know the rate.
      uint32_t channelCount = samples_length / (WEBRTC_DEFAULT_SAMPLE_RATE / 100);
      AutoTArray<int16_t*,2> channels;
      AutoTArray<const int16_t*,2> outputChannels;
      size_t frames = samples_length / channelCount;

      channels.SetLength(channelCount);

      size_t offset = 0;
      for (size_t i = 0; i < channelCount; i++) {
        channels[i] = samples_data + offset;
        offset += frames;
      }

      DeinterleaveAndConvertBuffer(scratch_buffer,
                                   frames,
                                   channelCount,
                                   channels.Elements());

      outputChannels.AppendElements(channels);

      segment.AppendFrames(samples.forget(), outputChannels, frames,
                           principal_handle_);

      // Handle track not actually added yet or removed/finished
      if (source_->AppendToTrack(track_id_, &segment)) {
        played_ticks_ += frames;
      } else {
        MOZ_MTLOG(ML_ERROR, "AppendToTrack failed");
        // we can't un-read the data, but that's ok since we don't want to
        // buffer - but don't i-loop!
        return;
      }
    }
  }

private:
  RefPtr<MediaSessionConduit> conduit_;
};

MediaPipelineReceiveAudio::MediaPipelineReceiveAudio(
    const std::string& pc,
    nsCOMPtr<nsIEventTarget> main_thread,
    nsCOMPtr<nsIEventTarget> sts_thread,
    SourceMediaStream* stream,
    const std::string& media_stream_track_id,
    TrackID numeric_track_id,
    int level,
    RefPtr<AudioSessionConduit> conduit,
    RefPtr<TransportFlow> rtp_transport,
    RefPtr<TransportFlow> rtcp_transport,
    nsAutoPtr<MediaPipelineFilter> filter) :
  MediaPipelineReceive(pc, main_thread, sts_thread,
                       stream, media_stream_track_id, level, conduit,
                       rtp_transport, rtcp_transport, filter),
  listener_(new PipelineListener(stream, numeric_track_id, conduit))
{}

void MediaPipelineReceiveAudio::DetachMedia()
{
  ASSERT_ON_THREAD(main_thread_);
  if (stream_) {
    stream_->RemoveListener(listener_);
    stream_ = nullptr;
  }
}

nsresult MediaPipelineReceiveAudio::Init() {
  ASSERT_ON_THREAD(main_thread_);
  MOZ_MTLOG(ML_DEBUG, __FUNCTION__);

  description_ = pc_ + "| Receive audio[";
  description_ += track_id_;
  description_ += "]";

  listener_->AddSelf();

  return MediaPipelineReceive::Init();
}

#ifndef USE_FAKE_MEDIA_STREAMS
void MediaPipelineReceiveAudio::SetPrincipalHandle_m(const PrincipalHandle& principal_handle)
{
  listener_->SetPrincipalHandle_m(principal_handle);
}
#endif // USE_FAKE_MEDIA_STREAMS

class MediaPipelineReceiveVideo::PipelineListener
  : public GenericReceiveListener {
public:
  PipelineListener(SourceMediaStream * source, TrackID track_id)
    : GenericReceiveListener(source, track_id),
      width_(0),
      height_(0),
#if defined(MOZILLA_INTERNAL_API)
      image_container_(),
      image_(),
#endif
      monitor_("Video PipelineListener")
  {
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
    image_container_ =
      LayerManager::CreateImageContainer(ImageContainer::ASYNCHRONOUS);
#endif
  }

  // Implement MediaStreamListener
  void NotifyPull(MediaStreamGraph* graph, StreamTime desired_time) override
  {
  #if defined(MOZILLA_INTERNAL_API)
    ReentrantMonitorAutoEnter enter(monitor_);

    RefPtr<Image> image = image_;
    StreamTime delta = desired_time - played_ticks_;

    // Don't append if we've already provided a frame that supposedly
    // goes past the current aDesiredTime Doing so means a negative
    // delta and thus messes up handling of the graph
    if (delta > 0) {
      VideoSegment segment;
      segment.AppendFrame(image.forget(), delta, IntSize(width_, height_),
                          principal_handle_);
      // Handle track not actually added yet or removed/finished
      if (source_->AppendToTrack(track_id_, &segment)) {
        played_ticks_ = desired_time;
      } else {
        MOZ_MTLOG(ML_ERROR, "AppendToTrack failed");
        return;
      }
    }
  #endif
  }

  // Accessors for external writes from the renderer
  void FrameSizeChange(unsigned int width,
                       unsigned int height,
                       unsigned int number_of_streams) {
    ReentrantMonitorAutoEnter enter(monitor_);

    width_ = width;
    height_ = height;
  }

  void RenderVideoFrame(const unsigned char* buffer,
                        size_t buffer_size,
                        uint32_t time_stamp,
                        int64_t render_time,
                        const RefPtr<layers::Image>& video_image)
  {
    RenderVideoFrame(buffer, buffer_size, width_, (width_ + 1) >> 1,
                     time_stamp, render_time, video_image);
  }

  void RenderVideoFrame(const unsigned char* buffer,
                        size_t buffer_size,
                        uint32_t y_stride,
                        uint32_t cbcr_stride,
                        uint32_t time_stamp,
                        int64_t render_time,
                        const RefPtr<layers::Image>& video_image)
  {
#ifdef MOZILLA_INTERNAL_API
    ReentrantMonitorAutoEnter enter(monitor_);
#endif // MOZILLA_INTERNAL_API

#if defined(MOZILLA_INTERNAL_API)
    if (buffer) {
      // Create a video frame using |buffer|.
#ifdef MOZ_WIDGET_GONK
      RefPtr<PlanarYCbCrImage> yuvImage = new GrallocImage();
#else
      RefPtr<PlanarYCbCrImage> yuvImage = image_container_->CreatePlanarYCbCrImage();
#endif
      uint8_t* frame = const_cast<uint8_t*>(static_cast<const uint8_t*> (buffer));

      PlanarYCbCrData yuvData;
      yuvData.mYChannel = frame;
      yuvData.mYSize = IntSize(y_stride, height_);
      yuvData.mYStride = y_stride;
      yuvData.mCbCrStride = cbcr_stride;
      yuvData.mCbChannel = frame + height_ * yuvData.mYStride;
      yuvData.mCrChannel = yuvData.mCbChannel + ((height_ + 1) >> 1) * yuvData.mCbCrStride;
      yuvData.mCbCrSize = IntSize(yuvData.mCbCrStride, (height_ + 1) >> 1);
      yuvData.mPicX = 0;
      yuvData.mPicY = 0;
      yuvData.mPicSize = IntSize(width_, height_);
      yuvData.mStereoMode = StereoMode::MONO;

      if (!yuvImage->CopyData(yuvData)) {
        MOZ_ASSERT(false);
        return;
      }

      image_ = yuvImage;
    }
#ifdef WEBRTC_GONK
    else {
      // Decoder produced video frame that can be appended to the track directly.
      MOZ_ASSERT(video_image);
      image_ = video_image;
    }
#endif // WEBRTC_GONK
#endif // MOZILLA_INTERNAL_API
  }

private:
  int width_;
  int height_;
#if defined(MOZILLA_INTERNAL_API)
  RefPtr<layers::ImageContainer> image_container_;
  RefPtr<layers::Image> image_;
#endif
  mozilla::ReentrantMonitor monitor_; // Monitor for processing WebRTC frames.
                                      // Protects image_ against:
                                      // - Writing from the GIPS thread
                                      // - Reading from the MSG thread
};

class MediaPipelineReceiveVideo::PipelineRenderer : public VideoRenderer
{
public:
  explicit PipelineRenderer(MediaPipelineReceiveVideo *pipeline) :
    pipeline_(pipeline) {}

  void Detach() { pipeline_ = nullptr; }

  // Implement VideoRenderer
  void FrameSizeChange(unsigned int width,
                       unsigned int height,
                       unsigned int number_of_streams) override
  {
    pipeline_->listener_->FrameSizeChange(width, height, number_of_streams);
  }

  void RenderVideoFrame(const unsigned char* buffer,
                        size_t buffer_size,
                        uint32_t time_stamp,
                        int64_t render_time,
                        const ImageHandle& handle) override
  {
    pipeline_->listener_->RenderVideoFrame(buffer, buffer_size,
                                           time_stamp, render_time,
                                           handle.GetImage());
  }

  void RenderVideoFrame(const unsigned char* buffer,
                        size_t buffer_size,
                        uint32_t y_stride,
                        uint32_t cbcr_stride,
                        uint32_t time_stamp,
                        int64_t render_time,
                        const ImageHandle& handle) override
  {
    pipeline_->listener_->RenderVideoFrame(buffer, buffer_size,
                                           y_stride, cbcr_stride,
                                           time_stamp, render_time,
                                           handle.GetImage());
  }

private:
  MediaPipelineReceiveVideo *pipeline_;  // Raw pointer to avoid cycles
};


MediaPipelineReceiveVideo::MediaPipelineReceiveVideo(
    const std::string& pc,
    nsCOMPtr<nsIEventTarget> main_thread,
    nsCOMPtr<nsIEventTarget> sts_thread,
    SourceMediaStream *stream,
    const std::string& media_stream_track_id,
    TrackID numeric_track_id,
    int level,
    RefPtr<VideoSessionConduit> conduit,
    RefPtr<TransportFlow> rtp_transport,
    RefPtr<TransportFlow> rtcp_transport,
    nsAutoPtr<MediaPipelineFilter> filter) :
  MediaPipelineReceive(pc, main_thread, sts_thread,
                       stream, media_stream_track_id, level, conduit,
                       rtp_transport, rtcp_transport, filter),
  renderer_(new PipelineRenderer(this)),
  listener_(new PipelineListener(stream, numeric_track_id))
{}

void MediaPipelineReceiveVideo::DetachMedia()
{
  ASSERT_ON_THREAD(main_thread_);

  // stop generating video and thus stop invoking the PipelineRenderer
  // and PipelineListener - the renderer has a raw ptr to the Pipeline to
  // avoid cycles, and the render callbacks are invoked from a different
  // thread so simple null-checks would cause TSAN bugs without locks.
  static_cast<VideoSessionConduit*>(conduit_.get())->DetachRenderer();
  if (stream_) {
    stream_->RemoveListener(listener_);
    stream_ = nullptr;
  }
}

nsresult MediaPipelineReceiveVideo::Init() {
  ASSERT_ON_THREAD(main_thread_);
  MOZ_MTLOG(ML_DEBUG, __FUNCTION__);

  description_ = pc_ + "| Receive video[";
  description_ += track_id_;
  description_ += "]";

#if defined(MOZILLA_INTERNAL_API)
  listener_->AddSelf();
#endif

  // Always happens before we can DetachMedia()
  static_cast<VideoSessionConduit *>(conduit_.get())->
      AttachRenderer(renderer_);

  return MediaPipelineReceive::Init();
}

#ifndef USE_FAKE_MEDIA_STREAMS
void MediaPipelineReceiveVideo::SetPrincipalHandle_m(const PrincipalHandle& principal_handle)
{
  listener_->SetPrincipalHandle_m(principal_handle);
}
#endif // USE_FAKE_MEDIA_STREAMS

}  // end namespace
