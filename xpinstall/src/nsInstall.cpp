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



#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"

#include "nsVector.h"
#include "nsHashtable.h"
#include "nsFileSpec.h"
#include "nsSpecialSystemDirectory.h"

#include "prmem.h"
#include "pratom.h"
#include "prefapi.h"
#include "VerReg.h"

#include "zipfile.h"

#include "nsInstall.h"
#include "nsInstallFolder.h"

#include "nsIDOMInstallVersion.h"

#include "nsInstallFile.h"
#include "nsInstallDelete.h"
#include "nsInstallExecute.h"
#include "nsInstallPatch.h"

#ifdef XP_PC
#define FILESEP "\\"
#elif defined(XP_MAC)
#define FILESEP ":"
#else
#define FILESEP "/"
#endif 



nsInstallInfo::nsInstallInfo(const nsString& fromURL)
{
    nsInstallInfo(fromURL, "", "");
}

nsInstallInfo::nsInstallInfo(const nsString& fromURL, const nsString& arguments)
{
    nsInstallInfo(fromURL, arguments, "");
}

nsInstallInfo::nsInstallInfo(const nsString& fromURL, const nsString& arguments, const nsString& flags)
{

    mFromURL            = new nsString(fromURL);
    mArguments          = new nsString(arguments);
    mFlags              = new nsString(flags);
    mLocalFile          = new nsString();
    
    mInstalled          = PR_FALSE;
    
    mMultipleTrigger    = PR_FALSE;
    mFromURLs           = nsnull;
    mLocalFiles         = nsnull;

    MakeTempFile(fromURL, *mLocalFile);
}

nsInstallInfo::nsInstallInfo(nsVector* fromURL, const nsString& arguments, const nsString& flags)
{
    mMultipleTrigger = PR_TRUE;

    mFromURLs        = fromURL;
    mLocalFiles      = new nsVector();   
    mArguments       = new nsString(arguments);
    mFlags           = new nsString(flags);

    mInstalled       = PR_FALSE;
    mMultipleTrigger = PR_FALSE;
    
    // The following are not to be used with the Multiple Trigger 
    mLocalFile  = nsnull;
    mFromURL    = nsnull;

}


nsInstallInfo::DeleteVector(nsVector* vector)
{
    if (vector != nsnull)
    {
        PRUint32 i=0;
        for (; i < vector->GetSize(); i++) 
        {
            nsString* element = (nsString*)vector->Get(i);
            if (element != nsnull)
                delete element;
        }

        vector->RemoveAll();
        delete (vector);
        vector = nsnull;
    }
}

nsInstallInfo::~nsInstallInfo()
{
    if (mMultipleTrigger)
    {
        DeleteVector(mFromURLs);
        DeleteVector(mLocalFiles);
    }
    else
    {
        
        if (mLocalFile)
            delete mLocalFile;
        
        delete mFromURL;
    }

    delete mArguments;
    delete mFlags;
}

nsInstallInfo::MakeTempFile(nsString aURL, nsString& tempFileString)
{
    // Checking to see if the url is local is a hack.  I should see if 
    // netlib could do this for me: given a url, give me path on the users
    // if it is local otherwise return null to indicate that I need to 
    // create a tempfile.
    if (aURL.Compare(nsString("file://"), false, 7) == 0)
    {       
        tempFileString = nsFileSpec(nsFileURL(aURL)).GetCString();
    }
    else
    {
       nsSpecialSystemDirectory tempFile(nsSpecialSystemDirectory::OS_TemporaryDirectory);
    
        PRInt32 result = aURL.RFind('/');
        if (result != -1)
        {            
            nsString jarName;
            aURL.Right(jarName, (aURL.Length() - result) );        
            tempFile += jarName;
        }
        else
        {   
            tempFile += "xpinstall.jar";
        }

        tempFile.MakeUnique();

        tempFileString = nsString(tempFile.GetCString());
    }
}

