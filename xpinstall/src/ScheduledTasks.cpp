/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *     Daniel Veditz <dveditz@netscape.com>
 *     Douglas Turner <dougt@netscape.com>
 */

#include "nscore.h"
#include "NSReg.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsInstall.h" // for error codes
#include "prmem.h"
#include "ScheduledTasks.h"



static nsresult 
GetPersistentStringFromSpec(const nsFileSpec& inSpec, char **string)
{
    if (!string) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIFileSpec> spec;
#ifdef XP_MAC
    nsFileSpec interim = inSpec.GetFSSpec(); /* XXX get rid of mError in nsFileSpec */
    nsresult rv = NS_NewFileSpecWithSpec(interim, getter_AddRefs(spec));
#else
    nsresult rv = NS_NewFileSpecWithSpec(inSpec, getter_AddRefs(spec));
#endif
    if (NS_SUCCEEDED(rv)) {
        rv = spec->GetPersistentDescriptorString(string);
    } 
    else {
        *string = nsnull;
    }
    return rv;
}





#ifdef _WINDOWS
#include <sys/stat.h>
#include <windows.h>

PRInt32 ReplaceExistingWindowsFile(const nsFileSpec& currentSpec, const nsFileSpec& finalSpec)
{
    // this routine is now for DOS-based windows only. WinNT should 
    // be taken care of by the XP code
    //
    // NOTE for WINNT:
    //
    // the MOVEFILE_DELAY_UNTIL_REBOOT option doesn't work on
    // NT 3.51 SP4 or on NT 4.0 until SP2. On the broken versions
    // of NT 4.0 Microsoft warns using it can lead to an irreparably 
    // corrupt  windows' registry "after an unknown number of calls".
    // Time to reinstall windows when that happens.
    //
    // I don't want to risk it, I also don't want two separate code
    // paths to test, so we do it the lame way on all NT systems
    // until such time as there are few enough old revs around to
    // make it worth switching back to MoveFileEx().

    PRInt32 err = -1;

    /* Get OS version info */
    DWORD dwVersion = GetVersion();

    /* Get build numbers for Windows NT or Win32s */

    if (dwVersion > 0x80000000)
    {
        // Windows 95 or Win16

        // Place an entry in the WININIT.INI file in the Windows directory
        // to delete finalName and rename currentName to be finalName at reboot

        int     strlen;
        char    Src[_MAX_PATH];   // 8.3 name
        char    Dest[_MAX_PATH];  // 8.3 name

        char* final   = strdup(finalSpec.GetNativePathCString());
        char* current = strdup(currentSpec.GetNativePathCString());

        strlen = GetShortPathName( (LPCTSTR)current, (LPTSTR)Src, (DWORD)sizeof(Src) );
        if ( strlen > 0 ) 
        {
            free(current);
            current   = strdup(Src);
        }

        strlen = GetShortPathName( (LPCTSTR) final, (LPTSTR) Dest, (DWORD) sizeof(Dest));
        if ( strlen > 0 ) 
        {
            free(final);
            final   = strdup(Dest);
        }
        
        /* NOTE: use OEM filenames! Even though it looks like a Windows
         *       .INI file, WININIT.INI is processed under DOS 
         */
        
        AnsiToOem( final, final );
        AnsiToOem( current, current );

        if ( WritePrivateProfileString( "Rename", final, current, "WININIT.INI" ) )
            err = 0;

        free(final);
        free(current);
    }
    
    return err;
}
#endif




PRInt32 DeleteFileNowOrSchedule(const nsFileSpec& filename)
{

    PRInt32 result = nsInstall::SUCCESS;

    filename.Delete(PR_FALSE);
    if (filename.Exists())
    {
        // could not delete, schedule it for later

        RKEY newkey;
        HREG reg;
        REGERR  err;
        result = nsInstall::UNEXPECTED_ERROR;

        err = NR_RegOpen("", &reg) ;
        if ( err == REGERR_OK )
        {
            err = NR_RegAddKey(reg,ROOTKEY_PRIVATE,REG_DELETE_LIST_KEY,&newkey);
            if ( err == REGERR_OK )
            {
                char    valname[20];
                char*   fnamestr = nsnull;

                err = NR_RegGetUniqueName( reg, valname, sizeof(valname) );
                if ( err == REGERR_OK )
                {
                    nsresult rv;
                    rv = GetPersistentStringFromSpec( filename, &fnamestr );
                    if ( NS_SUCCEEDED(rv) && fnamestr )
                    {

                        err = NR_RegSetEntry( reg, newkey, valname, 
                                              REGTYPE_ENTRY_BYTES, 
                                              (void*)fnamestr, 
                                              strlen(fnamestr)+1);

                        if ( err == REGERR_OK )
                            result = nsInstall::REBOOT_NEEDED;
                    }
                }
            }

            NR_RegClose(reg);
        }
    }

    return result;
}




