/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Justin Bradford <jab@atdot.org>
 */

#ifndef _NSSOCKSSOCKETPROVIDER_H_
#define _NSSOCKSSOCKETPROVIDER_H_

#include "nsISOCKSSocketProvider.h"


/* 8dbe7246-1dd2-11b2-9b8f-b9a849e4403a */
#define NS_SOCKSSOCKETPROVIDER_CID { 0x8dbe7246, 0x1dd2, 0x11b2, {0x9b, 0x8f, 0xb9, 0xa8, 0x49, 0xe4, 0x40, 0x3a}}

class nsSOCKSSocketProvider : public nsISOCKSSocketProvider
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISOCKETPROVIDER
    NS_DECL_NSISOCKSSOCKETPROVIDER
    
    // nsSOCKSSocketProvider methods:
    nsSOCKSSocketProvider();
    virtual ~nsSOCKSSocketProvider();
    
    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);
    
    nsresult Init();
    
protected:
};

#endif /* _NSSOCKSSOCKETPROVIDER_H_ */
