/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _zipstruct_h
#define _zipstruct_h

/*
 *  Certain constants and structures for
 *  the Phil Katz ZIP archive format.
 *
 */

typedef struct ZipLocal_ {
  unsigned char signature[4];
  unsigned char word[2];
  unsigned char bitflag[2];
  unsigned char method[2];
  unsigned char time[2];
  unsigned char date[2];
  unsigned char crc32[4];
  unsigned char size[4];
  unsigned char orglen[4];
  unsigned char filename_len[2];
  unsigned char extrafield_len[2];
} ZipLocal;

/*
 * 'sizeof(struct XXX)' includes padding on ARM (see bug 87965)
 * As the internals of a jar/zip file must not depend on the target
 * architecture (i386, ppc, ARM, ...), use a fixed value instead.
 */
#define ZIPLOCAL_SIZE (4 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4 + 2 + 2)

typedef struct ZipCentral_ {
  unsigned char signature[4];
  unsigned char version_made_by[2];
  unsigned char version[2];
  unsigned char bitflag[2];
  unsigned char method[2];
  unsigned char time[2];
  unsigned char date[2];
  unsigned char crc32[4];
  unsigned char size[4];
  unsigned char orglen[4];
  unsigned char filename_len[2];
  unsigned char extrafield_len[2];
  unsigned char commentfield_len[2];
  unsigned char diskstart_number[2];
  unsigned char internal_attributes[2];
  unsigned char external_attributes[4];
  unsigned char localhdr_offset[4];
} ZipCentral;

/*
 * 'sizeof(struct XXX)' includes padding on ARM (see bug 87965)
 * As the internals of a jar/zip file must not depend on the target
 * architecture (i386, ppc, ARM, ...), use a fixed value instead.
 */
#define ZIPCENTRAL_SIZE \
  (4 + 2 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4 + 2 + 2 + 2 + 2 + 2 + 4 + 4)

typedef struct ZipEnd_ {
  unsigned char signature[4];
  unsigned char disk_nr[2];
  unsigned char start_central_dir[2];
  unsigned char total_entries_disk[2];
  unsigned char total_entries_archive[2];
  unsigned char central_dir_size[4];
  unsigned char offset_central_dir[4];
  unsigned char commentfield_len[2];
} ZipEnd;

/*
 * 'sizeof(struct XXX)' includes padding on ARM (see bug 87965)
 * As the internals of a jar/zip file must not depend on the target
 * architecture (i386, ppc, ARM, ...), use a fixed value instead.
 */
#define ZIPEND_SIZE (4 + 2 + 2 + 2 + 2 + 4 + 4 + 2)

/* signatures */
#define LOCALSIG 0x04034B50l
#define CENTRALSIG 0x02014B50l
#define ENDSIG 0x06054B50l
#define ENDSIG64 0x6064B50l

/* extra fields */
#define EXTENDED_TIMESTAMP_FIELD 0x5455
#define EXTENDED_TIMESTAMP_MODTIME 0x01

/* compression methods */
#define STORED 0
#define SHRUNK 1
#define REDUCED1 2
#define REDUCED2 3
#define REDUCED3 4
#define REDUCED4 5
#define IMPLODED 6
#define TOKENIZED 7
#define DEFLATED 8
#define UNSUPPORTED 0xFF

#endif /* _zipstruct_h */