PRInt32 ReplaceFileNow(nsFileSpec& replacementFile, nsFileSpec& doomedFile )
{
    // replacement file must exist, doomed file doesn't have to
    if ( !replacementFile.Exists() )
        return nsInstall::DOES_NOT_EXIST;

    // don't have to do anything if the files are the same
    if ( replacementFile == doomedFile )
        return nsInstall::SUCCESS;


    PRInt32 result = nsInstall::ACCESS_DENIED;

    // first try to rename the doomed file out of the way (if it exists)
    char*   leafname;
    nsFileSpec tmpFile( doomedFile );
    if ( tmpFile.Exists() )
    {
        tmpFile.MakeUnique();
        leafname = tmpFile.GetLeafName();
        tmpFile = doomedFile;
        tmpFile.Rename( leafname );
        nsCRT::free( leafname );
    }


    // if doomedFile is gone move new file into place
    nsresult rv;
    if ( !doomedFile.Exists() )
    {
        nsFileSpec parentofFinalFile;
        nsFileSpec parentofReplacementFile;

        doomedFile.GetParent(parentofFinalFile);
        replacementFile.GetParent(parentofReplacementFile);

        // XXX looks dangerous, the replacement file name may NOT be unique in the
        // target directory if we have to move it! Either we should never move the
        // files like this (i.e. error if not in the same dir) or we need to take
        // a little more care in the move.
        if(parentofReplacementFile != parentofFinalFile)
        {
            NS_WARN_IF_FALSE( 0, "File unpacked into a non-dest dir" );
            rv = replacementFile.MoveToDir(parentofFinalFile);
        }
        else
        	rv = NS_OK;
        	
        leafname = doomedFile.GetLeafName();
        if ( NS_SUCCEEDED(rv) )
            rv = replacementFile.Rename( leafname );

        if ( NS_SUCCEEDED(rv) ) 
        {
            // we replaced the old file OK, now we have to
            // get rid of it permanently
            result = DeleteFileNowOrSchedule( tmpFile );
        }
        else
        {
            // couldn't rename file, try to put old file back
            tmpFile.Rename( leafname );
        }
        nsCRT::free( leafname );
    }

    return result;
}





PRInt32 ReplaceFileNowOrSchedule(nsFileSpec& replacementFile, nsFileSpec& doomedFile )
{
    PRInt32 result = ReplaceFileNow( replacementFile, doomedFile );

    if ( result == nsInstall::ACCESS_DENIED )
    {
        // if we couldn't replace the file schedule it for later
#ifdef _WINDOWS
        if ( ReplaceExistingWindowsFile(replacementFile, doomedFile) == 0 )
            return nsInstall::REBOOT_NEEDED;
#endif

        RKEY    listkey;
        RKEY    filekey;
        HREG    reg;
        REGERR  err;

        if ( REGERR_OK == NR_RegOpen("", &reg) ) 
        {
            err = NR_RegAddKey( reg, ROOTKEY_PRIVATE, REG_REPLACE_LIST_KEY, &listkey );
            if ( err == REGERR_OK ) 
            {
                char     valname[20];
                char*    fsrc = nsnull;
                char*    fdest = nsnull;
                REGERR   err2;
                nsresult rv, rv2;

                err = NR_RegGetUniqueName( reg, valname, sizeof(valname) );
                if ( err == REGERR_OK )
                {
                    err = NR_RegAddKey( reg, listkey, valname, &filekey );
                    if ( REGERR_OK == err )
                    {
                        rv = GetPersistentStringFromSpec(replacementFile, &fsrc);
                        rv2 = GetPersistentStringFromSpec(doomedFile, &fdest);
                        if ( NS_SUCCEEDED(rv) && NS_SUCCEEDED(rv2) )
                        {

                            err = NR_RegSetEntry( reg, filekey, 
                                                  REG_REPLACE_SRCFILE,
                                                  REGTYPE_ENTRY_BYTES, 
                                                  (void*)fsrc, 
                                                  strlen(fsrc));

                            err2 = NR_RegSetEntry(reg, filekey,
                                                  REG_REPLACE_DESTFILE,
                                                  REGTYPE_ENTRY_BYTES,
                                                  (void*)fdest,
                                                  strlen(fdest));

                            if ( err == REGERR_OK && err2 == REGERR_OK )
                                result = nsInstall::REBOOT_NEEDED;
                            else
                                NR_RegDeleteKey( reg, listkey, valname );
                        }

                        if (NS_SUCCEEDED(rv))
                            nsCRT::free(fsrc);

                        if (NS_SUCCEEDED(rv2))
                            nsCRT::free(fdest);
                    }
                }
            }

            NR_RegClose(reg);
        }
    }

    return result;
}




//-----------------------------------------------------------------------------
//
//          STARTUP: DO SCHEDULED ACTIONS
//
//-----------------------------------------------------------------------------

void DeleteScheduledFiles(HREG);
void ReplaceScheduledFiles(HREG);

