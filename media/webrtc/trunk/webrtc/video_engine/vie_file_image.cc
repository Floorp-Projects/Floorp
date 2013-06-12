/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Placed first to get WEBRTC_VIDEO_ENGINE_FILE_API.
#include "engine_configurations.h"  // NOLINT

#ifdef WEBRTC_VIDEO_ENGINE_FILE_API

#include "video_engine/vie_file_image.h"

#include <stdio.h>  // NOLINT

#include "common_video/interface/video_image.h"
#include "common_video/jpeg/include/jpeg.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

int ViEFileImage::ConvertJPEGToVideoFrame(int engine_id,
                                          const char* file_nameUTF8,
                                          I420VideoFrame* video_frame) {
  // Read jpeg file into temporary buffer.
  EncodedImage image_buffer;

  FILE* image_file = fopen(file_nameUTF8, "rb");
  if (!image_file) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                 "%s could not open file %s", __FUNCTION__, file_nameUTF8);
    return -1;
  }
  if (fseek(image_file, 0, SEEK_END) != 0) {
    fclose(image_file);
    WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                 "ConvertJPEGToVideoFrame fseek SEEK_END error for file %s",
                 file_nameUTF8);
    return -1;
  }
  int buffer_size = ftell(image_file);
  if (buffer_size == -1) {
    fclose(image_file);
    WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                 "ConvertJPEGToVideoFrame could tell file size for file %s",
                 file_nameUTF8);
    return -1;
  }
  image_buffer._size = buffer_size;
  if (fseek(image_file, 0, SEEK_SET) != 0) {
    fclose(image_file);
    WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                 "ConvertJPEGToVideoFrame fseek SEEK_SET error for file %s",
                 file_nameUTF8);
    return -1;
  }
  image_buffer._buffer = new WebRtc_UWord8[ image_buffer._size + 1];
  if (image_buffer._size != fread(image_buffer._buffer, sizeof(WebRtc_UWord8),
                                  image_buffer._size, image_file)) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                 "%s could not read file %s", __FUNCTION__, file_nameUTF8);
    fclose(image_file);
    delete [] image_buffer._buffer;
    return -1;
  }
  fclose(image_file);

  int ret = ConvertJpegToI420(image_buffer, video_frame);

  delete [] image_buffer._buffer;
  image_buffer._buffer = NULL;

  if (ret == -1) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                 "%s could decode file %s from jpeg format", __FUNCTION__,
                 file_nameUTF8);
    return -1;
  } else if (ret == -3) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                 "%s could not convert jpeg's data to i420 format",
                 __FUNCTION__, file_nameUTF8);
  }
  return 0;
}

int ViEFileImage::ConvertPictureToI420VideoFrame(int engine_id,
                                                 const ViEPicture& picture,
                                                 I420VideoFrame* video_frame) {
  int half_width = (picture.width + 1) / 2;
  int half_height = (picture.height + 1) / 2;
  int size_uv = half_width * half_height;
  int size_y = picture.width * picture.height;
  return video_frame->CreateFrame(size_y, picture.data,
                                  size_uv, picture.data + size_y,
                                  size_uv, picture.data + size_y +
                                  size_uv,
                                  picture.width, picture.height,
                                  picture.width, half_width, half_width);
  return 0;
}

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_FILE_API
