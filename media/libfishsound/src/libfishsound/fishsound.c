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
fish_sound_identify (unsigned char * buf, long bytes)
{
  if (bytes < 8) return FISH_SOUND_ERR_SHORT_IDENTIFY;

  if (HAVE_VORBIS &&
      fish_sound_vorbis_identify (buf, bytes) != FISH_SOUND_UNKNOWN)
    return FISH_SOUND_VORBIS;

  if (HAVE_SPEEX &&
      fish_sound_speex_identify (buf, bytes) != FISH_SOUND_UNKNOWN)
    return FISH_SOUND_SPEEX;

  if (fish_sound_flac_identify (buf, bytes) != FISH_SOUND_UNKNOWN)
    return FISH_SOUND_FLAC;

  return FISH_SOUND_UNKNOWN;
}

int
fish_sound_set_format (FishSound * fsound, int format)
{
  if (format == FISH_SOUND_VORBIS) {
    fsound->codec = fish_sound_vorbis_codec ();
  } else if (format == FISH_SOUND_SPEEX) {
    fsound->codec = fish_sound_speex_codec ();
  } else if (format == FISH_SOUND_FLAC) {
    fsound->codec = fish_sound_flac_codec ();
   } else {
    return -1;
  }

  if (fsound->codec && fsound->codec->init)
    if (fsound->codec->init (fsound) == NULL) return -1;

  fsound->info.format = format;

  return format;
}

FishSound *
fish_sound_new (int mode, FishSoundInfo * fsinfo)
{
  FishSound * fsound;

  if (!FS_DECODE && mode == FISH_SOUND_DECODE) return NULL;

  if (!FS_ENCODE && mode == FISH_SOUND_ENCODE) return NULL;

  if (mode == FISH_SOUND_ENCODE) {
    if (fsinfo == NULL) {
      return NULL;
    } else {
      if (!(HAVE_VORBIS && HAVE_VORBISENC)) {
	if (fsinfo->format == FISH_SOUND_VORBIS) return NULL;
      }
      if (!HAVE_SPEEX) {
	if (fsinfo->format == FISH_SOUND_SPEEX) return NULL;
      }
      if (!HAVE_FLAC) {
        if (fsinfo->format == FISH_SOUND_FLAC) return NULL;
      }
    }
  } else if (mode != FISH_SOUND_DECODE) {
    return NULL;
  }

  fsound = fs_malloc (sizeof (FishSound));
  if (fsound == NULL) return NULL;

  fsound->mode = mode;
  fsound->interleave = 0;
  fsound->frameno = 0;
  fsound->next_granulepos = -1;
  fsound->next_eos = 0;
  fsound->codec = NULL;
  fsound->codec_data = NULL;
  fsound->callback.encoded = NULL;
  fsound->user_data = NULL;

  fish_sound_comments_init (fsound);

  if (mode == FISH_SOUND_DECODE) {
    fsound->info.samplerate = 0;
    fsound->info.channels = 0;
    fsound->info.format = FISH_SOUND_UNKNOWN;
  } else if (mode == FISH_SOUND_ENCODE) {
    fsound->info.samplerate = fsinfo->samplerate;
    fsound->info.channels = fsinfo->channels;
    fsound->info.format = fsinfo->format;

    if (fish_sound_set_format (fsound, fsinfo->format) == -1) {
      fs_free (fsound);
      return NULL;
    }
  }

  return fsound;
}

long
fish_sound_flush (FishSound * fsound)
{
  if (fsound == NULL) return -1;

  if (fsound->codec && fsound->codec->flush)
    return fsound->codec->flush (fsound);

  return 0;
}

int
fish_sound_reset (FishSound * fsound)
{
  if (fsound == NULL) return -1;

  if (fsound->codec && fsound->codec->reset)
    return fsound->codec->reset (fsound);

  return 0;
}

FishSound *
fish_sound_delete (FishSound * fsound)
{
  if (fsound == NULL) return NULL;

  if (fsound->codec && fsound->codec->del)
    fsound->codec->del (fsound);

  fs_free (fsound->codec);

  fish_sound_comments_free (fsound);

  fs_free (fsound);

  return NULL;
}

int
fish_sound_command (FishSound * fsound, int command, void * data, int datasize)
{
  FishSoundInfo * fsinfo = (FishSoundInfo *)data;
  int * pi = (int *)data;

  if (fsound == NULL) return -1;

  switch (command) {
  case FISH_SOUND_GET_INFO:
    memcpy (fsinfo, &fsound->info, sizeof (FishSoundInfo));
    break;
  case FISH_SOUND_GET_INTERLEAVE:
    *pi = fsound->interleave;
    break;
  case FISH_SOUND_SET_INTERLEAVE:
    fsound->interleave = (*pi ? 1 : 0);
    break;
  default:
    if (fsound->codec && fsound->codec->command)
      return fsound->codec->command (fsound, command, data, datasize);
    break;
  }

  return 0;
}

int
fish_sound_get_interleave (FishSound * fsound)
{
  if (fsound == NULL) return -1;

  return fsound->interleave;
}

#ifndef FS_DISABLE_DEPRECATED
int
fish_sound_set_interleave (FishSound * fsound, int interleave)
{
  if (fsound == NULL) return -1;

  fsound->interleave = (interleave ? 1 : 0);

  return 0;
}
#endif

long
fish_sound_get_frameno (FishSound * fsound)
{
  if (fsound == NULL) return -1L;

  return fsound->frameno;
}

int
fish_sound_set_frameno (FishSound * fsound, long frameno)
{
  if (fsound == NULL) return -1;

  fsound->frameno = frameno;

  return 0;
}

int
fish_sound_prepare_truncation (FishSound * fsound, long next_granulepos,
			       int next_eos)
{
  if (fsound == NULL) return -1;

  fsound->next_granulepos = next_granulepos;
  fsound->next_eos = next_eos;

  return 0;
}
