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


#ifndef __NS_INSTALL_H__
#define __NS_INSTALL_H__

#include "nscore.h"
#include "nsISupports.h"

#include "jsapi.h"

#include "plevent.h"

#include "nsString.h"
#include "nsFileSpec.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsCOMPtr.h"
#include "nsILocalFile.h"

#include "nsSoftwareUpdate.h"

#include "nsInstallObject.h"
#include "nsInstallVersion.h"
#include "nsInstallFolder.h"

#include "nsIXPINotifier.h"
#include "nsPIXPIProxy.h"

#include "nsIStringBundle.h"
#include "nsILocale.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIPersistentProperties2.h"
#include "nsIEnumerator.h"
#include "nsIZipReader.h"
#include "nsIChromeRegistry.h"

#define XPINSTALL_BUNDLE_URL "chrome://communicator/locale/xpinstall/xpinstall.properties"

//file and directory name length maximums
#ifdef XP_MAC
#define MAX_FILENAME 31
#elif defined (XP_PC)
#define MAX_FILENAME 128
#else 
#define MAX_FILENAME 1024
#endif

class nsInstallInfo
{
  public:
    
    nsInstallInfo( PRUint32         aInstallType,
                   nsIFile*         aFile, 
                   const PRUnichar* aURL, 
                   const PRUnichar* aArgs, 
                   PRUint32         aFlags, 
                   nsIXPIListener*  aListener,
                   nsIDOMWindowInternal* aParentWindow,
                   nsIChromeRegistry*   aChromeReg);

    virtual ~nsInstallInfo();

    nsIFile*            GetFile()               { return mFile.get(); }
    const PRUnichar*    GetURL()                { return mURL.get(); }
    const PRUnichar*    GetArguments()          { return mArgs.get(); }
    PRUint32            GetFlags()              { return mFlags; }
    PRUint32            GetType()               { return mType; }
    nsIXPIListener*     GetListener()           { return mListener.get(); }
    nsIChromeRegistry*  GetChromeRegistry()     { return mChromeReg.get(); }
    nsIDOMWindowInternal* GetParentDOMWindow()  { return mParent.get(); }

  private:

    nsresult  mError;

    PRUint32   mType;
    PRUint32   mFlags;
    nsString   mURL;
    nsString   mArgs;

    nsCOMPtr<nsIFile>           mFile;
    nsCOMPtr<nsIXPIListener>    mListener;
    nsCOMPtr<nsIDOMWindowInternal> mParent;
    nsCOMPtr<nsIChromeRegistry> mChromeReg;
};

#ifdef XP_PC
#define FILESEP '\\'
#elif defined XP_MAC
#define FILESEP ':'
#elif defined XP_BEOS
#define FILESEP '/'
#else
#define FILESEP '/'
#endif

class nsInstall
{
    friend class nsWinReg;
    friend class nsWinProfile;

    public:

        enum
        {
            BAD_PACKAGE_NAME            = -200,
            UNEXPECTED_ERROR            = -201,
            ACCESS_DENIED               = -202,
            TOO_MANY_CERTIFICATES       = -203,
            NO_INSTALL_SCRIPT           = -204,
            NO_CERTIFICATE              = -205,
            NO_MATCHING_CERTIFICATE     = -206,
            CANT_READ_ARCHIVE           = -207,
            INVALID_ARGUMENTS           = -208,
            ILLEGAL_RELATIVE_PATH       = -209,
            USER_CANCELLED              = -210,
            INSTALL_NOT_STARTED         = -211,
            SILENT_MODE_DENIED          = -212,
            NO_SUCH_COMPONENT           = -213,
            DOES_NOT_EXIST              = -214,
            READ_ONLY                   = -215,
            IS_DIRECTORY                = -216,
            NETWORK_FILE_IS_IN_USE      = -217,
            APPLE_SINGLE_ERR            = -218,
            INVALID_PATH_ERR            = -219,
            PATCH_BAD_DIFF              = -220,
            PATCH_BAD_CHECKSUM_TARGET   = -221,
            PATCH_BAD_CHECKSUM_RESULT   = -222,
            UNINSTALL_FAILED            = -223,
            PACKAGE_FOLDER_NOT_SET      = -224,
            EXTRACTION_FAILED           = -225,
            FILENAME_ALREADY_USED       = -226,
            INSTALL_CANCELLED           = -227,
            DOWNLOAD_ERROR              = -228,
            SCRIPT_ERROR                = -229,

