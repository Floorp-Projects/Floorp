#ifndef __NS_INSTALLFOLDER_H__
#define __NS_INSTALLFOLDER_H__

#include "nscore.h"
#include "nsString.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIScriptObjectOwner.h"

#include "nsIDOMInstallFolder.h"
#include "nsSoftwareUpdate.h"

#include "prtypes.h"

class nsInstallFolder: public nsIScriptObjectOwner, public nsIDOMInstallFolder
{
    public:
        static const nsIID& IID() { static nsIID iid = NS_SoftwareUpdateInstallFolder_CID; return iid; }

        nsInstallFolder();
        ~nsInstallFolder();
        
        NS_DECL_ISUPPORTS

        NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
        NS_IMETHOD    SetScriptObject(void* aScriptObject);
        
        NS_IMETHOD    Init(const nsString& aFolderID, const nsString& aVrPatch, const nsString& aPackageName);
        
        NS_IMETHOD    GetDirectoryPath(nsString& aDirectoryPath);
        NS_IMETHOD    MakeFullPath(const nsString& aRelativePath, nsString& aFullPath);
        NS_IMETHOD    IsJavaCapable(PRBool* aReturn);
        NS_IMETHOD    ToString(nsString& aFolderString);

        

    private:
        void *mScriptObject;
        
        nsString* mUrlPath;  	            // Full path to the directory. Used to cache results from GetDirectoryPath
		nsString* mFolderID; 		        // Unique string specifying a folder
		nsString* mVersionRegistryPath;     // Version registry path of the package
		nsString* mUserPackageName;  	    // Name of the package presented to the user

		void         SetDirectoryPath(nsString** aFolderString);		
        void         PickDefaultDirectory(nsString** aFolderString);
        nsString*    GetNativePath(const nsString& path);
        PRInt32      MapNameToEnum(nsString* name);
        void         AppendSlashToDirPath(nsString* dirPath);
        PRBool       IsJavaDir(void);
};


class nsInstallFolderFactory : public nsIFactory 
{
    public:
        
        nsInstallFolderFactory();
        ~nsInstallFolderFactory();
        
        NS_DECL_ISUPPORTS

              NS_IMETHOD CreateInstance(nsISupports *aOuter,
                                        REFNSIID aIID,
                                        void **aResult);

              NS_IMETHOD LockFactory(PRBool aLock);

};

#endif