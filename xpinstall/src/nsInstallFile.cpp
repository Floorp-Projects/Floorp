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

#include "nsInstallFile.h"
#include "nsFileSpec.h"
#include "VerReg.h"
#include "ScheduledTasks.h"
#include "nsInstall.h"
#include "nsInstallVersion.h"
#include "nsInstallResources.h"

/* Public Methods */

/*	Constructor
        inInstall    - softUpdate object we belong to
        inComponentName	- full path of the registry component
        inVInfo	        - full version info
        inJarLocation   - location inside the JAR file
        inFinalFileSpec	- final	location on disk
*/
nsInstallFile::nsInstallFile(nsInstall* inInstall,
                             const nsString& inComponentName,
                             nsIDOMInstallVersion* inVInfo,
                             const nsString& inJarLocation,
                             const nsString& folderSpec,
                             const nsString& inPartialPath,
                             PRBool forceInstall,
                             PRInt32 *error) 
: nsInstallObject(inInstall)
{
    mExtracedFile= nsnull;
    mFinalFile   = nsnull;
    mUpgradeFile = PR_FALSE;

    if ((folderSpec == "null") || (inInstall == NULL)  || (inVInfo == NULL)) 
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }
    
     mFinalFile = new nsFileSpec(folderSpec);
    *mFinalFile += inPartialPath;
    
    mReplaceFile            = mFinalFile->Exists();

    mForceInstall           = forceInstall;
    
    mVersionRegistryName    = new nsString(inComponentName);
    mJarLocation            = new nsString(inJarLocation);
    mVersionInfo	        = new nsInstallVersion();
    
    nsString tempString;
    inVInfo->ToString(tempString);
    mVersionInfo->Init(tempString);
        
    nsString regPackageName;
    mInstall->GetRegPackageName(regPackageName);
    
    // determine Child status
    if ( regPackageName == "" ) 
    {
        // in the "current communicator package" absolute pathnames (start
        // with slash) indicate shared files -- all others are children
        mChildFile = ( mVersionRegistryName->CharAt(0) != '/' );
    } 
    else 
    {
        //mChildFile = mVersionRegistryName.startsWith(regPackageName);
        /* Because nsString doesn't support startWith, implemented the following. Waiting for approval */
        if (mVersionRegistryName->Find(regPackageName) == 0)
        {
            mChildFile = true;
        }
        else
        {
            mChildFile = false;
        }
    }
    

}


nsInstallFile::~nsInstallFile()
{
    if (mVersionRegistryName)
        delete mVersionRegistryName;
  
    if (mJarLocation)
        delete mJarLocation;
  
    if (mExtracedFile)
        delete mExtracedFile;

    if (mFinalFile)
        delete mFinalFile;
  
    if (mVersionInfo)
      delete mVersionInfo;
}

/* Prepare
 * Extracts file out of the JAR archive
 */
PRInt32 nsInstallFile::Prepare()
{
    if (mInstall == nsnull || mFinalFile == nsnull || mJarLocation == nsnull ) 
        return nsInstall::INVALID_ARGUMENTS;

    return mInstall->ExtractFileFromJar(*mJarLocation, mFinalFile, &mExtracedFile);
}

/* Complete
 * Completes the install:
 * - move the downloaded file to the final location
 * - updates the registry
 */
PRInt32 nsInstallFile::Complete()
{
    PRInt32 err;

    if (mInstall == nsnull || mVersionRegistryName == nsnull || mFinalFile == nsnull ) 
    {
       return nsInstall::INVALID_ARGUMENTS;
    }

    err = CompleteFileMove();
    
    if ( 0 == err || nsInstall::REBOOT_NEEDED == err ) 
    {
        err = RegisterInVersionRegistry();
    }
    
    return err;

}

void nsInstallFile::Abort()
{
    if (mExtracedFile != nsnull)
        mExtracedFile->Delete(PR_FALSE);
}

