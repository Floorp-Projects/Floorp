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
#include "nsInstallFolder.h"
#include "nsInstallVersion.h"

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
        PRInt32    AddDirectory(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aSubdir, PRBool aForceMode, PRInt32* aReturn);
        PRInt32    AddSubcomponent(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aTargetName, PRBool aForceMode, PRInt32* aReturn);
        PRInt32    DeleteComponent(const nsString& aRegistryName, PRInt32* aReturn);
        PRInt32    DeleteFile(nsIDOMInstallFolder* aFolder, const nsString& aRelativeFileName, PRInt32* aReturn);
        PRInt32    DiskSpaceAvailable(nsIDOMInstallFolder* aFolder, PRInt32* aReturn);
        PRInt32    Execute(const nsString& aJarSource, const nsString& aArgs, PRInt32* aReturn);
        PRInt32    FinalizeInstall(PRInt32* aReturn);
        PRInt32    Gestalt(const nsString& aSelector, PRInt32* aReturn);
        PRInt32    GetComponentFolder(const nsString& aComponentName, const nsString& aSubdirectory, nsIDOMInstallFolder** aFolder);
        PRInt32    GetFolder(const nsString& aTargetFolder, const nsString& aSubdirectory, nsIDOMInstallFolder** aFolder);
        PRInt32    GetLastError(PRInt32* aReturn);
        PRInt32    GetWinProfile(nsIDOMInstallFolder* aFolder, const nsString& aFile, PRInt32* aReturn);
        PRInt32    GetWinRegistry(PRInt32* aReturn);
        PRInt32    Patch(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aTargetName, PRInt32* aReturn);
        PRInt32    ResetError();
        PRInt32    SetPackageFolder(nsIDOMInstallFolder* aFolder);
        PRInt32    StartInstall(const nsString& aUserPackageName, const nsString& aPackageName, const nsString& aVersion, PRInt32 aFlags, PRInt32* aReturn);
        PRInt32    Uninstall(const nsString& aPackageName, PRInt32* aReturn);
        


        PRInt32    ExtractFileFromJar(const nsString& aJarfile, const nsString& aFinalFile, nsString** aTempFile);
        void       AddPatch(nsHashKey *aKey, nsString* fileName);
        void       GetPatch(nsHashKey *aKey, nsString* fileName);
        
        void       GetJarFileLocation(char** aFile);
        void       SetJarFileLocation(char* aFile);

        void       GetInstallArguments(char** args);
        void       SetInstallArguments(char* args);


    private:
        JSObject*           mScriptObject;
        

        char*               mJarFileLocation;
        void*               mJarFileData;
        
        char*               mInstallArguments;

        PRBool              mUserCancelled;
        
        PRBool              mUninstallPackage;
        PRBool              mRegisterPackage;

        nsString            mPackageName;        /* Name of the package we are installing */
        nsString            mUserPackageName;    /* User-readable package name */

        nsInstallVersion*   mVersionInfo;        /* Component version info */
        nsInstallFolder*    mPackageFolder;
        
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