#include "nscore.h"
#include "prtypes.h"
#include "nsISupports.h"

#ifndef nsILoadAttribs_h___
#define nsILoadAttribs_h___

// Class ID for an implementation of nsILoadAttribs
// {8942D321-48D3-11d2-9E7A-006008BF092E}
#define NS_ILOAD_ATTRIBS_IDD \
 { 0x8942d321, 0x48d3, 0x11d2,{0x9e, 0x7a, 0x00, 0x60, 0x08, 0xbf, 0x09, 0x2e}}

// Defining attributes of a url's load behavior.
class nsILoadAttribs : public nsISupports {
public:
    // Bypass Proxy.
    NS_IMETHOD SetBypassProxy(PRBool aBypass) = 0;
    NS_IMETHOD GetBypassProxy(PRBool *aBypass) = 0;

    // Local IP address.
    NS_IMETHOD SetLocalIP(const PRUint32 aLocalIP) = 0;
    NS_IMETHOD GetLocalIP(PRUint32 *aLocalIP) = 0;
};

/* Creation routines. */

extern NS_NET nsresult NS_NewLoadAttribs(nsILoadAttribs** aInstancePtrResult);

#endif // nsILoadAttribs_h___