nsString& 
nsInstallInfo::GetFromURL(PRUint32 index)
{
    if (mMultipleTrigger)
    {
        nsString* element = (nsString*)mFromURLs->Get(index);
        return *element;
    }
    else
    {
        return *mFromURL;
    }
}

nsString& 
nsInstallInfo::GetLocalFile(PRUint32 index)
{
    if (mMultipleTrigger)
    {
        nsString* element = (nsString*)mLocalFiles->Get(index);
        return *element;
    }
    else
    {
        return *mLocalFile;
    }
}

nsString& 
nsInstallInfo::GetArguments()
{
    return *mArguments;
}

nsString& nsInstallInfo::GetFlags()
{
    return *mFlags;
}


PRBool
nsInstallInfo::IsMultipleTrigger()
{
    return mMultipleTrigger;
}

nsInstall::nsInstall()
{
    mScriptObject           = nsnull;           // this is the jsobject for our context
    mVersionInfo            = nsnull;           // this is the version information passed to us in StartInstall()
    mJarFileData            = nsnull;           // this is an opaque handle to the jarfile.  
    mRegistryPackageName    = "";               // this is the name that we will add into the registry for the component we are installing 
    mUIName                 = "";               // this is the name that will be displayed in UI.

    mUninstallPackage = PR_FALSE;
    mRegisterPackage  = PR_FALSE;

    mJarFileLocation = "";
    mInstallArguments = "";
}

nsInstall::~nsInstall()
{
    if (mVersionInfo != nsnull)
        delete mVersionInfo;
}


nsInstall::SetScriptObject(void *aScriptObject)
{
  mScriptObject = (JSObject*) aScriptObject;
  return NS_OK;
}

PRInt32    
nsInstall::GetUserPackageName(nsString& aUserPackageName)
{
    aUserPackageName = mUIName;
    return NS_OK;
}

PRInt32    
nsInstall::GetRegPackageName(nsString& aRegPackageName)
{
    aRegPackageName = mRegistryPackageName;
    return NS_OK;
}

PRInt32    
nsInstall::AbortInstall()
{
    nsInstallObject* ie;
    if (mInstalledFiles != nsnull) 
    {
        PRUint32 i=0;
        for (i=0; i < mInstalledFiles->GetSize(); i++) 
        {
            ie = (nsInstallObject *)mInstalledFiles->Get(i);
            if (ie == nsnull)
                continue;
            ie->Abort();
        }
    }
    
    CleanUp();
    
    return NS_OK;
}

