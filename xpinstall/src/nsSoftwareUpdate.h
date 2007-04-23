
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
class nsIPrincipal;

#include "nsIScriptExternalNameSet.h"
#include "nsIObserver.h"
#include "nsPIXPIStubHook.h"
#include "nsTopProgressNotifier.h"

class nsSoftwareUpdate: public nsISoftwareUpdate,
                        public nsPIXPIStubHook,
                        public nsIObserver
{
    public:

        NS_DEFINE_STATIC_CID_ACCESSOR( NS_SoftwareUpdate_CID )

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

        static void     NeedCleanup() { mNeedCleanup = PR_TRUE; }

        NS_DECL_ISUPPORTS
        NS_DECL_NSPIXPISTUBHOOK
        NS_DECL_NSIOBSERVER

        NS_IMETHOD InstallJar( nsIFile* localFile,
                               const PRUnichar* URL,
                               const PRUnichar* arguments,
                               nsIPrincipal* principal = nsnull,
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

        nsSoftwareUpdate();
        virtual ~nsSoftwareUpdate();

        static   PRBool             mNeedCleanup;

    private:
        static   nsSoftwareUpdate*  mInstance;
        static   nsCOMPtr<nsIFile>  mProgramDir;
        static   char*              mLogName;

        nsresult RunNextInstall();
        nsresult RegisterNameset();
        void     CreateMasterListener();
        void     Shutdown();

        PRLock*               mLock;
        PRBool                mInstalling;
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
        NS_IMETHOD InitializeNameSet(nsIScriptContext* aScriptContext);
};
#endif
