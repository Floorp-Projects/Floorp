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
#include <windows.h>

//----------------------------------------------------------------------------
// Native Windows file deletion function
//----------------------------------------------------------------------------
int NativeDeleteFile(const char* aFileToDelete)
{
    if (GetFileAttributes(aFileToDelete) == 0xFFFFFFFF)
    {
      return DONE;// No file to delete, do nothing.
    }
    else 
    {
        if(!DeleteFile(aFileToDelete))
          return TRY_LATER;
    }
    return DONE;
}

//----------------------------------------------------------------------------
// Native Windows file replacement function
//----------------------------------------------------------------------------
int NativeReplaceFile(const char* replacementFile, const char* doomedFile )
{
    // replacement file must exist, doomed file doesn't have to
    if (GetFileAttributes(replacementFile) == 0xFFFFFFFF)
        return DONE;

    // don't have to do anything if the files are the same
    if (CompareString(LOCALE_SYSTEM_DEFAULT,
                      NORM_IGNORECASE | SORT_STRINGSORT,
                      replacementFile, -1,
                      doomedFile, -1) == CSTR_EQUAL)
        return DONE;

    if (!DeleteFile(doomedFile))
    {
        if (GetFileAttributes(doomedFile) != 0xFFFFFFFF) // file exists          
            return TRY_LATER;
    }
    
    // doomedFile is gone move new file into place
    if (!MoveFile(replacementFile, doomedFile))
        return TRY_LATER; // this shouldn't happen

    return DONE;
}


int WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR args, int)
{
    HREG  reg;
    HKEY  hkRunOnceHandle;
    DWORD dwDisp;

    int status = DONE;

    if ( REGERR_OK == NR_StartupRegistry())
    {
        if ( REGERR_OK == NR_RegOpen("", &reg) )
        {
            LPTSTR cwd = new char[_MAX_PATH];
            if (cwd)
            {
                GetCurrentDirectory(_MAX_PATH, cwd);
                strcat(cwd, "\\xpicleanup.exe");

                RegCreateKeyEx(HKEY_CURRENT_USER,
                               "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
                               0,
                               NULL,
                               REG_OPTION_NON_VOLATILE,
                               KEY_WRITE,
                               NULL,
                               &hkRunOnceHandle,
                               &dwDisp);
                
                LPCTSTR cleanupKeyName = "mozilla_cleanup";
                
                RegSetValueEx(hkRunOnceHandle,
                              cleanupKeyName,
                              0,
                              REG_SZ,
                              (const unsigned char*)cwd,
                              strlen(cwd));

                do 
                {
                    status = PerformScheduledTasks(reg);
                    if (status != DONE)
                        Sleep(15000); // Sleep for 15 seconds
                } while (status == TRY_LATER);

                RegDeleteValue(hkRunOnceHandle, cleanupKeyName);
                delete[] cwd;
            }
        }
        NR_ShutdownRegistry();
    }
    return(0);
}