PRInt32    
nsInstall::AddDirectory(const nsString& aRegName, 
                        const nsString& aVersion, 
                        const nsString& aJarSource, 
                        const nsString& aFolder, 
                        const nsString& aSubdir, 
                        PRBool aForceMode, 
                        PRInt32* aReturn)
{
    nsInstallFile* ie = nsnull;
    PRInt32 result;
    
    if ( aJarSource == "null" || aFolder == "null") 
    {
        *aReturn = SaveError(nsInstall::INVALID_ARGUMENTS);
        return NS_OK;
    }
    
    result = SanityCheck();
    
    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
    
    nsString* qualifiedRegName = nsnull;
    
    if ( aRegName == "" ) 
    {
        // Default subName = location in jar file
        qualifiedRegName = GetQualifiedRegName( aJarSource );
    } 
    else 
    {
        qualifiedRegName = GetQualifiedRegName( aRegName );
    }

    if (qualifiedRegName == nsnull)
    {
        *aReturn = SaveError( BAD_PACKAGE_NAME );
        return NS_OK;
    }
    
    nsString subdirectory(aSubdir);

    if (subdirectory != "")
    {
        subdirectory.Append("/");
    }

    PRBool bInstall;
    
    nsVector paths;
    
    result = ExtractDirEntries(aJarSource, &paths);
    
    PRInt32  pathsUpperBound = paths.GetUpperBound();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
    
    for (int i=0; i< pathsUpperBound; i++)
    {
        nsInstallVersion* newVersion = new nsInstallVersion();
            
        nsString *fullRegName = new nsString(*qualifiedRegName);
        fullRegName->Append("/");
        fullRegName->Append(*(nsString *)paths[i]);
        
        char* fullRegNameCString = fullRegName->ToNewCString();
        
        if ( (aForceMode == PR_FALSE) && (aVersion == "null") &&
            (VR_ValidateComponent(fullRegNameCString) == 0))
        {
            VERSION versionStruct;
            VR_GetVersion( fullRegNameCString, &versionStruct);

            //fix: when we have overloading!
            nsInstallVersion* oldVer = new nsInstallVersion();
            oldVer->Init(versionStruct.major,
                         versionStruct.minor,
                         versionStruct.release,
                         versionStruct.build);
             
            
            newVersion->Init(aVersion);
            
            PRInt32 areTheyEqual;
            newVersion->CompareTo(oldVer, &areTheyEqual);
            delete newVersion;

            bInstall = ( areTheyEqual > 0 );
      
            if (oldVer)
		        delete oldVer;
        
        }
        else
        {
            // file doesn't exist or "forced" install
            bInstall = PR_TRUE;
        }
        
        delete fullRegNameCString;
        
        if (bInstall)
        {
            nsString *newJarSource = new nsString(aJarSource);
            newJarSource->Append("/");
            newJarSource->Append(*(nsString *)paths[i]);

            nsString* newSubDir;

            if (subdirectory != "")
            {
                newSubDir = new nsString(subdirectory);
                newSubDir->Append(*(nsString*)paths[i]);
            }
            else
            {
                newSubDir = new nsString(*(nsString*)paths[i]);
            }
            
            ie = new nsInstallFile( this,
                                    *fullRegName,
                                    newVersion,
                                    *newJarSource,
                                    aFolder,
                                    *newSubDir,
                                    aForceMode,
                                    &result);
            delete fullRegName;
            delete newJarSource;
            delete newSubDir;
            delete newVersion;

            if (result == nsInstall::SUCCESS)
            {
                result = ScheduleForInstall( ie );
            }
            else
            {
                delete ie;
            }
        }
    }
    
    if (qualifiedRegName != nsnull)
        delete qualifiedRegName;
    
    *aReturn = SaveError( result );
  
    return NS_OK;
}

PRInt32    
nsInstall::AddSubcomponent(const nsString& aRegName, 
                           const nsString& aVersion, 
                           const nsString& aJarSource, 
                           const nsString& aFolder, 
                           const nsString& aTargetName, 
                           PRBool aForceMode, 
                           PRInt32* aReturn)
{
    nsInstallFile*  ie;
    nsString*       qualifiedRegName = nsnull;
    
    PRInt32         errcode = nsInstall::SUCCESS;
    
    if ( aJarSource == "null" || aFolder == "null") 
    {
        *aReturn = SaveError( nsInstall::INVALID_ARGUMENTS );
        return NS_OK;
    }
    
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }


    if ( aRegName == "" ) 
    {
        // Default subName = location in jar file
        qualifiedRegName = GetQualifiedRegName( aJarSource );
    } 
    else 
    {
        qualifiedRegName = GetQualifiedRegName( aRegName );
    }

    if (qualifiedRegName == nsnull)
    {
        *aReturn = SaveError( BAD_PACKAGE_NAME );
        return NS_OK;
    }
    
    /* Check for existence of the newer	version	*/
    
    nsInstallVersion *newVersion = new nsInstallVersion();
    newVersion->Init(aVersion);

    PRBool versionNewer = PR_FALSE;
    char* qualifiedRegNameString = qualifiedRegName->ToNewCString();

    if ( (aForceMode == PR_FALSE ) && (aVersion !=  "null") && ( VR_ValidateComponent( qualifiedRegNameString ) == 0 ) ) 
    {
        VERSION versionStruct;
        
        VR_GetVersion( qualifiedRegNameString, &versionStruct );
    
        nsInstallVersion* oldVersion = new nsInstallVersion();
// FIX.  Once we move to XPConnect, we can have parameterized constructors.
        oldVersion->Init(versionStruct.major,
                         versionStruct.minor,
                         versionStruct.release,
                         versionStruct.build);

        PRInt32 areTheyEqual;
        newVersion->CompareTo((nsInstallVersion*)oldVersion, &areTheyEqual);
        
        if ( areTheyEqual != nsIDOMInstallVersion::EQUAL )
            versionNewer = PR_TRUE;
      
	    if ( oldVersion )
		    delete oldVersion;
    }
    else 
    {
        versionNewer = PR_TRUE;
    }
    
    
    if (qualifiedRegNameString != nsnull)
        delete qualifiedRegNameString;

    if (versionNewer) 
    {
        ie = new nsInstallFile( this, 
                                *qualifiedRegName, 
                                newVersion, 
                                aJarSource,
                                aFolder,
                                aTargetName, 
                                aForceMode, 
                                &errcode );

        if (errcode == nsInstall::SUCCESS) 
        {
            errcode = ScheduleForInstall( ie );
        }
        else
        {
            delete ie;
        }    
    }
    
    if (qualifiedRegName != nsnull)
        delete qualifiedRegName;

    if (newVersion != nsnull)
        delete newVersion;

    *aReturn = SaveError( errcode );
    return NS_OK;
}