            ALREADY_EXISTS              = -230,
            IS_FILE                     = -231,
            SOURCE_DOES_NOT_EXIST       = -232,
            SOURCE_IS_DIRECTORY         = -233,
            SOURCE_IS_FILE              = -234,
            INSUFFICIENT_DISK_SPACE     = -235,
            FILENAME_TOO_LONG           = -236,

            UNABLE_TO_LOCATE_LIB_FUNCTION = -237,
            UNABLE_TO_LOAD_LIBRARY        = -238,

            CHROME_REGISTRY_ERROR       = -239,

            MALFORMED_INSTALL           = -240,

            KEY_ACCESS_DENIED           = -241,
            KEY_DOES_NOT_EXIST          = -242,
            VALUE_DOES_NOT_EXIST        = -243,

            OUT_OF_MEMORY               = -299,

            GESTALT_UNKNOWN_ERR         = -5550,
            GESTALT_INVALID_ARGUMENT    = -5551,
            
            SUCCESS                     = 0,
            REBOOT_NEEDED               = 999,
            
            INSTALL_FILE_UNEXPECTED_MSG_ID = 0,
            DETAILS_REPLACE_FILE_MSG_ID = 1,
            DETAILS_INSTALL_FILE_MSG_ID = 2,

            DO_NOT_UNINSTALL            = 2,
            WIN_SHARED_FILE             = 4
        };


        nsInstall(nsIZipReader * theJARFile);
        virtual ~nsInstall();

        PRInt32    SetScriptObject(void* aScriptObject);

        PRInt32    SaveWinRegPrototype(void* aScriptObject);
        PRInt32    SaveWinProfilePrototype(void* aScriptObject);

        JSObject*  RetrieveWinRegPrototype(void);
        JSObject*  RetrieveWinProfilePrototype(void);

        PRInt32    AbortInstall(PRInt32 aErrorNumber);

        PRInt32    AddDirectory(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsInstallFolder* aFolder, const nsString& aSubdir, PRInt32 aMode, PRInt32* aReturn);
        PRInt32    AddDirectory(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsInstallFolder* aFolder, const nsString& aSubdir, PRInt32* aReturn);
        PRInt32    AddDirectory(const nsString& aRegName, const nsString& aJarSource, nsInstallFolder* aFolder, const nsString& aSubdir, PRInt32* aReturn);
        PRInt32    AddDirectory(const nsString& aJarSource, PRInt32* aReturn);

        PRInt32    AddSubcomponent(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsInstallFolder *aFolder, const nsString& aTargetName, PRInt32 aMode, PRInt32* aReturn);
        PRInt32    AddSubcomponent(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsInstallFolder *aFolder, const nsString& aTargetName, PRInt32* aReturn);
        PRInt32    AddSubcomponent(const nsString& aRegName, const nsString& aJarSource, nsInstallFolder *aFolder, const nsString& aTargetName, PRInt32* aReturn);
        PRInt32    AddSubcomponent(const nsString& aJarSource, PRInt32* aReturn);

        PRInt32    DiskSpaceAvailable(const nsString& aFolder, PRInt64* aReturn);
        PRInt32    Execute(const nsString& aJarSource, const nsString& aArgs, PRBool aBlocking, PRInt32* aReturn);
        PRInt32    FinalizeInstall(PRInt32* aReturn);
        PRInt32    Gestalt(const nsString& aSelector, PRInt32* aReturn);

        PRInt32    GetComponentFolder(const nsString& aComponentName, const nsString& aSubdirectory, nsInstallFolder** aFolder);
        PRInt32    GetComponentFolder(const nsString& aComponentName, nsInstallFolder** aFolder);

        PRInt32    GetFolder(nsInstallFolder& aTargetFolder, const nsString& aSubdirectory, nsInstallFolder** aFolder);
        PRInt32    GetFolder(const nsString& aTargetFolder, const nsString& aSubdirectory, nsInstallFolder** aFolder);
        PRInt32    GetFolder(const nsString& aTargetFolder, nsInstallFolder** aFolder);

