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

#include "nsInstall.h"
#include "nsInstallUninstall.h"
#include "nsInstallResources.h"
#include "VerReg.h"
#include "prmem.h"
#include "nsFileSpec.h"
#include "ScheduledTasks.h"

extern "C" NS_EXPORT PRInt32 SU_Uninstall(char *regPackageName);
REGERR su_UninstallProcessItem(char *component_path);


nsInstallUninstall::nsInstallUninstall( nsInstall* inInstall,
                                        const nsString& regName,
                                        PRInt32 *error)

: nsInstallObject(inInstall)
{
    if (regName.Equals("")) 
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }
    
    mRegName.SetString(regName);

    char* userName = (char*)PR_Malloc(MAXREGPATHLEN);
    PRInt32 err = VR_GetUninstallUserName( (char*) (const char*) nsAutoCString(regName), 
                                           userName, 
                                           MAXREGPATHLEN );
    
    mUIName.SetString(userName);
    
    if (err != REGERR_OK)
    {
        *error = nsInstall::NO_SUCH_COMPONENT;
    }
    
    PR_FREEIF(userName);
    
}

nsInstallUninstall::~nsInstallUninstall()
{
}


PRInt32 nsInstallUninstall::Prepare()
{
    // no set-up necessary
    return nsInstall::SUCCESS;
}

PRInt32 nsInstallUninstall::Complete()
{
    PRInt32 err = nsInstall::SUCCESS;

    if (mInstall == NULL) 
       return nsInstall::INVALID_ARGUMENTS;

    err = SU_Uninstall( (char*)(const char*) nsAutoCString(mRegName) );
    
    return err;
}

void nsInstallUninstall::Abort()
{
}

char* nsInstallUninstall::toString()
{
    char* buffer = new char[1024];
    
    if (buffer == nsnull)
        return buffer;
    
    char* temp = mUIName.ToNewCString();
    
    sprintf( buffer, nsInstallResources::GetUninstallString(), temp);
    
    if (temp)
        delete [] temp;

    return buffer;
}


PRBool
nsInstallUninstall::CanUninstall()
{
    return PR_FALSE;
}

PRBool
nsInstallUninstall::RegisterPackageNode()
{
    return PR_FALSE;
}

extern "C" NS_EXPORT PRInt32 SU_Uninstall(char *regPackageName)
{
    REGERR status = REGERR_FAIL;
    char pathbuf[MAXREGPATHLEN+1] = {0};
    char sharedfilebuf[MAXREGPATHLEN+1] = {0};
    REGENUM state = 0;
    int32 length;
    int32 err;

    if (regPackageName == NULL)
        return REGERR_PARAM;

    if (pathbuf == NULL)
        return REGERR_PARAM;

    /* Get next path from Registry */
    status = VR_Enum( regPackageName, &state, pathbuf, MAXREGPATHLEN );

    /* if we got a good path */
    while (status == REGERR_OK)
    {
        char component_path[2*MAXREGPATHLEN+1] = {0};
        strcat(component_path, regPackageName);
        length = strlen(regPackageName);
        if (component_path[length - 1] != '/')
            strcat(component_path, "/");
        strcat(component_path, pathbuf);
        err = su_UninstallProcessItem(component_path);
        status = VR_Enum( regPackageName, &state, pathbuf, MAXREGPATHLEN );  
    }
    
    err = VR_Remove(regPackageName);
    // there is a problem here.  It looks like if the file is refcounted, we still blow away the reg key
    // FIX!
    state = 0;
    status = VR_UninstallEnumSharedFiles( regPackageName, &state, sharedfilebuf, MAXREGPATHLEN );
    while (status == REGERR_OK)
    {
        err = su_UninstallProcessItem(sharedfilebuf);
        err = VR_UninstallDeleteFileFromList(regPackageName, sharedfilebuf);
        status = VR_UninstallEnumSharedFiles( regPackageName, &state, sharedfilebuf, MAXREGPATHLEN );
    }

    err = VR_UninstallDeleteSharedFilesKey(regPackageName);
    err = VR_UninstallDestroy(regPackageName);
    return err;
}

REGERR su_UninstallProcessItem(char *component_path)
{
    int refcount;
    int err;
    char filepath[MAXREGPATHLEN];

    err = VR_GetPath(component_path, sizeof(filepath), filepath);
    if ( err == REGERR_OK )
    {
        err = VR_GetRefCount(component_path, &refcount);  
        if ( err == REGERR_OK )
        {
            --refcount;
            if (refcount > 0)
                err = VR_SetRefCount(component_path, refcount);  
            else 
            {
                err = VR_Remove(component_path);
                DeleteFileNowOrSchedule(nsFileSpec(filepath));
            }
        }
        else
        {
            /* delete node and file */
            err = VR_Remove(component_path);
            DeleteFileNowOrSchedule(nsFileSpec(filepath));
        }
    }
    return err;
}
