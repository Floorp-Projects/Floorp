/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Navigator.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corp.  Portions created by Netscape Communications Corp. are
 * Copyright (C) 1998, 1999, 2000, 2001 Netscape Communications Corp.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Don Bragg <dbragg@netscape.com>
 */
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

