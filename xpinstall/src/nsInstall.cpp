/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *   Douglas Turner <dougt@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Sean Su <ssu@netscape.com>
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */



#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsNativeCharsetUtils.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsHashtable.h"
#include "nsIFileChannel.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

#include "prmem.h"
#include "plstr.h"
#include "prprf.h"
#include "nsCRT.h"

#include "VerReg.h"

#include "nsInstall.h"
#include "nsInstallFolder.h"
#include "nsInstallVersion.h"
#include "nsInstallFile.h"
#include "nsInstallExecute.h"
#include "nsInstallPatch.h"
#include "nsInstallUninstall.h"
#include "nsInstallResources.h"
#include "nsXPIProxy.h"
#include "nsRegisterItem.h"
#include "nsNetUtil.h"
#include "ScheduledTasks.h"
#include "nsIPersistentProperties2.h"

#include "nsIProxyObjectManager.h"
#include "nsProxiedService.h"

#ifdef _WINDOWS
#include "nsWinReg.h"
#include "nsWinProfile.h"
#endif

#include "nsInstallFileOpEnums.h"
#include "nsInstallFileOpItem.h"

#if defined(XP_MAC) || defined(XP_MACOSX)
#include <Gestalt.h>
#include "nsAppleSingleDecoder.h"
#include "nsILocalFileMac.h"
#endif

#include "nsILocalFile.h"
#include "nsIURL.h"

#if defined(XP_UNIX) || defined(XP_BEOS)
#include <sys/utsname.h>
#endif /* XP_UNIX */

#if defined(XP_WIN)
#include <windows.h>
#endif

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);

#define kInstallLocaleProperties "chrome://global/locale/commonDialogs.properties"

/**
 * Request that XPCOM perform an autoreg at startup. (Used
 * internally by XPI.)  This basically drops a file next to the
 * application.  The next time the application launches, XPCOM
 * sees the file, deletes it and autoregisters components.
 */
static void
NS_SoftwareUpdateRequestAutoReg()
{
  nsresult rv;
  nsCOMPtr<nsIFile> file;

  if (nsSoftwareUpdate::GetProgramDirectory())
    // xpistub case, use target directory instead
    nsSoftwareUpdate::GetProgramDirectory()->Clone(getter_AddRefs(file));
  else
    NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                           getter_AddRefs(file));

  if (!file) {
    NS_WARNING("Getting NS_XPCOM_CURRENT_PROCESS_DIR failed");
    return;
  }

  file->AppendNative(nsDependentCString(".autoreg"));

  rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0666);

  if (NS_FAILED(rv)) {
    NS_WARNING("creating file failed");
    return;
  }
}



MOZ_DECL_CTOR_COUNTER(nsInstallInfo)

nsInstallInfo::nsInstallInfo(PRUint32           aInstallType,
                             nsIFile*           aFile,
                             const PRUnichar*   aURL,
                             const PRUnichar*   aArgs,
                             nsIPrincipal*      aPrincipal,
                             PRUint32           flags,
                             nsIXPIListener*    aListener,
                             nsIXULChromeRegistry* aChromeRegistry)
: mPrincipal(aPrincipal),
  mError(0),
  mType(aInstallType),
  mFlags(flags),
  mURL(aURL),
  mArgs(aArgs),
  mFile(aFile),
  mListener(aListener),
  mChromeRegistry(aChromeRegistry)
{
    MOZ_COUNT_CTOR(nsInstallInfo);
}


nsInstallInfo::~nsInstallInfo()
{
  MOZ_COUNT_DTOR(nsInstallInfo);
}

static NS_DEFINE_IID(kISoftwareUpdateIID, NS_ISOFTWAREUPDATE_IID);
static NS_DEFINE_IID(kSoftwareUpdateCID,  NS_SoftwareUpdate_CID);


MOZ_DECL_CTOR_COUNTER(nsInstall)

