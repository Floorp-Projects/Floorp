/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *     Pierre Phaneuf <pp@ludusdesign.com>
 */



#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"

#include "nsHashtable.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"

#include "nsIPref.h"

#include "prmem.h"
#include "plstr.h"
#include "prprf.h"

#include "VerReg.h"

#include "nsInstall.h"
#include "nsInstallFolder.h"
#include "nsInstallVersion.h"
#include "nsInstallFile.h"
#include "nsInstallDelete.h"
#include "nsInstallExecute.h"
#include "nsInstallPatch.h"
#include "nsInstallUninstall.h"
#include "nsInstallResources.h"
#include "nsNetUtil.h"

#include "nsProxiedService.h"
#include "nsINetSupportDialogService.h"
#include "nsIPrompt.h"

#ifdef _WINDOWS
#include "nsWinReg.h"
#include "nsWinProfile.h"
#endif

#include "nsInstallFileOpEnums.h"
#include "nsInstallFileOpItem.h"

#ifdef XP_MAC
#include "Gestalt.h"
#include "nsAppleSingleDecoder.h"
#endif 

#include "nsILocalFile.h"

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);

static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);
	
#define XPINSTALL_BUNDLE_URL "chrome://xpinstall/locale/xpinstall.properties"

// filename length maximums
#ifdef XP_MAC
#define MAX_NAME_LENGTH 31
#elif defined (XP_PC)
#define MAX_NAME_LENGTH 128
#elif defined (XP_UNIX)
#define MAX_NAME_LENGTH 1024 //got this one from nsComponentManager.h
#elif defined (XP_BEOS)
#define MAX_NAME_LENGTH MAXNAMLEN
#endif

MOZ_DECL_CTOR_COUNTER(nsInstallInfo);

nsInstallInfo::nsInstallInfo(nsIFileSpec*     aFile, 
                             const PRUnichar* aURL,
                             const PRUnichar* aArgs, 
                             long             flags, 
                             nsIXPINotifier*  aNotifier)
: mError(0), 
  mFlags(flags),
  mURL(aURL),
  mArgs(aArgs), 
  mFile(aFile), 
  mNotifier(aNotifier)
{
    MOZ_COUNT_CTOR(nsInstallInfo);
}



nsInstallInfo::~nsInstallInfo()
{
  MOZ_COUNT_DTOR(nsInstallInfo);
}

nsresult
nsInstallInfo::GetLocalFile(nsFileSpec& aSpec)
{
    if (!mFile)
       return NS_ERROR_NULL_POINTER;
       
    return mFile->GetFileSpec(&aSpec);
}




static NS_DEFINE_IID(kISoftwareUpdateIID, NS_ISOFTWAREUPDATE_IID);
static NS_DEFINE_IID(kSoftwareUpdateCID,  NS_SoftwareUpdate_CID);

static NS_DEFINE_IID(kIZipReaderIID, NS_IZIPREADER_IID);
static NS_DEFINE_IID(kZipReaderCID,  NS_ZIPREADER_CID);

MOZ_DECL_CTOR_COUNTER(nsInstall);

nsInstall::nsInstall(nsIZipReader * theJARFile)
{
    MOZ_COUNT_CTOR(nsInstall);

    mScriptObject           = nsnull;           // this is the jsobject for our context
    mVersionInfo            = nsnull;           // this is the version information passed to us in StartInstall()
    mInstalledFiles         = nsnull;           // the list of installed objects
    mRegistryPackageName    = "";               // this is the name that we will add into the registry for the component we are installing
    mUIName                 = "";               // this is the name that will be displayed in UI.
    mPatchList              = nsnull;
    mUninstallPackage       = PR_FALSE;
    mRegisterPackage        = PR_FALSE;
    mStatusSent             = PR_FALSE;
    mStartInstallCompleted  = PR_FALSE;
    mJarFileLocation        = "";
    mInstallArguments       = "";
    mPackageFolder          = nsnull;

    // mJarFileData is an opaque handle to the jarfile.
    mJarFileData = theJARFile;

    nsISoftwareUpdate *su;
    nsresult rv = nsServiceManager::GetService(kSoftwareUpdateCID, 
                                               kISoftwareUpdateIID,
                                               (nsISupports**) &su);
    
    if (NS_SUCCEEDED(rv))
    {
        su->GetMasterNotifier( &mNotifier );
    }

    su->Release();

    // get the resourced xpinstall string bundle
    mStringBundle = nsnull;
    nsIStringBundleService *service;
    rv = nsServiceManager::GetService( kStringBundleServiceCID, 
                                       NS_GET_IID(nsIStringBundleService),
                                       (nsISupports**) &service );
    if (NS_SUCCEEDED(rv) && service)
    {
        nsILocale* locale = nsnull;
        rv = service->CreateBundle( XPINSTALL_BUNDLE_URL, locale,
                                    getter_AddRefs(mStringBundle) );
        nsServiceManager::ReleaseService( kStringBundleServiceCID, service );
    }
}

nsInstall::~nsInstall()
{
    if (mVersionInfo != nsnull)
        delete mVersionInfo;

    if (mPackageFolder)
        delete mPackageFolder;

    MOZ_COUNT_DTOR(nsInstall);
}

PRInt32
nsInstall::SetScriptObject(void *aScriptObject)
{
  mScriptObject = (JSObject*) aScriptObject;
  return NS_OK;
}

#ifdef _WINDOWS
PRInt32
nsInstall::SaveWinRegPrototype(void *aScriptObject)
{
  mWinRegObject = (JSObject*) aScriptObject;
  return NS_OK;
}
PRInt32
nsInstall::SaveWinProfilePrototype(void *aScriptObject)
{
  mWinProfileObject = (JSObject*) aScriptObject;
  return NS_OK;
}

JSObject*
nsInstall::RetrieveWinRegPrototype()
{
  return mWinRegObject;
}

JSObject*
nsInstall::RetrieveWinProfilePrototype()
{
  return mWinProfileObject;
}
#endif


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

void
nsInstall::InternalAbort(PRInt32 errcode)
{
    if (mNotifier)
    {
        mNotifier->FinalStatus(mInstallURL.GetUnicode(), errcode);
        mStatusSent = PR_TRUE;
    }

    nsInstallObject* ie;
    if (mInstalledFiles != nsnull) 
    {
        for (PRInt32 i=mInstalledFiles->Count() - 1; i >= 0; i--) 
        {
            ie = (nsInstallObject *)mInstalledFiles->ElementAt(i);
            if (ie) 
                ie->Abort();
        }
    }
    
    CleanUp();
}

PRInt32    
nsInstall::AbortInstall(PRInt32 aErrorNumber)
{
    InternalAbort(aErrorNumber);
    return NS_OK;
}

