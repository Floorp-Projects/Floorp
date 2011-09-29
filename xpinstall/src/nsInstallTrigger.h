#ifndef __NS_INSTALLTRIGGER_H__
#define __NS_INSTALLTRIGGER_H__

#include "nscore.h"
#include "nsString.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIScriptObjectOwner.h"

#include "prtypes.h"
#include "nsHashtable.h"

#include "nsIDOMInstallTriggerGlobal.h"
#include "nsXPITriggerInfo.h"

#include "nsIContentHandler.h"

#define NOT_CHROME          0
#define CHROME_SKIN         1
#define CHROME_LOCALE       2
#define CHROME_SAFEMAX      CHROME_SKIN
#define CHROME_CONTENT      4
#define CHROME_ALL          (CHROME_SKIN | CHROME_LOCALE | CHROME_CONTENT)
#define CHROME_PROFILE      8
#define CHROME_DELAYED      0x10
#define CHROME_SELECT       0x20

#define XPI_PERMISSION      "install"

#define XPI_WHITELIST       PR_TRUE
#define XPI_GLOBAL          PR_FALSE

#define XPINSTALL_ENABLE_PREF            "xpinstall.enabled"
#define XPINSTALL_WHITELIST_ADD          "xpinstall.whitelist.add"
#define XPINSTALL_WHITELIST_ADD_36       "xpinstall.whitelist.add.36"
#define XPINSTALL_WHITELIST_REQUIRED     "xpinstall.whitelist.required"
#define XPINSTALL_BLACKLIST_ADD          "xpinstall.blacklist.add"

class nsInstallTrigger: public nsIScriptObjectOwner,
                        public nsIDOMInstallTriggerGlobal,
                        public nsIContentHandler
{
    public:
        nsInstallTrigger();
        virtual ~nsInstallTrigger();

        NS_DECL_ISUPPORTS
        NS_DECL_NSICONTENTHANDLER

        NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
        NS_IMETHOD    SetScriptObject(void* aScriptObject);

        NS_IMETHOD    GetOriginatingURI(nsIScriptGlobalObject* aGlobalObject, nsIURI * *aUri);
        NS_IMETHOD    UpdateEnabled(nsIScriptGlobalObject* aGlobalObject, bool aUseWhitelist, bool* aReturn);
        NS_IMETHOD    UpdateEnabled(nsIURI* aURI, bool aUseWhitelist, bool* aReturn);
        NS_IMETHOD    StartInstall(nsIXPIInstallInfo* aInstallInfo, bool* aReturn);


    private:
        bool    AllowInstall(nsIURI* aLaunchURI);
        void *mScriptObject;
};

#define NS_INSTALLTRIGGERCOMPONENT_CONTRACTID "@mozilla.org/xpinstall/installtrigger;1"
#endif
