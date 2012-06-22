/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_CAPTURER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_CAPTURER_H_

#include "common_types.h"
#include "engine_configurations.h"
#include "modules/video_capture/main/interface/video_capture.h"
#include "modules/video_coding/codecs/interface/video_codec_interface.h"
#include "modules/video_coding/main/interface/video_coding.h"
#include "modules/video_processing/main/interface/video_processing.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "typedefs.h"
#include "video_engine/include/vie_capture.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_frame_provider_base.h"

namespace webrtc {

class CriticalSectionWrapper;
class EventWrapper;
class ProcessThread;
class ThreadWrapper;
class ViEEffectFilter;
class ViEEncoder;
struct ViEPicture;

class ViECapturer
    : public ViEFrameProviderBase,
      public ViEExternalCapture,
      protected VCMReceiveCallback,
      protected VideoCaptureDataCallback,
      protected VideoCaptureFeedBack,
      protected VideoEncoder {
 public:
  static ViECapturer* CreateViECapture(int capture_id,
                                       int engine_id,
                                       VideoCaptureModule& capture_module,
                                       ProcessThread& module_process_thread);

  static ViECapturer* CreateViECapture(
      int capture_id,
      int engine_id,
      const char* device_unique_idUTF8,
      WebRtc_UWord32 device_unique_idUTF8Length,
      ProcessThread& module_process_thread);

  ~ViECapturer();

  // Implements ViEFrameProviderBase.
  int FrameCallbackChanged();
  virtual int DeregisterFrameCallback(const ViEFrameCallback* callbackObject);
  bool IsFrameCallbackRegistered(const ViEFrameCallback* callbackObject);

  // Implements ExternalCapture.
  virtual int IncomingFrame(unsigned char* video_frame,
                            unsigned int video_frame_length,
                            unsigned short width, unsigned short height,
                            RawVideoType video_type,
                            unsigned long long capture_time = 0);

  virtual int IncomingFrameI420(const ViEVideoFrameI420& video_frame,
                                unsigned long long capture_time = 0);

  // Use this capture device as encoder.
  // Returns 0 if the codec is supported by this capture device.
  virtual WebRtc_Word32 PreEncodeToViEEncoder(const VideoCodec& codec,
                                              ViEEncoder& vie_encoder,
                                              WebRtc_Word32 vie_encoder_id);

  // Start/Stop.
  WebRtc_Word32 Start(
      const CaptureCapability& capture_capability = CaptureCapability());
  WebRtc_Word32 Stop();
  bool Started();

  // Overrides the capture delay.
  WebRtc_Word32 SetCaptureDelay(WebRtc_Word32 delay_ms);

  // Sets rotation of the incoming captured frame.
  WebRtc_Word32 SetRotateCapturedFrames(const RotateCapturedFrame rotation);

  // Effect filter.
  WebRtc_Word32 RegisterEffectFilter(ViEEffectFilter* effect_filter);
  WebRtc_Word32 EnableDenoising(bool enable);
  WebRtc_Word32 EnableDeflickering(bool enable);
  WebRtc_Word32 EnableBrightnessAlarm(bool enable);

  // Statistics observer.
  WebRtc_Word32 RegisterObserver(ViECaptureObserver& observer);
  WebRtc_Word32 DeRegisterObserver();
  bool IsObserverRegistered();

  // Information.
  const char* CurrentDeviceName() const;

  // Set device image.
  WebRtc_Word32 SetCaptureDeviceImage(const VideoFrame& capture_device_image);

 protected:
  ViECapturer(int capture_id,
              int engine_id,
              ProcessThread& module_process_thread);

  WebRtc_Word32 Init(VideoCaptureModule& capture_module);
  WebRtc_Word32 Init(const char* device_unique_idUTF8,
                     const WebRtc_UWord32 device_unique_idUTF8Length);

  // Implements VideoCaptureDataCallback.
  virtual void OnIncomingCapturedFrame(const WebRtc_Word32 id,
                                       VideoFrame& video_frame,
                                       VideoCodecType codec_type);
  virtual void OnCaptureDelayChanged(const WebRtc_Word32 id,
                                     const WebRtc_Word32 delay);

  bool EncoderActive();

  // Returns true if the capture capability has been set in |StartCapture|
  // function and may not be changed.
  bool CaptureCapabilityFixed();

  // Help function used for keeping track of VideoImageProcesingModule.
  // Creates the module if it is needed, returns 0 on success and guarantees
  // that the image proc module exist.
  WebRtc_Word32 IncImageProcRefCount();
  WebRtc_Word32 DecImageProcRefCount();

  // Implements VideoEncoder.
  virtual WebRtc_Word32 Version(char* version,
                                WebRtc_Word32 length) const;
  virtual WebRtc_Word32 InitEncode(const VideoCodec* codec_settings,
                                   WebRtc_Word32 number_of_cores,
                                   WebRtc_UWord32 max_payload_size);
  virtual WebRtc_Word32 Encode(const RawImage& input_image,
                               const CodecSpecificInfo* codec_specific_info,
                               const VideoFrameType frame_type);
  virtual WebRtc_Word32 RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback);
  virtual WebRtc_Word32 Release();
  virtual WebRtc_Word32 Reset();
  virtual WebRtc_Word32 SetChannelParameters(WebRtc_UWord32 packet_loss,
                                             int rtt);
  virtual WebRtc_Word32 SetRates(WebRtc_UWord32 new_bit_rate,
                                 WebRtc_UWord32 frame_rate);