PRInt32    
nsInstall::DeleteComponent(const nsString& aRegistryName, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }

    
    nsString* qualifiedRegName = GetQualifiedRegName( aRegistryName);
    
    if (qualifiedRegName == nsnull)
    {
        *aReturn = SaveError( BAD_PACKAGE_NAME );
        return NS_OK;
    }
    
    nsInstallDelete* id = new nsInstallDelete(this, "", *qualifiedRegName, &result);
    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( id );
    }
    
    delete qualifiedRegName;

    *aReturn = SaveError(result);

    return NS_OK;
}

PRInt32    
nsInstall::DeleteFile(const nsString& aFolder, const nsString& aRelativeFileName, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
   
    nsInstallDelete* id = new nsInstallDelete(this, aFolder, aRelativeFileName, &result);

    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( id );
    }
        
    if (result == nsInstall::FILE_DOES_NOT_EXIST) 
    {
        result = nsInstall::SUCCESS;
    }

    *aReturn = SaveError(result);

    return NS_OK;
}

PRInt32    
nsInstall::DiskSpaceAvailable(const nsString& aFolder, PRInt32* aReturn)
{
    return NS_OK;
}

PRInt32    
nsInstall::Execute(const nsString& aJarSource, const nsString& aArgs, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
   
    nsInstallExecute* ie = new nsInstallExecute(this, aJarSource, aArgs, &result);

    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( ie );
    }
        
    *aReturn = SaveError(result);

    return NS_OK;
}

PRInt32    
nsInstall::FinalizeInstall(PRInt32* aReturn)
{
    PRBool  rebootNeeded = PR_FALSE;

    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
    
    if ( mInstalledFiles == NULL || mInstalledFiles->GetSize() == 0 ) 
    {
        // no actions queued: don't register the package version
        // and no need for user confirmation
    
        CleanUp();
        return NS_OK; 
    }

    nsInstallObject* ie = nsnull;

    if ( mUninstallPackage )
    {
        // The Version Registry is not real.  FIX!
        //
        //VR_UninstallCreateNode( nsAutoCString(mRegistryPackageName), nsAutoCString(mUIName));
    }
      
    PRUint32 i=0;
    for (i=0; i < mInstalledFiles->GetSize(); i++) 
    {
        ie = (nsInstallObject*)mInstalledFiles->Get(i);
        if (ie == NULL)
            continue;

        ie->Complete();

        if (result != nsInstall::SUCCESS) 
        {
            ie->Abort();
            *aReturn = SaveError( result );
            return NS_OK;
        }

        //SetProgressDialogThermo(++count);
    }

    *aReturn = NS_OK;
    return NS_OK;
}

PRInt32    
nsInstall::Gestalt(const nsString& aSelector, PRInt32* aReturn)
{
    *aReturn = nsnull;
    return NS_OK;    
}

