/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsSimpleDataBase_h__
#define nsSimpleDataBase_h__

#include "nsIRDFDataBase.h"
#include "nsVoidArray.h"

class nsSimpleDataBase : public nsIRDFDataBase {
protected:
    nsVoidArray mDataSources;
    virtual ~nsSimpleDataBase(void);

public:
    nsSimpleDataBase(void);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource interface
    NS_IMETHOD Init(const nsString& uri);

    NS_IMETHOD GetSource(nsIRDFNode* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFNode*& source);

    NS_IMETHOD GetSources(nsIRDFNode* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFCursor*& sources);

    NS_IMETHOD GetTarget(nsIRDFNode* source,
                         nsIRDFNode* property,
                         PRBool tv,
                         nsIRDFNode*& target);

    NS_IMETHOD GetTargets(nsIRDFNode* source,
                          nsIRDFNode* property,
                          PRBool tv,
                          nsIRDFCursor*& targets);

    NS_IMETHOD Assert(nsIRDFNode* source, 
                      nsIRDFNode* property, 
                      nsIRDFNode* target,
                      PRBool tv = PR_TRUE);

    NS_IMETHOD Unassert(nsIRDFNode* source,
                        nsIRDFNode* property,
                        nsIRDFNode* target);

    NS_IMETHOD HasAssertion(nsIRDFNode* source,
                            nsIRDFNode* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool& hasAssertion);

    NS_IMETHOD AddObserver(nsIRDFObserver* n);

    NS_IMETHOD RemoveObserver(nsIRDFObserver* n);

    NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                           nsIRDFCursor*& labels);

    NS_IMETHOD ArcLabelsOut(nsIRDFNode* source,
                            nsIRDFCursor*& labels);

    NS_IMETHOD Flush();

    // nsIRDFDataBase interface
    NS_IMETHOD AddDataSource(nsIRDFDataSource* source);
    NS_IMETHOD RemoveDataSource(nsIRDFDataSource* source);
};


#endif // nsSimpleDataBase_h__

