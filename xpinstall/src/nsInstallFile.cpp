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
#if defined(XP_MAC)
#include <stat.h>
#else
#include <sys/stat.h>
#endif

#include "prio.h"
#include "prmem.h"
#include "plstr.h"

#include "VerReg.h"
#include "nsInstallFile.h"

#include "nsIDOMInstall.h"
#include "nsIDOMInstallFolder.h"
#include "nsIDOMInstallVersion.h"

#include "nsInstallErrorMessages.h"

static PRBool endsWith(nsString* str, char* string_to_find);

static PRBool endsWith(nsString* str, char* string_to_find) 
{
  PRBool found = PR_FALSE;
  if (str) {
    int len = strlen(".zip");
    int size = str->Length();
    int offset = str->RFind(string_to_find, PR_FALSE);
    if (offset == (size - len))
      found = PR_TRUE;
  }
  return found;
}

/* Public Methods */

/*	Constructor
        inInstall    - softUpdate object we belong to
        inComponentName	- full path of the registry component
        inVInfo	        - full version info
        inJarLocation   - location inside the JAR file
        inFinalFileSpec	- final	location on disk
*/
nsInstallFile::nsInstallFile(nsIDOMInstall* inInstall,
                             const nsString& inVRName,
                             nsIDOMInstallVersion* inVInfo,
                             const nsString& inJarLocation,
                             nsIDOMInstallFolder* folderSpec,
                             const nsString& inPartialPath,
                             PRBool forceInstall,
                             char* *errorMsg) 
: nsInstallObject(inInstall)
{
    mTempFile    = nsnull;
    mFinalFile   = nsnull;
    mUpgradeFile = PR_FALSE;

    if ((folderSpec == NULL) || (inInstall == NULL)  ||
        (inVInfo == NULL)) 
    {
        *errorMsg = nsInstallErrorMessages::GetErrorMsg( "Invalid arguments to the constructor", 
            nsIDOMInstall::SUERR_INVALID_ARGUMENTS);
        return;
    }
    
    mVersionRegistryName    = new nsString(inVRName);
    mJarLocation            = new nsString(inJarLocation);
    mVersionInfo	        = inVInfo; /* XXX: Who owns and who free's this object. Is it nsSoftwareUpdate?? */
    mForceInstall           = forceInstall;
    
    folderSpec->IsJavaCapable(&mJavaInstall);
    mFinalFile = new nsString();
    folderSpec->MakeFullPath(inPartialPath, *mFinalFile);
    
    mReplaceFile            = DoesFileExist();

    
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
  delete mVersionRegistryName;
  delete mJarLocation;
  if (mTempFile)
    delete mTempFile;
  if (mFinalFile)
    delete mFinalFile;
}

/* Prepare
 * Extracts file out of the JAR archive into the temp directory
 */
char* nsInstallFile::Prepare()
{
    char *errorMsg = NULL;

    if (mInstall == NULL) 
    {
        errorMsg = nsInstallErrorMessages::GetErrorMsg("nsSoftwareUpdate object is null", 
                                                nsIDOMInstall::SUERR_INVALID_ARGUMENTS);
        return errorMsg;
    }
    
    if (mJarLocation == NULL) 
    {
        errorMsg = nsInstallErrorMessages::GetErrorMsg("JAR file is null", 
                                                nsIDOMInstall::SUERR_INVALID_ARGUMENTS);

        return errorMsg;
    }

    if (mFinalFile == NULL) 
    {
        errorMsg = nsInstallErrorMessages::GetErrorMsg("folderSpec's full path (mFinalFile) was null", 
                                                nsIDOMInstall::SUERR_INVALID_ARGUMENTS);
        return errorMsg;
  }


    nsString errString;
    mInstall->ExtractFileFromJar(*mJarLocation, *mFinalFile, *mTempFile, errString);

    if (errString != "") 
    {
        return errString.ToNewCString();
    }
  
    return NULL;
}

/* Complete
 * Completes the install:
 * - move the downloaded file to the final location
 * - updates the registry
 */
