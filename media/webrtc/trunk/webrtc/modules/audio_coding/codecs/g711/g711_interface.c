/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <string.h>
#include "g711.h"
#include "g711_interface.h"
#include "typedefs.h"

WebRtc_Word16 WebRtcG711_EncodeA(void *state,
                                 WebRtc_Word16 *speechIn,
                                 WebRtc_Word16 len,
                                 WebRtc_Word16 *encoded)
{
    int n;
    WebRtc_UWord16 tempVal,  tempVal2;

    // Set and discard to avoid getting warnings
    (void)(state = NULL);

    // Sanity check of input length
    if (len < 0) {
        return (-1);
    }

    // Loop over all samples
    for (n = 0; n < len; n++) {
        tempVal = (WebRtc_UWord16)linear_to_alaw(speechIn[n]);

#ifdef WEBRTC_BIG_ENDIAN
        if ((n & 0x1) == 1) {
            encoded[n>>1]|=((WebRtc_UWord16)tempVal);
        } else {
            encoded[n>>1]=((WebRtc_UWord16)tempVal)<<8;
        }
#else
        if ((n & 0x1) == 1) {
            tempVal2 |= ((WebRtc_UWord16) tempVal) << 8;
            encoded[n >> 1] |= ((WebRtc_UWord16) tempVal) << 8;
        } else {
            tempVal2 = ((WebRtc_UWord16) tempVal);
            encoded[n >> 1] = ((WebRtc_UWord16) tempVal);
        }
#endif
    }
    return (len);
}

WebRtc_Word16 WebRtcG711_EncodeU(void  *state,
                                 WebRtc_Word16 *speechIn,
                                 WebRtc_Word16 len,
                                 WebRtc_Word16 *encoded)
{
    int n;
    WebRtc_UWord16 tempVal;

    // Set and discard to avoid getting warnings
    (void)(state = NULL);

    // Sanity check of input length
    if (len < 0) {
        return (-1);
    }

    // Loop over all samples
    for (n = 0; n < len; n++) {
        tempVal = (WebRtc_UWord16)linear_to_ulaw(speechIn[n]);

 #ifdef WEBRTC_BIG_ENDIAN
        if ((n & 0x1) == 1) {
            encoded[n>>1]|=((WebRtc_UWord16)tempVal);
        } else {
            encoded[n>>1]=((WebRtc_UWord16)tempVal)<<8;
        }
 #else
        if ((n & 0x1) == 1) {
            encoded[n >> 1] |= ((WebRtc_UWord16) tempVal) << 8;
        } else {
            encoded[n >> 1] = ((WebRtc_UWord16) tempVal);
        }
 #endif
    }
    return (len);
}

WebRtc_Word16 WebRtcG711_DecodeA(void *state,
                                 WebRtc_Word16 *encoded,
                                 WebRtc_Word16 len,
                                 WebRtc_Word16 *decoded,
                                 WebRtc_Word16 *speechType)
{
    int n;
    WebRtc_UWord16 tempVal;

    // Set and discard to avoid getting warnings
    (void)(state = NULL);

    // Sanity check of input length
    if (len < 0) {
        return (-1);
    }

    for (n = 0; n < len; n++) {
 #ifdef WEBRTC_BIG_ENDIAN
        if ((n & 0x1) == 1) {
            tempVal=((WebRtc_UWord16)encoded[n>>1] & 0xFF);
        } else {
            tempVal=((WebRtc_UWord16)encoded[n>>1] >> 8);
        }
 #else
        if ((n & 0x1) == 1) {
            tempVal = (encoded[n >> 1] >> 8);
        } else {
            tempVal = (encoded[n >> 1] & 0xFF);
        }
 #endif
        decoded[n] = (WebRtc_Word16) alaw_to_linear(tempVal);
    }

    *speechType = 1;
    return (len);
}

WebRtc_Word16 WebRtcG711_DecodeU(void *state,
                                 WebRtc_Word16 *encoded,
                                 WebRtc_Word16 len,
                                 WebRtc_Word16 *decoded,
                                 WebRtc_Word16 *speechType)
{
    int n;
    WebRtc_UWord16 tempVal;

    // Set and discard to avoid getting warnings
    (void)(state = NULL);

    // Sanity check of input length
    if (len < 0) {
        return (-1);
    }

    for (n = 0; n < len; n++) {
 #ifdef WEBRTC_BIG_ENDIAN
        if ((n & 0x1) == 1) {
            tempVal=((WebRtc_UWord16)encoded[n>>1] & 0xFF);
        } else {
            tempVal=((WebRtc_UWord16)encoded[n>>1] >> 8);
        }
 #else
        if ((n & 0x1) == 1) {
            tempVal = (encoded[n >> 1] >> 8);
        } else {
            tempVal = (encoded[n >> 1] & 0xFF);
        }
 #endif
        decoded[n] = (WebRtc_Word16) ulaw_to_linear(tempVal);
    }

    *speechType = 1;
    return (len);
}

int WebRtcG711_DurationEst(void* state,
                           const uint8_t* payload,
                           int payload_length_bytes) {
    (void)state;
    (void)payload;
    /* G.711 is one byte per sample, so we can just return the number of
       bytes. */
    return payload_length_bytes;
}

WebRtc_Word16 WebRtcG711_Version(char* version, WebRtc_Word16 lenBytes)
{
    strncpy(version, "2.0.0", lenBytes);
    return 0;
}
