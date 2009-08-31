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
#include <stdlib.h>
#include <string.h>

#include <ctype.h>

#include "private.h"
#include "convert.h"

/*#define DEBUG*/
#include "debug.h"

#if HAVE_VORBIS

#include <vorbis/codec.h>
#if HAVE_VORBISENC
#include <vorbis/vorbisenc.h>
#endif

typedef struct _FishSoundVorbisInfo {
  int packetno;
  int finished;
  vorbis_info vi;
  vorbis_comment vc;
  vorbis_dsp_state vd; /** central working state for the PCM->packet encoder */
  vorbis_block vb;     /** local working space for PCM->packet encode */
  float ** pcm; /** ongoing pcm working space for decoder (stateful) */
  float * ipcm; /** interleaved pcm for interfacing with user */
  long max_pcm;
} FishSoundVorbisInfo;

int
fish_sound_vorbis_identify (unsigned char * buf, long bytes)
{
  struct vorbis_info vi;
  struct vorbis_comment vc;
  ogg_packet op;
  int ret, id = FISH_SOUND_UNKNOWN;

  if (!strncmp ((char *)&buf[1], "vorbis", 6)) {
    /* if only a short buffer was passed, do a weak identify */
    if (bytes == 8) return FISH_SOUND_VORBIS;

    /* otherwise, assume the buffer is an entire initial header and
     * feed it to vorbis_synthesis_headerin() */

    vorbis_info_init (&vi);
    vorbis_comment_init (&vc);

    op.packet = buf;
    op.bytes = bytes;
    op.b_o_s = 1;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 0;

    if ((ret = vorbis_synthesis_headerin (&vi, &vc, &op)) == 0) {
      if (vi.rate != 0) id = FISH_SOUND_VORBIS;
    } else {
      debug_printf (1, "vorbis_synthesis_headerin returned %d", ret);
    }

    vorbis_info_clear (&vi);
  }

  return id;
}

static int
fs_vorbis_command (FishSound * fsound, int command, void * data,
		   int datasize)
{
  return 0;
}

#if FS_DECODE
static long
fs_vorbis_decode (FishSound * fsound, unsigned char * buf, long bytes)
{
  FishSoundVorbisInfo * fsv = (FishSoundVorbisInfo *)fsound->codec_data;
  ogg_packet op;
  long samples;
  float * pcm_new;
  int ret;

  /* Make an ogg_packet structure to pass the data to libvorbis */
  op.packet = buf;
  op.bytes = bytes;
  op.b_o_s = (fsv->packetno == 0) ? 1 : 0;
  op.e_o_s = fsound->next_eos;
  op.granulepos = fsound->next_granulepos;
  op.packetno = fsv->packetno;

  if (fsv->packetno < 3) {

    if ((ret = vorbis_synthesis_headerin (&fsv->vi, &fsv->vc, &op)) == 0) {
      if (fsv->vi.rate != 0) {
	debug_printf (1, "Got vorbis info: version %d\tchannels %d\trate %ld",
                      fsv->vi.version, fsv->vi.channels, fsv->vi.rate);
	fsound->info.samplerate = fsv->vi.rate;
	fsound->info.channels = fsv->vi.channels;
      }
    }

    /* Decode comments from packet 1. Vorbis has 7 bytes of marker at the
     * start of vorbiscomment packet. */
    if (fsv->packetno == 1 && bytes > 7 && buf[0] == 0x03 &&
	!strncmp ((char *)&buf[1], "vorbis", 6)) {
      if (fish_sound_comments_decode (fsound, buf+7, bytes-7) == FISH_SOUND_ERR_OUT_OF_MEMORY) {
        fsv->packetno++;
        return FISH_SOUND_ERR_OUT_OF_MEMORY;
      }
    } else if (fsv->packetno == 2) {
      vorbis_synthesis_init (&fsv->vd, &fsv->vi);
      vorbis_block_init (&fsv->vd, &fsv->vb);
    }
  } else {
    FishSoundDecoded_FloatIlv df;
    FishSoundDecoded_Float dfi;
    int r;
    if ((r = vorbis_synthesis (&fsv->vb, &op)) == 0) 
      vorbis_synthesis_blockin (&fsv->vd, &fsv->vb);
    
    if (r == OV_EBADPACKET) {
      return FISH_SOUND_ERR_GENERIC;
    }

    while ((samples = vorbis_synthesis_pcmout (&fsv->vd, &fsv->pcm)) > 0) {
      vorbis_synthesis_read (&fsv->vd, samples);

      if (fsound->frameno != -1)
	fsound->frameno += samples;

      if (fsound->interleave) {
	if (samples > fsv->max_pcm) {
          pcm_new = realloc (fsv->ipcm, sizeof(float) * samples *
			     fsound->info.channels);
          if (pcm_new == NULL) {
            /* Allocation failure; just truncate here, fail gracefully elsewhere */
            samples = fsv->max_pcm;
          } else {
	    fsv->ipcm = pcm_new;
	    fsv->max_pcm = samples;
          }
	}
	_fs_interleave (fsv->pcm, (float **)fsv->ipcm, samples,
			fsound->info.channels, 1.0);

	dfi = (FishSoundDecoded_FloatIlv)fsound->callback.decoded_float_ilv;
	dfi (fsound, (float **)fsv->ipcm, samples, fsound->user_data);
      } else {
	df = (FishSoundDecoded_Float)fsound->callback.decoded_float;
	df (fsound, fsv->pcm, samples, fsound->user_data);
      }
    }
  }

  if (fsound->next_granulepos != -1) {
    fsound->frameno = fsound->next_granulepos;
    fsound->next_granulepos = -1;
  }

  fsv->packetno++;

  return 0;
}
#else /* !FS_DECODE */

