/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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


#include "prmem.h"

#include "nsFileSpec.h"

#include "VerReg.h"
#include "nsInstallDelete.h"

#include "nsInstall.h"
#include "nsIDOMInstallFolder.h"
#include "nsIDOMInstallVersion.h"

#include "nsInstallErrorMessages.h"


nsInstallDelete::nsInstallDelete( nsInstall* inInstall,
                                  nsIDOMInstallFolder*  folderSpec,
                                  const nsString& inPartialPath,
                                  PRInt32 *error)

: nsInstallObject(inInstall)
{
    if ((folderSpec == NULL) || (inInstall == NULL)) 
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }
    
    mDeleteStatus = DELETE_FILE;
    mFinalFile      = "";
    mRegistryName   = "";
    folderSpec->MakeFullPath(inPartialPath, mFinalFile);
 
    *error = ProcessInstallDelete();
}

nsInstallDelete::nsInstallDelete( nsInstall* inInstall,
                                  const nsString& inComponentName,
                                  PRInt32 *error)

: nsInstallObject(inInstall)
{
    if (inInstall == NULL) 
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }
    
    mDeleteStatus = DELETE_COMPONENT;
    mFinalFile    = "";
    mRegistryName = inComponentName;

    *error = ProcessInstallDelete();
}


nsInstallDelete::~nsInstallDelete()
{
}


PRInt32 nsInstallDelete::Prepare()
{
    // no set-up necessary
    return nsInstall::SUCCESS;
}

PRInt32 nsInstallDelete::Complete()
{
    PRInt32 err = nsInstall::SUCCESS;

    if (mInstall == NULL) 
       return nsInstall::INVALID_ARGUMENTS;

    if (mDeleteStatus == DELETE_COMPONENT) 
    {
        char* tempString = mRegistryName.ToNewCString();
        err = VR_Remove( tempString );
        delete tempString;
    }

    if ((mDeleteStatus == DELETE_FILE) || (err == REGERR_OK)) 
    {
        err = NativeComplete();
    }	
    else 
    {
        err = nsInstall::UNEXPECTED_ERROR;
    }

    
    return err;
}

void nsInstallDelete::Abort()
{
}

char* nsInstallDelete::toString()
{
    if (mDeleteStatus == DELETE_FILE) 
    {
        // FIX!
       // return nsInstallErrorMessages::GetString(nsInstall::DETAILS_DELETE_FILE, mFinalFile);
    } 
    else 
    {
        // return nsInstallErrorMessages::GetString(nsInstall::DETAILS_DELETE_COMPONENT, mRegistryName);
    }

    return nsnull;
}


PRBool
nsInstallDelete::CanUninstall()
{
    return PR_FALSE;
}

PRBool
nsInstallDelete::RegisterPackageNode()
{
    return PR_FALSE;
}


PRInt32 nsInstallDelete::ProcessInstallDelete()
{
    PRInt32 err;
    char* tempString;

    if (mDeleteStatus == DELETE_COMPONENT) 
    {
        /* Check if the component is in the registry */
        tempString = mRegistryName.ToNewCString();
        err = VR_InRegistry(tempString);
        delete tempString;
        
        if (err != REGERR_OK) 
        {
            return err;
        } 
        else 
        {
            tempString = (char*)PR_Calloc(MAXREGPATHLEN, sizeof(char));
            char* regTempString = mRegistryName.ToNewCString();

            err = VR_GetPath( regTempString , MAXREGPATHLEN, tempString);
            
            delete regTempString;

            if (err == REGERR_OK)
            {
                mFinalFile.SetString(tempString);
            }

            PR_FREEIF(tempString);
        }
    }

    tempString = mFinalFile.ToNewCString();
    
    nsFileSpec file(tempString);
    delete tempString;

    if (file.Exists())
    {
        if (file.IsFile())
        {
            err = nsInstall::SUCCESS;
        }
        else
        {
            err = nsInstall::FILE_IS_DIRECTORY;
        }
    }
    else
    {
        err = nsInstall::FILE_DOES_NOT_EXIST;
    }

    return err;
}



PRInt32 nsInstallDelete::NativeComplete()
{
    char * tempFile = mFinalFile.ToNewCString();
    nsFileSpec file(tempFile);
    delete tempFile;

    if (file.Exists())
    {
        if (file.IsFile())
        {
           file.Delete(false);

           if (file.Exists())
           {
                // If file still exists, we need to delete it later!
                // FIX DeleteOldFileLater( (char*)finalFile );
                return nsInstall::REBOOT_NEEDED;
           }
        }
        else
        {
            return nsInstall::FILE_IS_DIRECTORY;
        }
    }
    else
    {
        return nsInstall::FILE_DOES_NOT_EXIST;
    }
}

