/* Copyright (c) 2007-2012 IETF Trust, CSIRO, Xiph.Org Foundation,
                           Gregory Maxwell. All rights reserved.
   Written by Jean-Marc Valin and Gregory Maxwell */
/**
  @file celt.h
  @brief Contains all the functions for encoding and decoding audio
 */

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

#ifndef OPUS_CUSTOM_H
#define OPUS_CUSTOM_H


#include "opus_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CUSTOM_MODES
#define OPUS_CUSTOM_EXPORT OPUS_EXPORT
#define OPUS_CUSTOM_EXPORT_STATIC OPUS_EXPORT
#else
#define OPUS_CUSTOM_EXPORT
#ifdef CELT_C
#define OPUS_CUSTOM_EXPORT_STATIC static inline
#else
#define OPUS_CUSTOM_EXPORT_STATIC
#endif
#endif

/** Contains the state of an encoder. One encoder state is needed
    for each stream. It is initialised once at the beginning of the
    stream. Do *not* re-initialise the state for every frame.
   @brief Encoder state
 */
typedef struct OpusCustomEncoder OpusCustomEncoder;

/** State of the decoder. One decoder state is needed for each stream.
    It is initialised once at the beginning of the stream. Do *not*
    re-initialise the state for every frame */
typedef struct OpusCustomDecoder OpusCustomDecoder;

/** The mode contains all the information necessary to create an
    encoder. Both the encoder and decoder need to be initialised
    with exactly the same mode, otherwise the quality will be very
    bad */
typedef struct OpusCustomMode OpusCustomMode;

/** Creates a new mode struct. This will be passed to an encoder or
    decoder. The mode MUST NOT BE DESTROYED until the encoders and
    decoders that use it are destroyed as well.
 @param Fs Sampling rate (32000 to 96000 Hz)
 @param frame_size Number of samples (per channel) to encode in each
                   packet (even values; 64 - 512)
 @param error Returned error code (if NULL, no error will be returned)
 @return A newly created mode
*/
OPUS_CUSTOM_EXPORT OpusCustomMode *opus_custom_mode_create(opus_int32 Fs, int frame_size, int *error);

/** Destroys a mode struct. Only call this after all encoders and
    decoders using this mode are destroyed as well.
 @param mode Mode to be destroyed
*/
OPUS_CUSTOM_EXPORT void opus_custom_mode_destroy(OpusCustomMode *mode);

/* Encoder */

OPUS_CUSTOM_EXPORT_STATIC int opus_custom_encoder_get_size(const OpusCustomMode *mode, int channels);

/** Creates a new encoder state. Each stream needs its own encoder
    state (can't be shared across simultaneous streams).
 @param mode Contains all the information about the characteristics of
 *  the stream (must be the same characteristics as used for the
 *  decoder)
 @param channels Number of channels
 @param error Returns an error code
 @return Newly created encoder state.
*/
OPUS_CUSTOM_EXPORT OpusCustomEncoder *opus_custom_encoder_create(const OpusCustomMode *mode, int channels, int *error);

OPUS_CUSTOM_EXPORT_STATIC int opus_custom_encoder_init(OpusCustomEncoder *st, const OpusCustomMode *mode, int channels);

/** Destroys a an encoder state.
 @param st Encoder state to be destroyed
 */
OPUS_CUSTOM_EXPORT void opus_custom_encoder_destroy(OpusCustomEncoder *st);

/** Encodes a frame of audio.
 @param st Encoder state
 @param pcm PCM audio in float format, with a normal range of +/-1.0.
 *          Samples with a range beyond +/-1.0 are supported but will
 *          be clipped by decoders using the integer API and should
 *          only be used if it is known that the far end supports
 *          extended dynmaic range. There must be exactly
 *          frame_size samples per channel.
 @param compressed The compressed data is written here. This may not alias pcm or
 *                 optional_synthesis.
 @param nbCompressedBytes Maximum number of bytes to use for compressing the frame
 *          (can change from one frame to another)
 @return Number of bytes written to "compressed". Will be the same as
 *       "nbCompressedBytes" unless the stream is VBR and will never be larger.
 *       If negative, an error has occurred (see error codes). It is IMPORTANT that
 *       the length returned be somehow transmitted to the decoder. Otherwise, no
 *       decoding is possible.
*/
OPUS_CUSTOM_EXPORT int opus_custom_encode_float(OpusCustomEncoder *st, const float *pcm, int frame_size, unsigned char *compressed, int maxCompressedBytes);

