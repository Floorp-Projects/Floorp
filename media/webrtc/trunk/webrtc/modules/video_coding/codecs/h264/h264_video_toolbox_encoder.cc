/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/video_coding/codecs/h264/h264_video_toolbox_encoder.h"

#if defined(WEBRTC_VIDEO_TOOLBOX_SUPPORTED)

#include <string>
#include <vector>

#include "libyuv/convert_from.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/video_coding/codecs/h264/h264_video_toolbox_nalu.h"

namespace internal {

// Convenience function for creating a dictionary.
inline CFDictionaryRef CreateCFDictionary(CFTypeRef* keys,
                                          CFTypeRef* values,
                                          size_t size) {
  return CFDictionaryCreate(kCFAllocatorDefault, keys, values, size,
                            &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}

// Copies characters from a CFStringRef into a std::string.
std::string CFStringToString(const CFStringRef cf_string) {
  RTC_DCHECK(cf_string);
  std::string std_string;
  // Get the size needed for UTF8 plus terminating character.
  size_t buffer_size =
      CFStringGetMaximumSizeForEncoding(CFStringGetLength(cf_string),
                                        kCFStringEncodingUTF8) +
      1;
  rtc::scoped_ptr<char[]> buffer(new char[buffer_size]);
  if (CFStringGetCString(cf_string, buffer.get(), buffer_size,
                         kCFStringEncodingUTF8)) {
    // Copy over the characters.
    std_string.assign(buffer.get());
  }
  return std_string;
}

// Convenience function for setting a VT property.
void SetVTSessionProperty(VTSessionRef session,
                          CFStringRef key,
                          int32_t value) {
  CFNumberRef cfNum =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
  OSStatus status = VTSessionSetProperty(session, key, cfNum);
  CFRelease(cfNum);
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                  << " to " << value << ": " << status;
  }
}

// Convenience function for setting a VT property.
void SetVTSessionProperty(VTSessionRef session, CFStringRef key, bool value) {
  CFBooleanRef cf_bool = (value) ? kCFBooleanTrue : kCFBooleanFalse;
  OSStatus status = VTSessionSetProperty(session, key, cf_bool);
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                  << " to " << value << ": " << status;
  }
}

// Convenience function for setting a VT property.
void SetVTSessionProperty(VTSessionRef session,
                          CFStringRef key,
                          CFStringRef value) {
  OSStatus status = VTSessionSetProperty(session, key, value);
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    std::string val_string = CFStringToString(value);
    LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                  << " to " << val_string << ": " << status;
  }
}

// Struct that we pass to the encoder per frame to encode. We receive it again
// in the encoder callback.
struct FrameEncodeParams {
  FrameEncodeParams(webrtc::EncodedImageCallback* cb,
                    const webrtc::CodecSpecificInfo* csi,
                    int32_t w,
                    int32_t h,
                    int64_t rtms,
                    uint32_t ts)
      : callback(cb), width(w), height(h), render_time_ms(rtms), timestamp(ts) {
    if (csi) {
      codec_specific_info = *csi;
    } else {
      codec_specific_info.codecType = webrtc::kVideoCodecH264;
    }
  }
  webrtc::EncodedImageCallback* callback;
  webrtc::CodecSpecificInfo codec_specific_info;
  int32_t width;
  int32_t height;
  int64_t render_time_ms;
  uint32_t timestamp;
};

