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
    return nsnull;
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
        if (mFinalFile->Exists() == PR_FALSE)
        {
            // We can simple move the extracted file to the mFinalFile's parent
            nsFileSpec parentofFinalFile;

            mFinalFile->GetParent(parentofFinalFile);
            result = mExtracedFile->Move(parentofFinalFile);
            
            char* leafName = mFinalFile->GetLeafName();
            mExtracedFile->Rename(leafName);
            delete [] leafName;

        }
        else
        {
            mFinalFile->Delete(PR_FALSE);

            if (! mFinalFile->Exists())
            {    
                // Now that we have move the existing file, we can move the mExtracedFile into place.
                nsFileSpec parentofFinalFile;

                mFinalFile->GetParent(parentofFinalFile);
                result = mExtracedFile->Move(parentofFinalFile);
            
                char* leafName = mFinalFile->GetLeafName();
                mExtracedFile->Rename(leafName);
                delete [] leafName;
            }
            else
            {
                ReplaceFileLater(mExtracedFile->GetCString(), mFinalFile->GetCString() );
                result = nsInstall::REBOOT_NEEDED;
            }
        }
    }

  return result;  
}

PRInt32
nsInstallFile::RegisterInVersionRegistry()
{
    PRInt32 err;
    int refCount;
    int rc;
    char* tempCString;

    char *final_file  = (char*)mFinalFile->GetCString();  // FIX: mac?  What should we be passing to the version registry: native of unix???
    char *vr_name     = mVersionRegistryName->ToNewCString();
      
    nsString regPackageName;
    mInstall->GetRegPackageName(regPackageName);
    
  
    // Register file and log for Uninstall
  
     // we ignore all registry errors because they're not
    // important enough to abort an otherwise OK install.
    if (!mChildFile) 
    {
        int found;
        if (regPackageName != "") 
        {
            tempCString = regPackageName.ToNewCString();
            found = VR_UninstallFileExistsInList( tempCString , vr_name );
        } 
        else 
        {
            found = VR_UninstallFileExistsInList( "", vr_name );
        }
        
        if (found != REGERR_OK)
            mUpgradeFile = PR_FALSE;
        else
            mUpgradeFile = PR_TRUE;
    } 
    else if (REGERR_OK == VR_InRegistry(vr_name)) 
    {
        mUpgradeFile = PR_TRUE;
    } 
    else 
    {
        mUpgradeFile = PR_FALSE;
    }

    err = VR_GetRefCount( vr_name, &refCount );
    if ( err != REGERR_OK ) 
    {
        refCount = 0;
    }

    if (!mUpgradeFile) 
    {
        if (refCount != 0) 
        {
            rc = 1 + refCount;
            nsString tempString;
            mVersionInfo->ToString(tempString);
            tempCString = regPackageName.ToNewCString();
            VR_Install( vr_name, final_file, tempCString, PR_FALSE );
            VR_SetRefCount( vr_name, rc );
        } 
        else 
        {
            if (mFinalFile->Exists())
            {
                nsString tempString;
                mVersionInfo->ToString(tempString);
                tempCString = regPackageName.ToNewCString();
                VR_Install( vr_name, final_file, tempCString, PR_FALSE);
                VR_SetRefCount( vr_name, 2 );
            }
            else
            {
                nsString tempString;
                mVersionInfo->ToString(tempString);
                tempCString = regPackageName.ToNewCString();
                VR_Install( vr_name, final_file, tempCString, PR_FALSE );
                VR_SetRefCount( vr_name, 1 );
            }
        }
    } 
    else if (mUpgradeFile) 
    {
        if (refCount == 0) 
        {
            nsString tempString;
            mVersionInfo->ToString(tempString);
            tempCString = regPackageName.ToNewCString();
            VR_Install( vr_name, final_file, tempCString, PR_FALSE );
            VR_SetRefCount( vr_name, 1 );
        } 
        else 
        {
            nsString tempString;
            mVersionInfo->ToString(tempString);
            tempCString = regPackageName.ToNewCString();
            VR_Install( vr_name, final_file, tempCString, PR_FALSE );
            VR_SetRefCount( vr_name, 0 );
        }
    }
    
    if ( !mChildFile && !mUpgradeFile ) 
    {
        if (regPackageName != "") 
        {
            if (tempCString == nsnull)
                tempCString = regPackageName.ToNewCString();
            
            VR_UninstallAddFileToList( tempCString, vr_name );
        } 
        else 
        {
            VR_UninstallAddFileToList( "", vr_name );
        }
    }

    if (vr_name != nsnull)
        delete vr_name;
        
    if (tempCString != nsnull)
        delete [] tempCString;
    
    if ( err != 0 ) 
        return nsInstall::UNEXPECTED_ERROR;
    
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
