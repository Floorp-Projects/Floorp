/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 */

#ifndef ___nsNetModuleMgr_h__
#define ___nsNetModuleMgr_h__

#include "nsINetModuleMgr.h"
#include "prlock.h"
#include "nspr.h"
#include "nsISupportsArray.h"

class nsNetModuleMgr : public nsINetModuleMgr {
protected:
    static nsNetModuleMgr* gManager;

public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsINetModuleMgr
    NS_DECL_NSINETMODULEMGR
    
    // nsNetModuleMgr
    nsNetModuleMgr();
    virtual ~nsNetModuleMgr();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);


    nsISupportsArray   *mEntries;
    PRMonitor*          mMonitor;
};


#endif // ___nsNetModuleMgr_h__