// We receive I420Frames as input, but we need to feed CVPixelBuffers into the
// encoder. This performs the copy and format conversion.
// TODO(tkchin): See if encoder will accept i420 frames and compare performance.
bool CopyVideoFrameToPixelBuffer(const webrtc::VideoFrame& frame,
                                 CVPixelBufferRef pixel_buffer) {
  RTC_DCHECK(pixel_buffer);
  RTC_DCHECK(CVPixelBufferGetPixelFormatType(pixel_buffer) ==
             kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);
  RTC_DCHECK(CVPixelBufferGetHeightOfPlane(pixel_buffer, 0) ==
             static_cast<size_t>(frame.height()));
  RTC_DCHECK(CVPixelBufferGetWidthOfPlane(pixel_buffer, 0) ==
             static_cast<size_t>(frame.width()));

  CVReturn cvRet = CVPixelBufferLockBaseAddress(pixel_buffer, 0);
  if (cvRet != kCVReturnSuccess) {
    LOG(LS_ERROR) << "Failed to lock base address: " << cvRet;
    return false;
  }
  uint8_t* dst_y = reinterpret_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 0));
  int dst_stride_y = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 0);
  uint8_t* dst_uv = reinterpret_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 1));
  int dst_stride_uv = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 1);
  // Convert I420 to NV12.
  int ret = libyuv::I420ToNV12(
      frame.buffer(webrtc::kYPlane), frame.stride(webrtc::kYPlane),
      frame.buffer(webrtc::kUPlane), frame.stride(webrtc::kUPlane),
      frame.buffer(webrtc::kVPlane), frame.stride(webrtc::kVPlane), dst_y,
      dst_stride_y, dst_uv, dst_stride_uv, frame.width(), frame.height());
  CVPixelBufferUnlockBaseAddress(pixel_buffer, 0);
  if (ret) {
    LOG(LS_ERROR) << "Error converting I420 VideoFrame to NV12 :" << ret;
    return false;
  }
  return true;
}

// This is the callback function that VideoToolbox calls when encode is
// complete.
void VTCompressionOutputCallback(void* encoder,
                                 void* params,
                                 OSStatus status,
                                 VTEncodeInfoFlags info_flags,
                                 CMSampleBufferRef sample_buffer) {
  rtc::scoped_ptr<FrameEncodeParams> encode_params(
      reinterpret_cast<FrameEncodeParams*>(params));
  if (status != noErr) {
    LOG(LS_ERROR) << "H264 encoding failed.";
    return;
  }
  if (info_flags & kVTEncodeInfo_FrameDropped) {
    LOG(LS_INFO) << "H264 encode dropped frame.";
  }

  bool is_keyframe = false;
  CFArrayRef attachments =
      CMSampleBufferGetSampleAttachmentsArray(sample_buffer, 0);
  if (attachments != nullptr && CFArrayGetCount(attachments)) {
    CFDictionaryRef attachment =
        static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, 0));
    is_keyframe =
        !CFDictionaryContainsKey(attachment, kCMSampleAttachmentKey_NotSync);
  }

  // Convert the sample buffer into a buffer suitable for RTP packetization.
  // TODO(tkchin): Allocate buffers through a pool.
  rtc::scoped_ptr<rtc::Buffer> buffer(new rtc::Buffer());
  rtc::scoped_ptr<webrtc::RTPFragmentationHeader> header;
  if (!H264CMSampleBufferToAnnexBBuffer(sample_buffer, is_keyframe,
                                        buffer.get(), header.accept())) {
    return;
  }
  webrtc::EncodedImage frame(buffer->data(), buffer->size(), buffer->size());
  frame._encodedWidth = encode_params->width;
  frame._encodedHeight = encode_params->height;
  frame._completeFrame = true;
  frame._frameType =
      is_keyframe ? webrtc::kVideoFrameKey : webrtc::kVideoFrameDelta;
  frame.capture_time_ms_ = encode_params->render_time_ms;
  frame._timeStamp = encode_params->timestamp;

  int result = encode_params->callback->Encoded(
      frame, &(encode_params->codec_specific_info), header.get());
  if (result != 0) {
    LOG(LS_ERROR) << "Encoded callback failed: " << result;
  }
}

}  // namespace internal

