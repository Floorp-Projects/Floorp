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

#include "prprf.h"
#include "nsInstallFile.h"
#include "nsFileSpec.h"
#include "VerReg.h"
#include "ScheduledTasks.h"
#include "nsInstall.h"
#include "nsIDOMInstallVersion.h"
#include "nsInstallResources.h"
#include "nsInstallLogComment.h"
#include "nsInstallBitwise.h"

/* Public Methods */

/*	Constructor
        inInstall    - softUpdate object we belong to
        inComponentName	- full path of the registry component
        inVInfo	        - full version info
        inJarLocation   - location inside the JAR file
        inFinalFileSpec	- final	location on disk
*/

MOZ_DECL_CTOR_COUNTER(nsInstallFile);

nsInstallFile::nsInstallFile(nsInstall* inInstall,
                             const nsString& inComponentName,
                             const nsString& inVInfo,
                             const nsString& inJarLocation,
                             nsInstallFolder *folderSpec,
                             const nsString& inPartialPath,
                             PRInt32 mode,
                             PRInt32 *error) 
  : nsInstallObject(inInstall),
    mVersionInfo(nsnull),
    mJarLocation(nsnull),
    mExtractedFile(nsnull),
    mFinalFile(nsnull),
    mVersionRegistryName(nsnull),
    mReplaceFile(PR_FALSE),
    mChildFile(PR_TRUE),
    mUpgradeFile(PR_FALSE),
    mSkipInstall(PR_FALSE),
    mMode(mode)
{
    MOZ_COUNT_CTOR(nsInstallFile);

    PRBool flagExists, flagIsFile;
    mFolderCreateCount = 0;

    if ((folderSpec == nsnull) || (inInstall == NULL))
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }

    *error = nsInstall::SUCCESS;
    
    /* Check for existence of the newer	version	*/
#if 0 // XXX need to re-implement force mode in the opposite sense

    char* qualifiedRegNameString = inComponentName.ToNewCString();

    // --------------------------------------------------------------------
    // we always install if forceInstall is true, or the new file's
    // version is null, or the file doesn't previously exist.
    //
    // IFF it's not force, AND the new file has a version, AND it's been
    // previously installed, THEN we have to do the version comparing foo.
    // --------------------------------------------------------------------
    if ( !(mode & INSTALL_NO_COMPARE ) && (inVInfo !=  "") && 
          ( VR_ValidateComponent( qualifiedRegNameString ) == 0 ) ) 
    {
        nsInstallVersion *newVersion = new nsInstallVersion();
        
        if (newVersion == nsnull)
        {
            Recycle(qualifiedRegNameString);
            *error = nsInstall::OUT_OF_MEMORY;
            return;
        }

        newVersion->Init(inVInfo);
        
        VERSION versionStruct;
        
        VR_GetVersion( qualifiedRegNameString, &versionStruct );
        
        nsInstallVersion* oldVersion = new nsInstallVersion();
        
        if (oldVersion == nsnull)
        {
            Recycle(qualifiedRegNameString);
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

        if ( areTheyEqual < 0 )
        {
            // the file to be installed is OLDER than what is on disk.
            // Don't install it.
            mSkipInstall = PR_TRUE;
        }
    }

    Recycle(qualifiedRegNameString);
#endif

    nsCOMPtr<nsIFile> tmp = folderSpec->GetFileSpec();
    if (!tmp)
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }

    tmp->Clone(getter_AddRefs(mFinalFile));
    if (mFinalFile == nsnull)
    {
        *error = nsInstall::OUT_OF_MEMORY;
        return;
    }

    mFinalFile->Exists(&flagExists);
    if (flagExists)
    {
        // is there a file with the same name as the proposed folder?
        mFinalFile->IsFile(&flagIsFile);
        if ( flagIsFile) 
        {
            *error = nsInstall::ACCESS_DENIED;
            return;
        }
        // else this directory already exists, so do nothing
    }

    //Need to parse the inPartialPath to remove any separators
    PRBool finished = PR_FALSE;
    PRUint32 offset = 0;
    PRInt32 location = 0, pass = 0;
    nsString subString;

    //nsString tempPartialPath(inPartialPath);

    while (!finished)
    {
        location = inPartialPath.FindChar('/',PR_FALSE, offset);
        if ((location < 0) && (pass == 0)) //no separators were found
        {
            nsAutoCString tempPartialPath(inPartialPath);
            mFinalFile->Append(tempPartialPath);
            finished = PR_TRUE;
        }
        else if ((location < 0) && (pass > 0) && (offset < inPartialPath.mLength)) //last occurance
        {
            nsresult rv = inPartialPath.Mid(subString, offset, inPartialPath.mLength-offset);
            nsAutoCString tempSubString(subString);
            mFinalFile->Append(tempSubString);
            finished = PR_TRUE;
        }
        else
        {
            nsresult rv = inPartialPath.Mid(subString, offset, location-offset);
            nsAutoCString tempSubString(subString);
            mFinalFile->Append(tempSubString);
            offset = location + 1;
            pass++;
        }
    }

    //{
    //    nsresult rv = mFinalFile->Append(inPartialPath.ToNewCString());
    //    if (rv != NS_OK)
    //    {
    //        *error = nsInstall::ILLEGAL_RELATIVE_PATH;
    //       return;
    //    }
    //}
    
    mFinalFile->Exists(&mReplaceFile);
    mVersionRegistryName  = new nsString(inComponentName);
    mJarLocation          = new nsString(inJarLocation);
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
    if ( regPackageName.IsEmpty() ) 
    {
        // in the "current communicator package" absolute pathnames (start
        // with slash) indicate shared files -- all others are children
        mChildFile = ( mVersionRegistryName->CharAt(0) != '/' );
    } 
    else 
    {
        mChildFile = mVersionRegistryName->EqualsWithConversion( regPackageName,
                                                                 PR_FALSE,
                                                                 regPackageName.Length() );
    }
}


