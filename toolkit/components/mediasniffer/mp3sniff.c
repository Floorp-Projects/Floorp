/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* MPEG format parsing */

#include "mp3sniff.h"

/* Maximum packet size is 320 kbits/s * 144 / 32 kHz + 1 padding byte */
#define MP3_MAX_SIZE 1441

typedef struct {
  int version;
  int layer;
  int errp;
  int bitrate;
  int freq;
  int pad;
  int priv;
  int mode;
  int modex;
  int copyright;
  int original;
  int emphasis;
} mp3_header;

/* Parse the 4-byte header in p and fill in the header struct. */
static void mp3_parse(const uint8_t *p, mp3_header *header)
{
  const int bitrates[2][16] = {
        /* MPEG version 1 layer 3 bitrates. */
	{0,  32000,  40000,  48000,  56000,  64000,  80000,  96000,
         112000, 128000, 160000, 192000, 224000, 256000, 320000, 0},
        /* MPEG Version 2 and 2.5 layer 3 bitrates */
        {0, 8000, 16000, 24000, 32000, 40000, 48000, 56000, 64000,
         80000, 96000, 112000, 128000, 144000, 160000, 0} };
  const int samplerates[4] = {44100, 48000, 32000, 0};

  header->version = (p[1] & 0x18) >> 3;
  header->layer = 4 - ((p[1] & 0x06) >> 1);
  header->errp = (p[1] & 0x01);

  header->bitrate = bitrates[(header->version & 1) ? 0 : 1][(p[2] & 0xf0) >> 4];
  header->freq = samplerates[(p[2] & 0x0c) >> 2];
  if (header->version == 2) header->freq >>= 1;
  else if (header->version == 0) header->freq >>= 2;
  header->pad = (p[2] & 0x02) >> 1;
  header->priv = (p[2] & 0x01);

  header->mode = (p[3] & 0xc0) >> 6;
  header->modex = (p[3] & 0x30) >> 4;
  header->copyright = (p[3] & 0x08) >> 3;
  header->original = (p[3] & 0x04) >> 2;
  header->emphasis = (p[3] & 0x03);
}

/* calculate the size of an mp3 frame from its header */
static int mp3_framesize(mp3_header *header)
{
  int size;
  int scale;

  if ((header->version & 1) == 0) scale = 72;
  else scale = 144;
  size = header->bitrate * scale / header->freq;
  if (header->pad) size += 1;

  return size;
}

static int is_mp3(const uint8_t *p, long length) {
  /* Do we have enough room to see a 4 byte header? */
  if (length < 4) return 0;
  /* Do we have a sync pattern? */
  if (p[0] == 0xff && (p[1] & 0xe0) == 0xe0) {
    /* Do we have any illegal field values? */
    if (((p[1] & 0x06) >> 1) == 0) return 0;  /* No layer 4 */
    if (((p[2] & 0xf0) >> 4) == 15) return 0; /* Bitrate can't be 1111 */
    if (((p[2] & 0x0c) >> 2) == 3) return 0;  /* Samplerate can't be 11 */
    /* Looks like a header. */
    if ((4 - ((p[1] & 0x06) >> 1)) != 3) return 0; /* Only want level 3 */
    return 1;
  }
  return 0;
}

/* Identify an ID3 tag based on its header. */
/* http://id3.org/id3v2.4.0-structure */
static int is_id3(const uint8_t *p, long length) {
  /* Do we have enough room to see the header? */
  if (length < 10) return 0;
  /* Do we have a sync pattern? */
  if (p[0] == 'I' && p[1] == 'D' && p[2] == '3') {
    if (p[3] == 0xff || p[4] == 0xff) return 0; /* Illegal version. */
    if (p[6] & 0x80 || p[7] & 0x80 ||
        p[8] & 0x80) return 0; /* Bad length encoding. */
    /* Looks like an id3 header. */
    return 1;
  }
  return 0;
}

/* Calculate the size of an id3 tag structure from its header. */
static int id3_framesize(const uint8_t *p, long length)
{
  int size;

  /* Header is 10 bytes. */
  if (length < 10) {
    return 0;
  }
  /* Frame is header plus declared size. */
  size = 10 + (p[9] | (p[8] << 7) | (p[7] << 14) | (p[6] << 21));

  return size;
}

int mp3_sniff(const uint8_t *buf, long length)
{
  mp3_header header;
  const uint8_t *p;
  long skip;
  long avail;

  p = buf;
  avail = length;
  while (avail >= 4) {
    if (is_id3(p, avail)) {
      /* Skip over any id3 tags */
      skip = id3_framesize(p, avail);
      p += skip;
      avail -= skip;
    } else if (is_mp3(p, avail)) {
      mp3_parse(p, &header);
      skip = mp3_framesize(&header);
      if (skip < 4 || skip + 4 >= avail) {
        return 0;
      }
      p += skip;
      avail -= skip;
      /* Check for a second header at the expected offset. */
      if (is_mp3(p, avail)) {
        /* Looks like mp3. */
        return 1;
      } else {
        /* No second header. Not mp3. */
        return 0;
      }
    } else {
      /* No id3 tag or mp3 header. Not mp3. */
      return 0;
    }
  }

  return 0;
}
