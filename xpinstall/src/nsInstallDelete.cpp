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


#include "prmem.h"

#include "nsFileSpec.h"

#include "VerReg.h"
#include "ScheduledTasks.h"
#include "nsInstallDelete.h"
#include "nsInstallResources.h"

#include "nsInstall.h"
#include "nsIDOMInstallVersion.h"

nsInstallDelete::nsInstallDelete( nsInstall* inInstall,
                                  const nsString& folderSpec,
                                  const nsString& inPartialPath,
                                  PRInt32 *error)

: nsInstallObject(inInstall)
{
    if ((folderSpec == "null") || (inInstall == NULL)) 
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }
    
    mDeleteStatus = DELETE_FILE;
    mFinalFile      = nsnull;
    mRegistryName   = "";
    

    mFinalFile = new nsFileSpec(folderSpec);

    if (mFinalFile == nsnull)
    {
        *error = nsInstall::OUT_OF_MEMORY;
        return;
    }

    *mFinalFile += inPartialPath;

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
    mFinalFile    = nsnull;
    mRegistryName = inComponentName;

    *error = ProcessInstallDelete();
}


nsInstallDelete::~nsInstallDelete()
{
    if (mFinalFile == nsnull)
        delete mFinalFile;
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
        char* temp = mRegistryName.ToNewCString();
        if (temp)
        {
            err = VR_Remove(temp);
            delete [] temp;
        }
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
    char* buffer = new char[1024];
    
    if (buffer == nsnull)
        return nsnull;

    if (mDeleteStatus == DELETE_COMPONENT)
    {
        char* temp = mRegistryName.ToNewCString();
        sprintf( buffer, nsInstallResources::GetDeleteComponentString(), temp);
        if (temp)
            delete [] temp;
    }
    else
    {
        if (mFinalFile)
            sprintf( buffer, nsInstallResources::GetDeleteFileString(), mFinalFile->GetCString());
    }

    return buffer;
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
    
    char* tempCString = nsnull;

    if (mDeleteStatus == DELETE_COMPONENT) 
    {
        /* Check if the component is in the registry */
        tempCString = mRegistryName.ToNewCString();
        
        if (tempCString == nsnull)
                return nsInstall::OUT_OF_MEMORY;

        err = VR_InRegistry( tempCString );
                
        if (err != REGERR_OK) 
        {
            return err;
        } 
        else 
        {
            char* tempRegistryString;

            tempRegistryString = (char*)PR_Calloc(MAXREGPATHLEN, sizeof(char));
            
            if (tempRegistryString == nsnull)
                return nsInstall::OUT_OF_MEMORY;

            err = VR_GetPath( tempCString , MAXREGPATHLEN, tempRegistryString);
            
            if (err == REGERR_OK)
            {
                if (mFinalFile)
                    delete mFinalFile;

                mFinalFile = new nsFileSpec(tempRegistryString);
                
                if (mFinalFile == nsnull)
                    return nsInstall::OUT_OF_MEMORY;
                
            }

            PR_FREEIF(tempRegistryString);
        }
    }
    
    if(tempCString)
        delete [] tempCString;

    if (mFinalFile->Exists())
    {
        if (mFinalFile->IsFile())
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
    if (mFinalFile->Exists())
    {
        if (mFinalFile->IsFile())
        {
           return DeleteFileNowOrSchedule(*mFinalFile);
        }
        else
        {
            return nsInstall::FILE_IS_DIRECTORY;
        }
    }
    
    return nsInstall::FILE_DOES_NOT_EXIST;
}