PRInt32    
nsInstall::GetComponentFolder(const nsString& aComponentName, const nsString& aSubdirectory, nsString** aFolder)
{
    long err;
    char* dir;
    char* componentCString;

    *aFolder = nsnull;

    nsString *tempString = GetQualifiedPackageName( aComponentName );
    
    if (tempString == nsnull)
        return NS_OK;
    
    componentCString = tempString->ToNewCString();
    delete tempString;
      
    dir = (char*)PR_Malloc(MAXREGPATHLEN);
    err = VR_GetDefaultDirectory( componentCString, MAXREGPATHLEN, dir );
    if (err != REGERR_OK)
    {
        PR_FREEIF(dir);
    }


    if ( dir == NULL ) 
    {
        dir = (char*)PR_Malloc(MAXREGPATHLEN);
        err = VR_GetPath( componentCString, MAXREGPATHLEN, dir );
        if (err != REGERR_OK)
        {
            PR_FREEIF(dir);
        }
    
        if ( dir != nsnull ) 
        {
            int i;

            nsString dirStr(dir);
            if (  (i = dirStr.RFind(FILESEP)) > 0 ) 
            {
                PR_FREEIF(dir);  
                dir = (char*)PR_Malloc(i);
                dir = dirStr.ToCString(dir, i);
            }
        }
    }

    if ( dir != NULL ) 
    {
        *aFolder = new nsString(dir);
    }

    PR_FREEIF(dir);
    delete [] componentCString;

    return NS_OK;
}

PRInt32    
nsInstall::GetFolder(const nsString& targetFolder, const nsString& aSubdirectory, nsString** aFolder)
{
    nsInstallFolder* spec = nsnull;
    *aFolder = nsnull;

    spec = new nsInstallFolder(targetFolder, aSubdirectory);   
       
    nsString dirString;
    spec->GetDirectoryPath(dirString);

    *aFolder = new nsString(dirString);
    

    return NS_OK;    
}

PRInt32    
nsInstall::GetLastError(PRInt32* aReturn)
{
    *aReturn = mLastError;
    return NS_OK;
}

PRInt32    
nsInstall::GetWinProfile(const nsString& aFolder, const nsString& aFile, PRInt32* aReturn)
{
    return NS_OK;
}

PRInt32    
nsInstall::GetWinRegistry(PRInt32* aReturn)
{
    return NS_OK;
}

PRInt32    
nsInstall::Patch(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, const nsString& aFolder, const nsString& aTargetName, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }

    nsString* qualifiedRegName = GetQualifiedRegName( aRegName );
    
    if (qualifiedRegName == nsnull)
    {
        *aReturn = SaveError( nsInstall::BAD_PACKAGE_NAME );
        return NS_OK;
    }

    nsInstallVersion *newVersion = new nsInstallVersion();
    newVersion->Init(aVersion);

    nsInstallPatch* ip = new nsInstallPatch( this,
                                             *qualifiedRegName,
                                             newVersion,
                                             aJarSource,
                                             &result);
    

    delete newVersion;

    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( ip );
    }
        
    *aReturn = SaveError(result);

    return NS_OK;
}

PRInt32    
nsInstall::ResetError()
{
    mLastError = nsInstall::SUCCESS;
    return NS_OK;
}


PRInt32    
nsInstall::SetPackageFolder(const nsString& aFolder)
{
    if (mPackageFolder != nsnull)
        delete mPackageFolder;

    mPackageFolder = new nsString(aFolder);

    return NS_OK;
}

/**
 * Call this to initialize the update
 * Opens the jar file and gets the certificate of the installer
 * Opens up the gui, and asks for proper security privileges
 *
 * @param aUserPackageName   
 *
 * @param aPackageName      Full qualified  version registry name of the package
 *                          (ex: "/Plugins/Adobe/Acrobat")
 *                          NULL or empty package names are errors
 *
 * @param inVInfo           version of the package installed.
 *                          Can be NULL, in which case package is installed
 *                          without a version. Having a NULL version, this 
 *                          package is automatically updated in the future 
 *                          (ie. no version check is performed).
 *
 * @param flags             Once was securityLevel(LIMITED_INSTALL or FULL_INSTALL).  Now
 *                          can be either NO_STATUS_DLG or NO_FINALIZE_DLG
 */
