/* -*- Mode: C++; tab-width: 4; tabs-indent-mode: nil -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* su_win.c
 * Win specific softupdate routines
 */

#ifdef XP_OS2
  #include os2updt.h"
#endif  

#include <stdafx.h>
#include "xp_mcom.h"
#include "su_folderspec.h"
#include "su_instl.h"
#include "xp_str.h"
#include "npapi.h"
#include "libevent.h"
#include "fe_proto.h"
#include "xpgetstr.h"
#include <windows.h>
#include "NSReg.h"

#ifdef WIN32
#include <winver.h>
#elif !defined (XP_OS2)
#include <ver.h>
#endif

extern NPError wfe_GetPluginsDirectory(CString& csDirname);
extern "C" char * FE_GetProgramDirectory(char *buffer, int length);

extern "C" int SU_NEED_TO_REBOOT;

extern "C" REGERR  fe_DeleteOldFileLater(char * filename);
extern "C" REGERR  fe_ReplaceOldFileLater(char *tmpfile, char *target);

XP_Bool fe_FileNeedsUpdate(char *sourceFile, char *targetFile);

#if defined(WIN32) || defined(XP_OS2)
#define su_FileRemove(file,type) XP_FileRemove((file),(type))
#else
int su_FileRemove(char *fname, XP_FileType type);
#endif



char * FE_GetDirectoryPath( su_DirSpecID folderID)
{
	char * directory = NULL;
    char path[_MAX_PATH];
    DWORD dwVersion;
    UINT len;

	switch( folderID)
	{
	case ePluginFolder:
		{
			NPError err;
			CString c;
			err = wfe_GetPluginsDirectory(c);
			if (err == 0) {
    			c += (TCHAR)'\\';
				directory = XP_STRDUP( c );
            }
		}
		break;

	case eProgramFolder:
        FE_GetProgramDirectory( path, _MAX_PATH );
        directory = XP_STRDUP( path );
        break;

	case ePackageFolder:
        break;

    case eTemporaryFolder:
    {
        char* tmpdir = XP_TempDirName();
        XP_STRCPY( path, tmpdir );
        XP_STRCAT( path, "\\" );
        XP_FREEIF(tmpdir);

        directory = XP_STRDUP( path );
    }
        break;

	case eCommunicatorFolder:
	{
		char* p;

        	FE_GetProgramDirectory( path, _MAX_PATH );
		
		if ( (p = XP_STRRCHR( path, '\\' )) ) *p =  '\0';
		if ( (p = XP_STRRCHR( path, '\\' )) ) p[1] =  '\0';

        	directory = XP_STRDUP( path );
	}
        break;


    case eWin_System16Folder:
        len = GetSystemDirectory( path, _MAX_PATH );
    	// If Windows NT
#ifndef XP_OS2
    	dwVersion = GetVersion();
        if ( dwVersion < 0x80000000 ) {
            // and the last two chars of the system dir are "32"
            if ( XP_STRCMP( path+len-2, "32" ) == 0 ) {
                // 16-bit system dir is just "System"
                len = len - 2;
            }
        }
#endif
        XP_STRCPY( path+len, "\\" );
        directory = XP_STRDUP( path );
        break;

	case eWin_SystemFolder:
        len = GetSystemDirectory( path, _MAX_PATH );
        XP_STRCPY( path+len, "\\" );
        directory = XP_STRDUP( path );
        break;

	case eWin_WindowsFolder:
        len = GetWindowsDirectory( path, _MAX_PATH );
        XP_STRCPY( path+len, "\\" );
        directory = XP_STRDUP( path );
        break;

	case eJavaBinFolder:
        FE_GetProgramDirectory( path, _MAX_PATH );
        directory = PR_smprintf("%sjava\\bin\\", path);
        break;

	case eJavaClassesFolder:
        FE_GetProgramDirectory( path, _MAX_PATH );
        directory = PR_smprintf("%sjava\\classes\\", path);
        break;

	case eJavaDownloadFolder:
        FE_GetProgramDirectory( path, _MAX_PATH );
        directory = PR_smprintf("%sjava\\download\\", path);
        break;

    case eNetHelpFolder:
        {
            char* tmpdir = FE_GetNetHelpDir();
            if ('/' != *tmpdir ) {
                /* we got a valid file url from FE_GetNetHelpDir() */
                directory = WH_FileName(tmpdir+7, xpURL);
            }
            else {
                /* windows FE couldn't find registry setting */
                FE_GetProgramDirectory( path, _MAX_PATH );
                XP_STRCAT( path, "NetHelp\\" );
                directory = XP_STRDUP( path );
            }
            XP_FREEIF(tmpdir);
        }
        break;
       
    case eOSDriveFolder:
        len = GetWindowsDirectory( path, _MAX_PATH );
        if (len)
        {
            if ( path[1] == ':' && path[2] == '\\' )
            {
                path[3] = 0;
                directory = XP_STRDUP( path );
            }
        }
        break;
      


    // inapplicable platform specific directories
    case eMac_SystemFolder:
	case eMac_DesktopFolder:
	case eMac_TrashFolder:
    case eMac_StartupFolder:
	case eMac_ShutdownFolder:
	case eMac_AppleMenuFolder:
	case eMac_ControlPanelFolder:
	case eMac_ExtensionFolder:
	case eMac_FontsFolder:
	case eMac_PreferencesFolder:
	case eUnix_LocalFolder:
	case eUnix_LibFolder:
        break;

    case eFileURLFolder: // should never be handled in this routine
    default:
        // someone added a new one and didn't take care of it
        XP_ASSERT(0);
		break;
	}
	return directory;
}

