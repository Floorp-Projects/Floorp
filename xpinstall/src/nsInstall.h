#ifndef __NS_INSTALL_H__
#define __NS_INSTALL_H__

#include "nscore.h"
#include "nsString.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIScriptObjectOwner.h"

#include "nsVector.h"

#include "nsIDOMInstall.h"
#include "nsInstallObject.h"

#include "nsSoftwareUpdate.h"

#include "nsInstallFolder.h"
#include "nsInstallVersion.h"

class nsInstall: public nsIScriptObjectOwner, public nsIDOMInstall
{
    public:
        static const nsIID& IID() { static nsIID iid = NS_SoftwareUpdateInstall_CID; return iid; }

        nsInstall();
        ~nsInstall();
        
        NS_DECL_ISUPPORTS
    
        NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
        NS_IMETHOD    SetScriptObject(void* aScriptObject);
        
        NS_IMETHOD    GetUserPackageName(nsString& aUserPackageName);
        NS_IMETHOD    GetRegPackageName(nsString& aRegPackageName);

        NS_IMETHOD    AbortInstall();
        NS_IMETHOD    AddDirectory(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aSubdir, PRBool aForceMode, PRInt32* aReturn);
        NS_IMETHOD    AddSubcomponent(const nsString& aRegName, nsIDOMInstallVersion* aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aTargetName, PRBool aForceMode, PRInt32* aReturn);
        NS_IMETHOD    DeleteComponent(const nsString& aRegistryName, PRInt32* aReturn);
        NS_IMETHOD    DeleteFile(nsIDOMInstallFolder* aFolder, const nsString& aRelativeFileName, PRInt32* aReturn);
        NS_IMETHOD    DiskSpaceAvailable(nsIDOMInstallFolder* aFolder, PRInt32* aReturn);
        NS_IMETHOD    Execute(const nsString& aJarSource, const nsString& aArgs, PRInt32* aReturn);
        NS_IMETHOD    FinalizeInstall(PRInt32* aReturn);
        NS_IMETHOD    Gestalt(const nsString& aSelector, PRInt32* aReturn);
        NS_IMETHOD    GetComponentFolder(const nsString& aRegName, const nsString& aSubdirectory, nsIDOMInstallFolder** aFolder);
        NS_IMETHOD    GetFolder(nsIDOMInstallFolder* aTargetFolder, const nsString& aSubdirectory, nsIDOMInstallFolder** aFolder);
        NS_IMETHOD    GetLastError(PRInt32* aReturn);
        NS_IMETHOD    GetWinProfile(nsIDOMInstallFolder* aFolder, const nsString& aFile, PRInt32* aReturn);
        NS_IMETHOD    GetWinRegistry(PRInt32* aReturn);
        NS_IMETHOD    Patch(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aTargetName, PRInt32* aReturn);
        NS_IMETHOD    ResetError();
        NS_IMETHOD    SetPackageFolder(nsIDOMInstallFolder* aFolder);
        NS_IMETHOD    StartInstall(const nsString& aUserPackageName, const nsString& aPackageName, const nsString& aVersion, PRInt32 aFlags, PRInt32* aReturn);
        NS_IMETHOD    Uninstall(const nsString& aPackageName, PRInt32* aReturn);
        

/*needs to be noscript*/
        NS_IMETHOD    ExtractFileFromJar(const nsString& aJarfile, const nsString& aFinalFile, nsString& aTempFile, PRInt32* aError);


    private:
        void *mScriptObject;
        
        PRBool              mUserCancelled;
        PRBool              mShowProgress;
        PRBool              mShowFinalize;
        PRBool              mUninstallPackage;
        PRBool              mRegisterPackage;

        nsString            mPackageName;        /* Name of the package we are installing */
        nsString            mUserPackageName;    /* User-readable package name */

        nsInstallVersion*   mVersionInfo;        /* Component version info */
        nsInstallFolder*    mPackageFolder;
        nsVector*           mInstalledFiles;        

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

        PRInt32      ScheduleForInstall(nsInstallObject* ob);

};


class nsInstallFactory : public nsIFactory 
{
    public:
        
        nsInstallFactory();
        ~nsInstallFactory();
        
        NS_DECL_ISUPPORTS

              NS_IMETHOD CreateInstance(nsISupports *aOuter,
                                        REFNSIID aIID,
                                        void **aResult);

              NS_IMETHOD LockFactory(PRBool aLock);

};

#endif