PRInt32    
nsInstall::StartInstall(const nsString& aUserPackageName, const nsString& aPackageName, const nsString& aVersion, PRInt32 aFlags, PRInt32* aReturn)
{
    *aReturn     = nsInstall::SUCCESS;
    
    ResetError();
        
    mUserCancelled = PR_FALSE; 
    
    mUIName = aUserPackageName;

    if ( aPackageName.Equals("") ) 
    {
        *aReturn = nsInstall::INVALID_ARGUMENTS;
        return NS_OK;
    }
    
    nsString *tempString = GetQualifiedPackageName( aPackageName );
    
    mRegistryPackageName.SetLength(0);
    mRegistryPackageName.Append( *tempString );  
    
    delete tempString;

    /* Check to see if the PackageName ends in a '/'.  If it does nuke it. */

    if (mRegistryPackageName.Last() == '/')
    {
        PRInt32 index = mRegistryPackageName.Length();
        mRegistryPackageName.Truncate(--index);
    }
    
    if (mVersionInfo != nsnull)
        delete mVersionInfo;

    mVersionInfo    = new nsInstallVersion();
    mVersionInfo->Init(aVersion);

    mInstalledFiles = new nsVector();
    mPatchList      = new nsHashtable();

    /* this function should also check security!!! */
    *aReturn = OpenJARFile();

    if (*aReturn != nsInstall::SUCCESS)
    {
        /* if we can not continue with the javascript return a JAR error*/
        return -1;  /* FIX: need real error code */
    }
 
    /* Show our window here */
    
    SaveError(*aReturn);
    
    if (*aReturn != nsInstall::SUCCESS)
    {
        mRegistryPackageName = ""; // Reset!
    }

    return NS_OK;
}

PRInt32    
nsInstall::Uninstall(const nsString& aPackageName, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }

    nsString* qualifiedPackageName = GetQualifiedPackageName( aPackageName );
    
    if (qualifiedPackageName == nsnull)
    {
        *aReturn = SaveError( BAD_PACKAGE_NAME );
        return NS_OK;
    }
// Create UNinstall object here FIX

    delete qualifiedPackageName;
    
    if (result == SUCCESS) 
    {
//        result = ScheduleForInstall( ip );
    }
        
    *aReturn = SaveError(result);

    return NS_OK;
}

////////////////////////////////////////


void       
nsInstall::AddPatch(nsHashKey *aKey, nsFileSpec* fileName)
{
    if (mPatchList != nsnull)
    {
        mPatchList->Put(aKey, fileName);
    }
}

void       
nsInstall::GetPatch(nsHashKey *aKey, nsFileSpec* fileName)
{
    if (mPatchList != nsnull)
    {
        fileName = (nsFileSpec*) mPatchList->Get(aKey);
    }
}

/////////////////////////////////////////////////////////////////////////
// Private Methods
/////////////////////////////////////////////////////////////////////////

/**
 * ScheduleForInstall
 * call this to put an InstallObject on the install queue
 * Do not call installedFiles.addElement directly, because this routine also 
 * handles progress messages
 */
PRInt32 
nsInstall::ScheduleForInstall(nsInstallObject* ob)
{
    PRInt32 error = nsInstall::SUCCESS;

    char *objString = ob->toString();

    // flash current item
    //SetProgressDialogItem( objString );

    PR_FREEIF(objString);
    
    // do any unpacking or other set-up
    error = ob->Prepare();
    
    if (error != nsInstall::SUCCESS) 
        return error;
    
    
    // Add to installation list if we haven't thrown out
    
    mInstalledFiles->Add( ob );

    // turn on flags for creating the uninstall node and
    // the package node for each InstallObject
    
    if (ob->CanUninstall())
        mUninstallPackage = PR_TRUE;
	
    if (ob->RegisterPackageNode())
        mRegisterPackage = PR_TRUE;
  
  return nsInstall::SUCCESS;
}


    
/**
 * SanityCheck
 *
 * This routine checks if the packageName is null. It also checks the flag if the user cancels
 * the install progress dialog is set and acccordingly aborts the install.
 */
