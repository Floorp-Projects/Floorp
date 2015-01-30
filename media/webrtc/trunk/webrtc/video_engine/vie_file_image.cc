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
#include "webrtc/engine_configurations.h"

#ifdef WEBRTC_VIDEO_ENGINE_FILE_API

#include "webrtc/video_engine/vie_file_image.h"

#include <stdio.h>  // NOLINT

#include "webrtc/common_video/interface/video_image.h"
#include "webrtc/common_video/jpeg/include/jpeg.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

namespace webrtc {

int ViEFileImage::ConvertJPEGToVideoFrame(int engine_id,
                                          const char* file_nameUTF8,
                                          I420VideoFrame* video_frame) {
  // Read jpeg file into temporary buffer.
  EncodedImage image_buffer;

  FILE* image_file = fopen(file_nameUTF8, "rb");
  if (!image_file) {
    return -1;
  }
  if (fseek(image_file, 0, SEEK_END) != 0) {
    fclose(image_file);
    return -1;
  }
  int buffer_size = ftell(image_file);
  if (buffer_size == -1) {
    fclose(image_file);
    return -1;
  }
  image_buffer._size = buffer_size;
  if (fseek(image_file, 0, SEEK_SET) != 0) {
    fclose(image_file);
    return -1;
  }
  image_buffer._buffer = new uint8_t[ image_buffer._size + 1];
  if (image_buffer._size != fread(image_buffer._buffer, sizeof(uint8_t),
                                  image_buffer._size, image_file)) {
    fclose(image_file);
    delete [] image_buffer._buffer;
    return -1;
  }
  fclose(image_file);

  int ret = ConvertJpegToI420(image_buffer, video_frame);

  delete [] image_buffer._buffer;
  image_buffer._buffer = NULL;

  if (ret == -1) {
    return -1;
  } else if (ret == -3) {
  }
  return 0;
}

int ViEFileImage::ConvertPictureToI420VideoFrame(int engine_id,
                                                 const ViEPicture& picture,
                                                 I420VideoFrame* video_frame) {
  int half_width = (picture.width + 1) / 2;
  video_frame->CreateEmptyFrame(picture.width, picture.height,
                                picture.width, half_width, half_width);
  return ConvertToI420(kI420, picture.data, 0, 0,
                       picture.width, picture.height,
                       0, kRotateNone, video_frame);
}

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_FILE_API
