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

#ifndef __FISH_SOUND_DEPRECATED_H__
#define __FISH_SOUND_DEPRECATED_H__

/** \file
 * Deprecated interfaces
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * DEPRECATED TYPE.
 * Signature of a callback for libfishsound to call when it has decoded
 * PCM audio data, and you want this provided as floats using the current
 * interleave method as set by fish_sound_set_interleave().
 */
typedef FishSoundDecoded_Float FishSoundDecoded;

/**
 * DEPRECATED FUNCTION.
 * Set the callback for libfishsound to call when it has a block of decoded
 * PCM audio ready, and you want this provided as floats using the current
 * interleave method as set by fish_sound_set_interleave().
 * This function, and fish_sound_set_interleave(), have been superceded by
 * the typesafe fish_sound_set_decoded_TYPE() callbacks, such as
 * fish_sound_set_decoded_float() or fish_sound_set_decoded_float_ilv().
 *
 * \param fsound A FishSound* handle (created with mode FISH_SOUND_DECODE)
 * \param decoded The callback to call
 * \param user_data Arbitrary user data to pass to the callback
 * \returns 0 on success, -1 on failure
 */
int fish_sound_set_decoded_callback (FishSound * fsound,
				     FishSoundDecoded decoded,
				     void * user_data);

/**
 * DEPRECATED FUNCTION.
 * Set the PCM format used by a FishSound object. The default value is
 * non-interleaved.
 * Prior to libfishsound 0.7.0, you would (optionally) specify whether you
 * wanted to receive interleaved or per-channel PCM data using
 * fish_sound_set_interleave(), the default being per-channel
 * (non-interleaved) PCM.
 * Whether or not your decoded callback expects interleaved or
 * non-interleaved data is now implied by the particular
 * fish_sound_set_decoded_TYPE() method you use to set it, such as
 * fish_sound_set_decoded_float() or fish_sound_set_decoded_float_ilv().
 *
 * \param fsound A FishSound* handle
 * \param interleave Whether to use interleaved PCM or not. Valid values are
 * 0 for non-interleaved, and 1 for interleaved.
 * \retval 0 Success
 * \retval -1 Invalid \a fsound
 */
int fish_sound_set_interleave (FishSound * fsound, int interleave);

/**
 * DEPRECATED FUNCTION.
 * Encode a block of audio
 * \param fsound A FishSound* handle (created with mode FISH_SOUND_ENCODE)
 * \param pcm The audio data to encode
 * \param frames A count of frames to encode
 * \returns The number of frames encoded
 * \note For multichannel audio, the audio data is interpreted according
 * to the current PCM style
 */
long fish_sound_encode (FishSound * fsound, float ** pcm, long frames);

#ifdef __cplusplus
}
#endif

#endif /* __FISH_SOUND_DEPRECATED_H__ */
