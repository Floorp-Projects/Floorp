/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ParseRTPList_h___
#define ParseRTPList_h___

#include <stdint.h>
#include <string.h>
#include "prtime.h"

/* ParseFTPList() parses lines from an FTP LIST command.
**
** Written July 2002 by Cyrus Patel <cyp@fb14.uni-mainz.de>
** with acknowledgements to squid, lynx, wget and ftpmirror.
**
** Arguments:
**   'line':       line of FTP data connection output. The line is assumed
**                 to end at the first '\0' or '\n' or '\r\n'.
**   'state':      a structure used internally to track state between
**                 lines. Needs to be bzero()'d at LIST begin.
**   'result':     where ParseFTPList will store the results of the parse
**                 if 'line' is not a comment and is not junk.
**
** Returns one of the following:
**    'd' - LIST line is a directory entry ('result' is valid)
**    'f' - LIST line is a file's entry ('result' is valid)
**    'l' - LIST line is a symlink's entry ('result' is valid)
**    '?' - LIST line is junk. (cwd, non-file/dir/link, etc)
**    '"' - its not a LIST line (its a "comment")
**
** It may be advisable to let the end-user see "comments" (particularly when
** the listing results in ONLY such lines) because such a listing may be:
** - an unknown LIST format (NLST or "custom" format for example)
** - an error msg (EPERM,ENOENT,ENFILE,EMFILE,ENOTDIR,ENOTBLK,EEXDEV etc).
** - an empty directory and the 'comment' is a "total 0" line or similar.
**   (warning: a "total 0" can also mean the total size is unknown).
**
** ParseFTPList() supports all known FTP LISTing formats:
** - '/bin/ls -l' and all variants (including Hellsoft FTP for NetWare);
** - EPLF (Easily Parsable List Format);
** - Windows NT's default "DOS-dirstyle";
** - OS/2 basic server format LIST format;
** - VMS (MultiNet, UCX, and CMU) LIST format (including multi-line format);
** - IBM VM/CMS, VM/ESA LIST format (two known variants);
** - SuperTCP FTP Server for Win16 LIST format;
** - NetManage Chameleon (NEWT) for Win16 LIST format;
** - '/bin/dls' (two known variants, plus multi-line) LIST format;
** If there are others, then I'd like to hear about them (send me a sample).
**
** NLSTings are not supported explicitely because they cannot be machine
** parsed consistently: NLSTings do not have unique characteristics - even
** the assumption that there won't be whitespace on the line does not hold
** because some nlistings have more than one filename per line and/or
** may have filenames that have spaces in them. Moreover, distinguishing
** between an error message and an NLST line would require ParseList() to
** recognize all the possible strerror() messages in the world.
*/


/* #undef anything you don't want to support */
#define SUPPORT_LSL  /* /bin/ls -l and dozens of variations therof */
#define SUPPORT_DLS  /* /bin/dls format (very, Very, VERY rare) */
#define SUPPORT_EPLF /* Extraordinarily Pathetic List Format */
#define SUPPORT_DOS  /* WinNT server in 'site dirstyle' dos */
#define SUPPORT_VMS  /* VMS (all: MultiNet, UCX, CMU-IP) */
#define SUPPORT_CMS  /* IBM VM/CMS,VM/ESA (z/VM and LISTING forms) */
#define SUPPORT_OS2  /* IBM TCP/IP for OS/2 - FTP Server */
#define SUPPORT_W16  /* win16 hosts: SuperTCP or NetManage Chameleon */

struct list_state
{
  list_state() {
    memset(this, 0, sizeof(*this));
  }

  PRTime         now_time;      /* needed for year determination */
  PRExplodedTime now_tm;        /* needed for year determination */
  int32_t        lstyle;        /* LISTing style */
  int32_t        parsed_one;    /* returned anything yet? */
  char           carry_buf[84]; /* for VMS multiline */
  uint32_t       carry_buf_len; /* length of name in carry_buf */
  uint32_t       numlines;      /* number of lines seen */
};

struct list_result
{
  int32_t           fe_type;      /* 'd'(dir) or 'l'(link) or 'f'(file) */
  const char *      fe_fname;     /* pointer to filename */
  uint32_t          fe_fnlen;     /* length of filename */
  const char *      fe_lname;     /* pointer to symlink name */
  uint32_t          fe_lnlen;     /* length of symlink name */
  char              fe_size[40];  /* size of file in bytes (<= (2^128 - 1)) */
  PRExplodedTime    fe_time;      /* last-modified time */
  int32_t           fe_cinfs;     /* file system is definitely case insensitive */
                                  /* (converting all-upcase names may be desirable) */
};

int ParseFTPList(const char *line,
                 struct list_state *state,
                 struct list_result *result );

#endif /* !ParseRTPList_h___ */
