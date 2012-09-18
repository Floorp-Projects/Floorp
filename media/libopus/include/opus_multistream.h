/* Copyright (c) 2011-2012 IETF Trust, Xiph.Org Foundation. All rights reserved.
   Written by Jean-Marc Valin */
/*

   This file is extracted from RFC6716. Please see that RFC for additional
   information.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of Internet Society, IETF or IETF Trust, nor the
   names of specific contributors, may be used to endorse or promote
   products derived from this software without specific prior written
   permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file opus_multistream.h
 * @brief Opus reference implementation multistream API
 */

#ifndef OPUS_MULTISTREAM_H
#define OPUS_MULTISTREAM_H

#include "opus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpusMSEncoder OpusMSEncoder;
typedef struct OpusMSDecoder OpusMSDecoder;

#define __opus_check_encstate_ptr(ptr) ((ptr) + ((ptr) - (OpusEncoder**)(ptr)))
#define __opus_check_decstate_ptr(ptr) ((ptr) + ((ptr) - (OpusDecoder**)(ptr)))

#define OPUS_MULTISTREAM_GET_ENCODER_STATE_REQUEST 5120
#define OPUS_MULTISTREAM_GET_DECODER_STATE_REQUEST 5122

#define OPUS_MULTISTREAM_GET_ENCODER_STATE(x,y) OPUS_MULTISTREAM_GET_ENCODER_STATE_REQUEST, __opus_check_int(x), __opus_check_encstate_ptr(y)
#define OPUS_MULTISTREAM_GET_DECODER_STATE(x,y) OPUS_MULTISTREAM_GET_DECODER_STATE_REQUEST, __opus_check_int(x), __opus_check_decstate_ptr(y)

/** Allocate and initialize a multistream encoder state object.
 *  Call opus_multistream_encoder_destroy() to release
 *  this object when finished. */
OPUS_EXPORT OpusMSEncoder *opus_multistream_encoder_create(
      opus_int32 Fs,            /**< Sampling rate of input signal (Hz) */
      int channels,             /**< Number of channels in the input signal */
      int streams,              /**< Total number of streams to encode from the input */
      int coupled_streams,      /**< Number of coupled (stereo) streams to encode */
      unsigned char *mapping,   /**< Encoded mapping between channels and streams */
      int application,          /**< Coding mode (OPUS_APPLICATION_VOIP/OPUS_APPLICATION_AUDIO) */
      int *error                /**< Error code */
);

/** Initialize an already allocated multistream encoder state. */
OPUS_EXPORT int opus_multistream_encoder_init(
      OpusMSEncoder *st,        /**< Encoder state */
      opus_int32 Fs,            /**< Sampling rate of input signal (Hz) */
      int channels,             /**< Number of channels in the input signal */
      int streams,              /**< Total number of streams to encode from the input */
      int coupled_streams,      /**< Number of coupled (stereo) streams to encode */
      unsigned char *mapping,   /**< Encoded mapping between channels and streams */
      int application           /**< Coding mode (OPUS_APPLICATION_VOIP/OPUS_APPLICATION_AUDIO) */
);

/** Returns length of the data payload (in bytes) or a negative error code */
OPUS_EXPORT int opus_multistream_encode(
    OpusMSEncoder *st,          /**< Encoder state */
    const opus_int16 *pcm,      /**< Input signal as interleaved samples. Length is frame_size*channels */
    int frame_size,             /**< Number of samples per frame of input signal */
    unsigned char *data,        /**< Output buffer for the compressed payload (no more than max_data_bytes long) */
    int max_data_bytes          /**< Allocated memory for payload; don't use for controlling bitrate */
);

/** Returns length of the data payload (in bytes) or a negative error code. */
OPUS_EXPORT int opus_multistream_encode_float(
      OpusMSEncoder *st,        /**< Encoder state */
      const float *pcm,         /**< Input signal interleaved in channel order. length is frame_size*channels */
      int frame_size,           /**< Number of samples per frame of input signal */
      unsigned char *data,      /**< Output buffer for the compressed payload (no more than max_data_bytes long) */
      int max_data_bytes        /**< Allocated memory for payload; don't use for controlling bitrate */
  );