#define fs_vorbis_decode NULL

#endif

#if FS_ENCODE && HAVE_VORBISENC

static FishSound *
fs_vorbis_enc_headers (FishSound * fsound)
{
  FishSoundVorbisInfo * fsv = (FishSoundVorbisInfo *)fsound->codec_data;
  const FishSoundComment * comment;
  ogg_packet header;
  ogg_packet header_comm;
  ogg_packet header_code;

  /* Vorbis streams begin with three headers:
   *   1. The initial header (with most of the codec setup parameters),
   *      which is mandated by the Ogg bitstream spec,
   *   2. The second header which holds any comment fields,
   *   3. The third header which contains the bitstream codebook.
   * We merely need to make the headers, then pass them to libvorbis one at
   * a time; libvorbis handles the additional Ogg bitstream constraints.
   */

  /* Update the comments */
  for (comment = fish_sound_comment_first (fsound); comment;
       comment = fish_sound_comment_next (fsound, comment)) {
    debug_printf (1, "%s = %s", comment->name, comment->value);
    vorbis_comment_add_tag (&fsv->vc, comment->name, comment->value);
  }

  /* Generate the headers */
  vorbis_analysis_headerout(&fsv->vd, &fsv->vc,
			    &header, &header_comm, &header_code);

  /* Pass the generated headers to the user */
  if (fsound->callback.encoded) {
    FishSoundEncoded encoded = (FishSoundEncoded)fsound->callback.encoded;

    encoded (fsound, header.packet, header.bytes, fsound->user_data);
    encoded (fsound, header_comm.packet, header_comm.bytes,
	     fsound->user_data);
    encoded (fsound, header_code.packet, header_code.bytes,
	     fsound->user_data);
    fsv->packetno = 3;
  }

  return fsound;
}

static long
fs_vorbis_encode_write (FishSound * fsound, long len)
{
  FishSoundVorbisInfo * fsv = (FishSoundVorbisInfo *)fsound->codec_data;
  ogg_packet op;

  vorbis_analysis_wrote (&fsv->vd, len);

  while (vorbis_analysis_blockout (&fsv->vd, &fsv->vb) == 1) {
    vorbis_analysis (&fsv->vb, NULL);
    vorbis_bitrate_addblock (&fsv->vb);

    while (vorbis_bitrate_flushpacket (&fsv->vd, &op)) {
      if (fsound->callback.encoded) {
	FishSoundEncoded encoded = (FishSoundEncoded)fsound->callback.encoded;

	if (op.granulepos != -1)
	  fsound->frameno = op.granulepos;

	encoded (fsound, op.packet, op.bytes, fsound->user_data);

	fsv->packetno++;
      }
    }
  }

  return len;
}

static int
fs_vorbis_finish (FishSound * fsound)
{
  FishSoundVorbisInfo * fsv = (FishSoundVorbisInfo *)fsound->codec_data;

  if (!fsv->finished) {
    if (fsound->mode == FISH_SOUND_ENCODE) {
      fs_vorbis_encode_write (fsound, 0);
    }
    fsv->finished = 1;
  }

  return 0;
}

static long
fs_vorbis_encode_f_ilv (FishSound * fsound, float ** pcm, long frames)
{
  FishSoundVorbisInfo * fsv = (FishSoundVorbisInfo *)fsound->codec_data;
  float ** vpcm;
  long len, remaining = frames;
  float * d = (float *)pcm;

  if (fsv->packetno == 0) {
    fs_vorbis_enc_headers (fsound);
  }

  if (frames == 0) {
    fs_vorbis_finish (fsound);
    return 0;
  }

  while (remaining > 0) {
    len = MIN (1024, remaining);

    /* expose the buffer to submit data */
    vpcm = vorbis_analysis_buffer (&fsv->vd, 1024);

    _fs_deinterleave ((float **)d, vpcm, len, fsound->info.channels, 1.0);

    d += (len * fsound->info.channels);

    fs_vorbis_encode_write (fsound, len);

    remaining -= len;
  }

  /**
   * End of input. Tell libvorbis we're at the end of stream so that it can
   * handle the last frame and mark the end of stream in the output properly.
   */
  if (fsound->next_eos)
    fs_vorbis_finish (fsound);

  return 0;
}

