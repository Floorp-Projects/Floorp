#ifndef __NS_INSTALLVERSION_H__
#define __NS_INSTALLVERSION_H__

#include "nscore.h"
#include "nsString.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIScriptObjectOwner.h"

#include "nsIDOMInstallVersion.h"
#include "nsSoftwareUpdate.h"

#include "prtypes.h"

class nsInstallVersion: public nsIScriptObjectOwner, public nsIDOMInstallVersion
{
    public:
        static const nsIID& IID() { static nsIID iid = NS_SoftwareUpdateInstallVersion_CID; return iid; }

        nsInstallVersion();
        virtual ~nsInstallVersion();
        
        NS_DECL_ISUPPORTS

        NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
        NS_IMETHOD    SetScriptObject(void* aScriptObject);
        
        NS_IMETHOD    Init(PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild);
        NS_IMETHOD    Init(const nsString& aVersionString);

        NS_IMETHOD    GetMajor(PRInt32* aMajor);
        NS_IMETHOD    SetMajor(PRInt32 aMajor);

        NS_IMETHOD    GetMinor(PRInt32* aMinor);
        NS_IMETHOD    SetMinor(PRInt32 aMinor);

        NS_IMETHOD    GetRelease(PRInt32* aRelease);
        NS_IMETHOD    SetRelease(PRInt32 aRelease);

        NS_IMETHOD    GetBuild(PRInt32* aBuild);
        NS_IMETHOD    SetBuild(PRInt32 aBuild);
        
        NS_IMETHOD    ToString(nsString& aReturn);
        NS_IMETHOD    CompareTo(nsIDOMInstallVersion* aVersion, PRInt32* aReturn);
        NS_IMETHOD    CompareTo(const nsString& aString, PRInt32* aReturn);
        NS_IMETHOD    CompareTo(PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild, PRInt32* aReturn);

        static nsresult StringToVersionNumbers(const nsString& version, PRInt32 *aMajor, PRInt32 *aMinor, PRInt32 *aRelease, PRInt32 *aBuild);

    private:
        void *mScriptObject;
        PRInt32 major;
        PRInt32 minor;
        PRInt32 release;
        PRInt32 build;
};


#define NS_INSTALLVERSIONCOMPONENT_CONTRACTID  "@mozilla.org/xpinstall/installversion;1"
#endif
