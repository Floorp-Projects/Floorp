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


#ifndef __NS_INSTALL_H__
#define __NS_INSTALL_H__

#include "nscore.h"
#include "nsISupports.h"

#include "jsapi.h"

#include "nsString.h"
#include "nsFileSpec.h"
#include "nsVector.h"
#include "nsHashtable.h"

#include "nsSoftwareUpdate.h"

#include "nsInstallObject.h"
#include "nsInstallVersion.h"

class nsInstallInfo
{
  public:
    nsInstallInfo(const nsString& fromURL);
    nsInstallInfo(const nsString& fromURL, const nsString& arguments);
    nsInstallInfo(const nsString& fromURL, const nsString& arguments, const nsString& flags);
    
    nsInstallInfo(nsVector* fromURL, const nsString& arguments, const nsString& flags);
    ~nsInstallInfo();

    nsString& GetFromURL(PRUint32 index = 0);
    
    nsString& GetLocalFile(PRUint32 index = 0);

    nsString& GetArguments();
    nsString& GetFlags();
    
    PRBool    IsMultipleTrigger();
  
  private:
    DeleteVector(nsVector* vector);
    MakeTempFile(nsString aURL, nsString& tempFileString);

    nsString *mLocalFile;
    nsString *mFromURL;
    nsString *mArguments;
    nsString *mFlags;
    

    PRBool  mInstalled;
    PRBool  mMultipleTrigger;

    nsVector *mFromURLs;
    nsVector *mLocalFiles;
};




class nsInstall
{
    public:
       
        enum 
        {
            BAD_PACKAGE_NAME            = -200,
            UNEXPECTED_ERROR            = -201,
            ACCESS_DENIED               = -202,
            TOO_MANY_CERTIFICATES       = -203,
            NO_INSTALLER_CERTIFICATE    = -204,
            NO_CERTIFICATE              = -205,
            NO_MATCHING_CERTIFICATE     = -206,
            UNKNOWN_JAR_FILE            = -207,
            INVALID_ARGUMENTS           = -208,
            ILLEGAL_RELATIVE_PATH       = -209,
            USER_CANCELLED              = -210,
            INSTALL_NOT_STARTED         = -211,
            SILENT_MODE_DENIED          = -212,
            NO_SUCH_COMPONENT           = -213,
            FILE_DOES_NOT_EXIST         = -214,
            FILE_READ_ONLY              = -215,
            FILE_IS_DIRECTORY           = -216,
            NETWORK_FILE_IS_IN_USE      = -217,
            APPLE_SINGLE_ERR            = -218,
            INVALID_PATH_ERR            = -219,
            PATCH_BAD_DIFF              = -220,
            PATCH_BAD_CHECKSUM_TARGET   = -221,
            PATCH_BAD_CHECKSUM_RESULT   = -222,
            UNINSTALL_FAILED            = -223,
            GESTALT_UNKNOWN_ERR         = -5550,
            GESTALT_INVALID_ARGUMENT    = -5551,
            
            SUCCESS                     = 0,
            REBOOT_NEEDED               = 999,
            
            LIMITED_INSTALL             = 0,
            FULL_INSTALL                = 1,
            NO_STATUS_DLG               = 2,
            NO_FINALIZE_DLG             = 4,

            INSTALL_FILE_UNEXPECTED_MSG_ID = 0,
            DETAILS_REPLACE_FILE_MSG_ID = 1,
            DETAILS_INSTALL_FILE_MSG_ID = 2
        };


        nsInstall();
        ~nsInstall();
        
        PRInt32      SetScriptObject(void* aScriptObject);
        
        PRInt32    GetUserPackageName(nsString& aUserPackageName);
        PRInt32    GetRegPackageName(nsString& aRegPackageName);

        PRInt32    AbortInstall();
        PRInt32    AddDirectory(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, const nsString& aFolder, const nsString& aSubdir, PRBool aForceMode, PRInt32* aReturn);
        PRInt32    AddSubcomponent(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, const nsString& aFolder, const nsString& aTargetName, PRBool aForceMode, PRInt32* aReturn);
        PRInt32    DeleteComponent(const nsString& aRegistryName, PRInt32* aReturn);
        PRInt32    DeleteFile(const nsString& aFolder, const nsString& aRelativeFileName, PRInt32* aReturn);
        PRInt32    DiskSpaceAvailable(const nsString& aFolder, PRInt32* aReturn);
        PRInt32    Execute(const nsString& aJarSource, const nsString& aArgs, PRInt32* aReturn);
        PRInt32    FinalizeInstall(PRInt32* aReturn);
        PRInt32    Gestalt(const nsString& aSelector, PRInt32* aReturn);
        PRInt32    GetComponentFolder(const nsString& aComponentName, const nsString& aSubdirectory, nsString** aFolder);
        PRInt32    GetFolder(const nsString& aTargetFolder, const nsString& aSubdirectory, nsString** aFolder);
        PRInt32    GetLastError(PRInt32* aReturn);
        PRInt32    GetWinProfile(const nsString& aFolder, const nsString& aFile, PRInt32* aReturn);
        PRInt32    GetWinRegistry(PRInt32* aReturn);
        PRInt32    Patch(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, const nsString& aFolder, const nsString& aTargetName, PRInt32* aReturn);
        PRInt32    ResetError();
        PRInt32    SetPackageFolder(const nsString& aFolder);
        PRInt32    StartInstall(const nsString& aUserPackageName, const nsString& aPackageName, const nsString& aVersion, PRInt32 aFlags, PRInt32* aReturn);
        PRInt32    Uninstall(const nsString& aPackageName, PRInt32* aReturn);
        


        PRInt32    ExtractFileFromJar(const nsString& aJarfile, nsFileSpec* aSuggestedName, nsFileSpec** aRealName);
        void       AddPatch(nsHashKey *aKey, nsFileSpec* fileName);
        void       GetPatch(nsHashKey *aKey, nsFileSpec* fileName);
        
        void       GetJarFileLocation(nsString& aFile);
        void       SetJarFileLocation(const nsString& aFile);

        void       GetInstallArguments(nsString& args);
        void       SetInstallArguments(const nsString& args);


    private:
        JSObject*           mScriptObject;
        
        nsString            mJarFileLocation;
        void*               mJarFileData;
        
        nsString            mInstallArguments;

        PRBool              mUserCancelled;
        
        PRBool              mUninstallPackage;
        PRBool              mRegisterPackage;

        nsString            mRegistryPackageName;   /* Name of the package we are installing */
        nsString            mUIName;                /* User-readable package name */

        nsInstallVersion*   mVersionInfo;        /* Component version info */
        
        nsString*           mPackageFolder;
        
        nsVector*           mInstalledFiles;        
        nsHashtable*        mPatchList;

        PRInt32             mLastError;

        void        ParseFlags(int flags);
        PRInt32     SanityCheck(void);
        
        nsString *  GetQualifiedRegName( const nsString& name );
        nsString*   GetQualifiedPackageName( const nsString& name );
        nsString*   CurrentUserNode();
        PRBool      BadRegName(nsString* regName);
        PRInt32     SaveError(PRInt32 errcode);

        void        CleanUp();

        PRInt32     OpenJARFile(void);
        void        CloseJARFile(void);
        PRInt32     ExtractDirEntries(const nsString& directory, nsVector *paths);

        PRInt32     ScheduleForInstall(nsInstallObject* ob);
        

};

#endif