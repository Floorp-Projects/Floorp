
#ifndef nsSoftwareUpdate_h___
#define nsSoftwareUpdate_h___

#include "nsSoftwareUpdateIIDs.h"
#include "nsISoftwareUpdate.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsVector.h"
#include "prlock.h"

class nsInstallInfo;

#include "nsIScriptExternalNameSet.h"
#include "nsIAppShellComponent.h"
#include "nsIXPINotifier.h"
#include "nsPvtIXPIStubHook.h"
#include "nsTopProgressNotifier.h"

class nsSoftwareUpdate: public nsIAppShellComponent, 
                        public nsISoftwareUpdate, 
                        public nsPvtIXPIStubHook
{
    public:
        
        NS_DEFINE_STATIC_CID_ACCESSOR( NS_SoftwareUpdate_CID );

        static nsSoftwareUpdate *GetInstance();

        /** GetProgramDirectory
         *  information used within the XPI module -- not
         *  available through any interface
         */
        static nsIFileSpec* GetProgramDirectory() { return mProgramDir; }

        NS_DECL_ISUPPORTS
        NS_DECL_IAPPSHELLCOMPONENT
        
        NS_IMETHOD InstallJar( nsIFileSpec* localFile,
                               const PRUnichar* URL,
                               const PRUnichar* arguments,
                               long flags = 0,
                               nsIXPINotifier* notifier = 0);  

        NS_IMETHOD RegisterNotifier(nsIXPINotifier *notifier);
        
        NS_IMETHOD InstallJarCallBack();
        NS_IMETHOD GetMasterNotifier(nsIXPINotifier **notifier);
        NS_IMETHOD SetActiveNotifier(nsIXPINotifier *notifier);

        /** SetProgramDirectory() is private for the Install Wizard.
         *  The mStubLockout property makes sure this is only called
         *  once, and is also set by the AppShellComponent initialize
         *  so it can't be called during a normal Mozilla run
         */
        NS_IMETHOD SetProgramDirectory(nsIFileSpec *dir);

        nsSoftwareUpdate();
        virtual ~nsSoftwareUpdate();


    private:
        static   nsSoftwareUpdate* mInstance;
        static   nsIFileSpec*      mProgramDir;

        nsresult RunNextInstall();
        nsresult DeleteScheduledNodes();
        
        PRLock*           mLock;
        PRBool            mInstalling;
        PRBool            mStubLockout;
        nsVector*         mJarInstallQueue;
        nsTopProgressNotifier   mMasterNotifier;
};


class nsSoftwareUpdateNameSet : public nsIScriptExternalNameSet 
{
    public:
        nsSoftwareUpdateNameSet();
        virtual ~nsSoftwareUpdateNameSet();

        NS_DECL_ISUPPORTS
            NS_IMETHOD InitializeClasses(nsIScriptContext* aScriptContext);
            NS_IMETHOD AddNameSet(nsIScriptContext* aScriptContext);
};

#define XPINSTALL_ENABLE_PREF      "xpinstall.enabled"
#define XPINSTALL_DETAILS_PREF     "xpinstall.show_details"

#endif