namespace webrtc {

H264VideoToolboxEncoder::H264VideoToolboxEncoder()
    : callback_(nullptr), compression_session_(nullptr) {}

H264VideoToolboxEncoder::~H264VideoToolboxEncoder() {
  DestroyCompressionSession();
}

int H264VideoToolboxEncoder::InitEncode(const VideoCodec* codec_settings,
                                        int number_of_cores,
                                        size_t max_payload_size) {
  RTC_DCHECK(codec_settings);
  RTC_DCHECK_EQ(codec_settings->codecType, kVideoCodecH264);
  // TODO(tkchin): We may need to enforce width/height dimension restrictions
  // to match what the encoder supports.
  width_ = codec_settings->width;
  height_ = codec_settings->height;
  // We can only set average bitrate on the HW encoder.
  bitrate_ = codec_settings->startBitrate * 1000;

  // TODO(tkchin): Try setting payload size via
  // kVTCompressionPropertyKey_MaxH264SliceBytes.

  return ResetCompressionSession();
}

int H264VideoToolboxEncoder::Encode(
    const VideoFrame& input_image,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  if (input_image.IsZeroSize()) {
    // It's possible to get zero sizes as a signal to produce keyframes (this
    // happens for internal sources). But this shouldn't happen in
    // webrtcvideoengine2.
    RTC_NOTREACHED();
    return WEBRTC_VIDEO_CODEC_OK;
  }
  if (!callback_ || !compression_session_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  // Get a pixel buffer from the pool and copy frame data over.
  CVPixelBufferPoolRef pixel_buffer_pool =
      VTCompressionSessionGetPixelBufferPool(compression_session_);
  CVPixelBufferRef pixel_buffer = nullptr;
  CVReturn ret = CVPixelBufferPoolCreatePixelBuffer(nullptr, pixel_buffer_pool,
                                                    &pixel_buffer);
  if (ret != kCVReturnSuccess) {
    LOG(LS_ERROR) << "Failed to create pixel buffer: " << ret;
    // We probably want to drop frames here, since failure probably means
    // that the pool is empty.
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  RTC_DCHECK(pixel_buffer);
  if (!internal::CopyVideoFrameToPixelBuffer(input_image, pixel_buffer)) {
    LOG(LS_ERROR) << "Failed to copy frame data.";
    CVBufferRelease(pixel_buffer);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Check if we need a keyframe.
  bool is_keyframe_required = false;
  if (frame_types) {
    for (auto frame_type : *frame_types) {
      if (frame_type == kVideoFrameKey) {
        is_keyframe_required = true;
        break;
      }
    }
  }

  CMTime presentation_time_stamp =
      CMTimeMake(input_image.render_time_ms(), 1000);
  CFDictionaryRef frame_properties = nullptr;
  if (is_keyframe_required) {
    CFTypeRef keys[] = {kVTEncodeFrameOptionKey_ForceKeyFrame};
    CFTypeRef values[] = {kCFBooleanTrue};
    frame_properties = internal::CreateCFDictionary(keys, values, 1);
  }
  rtc::scoped_ptr<internal::FrameEncodeParams> encode_params;
  encode_params.reset(new internal::FrameEncodeParams(
      callback_, codec_specific_info, width_, height_,
      input_image.render_time_ms(), input_image.timestamp()));
  VTCompressionSessionEncodeFrame(
      compression_session_, pixel_buffer, presentation_time_stamp,
      kCMTimeInvalid, frame_properties, encode_params.release(), nullptr);
  if (frame_properties) {
    CFRelease(frame_properties);
  }
  if (pixel_buffer) {
    CVBufferRelease(pixel_buffer);
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264VideoToolboxEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264VideoToolboxEncoder::SetChannelParameters(uint32_t packet_loss,
                                                  int64_t rtt) {
  // Encoder doesn't know anything about packet loss or rtt so just return.
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264VideoToolboxEncoder::SetRates(uint32_t new_bitrate_kbit,
                                      uint32_t frame_rate) {
  bitrate_ = new_bitrate_kbit * 1000;
  if (compression_session_) {
    internal::SetVTSessionProperty(compression_session_,
                                   kVTCompressionPropertyKey_AverageBitRate,
                                   bitrate_);
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264VideoToolboxEncoder::Release() {
  callback_ = nullptr;
  // Need to reset to that the session is invalidated and won't use the
  // callback anymore.
  return ResetCompressionSession();
}

int H264VideoToolboxEncoder::ResetCompressionSession() {
  DestroyCompressionSession();

  // Set source image buffer attributes. These attributes will be present on
  // buffers retrieved from the encoder's pixel buffer pool.
  const size_t attributes_size = 3;
  CFTypeRef keys[attributes_size] = {
#if defined(WEBRTC_IOS)
    kCVPixelBufferOpenGLESCompatibilityKey,
#elif defined(WEBRTC_MAC)
    kCVPixelBufferOpenGLCompatibilityKey,
#endif
    kCVPixelBufferIOSurfacePropertiesKey,
    kCVPixelBufferPixelFormatTypeKey
  };
  CFDictionaryRef io_surface_value =
      internal::CreateCFDictionary(nullptr, nullptr, 0);
  int64_t nv12type = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
  CFNumberRef pixel_format =
      CFNumberCreate(nullptr, kCFNumberLongType, &nv12type);
  CFTypeRef values[attributes_size] = {kCFBooleanTrue, io_surface_value,
                                       pixel_format};
  CFDictionaryRef source_attributes =
      internal::CreateCFDictionary(keys, values, attributes_size);
  if (io_surface_value) {
    CFRelease(io_surface_value);
    io_surface_value = nullptr;
  }
  if (pixel_format) {
    CFRelease(pixel_format);
    pixel_format = nullptr;
  }
  OSStatus status = VTCompressionSessionCreate(
      nullptr,  // use default allocator
      width_, height_, kCMVideoCodecType_H264,
      nullptr,  // use default encoder
      source_attributes,
      nullptr,  // use default compressed data allocator
      internal::VTCompressionOutputCallback, this, &compression_session_);
  if (source_attributes) {
    CFRelease(source_attributes);
    source_attributes = nullptr;
  }
  if (status != noErr) {
    LOG(LS_ERROR) << "Failed to create compression session: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ConfigureCompressionSession();
  return WEBRTC_VIDEO_CODEC_OK;
}

void H264VideoToolboxEncoder::ConfigureCompressionSession() {
  RTC_DCHECK(compression_session_);
  internal::SetVTSessionProperty(compression_session_,
                                 kVTCompressionPropertyKey_RealTime, true);
  internal::SetVTSessionProperty(compression_session_,
                                 kVTCompressionPropertyKey_ProfileLevel,
                                 kVTProfileLevel_H264_Baseline_AutoLevel);
  internal::SetVTSessionProperty(
      compression_session_, kVTCompressionPropertyKey_AverageBitRate, bitrate_);
  internal::SetVTSessionProperty(compression_session_,
                                 kVTCompressionPropertyKey_AllowFrameReordering,
                                 false);
  // TODO(tkchin): Look at entropy mode and colorspace matrices.
  // TODO(tkchin): Investigate to see if there's any way to make this work.
  // May need it to interop with Android. Currently this call just fails.
  // On inspecting encoder output on iOS8, this value is set to 6.
  // internal::SetVTSessionProperty(compression_session_,
  //     kVTCompressionPropertyKey_MaxFrameDelayCount,
  //     1);
  // TODO(tkchin): See if enforcing keyframe frequency is beneficial in any
  // way.
  // internal::SetVTSessionProperty(
  //     compression_session_,
  //     kVTCompressionPropertyKey_MaxKeyFrameInterval, 240);
  // internal::SetVTSessionProperty(
  //     compression_session_,
  //     kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, 240);
}

void H264VideoToolboxEncoder::DestroyCompressionSession() {
  if (compression_session_) {
    VTCompressionSessionInvalidate(compression_session_);
    CFRelease(compression_session_);
    compression_session_ = nullptr;
  }
}

const char* H264VideoToolboxEncoder::ImplementationName() const {
  return "VideoToolbox";
}

}  // namespace webrtc

#endif  // defined(WEBRTC_VIDEO_TOOLBOX_SUPPORTED)
