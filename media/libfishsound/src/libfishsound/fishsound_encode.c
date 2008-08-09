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

#include "config.h"

#include <stdio.h>
#include <string.h>

#include "private.h"

int
fish_sound_set_encoded_callback (FishSound * fsound,
				 FishSoundEncoded encoded,
				 void * user_data)
{
  if (fsound == NULL) return -1;

#if FS_ENCODE
  fsound->callback.encoded = (void *)encoded;
  fsound->user_data = user_data;
#else
  return FISH_SOUND_ERR_DISABLED;
#endif

  return 0;
}

long fish_sound_encode_float (FishSound * fsound, float * pcm[], long frames)
{
  if (fsound == NULL) return -1;

#if FS_ENCODE
  if (fsound->codec && fsound->codec->encode_f)
    return fsound->codec->encode_f (fsound, pcm, frames);
#else
  return FISH_SOUND_ERR_DISABLED;
#endif

  return 0;
}

long fish_sound_encode_float_ilv (FishSound * fsound, float ** pcm,
				  long frames)
{
  if (fsound == NULL) return -1;

#if FS_ENCODE
  if (fsound->codec && fsound->codec->encode_f_ilv)
    return fsound->codec->encode_f_ilv (fsound, pcm, frames);
#else
  return FISH_SOUND_ERR_DISABLED;
#endif

  return 0;
}

#ifndef FS_DISABLE_DEPRECATED
long
fish_sound_encode (FishSound * fsound, float ** pcm, long frames)
{
  if (fsound == NULL) return -1;

#if FS_ENCODE
  if (fsound->interleave) {
    if (fsound->codec && fsound->codec->encode_f_ilv)
      return fsound->codec->encode_f_ilv (fsound, pcm, frames);
  } else {
    if (fsound->codec && fsound->codec->encode_f)
      return fsound->codec->encode_f (fsound, pcm, frames);
  }
#else
  return FISH_SOUND_ERR_DISABLED;
#endif

  return 0;
}
#endif /* DEPRECATED */
