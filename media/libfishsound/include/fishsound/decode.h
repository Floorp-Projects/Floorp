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

#ifndef __FISH_SOUND_DECODE_H__
#define __FISH_SOUND_DECODE_H__

#ifdef __cplusplus
extern "C" {
#endif

/** \file
 * Decode functions and callback prototypes
 */

/**
 * Signature of a callback for libfishsound to call when it has decoded
 * PCM audio data, and you want this provided as non-interleaved floats.
 * \param fsound The FishSound* handle
 * \param pcm The decoded audio
 * \param frames The count of frames decoded
 * \param user_data Arbitrary user data
 * \retval FISH_SOUND_CONTINUE Continue decoding
 * \retval FISH_SOUND_STOP_OK Stop decoding immediately and
 * return control to the fish_sound_decode() caller
 * \retval FISH_SOUND_STOP_ERR Stop decoding immediately, purge buffered
 * data, and return control to the fish_sound_decode() caller
 */
typedef int (*FishSoundDecoded_Float) (FishSound * fsound, float * pcm[],
				       long frames, void * user_data);

/**
 * Signature of a callback for libfishsound to call when it has decoded
 * PCM audio data, and you want this provided as interleaved floats.
 * \param fsound The FishSound* handle
 * \param pcm The decoded audio
 * \param frames The count of frames decoded
 * \param user_data Arbitrary user data
 * \retval FISH_SOUND_CONTINUE Continue decoding
 * \retval FISH_SOUND_STOP_OK Stop decoding immediately and
 * return control to the fish_sound_decode() caller
 * \retval FISH_SOUND_STOP_ERR Stop decoding immediately, purge buffered
 * data, and return control to the fish_sound_decode() caller
 */
typedef int (*FishSoundDecoded_FloatIlv) (FishSound * fsound, float ** pcm,
					  long frames, void * user_data);

/**
 * Set the callback for libfishsound to call when it has a block of decoded
 * PCM audio ready, and you want this provided as non-interleaved floats.
 * \param fsound A FishSound* handle (created with mode FISH_SOUND_DECODE)
 * \param decoded The callback to call
 * \param user_data Arbitrary user data to pass to the callback
 * \retval 0 Success
 * \retval FISH_SOUND_ERR_BAD Not a valid FishSound* handle
 * \retval FISH_SOUND_ERR_OUT_OF_MEMORY Out of memory
 */
int fish_sound_set_decoded_float (FishSound * fsound,
				  FishSoundDecoded_Float decoded,
				  void * user_data);

/**
 * Set the callback for libfishsound to call when it has a block of decoded
 * PCM audio ready, and you want this provided as interleaved floats.
 * \param fsound A FishSound* handle (created with mode FISH_SOUND_DECODE)
 * \param decoded The callback to call
 * \param user_data Arbitrary user data to pass to the callback
 * \retval 0 Success
 * \retval FISH_SOUND_ERR_BAD Not a valid FishSound* handle
 * \retval FISH_SOUND_ERR_OUT_OF_MEMORY Out of memory
 */
int fish_sound_set_decoded_float_ilv (FishSound * fsound,
				      FishSoundDecoded_FloatIlv decoded,
				      void * user_data);

/**
 * Decode a block of compressed data.
 * No internal buffering is done, so a complete compressed audio packet
 * must be passed each time.
 * \param fsound A FishSound* handle (created with mode FISH_SOUND_DECODE)
 * \param buf A buffer containing a compressed audio packet
 * \param bytes A count of bytes to decode (i.e. the length of buf)
 * \returns The number of bytes consumed
 * \retval FISH_SOUND_ERR_STOP_OK Decoding was stopped by a FishSoundDecode*
 * callback returning FISH_SOUND_STOP_OK before any input bytes were consumed.
 * This will occur when PCM is decoded from previously buffered input, and
 * stopping is immediately requested.
 * \retval FISH_SOUND_ERR_STOP_ERR Decoding was stopped by a FishSoundDecode*
 * callback returning FISH_SOUND_STOP_ERR before any input bytes were consumed.
 * This will occur when PCM is decoded from previously buffered input, and
 * stopping is immediately requested.
 * \retval FISH_SOUND_ERR_BAD Not a valid FishSound* handle
 * \retval FISH_SOUND_ERR_OUT_OF_MEMORY Out of memory
 */
long fish_sound_decode (FishSound * fsound, unsigned char * buf, long bytes);

#ifdef __cplusplus
}
#endif

#endif /* __FISH_SOUND_DECODE_H__ */