PRInt32
nsInstall::SanityCheck(void)
{
    if ( mRegistryPackageName == "" || mUIName == "") 
    {
        return INSTALL_NOT_STARTED;	
    }

    if (mUserCancelled) 
    {
        AbortInstall();
        SaveError(USER_CANCELLED);
        return USER_CANCELLED;
    }
	
	return 0;
}

/**
 * GetQualifiedPackageName
 *
 * This routine converts a package-relative component registry name
 * into a full name that can be used in calls to the version registry.
 */

nsString *
nsInstall::GetQualifiedPackageName( const nsString& name )
{
    nsString* qualifedName = nsnull;
    
    /* this functions is really messed up.  The original checkin is messed as well */

    if ( name.Equals( "=USER=/") )
    {
        qualifedName = CurrentUserNode();
        qualifedName->Insert( name, 7 );
    }
    else
    {
        qualifedName = new nsString(name);
    }
    
    if (BadRegName(qualifedName)) 
    {
        if (qualifedName != nsnull)
        {
            delete qualifedName;
            qualifedName = nsnull;
        }
    }
    return qualifedName;
}


/**
 * GetQualifiedRegName
 *
 * Allocates a new string and returns it. Caller is supposed to free it
 *
 * This routine converts a package-relative component registry name
 * into a full name that can be used in calls to the version registry.
 */
nsString *
nsInstall::GetQualifiedRegName(const nsString& name )
{
    nsString *qualifiedRegName;

    nsString comm("=COMM=/");
    nsString usr ("=USER=/");

    if ( name.Compare(comm, PR_TRUE, comm.Length()) == 0 ) 
    {
        qualifiedRegName = new nsString( name );
        qualifiedRegName->Cut( 0, comm.Length() );
    }
    else if ( name.Compare(usr, PR_TRUE, usr.Length()) == 0 ) 
    {
        qualifiedRegName = new nsString( name );
        qualifiedRegName->Cut( 0, usr.Length() );
    }
    else if ( name[0] != '/' )
    {
        if (mUIName != "")
        {
            qualifiedRegName = new nsString(mUIName);
            qualifiedRegName->Append("/");
            qualifiedRegName->Append(name);
        }
        else
        {
            qualifiedRegName = new nsString(name);
        }
    }
    else
    {
        qualifiedRegName = new nsString(name);
    }

    if (BadRegName(qualifiedRegName)) 
    {
        delete qualifiedRegName;
        qualifiedRegName = NULL;
    }
  
    return qualifiedRegName;
}



nsString*
nsInstall::CurrentUserNode()
{
    nsString *qualifedName;
    nsString *profileName;
    
    char *profname;
    int len = MAXREGNAMELEN;
    int err;

    profname = (char*) PR_Malloc(len);

    err = PREF_GetCharPref( "profile.name", profname, &len );

    if ( err != PREF_OK )
    {
        PR_FREEIF(profname);
        profname = NULL;
    }
    
    profileName  = new nsString(profname);
    qualifedName = new nsString("/Netscape/Users/");
    
    qualifedName->Append(*profileName);
    qualifedName->Append("/");

    if (profileName != nsnull)
        delete profileName;

    return qualifedName;
}

// catch obvious registry name errors proactively
// rather than returning some cryptic libreg error
PRBool 
nsInstall::BadRegName(nsString* regName)
{
    if (regName == nsnull)
        return PR_TRUE;
    
    if ((regName->First() == ' ' ) || (regName->Last() == ' ' ))
        return PR_TRUE;
        
    if ( regName->Find("//") != -1 )
        return PR_TRUE;
     
    if ( regName->Find(" /") != -1 )
        return PR_TRUE;

    if ( regName->Find("/ ") != -1  )
        return PR_TRUE;        
    
    if ( regName->Find("=") != -1 )
        return PR_TRUE;           

    return PR_FALSE;
}

