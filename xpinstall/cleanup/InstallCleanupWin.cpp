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
#include "InstallCleanupDefines.h"
#include <windows.h>
#include <string.h>
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
    bool foundSpecialFile = FALSE;

    int status = DONE;

    if ( REGERR_OK == NR_StartupRegistry())
    {
        DWORD charsWritten;
        char appPath[_MAX_PATH];
        charsWritten = GetModuleFileName(NULL, appPath, _MAX_PATH);
        if (charsWritten > 0)
        {
            char regFilePath[_MAX_PATH];
            
            strcpy(regFilePath, appPath);
            char* lastSlash = strrchr(regFilePath, '\\');
            lastSlash++; 
            *lastSlash = 0;//strip of the executable name
            strcat(regFilePath, CLEANUP_REGISTRY); //append reg file name
    
            if ( GetFileAttributes(regFilePath) == 0xFFFFFFFF ) // file doesn't exist
              strcpy(regFilePath, ""); // an empty reg file tells RegOpen to get the "default"
            else
              foundSpecialFile = TRUE;

            if ( REGERR_OK == NR_RegOpen(regFilePath, &reg) )
            {
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
                              (const unsigned char*)appPath,
                              strlen(appPath));

                do 
                {
                    status = PerformScheduledTasks(reg);
                    if (status != DONE)
                        Sleep(1000); // Sleep for 1 second
                } while (status == TRY_LATER);

                RegDeleteValue(hkRunOnceHandle, cleanupKeyName);
                NR_RegClose(&reg);
            }
            NR_ShutdownRegistry();
            DeleteFile(regFilePath);
        }
    }
    return(0);
}