nsInstallFile::~nsInstallFile()
{
    if (mVersionRegistryName)
        delete mVersionRegistryName;
  
    if (mJarLocation)
        delete mJarLocation;
  
    if (mVersionInfo)
        delete mVersionInfo;
    
    //if(mFinalFile)
    //    mFinalFile = nsnull;

    //if(mExtractedFile)
    //    mExtractedFile = nsnull;

    MOZ_COUNT_DTOR(nsInstallFile);
}




void nsInstallFile::CreateAllFolders(nsInstall *inInstall, nsIFile *inFolderPath, PRInt32 *error)
{
    /* the nsFileSpecMac.cpp operator += requires "this" (the nsFileSpec)
     * to be an existing dir
     */

    nsCOMPtr<nsIFile>   nsfsFolderPath;
    nsString            nsStrFolder;
    PRBool              flagExists;
    int                 result = 0;
    nsInstallLogComment *ilc   = nsnull;

    inFolderPath->Exists(&flagExists);
    if(!flagExists)
    {
        char *szPath = nsnull;

        inFolderPath->GetParent(getter_AddRefs(nsfsFolderPath));
        CreateAllFolders(inInstall, nsfsFolderPath, error);

        inFolderPath->Create(nsIFile::DIRECTORY_TYPE, 0755); //nsIFileXXX: What kind of permissions are required here?
        ++mFolderCreateCount;

        inFolderPath->GetPath(&szPath);
        nsStrFolder.AssignWithConversion(szPath);
        nsMemory::Free(szPath);
        ilc = new nsInstallLogComment(inInstall, NS_ConvertASCIItoUCS2("CreateFolder"), nsStrFolder, error);
        if(ilc == nsnull)
            *error = nsInstall::OUT_OF_MEMORY;

        if(*error == nsInstall::SUCCESS) 
            *error = mInstall->ScheduleForInstall(ilc);
    }
}

#ifdef XXX_SSU
void nsInstallFile::RemoveAllFolders()
{
    /* the nsFileSpecMac.cpp operator += requires "this" (the nsFileSpec)
     * to be an existing dir
     */

    PRUint32   i;
    nsFileSpec nsfsFolder;
    nsFileSpec nsfsParentFolder;
    nsString   nsStrFolder;

    if(mFinalFile != nsnull)
    {
      mFinalFile->GetParent(nsfsFolder);
      for(i = 0; i < mFolderCreateCount; i++)
      {
          nsfsFolder.Delete(PR_FALSE);
          nsfsFolder.GetParent(nsfsParentFolder);
          nsfsFolder = nsfsParentFolder;
      }
    }
}
#endif






/* Prepare
 * Extracts file out of the JAR archive
 */
