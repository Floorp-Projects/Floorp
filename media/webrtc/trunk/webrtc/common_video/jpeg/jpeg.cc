/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WIN32)
#include <basetsd.h>
#endif
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

// NOTE(ajm): Path provided by gyp.
#include "libyuv.h"  // NOLINT
#include "libyuv/mjpeg_decoder.h"  // NOLINT

#include "webrtc/common_video/jpeg/data_manager.h"
#include "webrtc/common_video/jpeg/include/jpeg.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

extern "C" {
#if defined(USE_SYSTEM_LIBJPEG)
#include <jpeglib.h>
#else
#include "jpeglib.h"
#endif
}


namespace webrtc
{

// Error handler
struct myErrorMgr {

    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};
typedef struct myErrorMgr * myErrorPtr;

METHODDEF(void)
MyErrorExit (j_common_ptr cinfo)
{
    myErrorPtr myerr = (myErrorPtr) cinfo->err;

    // Return control to the setjmp point
    longjmp(myerr->setjmp_buffer, 1);
}

JpegEncoder::JpegEncoder()
{
   _cinfo = new jpeg_compress_struct;
    strcpy(_fileName, "Snapshot.jpg");
}

JpegEncoder::~JpegEncoder()
{
    if (_cinfo != NULL)
    {
        delete _cinfo;
        _cinfo = NULL;
    }
}


int32_t
JpegEncoder::SetFileName(const char* fileName)
{
    if (!fileName)
    {
        return -1;
    }

    if (fileName)
    {
        strncpy(_fileName, fileName, 256);
        _fileName[256] = 0;
    }
    return 0;
}


int32_t
JpegEncoder::Encode(const I420VideoFrame& inputImage)
{
    if (inputImage.IsZeroSize())
    {
        return -1;
    }
    if (inputImage.width() < 1 || inputImage.height() < 1)
    {
        return -1;
    }

    FILE* outFile = NULL;

    const int width = inputImage.width();
    const int height = inputImage.height();

    // Set error handler
    myErrorMgr      jerr;
    _cinfo->err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = MyErrorExit;
    // Establish the setjmp return context
    if (setjmp(jerr.setjmp_buffer))
    {
        // If we get here, the JPEG code has signaled an error.
        jpeg_destroy_compress(_cinfo);
        if (outFile != NULL)
        {
            fclose(outFile);
        }
        return -1;
    }

    if ((outFile = fopen(_fileName, "wb")) == NULL)
    {
        return -2;
    }
    // Create a compression object
    jpeg_create_compress(_cinfo);

    // Setting destination file
    jpeg_stdio_dest(_cinfo, outFile);

    // Set parameters for compression
    _cinfo->in_color_space = JCS_YCbCr;
    jpeg_set_defaults(_cinfo);

    _cinfo->image_width = width;
    _cinfo->image_height = height;
    _cinfo->input_components = 3;

    _cinfo->comp_info[0].h_samp_factor = 2;   // Y
    _cinfo->comp_info[0].v_samp_factor = 2;
    _cinfo->comp_info[1].h_samp_factor = 1;   // U
    _cinfo->comp_info[1].v_samp_factor = 1;
    _cinfo->comp_info[2].h_samp_factor = 1;   // V
    _cinfo->comp_info[2].v_samp_factor = 1;
    _cinfo->raw_data_in = TRUE;
    // Converting to a buffer
    // TODO(mikhal): This is a tmp implementation. Will update to use LibYuv
    // Encode when that becomes available.
    unsigned int length = CalcBufferSize(kI420, width, height);
    scoped_array<uint8_t> image_buffer(new uint8_t[length]);
    ExtractBuffer(inputImage, length, image_buffer.get());
    int height16 = (height + 15) & ~15;
    uint8_t* imgPtr = image_buffer.get();

    uint8_t* origImagePtr = NULL;
    if (height16 != height)
    {
        // Copy image to an adequate size buffer
        uint32_t requiredSize = CalcBufferSize(kI420, width, height16);
        origImagePtr = new uint8_t[requiredSize];
        memset(origImagePtr, 0, requiredSize);
        memcpy(origImagePtr, image_buffer.get(), length);
        imgPtr = origImagePtr;
    }

    jpeg_start_compress(_cinfo, TRUE);

    JSAMPROW y[16],u[8],v[8];
    JSAMPARRAY data[3];

    data[0] = y;
    data[1] = u;
    data[2] = v;

    int i, j;

    for (j = 0; j < height; j += 16)
    {
        for (i = 0; i < 16; i++)
        {
            y[i] = (JSAMPLE*)imgPtr + width * (i + j);

            if (i % 2 == 0)
            {
                u[i / 2] = (JSAMPLE*) imgPtr + width * height +
                            width / 2 * ((i + j) / 2);
                v[i / 2] = (JSAMPLE*) imgPtr + width * height +
                            width * height / 4 + width / 2 * ((i + j) / 2);
            }
        }
        jpeg_write_raw_data(_cinfo, data, 16);
    }

    jpeg_finish_compress(_cinfo);
    jpeg_destroy_compress(_cinfo);

    fclose(outFile);

    if (origImagePtr != NULL)
    {
        delete [] origImagePtr;
    }

    return 0;
}

int ConvertJpegToI420(const EncodedImage& input_image,
                      I420VideoFrame* output_image) {

  if (output_image == NULL)
    return -1;
  // TODO(mikhal): Update to use latest API from LibYuv when that becomes
  // available.
  libyuv::MJpegDecoder jpeg_decoder;
  bool ret = jpeg_decoder.LoadFrame(input_image._buffer, input_image._size);
  if (ret == false)
    return -1;
  if (jpeg_decoder.GetNumComponents() == 4)
    return -2;  // not supported.
  int width = jpeg_decoder.GetWidth();
  int height = jpeg_decoder.GetHeight();
  output_image->CreateEmptyFrame(width, height, width,
                                 (width + 1) / 2, (width + 1) / 2);
  return ConvertToI420(kMJPG,
                       input_image._buffer,
                       0, 0,  // no cropping
                       width, height,
                       input_image._size,
                       kRotateNone,
                       output_image);
}


}
