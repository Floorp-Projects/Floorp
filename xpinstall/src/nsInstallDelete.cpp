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


#include "prmem.h"

#include "nsFileSpec.h"

#include "VerReg.h"
#include "ScheduledTasks.h"
#include "nsInstallDelete.h"
#include "nsInstallResources.h"

#include "nsInstall.h"
#include "nsIDOMInstallVersion.h"

MOZ_DECL_CTOR_COUNTER(nsInstallDelete)

nsInstallDelete::nsInstallDelete( nsInstall* inInstall,
                                  nsInstallFolder* folderSpec,
                                  const nsString& inPartialPath,
                                  PRInt32 *error)

: nsInstallObject(inInstall)
{
    MOZ_COUNT_CTOR(nsInstallDelete);

    if ((folderSpec == nsnull) || (inInstall == nsnull)) 
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }
    
    mDeleteStatus = DELETE_FILE;
    mFinalFile      = nsnull;
    
    nsCOMPtr<nsIFile> tmp = folderSpec->GetFileSpec();
    if (!tmp)
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }

    tmp->Clone(getter_AddRefs(mFinalFile));;
    if (mFinalFile == nsnull)
    {
        *error = nsInstall::OUT_OF_MEMORY;
        return;
    }
    nsAutoCString tempPartialPath(inPartialPath);

    mFinalFile->Append(tempPartialPath);

    *error = ProcessInstallDelete();
}

nsInstallDelete::nsInstallDelete( nsInstall* inInstall,
                                  const nsString& inComponentName,
                                  PRInt32 *error)

: nsInstallObject(inInstall)
{
    MOZ_COUNT_CTOR(nsInstallDelete);

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

    MOZ_COUNT_DTOR(nsInstallDelete);
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
            Recycle(temp);
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
    char* rsrcVal = nsnull;
    
    if (buffer == nsnull || !mInstall)
        return nsnull;

    if (mDeleteStatus == DELETE_COMPONENT)
    {
        char* temp = mRegistryName.ToNewCString();
        rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("DeleteComponent"));

        if (rsrcVal)
        {
            sprintf( buffer, rsrcVal, temp);
            nsCRT::free(rsrcVal);
        }
        if (temp)
            Recycle(temp);
    }
    else
    {
        if (mFinalFile)
        {
            rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("DeleteComponent"));

            if (rsrcVal)
            {
              char* temp;
              mFinalFile->GetPath(&temp);
              sprintf( buffer, rsrcVal, temp);
                nsCRT::free(rsrcVal);
            }
        }
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
                //if (mFinalFile)
                //    delete mFinalFile;

                nsCOMPtr<nsILocalFile> tempLocalFile;
                NS_NewLocalFile(tempRegistryString, PR_TRUE, getter_AddRefs(tempLocalFile));
                mFinalFile = tempLocalFile;
                
                if (mFinalFile == nsnull)
                    return nsInstall::OUT_OF_MEMORY;
                
            }

            PR_FREEIF(tempRegistryString);
        }
    }
    
    if(tempCString)
        Recycle(tempCString);

    PRBool flagExists, flagIsFile;

    mFinalFile->Exists(&flagExists);
    if (flagExists)
    {
        mFinalFile->IsFile(&flagIsFile);
        if (flagIsFile)
        {
            err = nsInstall::SUCCESS;
        }
        else
        {
            err = nsInstall::IS_DIRECTORY;
        }
    }
    else
    {
        err = nsInstall::DOES_NOT_EXIST;
    }

    return err;
}



PRInt32 nsInstallDelete::NativeComplete()
{
    PRBool flagExists, flagIsFile;

    mFinalFile->Exists(&flagExists);
    NS_WARN_IF_FALSE(flagExists,"nsInstallDelete::Complete -- file should exist!");
    if (flagExists)
    {
        mFinalFile->IsFile(&flagIsFile);
        if (flagIsFile)
        {
           return DeleteFileNowOrSchedule(mFinalFile);
        }
        else
        {
            NS_ASSERTION(0,"nsInstallDelete::Complete -- expected file was a directory!");
            return nsInstall::IS_DIRECTORY;
        }
    }
    
    return nsInstall::DOES_NOT_EXIST;
}

