
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
#include "nsTopProgressNotifier.h"

class nsSoftwareUpdate:  public nsIAppShellComponent, public nsISoftwareUpdate
{
    public:
        
        NS_DEFINE_STATIC_CID_ACCESSOR( NS_SoftwareUpdate_CID );

        static nsSoftwareUpdate *GetInstance();

        nsSoftwareUpdate();
        virtual ~nsSoftwareUpdate();

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
        

    private:
        static   nsSoftwareUpdate* mInstance;

        nsresult RunNextInstall();
        nsresult DeleteScheduledNodes();
        
        PRLock*           mLock;
        PRBool            mInstalling;
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
