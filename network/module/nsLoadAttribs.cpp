#include "nsString.h"
#include "nsILoadAttribs.h"
#include "prtypes.h"

static NS_DEFINE_IID(kILoadAttribs, NS_ILOAD_ATTRIBS_IDD);

// nsLoadAttribs definition.
class nsLoadAttribs : public nsILoadAttribs {
public:
    nsLoadAttribs();
    virtual ~nsLoadAttribs();

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsILoadAttribs
    NS_IMETHOD SetBypassProxy(PRBool aBypass);
    NS_IMETHOD GetBypassProxy(PRBool *aBypass);
    NS_IMETHOD SetLocalIP(const PRUint32 aIP);
    NS_IMETHOD GetLocalIP(PRUint32 *aIP);

private:
    PRBool mBypass;
    PRUint32 mLocalIP;
};

// nsLoadAttribs Implementation

NS_IMPL_ADDREF(nsLoadAttribs)
NS_IMPL_RELEASE(nsLoadAttribs)

nsresult
nsLoadAttribs::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    if (aIID.Equals(kILoadAttribs)) {
        *aInstancePtr = (void*) ((nsILoadAttribs*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports *)this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

nsLoadAttribs::nsLoadAttribs() {
    mBypass = PR_FALSE;
    mLocalIP = 0;
}

nsLoadAttribs::~nsLoadAttribs() {
}

nsresult
nsLoadAttribs::SetBypassProxy(PRBool aBypass) {
    mBypass = aBypass;
    return NS_OK;
}

nsresult
nsLoadAttribs::GetBypassProxy(PRBool *aBypass) {
    *aBypass = mBypass;
    return NS_OK;
}

nsresult
nsLoadAttribs::SetLocalIP(const PRUint32 aLocalIP) {
    mLocalIP = aLocalIP;
    return NS_OK;
}

nsresult
nsLoadAttribs::GetLocalIP(PRUint32 *aLocalIP) {
    *aLocalIP = mLocalIP;
    return NS_OK;
}

// Creation routines
NS_NET nsresult NS_NewLoadAttribs(nsILoadAttribs** aInstancePtrResult) {
  nsILoadAttribs* it = new nsLoadAttribs();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kILoadAttribs, (void **) aInstancePtrResult);
}