PRInt32    
nsInstall::AddDirectory(const nsString& aRegName, 
                        const nsString& aVersion, 
                        const nsString& aJarSource, 
                        nsInstallFolder *aFolder,
                        const nsString& aSubdir, 
                        PRInt32 aMode, 
                        PRInt32* aReturn)
{
    nsInstallFile* ie = nsnull;
    PRInt32 result;
    
    if ( aJarSource.Equals("") || aFolder == nsnull ) 
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
    
    nsString qualifiedRegName;
    
    if ( aRegName.Equals("")) 
    {
        // Default subName = location in jar file
        *aReturn = GetQualifiedRegName( aJarSource, qualifiedRegName);
    } 
    else 
    {
        *aReturn = GetQualifiedRegName( aRegName, qualifiedRegName );
    }

    if (*aReturn != SUCCESS)
    {
        return NS_OK;
    }
    
    nsString qualifiedVersion = aVersion;
    if (qualifiedVersion == "")
    {
        // assume package version for overriden forms that don't take version info
        *aReturn = mVersionInfo->ToString(qualifiedVersion);

        if (NS_FAILED(*aReturn))
        {
            SaveError( nsInstall::UNEXPECTED_ERROR );
            return NS_OK;
        }
    }
	
    nsString subdirectory(aSubdir);

    if (subdirectory != "")
    {
        subdirectory.Append("/");
    }

    
    nsVoidArray *paths = new nsVoidArray();
    
    if (paths == nsnull)
    {
        *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
        return NS_OK;
    }

    result = ExtractDirEntries(aJarSource, paths);
    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
    
    PRInt32 count = paths->Count();
    
    if (count == 0)
    {
        *aReturn = SaveError( nsInstall::DOES_NOT_EXIST );
        return NS_OK;
    }

    for (PRInt32 i=0; i < count; i++)
    {
        nsString *thisPath = (nsString *)paths->ElementAt(i);

        nsString newJarSource = aJarSource;
        newJarSource += "/";
        newJarSource += *thisPath;
        
        nsString fullRegName = qualifiedRegName;
        fullRegName += "/";
        fullRegName += *thisPath;
        

        nsString newSubDir;

        if (subdirectory != "")
        {
            newSubDir = subdirectory;
        }
        
        newSubDir += *thisPath;

        ie = new nsInstallFile( this,
                                fullRegName,
                                qualifiedVersion,
                                newJarSource,
                                aFolder,
                                newSubDir,
                                aMode,
                                &result);
        
        if (ie == nsnull)
        {
            *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
                return NS_OK;
        }

        if (result == nsInstall::SUCCESS)
        {
            result = ScheduleForInstall( ie );
        }
        else
        {
            delete ie;
        }
    
    }
    
    DeleteVector(paths);

    *aReturn = SaveError( result );
    return NS_OK;
}

PRInt32    
nsInstall::AddDirectory(const nsString& aRegName, 
                        const nsString& aVersion, 
                        const nsString& aJarSource, 
                        nsInstallFolder *aFolder,
                        const nsString& aSubdir, 
                        PRInt32* aReturn)
{
    return AddDirectory(aRegName, 
                        aVersion, 
                        aJarSource, 
                        aFolder, 
                        aSubdir, 
                        INSTALL_NO_COMPARE,
                        aReturn);
}

PRInt32    
nsInstall::AddDirectory(const nsString& aRegName, 
                        const nsString& aJarSource, 
                        nsInstallFolder *aFolder,
                        const nsString& aSubdir, 
                        PRInt32* aReturn)
{
    return AddDirectory(aRegName, 
                        "", 
                        aJarSource, 
                        aFolder, 
                        aSubdir, 
                        INSTALL_NO_COMPARE,
                        aReturn);
}

PRInt32    
nsInstall::AddDirectory(const nsString& aJarSource,
                        PRInt32* aReturn)
{
    if(mPackageFolder == nsnull)
    {
        *aReturn = SaveError( nsInstall::PACKAGE_FOLDER_NOT_SET );
        return NS_OK;
    }
    
    return AddDirectory("", 
                        "", 
                        aJarSource, 
                        mPackageFolder, 
                        "", 
                        INSTALL_NO_COMPARE,
                        aReturn);
}

PRInt32    
nsInstall::AddSubcomponent(const nsString& aRegName, 
                           const nsString& aVersion, 
                           const nsString& aJarSource, 
                           nsInstallFolder *aFolder, 
                           const nsString& aTargetName, 
                           PRInt32 aMode, 
                           PRInt32* aReturn)
{
    nsInstallFile*  ie;
    nsString        qualifiedRegName;
    nsString        qualifiedVersion = aVersion;
    nsString        tempTargetName   = aTargetName;
    
    PRInt32         errcode = nsInstall::SUCCESS;


    if(aJarSource.Equals("") || aFolder == nsnull ) 
    {
        *aReturn = SaveError( nsInstall::INVALID_ARGUMENTS );
        return NS_OK;
    }

    if(aTargetName.Length() > MAX_NAME_LENGTH)
    {
      *aReturn = SaveError( nsInstall::FILENAME_TOO_LONG );
      return NS_OK;
    }
    
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
    
    if( aTargetName.Equals("") )
      tempTargetName = aJarSource;
    
    if (qualifiedVersion == "")
        qualifiedVersion.Assign("0.0.0.0");   	


    if ( aRegName.Equals("") ) 
    {
        // Default subName = location in jar file
        *aReturn = GetQualifiedRegName( aJarSource, qualifiedRegName);
    } 
    else 
    {
        *aReturn = GetQualifiedRegName( aRegName, qualifiedRegName );
    }

    if (*aReturn != SUCCESS)
    {
        return NS_OK;
    }
    
   
    ie = new nsInstallFile( this, 
                            qualifiedRegName, 
                            qualifiedVersion, 
                            aJarSource,
                            aFolder,
                            tempTargetName, 
                            aMode, 
                            &errcode );
    
    if (ie == nsnull)
    {
        *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
            return NS_OK;
    }

    if (errcode == nsInstall::SUCCESS) 
    {
        errcode = ScheduleForInstall( ie );
    }
    else
    {
        delete ie;
    }    

    *aReturn = SaveError( errcode );
    return NS_OK;
}

PRInt32    
nsInstall::AddSubcomponent(const nsString& aRegName, 
                           const nsString& aVersion, 
                           const nsString& aJarSource, 
                           nsInstallFolder* aFolder, 
                           const nsString& aTargetName, 
                           PRInt32* aReturn)
{
    return AddSubcomponent(aRegName, 
                           aVersion, 
                           aJarSource, 
                           aFolder, 
                           aTargetName, 
                           INSTALL_NO_COMPARE, 
                           aReturn);
}

PRInt32    
nsInstall::AddSubcomponent(const nsString& aRegName, 
                           const nsString& aJarSource, 
                           nsInstallFolder* aFolder,
                           const nsString& aTargetName, 
                           PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }

    nsString version;
    *aReturn = mVersionInfo->ToString(version);

    if (NS_FAILED(*aReturn))
    {
        SaveError( nsInstall::UNEXPECTED_ERROR );
        return NS_OK;
    }
    
    return AddSubcomponent(aRegName, 
                           version, 
                           aJarSource, 
                           aFolder, 
                           aTargetName, 
                           INSTALL_NO_COMPARE, 
                           aReturn);
}