static long
fs_vorbis_encode_f (FishSound * fsound, float * pcm[], long frames)
{
  FishSoundVorbisInfo * fsv = (FishSoundVorbisInfo *)fsound->codec_data;
  float ** vpcm;
  long len, remaining = frames;
  int i;

  if (fsv->packetno == 0) {
    fs_vorbis_enc_headers (fsound);
  }

  if (frames == 0) {
    fs_vorbis_finish (fsound);
    return 0;
  }

  while (remaining > 0) {
    len = MIN (1024, remaining);

    debug_printf (1, "processing %ld frames", len);

    /* expose the buffer to submit data */
    vpcm = vorbis_analysis_buffer (&fsv->vd, 1024);

    for (i = 0; i < fsound->info.channels; i++) {
      memcpy (vpcm[i], pcm[i], sizeof (float) * len);
    }

    fs_vorbis_encode_write (fsound, len);

    remaining -= len;
  }

  /**
   * End of input. Tell libvorbis we're at the end of stream so that it can
   * handle the last frame and mark the end of stream in the output properly.
   */
  if (fsound->next_eos)
    fs_vorbis_finish (fsound);

  return 0;
}

static FishSound *
fs_vorbis_enc_init (FishSound * fsound)
{
  FishSoundVorbisInfo * fsv = (FishSoundVorbisInfo *)fsound->codec_data;

  debug_printf (1, "Vorbis enc init: %d channels, %d Hz", fsound->info.channels,
                fsound->info.samplerate);

  vorbis_encode_init_vbr (&fsv->vi, fsound->info.channels,
			  fsound->info.samplerate, (float)0.3 /* quality */);

  /* set up the analysis state and auxiliary encoding storage */
  vorbis_analysis_init (&fsv->vd, &fsv->vi);
  vorbis_block_init (&fsv->vd, &fsv->vb);

  return fsound;
}

#else /* ! FS_ENCODE && HAVE_VORBISENC */

#define fs_vorbis_encode_f NULL
#define fs_vorbis_encode_f_ilv NULL
#define fs_vorbis_finish NULL

#endif /* ! FS_ENCODE && HAVE_VORBISENC */

static int
fs_vorbis_reset (FishSound * fsound)
{
  FishSoundVorbisInfo * fsv = (FishSoundVorbisInfo *)fsound->codec_data;

  vorbis_block_init (&fsv->vd, &fsv->vb);
  fsv->packetno = 0;

  return 0;
}

static FishSound *
fs_vorbis_init (FishSound * fsound)
{
  FishSoundVorbisInfo * fsv;

  fsv = fs_malloc (sizeof (FishSoundVorbisInfo));
  if (fsv == NULL) return NULL;

  fsv->packetno = 0;
  fsv->finished = 0;
  vorbis_info_init (&fsv->vi);
  vorbis_comment_init (&fsv->vc);
  memset(&fsv->vd, 0, sizeof(fsv->vd));
  vorbis_block_init (&fsv->vd, &fsv->vb);
  fsv->pcm = NULL;
  fsv->ipcm = NULL;
  fsv->max_pcm = 0;

  fsound->codec_data = fsv;

#if FS_ENCODE && HAVE_VORBISENC

  if (fsound->mode == FISH_SOUND_ENCODE) {
    fs_vorbis_enc_init (fsound);
  }

#endif /* FS_ENCODE && HAVE_VORBISENC */

  return fsound;
}

static FishSound *
fs_vorbis_delete (FishSound * fsound)
{
  FishSoundVorbisInfo * fsv = (FishSoundVorbisInfo *)fsound->codec_data;

#if FS_ENCODE && HAVE_VORBISENC
  fs_vorbis_finish (fsound);
#endif /* FS_ENCODE && HAVE_VORBISENC */

  if (fsv->ipcm) fs_free (fsv->ipcm);

  vorbis_block_clear (&fsv->vb);
  vorbis_dsp_clear (&fsv->vd);
  vorbis_comment_clear (&fsv->vc);
  vorbis_info_clear (&fsv->vi);

  fs_free (fsv);
  fsound->codec_data = NULL;

  return fsound;
}

FishSoundCodec *
fish_sound_vorbis_codec (void)
{
  FishSoundCodec * codec;

  codec = (FishSoundCodec *) fs_malloc (sizeof (FishSoundCodec));
  if (codec == NULL) return NULL;

  codec->format.format = FISH_SOUND_VORBIS;
  codec->format.name = "Vorbis (Xiph.Org)";
  codec->format.extension = "ogg";

  codec->init = fs_vorbis_init;
  codec->del = fs_vorbis_delete;
  codec->reset = fs_vorbis_reset;
  codec->update = NULL; /* XXX */
  codec->command = fs_vorbis_command;
  codec->decode = fs_vorbis_decode;
  codec->encode_f = fs_vorbis_encode_f;
  codec->encode_f_ilv = fs_vorbis_encode_f_ilv;
  codec->flush = NULL;

  return codec;
}

#else /* !HAVE_VORBIS */

int
fish_sound_vorbis_identify (unsigned char * buf, long bytes)
{
  return FISH_SOUND_UNKNOWN;
}

FishSoundCodec *
fish_sound_vorbis_codec (void)
{
  return NULL;
}

#endif
