
#ifndef nsSoftwareUpdate_h___
#define nsSoftwareUpdate_h___

#include "nsSoftwareUpdateIIDs.h"
#include "nsISoftwareUpdate.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsVector.h"

class nsInstallInfo;

#include "nsIScriptExternalNameSet.h"
#include "nsIAppShellComponent.h"
#include "nsIXPInstallProgressNotifier.h"
#include "nsTopProgressNotifier.h"

class nsSoftwareUpdate:  public nsIAppShellComponent, public nsISoftwareUpdate
{
    public:
        
        NS_DEFINE_STATIC_CID_ACCESSOR( NS_SoftwareUpdate_CID );

        nsSoftwareUpdate();
        virtual ~nsSoftwareUpdate();

        NS_DECL_ISUPPORTS
        NS_DECL_IAPPSHELLCOMPONENT
        
        NS_IMETHOD InstallJar(const nsString& fromURL,
                              const nsString& localFile, 
                              long flags);  

        NS_IMETHOD RegisterNotifier(nsIXPInstallProgressNotifier *notifier);
        
        NS_IMETHOD InstallPending(void);

        NS_IMETHOD InstallJarCallBack();
        NS_IMETHOD GetTopLevelNotifier(nsIXPInstallProgressNotifier **notifier);


    private:
        
        nsresult RunNextInstall();
        nsresult DeleteScheduledNodes();
        
        PRBool            mInstalling;
        nsVector*         mJarInstallQueue;
        nsTopProgressNotifier   *mTopLevelObserver;
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

#define AUTOUPDATE_ENABLE_PREF     "autoupdate.enabled"
#define AUTOUPDATE_CONFIRM_PREF    "autoupdate.confirm_install"
#define CHARSET_HEADER             "Charset"
#define CONTENT_ENCODING_HEADER    "Content-encoding"
#define INSTALLER_HEADER           "Install-Script"
#define MOCHA_CONTEXT_PREFIX       "autoinstall:"
#define REG_SOFTUPDT_DIR           "Netscape/Communicator/SoftwareUpdate/"
#define LAST_REGPACK_TIME          "LastRegPackTime"


#endif