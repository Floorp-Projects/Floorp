/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Jpeg source data manager
 */

#ifndef WEBRTC_COMMON_VIDEO_JPEG_DATA_MANAGER
#define WEBRTC_COMMON_VIDEO_JPEG_DATA_MANAGER

#include <stdio.h>
extern "C" {
#if defined(USE_SYSTEM_LIBJPEG)
#include <jpeglib.h>
#else
#include "jpeglib.h"
#endif
}

namespace webrtc
{

// Source manager:


// a general function that will set these values
void
jpegSetSrcBuffer(j_decompress_ptr cinfo, JOCTET* srcBuffer, size_t bufferSize);


// Initialize source.  This is called by jpeg_read_header() before any
//  data is actually read.

void
initSrc(j_decompress_ptr cinfo);


// Fill input buffer
// This is called whenever bytes_in_buffer has reached zero and more
//  data is wanted.

boolean
fillInputBuffer(j_decompress_ptr cinfo);

// Skip input data
// Skip num_bytes worth of data.

void
skipInputData(j_decompress_ptr cinfo, long num_bytes);




// Terminate source
void
termSource (j_decompress_ptr cinfo);

} // end of namespace webrtc


#endif /* WEBRTC_COMMON_VIDEO_JPEG_DATA_MANAGER */
