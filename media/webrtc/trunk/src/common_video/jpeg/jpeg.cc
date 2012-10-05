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

#include "common_video/jpeg/include/jpeg.h"
#include "common_video/jpeg/data_manager.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"

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


WebRtc_Word32
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


WebRtc_Word32
JpegEncoder::Encode(const VideoFrame& inputImage)
{
    if (inputImage.Buffer() == NULL || inputImage.Size() == 0)
    {
        return -1;
    }
    if (inputImage.Width() < 1 || inputImage.Height() < 1)
    {
        return -1;
    }

    FILE* outFile = NULL;

    const WebRtc_UWord32 width = inputImage.Width();
    const WebRtc_UWord32 height = inputImage.Height();

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

    WebRtc_UWord32 height16 = (height + 15) & ~15;
    WebRtc_UWord8* imgPtr = inputImage.Buffer();
    WebRtc_UWord8* origImagePtr = NULL;
    if (height16 != height)
    {
        // Copy image to an adequate size buffer
        WebRtc_UWord32 requiredSize = CalcBufferSize(kI420, width, height16);
        origImagePtr = new WebRtc_UWord8[requiredSize];
        memset(origImagePtr, 0, requiredSize);
        memcpy(origImagePtr, inputImage.Buffer(), inputImage.Length());
        imgPtr = origImagePtr;
    }

    jpeg_start_compress(_cinfo, TRUE);

    JSAMPROW y[16],u[8],v[8];
    JSAMPARRAY data[3];

    data[0] = y;
    data[1] = u;
    data[2] = v;

    WebRtc_UWord32 i, j;

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

JpegDecoder::JpegDecoder()
{
    _cinfo = new jpeg_decompress_struct;
}

JpegDecoder::~JpegDecoder()
{
    if (_cinfo != NULL)
    {
        delete _cinfo;
        _cinfo = NULL;
    }
}

WebRtc_Word32
JpegDecoder::Decode(const EncodedImage& inputImage,
                    VideoFrame& outputImage)
{

    WebRtc_UWord8* tmpBuffer = NULL;
    // Set error handler
    myErrorMgr    jerr;
    _cinfo->err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = MyErrorExit;

    // Establish the setjmp return context
    if (setjmp(jerr.setjmp_buffer))
    {
        if (_cinfo->is_decompressor)
        {
            jpeg_destroy_decompress(_cinfo);
        }
        if (tmpBuffer != NULL)
        {
            delete [] tmpBuffer;
        }
        return -1;
    }

    _cinfo->out_color_space = JCS_YCbCr;

    // Create decompression object
    jpeg_create_decompress(_cinfo);

    // Specify data source
    jpegSetSrcBuffer(_cinfo, (JOCTET*) inputImage._buffer, inputImage._size);

    // Read header data
    jpeg_read_header(_cinfo, TRUE);

    _cinfo->raw_data_out = TRUE;
    jpeg_start_decompress(_cinfo);

    // Check header
    if (_cinfo->num_components == 4)
    {
        return -2; // not supported
    }
    if (_cinfo->progressive_mode == 1)
    {
        return -2; // not supported
    }


    WebRtc_UWord32 height = _cinfo->image_height;
    WebRtc_UWord32 width = _cinfo->image_width;

    // Making sure width and height are even
    if (height % 2)
    {
        height++;
    }
    if (width % 2)
    {
         width++;
    }

    WebRtc_UWord32 height16 = (height + 15) & ~15;
    WebRtc_UWord32 stride = (width + 15) & ~15;
    WebRtc_UWord32 uvStride = ((((stride + 1) >> 1) + 15) & ~15);

    WebRtc_UWord32 tmpRequiredSize =  stride * height16 +
                                      2 * (uvStride * ((height16 + 1) >> 1));
    WebRtc_UWord32 requiredSize = width * height * 3 >> 1;

    // Verify sufficient buffer size.
    outputImage.VerifyAndAllocate(requiredSize);
    WebRtc_UWord8* outPtr = outputImage.Buffer();

    if (tmpRequiredSize > requiredSize)
    {
        tmpBuffer = new WebRtc_UWord8[(int) (tmpRequiredSize)];
        outPtr = tmpBuffer;
    }

    JSAMPROW y[16],u[8],v[8];
    JSAMPARRAY data[3];
    data[0] = y;
    data[1] = u;
    data[2] = v;

    WebRtc_UWord32 hInd, i;
    WebRtc_UWord32 numScanLines = 16;
    WebRtc_UWord32 numLinesProcessed = 0;

    while (_cinfo->output_scanline < _cinfo->output_height)
    {
        hInd = _cinfo->output_scanline;
        for (i = 0; i < numScanLines; i++)
        {
            y[i] = outPtr + stride * (i + hInd);

            if (i % 2 == 0)
            {
                 u[i / 2] = outPtr + stride * height16 +
                            stride / 2 * ((i + hInd) / 2);
                 v[i / 2] = outPtr + stride * height16 +
                            stride * height16 / 4 +
                            stride / 2 * ((i + hInd) / 2);
            }
        }
        // Processes exactly one iMCU row per call
        numLinesProcessed = jpeg_read_raw_data(_cinfo, data, numScanLines);
        // Error in read
        if (numLinesProcessed == 0)
        {
            jpeg_abort((j_common_ptr)_cinfo);
            return -1;
        }
    }

    if (tmpRequiredSize > requiredSize)
    {
         WebRtc_UWord8* dstFramePtr = outputImage.Buffer();
         WebRtc_UWord8* tmpPtr = outPtr;

         for (WebRtc_UWord32 p = 0; p < 3; p++)
         {
             const WebRtc_UWord32 h = (p == 0) ? height : height >> 1;
             const WebRtc_UWord32 h16 = (p == 0) ? height16 : height16 >> 1;
             const WebRtc_UWord32 w = (p == 0) ? width : width >> 1;
             const WebRtc_UWord32 s = (p == 0) ? stride : stride >> 1;

             for (WebRtc_UWord32 i = 0; i < h; i++)
             {
                 memcpy(dstFramePtr, tmpPtr, w);
                 dstFramePtr += w;
                 tmpPtr += s;
             }
             tmpPtr += (h16 - h) * s;
         }
    }

    if (tmpBuffer != NULL)
    {
        delete [] tmpBuffer;
    }
    // Setting output Image parameter
    outputImage.SetWidth(width);
    outputImage.SetHeight(height);
    outputImage.SetLength(requiredSize);
    outputImage.SetTimeStamp(inputImage._timeStamp);

    jpeg_finish_decompress(_cinfo);
    jpeg_destroy_decompress(_cinfo);
    return 0;
}


}
