#ifndef __NS_INSTALLTRIGGER_H__
#define __NS_INSTALLTRIGGER_H__

#include "nscore.h"
#include "nsString.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIScriptObjectOwner.h"

#include "nsIDOMInstallTriggerGlobal.h"
#include "nsSoftwareUpdate.h"

#include "prtypes.h"
#include "nsHashtable.h"
#include "nsVector.h"

class nsInstallTrigger: public nsIScriptObjectOwner, public nsIDOMInstallTriggerGlobal
{
    public:
        static const nsIID& IID() { static nsIID iid = NS_SoftwareUpdateInstallTrigger_CID; return iid; }

        nsInstallTrigger();
        ~nsInstallTrigger();
        
        NS_DECL_ISUPPORTS

        NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
        NS_IMETHOD    SetScriptObject(void* aScriptObject);

        NS_IMETHOD    UpdateEnabled(PRBool* aReturn);
        NS_IMETHOD    StartSoftwareUpdate(const nsString& aURL, PRInt32* aReturn);
        NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, PRInt32 aDiffLevel, const nsString& aVersion, PRInt32 aMode, PRInt32* aReturn);
        NS_IMETHOD    CompareVersion(const nsString& aRegName, const nsString& aVersion, PRInt32* aReturn);

        
    private:
        void *mScriptObject;

};


class nsInstallTriggerFactory : public nsIFactory 
{
    public:
        
        nsInstallTriggerFactory();
        ~nsInstallTriggerFactory();
        
        NS_DECL_ISUPPORTS

              NS_IMETHOD CreateInstance(nsISupports *aOuter,
                                        REFNSIID aIID,
                                        void **aResult);

              NS_IMETHOD LockFactory(PRBool aLock);

};

#endif