char* nsInstallFile::Complete()
{
    int err;
    int refCount;
    int rc;

    if (mInstall == NULL) 
    {
        return nsInstallErrorMessages::GetErrorMsg("nsSoftwareUpdate object is null", 
                                                    nsIDOMInstall::SUERR_INVALID_ARGUMENTS);
    }
  
    if (mVersionRegistryName == NULL) 
    {
        return nsInstallErrorMessages::GetErrorMsg("version registry name is null", 
                                                    nsIDOMInstall::SUERR_INVALID_ARGUMENTS);
    }

    if (mFinalFile == NULL) 
    {
       return nsInstallErrorMessages::GetErrorMsg("folderSpec's full path (mFinalFile) is null", 
                                                    nsIDOMInstall::SUERR_INVALID_ARGUMENTS);
    }

    /* Check the security for our target */
    
    err = NativeComplete();
  

    char *vr_name    = mVersionRegistryName->ToNewCString();
    char *final_file = mFinalFile->ToNewCString();
  
      // Add java archives to the classpath. Don't add if we're
      // replacing an existing file -- it'll already be there.
  
    if ( mJavaInstall && !mReplaceFile ) 
    {
        PRBool found_zip = endsWith(mFinalFile, ".zip");
        PRBool found_jar = endsWith(mFinalFile, ".jar");;
        if (found_zip || found_jar) 
        {
            AddToClasspath( mFinalFile );
        }
    }
  
    nsString regPackageName;
    mInstall->GetRegPackageName(regPackageName);
    
  
    // Register file and log for Uninstall
  
    if ( 0 == err || nsIDOMInstall::SU_REBOOT_NEEDED == err ) 
    {
        // we ignore all registry errors because they're not
        // important enough to abort an otherwise OK install.
        if (!mChildFile) 
        {
            int found;
            if (regPackageName != "") 
            {
                char *reg_package_name = regPackageName.ToNewCString();
                found = VR_UninstallFileExistsInList( reg_package_name, vr_name );
                delete reg_package_name;
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
                VR_Install( vr_name, final_file, (char*)(PRUnichar*) tempString, PR_FALSE );
                VR_SetRefCount( vr_name, rc );
            } 
            else 
            {
                if (mReplaceFile)
                {
                    nsString tempString;
                    mVersionInfo->ToString(tempString);
                    VR_Install( vr_name, final_file, (char*)(PRUnichar*) tempString, PR_FALSE);
                    VR_SetRefCount( vr_name, 2 );
                }
                else
                {
                    nsString tempString;
                    mVersionInfo->ToString(tempString);
                    VR_Install( vr_name, final_file, (char*)(PRUnichar*) tempString, PR_FALSE );
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
                VR_Install( vr_name, final_file, (char*)(PRUnichar*) tempString, PR_FALSE );
                VR_SetRefCount( vr_name, 1 );
            } 
            else 
            {
                nsString tempString;
                mVersionInfo->ToString(tempString);
                VR_Install( vr_name, final_file, (char*)(PRUnichar*) tempString, PR_FALSE );
                VR_SetRefCount( vr_name, 0 );
            }
        }
    
        if ( !mChildFile && !mUpgradeFile ) 
        {
            if (regPackageName != "") 
            {
                char *reg_package_name = regPackageName.ToNewCString();
                VR_UninstallAddFileToList( reg_package_name, vr_name );
                delete reg_package_name;
            } 
            else 
            {
                VR_UninstallAddFileToList( "", vr_name );
            }
        }
    }
    delete vr_name;
    delete final_file;

    if ( err != 0 ) 
    {
           return nsInstallErrorMessages::GetErrorMsg(nsIDOMInstall::SU_INSTALL_FILE_UNEXPECTED_MSG_ID, mFinalFile, err);
    }
    return NULL;
}

void nsInstallFile::Abort()
{
    char* currentName;
    int result;

    /* Get the names */
    if (mTempFile == NULL) 
        return;
    currentName = mTempFile->ToNewCString();

    result = PR_Delete(currentName);
    PR_ASSERT(result == 0); /* XXX: need to fe_deletefilelater() or something */
    delete currentName;
}

char* nsInstallFile::toString()
{
    if (mReplaceFile) 
    {
        return nsInstallErrorMessages::GetString(nsIDOMInstall::SU_DETAILS_REPLACE_FILE_MSG_ID, mFinalFile);
    } 
    else 
    {
        return nsInstallErrorMessages::GetString(nsIDOMInstall::SU_DETAILS_INSTALL_FILE_MSG_ID, mFinalFile);
  }
}


/* Private Methods */

/* Complete 
 * copies the file to its final location
 * Tricky, we need to create the directories 
 */
int nsInstallFile::NativeComplete()
{
    char* currentName = NULL;
    char* finalName = NULL;
    int result = 0;
    
    if (mTempFile == nsnull) 
    {
        return -1;
    }
    
    /* Get the names */
    currentName = mTempFile->ToNewCString();
    finalName   = mFinalFile->ToNewCString();
	
    if ( finalName == NULL || currentName == NULL ) 
    {
        /* memory or JRI problems */
        result = -1;
        goto end;
    } 
    
    if ( PL_strcmp(finalName, currentName) == 0 ) 
    {
        /* No need to rename, they are the same */
        result = 0;
    } 
    else 
    {
        struct stat finfo;
        if (stat(finalName, &finfo) != 0)
        {
            PR_Rename(currentName, finalName);
        }
        else
        {
            /* Target exists, can't trust XP_FileRename--do platform
             * specific stuff in FE_ReplaceExistingFile()
             */
            result = -1;
        }
    }


    if (result != 0) 
    {
        struct stat finfo;
        if (stat(finalName, &finfo) == 0)
        {
            /* File already exists, need to remove the original */
            // result = FE_ReplaceExistingFile(currentName, xpURL, finalName, xpURL, mForceInstall);
            if ( result == nsIDOMInstall::SU_REBOOT_NEEDED ) 
            {
            }
        } 
        else 
        {
            /* Directory might not exist, check and create if necessary */
            char separator;
            char * end;
            separator = '/';
            end = PL_strrchr(finalName, separator);
            if (end) 
            {
                end[0] = 0;
                // FIX- this need to be made recursive?
                result = PR_MkDir( finalName, 0);
                end[0] = separator;
                if ( 0 == result )
                {
                    // FIX - this may not work on UNIX between different
                    // filesystems! 
                    result = PR_Rename(currentName, finalName);
                }
            }
        }
    }


end:
  delete [] finalName;
  delete [] currentName;
  return result;  
}



void nsInstallFile::AddToClasspath(nsString* file)
{
  if ( file != NULL ) {
    char *final_file = file->ToNewCString();
//    JVM_AddToClassPath(final_file);
    delete final_file;
  }
}


/* Finds out if the file exists
 */
PRBool nsInstallFile::DoesFileExist()
{
  if (mFinalFile == nsnull)
      return PR_FALSE;

  char* finalName = mFinalFile->ToNewCString();
  
  struct stat finfo;
  if ( stat(finalName, &finfo) != -1)
  {
      delete [] finalName;
      return PR_TRUE;
  }
  delete [] finalName;
  return PR_FALSE;
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
