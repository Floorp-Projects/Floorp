
#ifndef nsSoftwareUpdate_h___
#define nsSoftwareUpdate_h___

#include "nsSoftwareUpdateIIDs.h"
#include "nsISoftwareUpdate.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "prlock.h"
//#include "mozreg.h"
#include "NSReg.h"
#include "nsCOMPtr.h"

class nsInstallInfo;

#include "nsIScriptExternalNameSet.h"
#include "nsIAppShellComponent.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIXPIStubHook.h"
#include "nsTopProgressNotifier.h"

#if NOTIFICATION_ENABLE
#include "nsIUpdateNotification.h"
#endif


#define XPI_ROOT_KEY    "software/mozilla/xpinstall"
#define XPI_AUTOREG_VAL "Autoreg"
#define XPCOM_KEY       "software/mozilla/XPCOM"

class nsSoftwareUpdate: public nsIAppShellComponent, 
                        public nsISoftwareUpdate, 
                        public nsPIXPIStubHook
{
    public:
        
        NS_DEFINE_STATIC_CID_ACCESSOR( NS_SoftwareUpdate_CID );

        static nsSoftwareUpdate *GetInstance();

        /** GetProgramDirectory
         *  Information used within the XPI module -- not
         *  available through any interface
         */
        static nsIFile* GetProgramDirectory() { return mProgramDir; }

        /** GetLogName
         *  Optional log name used privately in the XPI module.
         */
        static char*    GetLogName() { return mLogName; }

        NS_DECL_ISUPPORTS
        NS_DECL_NSIAPPSHELLCOMPONENT
        
        NS_IMETHOD InstallJar( nsIFile* localFile,
                               const PRUnichar* URL,
                               const PRUnichar* arguments,
                               nsIDOMWindowInternal* aParentWindow,
                               PRUint32 flags = 0,
                               nsIXPIListener* aListener = 0);

        NS_IMETHOD InstallChrome( PRUint32 aType,
                                  nsIFile* aFile,
                                  const PRUnichar* URL,
                                  const PRUnichar* aName,
                                  PRBool aSelect,
                                  nsIXPIListener* aListener = 0);

        NS_IMETHOD RegisterListener(nsIXPIListener *aListener);
        
        NS_IMETHOD InstallJarCallBack();
        NS_IMETHOD GetMasterListener(nsIXPIListener **aListener);
        NS_IMETHOD SetActiveListener(nsIXPIListener *aListener);
        NS_IMETHOD StartupTasks( PRBool* needAutoreg );

        /** StubInitialize() is private for the Install Wizard.
         *  The mStubLockout property makes sure this is only called
         *  once, and is also set by the AppShellComponent initialize
         *  so it can't be called during a normal Mozilla run
         */
        NS_IMETHOD StubInitialize(nsIFile *dir, const char* logName);

        nsSoftwareUpdate();
        virtual ~nsSoftwareUpdate();


    private:
        static   nsSoftwareUpdate*  mInstance;
        static   nsCOMPtr<nsIFile>  mProgramDir;
        static   char*              mLogName;

#if NOTIFICATION_ENABLE
        static   nsIUpdateNotification *mUpdateNotifier;
#endif
        
        nsresult RunNextInstall();
        nsresult RegisterNameset();
        
        PRLock*               mLock;
        PRBool                mInstalling;
        PRBool                mStubLockout;
        nsVoidArray           mJarInstallQueue;
        nsTopProgressListener *mMasterListener;

        HREG                  mReg;
};


class nsSoftwareUpdateNameSet : public nsIScriptExternalNameSet 
{
    public:
        nsSoftwareUpdateNameSet();
        virtual ~nsSoftwareUpdateNameSet();

        // nsISupports
        NS_DECL_ISUPPORTS

        // nsIScriptExternalNameSet
        NS_IMETHOD InitializeClasses(nsIScriptContext* aScriptContext);
        NS_IMETHOD AddNameSet(nsIScriptContext* aScriptContext);
};
#endif
