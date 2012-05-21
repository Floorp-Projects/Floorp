/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#ifndef nsRDFService_h__
#define nsRDFService_h__

#include "nsIRDFService.h"
#include "nsWeakReference.h"
#include "nsIFactory.h"
#include "nsCOMPtr.h"
#include "pldhash.h"
#include "nsString.h"

struct PLHashTable;
class nsIRDFLiteral;
class nsIRDFInt;
class nsIRDFDate;
class BlobImpl;

class RDFServiceImpl : public nsIRDFService,
                       public nsSupportsWeakReference
{
protected:
    PLHashTable* mNamedDataSources;
    PLDHashTable mResources;
    PLDHashTable mLiterals;
    PLDHashTable mInts;
    PLDHashTable mDates;
    PLDHashTable mBlobs;

    nsCAutoString mLastURIPrefix;
    nsCOMPtr<nsIFactory> mLastFactory;
    nsCOMPtr<nsIFactory> mDefaultResourceFactory;

    RDFServiceImpl();
    nsresult Init();
    virtual ~RDFServiceImpl();

public:
    static RDFServiceImpl *gRDFService NS_VISIBILITY_HIDDEN;
    static nsresult CreateSingleton(nsISupports* aOuter,
                                    const nsIID& aIID, void **aResult);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFService
    NS_DECL_NSIRDFSERVICE

    // Implementation methods
    nsresult RegisterLiteral(nsIRDFLiteral* aLiteral);
    nsresult UnregisterLiteral(nsIRDFLiteral* aLiteral);
    nsresult RegisterInt(nsIRDFInt* aInt);
    nsresult UnregisterInt(nsIRDFInt* aInt);
    nsresult RegisterDate(nsIRDFDate* aDate);
    nsresult UnregisterDate(nsIRDFDate* aDate);
    nsresult RegisterBlob(BlobImpl* aBlob);
    nsresult UnregisterBlob(BlobImpl* aBlob);

    nsresult GetDataSource(const char *aURI, bool aBlock, nsIRDFDataSource **aDataSource );
};

#endif // nsRDFService_h__
