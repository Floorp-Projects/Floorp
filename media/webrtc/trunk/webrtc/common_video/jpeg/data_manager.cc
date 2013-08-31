/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/jpeg/data_manager.h"

namespace webrtc
{

typedef struct
{
    jpeg_source_mgr  mgr;
    JOCTET* next_input_byte;
    size_t bytes_in_buffer;      /* # of byte spaces remaining in buffer */
} DataSrcMgr;

void
jpegSetSrcBuffer(j_decompress_ptr cinfo, JOCTET* srcBuffer, size_t bufferSize)
{
    DataSrcMgr* src;
    if (cinfo->src == NULL)
    {  /* first time for this JPEG object? */
        cinfo->src = (struct jpeg_source_mgr *)
                   (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo,
                       JPOOL_PERMANENT, sizeof(DataSrcMgr));
    }

    // Setting required functionality
    src = (DataSrcMgr*) cinfo->src;
    src->mgr.init_source = initSrc;;
    src->mgr.fill_input_buffer = fillInputBuffer;
    src->mgr.skip_input_data = skipInputData;
    src->mgr.resync_to_restart = jpeg_resync_to_restart; // use default
    src->mgr.term_source = termSource;
    // setting buffer/src
    src->bytes_in_buffer = bufferSize;
    src->next_input_byte = srcBuffer;

}


void
initSrc(j_decompress_ptr cinfo)
{
    DataSrcMgr  *src = (DataSrcMgr*)cinfo->src;
    src->mgr.next_input_byte = src->next_input_byte;
    src->mgr.bytes_in_buffer = src->bytes_in_buffer;
}

boolean
fillInputBuffer(j_decompress_ptr cinfo)
{
    return false;
}


void
skipInputData(j_decompress_ptr cinfo, long num_bytes)
{
    DataSrcMgr* src = (DataSrcMgr*)cinfo->src;
    if (num_bytes > 0)
    {
          if ((unsigned long)num_bytes > src->mgr.bytes_in_buffer)
              src->mgr.bytes_in_buffer = 0;
          else
          {
              src->mgr.next_input_byte += num_bytes;
              src->mgr.bytes_in_buffer -= num_bytes;
          }
    }
}


void
termSource (j_decompress_ptr cinfo)
{
  //
}

} // end of namespace webrtc
