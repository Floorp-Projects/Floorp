
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Don Bragg <dbragg@netscape.com>
 *   IBM Corp.
 */

#include "InstallCleanup.h"
#include "InstallCleanupDefines.h"
#define INCL_DOSERRORS
#define INCL_DOS
#include <os2.h>
#include <string.h>
#include <sys/stat.h>
//----------------------------------------------------------------------------
// Native OS/2 file deletion function
//----------------------------------------------------------------------------
int NativeDeleteFile(const char* aFileToDelete)
{
    struct stat st;
    if (stat(aFileToDelete, &st) != 0)
    {
      return DONE;// No file to delete, do nothing.
    }
    else 
    {
        if(DosDelete(aFileToDelete) != NO_ERROR)
          return TRY_LATER;
    }
    return DONE;
}

//----------------------------------------------------------------------------
// Native OS/2 file replacement function
//----------------------------------------------------------------------------
int NativeReplaceFile(const char* replacementFile, const char* doomedFile )
{
    // replacement file must exist, doomed file doesn't have to
    struct stat st;
    if (stat(replacementFile, &st) != 0)
        return DONE;

    // don't have to do anything if the files are the same
    if (stricmp(replacementFile, doomedFile) == 0)
        return DONE;

    if (DosDelete(doomedFile) != NO_ERROR)
    {
        if (stat(doomedFile, &st) == 0)
            return TRY_LATER;
    }
    
    // doomedFile is gone move new file into place
    if (DosMove(replacementFile, doomedFile) != NO_ERROR)
        return TRY_LATER; // this shouldn't happen

    return DONE;
}

int main(int argc, char *argv[], char *envp[])
{
    HREG  reg;
    BOOL foundSpecialFile = FALSE;
    struct stat st;

    int status = DONE;

    if ( REGERR_OK == NR_StartupRegistry())
    {
        char regFilePath[CCHMAXPATH];

        strcpy(regFilePath, argv[0]);
        char* lastSlash = strrchr(regFilePath, '\\');
        lastSlash++; 
        *lastSlash = 0;//strip of the executable name
        strcat(regFilePath, CLEANUP_REGISTRY); //append reg file name
    
        if (stat(regFilePath, &st) != 0)
          strcpy(regFilePath, ""); // an empty reg file tells RegOpen to get the "default"
        else
          foundSpecialFile = TRUE;

        if ( REGERR_OK == NR_RegOpen(regFilePath, &reg) )
        {
            int numAttempts = 0;
            do 
            {
                status = PerformScheduledTasks(reg);
                if (status != DONE) {
                    DosSleep(1000); // Sleep for 1 second
                    numAttempts++;
                }
            } while ((status == TRY_LATER) && numAttempts <= 5);

            NR_RegClose(&reg);
        }
        NR_ShutdownRegistry();
        if (status == DONE) {
            DosDelete(regFilePath);
        }
    }
    return(0);
}

