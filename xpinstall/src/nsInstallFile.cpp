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
#include "nsIDOMInstallVersion.h"
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
                             const nsString& inVInfo,
                             const nsString& inJarLocation,
                             const nsString& folderSpec,
                             const nsString& inPartialPath,
                             PRBool forceInstall,
                             PRInt32 *error) 
: nsInstallObject(inInstall)
{
    mVersionRegistryName = nsnull;
    mJarLocation         = nsnull;
    mExtracedFile        = nsnull;
    mFinalFile           = nsnull;
    mVersionInfo         = nsnull;

    mUpgradeFile = PR_FALSE;
    
    if ((folderSpec == "null") || (inInstall == NULL)) 
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }

    /* Check for existence of the newer	version	*/
    
    char* qualifiedRegNameString = inComponentName.ToNewCString();

    if ( (forceInstall == PR_FALSE ) && (inVInfo !=  "") && ( VR_ValidateComponent( qualifiedRegNameString ) == 0 ) ) 
    {
        nsInstallVersion *newVersion = new nsInstallVersion();
        
        if (newVersion == nsnull)
        {
            delete [] qualifiedRegNameString;
            *error = nsInstall::OUT_OF_MEMORY;
            return;
        }

        newVersion->Init(inVInfo);
        
        VERSION versionStruct;
        
        VR_GetVersion( qualifiedRegNameString, &versionStruct );
        
        nsInstallVersion* oldVersion = new nsInstallVersion();
        
        if (newVersion == nsnull)
        {
            delete [] qualifiedRegNameString;
            delete oldVersion;
            *error = nsInstall::OUT_OF_MEMORY;
            return;
        }

        oldVersion->Init(versionStruct.major,
                         versionStruct.minor,
                         versionStruct.release,
                         versionStruct.build);

        PRInt32 areTheyEqual;
        newVersion->CompareTo(oldVersion, &areTheyEqual);
        
        delete oldVersion;
        delete newVersion;

        if (areTheyEqual == nsIDOMInstallVersion::MAJOR_DIFF_MINUS ||
            areTheyEqual == nsIDOMInstallVersion::MINOR_DIFF_MINUS ||
            areTheyEqual == nsIDOMInstallVersion::REL_DIFF_MINUS   ||
            areTheyEqual == nsIDOMInstallVersion::BLD_DIFF_MINUS   )
        {
            // the file to be installed is OLDER than what is on disk.  Return error
            delete [] qualifiedRegNameString;
            *error = areTheyEqual;
            return;
        }
    }

    delete [] qualifiedRegNameString;

    mFinalFile = new nsFileSpec(folderSpec);
    
    if (mFinalFile == nsnull)
    {
        *error = nsInstall::OUT_OF_MEMORY;
        return;
    }
    
    if ( mFinalFile->Exists() )
    {
        // is there a file with the same name as the proposed folder?
        if ( mFinalFile->IsFile() ) 
        {
            *error = nsInstall::FILENAME_ALREADY_USED;
            return;
        }
        // else this directory already exists, so do nothing
    }
    else
    {
        /* the nsFileSpecMac.cpp operator += requires "this" (the nsFileSpec)
         * to be an existing dir
         */
        int dirPermissions = 0755; // std default for UNIX, ignored otherwise
        mFinalFile->CreateDir(dirPermissions);
    }

    *mFinalFile += inPartialPath;
    
    mReplaceFile = mFinalFile->Exists();
    
    if (mReplaceFile == PR_FALSE)
    {
       /* although it appears that we are creating the dir _again_ it is necessary
        * when inPartialPath has arbitrary levels of nested dirs before the leaf
        */
        nsFileSpec parent;
        mFinalFile->GetParent(parent);
        nsFileSpec makeDirs(parent.GetCString(), PR_TRUE);
    }

    mForceInstall           = forceInstall;
    
    mVersionRegistryName    = new nsString(inComponentName);
    mJarLocation            = new nsString(inJarLocation);
    mVersionInfo	        = new nsString(inVInfo);
     
    if (mVersionRegistryName == nsnull ||
        mJarLocation         == nsnull ||
        mVersionInfo         == nsnull )
    {
        *error = nsInstall::OUT_OF_MEMORY;
        return;
    }

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
        
        // there is no "starts with" api in nsString.  LAME!
        nsString startsWith;
        mVersionRegistryName->Left(startsWith, regPackageName.Length());

        if (startsWith.Equals(regPackageName))
        {
            mChildFile = PR_TRUE;
        }
        else
        {
            mChildFile = PR_FALSE;
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

PRUnichar* nsInstallFile::toString()
{
    PRUnichar* buffer = new PRUnichar[1024];
    
    if (buffer == nsnull)
        return nsnull;

    if (mFinalFile == nsnull)
    {
        sprintf( (char *)buffer, nsInstallResources::GetInstallFileString(), nsnull);
    }
    else if (mReplaceFile)
    {
        // we are replacing this file.

        sprintf( (char *)buffer, nsInstallResources::GetReplaceFileString(), mFinalFile->GetCString());
    }
    else
    {
        sprintf( (char *)buffer, nsInstallResources::GetInstallFileString(), mFinalFile->GetCString());
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
        result = ReplaceFileNowOrSchedule(*mExtracedFile, *mFinalFile );
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
                (char*)(const char*)mFinalFile->GetNativePathCString(),  // DO NOT CHANGE THIS. 
                (char*)(const char*)nsAutoCString(*mVersionInfo), 
                PR_FALSE );

    if (mUpgradeFile) 
    {
        if (refCount == 0) 
            VR_SetRefCount( (char*)(const char*)nsAutoCString(*mVersionRegistryName), 1 );
        else 
            VR_SetRefCount( (char*)(const char*)nsAutoCString(*mVersionRegistryName), refCount );  //FIX?? what should the ref count be/
    }
    else
    {
        if (refCount != 0) 
        {
            VR_SetRefCount( (char*)(const char*)nsAutoCString(*mVersionRegistryName), refCount + 1 );
        } 
        else 
        {
            if (mReplaceFile)
                VR_SetRefCount( (char*)(const char*)nsAutoCString(*mVersionRegistryName), 2 );
            else
                VR_SetRefCount( (char*)(const char*)nsAutoCString(*mVersionRegistryName), 1 );
        }
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