        PRInt32    GetLastError(PRInt32* aReturn);
        PRInt32    GetWinProfile(const nsString& aFolder, const nsString& aFile, JSContext* jscontext, JSClass* WinProfileClass, jsval* aReturn);
        PRInt32    GetWinRegistry(JSContext* jscontext, JSClass* WinRegClass, jsval* aReturn);
        PRInt32	   LoadResources(JSContext* cx, const nsString& aBaseName, jsval* aReturn);
        PRInt32    Patch(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsInstallFolder* aFolder, const nsString& aTargetName, PRInt32* aReturn);
        PRInt32    Patch(const nsString& aRegName, const nsString& aJarSource, nsInstallFolder* aFolder, const nsString& aTargetName, PRInt32* aReturn);
        PRInt32    RegisterChrome(nsIFile* chrome, PRUint32 chromeType, const char* path);
        PRInt32    RefreshPlugins();
        PRInt32    ResetError(PRInt32 aError);
        PRInt32    SetPackageFolder(nsInstallFolder& aFolder);
        PRInt32    StartInstall(const nsString& aUserPackageName, const nsString& aPackageName, const nsString& aVersion, PRInt32* aReturn);
        PRInt32    Uninstall(const nsString& aPackageName, PRInt32* aReturn);

        PRInt32    FileOpDirCreate(nsInstallFolder& aTarget, PRInt32* aReturn);
        PRInt32    FileOpDirGetParent(nsInstallFolder& aTarget, nsInstallFolder** theParentFolder);
        PRInt32    FileOpDirRemove(nsInstallFolder& aTarget, PRInt32 aFlags, PRInt32* aReturn);
        PRInt32    FileOpDirRename(nsInstallFolder& aSrc, nsString& aTarget, PRInt32* aReturn);
        PRInt32    FileOpFileCopy(nsInstallFolder& aSrc, nsInstallFolder& aTarget, PRInt32* aReturn);
        PRInt32    FileOpFileDelete(nsInstallFolder& aTarget, PRInt32 aFlags, PRInt32* aReturn);
        PRInt32    FileOpFileExists(nsInstallFolder& aTarget, PRBool* aReturn);
        PRInt32    FileOpFileExecute(nsInstallFolder& aTarget, nsString& aParams, PRBool aBlocking, PRInt32* aReturn);
        PRInt32    FileOpFileGetNativeVersion(nsInstallFolder& aTarget, nsString* aReturn);
        PRInt32    FileOpFileGetDiskSpaceAvailable(nsInstallFolder& aTarget, PRInt64* aReturn);
        PRInt32    FileOpFileGetModDate(nsInstallFolder& aTarget, double* aReturn);
        PRInt32    FileOpFileGetSize(nsInstallFolder& aTarget, PRInt64* aReturn);
        PRInt32    FileOpFileIsDirectory(nsInstallFolder& aTarget, PRBool* aReturn);
        PRInt32    FileOpFileIsWritable(nsInstallFolder& aTarget, PRBool* aReturn);
        PRInt32    FileOpFileIsFile(nsInstallFolder& aTarget, PRBool* aReturn);
        PRInt32    FileOpFileModDateChanged(nsInstallFolder& aTarget, double aOldStamp, PRBool* aReturn);
        PRInt32    FileOpFileMove(nsInstallFolder& aSrc, nsInstallFolder& aTarget, PRInt32* aReturn);
        PRInt32    FileOpFileRename(nsInstallFolder& aSrc, nsString& aTarget, PRInt32* aReturn);
        PRInt32    FileOpFileWindowsGetShortName(nsInstallFolder& aTarget, nsString& aShortPathName);
        PRInt32    FileOpFileWindowsShortcut(nsIFile* aTarget, nsIFile* aShortcutPath, nsString& aDescription, nsIFile* aWorkingPath, nsString& aParams, nsIFile* aIcon, PRInt32 aIconId, PRInt32* aReturn);
        PRInt32    FileOpFileMacAlias(nsIFile *aSourceFile, nsIFile *aAliasFile, PRInt32* aReturn);
        PRInt32    FileOpFileUnixLink(nsInstallFolder& aTarget, PRInt32 aFlags, PRInt32* aReturn);
        PRInt32    FileOpWinRegisterServer(nsInstallFolder& aTarget, PRInt32* aReturn);

