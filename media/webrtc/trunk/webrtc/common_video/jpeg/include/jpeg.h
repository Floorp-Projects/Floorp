/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_VIDEO_JPEG
#define WEBRTC_COMMON_VIDEO_JPEG

#include "typedefs.h"
#include "common_video/interface/i420_video_frame.h"
#include "common_video/interface/video_image.h"  // EncodedImage

// jpeg forward declaration
struct jpeg_compress_struct;

namespace webrtc
{

// TODO(mikhal): Move this to LibYuv wrapper, when LibYuv will have a JPG
// Encode.
class JpegEncoder
{
public:
    JpegEncoder();
    ~JpegEncoder();

// SetFileName
// Input:
//  - fileName - Pointer to input vector (should be less than 256) to which the
//               compressed  file will be written to
//    Output:
//    - 0             : OK
//    - (-1)          : Error
    int32_t SetFileName(const char* fileName);

// Encode an I420 image. The encoded image is saved to a file
//
// Input:
//          - inputImage        : Image to be encoded
//
//    Output:
//    - 0             : OK
//    - (-1)          : Error
    int32_t Encode(const I420VideoFrame& inputImage);

private:

    jpeg_compress_struct*   _cinfo;
    char                    _fileName[257];
};

// Decodes a JPEG-stream
// Supports 1 image component. 3 interleaved image components,
// YCbCr sub-sampling  4:4:4, 4:2:2, 4:2:0.
//
// Input:
//    - input_image        : encoded image to be decoded.
//    - output_image       : VideoFrame to store decoded output.
//
//    Output:
//    - 0             : OK
//    - (-1)          : Error
//    - (-2)          : Unsupported format
int ConvertJpegToI420(const EncodedImage& input_image,
                      I420VideoFrame* output_image);
}
#endif /* WEBRTC_COMMON_VIDEO_JPEG  */
