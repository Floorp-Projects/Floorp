/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Daniel Veditz <dveditz@netscape.com>
 */

#ifndef _zipstruct_h
#define _zipstruct_h


/*
 *  Certain constants and structures for
 *  the Phil Katz ZIP archive format.
 *
 */

typedef struct ZipLocal_
  {
  unsigned char signature [4];
  unsigned char word [2];
  unsigned char bitflag [2];
  unsigned char method [2];
  unsigned char time [2];
  unsigned char date [2];
  unsigned char crc32 [4];
  unsigned char size [4];
  unsigned char orglen [4];
  unsigned char filename_len [2];
  unsigned char extrafield_len [2];
} ZipLocal;

typedef struct ZipCentral_
  {
  char signature [4];
  char version_made_by [2];
  char version [2];
  char bitflag [2];
  char method [2];
  char time [2];
  char date [2];
  char crc32 [4];
  char size [4];
  char orglen [4];
  char filename_len [2];
  char extrafield_len [2];
  char commentfield_len [2];
  char diskstart_number [2];
  char internal_attributes [2];
  char external_attributes [4];
  char localhdr_offset [4];
} ZipCentral;

typedef struct ZipEnd_
  {
  char signature [4];
  char disk_nr [2];
  char start_central_dir [2];
  char total_entries_disk [2];
  char total_entries_archive [2];
  char central_dir_size [4];
  char offset_central_dir [4];
  char commentfield_len [2];
} ZipEnd;

/* signatures */
#define LOCALSIG    0x04034B50l
#define CENTRALSIG  0x02014B50l
#define ENDSIG      0x06054B50l

/* compression methods */
#define STORED            0
#define SHRUNK            1
#define REDUCED1          2
#define REDUCED2          3
#define REDUCED3          4
#define REDUCED4          5
#define IMPLODED          6
#define TOKENIZED         7
#define DEFLATED          8


#endif /* _zipstruct_h */
