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

#ifndef nsRDFResource_h__
#define nsRDFResource_h__

#include "nsCOMPtr.h"
#include "nsIRDFNode.h"
#include "nsIRDFResource.h"
#include "nscore.h"
#include "nsString.h"
#include "rdf.h"
class nsIRDFService;

/**
 * This simple base class implements nsIRDFResource, and can be used as a
 * superclass for more sophisticated resource implementations.
 */
class nsRDFResource : public nsIRDFResource {
public:

    NS_DECL_ISUPPORTS

    // nsIRDFNode methods:
    NS_IMETHOD EqualsNode(nsIRDFNode* aNode, PRBool* aResult);

    // nsIRDFResource methods:
    NS_IMETHOD Init(const char* aURI);
    NS_IMETHOD GetValue(char* *aURI);
    NS_IMETHOD GetValueConst(const char** aURI);
    NS_IMETHOD EqualsString(const char* aURI, PRBool* aResult);
    NS_IMETHOD GetDelegate(const char* aKey, REFNSIID aIID, void** aResult);
    NS_IMETHOD ReleaseDelegate(const char* aKey);

    // nsRDFResource methods:
    nsRDFResource(void);
    virtual ~nsRDFResource(void);

protected:
    static nsIRDFService* gRDFService;
    static nsrefcnt gRDFServiceRefCnt;

protected:
    char* mURI;

    struct DelegateEntry {
        nsCString             mKey;
        nsCOMPtr<nsISupports> mDelegate;
        DelegateEntry*        mNext;
    };

    DelegateEntry* mDelegates;
};

#endif // nsRDFResource_h__
