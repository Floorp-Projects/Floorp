/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */

#ifndef _XI_ERRORS_H_
#define _XI_ERRORS_H_

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <gtk/gtk.h>

/*------------------------------------------------------------------*
 *   X Installer Errors
 *------------------------------------------------------------------*/
    enum
    {
        OK              = 0,
        E_MEM           = -601,     /* out of memory */
        E_PARAM         = -602,     /* invalid param */
        E_NO_MEMBER     = -603,     /* invalid member variable */
        E_INVALID_PTR   = -604,     /* invalid pointer */
        E_INVALID_KEY   = -605,     /* parse key has no value */
        E_EMPTY_README  = -606,     /* failed to read readme */
        E_EMPTY_LICENSE = -607,     /* failed to read license */
        E_OUT_OF_BOUNDS = -608,     /* out of bounds of comp/st list */
        E_REF_COUNT     = -609,     /* mismatched ref counts: mem leak */
        E_NO_COMPONENTS = -610,     /* no components in the INI file */
        E_NO_SETUPTYPES = -611,     /* no setup types in the INI file */
        E_URL_ALREADY   = -612,     /* URL at this index already exists */
        E_DIR_CREATE    = -613,     /* couldn't create the directory (tmp) */
        E_BAD_FTP_URL   = -614,     /* ftp url malformed */
        E_NO_DOWNLOAD   = -615,     /* download failed  */
        E_EXTRACTION    = -616,     /* extraction of xpcom failed */
        E_FORK_FAILED   = -617,     /* failed to fork a process */
        E_LIB_OPEN      = -618,     /* couldn't open stub lib */
        E_LIB_SYM       = -619,     /* couldn't get symbol in lib */
        E_XPI_FAIL      = -620,     /* a xpistub call failed */
        E_INSTALL       = -621,     /* a .xpi failed to install */
        E_CP_FAIL       = -622,     /* copy of a xpi failed */
        E_NO_DEST       = -623,     /* destination dir doesn't exist */
        E_MKDIR_FAIL    = -624,     /* can't make destination dir */
        E_OLD_INST      = -625,     /* old instllation exists */
        E_NO_PERMS      = -626,     /* don't have rwx perms on selected dir */
        E_NO_DISK_SPACE = -627,     /* not eough disk space to install */
        E_CRC_FAILED    = -628,     /* CRC failed */
        E_USAGE_SHOWN   = -629,     /* showed the usage message */
        E_DIR_NOT_EMPTY = -630      /* destination dir wasn't empty */
    };

#endif /* _XI_ERRORS_H_ */
