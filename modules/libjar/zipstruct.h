/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *   Jeroen Dobbelaere <jeroen.dobbelaere@acunia.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

/*
 * 'sizeof(struct XXX)' includes padding on ARM (see bug 87965)
 * As the internals of a jar/zip file must not depend on the target
 * architecture (i386, ppc, ARM, ...), use a fixed value instead.
 */ 
#define ZIPLOCAL_SIZE (4+2+2+2+2+2+4+4+4+2+2)

typedef struct ZipCentral_
  {
  unsigned char signature [4];
  unsigned char version_made_by [2];
  unsigned char version [2];
  unsigned char bitflag [2];
  unsigned char method [2];
  unsigned char time [2];
  unsigned char date [2];
  unsigned char crc32 [4];
  unsigned char size [4];
  unsigned char orglen [4];
  unsigned char filename_len [2];
  unsigned char extrafield_len [2];
  unsigned char commentfield_len [2];
  unsigned char diskstart_number [2];
  unsigned char internal_attributes [2];
  unsigned char external_attributes [4];
  unsigned char localhdr_offset [4];
} ZipCentral;

/*
 * 'sizeof(struct XXX)' includes padding on ARM (see bug 87965)
 * As the internals of a jar/zip file must not depend on the target
 * architecture (i386, ppc, ARM, ...), use a fixed value instead.
 */ 
#define ZIPCENTRAL_SIZE (4+2+2+2+2+2+2+4+4+4+2+2+2+2+2+4+4)

typedef struct ZipEnd_
  {
  unsigned char signature [4];
  unsigned char disk_nr [2];
  unsigned char start_central_dir [2];
  unsigned char total_entries_disk [2];
  unsigned char total_entries_archive [2];
  unsigned char central_dir_size [4];
  unsigned char offset_central_dir [4];
  unsigned char commentfield_len [2];
} ZipEnd;

/*
 * 'sizeof(struct XXX)' includes padding on ARM (see bug 87965)
 * As the internals of a jar/zip file must not depend on the target
 * architecture (i386, ppc, ARM, ...), use a fixed value instead.
 */ 
#define ZIPEND_SIZE (4+2+2+2+2+4+4+2)

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
#define UNSUPPORTED       0xFF


#endif /* _zipstruct_h */
