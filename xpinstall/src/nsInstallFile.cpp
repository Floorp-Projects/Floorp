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
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

/* Public Methods */

/*	Constructor
        inInstall    - softUpdate object we belong to
        inComponentName	- full path of the registry component
        inVInfo	        - full version info
        inJarLocation   - location inside the JAR file
        inFinalFileSpec	- final	location on disk
*/

MOZ_DECL_CTOR_COUNTER(nsInstallFile)

nsInstallFile::nsInstallFile(nsInstall* inInstall,
                             const nsString& inComponentName,
                             const nsString& inVInfo,
                             const nsString& inJarLocation,
                             nsInstallFolder *folderSpec,
                             const nsString& inPartialPath,
                             PRInt32 mode,
                             PRBool  aRegister,
                             PRInt32 *error) 
  : nsInstallObject(inInstall),
    mVersionInfo(nsnull),
    mJarLocation(nsnull),
    mExtractedFile(nsnull),
    mFinalFile(nsnull),
    mVersionRegistryName(nsnull),
    mReplaceFile(PR_FALSE),
    mRegister(aRegister),
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
    PRInt32 location = 0, nodeLength = 0;
    nsString subString;

    location = inPartialPath.FindChar('/',PR_FALSE, offset);
    if (location == ((PRInt32)inPartialPath.Length() - 1)) //trailing slash
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }

    while (!finished)
    {
        if (location == kNotFound) //no separators were found
        {
            nodeLength = inPartialPath.Length() - offset;
            finished = PR_TRUE;
        }
        else
        {
            nodeLength = location - offset;
        }
        
        if (nodeLength > MAX_FILENAME) 
        {
            *error = nsInstall::FILENAME_TOO_LONG;
            return;
        }
        else
        {
            nsresult rv = inPartialPath.Mid(subString, offset, nodeLength);
            mFinalFile->Append(NS_LossyConvertUCS2toASCII(subString).get());
            offset += nodeLength + 1;
            if (!finished)
                location = inPartialPath.FindChar('/',PR_FALSE, offset);
        }
    }

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
        ilc = new nsInstallLogComment(inInstall, NS_LITERAL_STRING("CreateFolder"), nsStrFolder, error);
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
          nsfsFolder.Remove(PR_FALSE);
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
   
    err = CompleteFileMove();
    
    if ( mRegister && (0 == err || nsInstall::REBOOT_NEEDED == err) ) 
    {
        nsXPIDLCString path;
        mFinalFile->GetPath(getter_Copies(path));
        VR_Install( NS_CONST_CAST(char*, NS_ConvertUCS2toUTF8(*mVersionRegistryName).get()),
                    NS_CONST_CAST(char*, path.get()),
                    NS_CONST_CAST(char*, NS_ConvertUCS2toUTF8(*mVersionInfo).get()),
                    PR_FALSE );
    }
    
    return err;

}

void nsInstallFile::Abort()
{
    if (mExtractedFile != nsnull)
        mExtractedFile->Remove(PR_FALSE);
}

#define RESBUFSIZE 4096
char* nsInstallFile::toString()
{
    char* buffer  = new char[RESBUFSIZE];
    char* rsrcVal = nsnull;

    if (buffer == nsnull || !mInstall)
        return nsnull;
    else
        buffer[0] = '\0';
    
    if (mReplaceFile)
    {
        if(mMode & nsInstall::WIN_SHARED_FILE)
        {
            rsrcVal = mInstall->GetResourcedString(NS_LITERAL_STRING("ReplaceSharedFile"));
        }
        else
        {
            rsrcVal = mInstall->GetResourcedString(NS_LITERAL_STRING("ReplaceFile"));
        }
    }
    else
    {
        if(mMode & nsInstall::WIN_SHARED_FILE)
        {
            rsrcVal = mInstall->GetResourcedString(NS_LITERAL_STRING("InstallSharedFile"));
        }
        else
        {
            rsrcVal = mInstall->GetResourcedString(NS_LITERAL_STRING("InstallFile"));
        }
    }

    if (rsrcVal)
    {
        char*    interimCStr = nsnull;
        nsString interimStr;

        if(mMode & nsInstall::DO_NOT_UNINSTALL)
          interimStr.AssignWithConversion("(*dnu*) ");

        interimStr.AppendWithConversion(rsrcVal);
        interimCStr = ToNewCString(interimStr);

        if(interimCStr)
        {
            nsXPIDLCString fname;
            if (mFinalFile)
                mFinalFile->GetPath(getter_Copies(fname));

            PR_snprintf( buffer, RESBUFSIZE, interimCStr, fname.get() );
            Recycle(interimCStr);
        }
        Recycle(rsrcVal);
    }

    return buffer;
}


PRInt32 nsInstallFile::CompleteFileMove()
{
    int    result         = 0;
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
      nsXPIDLCString path;
      mFinalFile->GetPath(getter_Copies(path));
      RegisterSharedFile(path, mReplaceFile);
    }

    return result;  
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
