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

#include "nsCRT.h"

#include "nsFileSpec.h"

#include "VerReg.h"
#include "nsInstallPatch.h"

#include "nsInstall.h"
#include "nsIDOMInstallFolder.h"
#include "nsIDOMInstallVersion.h"

#include "nsInstallErrorMessages.h"


nsInstallPatch::nsInstallPatch( nsInstall* inInstall,
                                const nsString& inVRName,
                                nsIDOMInstallVersion* inVInfo,
                                const nsString& inJarLocation,
                                PRInt32 *error)

: nsInstallObject(inInstall)
{
    if ((inInstall == nsnull) || (inVRName == "null") || (inJarLocation == "null")) 
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }

    mRegistryName   =   inVRName;
    mVersionInfo    =   inVInfo;    /* Who owns this object? May be we should make a copy of it */
    
    mJarLocation    =   inJarLocation; 

    mPatchFile      =   "";
    mTargetFile     =   "";
    mPatchedFile    =   "";

    char* tempTargetFile = (char*) PR_MALLOC(MAXREGPATHLEN);
    char* tempVRName     = inVRName.ToNewCString();
    
    PRInt32 err = VR_GetPath( tempVRName, MAXREGPATHLEN, tempTargetFile );
    
    if (err != REGERR_OK)
    {
        PR_FREEIF(tempTargetFile);
        
        *error = nsInstall::NO_SUCH_COMPONENT;
        return;
    }

    mTargetFile.SetString(tempTargetFile);
    
    delete tempVRName;
    delete tempTargetFile;

    *error = nsInstall::SUCCESS;
}


nsInstallPatch::nsInstallPatch( nsInstall* inInstall,
                                const nsString& inVRName,
                                nsIDOMInstallVersion* inVInfo,
                                const nsString& inJarLocation,
                                nsIDOMInstallFolder* folderSpec,
                                const nsString& inPartialPath,
                                PRInt32 *error)

: nsInstallObject(inInstall)
{
    if ((inInstall == nsnull) || (inVRName == "null") || (inJarLocation == "null")) 
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }

    mRegistryName   =   inVRName;
    mVersionInfo    =   inVInfo;    /* Who owns this object? May be we should make a copy of it */
    mJarLocation    =   inJarLocation; 

    mPatchFile      =   "";
    mPatchedFile    =   "";

    folderSpec->MakeFullPath(inPartialPath, mTargetFile);
}

nsInstallPatch::~nsInstallPatch()
{
}


PRInt32 nsInstallPatch::Prepare()
{
    PRInt32 err;
    PRBool deleteOldSrc;
    
    char *tempString = mTargetFile.ToNewCString();
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

    if (err != nsInstall::SUCCESS)
    {   
        return err;
    }

    mInstall->ExtractFileFromJar(mJarLocation, mTargetFile, mPatchFile, &err);
   
    
    nsString *fileName = nsnull;
    nsVoidKey ikey( (void*) nsCRT::HashValue( mTargetFile.GetUnicode() )  );
    
    mInstall->GetPatch(&ikey, fileName);

    if (fileName != nsnull) 
    {
        deleteOldSrc = PR_TRUE;
    } 
    else 
    {
        fileName = new nsString(mTargetFile);
        deleteOldSrc = PR_FALSE;
    }

    err = NativePatch( *fileName, mPatchFile, mPatchedFile);
    
    if (err != nsInstall::SUCCESS)
    {   
        return err;
    }

    if ( mPatchedFile != "" ) 
    {
        mInstall->AddPatch(&ikey, &mPatchedFile );
    } 
    else 
    {
        // This is a wierd state.  NativePatch should have generated an error here.  Is there any
        // reason why we return a Invalid_Arg error here?
        err = nsInstall::INVALID_ARGUMENTS;
    }

    if ( deleteOldSrc ) 
    {
        NativeDeleteFile( *fileName );
    }
  
    return err;
}

PRInt32 nsInstallPatch::Complete()
{  
    if ((mInstall == nsnull) || (mVersionInfo == nsnull) || (mPatchedFile == "") || (mTargetFile == "")) 
    {
        return nsInstall::INVALID_ARGUMENTS;
    }
    
    PRInt32 err = nsInstall::SUCCESS;

    nsString *fileName = nsnull;
    nsVoidKey ikey( (void*) nsCRT::HashValue( mTargetFile.GetUnicode() )  );
    
    mInstall->GetPatch(&ikey, fileName);
    
    if (fileName != nsnull && fileName->Equals(mPatchedFile) )
    {
        // the patch has not been superceded--do final replacement
        err = NativeReplace( mTargetFile, mPatchedFile );
        if ( 0 == err || nsInstall::REBOOT_NEEDED == err ) 
        {
            // WHY DO nsString suck so bad!

            char* tempVRString      = mRegistryName.ToNewCString();
            char* tempTargetFile    = mTargetFile.ToNewCString();
            
            nsString tempString;
            mVersionInfo->ToString(tempString);
            char* tempVersion = tempString.ToNewCString();
            
            err = VR_Install( tempVRString, tempTargetFile, tempVersion, PR_FALSE );
            
            delete tempVRString;
            delete tempTargetFile;
            delete tempVersion;

        }
        else
        {
            err = nsInstall::UNEXPECTED_ERROR;
        }
    }
    else
    {
        // nothing -- old intermediate patched file was
        // deleted by a superceding patch
    }

    return err;
}

void nsInstallPatch::Abort()
{
    nsString *fileName = nsnull;
    nsVoidKey ikey( (void*) nsCRT::HashValue( mTargetFile.GetUnicode() )  );

    mInstall->GetPatch(&ikey, fileName);

    if (fileName != nsnull && fileName->Equals(mPatchedFile) )
    {
        NativeDeleteFile( mPatchedFile );
    }
}

char* nsInstallPatch::toString()
{
    //return GetString1(DETAILS_PATCH, targetfile);
    return nsnull;
}


PRBool
nsInstallPatch::CanUninstall()
{
    return PR_FALSE;
}

PRBool
nsInstallPatch::RegisterPackageNode()
{
    return PR_FALSE;
}

PRInt32
nsInstallPatch::NativePatch(const nsString &sourceFile, const nsString &patchfile, nsString &newFile)
{
    return -1;
}

PRInt32
nsInstallPatch::NativeDeleteFile(const nsString& doomedFile)
{
    char * tempFile = doomedFile.ToNewCString();
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

PRInt32
nsInstallPatch::NativeReplace(const nsString& oldfile, nsString& newFile)
{

    char *tempString = oldfile.ToNewCString();
    nsFileSpec file(tempString);
    delete tempString;

    if (file.Exists() && (! file.IsFile()) )
        return nsInstall::FILE_IS_DIRECTORY;
    
    
    file.Delete(PR_FALSE);
    if (file.Exists())
    {
        //FIX: FE_ReplaceExistingFile
    }
    
    nsFileSpec parentDirectory;
    file.GetParent(parentDirectory);
    
    if (parentDirectory.Exists() && parentDirectory.IsDirectory())
    {
        char* currentName = oldfile.ToNewCString();
        char* finalName   = newFile.ToNewCString();
        
        // FIX - this may not work on UNIX between different
        // filesystems! 
        PRInt32 result = PR_Rename(currentName, finalName);
        
        delete currentName;
        delete finalName;

        if (result != 0)
        {
            return nsInstall::UNEXPECTED_ERROR;   
        }
    }
    
    return nsInstall::SUCCESS;
}
