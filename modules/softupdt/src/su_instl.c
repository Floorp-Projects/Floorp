/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* su_instl.c
 * netscape.softupdate.InstallFile.java
 * native implementation
 */

/* The following two includes are unnecessary, but prevent
 * IS_LITTLE_ENDIAN warnings */
#include "xp_mcom.h"

#include "fe_proto.h"
#include "zig.h"
#include "xp_file.h"
#include "su_folderspec.h"
#include "su_instl.h"
#include "softupdt.h"
#include "NSReg.h"
#include "gdiff.h"


extern int MK_OUT_OF_MEMORY;

static 	XP_Bool	rebootShown = FALSE;
#ifdef XP_WIN16
static 	XP_Bool	utilityScheduled = FALSE;
#endif

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0


REGERR su_DeleteOldFileLater(char * filename)
{
    RKEY newkey;
    REGERR result = -1;
    HREG reg;
    if ( REGERR_OK == NR_RegOpen("", &reg) ) {
        if (REGERR_OK == NR_RegAddKey( reg, ROOTKEY_PRIVATE, 
            REG_DELETE_LIST_KEY, &newkey) )
        {
            result = NR_RegSetEntryString( reg, newkey, filename, "" );
        }

        NR_RegClose(reg);
    }

    return result;
}



REGERR su_ReplaceOldFileLater(char *tmpfile, char *target )
{
    RKEY newkey;
    REGERR err;
    HREG reg;

    err = NR_RegOpen("", &reg);
    if ( err == REGERR_OK ) {
        err = NR_RegAddKey( reg, ROOTKEY_PRIVATE, REG_REPLACE_LIST_KEY, &newkey);
        if ( err == REGERR_OK ) {
            err = NR_RegSetEntryString( reg, newkey, tmpfile, target );
        }
        NR_RegClose(reg);
    }
    return err;
}
