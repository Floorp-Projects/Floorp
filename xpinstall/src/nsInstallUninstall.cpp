/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *   Douglas Turner <dougt@netscape.com>
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

#include "nsInstall.h"
#include "nsInstallUninstall.h"
#include "nsInstallResources.h"
#include "VerReg.h"
#include "nsCRT.h"
#include "prmem.h"
#include "ScheduledTasks.h"
#include "nsReadableUtils.h"
#include "nsILocalFile.h"

extern "C" NS_EXPORT PRInt32 SU_Uninstall(char *regPackageName);
REGERR su_UninstallProcessItem(char *component_path);

nsInstallUninstall::nsInstallUninstall( nsInstall* inInstall,
                                        const nsString& regName,
                                        PRInt32 *error)

: nsInstallObject(inInstall)
{
    MOZ_COUNT_CTOR(nsInstallUninstall);

    if (regName.IsEmpty()) 
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }
    
    mRegName.Assign(regName);

    char* userName = (char*)PR_Malloc(MAXREGPATHLEN);
    PRInt32 err = VR_GetUninstallUserName( NS_CONST_CAST(char*, NS_ConvertUTF16toUTF8(regName).get()),
                                           userName, 
                                           MAXREGPATHLEN );
    
    mUIName.AssignWithConversion(userName);
    
    if (err != REGERR_OK)
    {
        *error = nsInstall::NO_SUCH_COMPONENT;
    }
    
    PR_FREEIF(userName);
    
}

nsInstallUninstall::~nsInstallUninstall()
{
    MOZ_COUNT_DTOR(nsInstallUninstall);
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

    err = SU_Uninstall( NS_CONST_CAST(char*, NS_ConvertUTF16toUTF8(mRegName).get()) );
    
    return err;
}

void nsInstallUninstall::Abort()
{
}

char* nsInstallUninstall::toString()
{
    char* buffer = new char[1024];
    char* rsrcVal = nsnull;

    if (buffer == nsnull || !mInstall)
        return buffer;
    
    char* temp = ToNewCString(mUIName);
    
    if (temp)
    {
        rsrcVal = mInstall->GetResourcedString(NS_LITERAL_STRING("Uninstall"));

        if (rsrcVal)
        {
            sprintf( buffer, rsrcVal, temp);
            nsCRT::free(rsrcVal);
        }
    }

    if (temp)
         NS_Free(temp);

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
    nsCOMPtr<nsILocalFile> nsLFPath;
    nsCOMPtr<nsIFile> nsFPath;

    err = VR_GetPath(component_path, sizeof(filepath), filepath);
    if ( err == REGERR_OK )
    {
        NS_NewNativeLocalFile(nsDependentCString(filepath), PR_TRUE, getter_AddRefs(nsLFPath));
        nsFPath = nsLFPath;
        err = VR_GetRefCount(component_path, &refcount);  
        if ( err == REGERR_OK )
        {
            --refcount;
            if (refcount > 0)
                err = VR_SetRefCount(component_path, refcount);  
            else 
            {
                err = VR_Remove(component_path);
                DeleteFileNowOrSchedule(nsFPath);
            }
        }
        else
        {
            /* delete node and file */
            err = VR_Remove(component_path);
            DeleteFileNowOrSchedule(nsFPath);
        }
    }
    return err;
}