char* nsInstallFile::toString()
{
    char* buffer = new char[1024];
    
    if (mFinalFile == nsnull)
    {
        sprintf( buffer, nsInstallResources::GetInstallFileString(), nsnull);
    }
    else if (mReplaceFile)
    {
        // we are replacing this file.

        sprintf( buffer, nsInstallResources::GetReplaceFileString(), mFinalFile->GetCString());
    }
    else
    {
        sprintf( buffer, nsInstallResources::GetInstallFileString(), mFinalFile->GetCString());
    }

    return buffer;
}


PRInt32 nsInstallFile::CompleteFileMove()
{
    int result = 0;
    
    if (mExtracedFile == nsnull) 
    {
        return -1;
    }
   	
    if ( *mExtracedFile == *mFinalFile ) 
    {
        /* No need to rename, they are the same */
        result = 0;
    } 
    else 
    {
        result = ReplaceFileLater(*mExtracedFile, *mFinalFile );
    }

  return result;  
}

PRInt32
nsInstallFile::RegisterInVersionRegistry()
{
    int refCount;
    nsString regPackageName;
    mInstall->GetRegPackageName(regPackageName);
    
  
    // Register file and log for Uninstall
  
    if (!mChildFile) 
    {
        int found;
        if (regPackageName != "") 
        {
            found = VR_UninstallFileExistsInList( (char*)(const char*)nsAutoCString(regPackageName) , 
                                                  (char*)(const char*)nsAutoCString(*mVersionRegistryName));
        } 
        else 
        {
            found = VR_UninstallFileExistsInList( "", (char*)(const char*)nsAutoCString(*mVersionRegistryName) );
        }
        
        if (found != REGERR_OK)
            mUpgradeFile = PR_FALSE;
        else
            mUpgradeFile = PR_TRUE;
    } 
    else if (REGERR_OK == VR_InRegistry( (char*)(const char*)nsAutoCString(*mVersionRegistryName))) 
    {
        mUpgradeFile = PR_TRUE;
    } 
    else 
    {
        mUpgradeFile = PR_FALSE;
    }

    if ( REGERR_OK != VR_GetRefCount( (char*)(const char*)nsAutoCString(*mVersionRegistryName), &refCount )) 
    {
        refCount = 0;
    }

    VR_Install( (char*)(const char*)nsAutoCString(*mVersionRegistryName), 
                (char*)(const char*)nsprPath(*mFinalFile),
                (char*)(const char*)nsAutoCString(regPackageName), 
                PR_FALSE );

    if (!mUpgradeFile) 
    {
        if (refCount != 0) 
        {
            VR_SetRefCount( (char*)(const char*)nsAutoCString(*mVersionRegistryName), refCount + 1 );
        } 
        else 
        {
            if (mFinalFile->Exists())
                VR_SetRefCount( (char*)(const char*)nsAutoCString(*mVersionRegistryName), 2 );
            else
                VR_SetRefCount( (char*)(const char*)nsAutoCString(*mVersionRegistryName), 1 );
        }
    } 
    else 
    {
        if (refCount == 0) 
            VR_SetRefCount( (char*)(const char*)nsAutoCString(*mVersionRegistryName), 1 );
        else 
            VR_SetRefCount( (char*)(const char*)nsAutoCString(*mVersionRegistryName), 0 );
    }
    
    if ( !mChildFile && !mUpgradeFile ) 
    {
        if (regPackageName != "") 
        { 
            VR_UninstallAddFileToList( (char*)(const char*)nsAutoCString(regPackageName), 
                                       (char*)(const char*)nsAutoCString(*mVersionRegistryName));
        } 
        else 
        {
            VR_UninstallAddFileToList( "", (char*)(const char*)nsAutoCString(*mVersionRegistryName) );
        }
    }

    return nsInstall::SUCCESS;
}

/* CanUninstall
* InstallFile() installs files which can be uninstalled,
* hence this function returns true. 
*/
PRBool
nsInstallFile::CanUninstall()
{
    return TRUE;
}

/* RegisterPackageNode
* InstallFile() installs files which need to be registered,
* hence this function returns true.
*/
PRBool
nsInstallFile::RegisterPackageNode()
{
    return TRUE;
}