  // Implements  VCMReceiveCallback.
  virtual WebRtc_Word32 FrameToRender(VideoFrame& video_frame);

  // Implements VideoCaptureFeedBack
  virtual void OnCaptureFrameRate(const WebRtc_Word32 id,
                                  const WebRtc_UWord32 frame_rate);
  virtual void OnNoPictureAlarm(const WebRtc_Word32 id,
                                const VideoCaptureAlarm alarm);

  // Thread functions for deliver captured frames to receivers.
  static bool ViECaptureThreadFunction(void* obj);
  bool ViECaptureProcess();

  void DeliverI420Frame(VideoFrame& video_frame);
  void DeliverCodedFrame(VideoFrame& video_frame);

 private:
  // Never take capture_cs_ before deliver_cs_!
  scoped_ptr<CriticalSectionWrapper> capture_cs_;
  scoped_ptr<CriticalSectionWrapper> deliver_cs_;
  VideoCaptureModule* capture_module_;
  VideoCaptureExternal* external_capture_module_;
  ProcessThread& module_process_thread_;
  const int capture_id_;

  // Capture thread.
  ThreadWrapper& capture_thread_;
  EventWrapper& capture_event_;
  EventWrapper& deliver_event_;

  VideoFrame captured_frame_;
  VideoFrame deliver_frame_;
  VideoFrame encoded_frame_;

  // Image processing.
  ViEEffectFilter* effect_filter_;
  VideoProcessingModule* image_proc_module_;
  int image_proc_module_ref_counter_;
  VideoProcessingModule::FrameStats* deflicker_frame_stats_;
  VideoProcessingModule::FrameStats* brightness_frame_stats_;
  Brightness current_brightness_level_;
  Brightness reported_brightness_level_;
  bool denoising_enabled_;

  // Statistics observer.
  scoped_ptr<CriticalSectionWrapper> observer_cs_;
  ViECaptureObserver* observer_;

  // Encoding using encoding capable cameras.
  scoped_ptr<CriticalSectionWrapper> encoding_cs_;
  VideoCaptureModule::VideoCaptureEncodeInterface* capture_encoder_;
  EncodedImageCallback* encode_complete_callback_;
  VideoCodec codec_;
  // The ViEEncoder we are encoding for.
  ViEEncoder* vie_encoder_;
  // ViEEncoder id we are encoding for.
  WebRtc_Word32 vie_encoder_id_;
  // Used for decoding preencoded frames.
  VideoCodingModule* vcm_;
  EncodedVideoData decode_buffer_;
  bool decoder_initialized_;
  CaptureCapability requested_capability_;

  VideoFrame capture_device_image_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_CAPTURER_H_
