/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "pcm16b.h"

#include <stdlib.h>

#include "typedefs.h"

#define HIGHEND 0xFF00
#define LOWEND    0xFF



/* Encoder with WebRtc_Word16 Output */
WebRtc_Word16 WebRtcPcm16b_EncodeW16(WebRtc_Word16 *speechIn16b,
                                     WebRtc_Word16 len,
                                     WebRtc_Word16 *speechOut16b)
{
#ifdef WEBRTC_BIG_ENDIAN
    memcpy(speechOut16b, speechIn16b, len * sizeof(WebRtc_Word16));
#else
    int i;
    for (i=0;i<len;i++) {
        speechOut16b[i]=(((WebRtc_UWord16)speechIn16b[i])>>8)|((((WebRtc_UWord16)speechIn16b[i])<<8)&0xFF00);
    }
#endif
    return(len<<1);
}


/* Encoder with char Output (old version) */
WebRtc_Word16 WebRtcPcm16b_Encode(WebRtc_Word16 *speech16b,
                                  WebRtc_Word16 len,
                                  unsigned char *speech8b)
{
    WebRtc_Word16 samples=len*2;
    WebRtc_Word16 pos;
    WebRtc_Word16 short1;
    WebRtc_Word16 short2;
    for (pos=0;pos<len;pos++) {
        short1=HIGHEND & speech16b[pos];
        short2=LOWEND & speech16b[pos];
        short1=short1>>8;
        speech8b[pos*2]=(unsigned char) short1;
        speech8b[pos*2+1]=(unsigned char) short2;
    }
    return(samples);
}


/* Decoder with WebRtc_Word16 Input instead of char when the WebRtc_Word16 Encoder is used */
WebRtc_Word16 WebRtcPcm16b_DecodeW16(void *inst,
                                     WebRtc_Word16 *speechIn16b,
                                     WebRtc_Word16 len,
                                     WebRtc_Word16 *speechOut16b,
                                     WebRtc_Word16* speechType)
{
#ifdef WEBRTC_BIG_ENDIAN
    memcpy(speechOut16b, speechIn16b, ((len*sizeof(WebRtc_Word16)+1)>>1));
#else
    int i;
    int samples=len>>1;

    for (i=0;i<samples;i++) {
        speechOut16b[i]=(((WebRtc_UWord16)speechIn16b[i])>>8)|(((WebRtc_UWord16)(speechIn16b[i]&0xFF))<<8);
    }
#endif

    *speechType=1;

    // Avoid warning.
    (void)(inst = NULL);

    return(len>>1);
}

/* "old" version of the decoder that uses char as input (not used in NetEq any more) */
WebRtc_Word16 WebRtcPcm16b_Decode(unsigned char *speech8b,
                                  WebRtc_Word16 len,
                                  WebRtc_Word16 *speech16b)
{
    WebRtc_Word16 samples=len>>1;
    WebRtc_Word16 pos;
    WebRtc_Word16 shortval;
    for (pos=0;pos<samples;pos++) {
        shortval=((unsigned short) speech8b[pos*2]);
        shortval=(shortval<<8)&HIGHEND;
        shortval=shortval|(((unsigned short) speech8b[pos*2+1])&LOWEND);
        speech16b[pos]=shortval;
    }
    return(samples);
}
