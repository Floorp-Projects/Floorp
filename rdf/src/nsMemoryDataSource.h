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

#ifndef nsMemoryDataSource_h__
#define nsMemoryDataSource_h__

#include "prtypes.h"
#include "nsIRDFDataSource.h"
#include "nsHashtable.h"

class nsIRDFNode;
class nsIRDFCursor;
class nsIRDFRegistry;
class NodeImpl;

class nsMemoryDataSource : public nsIRDFDataSource {
protected:
    nsHashtable mNodes;
    nsIRDFRegistry* mRegistry;

    NodeImpl* Ensure(nsIRDFNode* node);

public:
    nsMemoryDataSource(void);
    virtual ~nsMemoryDataSource(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource methods
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
};


#endif // nsMemoryDataSource_h__
