/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