PRInt32    
nsInstall::SaveError(PRInt32 errcode)
{
  if ( errcode != nsInstall::SUCCESS ) 
    mLastError = errcode;
  
  return errcode;
}

/*
 * CleanUp
 * call	it when	done with the install
 *
 * XXX: This is a synchronized method. FIX it.
 */
void 
nsInstall::CleanUp(void)
{
    nsInstallObject* ie;
    CloseJARFile();
    
    if ( mInstalledFiles != NULL ) 
    {
        PRUint32 i=0;
        for (; i < mInstalledFiles->GetSize(); i++) 
        {
            ie = (nsInstallObject*)mInstalledFiles->Get(i);
            delete (ie);
        }

        mInstalledFiles->RemoveAll();
        delete (mInstalledFiles);
        mInstalledFiles = nsnull;
    }

    if (mPatchList)
    {
        // do I need to delete every entry?
        delete mPatchList;
    }
    
    mRegistryPackageName = ""; // used to see if StartInstall() has been called

    //CloseProgressDialog();
}


void       
nsInstall::GetJarFileLocation(nsString& aFile)
{
    aFile.SetString( mJarFileLocation );
}

void       
nsInstall::SetJarFileLocation(const nsString& aFile)
{
    mJarFileLocation.SetString(aFile);
}

void       
nsInstall::GetInstallArguments(nsString& args)
{
    args.SetString( mInstallArguments );
}

void       
nsInstall::SetInstallArguments(const nsString& args)
{
    mInstallArguments.SetString(args);
}



PRInt32 
nsInstall::OpenJARFile(void)
{    
    
    PRInt32 result = ZIP_OpenArchive(nsAutoCString(mJarFileLocation),  &mJarFileData);
    
    return result;
}

void
nsInstall::CloseJARFile(void)
{
    ZIP_CloseArchive(&mJarFileData);
    mJarFileData = nsnull;
}


// aJarFile         - This is the filepath within the jar file.
// aSuggestedName   - This is the name that we should try to extract to.  If we can, we will create a new temporary file.
// aRealName        - This is the name that we did extract to.  This will be allocated by use and should be disposed by the caller.

PRInt32    
nsInstall::ExtractFileFromJar(const nsString& aJarfile, nsFileSpec* aSuggestedName, nsFileSpec** aRealName)
{
    PRInt32 result;
    nsFileSpec *extractHereSpec;

    nsSpecialSystemDirectory tempFile(nsSpecialSystemDirectory::OS_TemporaryDirectory);
        
    if (aSuggestedName == nsnull || aSuggestedName->Exists() )
    {
        nsString tempfileName = "xpinstall";
         
        // Get the extention of the file in the jar.
        
        PRInt32 result = aJarfile.RFind('.');
        if (result != -1)
        {            
            // We found an extention.  Add it to the tempfileName string
            nsString extention;
            aJarfile.Right(extention, (aJarfile.Length() - result) );        
            tempfileName += extention;
        }
         
        tempFile += tempfileName;
         
        // Create a temporary file to extract to.
        tempFile.MakeUnique();
        
        extractHereSpec = new nsFileSpec(tempFile);
    }
    else
    {
        // extract to the final destination.
        extractHereSpec = new nsFileSpec(*aSuggestedName);
    }

    // FIX:  We will overwrite what is in the way.  is this something that we want to do?  
    extractHereSpec->Delete(PR_FALSE);

    result  = ZIP_ExtractFile( mJarFileData, nsAutoCString(aJarfile), extractHereSpec->GetCString() );
    
    if (result == 0)
    {
        *aRealName = extractHereSpec;
    }
    else
    {
        if (extractHereSpec != nsnull)
            delete extractHereSpec;
    }
    return result;
}


PRInt32 
nsInstall::ExtractDirEntries(const nsString& directory, nsVector *paths)
{
    return nsInstall::SUCCESS;
}