PRInt32    
nsInstall::AddSubcomponent(const nsString& aJarSource,
                           PRInt32* aReturn)
{
    if(mPackageFolder == nsnull)
    {
        *aReturn = SaveError( nsInstall::PACKAGE_FOLDER_NOT_SET );
        return NS_OK;
    }
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
    
    nsString version;
    *aReturn = mVersionInfo->ToString(version);

    if (NS_FAILED(*aReturn))
    {
        SaveError( nsInstall::UNEXPECTED_ERROR );
        return NS_OK;
    }

    return AddSubcomponent("", 
                           version, 
                           aJarSource, 
                           mPackageFolder, 
                           "",
                           INSTALL_NO_COMPARE, 
                           aReturn);
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

    nsString qualifiedRegName;

    *aReturn = GetQualifiedRegName( aRegistryName, qualifiedRegName);
    
    if (*aReturn != SUCCESS)
    {
        return NS_OK;
    }
    
    nsInstallDelete* id = new nsInstallDelete(this, qualifiedRegName, &result);
    
    if (id == nsnull)
    {
        *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
        return NS_OK;
    }

    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( id );
    }
    
    *aReturn = SaveError(result);

    return NS_OK;
}

PRInt32    
nsInstall::DeleteFile(nsInstallFolder* aFolder, const nsString& aRelativeFileName, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
   
    nsInstallDelete* id = new nsInstallDelete(this, aFolder, aRelativeFileName, &result);

    if (id == nsnull)
    {
        *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
        return NS_OK;
    }

    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( id );
    }
        
    if (result == nsInstall::DOES_NOT_EXIST) 
    {
        result = nsInstall::SUCCESS;
    }

    *aReturn = SaveError(result);

    return NS_OK;
}

PRInt32    
nsInstall::DiskSpaceAvailable(const nsString& aFolder, PRInt64* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        double d = SaveError( result );
        LL_L2D(d, *aReturn);
        return NS_OK;
    }
    
    nsFileSpec fsFolder(aFolder);

    *aReturn = fsFolder.GetDiskSpaceAvailable();
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
    
    if (ie == nsnull)
    {
        *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
        return NS_OK;
    }

    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( ie );
    }
        
    *aReturn = SaveError(result);
    return NS_OK;
}

PRInt32    
nsInstall::Execute(const nsString& aJarSource, PRInt32* aReturn)
{
    return Execute(aJarSource, "", aReturn);
}

PRInt32    
nsInstall::FinalizeInstall(PRInt32* aReturn)
{
    PRInt32 result = SUCCESS;
    PRBool  rebootNeeded = PR_FALSE;

    *aReturn = SanityCheck();

    if (*aReturn != nsInstall::SUCCESS)
    {
        SaveError( *aReturn );
        if (mNotifier)
        {
            mNotifier->FinalStatus(mInstallURL.GetUnicode(), *aReturn);
            mStatusSent = PR_TRUE;
        }
        return NS_OK;
    }
    

    if ( mInstalledFiles != NULL && mInstalledFiles->Count() > 0 )
    {
        if ( mUninstallPackage )
        {
            VR_UninstallCreateNode( (char*)(const char*) nsAutoCString(mRegistryPackageName), 
                                    (char*)(const char*) nsAutoCString(mUIName));
        }

        // Install the Component into the Version Registry.
        if (mVersionInfo)
        {
            nsString versionString;
            nsString path;

            mVersionInfo->ToString(versionString);

            if (mPackageFolder)
                mPackageFolder->GetDirectoryPath(path);

            VR_Install( (char*)(const char*)nsAutoCString(mRegistryPackageName), 
                        (char*)(const char*)nsAutoCString(path),   
                        (char*)(const char*)nsAutoCString(versionString), 
                        PR_FALSE );
        }

        nsInstallObject* ie = nsnull;

        for (PRInt32 i=0; i < mInstalledFiles->Count(); i++) 
        {
            ie = (nsInstallObject*)mInstalledFiles->ElementAt(i);
            NS_ASSERTION(ie, "NULL object in install queue!");
            if (ie == NULL)
                continue;

            result = ie->Complete();

            if (result != nsInstall::SUCCESS) 
            {
                if ( result == REBOOT_NEEDED )
                {
                    rebootNeeded = PR_TRUE;
                    result = SUCCESS;
                }
                else
                {
                    InternalAbort( result );
                    break;
                }
            }

            if (mNotifier)
            {
                char *objString = ie->toString();
                if (objString)
                {
                    mNotifier->FinalizeProgress(nsAutoString(objString).GetUnicode(),
                                               (i+1), mInstalledFiles->Count());
                    delete [] objString;
                }
            }
        }

        if ( result == SUCCESS )
        {
            if ( rebootNeeded )
                *aReturn = SaveError( REBOOT_NEEDED );

            // XXX for now all successful installs will trigger an Autoreg.
            // We eventually want to do this only when flagged.
            HREG reg;
            if ( REGERR_OK == NR_RegOpen("", &reg) )
            {
                RKEY xpiRoot;
                REGERR err;
                err = NR_RegAddKey(reg,ROOTKEY_COMMON,XPI_ROOT_KEY,&xpiRoot);
                if ( err == REGERR_OK )
                    NR_RegSetEntryString(reg, xpiRoot, XPI_AUTOREG_VAL, "yes");

                NR_RegClose(reg);
            }
        }
        else
            *aReturn = SaveError( result );

        if (mNotifier)
        {
            mNotifier->FinalStatus(mInstallURL.GetUnicode(), *aReturn);
            mStatusSent = PR_TRUE;
        }
    }
    else
    {
        // no actions queued: don't register the package version
        // and no need for user confirmation

        if (mNotifier)
        {
            mNotifier->FinalStatus(mInstallURL.GetUnicode(), *aReturn);
            mStatusSent = PR_TRUE;
        }
    }

    CleanUp();

    return NS_OK;
}

#ifdef XP_MAC
#define GESTALT_CHAR_CODE(x)          (((unsigned long) ((x[0]) & 0x000000FF)) << 24) \
                                    | (((unsigned long) ((x[1]) & 0x000000FF)) << 16) \
                                    | (((unsigned long) ((x[2]) & 0x000000FF)) << 8)  \
                                    | (((unsigned long) ((x[3]) & 0x000000FF)))
#endif /* XP_MAC */
								
PRInt32    
nsInstall::Gestalt(const nsString& aSelector, PRInt32* aReturn)
{
    *aReturn = nsnull;
    
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
#ifdef XP_MAC
	
    long    response = 0;
    char    selectorChars[4];
    int     i;
    OSErr   err = noErr;
    OSType  selector;
    
    if (aSelector == "")
    {
        return NS_OK;
    }

    for (i=0; i<4; i++)
        selectorChars[i] = aSelector.CharAt(i);
    selector = GESTALT_CHAR_CODE(selectorChars);
    
    err = ::Gestalt(selector, &response);
    
    if (err != noErr)
        *aReturn = err;
    else
        *aReturn = response;
	
#endif    
    return NS_OK;    
}

