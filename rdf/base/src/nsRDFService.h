/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
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
