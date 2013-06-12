/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file contains the implementation of functions
 * WebRtcSpl_MemSetW16()
 * WebRtcSpl_MemSetW32()
 * WebRtcSpl_MemCpyReversedOrder()
 * WebRtcSpl_CopyFromEndW16()
 * WebRtcSpl_ZerosArrayW16()
 * WebRtcSpl_ZerosArrayW32()
 * WebRtcSpl_OnesArrayW16()
 * WebRtcSpl_OnesArrayW32()
 *
 * The description header can be found in signal_processing_library.h
 *
 */

#include <string.h>
#include "signal_processing_library.h"


void WebRtcSpl_MemSetW16(WebRtc_Word16 *ptr, WebRtc_Word16 set_value, int length)
{
    int j;
    WebRtc_Word16 *arrptr = ptr;

    for (j = length; j > 0; j--)
    {
        *arrptr++ = set_value;
    }
}

void WebRtcSpl_MemSetW32(WebRtc_Word32 *ptr, WebRtc_Word32 set_value, int length)
{
    int j;
    WebRtc_Word32 *arrptr = ptr;

    for (j = length; j > 0; j--)
    {
        *arrptr++ = set_value;
    }
}

void WebRtcSpl_MemCpyReversedOrder(WebRtc_Word16* dest, WebRtc_Word16* source, int length)
{
    int j;
    WebRtc_Word16* destPtr = dest;
    WebRtc_Word16* sourcePtr = source;

    for (j = 0; j < length; j++)
    {
        *destPtr-- = *sourcePtr++;
    }
}

WebRtc_Word16 WebRtcSpl_CopyFromEndW16(G_CONST WebRtc_Word16 *vector_in,
                                       WebRtc_Word16 length,
                                       WebRtc_Word16 samples,
                                       WebRtc_Word16 *vector_out)
{
    // Copy the last <samples> of the input vector to vector_out
    WEBRTC_SPL_MEMCPY_W16(vector_out, &vector_in[length - samples], samples);

    return samples;
}

WebRtc_Word16 WebRtcSpl_ZerosArrayW16(WebRtc_Word16 *vector, WebRtc_Word16 length)
{
    WebRtcSpl_MemSetW16(vector, 0, length);
    return length;
}

WebRtc_Word16 WebRtcSpl_ZerosArrayW32(WebRtc_Word32 *vector, WebRtc_Word16 length)
{
    WebRtcSpl_MemSetW32(vector, 0, length);
    return length;
}

WebRtc_Word16 WebRtcSpl_OnesArrayW16(WebRtc_Word16 *vector, WebRtc_Word16 length)
{
    WebRtc_Word16 i;
    WebRtc_Word16 *tmpvec = vector;
    for (i = 0; i < length; i++)
    {
        *tmpvec++ = 1;
    }
    return length;
}

WebRtc_Word16 WebRtcSpl_OnesArrayW32(WebRtc_Word32 *vector, WebRtc_Word16 length)
{
    WebRtc_Word16 i;
    WebRtc_Word32 *tmpvec = vector;
    for (i = 0; i < length; i++)
    {
        *tmpvec++ = 1;
    }
    return length;
}
