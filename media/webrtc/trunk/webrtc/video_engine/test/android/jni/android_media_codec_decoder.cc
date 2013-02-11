/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <android/log.h>
#define LOG_TAG "AndroidMediaCodecDecoder"

#include <jni.h>

#include "android_media_codec_decoder.h"

namespace webrtc {

AndroidMediaCodecDecoder::AndroidMediaCodecDecoder(
    JavaVM* vm, jobject surface, jclass decoderClass)
  : decode_complete_callback_(NULL),
    vm_(vm),
    surface_(surface),
    mediaCodecDecoder_(NULL),
    decoderClass_(decoderClass),
    env_(NULL),
    setEncodedImageID_(NULL),
    vm_attached_(false) {
}

WebRtc_Word32 AndroidMediaCodecDecoder::InitDecode(
    const VideoCodec* codecSettings, WebRtc_Word32 numberOfCores) {
  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "%s", __func__);

  // TODO(dwkang): Detach this thread from VM. => this leads to a crash on
  //               "StopCall".
  int ret = vm_->AttachCurrentThread(&env_, NULL);
  // Get the JNI env for this thread
  if ((ret < 0) || !env_) {
      __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,
                          "Could not attach thread to JVM (%d, %p)", ret,
                          env_);
      return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  } else {
    vm_attached_ = true;
  }

  // Initialize the media codec java decoder class.
  jmethodID mid = env_->GetMethodID(decoderClass_, "<init>", "()V");
  mediaCodecDecoder_ = env_->NewGlobalRef(env_->NewObject(decoderClass_, mid));

  mid = env_->GetMethodID(
      decoderClass_, "configure", "(Landroid/view/SurfaceView;II)V");
  env_->CallVoidMethod(mediaCodecDecoder_, mid, surface_,
                       codecSettings->width, codecSettings->height);

  setEncodedImageID_ = env_->GetMethodID(
      decoderClass_, "setEncodedImage", "(Ljava/nio/ByteBuffer;J)V");

  // Call start()
  jmethodID startID = env_->GetMethodID(decoderClass_, "start", "()V");
  env_->CallVoidMethod(mediaCodecDecoder_, startID);
  return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32 AndroidMediaCodecDecoder::Decode(
    const EncodedImage& inputImage,
    bool missingFrames,
    const RTPFragmentationHeader* fragmentation,
    const CodecSpecificInfo* codecSpecificInfo,
    WebRtc_Word64 renderTimeMs) {
  if (!vm_attached_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  jobject byteBuffer =
      env_->NewDirectByteBuffer(inputImage._buffer, inputImage._length);
  env_->CallVoidMethod(
      mediaCodecDecoder_, setEncodedImageID_, byteBuffer, renderTimeMs);
  env_->DeleteLocalRef(byteBuffer);

  return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
}

WebRtc_Word32 AndroidMediaCodecDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "%s", __func__);
  return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32 AndroidMediaCodecDecoder::Release() {
  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "%s", __func__);
  env_->DeleteGlobalRef(mediaCodecDecoder_);
  mediaCodecDecoder_ = NULL;

  return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32 AndroidMediaCodecDecoder::Reset() {
  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "%s", __func__);
  return WEBRTC_VIDEO_CODEC_OK;
}

}  // namespace webrtc