int FE_ExecuteFile( const char * filename, const char * cmdline )
{
    if ( cmdline == NULL || filename == NULL )
        return -1;

    DWORD hInst = WinExec( cmdline, SW_NORMAL );
    fe_DeleteOldFileLater( (char*)filename );

    if ( hInst < 32 )
        return -1;
    else
        return 0;
}



/* Template for a delayed "rename" of a file on windows platform, need to:
     - Convert filenames to native form
     - Add error checking
 */
#ifndef NO_ERROR
#define NO_ERROR 0
#endif

int    FE_ReplaceExistingFile(char *CurrentFname, XP_FileType ctype,
		char *FinalFname, XP_FileType ftype, XP_Bool force)
{
    int     err;
    char *  currentName;
    char *  finalName;

    /* Convert file names to their native format */
    currentName = WH_FileName (CurrentFname, ctype);
    finalName   = WH_FileName (FinalFname, ftype);

    if ( currentName == NULL || finalName == NULL ) {
        err = -1;
        goto cleanup;
    }

    // replace file if forced or if windows version info is newer
    if (force || fe_FileNeedsUpdate( currentName, finalName ) )
    {
        /* try to delete the existing file */
        err = su_FileRemove( FinalFname, ftype );
        if ( NO_ERROR == err )
        {
            err = XP_FileRename(CurrentFname, ctype, FinalFname, ftype);
        }
        else
        {
            /* couldn't delete, probably in use. Schedule for later */
    	    DWORD	dwVersion, dwWindowsMajorVersion;

            char* final = finalName;
            char* current = currentName;

#if !defined(XP_OS2)
#if !defined(XP_WIN16)
    	    /* Get OS version info */
    	    dwVersion = GetVersion();
    	    dwWindowsMajorVersion =  (DWORD)(LOBYTE(LOWORD(dwVersion)));

	        /* Get build numbers for Windows NT or Win32s */

        	if (dwVersion < 0x80000000)                // Windows NT
    	    {
    		    /* On Windows NT */

                if ( WFE_IsMoveFileExBroken() )
                {
                    /* the MOVEFILE_DELAY_UNTIL_REBOOT option doesn't work on
                     * NT 3.51 SP4 or on NT 4.0 until SP2
                     */
                    struct stat statbuf;
                    BOOL nameFound = FALSE;
                    char tmpname[_MAX_PATH];

                    strcpy( tmpname, finalName );
                    int len = strlen(tmpname);
                    while (!nameFound && len < _MAX_PATH ) {
                        tmpname[len-1] = '~';
                        tmpname[len] = '\0';
                        if ( stat(tmpname, &statbuf) != 0 )
                            nameFound = TRUE;
                        else
                            len++;
                    }
                    if ( nameFound ) {
                        if ( MoveFile( finalName, tmpname ) ) {
                            if ( MoveFile( currentName, finalName ) ) {
                                err = fe_DeleteOldFileLater( tmpname );
                            }
                            else {
                                /* 2nd move failed, put old file back */
                                MoveFile( tmpname, finalName );
                            }
                        }
                        else {
                            /* non-executable in use; schedule for later */
                            err = fe_ReplaceOldFileLater( CurrentFname, FinalFname );
                        }
                    }
                }
                else if ( MoveFileEx(currentName, finalName, MOVEFILE_DELAY_UNTIL_REBOOT) )
                {
		            err = NO_ERROR;
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
                if ( strlen > 0 ) {
                    current = Src;
                }

		        strlen = GetShortPathName( (LPCTSTR) finalName, (LPTSTR) Dest, (DWORD) sizeof(Dest));
                if ( strlen > 0 ) {
                    final = Dest;
                }

#endif /* !XP_WIN16 */

                /* NOTE: use OEM filenames! Even though it looks like a Windows
                 *       .INI file, WININIT.INI is processed under DOS 
                 */
                AnsiToOem( final, final );
                AnsiToOem( current, current );

                if ( WritePrivateProfileString( "Rename", final, current, "WININIT.INI" ) )
		            err = NO_ERROR;

#if !defined(XP_WIN16)
            }
#endif /* XP_WIN16 */

#else /* XP_OS2 */
            ULONG usRetCode;
            usRetCode = WriteLockFileRenameEntry(final,current,"SOFTUPDT.LST");
            if (usRetCode == NO_ERROR) {
                err = NO_ERROR;
            }
#endif /* XP_OS2 */

            if ( NO_ERROR == err )
                err = REBOOT_NEEDED;
	    }
    } // (force || fe_FileNeedsUpdate( currentName, finalName ))
    else {
        // file to be installed was older than existing file--clean up
        err = XP_FileRemove( CurrentFname, ctype );
    }

cleanup:
    XP_FREEIF(finalName);
    XP_FREEIF(currentName);

	return err;
}


void PR_CALLBACK SU_AlertCallback(void * dummy)
{
	MWContext * cx;
	cx = XP_FindSomeContext();
	FE_Alert(cx, XP_GetString(SU_NEED_TO_REBOOT));
}



XP_Bool fe_FileNeedsUpdate(char *sourceFile, char *targetFile)
{
    XP_Bool needUpdate;

#if defined XP_OS2
    needUpdate = TRUE; //Do not have internal version control so
                       //assume that files should be replaced as
                       //in unix version
#else

    DWORD targetInfoSize;
    DWORD sourceInfoSize;
    DWORD bogusHandle;
    void *sourceData;
    void *targetData;
    VS_FIXEDFILEINFO *sourceInfo;
    VS_FIXEDFILEINFO *targetInfo;
    UINT  targIBSize;
    UINT  srcIBSize;

    // always replace old file if it doesn't have version info
    targetInfoSize = GetFileVersionInfoSize( targetFile, &bogusHandle );
    if (targetInfoSize == 0) 
        return TRUE;

    // if new file doesn't have a version assume it's older than one that does
    sourceInfoSize = GetFileVersionInfoSize( sourceFile, &bogusHandle );
    if (sourceInfoSize == 0) 
        return FALSE;
    
    // both files have version info blocks; we must compare them
    needUpdate = TRUE;
    sourceData = XP_ALLOC(sourceInfoSize);
    targetData = XP_ALLOC(targetInfoSize);

    if ( (sourceData != NULL) && (targetData != NULL) &&
        GetFileVersionInfo(targetFile, NULL, targetInfoSize, targetData) &&
        GetFileVersionInfo(sourceFile, NULL, sourceInfoSize, sourceData) &&
        VerQueryValue(targetData, "\\", (void**)&targetInfo, &targIBSize)  &&
        VerQueryValue(sourceData, "\\", (void**)&sourceInfo, &srcIBSize) )
    {
        DWORD targetMajorVer = targetInfo->dwFileVersionMS;
        DWORD targetMinorVer = targetInfo->dwFileVersionLS;
        DWORD sourceMajorVer = sourceInfo->dwFileVersionMS;
        DWORD sourceMinorVer = sourceInfo->dwFileVersionLS;

        // don't replace a file with a newer version
        if (targetMajorVer > sourceMajorVer) 
        {
            needUpdate = FALSE;
        }
        else if (targetMajorVer == sourceMajorVer && targetMinorVer > sourceMinorVer)
        {
            needUpdate = FALSE;
        }
        else
            needUpdate = TRUE;
    }

    XP_FREEIF(sourceData);
    XP_FREEIF(targetData);
#endif //XP_OS2
    return needUpdate;
}


REGERR fe_DeleteOldFileLater(char * filename)
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



REGERR fe_ReplaceOldFileLater(char *tmpfile, char *target )
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



#if !defined(WIN32) && !defined(XP_OS2)

#define BUFLEN 1024
void FE_ScheduleRenameUtility(void)
{
    int  cmdlength;
    char *folder;
    char * buf = (char*)XP_ALLOC(BUFLEN);
    if ( buf != NULL ) {
        FE_GetProgramDirectory(buf, BUFLEN);
        XP_STRCAT(buf, "NSINIT.EXE,");
        cmdlength = XP_STRLEN(buf);

        if (!GetProfileString("Windows","Run","",(buf+cmdlength),
            (BUFLEN-cmdlength))) 
        {
            // no other RUN commands so don't need trailing comma
            buf[cmdlength-1] = '\0';
        }

        WriteProfileString("Windows","Run",buf);

        XP_FREE(buf);
    }
}


// WIN16 only, Win32 is macro'd to usual XP_FileRemove()
int su_FileRemove(char *fname, XP_FileType type)
{
    HMODULE     hinst;
    char        *pFile;
    int         err = NO_ERROR;

    // convert to native filename and see if it's currently running
    pFile = WH_FileName( fname, type );
    if (pFile != NULL ) {
        // Don't try to delete if Windows says it's in use
        hinst = GetModuleHandle(pFile);
        if ( hinst != NULL ) {
            // GetModuleHandle() accepts the full path but matches only on the
            // root name.  Use GetModuleFileName() to verify a true match
            char module[_MAX_PATH];
            GetModuleFileName( hinst, module, _MAX_PATH );
            if ( stricmp( pFile, module ) == 0 ) {
                // they match
                err = -1;
            }
        }

        XP_FREE(pFile);
    }

    if ( err == NO_ERROR ) {
        // wasn't in use (or couldn't convert to native filename)
        // try to remove the old way
        err = XP_FileRemove( fname, type );
    }

    return err;
}

#endif /* !WIN32 */



#ifdef WIN32
BOOL WFE_IsMoveFileExBroken()
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

#endif /* WIN32 */


