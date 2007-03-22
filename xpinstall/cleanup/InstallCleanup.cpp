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
 * The Original Code is Mozilla Navigator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Don Bragg <dbragg@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "InstallCleanup.h"

int PerformScheduledTasks(HREG reg)
{
    int deleteComplete  = DONE;
    int replaceComplete = DONE;

    deleteComplete  = DeleteScheduledFiles( reg );
    replaceComplete = ReplaceScheduledFiles( reg );
    if ((deleteComplete == DONE) && (replaceComplete == DONE))
        return DONE;
    else
        return TRY_LATER;
}


//----------------------------------------------------------------------------
// ReplaceScheduledFiles
//----------------------------------------------------------------------------
int ReplaceScheduledFiles( HREG reg )
{
    RKEY    key;
    int rv = DONE;

    /* replace files if any listed */
    if (REGERR_OK == NR_RegGetKey(reg,ROOTKEY_PRIVATE,REG_REPLACE_LIST_KEY,&key))
    {
        char keyname[MAXREGNAMELEN];
        char doomedFile[MAXREGPATHLEN];
        char srcFile[MAXREGPATHLEN];

        uint32 bufsize;
        REGENUM state = 0;
        while (REGERR_OK == NR_RegEnumSubkeys( reg, key, &state, 
                               keyname, sizeof(keyname), REGENUM_CHILDREN))
        {
            bufsize = sizeof(srcFile);
            REGERR err1 = NR_RegGetEntry( reg, (RKEY)state,
                               REG_REPLACE_SRCFILE, &srcFile, &bufsize);

            bufsize = sizeof(doomedFile);
            REGERR err2 = NR_RegGetEntry( reg, (RKEY)state,
                               REG_REPLACE_DESTFILE, &doomedFile, &bufsize);

            if ( err1 == REGERR_OK && err2 == REGERR_OK )
            {
                int result = NativeReplaceFile( srcFile, doomedFile );
                if (result == DONE)
                {
                    // This one is done
                    NR_RegDeleteKey( reg, key, keyname );
                }
            }
        }

        /* delete list node if empty */
        state = 0;
        if (REGERR_NOMORE == NR_RegEnumSubkeys( reg, key, &state, keyname,
                                     sizeof(keyname), REGENUM_CHILDREN ))
        {
            NR_RegDeleteKey(reg, ROOTKEY_PRIVATE, REG_REPLACE_LIST_KEY);
            rv = DONE;
        }
        else
        {
            rv = TRY_LATER;
        }
    }
    return rv;
}

//----------------------------------------------------------------------------
// DeleteScheduledFiles
//----------------------------------------------------------------------------
int DeleteScheduledFiles( HREG reg )
{
    REGERR  err;
    RKEY    key;
    REGENUM state = 0;
    int rv = DONE;

    /* perform scheduled file deletions  */
    if (REGERR_OK == NR_RegGetKey(reg,ROOTKEY_PRIVATE,REG_DELETE_LIST_KEY,&key))
    {
        // the delete key exists, so we loop through its children
        // and try to delete all the listed files

        char    namebuf[MAXREGNAMELEN];
        char    valbuf[MAXREGPATHLEN];

        while (REGERR_OK == NR_RegEnumEntries( reg, key, &state, namebuf,
                                               sizeof(namebuf), 0 ) )
        {
            uint32 bufsize = sizeof(valbuf); // gets changed, must reset
            err = NR_RegGetEntry( reg, key, namebuf, valbuf, &bufsize );
            if ( err == REGERR_OK )
            {
                rv = NativeDeleteFile(valbuf);
                if (rv == DONE)
                    NR_RegDeleteEntry( reg, key, namebuf);
            }
        }

        /* delete list node if empty */
        state = 0;
        err = NR_RegEnumEntries(reg, key, &state, namebuf, sizeof(namebuf), 0);
        if ( err == REGERR_NOMORE )
        {
            NR_RegDeleteKey(reg, ROOTKEY_PRIVATE, REG_DELETE_LIST_KEY);
            rv = DONE;
        }
        else
        {
            rv = TRY_LATER;
        }
    }
    return rv;
}

