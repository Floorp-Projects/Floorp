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

/** @file
 * General constants used by liboggplay
 * 
 * @authors 
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 * Viktor Gal
 */
 
#ifndef __OGGPLAY_ENUMS_H__
#define __OGGPLAY_ENUMS_H__

/**
 * Definitions of error return values.
 */
typedef enum OggPlayErrorCode {
  E_OGGPLAY_CONTINUE            = 1,
  E_OGGPLAY_OK                  = 0,    /**< No error */
  E_OGGPLAY_BAD_OGGPLAY         = -1,   /**< supplied oggplay is not a valid OggPlay */
  E_OGGPLAY_BAD_READER          = -2,   /**< OggPlayReader is invalid */
  E_OGGPLAY_BAD_INPUT           = -3,   /**< Error in the input */
  E_OGGPLAY_NO_SUCH_CHUNK       = -4,
  E_OGGPLAY_BAD_TRACK           = -5,   /**< The requested track number does not exists */
  E_OGGPLAY_TRACK_IS_SKELETON   = -6,   /**< Trying to activate a Skeleton track */
  E_OGGPLAY_OGGZ_UNHAPPY        = -7,   /**< OGGZ error occured */
  E_OGGPLAY_END_OF_FILE         = -8,   /**< End of file */
  E_OGGPLAY_TRACK_IS_OVER       = -9,   /**< A given track has ended */
  E_OGGPLAY_BAD_CALLBACK_INFO   = -10,  /**< Invalid OggPlayCallbackInfo */
  E_OGGPLAY_WRONG_TRACK_TYPE    = -11,  /**< */
  E_OGGPLAY_UNINITIALISED       = -12,  /**< The OggPlay handle is not initialised */
  E_OGGPLAY_CALLBACK_MODE       = -13,  /**< OggPlay is used in callback mode */
  E_OGGPLAY_BUFFER_MODE         = -14,  /**< OggPlay is used in buffer mode */
  E_OGGPLAY_USER_INTERRUPT      = -15,  /**< User interrupt received */
  E_OGGPLAY_SOCKET_ERROR        = -16,  /**< Error while creating socket */
  E_OGGPLAY_TIMEOUT             = -17,  /**< TCP connection timeouted */
  E_OGGPLAY_CANT_SEEK           = -18,  /**< Could not performed the requested seek */
  E_OGGPLAY_NO_KATE_SUPPORT     = -19,  /**< LibKate is not supported */
  E_OGGPLAY_NO_TIGER_SUPPORT    = -20,  /**< LibTiger is not supported */
  E_OGGPLAY_OUT_OF_MEMORY       = -21,  /**< Out of memory */
  E_OGGPLAY_TYPE_OVERFLOW       = -22,  /**< Integer overflow detected */
  E_OGGPLAY_TRACK_IS_UNKNOWN    = -23,  /**< The selected track's content type is UNKNOWN */
  E_OGGPLAY_TRACK_UNINITIALISED = -24,  /**< Track is not initialised, i.e. bad headers or haven't seen them */
  E_OGGPLAY_NOTCHICKENPAYBACK   = -777
} OggPlayErrorCode;

/**
 * Definitions of the various record types
 */
typedef enum OggPlayDataType {
  OGGPLAY_INACTIVE      = -1,   /**< record is inactive */
  OGGPLAY_YUV_VIDEO     = 0,    /**< record is a YUV format video */
  OGGPLAY_RGBA_VIDEO    = 1,    /**< record is a video in RGB format */
  OGGPLAY_SHORTS_AUDIO  = 1000, /**< audio record in short format */
  OGGPLAY_FLOATS_AUDIO  = 1001, /**< audio record in float format */
  OGGPLAY_CMML          = 2000, /**< CMML record */
  OGGPLAY_KATE          = 3000, /**< KATE record */
  OGGPLAY_TYPE_UNKNOWN  = 9000  /**< content type of the record is unknown */
} OggPlayDataType;

/**
 * Definitions of the various states of a stream.
 */
typedef enum OggPlayStreamInfo {
  OGGPLAY_STREAM_UNINITIALISED = 0, /**< Stream is not initialised */
  OGGPLAY_STREAM_FIRST_DATA = 1,    /**< Stream received it's first data frame */
  OGGPLAY_STREAM_INITIALISED = 2,   /**< Stream is initialised */
  OGGPLAY_STREAM_LAST_DATA = 3,     /**< Stream received it's last data frame */
  OGGPLAY_STREAM_JUST_SEEKED = 4    /**< We've just seeked in the stream */
} OggPlayStreamInfo;

#endif
