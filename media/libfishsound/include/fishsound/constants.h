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

#ifndef __FISH_SOUND_CONSTANTS_H__
#define __FISH_SOUND_CONSTANTS_H__

/** \file
 * Constants used by libfishsound
 */

/** Mode of operation (encode or decode) */
typedef enum _FishSoundMode {
  /** Decode */
  FISH_SOUND_DECODE = 0x10,

  /** Encode */
  FISH_SOUND_ENCODE = 0x20
} FishSoundMode;

/** Identifiers for supported codecs */
typedef enum _FishSoundCodecID {
  /** Unknown */
  FISH_SOUND_UNKNOWN = 0x00,

  /** Vorbis */
  FISH_SOUND_VORBIS  = 0x01,

  /** Speex */
  FISH_SOUND_SPEEX   = 0x02,

  /** Flac */
  FISH_SOUND_FLAC    = 0x03
} FishSoundCodecID;

/** Decode callback return values */
typedef enum _FishSoundStopCtl {
  /** Continue calling decode callbacks */
  FISH_SOUND_CONTINUE = 0,
  
  /** Stop calling callbacks, but retain buffered data */
  FISH_SOUND_STOP_OK  = 1,
  
  /** Stop calling callbacks, and purge buffered data */
  FISH_SOUND_STOP_ERR = -1
} FishSoundStopCtl;

/** Command codes */
typedef enum _FishSoundCommand {
  /** No operation */
  FISH_SOUND_COMMAND_NOP                = 0x0000,

  /** Retrieve the FishSoundInfo */
  FISH_SOUND_GET_INFO                   = 0x1000,

  /** Query if multichannel audio should be interpreted as interleaved */
  FISH_SOUND_GET_INTERLEAVE             = 0x2000,

  /** Set to 1 to interleave, 0 to non-interleave */
  FISH_SOUND_SET_INTERLEAVE             = 0x2001,

  FISH_SOUND_SET_ENCODE_VBR             = 0x4000,
  
  FISH_SOUND_COMMAND_MAX
} FishSoundCommand;

/** Error values */
typedef enum _FishSoundError {
  /** No error */
  FISH_SOUND_OK                         = 0,

  /** generic error */
  FISH_SOUND_ERR_GENERIC                = -1,

  /** Not a valid FishSound* handle */
  FISH_SOUND_ERR_BAD                    = -2,

  /** The requested operation is not suitable for this FishSound* handle */
  FISH_SOUND_ERR_INVALID                = -3,

  /** Out of memory */
  FISH_SOUND_ERR_OUT_OF_MEMORY          = -4,

  /** Functionality disabled at build time */
  FISH_SOUND_ERR_DISABLED               = -10,

  /** Too few bytes passed to fish_sound_identify() */
  FISH_SOUND_ERR_SHORT_IDENTIFY         = -20,

  /** Comment violates VorbisComment restrictions */
  FISH_SOUND_ERR_COMMENT_INVALID        = -21
} FishSoundError;

#endif /* __FISH_SOUND_CONSTANTS_H__ */
