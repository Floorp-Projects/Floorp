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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
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

#ifdef _WINDOWS
#include <sys/stat.h>
#include <windows.h>

BOOL WIN32_IsMoveFileExBroken()
{
    /* the NT option MOVEFILE_DELAY_UNTIL_REBOOT is broken on 
     * Windows NT 3.51 Service Pack 4 and NT 4.0 before Service Pack 2
     */
    BOOL broken = FALSE;
    OSVERSIONINFO osinfo;

    // they *all* appear broken--better to have one way that works.
    return TRUE;

    osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (GetVersionEx(&osinfo) && osinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
        if ( osinfo.dwMajorVersion == 3 && osinfo.dwMinorVersion == 51 )
        {
            if ( 0 == stricmp(osinfo.szCSDVersion,"Service Pack 4"))
            {
                broken = TRUE;
            }
        }
        else if ( osinfo.dwMajorVersion == 4 )
        {
            if (osinfo.szCSDVersion[0] == '\0' || 
                (0 == stricmp(osinfo.szCSDVersion,"Service Pack 1")))
            {
                broken = TRUE;
            }
        }
    }

    return broken;
}


PRInt32 DoWindowsReplaceExistingFileStuff(const char* currentName, const char* finalName)
{
    PRInt32 err = 0;

    char* final   = strdup(finalName);
    char* current = strdup(currentName);

    /* couldn't delete, probably in use. Schedule for later */
     DWORD	dwVersion, dwWindowsMajorVersion;

    /* Get OS version info */
    dwVersion = GetVersion();
    dwWindowsMajorVersion =  (DWORD)(LOBYTE(LOWORD(dwVersion)));

    /* Get build numbers for Windows NT or Win32s */

    if (dwVersion < 0x80000000)                // Windows NT
    {
    	/* On Windows NT */
        if ( WIN32_IsMoveFileExBroken() )
        {
            /* the MOVEFILE_DELAY_UNTIL_REBOOT option doesn't work on
             * NT 3.51 SP4 or on NT 4.0 until SP2
             */
            struct stat statbuf;
            PRBool nameFound = PR_FALSE;
            char tmpname[_MAX_PATH];
            
            strncpy( tmpname, finalName, _MAX_PATH );
            int len = strlen(tmpname);
            while (!nameFound && len < _MAX_PATH ) 
            {
                    tmpname[len-1] = '~';
                    tmpname[len] = '\0';
                    if ( stat(tmpname, &statbuf) != 0 )
                        nameFound = TRUE;
                    else
                        len++;
            }

            if ( nameFound ) 
            {
                if ( MoveFile( finalName, tmpname ) ) 
                {
                    if ( MoveFile( currentName, finalName ) ) 
                    {
                        DeleteFileNowOrSchedule(nsFileSpec(tmpname));
                    }
                    else 
                    {
                        /* 2nd move failed, put old file back */
                        MoveFile( tmpname, finalName );
                    }
                }
                else
                {
                    /* non-executable in use; schedule for later */
                    return -1;  // let the start registry stuff do our work!
                }
            }
        }
        else if ( MoveFileEx(currentName, finalName, MOVEFILE_DELAY_UNTIL_REBOOT) )
        {
		    err = 0;
        }
    }
	else // Windows 95 or Win16
	{
        /*
 		 * Place an entry in the WININIT.INI file in the Windows directory
		 * to delete finalName and rename currentName to be finalName at reboot
 		 */

        int strlen;
 		char	Src[_MAX_PATH];   // 8.3 name
 		char	Dest[_MAX_PATH];  // 8.3 name

        strlen = GetShortPathName( (LPCTSTR)currentName, (LPTSTR)Src, (DWORD)sizeof(Src) );
        if ( strlen > 0 ) 
        {
            free(current);
            current   = strdup(Src);
        }

		strlen = GetShortPathName( (LPCTSTR) finalName, (LPTSTR) Dest, (DWORD) sizeof(Dest));
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
    }
    
    free(final);
    free(current);

    return err;
}
#endif





REGERR DeleteFileNowOrSchedule(const nsFileSpec& filename)
{

    REGERR result = 0;

    filename.Delete(PR_FALSE);
    
    if (filename.Exists())
    {
        RKEY newkey;
        HREG reg;
        if ( REGERR_OK == NR_RegOpen("", &reg) ) 
        {
            if (REGERR_OK == NR_RegAddKey( reg, ROOTKEY_PRIVATE, REG_DELETE_LIST_KEY, &newkey) )
            {
                // FIX should be using nsPersistentFileDescriptor!!!

                const char *fnamestr = filename.GetNativePathCString();
                result = NR_RegSetEntry( reg, newkey, (char*)fnamestr, REGTYPE_ENTRY_FILE, (void*)fnamestr, strlen(fnamestr));
                if (result == REGERR_OK)
                    result = nsInstall::REBOOT_NEEDED;
            }

            NR_RegClose(reg);
        }
    }

    return result;
}
/* tmp file is the bad one that we want to replace with target. */

