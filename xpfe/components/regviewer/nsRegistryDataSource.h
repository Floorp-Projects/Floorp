/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsregistrydatasource___h____
#define nsregistrydatasource___h____

#include "nsCOMPtr.h"
#include "nsIRDFDataSource.h"
#include "nsEnumeratorUtils.h"
#include "nsIRegistryDataSource.h"
#include "nsIRegistry.h"
#include "nsIServiceManager.h"
#include "nsIRDFService.h"

class nsRegistryDataSource : public nsIRDFDataSource,
                             public nsIRegistryDataSource
{
public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIRDFDATASOURCE

    NS_DECL_NSIREGISTRYDATASOURCE

    // Implementation methods
    PRInt32 GetKey(nsIRDFResource* aResource);

    nsCOMPtr<nsIRegistry> mRegistry;
    nsCOMPtr<nsISupportsArray> mObservers;

    static nsrefcnt gRefCnt;
    static nsIRDFService* gRDF;

    static nsIRDFResource* kKeyRoot;
    static nsIRDFResource* kSubkeys;
    static nsIRDFLiteral*  kBinaryLiteral;

    class SubkeyEnumerator : public nsISimpleEnumerator
    {
    public:
        NS_DECL_ISUPPORTS

        NS_DECL_NSISIMPLEENUMERATOR

        static nsresult
        Create(nsRegistryDataSource* aViewer, nsIRDFResource* aRootKey, nsISimpleEnumerator** aResult);

    protected:
        nsRegistryDataSource*        mViewer;
        nsCOMPtr<nsIRDFResource> mRootKey;
        nsCOMPtr<nsIEnumerator>  mEnum;
        nsCOMPtr<nsIRDFResource> mCurrent;
        PRBool                   mStarted;

        SubkeyEnumerator(nsRegistryDataSource* aViewer, nsIRDFResource* aRootKey);
        virtual ~SubkeyEnumerator();
        nsresult Init();

        nsresult
        ConvertRegistryNodeToResource(nsISupports* aRegistryNode, nsIRDFResource** aResult);
    };

    nsRegistryDataSource();
    virtual ~nsRegistryDataSource();
    nsresult Init();
};

#endif // nsregistrydatasource___h____