PRInt32
nsInstall::GetComponentFolder(const nsString& aComponentName, const nsString& aSubdirectory, nsInstallFolder** aNewFolder)
{
    long        err;
    char*       componentCString;
    char        dir[MAXREGPATHLEN];
    nsFileSpec  nsfsDir;


    if(!aNewFolder)
      return INVALID_ARGUMENTS;
    
    *aNewFolder = nsnull;

    nsString tempString;

    if ( GetQualifiedPackageName(aComponentName, tempString) != SUCCESS )
    {
        return NS_OK;
    }

    componentCString = tempString.ToNewCString();

    if((err = VR_GetDefaultDirectory( componentCString, MAXREGPATHLEN, dir )) != REGERR_OK)
    {
        if((err = VR_GetPath( componentCString, MAXREGPATHLEN, dir )) == REGERR_OK)
        {
            int i;

            nsString dirStr(dir);
            if (  (i = dirStr.RFindChar(FILESEP)) > 0 ) 
            {
                // i is the index in the string, not the total number of
                // characters in the string.  ToCString() requires the
                // total number of characters in the string to copy,
                // therefore add 1 to it.
                dirStr.Truncate(i + 1);
                dirStr.ToCString(dir, MAXREGPATHLEN);
            }
        }
        else
        {
            *dir = '\0';
        }
    }
    else
    {
        *dir = '\0';
    }

    if(*dir != '\0') 
    {
      *aNewFolder = new nsInstallFolder(dir, aSubdirectory);
    }

    if (componentCString)
        Recycle(componentCString);
    
    return NS_OK;
}

PRInt32    
nsInstall::GetComponentFolder(const nsString& aComponentName, nsInstallFolder** aNewFolder)
{
    return GetComponentFolder(aComponentName, "", aNewFolder);
}

PRInt32
nsInstall::GetFolder(const nsString& targetFolder, const nsString& aSubdirectory, nsInstallFolder** aNewFolder)
{
    /* This version of GetFolder takes an nsString object as the first param */
    if (!aNewFolder)
        return INVALID_ARGUMENTS;

    *aNewFolder = new nsInstallFolder(targetFolder, aSubdirectory);   
    if (*aNewFolder == nsnull)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

PRInt32    
nsInstall::GetFolder(const nsString& targetFolder, nsInstallFolder** aNewFolder)
{
    /* This version of GetFolder takes an nsString object as the only param */
    return GetFolder(targetFolder, "", aNewFolder);
}

PRInt32
nsInstall::GetFolder( nsInstallFolder& aTargetFolderObj, const nsString& aSubdirectory, nsInstallFolder** aNewFolder )
{
    /* This version of GetFolder takes a nsInstallFolder object as the first param */
    if (!aNewFolder)
        return INVALID_ARGUMENTS;
  
    *aNewFolder = new nsInstallFolder(aTargetFolderObj, aSubdirectory);
    if (*aNewFolder == nsnull)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}




PRInt32    
nsInstall::GetLastError(PRInt32* aReturn)
{
    *aReturn = mLastError;
    return NS_OK;
}

PRInt32    
nsInstall::GetWinProfile(const nsString& aFolder, const nsString& aFile, JSContext* jscontext, JSClass* WinProfileClass, jsval* aReturn)
{
    *aReturn = JSVAL_NULL;
    
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }

#ifdef _WINDOWS
    JSObject*     winProfileObject;
    nsWinProfile* nativeWinProfileObject = new nsWinProfile(this, aFolder, aFile);
    
    if (nativeWinProfileObject == nsnull)
        return NS_OK;
    
    JSObject*     winProfilePrototype    = this->RetrieveWinProfilePrototype();
    
    winProfileObject = JS_NewObject(jscontext, WinProfileClass, winProfilePrototype, NULL);
    if(winProfileObject == NULL)
        return NS_OK;
    
    JS_SetPrivate(jscontext, winProfileObject, nativeWinProfileObject);

    *aReturn = OBJECT_TO_JSVAL(winProfileObject);
#endif /* _WINDOWS */

    return NS_OK;
}

PRInt32    
nsInstall::GetWinRegistry(JSContext* jscontext, JSClass* WinRegClass, jsval* aReturn)
{
    *aReturn = JSVAL_NULL;
    
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }

#ifdef _WINDOWS
    JSObject* winRegObject;
    nsWinReg* nativeWinRegObject = new nsWinReg(this);
    
    if (nativeWinRegObject == nsnull)
        return NS_OK;

    JSObject* winRegPrototype    = this->RetrieveWinRegPrototype();

    winRegObject = JS_NewObject(jscontext, WinRegClass, winRegPrototype, NULL);
    if(winRegObject == NULL)
        return NS_OK;
 
    JS_SetPrivate(jscontext, winRegObject, nativeWinRegObject);

    *aReturn = OBJECT_TO_JSVAL(winRegObject);
#endif /* _WINDOWS */

    return NS_OK;
}

PRInt32
nsInstall::LoadResources(JSContext* cx, const nsString& aBaseName, jsval* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
    nsresult ret;
    nsFileSpec* resFile;
    nsFileURL* resFileURL = nsnull;
    nsIURI *url = nsnull;
    nsILocale* locale = nsnull;
    nsIStringBundleService* service = nsnull;
    nsIEventQueueService* pEventQueueService = nsnull;
    nsIStringBundle* bundle = nsnull;
    nsIBidirectionalEnumerator* propEnum = nsnull;
    *aReturn = JSVAL_NULL;
    jsval v = JSVAL_NULL;

    // set up JSObject to return
    JS_GetProperty( cx, JS_GetGlobalObject( cx ), "Object", &v );
    if (!v)
    {
        return NS_ERROR_NULL_POINTER;
    }
    JSClass *objclass = JS_GetClass( cx, JSVAL_TO_OBJECT(v) );
    JSObject *res = JS_NewObject( cx, objclass, JSVAL_TO_OBJECT(v), 0 );

    // extract properties file
    // XXX append locale info: lang code, country code, .properties suffix to aBaseName
    PRInt32 err = ExtractFileFromJar(aBaseName, nsnull, &resFile);
    if ( (!resFile) || (err != nsInstall::SUCCESS)  )
    {
        SaveError( err );
        return NS_OK;
    }
	
    // initialize string bundle and related services
    ret = nsServiceManager::GetService(kStringBundleServiceCID, 
                    kIStringBundleServiceIID, (nsISupports**) &service);
    if (NS_FAILED(ret)) 
        goto cleanup;
    ret = nsServiceManager::GetService(kEventQueueServiceCID,
                    kIEventQueueServiceIID, (nsISupports**) &pEventQueueService);
    if (NS_FAILED(ret)) 
        goto cleanup;
    ret = pEventQueueService->CreateThreadEventQueue();
    if (NS_FAILED(ret)) 
        goto cleanup;

    // construct properties file URL as required by StringBundle interface
    resFileURL = new nsFileURL( *resFile );
    ret = NS_NewURI(&url, resFileURL->GetURLString());
    if (resFileURL)
        delete resFileURL;
    if (NS_FAILED(ret)) 
        goto cleanup;

    // get the string bundle using the extracted properties file
#if 1
    {
      char* spec = nsnull;
      ret = url->GetSpec(&spec);
      if (NS_FAILED(ret)) {
        printf("cannot get url spec\n");
        nsServiceManager::ReleaseService(kStringBundleServiceCID, service);
        nsCRT::free(spec);
        return ret;
      }
      ret = service->CreateBundle(spec, locale, &bundle);
      nsCRT::free(spec);
    }
#else
    ret = service->CreateBundle(url, locale, &bundle);
#endif
    if (NS_FAILED(ret)) 
        goto cleanup;
    ret = bundle->GetEnumeration(&propEnum);
    if (NS_FAILED(ret))
        goto cleanup;

    // set the variables of the JSObject to return using the StringBundle's
    // enumeration service
    ret = propEnum->First();
    if (NS_FAILED(ret))
        goto cleanup;
    while (NS_SUCCEEDED(ret))
    {
        nsIPropertyElement* propElem = nsnull;
        ret = propEnum->CurrentItem((nsISupports**)&propElem);
        if (NS_FAILED(ret))
            goto cleanup;
        nsString* key = nsnull;
        nsString* val = nsnull;
        ret = propElem->GetKey(&key);
        if (NS_FAILED(ret)) 
            goto cleanup;
        ret = propElem->GetValue(&val);
        if (NS_FAILED(ret))
            goto cleanup;
        char* keyCStr = key->ToNewCString();
        PRUnichar* valCStr = val->ToNewUnicode();
        if (keyCStr && valCStr) 
        {
            JSString* propValJSStr = JS_NewUCStringCopyZ(cx, (jschar*) valCStr);
            jsval propValJSVal = STRING_TO_JSVAL(propValJSStr);
            JS_SetProperty(cx, res, keyCStr, &propValJSVal);
            delete[] keyCStr;
            delete[] valCStr;
        }
        if (key)
            delete key;
        if (val)
            delete val;
        ret = propEnum->Next();
    }
	 
    *aReturn = OBJECT_TO_JSVAL(res);
    ret = nsInstall::SUCCESS;

cleanup:
    SaveError( ret );
	
    // release services
    NS_IF_RELEASE( service );
    NS_IF_RELEASE( pEventQueueService );

    // release file, URL, StringBundle, Enumerator
    NS_IF_RELEASE( url );
    NS_IF_RELEASE( bundle );
    NS_IF_RELEASE( propEnum );
    if (resFile)
    {
		// delete the transient properties file
		resFile->Delete(PR_FALSE);
        delete resFile;
    }

    return NS_OK;
}