PRInt32 nsInstallFile::Prepare()
{
    PRInt32 error = nsInstall::SUCCESS;

    if (mSkipInstall)
        return nsInstall::SUCCESS;

    if (mInstall == nsnull || mFinalFile == nsnull || mJarLocation == nsnull )
        return nsInstall::INVALID_ARGUMENTS;

    if (mReplaceFile == PR_FALSE)
    {
       /* although it appears that we are creating the dir _again_ it is necessary
        * when inPartialPath has arbitrary levels of nested dirs before the leaf
        */
        nsCOMPtr<nsIFile> parent;
        mFinalFile->GetParent(getter_AddRefs(parent));
        CreateAllFolders(mInstall, parent, &error);
        if(nsInstall::SUCCESS != error)
            return error;
    }

    return mInstall->ExtractFileFromJar(*mJarLocation, mFinalFile, getter_AddRefs(mExtractedFile)); 
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
   
    if (mSkipInstall)
        return nsInstall::SUCCESS;

    err = CompleteFileMove();
    
    if ( 0 == err || nsInstall::REBOOT_NEEDED == err ) 
    {
        // XXX Don't register individual files for now -- crucial performance
        // speed up on the Mac, and we'll switch uninstall schemes after beta

        // RegisterInVersionRegistry();
    }
    
    return err;

}

void nsInstallFile::Abort()
{
    if (mExtractedFile != nsnull)
        mExtractedFile->Delete(PR_FALSE);
}

#define RESBUFSIZE 4096
char* nsInstallFile::toString()
{
    char* buffer  = new char[RESBUFSIZE];
    char* rsrcVal = nsnull;
    char* fname   = nsnull;

    if (buffer == nsnull || !mInstall)
        return nsnull;
    else
        buffer[0] = '\0';
    
    if (mReplaceFile)
    {
        if(mMode & nsInstall::WIN_SHARED_FILE)
        {
            rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("ReplaceSharedFile"));
        }
        else
        {
            rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("ReplaceFile"));
        }
    }
    else if (mSkipInstall)
    {
        if(mMode & nsInstall::WIN_SHARED_FILE)
        {
            rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("SkipSharedFile"));
        }
        else
        {
            rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("SkipFile"));
        }
    }
    else
    {
        if(mMode & nsInstall::WIN_SHARED_FILE)
        {
            rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("InstallSharedFile"));
        }
        else
        {
            rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("InstallFile"));
        }
    }

    if (rsrcVal)
    {
        char*    interimCStr = nsnull;
        nsString interimStr;

        if(mMode & nsInstall::DO_NOT_UNINSTALL)
          interimStr.AssignWithConversion("(*dnu*) ");

        interimStr.AppendWithConversion(rsrcVal);
        interimCStr = interimStr.ToNewCString();
        if(interimCStr == nsnull)
          return interimCStr;

        if (mFinalFile)
            mFinalFile->GetPath(&fname);

        PR_snprintf( buffer, RESBUFSIZE, interimCStr, fname );

        Recycle(rsrcVal);
    }

    return buffer;
}


PRInt32 nsInstallFile::CompleteFileMove()
{
    int    result         = 0;
    char   *temp;
    PRBool bAlreadyExists = PR_FALSE;
    PRBool bIsEqual = PR_FALSE;
    
    if (mExtractedFile == nsnull) 
    {
        return nsInstall::UNEXPECTED_ERROR;
    }
   	
    
    mExtractedFile->Equals(mFinalFile, &bIsEqual);
    if ( bIsEqual ) 
    {
        /* No need to rename, they are the same */
        result = nsInstall::SUCCESS;
    } 
    else 
    {
        result = ReplaceFileNowOrSchedule(mExtractedFile, mFinalFile );
    }

    if(mMode & nsInstall::WIN_SHARED_FILE)
    {
      if(mReplaceFile || mSkipInstall)
          bAlreadyExists = PR_TRUE;

      mFinalFile->GetPath(&temp);
      RegisterSharedFile(temp, bAlreadyExists);
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
        if (!regPackageName.IsEmpty()) 
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

    char* temp;
    mFinalFile->GetPath(&temp);
    VR_Install( (char*)(const char*)nsAutoCString(*mVersionRegistryName), 
                (char*)(const char*)temp,  // DO NOT CHANGE THIS. 
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
        if (!regPackageName.IsEmpty()) 
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
    return PR_TRUE;
}

/* RegisterPackageNode
* InstallFile() installs files which need to be registered,
* hence this function returns true.
*/
PRBool
nsInstallFile::RegisterPackageNode()
{
    return PR_TRUE;
}
