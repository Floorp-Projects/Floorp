/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDebug.h"
#include "nsIDebug2.h"

class nsDebugImpl : public nsIDebug2
{
public:
    nsDebugImpl() {}
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDEBUG
    NS_DECL_NSIDEBUG2
    
    static nsresult Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    /*
     * Inform nsDebugImpl that we're in multiprocess mode.
     *
     * If aDesc is not NULL, the string it points to must be
     * statically-allocated (i.e., it must be a string literal).
     */
    static void SetMultiprocessMode(const char *aDesc);
};


#define NS_DEBUG_CONTRACTID "@mozilla.org/xpcom/debug;1"
#define NS_DEBUG_CLASSNAME  "nsDebug Interface"
#define NS_DEBUG_CID                                 \
{ /* a80b1fb3-aaf6-4852-b678-c27eb7a518af */         \
  0xa80b1fb3,                                        \
    0xaaf6,                                          \
    0x4852,                                          \
    {0xb6, 0x78, 0xc2, 0x7e, 0xb7, 0xa5, 0x18, 0xaf} \
}            