PRInt32    
nsInstall::Patch(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsInstallFolder* aFolder, const nsString& aTargetName, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }

    nsString qualifiedRegName;
    
    *aReturn = GetQualifiedRegName( aRegName, qualifiedRegName);
    
    if (*aReturn != SUCCESS)
    {
        return NS_OK;
    }

    if (!mPatchList)
    {
        mPatchList = new nsHashtable();
        if (mPatchList == nsnull)
        {
            *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
            return NS_OK;
        }
    }
 
    nsInstallPatch* ip = new nsInstallPatch( this,
                                             qualifiedRegName,
                                             aVersion,
                                             aJarSource,
                                             aFolder,
                                             aTargetName,
                                             &result);
    
    if (ip == nsnull)
    {
        *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
        return NS_OK;
    }

    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( ip );
    }
        
    *aReturn = SaveError(result);
    return NS_OK;
}

PRInt32    
nsInstall::Patch(const nsString& aRegName, const nsString& aJarSource, nsInstallFolder* aFolder, const nsString& aTargetName, PRInt32* aReturn)
{
    return Patch(aRegName, "", aJarSource, aFolder, aTargetName, aReturn);
}

PRInt32    
nsInstall::ResetError()
{
    mLastError = nsInstall::SUCCESS;
    return NS_OK;
}

PRInt32    
nsInstall::SetPackageFolder(nsInstallFolder& aFolder)
{
    if (mPackageFolder)
        delete mPackageFolder;
    
    mPackageFolder = new nsInstallFolder(aFolder, "");

    return NS_OK;
}


PRInt32    
nsInstall::StartInstall(const nsString& aUserPackageName, const nsString& aRegistryPackageName, const nsString& aVersion, PRInt32* aReturn)
{
    if ( aUserPackageName.Length() == 0 )
    {
        // There must be some pretty name for the UI and the uninstall list
        *aReturn = SaveError(INVALID_ARGUMENTS);
        return NS_OK;
    }

    char szRegPackagePath[MAXREGPATHLEN];
    char* szRegPackageName = aRegistryPackageName.ToNewCString();
    
    if (szRegPackageName == nsnull)
    {
        *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
        return nsInstall::OUT_OF_MEMORY;
    }

    *szRegPackagePath = '0';
    *aReturn   = nsInstall::SUCCESS;
    
    ResetError();
        
    mUserCancelled = PR_FALSE; 
     
    mUIName = aUserPackageName;
    
    *aReturn = GetQualifiedPackageName( aRegistryPackageName, mRegistryPackageName );
    
    if (*aReturn != nsInstall::SUCCESS)
    {
        return NS_OK;
    }

    if(REGERR_OK == VR_GetDefaultDirectory(szRegPackageName, MAXREGPATHLEN, szRegPackagePath))
    {
      mPackageFolder = new nsInstallFolder(szRegPackagePath, "");
    }
    else
    {
      mPackageFolder = nsnull;
    }

    if(szRegPackageName)
      Recycle(szRegPackageName);

    if (mVersionInfo != nsnull)
        delete mVersionInfo;

    mVersionInfo    = new nsInstallVersion();
    if (mVersionInfo == nsnull)
    {
        *aReturn = nsInstall::OUT_OF_MEMORY;
        return SaveError(nsInstall::OUT_OF_MEMORY);
    }

    mVersionInfo->Init(aVersion);

    mInstalledFiles = new nsVoidArray();
    
    if (mInstalledFiles == nsnull)
    {
        *aReturn = nsInstall::OUT_OF_MEMORY;
        return SaveError(nsInstall::OUT_OF_MEMORY);
    }

    if (mNotifier)
            mNotifier->InstallStarted(mInstallURL.GetUnicode(), mUIName.GetUnicode());

    mStartInstallCompleted = PR_TRUE;
    return NS_OK;
}