void PerformScheduledTasks(void *data)
{
    HREG   reg;

    if ( REGERR_OK == NR_RegOpen("", &reg) )
    {
        DeleteScheduledFiles( reg );
        ReplaceScheduledFiles( reg );
        NR_RegClose(reg);
    }
}



void DeleteScheduledFiles( HREG reg )
{
    REGERR  err;
    RKEY    key;
    REGENUM state = 0;

    /* perform scheduled file deletions  */
    if (REGERR_OK == NR_RegGetKey(reg,ROOTKEY_PRIVATE,REG_DELETE_LIST_KEY,&key))
    {
        // the delete key exists, so we loop through its children
        // and try to delete all the listed files

        char    namebuf[MAXREGNAMELEN];
        char    valbuf[MAXREGPATHLEN];

        nsFileSpec              doomedFile;
        nsCOMPtr<nsIFileSpec>   spec;

        nsresult rv = NS_NewFileSpec(getter_AddRefs(spec));
        if (NS_SUCCEEDED(rv))
        {
            while (REGERR_OK == NR_RegEnumEntries( reg, key, &state, namebuf,
                                                   sizeof(namebuf), 0 ) )
            {
                uint32 bufsize = sizeof(valbuf); // gets changed, must reset
                err = NR_RegGetEntry( reg, key, namebuf, valbuf, &bufsize );
                if ( err == REGERR_OK )
                {
                    // no need to check return value of 
                    // SetPersistentDescriptorString, it's always NS_OK
                    spec->SetPersistentDescriptorString(valbuf);
                    rv = spec->GetFileSpec(&doomedFile);
                    if (NS_SUCCEEDED(rv)) 
                    {
                        doomedFile.Delete(PR_FALSE);
                        if ( !doomedFile.Exists() )
                        {
                            // deletion successful, don't have to retry
                            NR_RegDeleteEntry( reg, key, namebuf );
                        }
                    }
                }
            }

            /* delete list node if empty */
            state = 0;
            err = NR_RegEnumEntries(reg, key, &state, namebuf, sizeof(namebuf), 0);
            if ( err == REGERR_NOMORE )
            {
                NR_RegDeleteKey(reg, ROOTKEY_PRIVATE, REG_DELETE_LIST_KEY);
            }
        }
    }
}



void ReplaceScheduledFiles( HREG reg )
{
    RKEY    key;

    /* replace files if any listed */
    if (REGERR_OK == NR_RegGetKey(reg,ROOTKEY_PRIVATE,REG_REPLACE_LIST_KEY,&key))
    {
        char keyname[MAXREGNAMELEN];
        char doomedFile[MAXREGPATHLEN];
        char srcFile[MAXREGPATHLEN];

        nsFileSpec              doomedSpec;
        nsFileSpec              srcSpec;
        nsCOMPtr<nsIFileSpec>   src;
        nsCOMPtr<nsIFileSpec>   dest;
        nsresult                rv1, rv2;

        rv1 = NS_NewFileSpec(getter_AddRefs(src));
        rv2 = NS_NewFileSpec(getter_AddRefs(dest));
        if (NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2))
        {
            uint32 bufsize;
            REGENUM state = 0;
            while (REGERR_OK == NR_RegEnumSubkeys( reg, key, &state, 
                                   keyname, sizeof(keyname), REGENUM_CHILDREN))
            {
                bufsize = sizeof(srcFile);
                REGERR err1 = NR_RegGetEntry( reg, (RKEY)state,
                                    REG_REPLACE_SRCFILE, srcFile, &bufsize);

                bufsize = sizeof(doomedFile);
                REGERR err2 = NR_RegGetEntry( reg, (RKEY)state,
                                    REG_REPLACE_DESTFILE, doomedFile, &bufsize);

                if ( err1 == REGERR_OK && err2 == REGERR_OK )
                {
                    src->SetPersistentDescriptorString(srcFile);
                    rv1 = src->GetFileSpec(&srcSpec);

                    dest->SetPersistentDescriptorString(doomedFile);
                    rv2 = dest->GetFileSpec(&doomedSpec);

                    if (NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2))
                    {
                        // finally now try to do the replace
                        PRInt32 result = ReplaceFileNow( srcSpec, doomedSpec );

                        if ( result == nsInstall::DOES_NOT_EXIST ||
                             result == nsInstall::SUCCESS )
                        {
                            // This one is done
                            NR_RegDeleteKey( reg, key, keyname );
                        }
                    }
                }
            }


            /* delete list node if empty */
            state = 0;
            if (REGERR_NOMORE == NR_RegEnumSubkeys( reg, key, &state, keyname,
                                          sizeof(keyname), REGENUM_CHILDREN ))
            {
                NR_RegDeleteKey(reg, ROOTKEY_PRIVATE, REG_REPLACE_LIST_KEY);
            }
        }
    }
}
