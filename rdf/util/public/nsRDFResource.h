/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