        void       LogComment(nsString& aComment);

        PRInt32    ExtractFileFromJar(const nsString& aJarfile, nsIFile* aSuggestedName, nsIFile** aRealName);
        char*      GetResourcedString(const nsAString& aResName);
        void       AddPatch(nsHashKey *aKey, nsIFile* fileName);
        void       GetPatch(nsHashKey *aKey, nsIFile** fileName);
        
        nsIFile*   GetJarFileLocation() { return mJarFileLocation; }
        void       SetJarFileLocation(nsIFile* aFile);

        void       GetInstallArguments(nsString& args);
        void       SetInstallArguments(const nsString& args);

        void       GetInstallURL(nsString& url);
        void       SetInstallURL(const nsString& url);

        PRUint32   GetInstallFlags()  { return mInstallFlags; }
        void       SetInstallFlags(PRUint32 aFlags) { mInstallFlags = aFlags; }
        
        PRInt32    GetInstallPlatform(nsCString& aPlatform);

        nsIChromeRegistry*  GetChromeRegistry() { return mChromeRegistry; }
        void                SetChromeRegistry(nsIChromeRegistry* reg)
                                { mChromeRegistry = reg; }

        nsIDOMWindowInternal*  GetParentDOMWindow() { return mParent; }
        void                   SetParentDOMWindow(nsIDOMWindowInternal* parent)
        { mParent = parent; }

        PRBool     GetStatusSent() { return mStatusSent; }
        PRBool     InInstallTransaction(void) { return mInstalledFiles != nsnull; }

        PRInt32    Alert(nsString& string);
        PRInt32    Confirm(nsString& string, PRBool* aReturn);
        void       InternalAbort(PRInt32 errcode);

        PRInt32    ScheduleForInstall(nsInstallObject* ob);

        PRInt32    SaveError(PRInt32 errcode);

    private:
        JSObject*           mScriptObject;

        JSObject*           mWinRegObject;
        JSObject*           mWinProfileObject;

        
        nsCOMPtr<nsIFile>   mJarFileLocation;
        nsIZipReader*       mJarFileData;

        nsString            mInstallArguments;
        nsString            mInstallURL;
        PRUint32            mInstallFlags;
        nsCString           mInstallPlatform;
        nsIChromeRegistry*  mChromeRegistry; // we don't own it, it outlives us
        nsIDOMWindowInternal* mParent;
        nsInstallFolder*    mPackageFolder;

        PRBool              mUserCancelled;
        PRBool              mStatusSent;

        PRBool              mUninstallPackage;
        PRBool              mRegisterPackage;
        PRBool              mStartInstallCompleted;

        nsString            mRegistryPackageName;   /* Name of the package we are installing */
        nsString            mUIName;                /* User-readable package name */
        nsInstallVersion*   mVersionInfo;           /* Component version info */
        
        nsVoidArray*        mInstalledFiles;        
        //nsCOMPtr<nsISupportsArray>   mInstalledFiles;
        nsHashtable*        mPatchList;

        nsCOMPtr<nsIXPIListener>    mListener;
        nsCOMPtr<nsPIXPIProxy>      mUIThreadProxy;

        nsCOMPtr<nsIStringBundle>   mStringBundle;

        PRInt32             mLastError;

        void        ParseFlags(int flags);
        PRInt32     SanityCheck(void);
        void        GetTime(nsString &aString);

        nsPIXPIProxy* GetUIThreadProxy();

        PRInt32     GetQualifiedRegName(const nsString& name, nsString& qualifiedRegName );
        PRInt32     GetQualifiedPackageName( const nsString& name, nsString& qualifiedName );

        void        CurrentUserNode(nsString& userRegNode);
        PRBool      BadRegName(const nsString& regName);

        void        CleanUp();

        PRInt32     ExtractDirEntries(const nsString& directory, nsVoidArray *paths);

        static void DeleteVector(nsVoidArray* vector);
};

nsresult MakeUnique(nsILocalFile* file);

#endif