nsInstall::nsInstall(nsIZipReader * theJARFile)
{
    MOZ_COUNT_CTOR(nsInstall);

    mScriptObject           = nsnull;           // this is the jsobject for our context
    mVersionInfo            = nsnull;           // this is the version information passed to us in StartInstall()
    mInstalledFiles         = nsnull;           // the list of installed objects
//  mRegistryPackageName    = "";               // this is the name that we will add into the registry for the component we are installing
//  mUIName                 = "";               // this is the name that will be displayed in UI.
    mPatchList              = nsnull;
    mUninstallPackage       = PR_FALSE;
    mRegisterPackage        = PR_FALSE;
    mFinalStatus            = SUCCESS;
    mStartInstallCompleted  = PR_FALSE;
    mJarFileLocation        = nsnull;
    //mInstallArguments       = "";
    mPackageFolder          = nsnull;

    // mJarFileData is an opaque handle to the jarfile.
    mJarFileData = theJARFile;

    nsISoftwareUpdate *su;
    nsresult rv = nsServiceManager::GetService(kSoftwareUpdateCID,
                                               kISoftwareUpdateIID,
                                               (nsISupports**) &su);

    if (NS_SUCCEEDED(rv))
    {
        su->GetMasterListener( getter_AddRefs(mListener) );
    }

    su->Release();

    // get the resourced xpinstall string bundle
    mStringBundle = nsnull;
    NS_WITH_PROXIED_SERVICE( nsIStringBundleService,
                             service,
                             kStringBundleServiceCID,
                             NS_UI_THREAD_EVENTQ,
                             &rv );

    if (NS_SUCCEEDED(rv) && service)
    {
        rv = service->CreateBundle( XPINSTALL_BUNDLE_URL,
                                    getter_AddRefs(mStringBundle) );
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
nsInstall::GetInstallPlatform(nsCString& aPlatform)
{
  if (mInstallPlatform.IsEmpty())
  {
    // Duplicated from mozilla/netwerk/protocol/http/src/nsHTTPHandler.cpp
    // which is not yet available in a wizard install

    // Gather platform.
#if defined(XP_WIN)
    mInstallPlatform = "Windows";
#elif defined(XP_MAC) || defined(XP_MACOSX)
    mInstallPlatform = "Macintosh";
#elif defined (XP_UNIX)
    mInstallPlatform = "X11";
#elif defined(XP_BEOS)
    mInstallPlatform = "BeOS";
#elif defined(XP_OS2)
    mInstallPlatform = "OS/2";
#endif

    mInstallPlatform += "; ";

    // Gather OS/CPU.
#if defined(XP_WIN)
    OSVERSIONINFO info = { sizeof(OSVERSIONINFO) };
    if (GetVersionEx(&info)) {
        if ( info.dwPlatformId == VER_PLATFORM_WIN32_NT ) {
            if (info.dwMajorVersion      == 3) {
                mInstallPlatform += "WinNT3.51";
            }
            else if (info.dwMajorVersion == 4) {
                mInstallPlatform += "WinNT4.0";
            }
            else if (info.dwMajorVersion == 5) {
                mInstallPlatform += "Windows NT 5.0";
            }
            else {
                mInstallPlatform += "WinNT";
            }
        } else if (info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
            if (info.dwMinorVersion == 90)
                mInstallPlatform += "Win 9x 4.90";
            else if (info.dwMinorVersion > 0)
                mInstallPlatform += "Win98";
            else
                mInstallPlatform += "Win95";
        }
    }
#elif defined (XP_UNIX) || defined (XP_BEOS)
    struct utsname name;

    int ret = uname(&name);
    if (ret >= 0) {
       mInstallPlatform +=  (char*)name.sysname;
       mInstallPlatform += ' ';
       mInstallPlatform += (char*)name.release;
       mInstallPlatform += ' ';
       mInstallPlatform += (char*)name.machine;
    }
#elif defined (XP_MAC) || defined (XP_MACOSX)
    mInstallPlatform += "PPC";
#elif defined(XP_OS2)
    ULONG os2ver = 0;
    DosQuerySysInfo(QSV_VERSION_MINOR, QSV_VERSION_MINOR,
                    &os2ver, sizeof(os2ver));
    if (os2ver == 11)
        mInstallPlatform += "2.11";
    else if (os2ver == 30)
        mInstallPlatform += "Warp 3";
    else if (os2ver == 40)
        mInstallPlatform += "Warp 4";
    else if (os2ver == 45)
        mInstallPlatform += "Warp 4.5";
    else
        mInstallPlatform += "Warp ???";
#endif
  }

  aPlatform = mInstallPlatform;
  return NS_OK;
}


void
nsInstall::InternalAbort(PRInt32 errcode)
{
    mFinalStatus = errcode;

    nsInstallObject* ie;
    if (mInstalledFiles != nsnull)
    {
        // abort must work backwards through the list so cleanup can
        // happen in the correct order
        for (PRInt32 i = mInstalledFiles->Count()-1; i >= 0; i--)
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

    if ( aJarSource.IsEmpty() || aFolder == nsnull )
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

    if ( aRegName.IsEmpty())
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
    if (qualifiedVersion.IsEmpty())
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

    if (!subdirectory.IsEmpty())
    {
        subdirectory.AppendLiteral("/");
    }


    nsVoidArray *paths = new nsVoidArray();

    if (paths == nsnull)
    {
        *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
        return NS_OK;
    }

    PRInt32 count = 0;
    result = ExtractDirEntries(aJarSource, paths);
    if (result == nsInstall::SUCCESS)
    {
        count = paths->Count();
        if (count == 0)
            result = nsInstall::DOES_NOT_EXIST;
    }

    for (PRInt32 i=0; i < count && result == nsInstall::SUCCESS; i++)
    {
        nsString *thisPath = (nsString *)paths->ElementAt(i);

        nsString newJarSource = aJarSource;
        newJarSource.AppendLiteral("/");
        newJarSource += *thisPath;

        nsString newSubDir;

        if (!subdirectory.IsEmpty())
        {
            newSubDir = subdirectory;
        }

        newSubDir += *thisPath;

        ie = new nsInstallFile( this,
                                qualifiedRegName,
                                qualifiedVersion,
                                newJarSource,
                                aFolder,
                                newSubDir,
                                aMode,
                                (i == 0), // register the first one only
                                &result);

        if (ie == nsnull)
        {
            result = nsInstall::OUT_OF_MEMORY;
        }
        else if (result != nsInstall::SUCCESS)
        {
            delete ie;
        }
        else
        {
            result = ScheduleForInstall( ie );
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
                        EmptyString(),
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

    return AddDirectory(EmptyString(),
                        EmptyString(),
                        aJarSource,
                        mPackageFolder,
                        EmptyString(),
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


    if(aJarSource.IsEmpty() || aFolder == nsnull )
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

    if( aTargetName.IsEmpty() )
    {
        PRInt32 pos = aJarSource.RFindChar('/');

        if ( pos == kNotFound )
            tempTargetName = aJarSource;
        else
            aJarSource.Right(tempTargetName, aJarSource.Length() - (pos+1));
    }

    if (qualifiedVersion.IsEmpty())
        qualifiedVersion.AssignLiteral("0.0.0.0");


    if ( aRegName.IsEmpty() )
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
                            PR_TRUE,
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

    return AddSubcomponent(EmptyString(),
                           version,
                           aJarSource,
                           mPackageFolder,
                           EmptyString(),
                           INSTALL_NO_COMPARE,
                           aReturn);
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
    nsCOMPtr<nsILocalFile> folder;
    NS_NewLocalFile(aFolder, PR_TRUE, getter_AddRefs(folder));

    result = folder->GetDiskSpaceAvailable(aReturn);
    return NS_OK;
}

PRInt32
nsInstall::Execute(const nsString& aJarSource, const nsString& aArgs, PRBool aBlocking, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }

    nsInstallExecute* ie = new nsInstallExecute(this, aJarSource, aArgs, aBlocking, &result);

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
nsInstall::FinalizeInstall(PRInt32* aReturn)
{
    PRInt32 result = SUCCESS;
    PRBool  rebootNeeded = PR_FALSE;

    *aReturn = SanityCheck();

    if (*aReturn != nsInstall::SUCCESS)
    {
        SaveError( *aReturn );
        mFinalStatus = *aReturn;
        return NS_OK;
    }


    if ( mInstalledFiles->Count() > 0 )
    {
        if ( mUninstallPackage )
        {
            VR_UninstallCreateNode( NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(mRegistryPackageName).get()),
                                    NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(mUIName).get()));
        }

        // Install the Component into the Version Registry.
        if (mVersionInfo)
        {
            nsString versionString;
            nsCString path;

            mVersionInfo->ToString(versionString);
            nsCAutoString versionCString;
            versionCString.AssignWithConversion(versionString);

            if (mPackageFolder)
                mPackageFolder->GetDirectoryPath(path);

            VR_Install( NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(mRegistryPackageName).get()),
                        NS_CONST_CAST(char *, path.get()),
                        NS_CONST_CAST(char *, versionCString.get()),
                        PR_TRUE );
        }

        nsInstallObject* ie = nsnull;

        for (PRInt32 i=0; i < mInstalledFiles->Count(); i++)
        {
            ie = (nsInstallObject*)mInstalledFiles->ElementAt(i);
            NS_ASSERTION(ie, "NULL object in install queue!");
            if (ie == NULL)
                continue;

            if (mListener)
            {
                char *objString = ie->toString();
                if (objString)
                {
                    mListener->OnFinalizeProgress(
                                    NS_ConvertASCIItoUCS2(objString).get(),
                                    (i+1), mInstalledFiles->Count());
                    delete [] objString;
                }
            }

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
        }

        if ( result == SUCCESS )
        {
            if ( rebootNeeded )
                *aReturn = SaveError( REBOOT_NEEDED );

            if ( nsSoftwareUpdate::mNeedCleanup )
            {
                // Broadcast the fact that we have an incomplete install so
                // parts of Mozilla can take defensive action if necessary.
                //
                // This notification turns off turbo/server mode, for example
                nsPIXPIProxy* proxy = GetUIThreadProxy();
                if (proxy)
                    proxy->NotifyRestartNeeded();
            }

            // XXX for now all successful installs will trigger an Autoreg.
            // We eventually want to do this only when flagged.
            NS_SoftwareUpdateRequestAutoReg();
        }
        else
            *aReturn = SaveError( result );

        mFinalStatus = *aReturn;
    }
    else
    {
        // no actions queued: don't register the package version
        // and no need for user confirmation

        mFinalStatus = *aReturn;
    }

    CleanUp();

    return NS_OK;
}

#if defined(XP_MAC) || defined(XP_MACOSX)
#define GESTALT_CHAR_CODE(x)          (((unsigned long) ((x[0]) & 0x000000FF)) << 24) \
                                    | (((unsigned long) ((x[1]) & 0x000000FF)) << 16) \
                                    | (((unsigned long) ((x[2]) & 0x000000FF)) << 8)  \
                                    | (((unsigned long) ((x[3]) & 0x000000FF)))
#endif /* XP_MACOS || XP_MACOSX */

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
#if defined(XP_MAC) || defined(XP_MACOSX)

    long    response = 0;
    char    selectorChars[4];
    int     i;
    OSErr   err = noErr;
    OSType  selector;

    if (aSelector.IsEmpty())
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

#endif /* XP_MAC || XP_MACOSX */
    return NS_OK;
}

PRInt32
nsInstall::GetComponentFolder(const nsString& aComponentName, const nsString& aSubdirectory, nsInstallFolder** aNewFolder)
{
    long        err;
    char        dir[MAXREGPATHLEN];
    nsresult    res = NS_OK;

    if(!aNewFolder)
        return INVALID_ARGUMENTS;

    *aNewFolder = nsnull;


    nsString tempString;

    if ( GetQualifiedPackageName(aComponentName, tempString) != SUCCESS )
    {
        return NS_OK;
    }

    NS_ConvertUCS2toUTF8 componentCString(tempString);

    if((err = VR_GetDefaultDirectory( NS_CONST_CAST(char *, componentCString.get()), sizeof(dir), dir )) != REGERR_OK)
    {
        // if there's not a default directory, try to see if the component
        // // is registered as a file and then strip the filename off the path
        if((err = VR_GetPath( NS_CONST_CAST(char *, componentCString.get()), sizeof(dir), dir )) != REGERR_OK)
        {
          // no path, either
          *dir = '\0';
        }
    }

    nsCOMPtr<nsILocalFile> componentDir;
    nsCOMPtr<nsIFile> componentIFile;
    if(*dir != '\0')
        NS_NewNativeLocalFile( nsDependentCString(dir), PR_FALSE, getter_AddRefs(componentDir) );

    if ( componentDir )
    {
        PRBool isFile;

        res = componentDir->IsFile(&isFile);
        if (NS_SUCCEEDED(res) && isFile)
            componentDir->GetParent(getter_AddRefs(componentIFile));
        else
            componentIFile = do_QueryInterface(componentDir);

        nsInstallFolder * folder = new nsInstallFolder();
        if (!folder)
            return NS_ERROR_OUT_OF_MEMORY;

        res = folder->Init(componentIFile, aSubdirectory);
        if (NS_FAILED(res))
        {
            delete folder;
        }
        else
        {
            *aNewFolder = folder;
        }
    }

    return res;
}

PRInt32
nsInstall::GetComponentFolder(const nsString& aComponentName, nsInstallFolder** aNewFolder)
{
    return GetComponentFolder(aComponentName, EmptyString(), aNewFolder);
}

PRInt32
nsInstall::GetFolder(const nsString& targetFolder, const nsString& aSubdirectory, nsInstallFolder** aNewFolder)
{
    /* This version of GetFolder takes an nsString object as the first param */
    if (!aNewFolder)
        return INVALID_ARGUMENTS;

    * aNewFolder = nsnull;

    nsInstallFolder* folder = new nsInstallFolder();
    if (folder == nsnull)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult res = folder->Init(targetFolder, aSubdirectory);

    if (NS_FAILED(res))
    {
        delete folder;
        return res;
    }
    *aNewFolder = folder;
    return NS_OK;
}

PRInt32
nsInstall::GetFolder(const nsString& targetFolder, nsInstallFolder** aNewFolder)
{
    /* This version of GetFolder takes an nsString object as the only param */
    return GetFolder(targetFolder, EmptyString(), aNewFolder);
}

PRInt32
nsInstall::GetFolder( nsInstallFolder& aTargetFolderObj, const nsString& aSubdirectory, nsInstallFolder** aNewFolder )
{
    /* This version of GetFolder takes an nsString object as the first param */
    if (!aNewFolder)
        return INVALID_ARGUMENTS;

    * aNewFolder = nsnull;

    nsInstallFolder* folder = new nsInstallFolder();
    if (folder == nsnull)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult res = folder->Init(aTargetFolderObj, aSubdirectory);

    if (NS_FAILED(res))
    {
        delete folder;
        return res;
    }
    *aNewFolder = folder;
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
    nsCOMPtr<nsIFile> resFile;
    nsIURI *url = nsnull;
    nsIStringBundleService* service = nsnull;
    nsIEventQueueService* pEventQueueService = nsnull;
    nsIStringBundle* bundle = nsnull;
    nsCOMPtr<nsISimpleEnumerator> propEnum;
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
    PRInt32 err = ExtractFileFromJar(aBaseName, nsnull, getter_AddRefs(resFile));
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

    // get the string bundle using the extracted properties file
#if 1
    {
      nsCAutoString spec;
      ret = NS_GetURLSpecFromFile(resFile, spec);
      if (NS_FAILED(ret)) {
        NS_WARNING("cannot get url spec\n");
        nsServiceManager::ReleaseService(kStringBundleServiceCID, service);
        return ret;
      }
      ret = service->CreateBundle(spec.get(), &bundle);
    }
#else
    ret = service->CreateBundle(url, &bundle);
#endif
    if (NS_FAILED(ret))
        goto cleanup;
    ret = bundle->GetSimpleEnumeration(getter_AddRefs(propEnum));
    if (NS_FAILED(ret))
        goto cleanup;

    // set the variables of the JSObject to return using the StringBundle's
    // enumeration service
    PRBool hasMore;
    while (NS_SUCCEEDED(propEnum->HasMoreElements(&hasMore)) && hasMore)
    {
        nsCOMPtr<nsISupports> nextProp;
        ret = propEnum->GetNext(getter_AddRefs(nextProp));
        if (NS_FAILED(ret))
            goto cleanup;

        nsCOMPtr<nsIPropertyElement> propElem =
            do_QueryInterface(nextProp);
        if (!propElem)
            continue;

        nsAutoString pVal;
        nsCAutoString pKey;
        ret = propElem->GetKey(pKey);
        if (NS_FAILED(ret))
            goto cleanup;
        ret = propElem->GetValue(pVal);
        if (NS_FAILED(ret))
            goto cleanup;

        if (!pKey.IsEmpty() && !pVal.IsEmpty())
        {
            JSString* propValJSStr = JS_NewUCStringCopyZ(cx, NS_REINTERPRET_CAST(const jschar*, pVal.get()));
            jsval propValJSVal = STRING_TO_JSVAL(propValJSStr);
            nsString UCKey = NS_ConvertUTF8toUCS2(pKey);
            JS_SetUCProperty(cx, res, (jschar*)UCKey.get(), UCKey.Length(), &propValJSVal);
        }
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
    return Patch(aRegName, EmptyString(), aJarSource, aFolder, aTargetName, aReturn);
}

PRInt32
nsInstall::RegisterChrome(nsIFile* chrome, PRUint32 chromeType, const char* path)
{
    PRInt32 result = SanityCheck();
    if (result != SUCCESS)
        return SaveError( result );

    if (!chrome || !chromeType)
        return SaveError( INVALID_ARGUMENTS );

    nsRegisterItem* ri = new nsRegisterItem(this, chrome, chromeType, path);
    if (ri == nsnull)
        return SaveError(OUT_OF_MEMORY);
    else
        return SaveError(ScheduleForInstall( ri ));
}

nsPIXPIProxy* nsInstall::GetUIThreadProxy()
{
    if (!mUIThreadProxy)
    {
        nsresult rv;
        nsCOMPtr<nsIProxyObjectManager> pmgr =
                 do_GetService(kProxyObjectManagerCID, &rv);
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsPIXPIProxy> tmp(do_QueryInterface(new nsXPIProxy()));
            rv = pmgr->GetProxyForObject( NS_UI_THREAD_EVENTQ, NS_GET_IID(nsPIXPIProxy),
                    tmp, PROXY_SYNC | PROXY_ALWAYS, getter_AddRefs(mUIThreadProxy) );
        }
    }

    return mUIThreadProxy;
}

PRInt32
nsInstall::RefreshPlugins(PRBool aReloadPages)
{
    nsPIXPIProxy* proxy = GetUIThreadProxy();
    if (!proxy)
        return UNEXPECTED_ERROR;

    return proxy->RefreshPlugins(aReloadPages);
}


PRInt32
nsInstall::ResetError(PRInt32 aError)
{
    mLastError = aError;
    return SUCCESS;
}

PRInt32
nsInstall::SetPackageFolder(nsInstallFolder& aFolder)
{
    if (mPackageFolder)
        delete mPackageFolder;

    nsInstallFolder* folder = new nsInstallFolder();
    if (folder == nsnull)
    {
        return OUT_OF_MEMORY;
    }
    nsresult res = folder->Init(aFolder, EmptyString());

    if (NS_FAILED(res))
    {
        delete folder;
        return UNEXPECTED_ERROR;
    }
    mPackageFolder = folder;
    return SUCCESS;
}


PRInt32
nsInstall::StartInstall(const nsString& aUserPackageName, const nsString& aRegistryPackageName, const nsString& aVersion, PRInt32* aReturn)
{
    if ( aUserPackageName.IsEmpty() )
    {
        // There must be some pretty name for the UI and the uninstall list
        *aReturn = SaveError(INVALID_ARGUMENTS);
        return NS_OK;
    }

    char szRegPackagePath[MAXREGPATHLEN];

    *szRegPackagePath = '0';
    *aReturn   = nsInstall::SUCCESS;

    ResetError(nsInstall::SUCCESS);

    mUserCancelled = PR_FALSE;

    mUIName = aUserPackageName;

    *aReturn = GetQualifiedPackageName( aRegistryPackageName, mRegistryPackageName );

    if (*aReturn != nsInstall::SUCCESS)
    {
        SaveError( *aReturn );
        return NS_OK;
    }

    // initialize default version
    if (mVersionInfo != nsnull)
        delete mVersionInfo;

    mVersionInfo    = new nsInstallVersion();
    if (mVersionInfo == nsnull)
    {
        *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
        return NS_OK;
    }
    mVersionInfo->Init(aVersion);

    // initialize item queue
    mInstalledFiles = new nsVoidArray();

    if (mInstalledFiles == nsnull)
    {
        *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
        return NS_OK;
    }

    // initialize default folder if any (errors are OK)
    if (mPackageFolder != nsnull)
        delete mPackageFolder;

    mPackageFolder = nsnull;
    if(REGERR_OK == VR_GetDefaultDirectory(
                        NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(mRegistryPackageName).get()),
                        sizeof(szRegPackagePath), szRegPackagePath))
    {
        // found one saved in the registry
        mPackageFolder = new nsInstallFolder();
        nsCOMPtr<nsILocalFile> packageDir;
        NS_NewNativeLocalFile(
                            nsDependentCString(szRegPackagePath), // native path
                            PR_FALSE, getter_AddRefs(packageDir) );

        if (mPackageFolder && packageDir)
        {
            if (NS_FAILED( mPackageFolder->Init(packageDir, EmptyString()) ))
            {
                delete mPackageFolder;
                mPackageFolder = nsnull;
            }
        }
    }

    // We've correctly initialized an install transaction
    // - note that for commands that are only valid within one
    // - save error in case script doesn't call performInstall or cancelInstall
    // - broadcast to listeners
    mStartInstallCompleted = PR_TRUE;
    mFinalStatus = MALFORMED_INSTALL;
    if (mListener)
        mListener->OnPackageNameSet(mInstallURL.get(), mUIName.get(), aVersion.get());

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
nsInstall::AddPatch(nsHashKey *aKey, nsIFile* fileName)
{
    if (mPatchList != nsnull)
    {
        mPatchList->Put(aKey, fileName);
    }
}

void
nsInstall::GetPatch(nsHashKey *aKey, nsIFile** fileName)
{
    if (!fileName)
        return;
    else
        *fileName = nsnull;

    if (mPatchList != nsnull)
    {
        *fileName = (nsIFile*) mPatchList->Get(aKey);
    }
}

PRInt32
nsInstall::FileOpDirCreate(nsInstallFolder& aTarget, PRInt32* aReturn)
{
  nsCOMPtr<nsIFile> localFile = aTarget.GetFileSpec();
  if (localFile == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_DIR_CREATE, localFile, aReturn);
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
nsInstall::FileOpDirGetParent(nsInstallFolder& aTarget, nsInstallFolder** theParentFolder)
{
  nsCOMPtr<nsIFile> parent;
  nsCOMPtr<nsIFile> localFile = aTarget.GetFileSpec();

  nsresult rv = localFile->GetParent(getter_AddRefs(parent));
  if (NS_SUCCEEDED(rv) && parent)
  {
    nsInstallFolder* folder = new nsInstallFolder();
    if (folder == nsnull)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
      folder->Init(parent,EmptyString());
      *theParentFolder = folder;
  }
  else
      theParentFolder = nsnull;

  return NS_OK;
}

PRInt32
nsInstall::FileOpDirRemove(nsInstallFolder& aTarget, PRInt32 aFlags, PRInt32* aReturn)
{
  nsCOMPtr<nsIFile> localFile = aTarget.GetFileSpec();
  if (localFile == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_DIR_REMOVE, localFile, aFlags, aReturn);
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
nsInstall::FileOpDirRename(nsInstallFolder& aSrc, nsString& aTarget, PRInt32* aReturn)
{
  nsCOMPtr<nsIFile> localFile = aSrc.GetFileSpec();
  if (localFile == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_DIR_RENAME, localFile, aTarget, PR_FALSE, aReturn);
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
nsInstall::FileOpFileCopy(nsInstallFolder& aSrc, nsInstallFolder& aTarget, PRInt32* aReturn)
{
  nsCOMPtr<nsIFile> localSrcFile = aSrc.GetFileSpec();
  if (localSrcFile == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsCOMPtr<nsIFile>localTargetFile = aTarget.GetFileSpec();
  if (localTargetFile == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_COPY, localSrcFile, localTargetFile, aReturn);
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
nsInstall::FileOpFileDelete(nsInstallFolder& aTarget, PRInt32 aFlags, PRInt32* aReturn)
{
  nsCOMPtr<nsIFile> localFile = aTarget.GetFileSpec();
  if (localFile == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_DELETE, localFile, aFlags, aReturn);
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
nsInstall::FileOpFileExecute(nsInstallFolder& aTarget, nsString& aParams, PRBool aBlocking, PRInt32* aReturn)
{
  nsCOMPtr<nsIFile> localFile = aTarget.GetFileSpec();
  if (localFile == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_EXECUTE, localFile, aParams, aBlocking, aReturn);
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
nsInstall::FileOpFileExists(nsInstallFolder& aTarget, PRBool* aReturn)
{
  nsCOMPtr<nsIFile> localFile = aTarget.GetFileSpec();

  localFile->Exists(aReturn);
  return NS_OK;
}

#ifdef XP_WIN
#include <winver.h>
#endif

PRInt32
nsInstall::FileOpFileGetNativeVersion(nsInstallFolder& aTarget, nsString* aReturn)
{
  PRInt32           rv = NS_OK;

#ifdef XP_WIN
  PRBool            flagExists;
  nsCOMPtr<nsILocalFile> localTarget(do_QueryInterface(aTarget.GetFileSpec()));
  UINT              uLen;
  UINT              dwLen;
  DWORD             dwHandle;
  LPVOID            lpData;
  LPVOID            lpBuffer;
  VS_FIXEDFILEINFO  *lpBuffer2;
  DWORD             dwMajor   = 0;
  DWORD             dwMinor   = 0;
  DWORD             dwRelease = 0;
  DWORD             dwBuild   = 0;
  nsCAutoString     nativeTargetPath;
  char              *nativeVersionString = nsnull;

  if(localTarget == nsnull)
    return(rv);

  flagExists = PR_FALSE;
  localTarget->Exists(&flagExists);
  if(flagExists)
  {
    localTarget->GetNativePath(nativeTargetPath);
    uLen   = 0;
    /* GetFileVersionInfoSize() requires a char *, but the api doesn't
     * indicate that it will modify it */
    dwLen  = GetFileVersionInfoSize((char *)nativeTargetPath.get(), &dwHandle);
    lpData = (LPVOID)PR_Malloc(sizeof(long)*dwLen);
    if(!lpData)
      return(nsInstall::OUT_OF_MEMORY);

    /* GetFileVersionInfo() requires a char *, but the api doesn't
     * indicate that it will modify it */
    if(GetFileVersionInfo((char *)nativeTargetPath.get(), dwHandle, dwLen, lpData) != 0)
    {
      if(VerQueryValue(lpData, "\\", &lpBuffer, &uLen) != 0)
      {
        lpBuffer2 = (VS_FIXEDFILEINFO *)lpBuffer;
        dwMajor   = HIWORD(lpBuffer2->dwFileVersionMS);
        dwMinor   = LOWORD(lpBuffer2->dwFileVersionMS);
        dwRelease = HIWORD(lpBuffer2->dwFileVersionLS);
        dwBuild   = LOWORD(lpBuffer2->dwFileVersionLS);
      }
    }

    nativeVersionString = PR_smprintf("%d.%d.%d.%d", dwMajor, dwMinor, dwRelease, dwBuild);
    if(!nativeVersionString)
      rv = nsInstall::OUT_OF_MEMORY;
    else
    {
      aReturn->Assign(NS_ConvertASCIItoUCS2(nativeVersionString));
      PR_smprintf_free(nativeVersionString);
    }

    PR_FREEIF(lpData);
  }
#endif

  return(rv);
}

PRInt32
nsInstall::FileOpFileGetDiskSpaceAvailable(nsInstallFolder& aTarget, PRInt64* aReturn)
{
  nsresult rv;
  nsCOMPtr<nsIFile> file = aTarget.GetFileSpec();
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(file, &rv);

  localFile->GetDiskSpaceAvailable(aReturn);  //nsIFileXXX: need to figure out how to call GetDiskSpaceAvailable
  return NS_OK;
}

//nsIFileXXX: need to get nsIFile equivalent to GetModDate
PRInt32
nsInstall::FileOpFileGetModDate(nsInstallFolder& aTarget, double* aReturn)
{
    * aReturn = 0;

    nsCOMPtr<nsIFile> localFile = aTarget.GetFileSpec();

    if (localFile)
    {
        double newStamp;
        PRInt64 lastModDate = LL_ZERO;
        localFile->GetLastModifiedTime(&lastModDate);

        LL_L2D(newStamp, lastModDate);

        *aReturn = newStamp;
    }

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileGetSize(nsInstallFolder& aTarget, PRInt64* aReturn)
{
  nsCOMPtr<nsIFile> localFile = aTarget.GetFileSpec();

  localFile->GetFileSize(aReturn);
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileIsDirectory(nsInstallFolder& aTarget, PRBool* aReturn)
{
  nsCOMPtr<nsIFile> localFile = aTarget.GetFileSpec();

  localFile->IsDirectory(aReturn);
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileIsWritable(nsInstallFolder& aTarget, PRBool* aReturn)
{
  nsCOMPtr<nsIFile> localFile = aTarget.GetFileSpec();

  localFile->IsWritable(aReturn);
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileIsFile(nsInstallFolder& aTarget, PRBool* aReturn)
{
  nsCOMPtr<nsIFile> localFile = aTarget.GetFileSpec();

  localFile->IsFile(aReturn);
  return NS_OK;
}

//nsIFileXXX: need to get the ModDateChanged equivalent for nsIFile
PRInt32
nsInstall::FileOpFileModDateChanged(nsInstallFolder& aTarget, double aOldStamp, PRBool* aReturn)
{
    *aReturn = PR_TRUE;

    nsCOMPtr<nsIFile> localFile = aTarget.GetFileSpec();
    if (localFile)
    {
        double newStamp;
        PRInt64 lastModDate = LL_ZERO;
        localFile->GetLastModifiedTime(&lastModDate);

        LL_L2D(newStamp, lastModDate);

        *aReturn = !(newStamp == aOldStamp);
    }
    return NS_OK;
}

PRInt32
nsInstall::FileOpFileMove(nsInstallFolder& aSrc, nsInstallFolder& aTarget, PRInt32* aReturn)
{
  nsCOMPtr<nsIFile> localSrcFile = aSrc.GetFileSpec();
  if (localSrcFile == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }
  nsCOMPtr<nsIFile> localTargetFile = aTarget.GetFileSpec();
  if (localTargetFile == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_MOVE, localSrcFile, localTargetFile, aReturn);
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
nsInstall::FileOpFileRename(nsInstallFolder& aSrc, nsString& aTarget, PRInt32* aReturn)
{
  nsCOMPtr<nsIFile> localFile = aSrc.GetFileSpec();
  if (localFile == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_RENAME, localFile, aTarget, PR_FALSE, aReturn);
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

#ifdef _WINDOWS
#include <winbase.h>
#endif


PRInt32
nsInstall::FileOpFileWindowsGetShortName(nsInstallFolder& aTarget, nsString& aShortPathName)
{
#ifdef _WINDOWS

  PRInt32             err;
  PRBool              flagExists;
  nsString            tmpNsString;
  nsCAutoString       nativeTargetPath;
  nsAutoString        unicodePath;
  char                nativeShortPathName[MAX_PATH];
  nsCOMPtr<nsIFile>   localTarget(aTarget.GetFileSpec());

  if(localTarget == nsnull)
    return NS_OK;

  localTarget->Exists(&flagExists);
  if(flagExists)
  {
    memset(nativeShortPathName, 0, MAX_PATH);
    localTarget->GetNativePath(nativeTargetPath);

    err = GetShortPathName(nativeTargetPath.get(), nativeShortPathName, MAX_PATH);
    if((err > 0) && (*nativeShortPathName == '\0'))
    {
      // NativeShortPathName buffer not big enough.
      // Reallocate and try again.
      // err will have the required size.
      char *nativeShortPathNameTmp = new char[err + 1];
      if(nativeShortPathNameTmp == nsnull)
        return NS_OK;

      err = GetShortPathName(nativeTargetPath.get(), nativeShortPathNameTmp, err + 1);
      // It is safe to assume that the second time around the buffer is big
      // enough and not to worry about it unless it's a different problem.  If
      // it failed the first time because of buffer size being too small, err
      // will be the buffer size required.  If it's any other error, err will
      // be 0 and GetLastError() will have the actual error.
      if(err != 0)
      {
        // if err is 0, it's not a buffer size problem.  It's something else unexpected.
        NS_CopyNativeToUnicode(nsDependentCString(nativeShortPathNameTmp), unicodePath);
      }

      if(nativeShortPathNameTmp)
        delete [] nativeShortPathNameTmp;
    }
    else if(err != 0)
    {
      // if err is 0, it's not a buffer size problem.  It's something else unexpected.
      NS_CopyNativeToUnicode(nsDependentCString(nativeShortPathName), unicodePath);
    }
  }

  if (!unicodePath.IsEmpty())
    aShortPathName = unicodePath;

#endif

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileWindowsShortcut(nsIFile* aTarget, nsIFile* aShortcutPath, nsString& aDescription, nsIFile* aWorkingPath, nsString& aParams, nsIFile* aIcon, PRInt32 aIconId, PRInt32* aReturn)
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
nsInstall::FileOpFileMacAlias(nsIFile *aSourceFile, nsIFile *aAliasFile, PRInt32* aReturn)
{

  *aReturn = nsInstall::SUCCESS;

#if defined(XP_MAC) || defined(XP_MACOSX)

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_MAC_ALIAS, aSourceFile, aAliasFile, aReturn);
  if (!ifop)
  {
      *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
      return NS_OK;
  }

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

#endif /* XP_MAC || XP_MACOSX */

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileUnixLink(nsInstallFolder& aTarget, PRInt32 aFlags, PRInt32* aReturn)
{
  return NS_OK;
}

PRInt32
nsInstall::FileOpWinRegisterServer(nsInstallFolder& aTarget, PRInt32* aReturn)
{
  nsCOMPtr<nsIFile> localFile = aTarget.GetFileSpec();
  if (localFile == nsnull)
  {
     *aReturn = SaveError(nsInstall::OUT_OF_MEMORY);
     return NS_OK;
  }

  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_WIN_REGISTER_SERVER, localFile, aReturn);
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

void
nsInstall::LogComment(const nsAString& aComment)
{
  if(mListener)
    mListener->OnLogComment(PromiseFlatString(aComment).get());
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

    if (mListener)
        mListener->OnItemScheduled(NS_ConvertASCIItoUCS2(objString).get());


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
    else if ( mListener )
    {
        // error in preparation step -- log it
        char* errRsrc = GetResourcedString(NS_LITERAL_STRING("ERROR"));
        if (errRsrc)
        {
            char* errprefix = PR_smprintf("%s (%d): ", errRsrc, error);
            nsString errstr; errstr.AssignWithConversion(errprefix);
            errstr.AppendWithConversion(objString);

            mListener->OnLogComment( errstr.get() );

            PR_smprintf_free(errprefix);
            nsCRT::free(errRsrc);
        }
    }

    if (error != SUCCESS)
        SaveError(error);

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

    if ( startOfName.EqualsLiteral("=USER=/") )
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

    if ( startOfName.EqualsLiteral("=COMM=/") || startOfName.EqualsLiteral("=USER=/"))
    {
        qualifiedRegName = startOfName;
    }
    else if (name.CharAt(0) != '/' &&
             !mRegistryPackageName.IsEmpty())
    {
        qualifiedRegName = mRegistryPackageName
                         + NS_LITERAL_STRING("/")
                         + name;
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


void
nsInstall::CurrentUserNode(nsString& userRegNode)
{
    nsXPIDLCString profname;
    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);

    if ( prefBranch )
    {
        prefBranch->GetCharPref("profile.name", getter_Copies(profname));
    }

    userRegNode.AssignLiteral("/Netscape/Users/");
    if ( !profname.IsEmpty() )
    {
        userRegNode.AppendWithConversion(profname);
        userRegNode.AppendLiteral("/");
    }
}

// catch obvious registry name errors proactively
// rather than returning some cryptic libreg error
PRBool
nsInstall::BadRegName(const nsString& regName)
{
    if ( regName.IsEmpty() )
        return PR_TRUE;

    if ((regName.First() == ' ' ) || (regName.Last() == ' ' ))
        return PR_TRUE;

    if ( regName.Find("//") != -1 )
        return PR_TRUE;

    if ( regName.Find(" /") != -1 )
        return PR_TRUE;

    if ( regName.Find("/ ") != -1  )
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
 * call it when done with the install
 *
 */
void
nsInstall::CleanUp(void)
{
    nsInstallObject* ie;

    if ( mInstalledFiles != nsnull )
    {
        for (PRInt32 i=0; i < mInstalledFiles->Count(); i++)
        {
            ie = (nsInstallObject*)mInstalledFiles->ElementAt(i);
            if (ie)
                delete ie;
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

    mRegistryPackageName.SetLength(0); // used to see if StartInstall() has been called
    mStartInstallCompleted = PR_FALSE;
}


void
nsInstall::SetJarFileLocation(nsIFile* aFile)
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

//-----------------------------------------------------------------------------
// GetTranslatedString :
// This is a utility function that translates "Alert" or
// "Confirm" to pass as the title to the PromptService Alert and Confirm
// functions as the title.  If you pass nsnull as the title, you get garbage
// instead of a blank title.
//-----------------------------------------------------------------------------
PRUnichar *GetTranslatedString(const PRUnichar* aString)
{
    nsCOMPtr<nsIStringBundleService> stringService = do_GetService(kStringBundleServiceCID);
    nsCOMPtr<nsIStringBundle> stringBundle;
    PRUnichar* translatedString;

    nsresult rv = stringService->CreateBundle(kInstallLocaleProperties, getter_AddRefs(stringBundle));
    if (NS_FAILED(rv)) return nsnull;

    rv = stringBundle->GetStringFromName(aString, &translatedString);
    if (NS_FAILED(rv)) return nsnull;

    return translatedString;
}

PRInt32
nsInstall::Alert(nsString& string)
{
    nsPIXPIProxy *ui = GetUIThreadProxy();
    if (!ui)
        return UNEXPECTED_ERROR;

    return ui->Alert( GetTranslatedString(NS_LITERAL_STRING("Alert").get()),
                      string.get());
}

PRInt32
nsInstall::Confirm(nsString& string, PRBool* aReturn)
{
    *aReturn = PR_FALSE; /* default value */

    nsPIXPIProxy *ui = GetUIThreadProxy();
    if (!ui)
        return UNEXPECTED_ERROR;

    return ui->Confirm( GetTranslatedString(NS_LITERAL_STRING("Confirm").get()),
                        string.get(),
                        aReturn);
}


// aJarFile         - This is the filepath within the jar file.
// aSuggestedName   - This is the name that we should try to extract to.  If we can, we will create a new temporary file.
// aRealName        - This is the name that we did extract to.  This will be allocated by use and should be disposed by the caller.

PRInt32
nsInstall::ExtractFileFromJar(const nsString& aJarfile, nsIFile* aSuggestedName, nsIFile** aRealName)
{
    PRInt32 extpos = 0;
    nsCOMPtr<nsIFile> extractHereSpec;
    nsCOMPtr<nsILocalFile> tempFile;
    nsresult rv;

    if (aSuggestedName == nsnull)
    {
        nsCOMPtr<nsIProperties> directoryService =
                 do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);

        directoryService->Get(NS_OS_TEMP_DIR, NS_GET_IID(nsIFile), getter_AddRefs(tempFile));

        nsAutoString tempFileName(NS_LITERAL_STRING("xpinstall"));

        // Get the extension of the file in the JAR
        extpos = aJarfile.RFindChar('.');
        if (extpos != -1)
        {
            // We found the extension; add it to the tempFileName string
            nsString extension;
            aJarfile.Right(extension, (aJarfile.Length() - extpos) );
            tempFileName += extension;
        }
        tempFile->Append(tempFileName);
        tempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0664);
        tempFile->Clone(getter_AddRefs(extractHereSpec));

        if (extractHereSpec == nsnull)
            return nsInstall::OUT_OF_MEMORY;
    }
    else
    {
        nsCOMPtr<nsIFile> temp;
        aSuggestedName->Clone(getter_AddRefs(temp));

        PRBool exists;
        temp->Exists(&exists);
        if (exists)
        {
            PRBool writable;
            temp->IsWritable(&writable);
            if (!writable) // Can't extract. Target is readonly.
                return nsInstall::READ_ONLY;

            tempFile = do_QueryInterface(temp, &rv); //convert to an nsILocalFile
            if (tempFile == nsnull)
                return nsInstall::OUT_OF_MEMORY;

            //get the leafname so we can convert its extension to .new
            nsAutoString newLeafName;
            tempFile->GetLeafName(newLeafName);

            PRInt32 extpos = newLeafName.RFindChar('.');
            if (extpos != -1)
            {
                // We found the extension;
                newLeafName.Truncate(extpos + 1); //strip off the old extension
            }
            newLeafName.AppendLiteral("new");

            //Now reset the leafname
            tempFile->SetLeafName(newLeafName);
            tempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0644);
            extractHereSpec = tempFile;
        }
        extractHereSpec = temp;
    }

    rv = mJarFileData->Extract(NS_LossyConvertUCS2toASCII(aJarfile).get(),
                               extractHereSpec);
    if (NS_FAILED(rv))
    {
        switch (rv) {
          case NS_ERROR_FILE_ACCESS_DENIED:         return ACCESS_DENIED;
          case NS_ERROR_FILE_DISK_FULL:             return INSUFFICIENT_DISK_SPACE;
          case NS_ERROR_FILE_TARGET_DOES_NOT_EXIST: return DOES_NOT_EXIST;
          default:                                  return EXTRACTION_FAILED;
        }
    }

#if defined(XP_MAC) || defined(XP_MACOSX)
    FSRef finalRef, extractedRef;

    nsCOMPtr<nsILocalFileMac> tempExtractHereSpec;
    tempExtractHereSpec = do_QueryInterface(extractHereSpec, &rv);
    tempExtractHereSpec->GetFSRef(&extractedRef);

    if ( nsAppleSingleDecoder::IsAppleSingleFile(&extractedRef) )
    {
        nsAppleSingleDecoder *asd = 
          new nsAppleSingleDecoder(&extractedRef, &finalRef);
        OSErr decodeErr = fnfErr;

        if (asd)
            decodeErr = asd->Decode();

        if (decodeErr != noErr)
        {
            if (asd)
                delete asd;
            return EXTRACTION_FAILED;
        }

        if (noErr != FSCompareFSRefs(&extractedRef, &finalRef))
        {
            // delete the unique extracted file that got renamed in AS decoding
            FSDeleteObject(&extractedRef);

            // "real name" in AppleSingle entry may cause file rename
            tempExtractHereSpec->InitWithFSRef(&finalRef);
            extractHereSpec = do_QueryInterface(tempExtractHereSpec, &rv);
        }
    }
#endif /* XP_MAC || XP_MACOSX */

    extractHereSpec->Clone(aRealName);
    
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
nsInstall::GetResourcedString(const nsAString& aResName)
{
    if (mStringBundle)
    {
        nsXPIDLString ucRscdStr;
        nsresult rv = mStringBundle->GetStringFromName(PromiseFlatString(aResName).get(),
                                                     getter_Copies(ucRscdStr));
        if (NS_SUCCEEDED(rv))
            return ToNewCString(ucRscdStr);
    }

    /*
    ** We don't have a string bundle, the necessary libs, or something went wrong
    ** so we failover to hardcoded english strings so we log something rather
    ** than nothing due to failure above: always the case for the Install Wizards.
    */
    return nsCRT::strdup(nsInstallResources::GetDefaultVal(
                    NS_LossyConvertUCS2toASCII(aResName).get()));
}


PRInt32
nsInstall::ExtractDirEntries(const nsString& directory, nsVoidArray *paths)
{
    char                *buf;
    nsISimpleEnumerator *jarEnum = nsnull;
    nsIZipEntry         *currZipEntry = nsnull;

    if ( paths )
    {
        nsString pattern(directory + NS_LITERAL_STRING("/*"));
        PRInt32 prefix_length = directory.Length()+1; // account for slash

        nsresult rv = mJarFileData->FindEntries(
                          NS_LossyConvertUCS2toASCII(pattern).get(),
                          &jarEnum );
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
                        nsString* tempString = new nsString; tempString->AssignWithConversion(buf+prefix_length);
                        paths->AppendElement(tempString);
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