REGERR ReplaceFileNowOrSchedule(nsFileSpec& replacementFile, nsFileSpec& doomedFile )
{
    REGERR result = 0;
    
    if(replacementFile == doomedFile)
    {
        /* do not have to do anything */
        return result;
    }

    doomedFile.Delete(PR_FALSE);

    if ( !doomedFile.Exists() )
    {
        // Now that we have removed the existing file, we can move the mExtracedFile or mPatchedFile into place.
        nsFileSpec parentofFinalFile;
        nsFileSpec parentofReplacementFile;

        doomedFile.GetParent(parentofFinalFile);
        replacementFile.GetParent(parentofReplacementFile);
        if(parentofReplacementFile != parentofFinalFile)
            result = replacementFile.Move(parentofFinalFile);
        else
        	result = NS_OK;
        	
        if ( NS_SUCCEEDED(result) )
        {
            char* leafName = doomedFile.GetLeafName();
            replacementFile.Rename(leafName);
            nsCRT::free(leafName);
        }
    }
    else
    {
#ifdef _WINDOWS
        if (DoWindowsReplaceExistingFileStuff(replacementFile.GetNativePathCString(), doomedFile.GetNativePathCString()) == 0)
            return 0;
#endif

        RKEY newkey;
        HREG reg;

        if ( REGERR_OK == NR_RegOpen("", &reg) ) 
        {
            result = NR_RegAddKey( reg, ROOTKEY_PRIVATE, REG_REPLACE_LIST_KEY, &newkey);
            if ( result == REGERR_OK ) 
            {
                char* replacementFileName = (char*)(const char*)replacementFile.GetNativePathCString();
                result = NR_RegSetEntry( reg, newkey, (char*)(const char*)doomedFile.GetNativePathCString(), REGTYPE_ENTRY_FILE, replacementFileName, strlen(replacementFileName));
                if (result == REGERR_OK)
                    result = nsInstall::REBOOT_NEEDED;
            }

            NR_RegClose(reg);
        }
    }

    return result;
}

void DeleteScheduledFiles(void);
void ReplaceScheduledFiles(void);

extern "C" void PerformScheduledTasks(void *data)
{
    DeleteScheduledFiles();
    ReplaceScheduledFiles();
}


void DeleteScheduledFiles(void)
{
    HREG reg;

    if (REGERR_OK == NR_RegOpen("", &reg))
    {
        RKEY    key;
	      REGENUM state = 0;

        /* perform scheduled file deletions and replacements (PC only) */
        if (REGERR_OK ==  NR_RegGetKey(reg, ROOTKEY_PRIVATE, REG_DELETE_LIST_KEY,&key))
        {
            char buf[MAXREGNAMELEN];

            while (REGERR_OK == NR_RegEnumEntries(reg, key, &state, buf, sizeof(buf), NULL ))
            {
                nsFileSpec doomedFile(buf);

                doomedFile.Delete(PR_FALSE);
                
                if (! doomedFile.Exists()) 
                {
                    NR_RegDeleteEntry( reg, key, buf );
                }
            }

            /* delete list node if empty */
			      if (REGERR_NOMORE == NR_RegEnumEntries( reg, key, &state, buf, sizeof(buf), NULL ))
            {
                NR_RegDeleteKey(reg, ROOTKEY_PRIVATE, REG_DELETE_LIST_KEY);
            }
        }
        
        NR_RegClose(reg);
    }
}

void ReplaceScheduledFiles(void)
{
    HREG reg;
     
    if (REGERR_OK == NR_RegOpen("", &reg))
    {
        RKEY    key;
	    REGENUM state;

        /* replace files if any listed */
        if (REGERR_OK ==  NR_RegGetKey(reg, ROOTKEY_PRIVATE, REG_REPLACE_LIST_KEY, &key))
        {
            char dummyFile[MAXREGNAMELEN];
            char target[MAXREGNAMELEN];

            state = 0;
            while (REGERR_OK == NR_RegEnumEntries(reg, key, &state, dummyFile, sizeof(dummyFile), NULL ))
            {

                nsFileSpec replaceFile(dummyFile);

                if (! replaceFile.Exists() )
                {
                    NR_RegDeleteEntry( reg, key, dummyFile );
                }
                else if ( REGERR_OK != NR_RegGetEntryString( reg, key, dummyFile, target, sizeof(target) ) )
                {
                    /* can't read target filename, corruption? */
                    NR_RegDeleteEntry( reg, key, dummyFile );
                }
                else 
                {
                    nsFileSpec targetFile(target);
                
                    targetFile.Delete(PR_FALSE);
                
                    if (!targetFile.Exists())
                    {
                        nsFileSpec parentofTarget;
                        targetFile.GetParent(parentofTarget);                                           
                    
                        nsresult result = replaceFile.Move(parentofTarget);
                        if ( NS_SUCCEEDED(result) )
                        {
                            char* leafName = targetFile.GetLeafName();
                            replaceFile.Rename(leafName);
                            nsCRT::free(leafName);
                            
                            NR_RegDeleteEntry( reg, key, dummyFile );
                        }
                    }
                }
            }
            /* delete list node if empty */
            if (REGERR_NOMORE == NR_RegEnumEntries(reg, key, &state, dummyFile, sizeof(dummyFile), NULL )) 
            {
                NR_RegDeleteKey(reg, ROOTKEY_PRIVATE, REG_REPLACE_LIST_KEY);
            }
        }
        
        NR_RegClose(reg);
    }
}