/** Gets the size of an OpusMSEncoder structure.
  * @returns size
  */
OPUS_EXPORT opus_int32 opus_multistream_encoder_get_size(
      int streams,              /**< Total number of coded streams */
      int coupled_streams       /**< Number of coupled (stereo) streams */
);

/** Deallocate a multstream encoder state */
OPUS_EXPORT void opus_multistream_encoder_destroy(OpusMSEncoder *st);

/** Get or set options on a multistream encoder state */
OPUS_EXPORT int opus_multistream_encoder_ctl(OpusMSEncoder *st, int request, ...);

/** Allocate and initialize a multistream decoder state object.
 *  Call opus_multistream_decoder_destroy() to release
 *  this object when finished. */
OPUS_EXPORT OpusMSDecoder *opus_multistream_decoder_create(
      opus_int32 Fs,            /**< Sampling rate to decode at (Hz) */
      int channels,             /**< Number of channels to decode */
      int streams,              /**< Total number of coded streams in the multistream */
      int coupled_streams,      /**< Number of coupled (stereo) streams in the multistream */
      unsigned char *mapping,   /**< Stream to channel mapping table */
      int *error                /**< Error code */
);

/** Intialize a previously allocated decoder state object. */
OPUS_EXPORT int opus_multistream_decoder_init(
      OpusMSDecoder *st,        /**< Encoder state */
      opus_int32 Fs,            /**< Sample rate of input signal (Hz) */
      int channels,             /**< Number of channels in the input signal */
      int streams,              /**< Total number of coded streams */
      int coupled_streams,      /**< Number of coupled (stereo) streams */
      unsigned char *mapping    /**< Stream to channel mapping table */
);

/** Returns the number of samples decoded or a negative error code */
OPUS_EXPORT int opus_multistream_decode(
    OpusMSDecoder *st,          /**< Decoder state */
    const unsigned char *data,  /**< Input payload. Use a NULL pointer to indicate packet loss */
    int len,                    /**< Number of bytes in payload */
    opus_int16 *pcm,            /**< Output signal, samples interleaved in channel order . length is frame_size*channels */
    int frame_size,             /**< Number of samples per frame of input signal */
    int decode_fec              /**< Flag (0/1) to request that any in-band forward error correction data be */
                                /**< decoded. If no such data is available the frame is decoded as if it were lost. */
);

/** Returns the number of samples decoded or a negative error code */
OPUS_EXPORT int opus_multistream_decode_float(
    OpusMSDecoder *st,          /**< Decoder state */
    const unsigned char *data,  /**< Input payload buffer. Use a NULL pointer to indicate packet loss */
    int len,                    /**< Number of payload bytes in data */
    float *pcm,                 /**< Buffer for the output signal (interleaved iin channel order). length is frame_size*channels */
    int frame_size,             /**< Number of samples per frame of input signal */
    int decode_fec              /**< Flag (0/1) to request that any in-band forward error correction data be */
                                /**< decoded. If no such data is available the frame is decoded as if it were lost. */
);

/** Gets the size of an OpusMSDecoder structure.
  * @returns size
  */
OPUS_EXPORT opus_int32 opus_multistream_decoder_get_size(
      int streams,              /**< Total number of coded streams */
      int coupled_streams       /**< Number of coupled (stereo) streams */
);

/** Get or set options on a multistream decoder state */
OPUS_EXPORT int opus_multistream_decoder_ctl(OpusMSDecoder *st, int request, ...);

/** Deallocate a multistream decoder state object */
OPUS_EXPORT void opus_multistream_decoder_destroy(OpusMSDecoder *st);

#ifdef __cplusplus
}
#endif

#endif /* OPUS_MULTISTREAM_H */