/** Encodes a frame of audio.
 @param st Encoder state
 @param pcm PCM audio in signed 16-bit format (native endian). There must be
 *          exactly frame_size samples per channel.
 @param compressed The compressed data is written here. This may not alias pcm or
 *                         optional_synthesis.
 @param nbCompressedBytes Maximum number of bytes to use for compressing the frame
 *                        (can change from one frame to another)
 @return Number of bytes written to "compressed". Will be the same as
 *       "nbCompressedBytes" unless the stream is VBR and will never be larger.
 *       If negative, an error has occurred (see error codes). It is IMPORTANT that
 *       the length returned be somehow transmitted to the decoder. Otherwise, no
 *       decoding is possible.
 */
OPUS_CUSTOM_EXPORT int opus_custom_encode(OpusCustomEncoder *st, const opus_int16 *pcm, int frame_size, unsigned char *compressed, int maxCompressedBytes);

/** Query and set encoder parameters
 @param st Encoder state
 @param request Parameter to change or query
 @param value Pointer to a 32-bit int value
 @return Error code
*/
OPUS_CUSTOM_EXPORT int opus_custom_encoder_ctl(OpusCustomEncoder * restrict st, int request, ...);

/* Decoder */

OPUS_CUSTOM_EXPORT_STATIC int opus_custom_decoder_get_size(const OpusCustomMode *mode, int channels);

/** Creates a new decoder state. Each stream needs its own decoder state (can't
    be shared across simultaneous streams).
 @param mode Contains all the information about the characteristics of the
             stream (must be the same characteristics as used for the encoder)
 @param channels Number of channels
 @param error Returns an error code
 @return Newly created decoder state.
 */
OPUS_CUSTOM_EXPORT OpusCustomDecoder *opus_custom_decoder_create(const OpusCustomMode *mode, int channels, int *error);

OPUS_CUSTOM_EXPORT_STATIC int opus_custom_decoder_init(OpusCustomDecoder *st, const OpusCustomMode *mode, int channels);

/** Destroys a a decoder state.
 @param st Decoder state to be destroyed
 */
OPUS_CUSTOM_EXPORT void opus_custom_decoder_destroy(OpusCustomDecoder *st);

/** Decodes a frame of audio.
 @param st Decoder state
 @param data Compressed data produced by an encoder
 @param len Number of bytes to read from "data". This MUST be exactly the number
            of bytes returned by the encoder. Using a larger value WILL NOT WORK.
 @param pcm One frame (frame_size samples per channel) of decoded PCM will be
            returned here in float format.
 @return Error code.
   */
OPUS_CUSTOM_EXPORT int opus_custom_decode_float(OpusCustomDecoder *st, const unsigned char *data, int len, float *pcm, int frame_size);

/** Decodes a frame of audio.
 @param st Decoder state
 @param data Compressed data produced by an encoder
 @param len Number of bytes to read from "data". This MUST be exactly the number
            of bytes returned by the encoder. Using a larger value WILL NOT WORK.
 @param pcm One frame (frame_size samples per channel) of decoded PCM will be
            returned here in 16-bit PCM format (native endian).
 @return Error code.
 */
OPUS_CUSTOM_EXPORT int opus_custom_decode(OpusCustomDecoder *st, const unsigned char *data, int len, opus_int16 *pcm, int frame_size);

/** Query and set decoder parameters
   @param st Decoder state
   @param request Parameter to change or query
   @param value Pointer to a 32-bit int value
   @return Error code
 */
OPUS_CUSTOM_EXPORT int opus_custom_decoder_ctl(OpusCustomDecoder * restrict st, int request, ...);

#ifdef __cplusplus
}
#endif

#endif /* OPUS_CUSTOM_H */