PRInt32    
nsInstall::Uninstall(const nsString& aRegistryPackageName, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
    
    nsString qualifiedPackageName;

    *aReturn = GetQualifiedPackageName( aRegistryPackageName, qualifiedPackageName );
    
    if (*aReturn != SUCCESS)
    {
        return NS_OK;
    }

    nsInstallUninstall *ie = new nsInstallUninstall( this,
                                                     qualifiedPackageName,
                                                     &result );

    if (ie == nsnull)
    {
        *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
        return NS_OK;
    }
    
    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( ie );
    }
    else
    {
        delete ie;
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
nsInstall::GetPatch(nsHashKey *aKey, nsFileSpec** fileName)
{
    if (!fileName) 
        return;
    else
        *fileName = nsnull;

    if (mPatchList != nsnull)
    {
        *fileName = (nsFileSpec*) mPatchList->Get(aKey);
    }
}

PRInt32
nsInstall::FileOpDirCreate(nsInstallFolder& aTarget, PRInt32* aReturn)
{
  nsFileSpec* localFS = new nsFileSpec(*aTarget.GetFileSpec());
  if (localFS == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_DIR_CREATE, *localFS, aReturn);
  if (ifop == nsnull)
  {
    delete localFS;  
    *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
      return NS_OK;
  }

  PRInt32 result = SanityCheck();
  if (result != nsInstall::SUCCESS)
  {
      delete localFS;
      delete ifop;
      *aReturn = SaveError( result );
      return NS_OK;
  }
  
  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
  delete localFS;
  
  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpDirGetParent(nsInstallFolder& aTarget, nsFileSpec* aReturn)
{
  nsFileSpec* localFS = aTarget.GetFileSpec();

  localFS->GetParent(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpDirRemove(nsInstallFolder& aTarget, PRInt32 aFlags, PRInt32* aReturn)
{
  nsFileSpec* localFS = new nsFileSpec(*aTarget.GetFileSpec());
  if (localFS == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_DIR_REMOVE, *localFS, aFlags, aReturn);
  if (ifop == nsnull)
  {
      delete localFS;
      *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
      return NS_OK;
  }

  PRInt32 result = SanityCheck();
  if (result != nsInstall::SUCCESS)
  {
      delete localFS;
      delete ifop;
      *aReturn = SaveError( result );
      return NS_OK;
  }

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
  
  delete localFS;

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpDirRename(nsInstallFolder& aSrc, nsString& aTarget, PRInt32* aReturn)
{
  nsFileSpec* localFS = new nsFileSpec(*aSrc.GetFileSpec());
  if (localFS == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_DIR_RENAME, *localFS, aTarget, aReturn);
  if (ifop == nsnull)
  {
      delete localFS;
      *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
      return NS_OK;
  }

  PRInt32 result = SanityCheck();
  if (result != nsInstall::SUCCESS)
  {
      delete localFS;
      delete ifop;
      *aReturn = SaveError( result );
      return NS_OK;
  }

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
  
  delete localFS;

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileCopy(nsInstallFolder& aSrc, nsInstallFolder& aTarget, PRInt32* aReturn)
{
  nsFileSpec* localSrcFS = new nsFileSpec(*aSrc.GetFileSpec());
  if (localSrcFS == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsFileSpec* localTargetFS = new nsFileSpec(*aTarget.GetFileSpec());
  if (localTargetFS == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_COPY, *localSrcFS, *localTargetFS, aReturn);
  if (ifop == nsnull)
  {
      delete localSrcFS;
      delete localTargetFS;
      *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
      return NS_OK;
  }

  PRInt32 result = SanityCheck();
  if (result != nsInstall::SUCCESS)
  {
      delete localSrcFS;
      delete localTargetFS;
      delete ifop;
      *aReturn = SaveError( result );
      return NS_OK;
  }

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }

  delete localSrcFS;
  delete localTargetFS;
      
  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileDelete(nsInstallFolder& aTarget, PRInt32 aFlags, PRInt32* aReturn)
{
  nsFileSpec* localFS = new nsFileSpec(*aTarget.GetFileSpec());
  if (localFS == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_DELETE, *localFS, aFlags, aReturn);
  if (ifop == nsnull)
  {
      delete localFS;
      *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
      return NS_OK;
  }

  PRInt32 result = SanityCheck();
  if (result != nsInstall::SUCCESS)
  {
      delete localFS;
      delete ifop;
      *aReturn = SaveError( result );
      return NS_OK;
  }

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
  
  delete localFS;

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileExecute(nsInstallFolder& aTarget, nsString& aParams, PRInt32* aReturn)
{
  nsFileSpec* localFS = new nsFileSpec(*aTarget.GetFileSpec());
  if (localFS == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_EXECUTE, *localFS, aParams, aReturn);
  if (ifop == nsnull)
  {
      delete localFS;
      *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
      return NS_OK;
  }

  PRInt32 result = SanityCheck();
  if (result != nsInstall::SUCCESS)
  {
      delete localFS;
      delete ifop;
      *aReturn = SaveError( result );
      return NS_OK;
  }

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
  
  delete localFS;

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileExists(nsInstallFolder& aTarget, PRBool* aReturn)
{
  nsFileSpec* localFS = aTarget.GetFileSpec();

  *aReturn = localFS->Exists();
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileGetNativeVersion(nsInstallFolder& aTarget, nsString* aReturn)
{
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileGetDiskSpaceAvailable(nsInstallFolder& aTarget, PRInt64* aReturn)
{
  nsFileSpec* localFS = aTarget.GetFileSpec();

  *aReturn = localFS->GetDiskSpaceAvailable();
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileGetModDate(nsInstallFolder& aTarget, nsFileSpec::TimeStamp* aReturn)
{
  nsFileSpec* localFS = aTarget.GetFileSpec();

  localFS->GetModDate(*aReturn);
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileGetSize(nsInstallFolder& aTarget, PRUint32* aReturn)
{
  nsFileSpec* localFS = aTarget.GetFileSpec();

  *aReturn = localFS->GetFileSize();
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileIsDirectory(nsInstallFolder& aTarget, PRBool* aReturn)
{
  nsFileSpec* localFS = aTarget.GetFileSpec();

  *aReturn = localFS->IsDirectory();
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileIsFile(nsInstallFolder& aTarget, PRBool* aReturn)
{
  nsFileSpec* localFS = aTarget.GetFileSpec();

  *aReturn = localFS->IsFile();
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileModDateChanged(nsInstallFolder& aTarget, nsFileSpec::TimeStamp& aOldStamp, PRBool* aReturn)
{
  nsFileSpec* localFS = aTarget.GetFileSpec();

  *aReturn = localFS->ModDateChanged(aOldStamp);
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileMove(nsInstallFolder& aSrc, nsInstallFolder& aTarget, PRInt32* aReturn)
{
  nsFileSpec* localSrcFS = new nsFileSpec(*aSrc.GetFileSpec());
  if (localSrcFS == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }
  nsFileSpec* localTargetFS = new nsFileSpec(*aTarget.GetFileSpec());
  if (localTargetFS == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_MOVE, *localSrcFS, *localTargetFS, aReturn);
  if (ifop == nsnull)
  {
      delete localSrcFS;
      delete localTargetFS;
      *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
      return NS_OK;
  }

  PRInt32 result = SanityCheck();
  if (result != nsInstall::SUCCESS)
  {
      delete localSrcFS;
      delete localTargetFS;
      delete ifop;
      *aReturn = SaveError( result );
      return NS_OK;
  }

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }

  delete localSrcFS;
  delete localTargetFS;

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileRename(nsInstallFolder& aSrc, nsString& aTarget, PRInt32* aReturn)
{
  nsFileSpec* localFS = new nsFileSpec(*aSrc.GetFileSpec());
  if (localFS == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_RENAME,*localFS, aTarget, aReturn);
  if (ifop == nsnull)
  {
      delete localFS;
      *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
      return NS_OK;
  }

  PRInt32 result = SanityCheck();
  if (result != nsInstall::SUCCESS)
  {
      delete localFS;
      delete ifop;
      *aReturn = SaveError( result );
      return NS_OK;
  }

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
  
  delete localFS;

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileWindowsShortcut(nsFileSpec& aTarget, nsFileSpec& aShortcutPath, nsString& aDescription, nsFileSpec& aWorkingPath, nsString& aParams, nsFileSpec& aIcon, PRInt32 aIconId, PRInt32* aReturn)
{

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_WIN_SHORTCUT, aTarget, aShortcutPath, aDescription, aWorkingPath, aParams, aIcon, aIconId, aReturn);
  if (ifop == nsnull)
  {
      *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
      return NS_OK;
  }

  PRInt32 result = SanityCheck();
  if (result != nsInstall::SUCCESS)
  {
      delete ifop;
      *aReturn = SaveError( result );
      return NS_OK;
  }

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
      
  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileMacAlias(nsString& aSourcePath, nsString& aAliasPath, PRInt32* aReturn)
{

  *aReturn = nsInstall::SUCCESS;

#ifdef XP_MAC
  nsFileSpec nsfsSource(aSourcePath, PR_FALSE);
  nsFileSpec nsfsAlias(aAliasPath, PR_TRUE);
  
  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_MAC_ALIAS, nsfsSource, nsfsAlias, aReturn);

  PRInt32 result = SanityCheck();
  if (result != nsInstall::SUCCESS)
  {
      *aReturn = SaveError( result );
      return NS_OK;
  }

  if (ifop == nsnull)
  {
      *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
      return NS_OK;
  }

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
      
  SaveError(*aReturn);
#endif

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileUnixLink(nsInstallFolder& aTarget, PRInt32 aFlags, PRInt32* aReturn)
{
  return NS_OK;
}

void
nsInstall::LogComment(nsString& aComment)
{
  if(mNotifier)
    mNotifier->LogComment(aComment.GetUnicode());
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

    if (mNotifier)
        mNotifier->ItemScheduled(nsAutoString(objString).GetUnicode());


    // do any unpacking or other set-up
    error = ob->Prepare();
    
    if (error == nsInstall::SUCCESS) 
    {
        // Add to installation list
        mInstalledFiles->AppendElement( ob );

        // turn on flags for creating the uninstall node and
        // the package node for each InstallObject

        if (ob->CanUninstall())
            mUninstallPackage = PR_TRUE;

        if (ob->RegisterPackageNode())
            mRegisterPackage = PR_TRUE;
    }
    else if ( mNotifier )
    {
        // error in preparation step -- log it
        char* errRsrc = GetResourcedString("ERROR");
        if (errRsrc)
        {
            char* errprefix = PR_smprintf("%s (%d): ", errRsrc, error);
            nsString errstr = errprefix;
            errstr += objString;

            mNotifier->LogComment( errstr.GetUnicode() );

            PR_smprintf_free(errprefix);
            nsCRT::free(errRsrc);
        }   
    }

    if (objString)
        delete [] objString;
    
    return error;
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
    if ( mInstalledFiles == nsnull || mStartInstallCompleted == PR_FALSE ) 
    {
        return INSTALL_NOT_STARTED;	
    }

    if (mUserCancelled) 
    {
        InternalAbort(USER_CANCELLED);
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

PRInt32
nsInstall::GetQualifiedPackageName( const nsString& name, nsString& qualifiedName )
{
    nsString startOfName;
    name.Left(startOfName, 7);

    if ( startOfName.Equals( "=USER=/") )
    {
        CurrentUserNode(qualifiedName);
        qualifiedName += name;
    }
    else
    {
        qualifiedName = name;
    }
    
    if (BadRegName(qualifiedName)) 
    {
        return BAD_PACKAGE_NAME;
    }


    /* Check to see if the PackageName ends in a '/'.  If it does nuke it. */

    if (qualifiedName.Last() == '/')
    {
        PRInt32 index = qualifiedName.Length();
        qualifiedName.Truncate(--index);
    }

    return SUCCESS;
}


/**
 * GetQualifiedRegName
 *
 * This routine converts a package-relative component registry name
 * into a full name that can be used in calls to the version registry.
 */
PRInt32
nsInstall::GetQualifiedRegName(const nsString& name, nsString& qualifiedRegName )
{
    nsString startOfName;
    name.Left(startOfName, 7);

    nsString usr ();

    if ( startOfName.Equals("=COMM=/") || startOfName.Equals("=USER=/")) 
    {
        qualifiedRegName = name;
        qualifiedRegName.Cut( 0, 7 );
    }
    else if ( name.CharAt(0) != '/' )
    {
        if (mRegistryPackageName != "")
        {
            qualifiedRegName = mRegistryPackageName;
            qualifiedRegName += "/";
            qualifiedRegName += name;
        }
        else
        {
            qualifiedRegName = name;
        }
    }
    else
    {
       qualifiedRegName = name;
    }

    if (BadRegName(qualifiedRegName)) 
    {
        return BAD_PACKAGE_NAME;
    }
  
    return SUCCESS;
}


static NS_DEFINE_IID(kPrefsIID, NS_IPREF_IID);
static NS_DEFINE_IID(kPrefsCID,  NS_PREF_CID);

void
nsInstall::CurrentUserNode(nsString& userRegNode)
{    
    char *profname;
    nsIPref * prefs;
    
    nsresult rv = nsServiceManager::GetService(kPrefsCID, 
                                               kPrefsIID,
                                               (nsISupports**) &prefs);


    if ( NS_SUCCEEDED(rv) )
    {
        rv = prefs->CopyCharPref("profile.name", &profname);

        if ( NS_FAILED(rv) )
        {
            PR_FREEIF(profname); // Allocated by PREF_CopyCharPref
            profname = NULL;
        }

        NS_RELEASE(prefs);
    }
    else
    {
        profname = NULL;
    }
    
    userRegNode = "/Netscape/Users/";
    if (profname != nsnull)
    {
        userRegNode += nsString(profname);
        userRegNode += "/";
        PR_FREEIF(profname);
    }
}

// catch obvious registry name errors proactively
// rather than returning some cryptic libreg error
PRBool 
nsInstall::BadRegName(const nsString& regName)
{
    if ((regName.First() == ' ' ) || (regName.Last() == ' ' ))
        return PR_TRUE;
        
    if ( regName.Find("//") != -1 )
        return PR_TRUE;
     
    if ( regName.Find(" /") != -1 )
        return PR_TRUE;

    if ( regName.Find("/ ") != -1  )
        return PR_TRUE;        
    
    if ( regName.Find("=") != -1 )
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
 */
void 
nsInstall::CleanUp(void)
{
    nsInstallObject* ie;
    
    if ( mInstalledFiles != NULL ) 
    {
        for (PRInt32 i=0; i < mInstalledFiles->Count(); i++) 
        {
            ie = (nsInstallObject*)mInstalledFiles->ElementAt(i);
            if (ie)
                delete (ie);
        }

        mInstalledFiles->Clear();
        delete (mInstalledFiles);
        mInstalledFiles = nsnull;
    }

    if (mPatchList != nsnull)
    {
        mPatchList->Reset();
        delete mPatchList;
        mPatchList = nsnull;
    }
    
    if (mPackageFolder != nsnull)
    {
      delete (mPackageFolder);
      mPackageFolder = nsnull;
    }

    mRegistryPackageName = ""; // used to see if StartInstall() has been called
    mStartInstallCompleted = PR_FALSE;
}


void       
nsInstall::GetJarFileLocation(nsString& aFile)
{
    aFile = mJarFileLocation.GetCString();
}

void       
nsInstall::SetJarFileLocation(const nsFileSpec& aFile)
{
    mJarFileLocation = aFile;
}

void       
nsInstall::GetInstallArguments(nsString& args)
{
    args = mInstallArguments;
}

void       
nsInstall::SetInstallArguments(const nsString& args)
{
    mInstallArguments = args;
}


void nsInstall::GetInstallURL(nsString& url)        { url = mInstallURL; }
void nsInstall::SetInstallURL(const nsString& url)  { mInstallURL = url; }


PRInt32    
nsInstall::Alert(nsString& string)
{
    nsresult res;  

    NS_WITH_PROXIED_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, NS_UI_THREAD_EVENTQ, &res);
    if (NS_FAILED(res)) 
        return res;
    
    return dialog->Alert(string.GetUnicode());
}

PRInt32    
nsInstall::Confirm(nsString& string, PRBool* aReturn)
{
    *aReturn = PR_FALSE; /* default value */
    
    nsresult res;  
    NS_WITH_PROXIED_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, NS_UI_THREAD_EVENTQ, &res);
    if (NS_FAILED(res)) 
        return res;
    
    return dialog->Confirm(string.GetUnicode(), aReturn);
}


// aJarFile         - This is the filepath within the jar file.
// aSuggestedName   - This is the name that we should try to extract to.  If we can, we will create a new temporary file.
// aRealName        - This is the name that we did extract to.  This will be allocated by use and should be disposed by the caller.

PRInt32    
nsInstall::ExtractFileFromJar(const nsString& aJarfile, nsFileSpec* aSuggestedName, nsFileSpec** aRealName)
{
    PRInt32 extpos = 0;
    nsFileSpec *extractHereSpec;

    if (aSuggestedName == nsnull)
    {
        nsSpecialSystemDirectory tempFile(nsSpecialSystemDirectory::OS_TemporaryDirectory);
        nsString tempFileName = "xpinstall";

        // Get the extension of the file in the JAR
        extpos = aJarfile.RFindChar('.');
        if (extpos != -1)
        {
            // We found the extension; add it to the tempFileName string
            nsString extension;
            aJarfile.Right(extension, (aJarfile.Length() - extpos) );
            tempFileName += extension;
        }

        tempFile += tempFileName;

        // Create a temporary file to extract to
        tempFile.MakeUnique();

        extractHereSpec = new nsFileSpec(tempFile);

        if (extractHereSpec == nsnull)
            return nsInstall::OUT_OF_MEMORY;
    }
    else
    {
        // extract to the final destination.
        extractHereSpec = new nsFileSpec(*aSuggestedName);
        if (extractHereSpec == nsnull)
            return nsInstall::OUT_OF_MEMORY;

        extractHereSpec->MakeUnique();
    }

    // We will overwrite what is in the way.  is this something that we want to do?  
    extractHereSpec->Delete(PR_FALSE);

    nsresult rv;
    nsCOMPtr<nsILocalFile> file;
    rv = NS_NewLocalFile(*extractHereSpec, getter_AddRefs(file));
    if (NS_SUCCEEDED(rv))
        rv = mJarFileData->Extract(nsAutoCString(aJarfile), file);
    if (NS_FAILED(rv)) 
    {
        if (extractHereSpec != nsnull)
            delete extractHereSpec;
        return EXTRACTION_FAILED;
    }
    
#ifdef XP_MAC
	FSSpec finalSpec, extractedSpec = extractHereSpec->GetFSSpec();
	
	if ( nsAppleSingleDecoder::IsAppleSingleFile(&extractedSpec) )
	{
		nsAppleSingleDecoder *asd = new nsAppleSingleDecoder(&extractedSpec, &finalSpec);
        OSErr decodeErr = fnfErr;

        if (asd)
            decodeErr = asd->Decode();
	
		if (decodeErr != noErr)
		{			
			if (extractHereSpec)
				delete extractHereSpec;
			if (asd)
				delete asd;
			return EXTRACTION_FAILED;
		}
	
		if ( !(extractedSpec.vRefNum == finalSpec.vRefNum) ||
			 !(extractedSpec.parID   == finalSpec.parID)   ||
			 !(nsAppleSingleDecoder::PLstrcmp(extractedSpec.name, finalSpec.name)) )
		{
			// delete the unique extracted file that got renamed in AS decoding
			FSpDelete(&extractedSpec);
			
			// "real name" in AppleSingle entry may cause file rename
			*extractHereSpec = finalSpec;
		}
	}		
#endif

    *aRealName = extractHereSpec;

    return nsInstall::SUCCESS;
}

/**
 * GetResourcedString
 * 
 * Obtains the string resource for actions and messages that are displayed
 * in user interface confirmation and progress dialogs.
 *
 * @param   aResName    - property name/identifier of string resource
 * @return  rscdStr     - corresponding resourced value in the string bundle
 */
char*
nsInstall::GetResourcedString(const nsString& aResName)
{
    nsString rscdStr = "";
    PRBool bStrBdlSuccess = PR_FALSE;

    if (mStringBundle)
    {   
        const PRUnichar *ucResName = aResName.GetUnicode();
        PRUnichar *ucRscdStr = nsnull;
        nsresult rv = mStringBundle->GetStringFromName(ucResName, &ucRscdStr);
        if (NS_SUCCEEDED(rv))
        {
            bStrBdlSuccess = PR_TRUE;
            rscdStr = ucRscdStr;
        }
    }
    
    /*
    ** We don't have a string bundle, the necessary libs, or something went wrong
    ** so we failover to hardcoded english strings so we log something rather
    ** than nothing due to failure above: always the case for the Install Wizards.
    */
    if (!bStrBdlSuccess)
    {
        char *cResName = aResName.ToNewCString();
        rscdStr = nsInstallResources::GetDefaultVal(cResName);
        if (cResName)
            Recycle(cResName);
    }
    
    return rscdStr.ToNewCString();
}


PRInt32 
nsInstall::ExtractDirEntries(const nsString& directory, nsVoidArray *paths)
{
    char                *buf;
    nsISimpleEnumerator *jarEnum = nsnull;
    nsIZipEntry         *currZipEntry = nsnull;

    if ( paths )
    {
        nsString pattern(directory);
        pattern += "/*";
        PRInt32 prefix_length = directory.Length()+1; // account for slash

        nsresult rv = mJarFileData->FindEntries( nsAutoCString(pattern), &jarEnum );
        if (NS_FAILED(rv) || !jarEnum)
            goto handle_err;

        PRBool bMore;
        rv = jarEnum->HasMoreElements(&bMore);
        while (bMore && NS_SUCCEEDED(rv))
        {
            rv = jarEnum->GetNext( (nsISupports**) &currZipEntry );
            if (currZipEntry)
            {
                // expensive 'buf' callee malloc per iteration!
                rv = currZipEntry->GetName(&buf);
                if (NS_FAILED(rv)) 
                    goto handle_err;
                if (buf)
                {
                    PRInt32 namelen = PL_strlen(buf);
                    NS_ASSERTION( prefix_length <= namelen, "Match must be longer than pattern!" );

                    if ( buf[namelen-1] != '/' ) 
                    {
                        // XXX manipulation should be in caller
                        paths->AppendElement( new nsString(buf+prefix_length) );
                    }

                    PR_FREEIF( buf );
                }
                NS_IF_RELEASE(currZipEntry);
            }
            rv = jarEnum->HasMoreElements(&bMore);
        }
    }

    NS_IF_RELEASE(jarEnum);
    return SUCCESS;

handle_err:    
    NS_IF_RELEASE(jarEnum);                         
    NS_IF_RELEASE(currZipEntry); 
    return EXTRACTION_FAILED;
}

void
nsInstall::DeleteVector(nsVoidArray* vector)
{
    if (vector != nsnull)
    {
        for (PRInt32 i=0; i < vector->Count(); i++) 
        {
            nsString* element = (nsString*)vector->ElementAt(i);
            if (element != nsnull)
                delete element;
        }

        vector->Clear();
        delete (vector);
        vector = nsnull;
    }
}
