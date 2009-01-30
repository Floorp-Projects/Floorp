/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * oggplay_tools.h
 * 
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 */

#ifndef __OGGPLAY_TOOLS_H__
#define __OGGPLAY_TOOLS_H__

#include <ogg/ogg.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#include <time.h>
#else
#include <sys/time.h>
#include <time.h>
#endif

/* structure holds pointers to y, u, v channels */
typedef struct _OggPlayYUVChannels {
    unsigned char * ptry;
    unsigned char * ptru;
    unsigned char * ptrv;
    int             y_width;
    int             y_height;
    int             uv_width;
    int             uv_height;
} OggPlayYUVChannels;

/* structure holds pointers to y, u, v channels */
typedef struct _OggPlayRGBChannels {
    unsigned char * ptro;
    int             rgb_width;
    int             rgb_height;
} OggPlayRGBChannels;

void 
oggplay_yuv2rgb(OggPlayYUVChannels* yuv, OggPlayRGBChannels * rgb);

void 
oggplay_yuv2argb(OggPlayYUVChannels* yuv, OggPlayRGBChannels * rgb);

void
oggplay_yuv2bgr(OggPlayYUVChannels* yuv, OggPlayRGBChannels * rgb);

ogg_int64_t
oggplay_sys_time_in_ms(void);

void
oggplay_millisleep(long ms);

#ifdef __cplusplus
}
#endif

#endif /*__OGGPLAY_TOOLS_H__*/

