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

#ifndef nsIRDFDataSource_h__
#define nsIRDFDataSource_h__

/*
  This file contains the interface definition for an RDF data source.
*/

#include "nsISupports.h"

// {0F78DA58-8321-11d2-8EAC-00805F29F370}
#define NS_IRDFDATASOURCE_IID \
{ 0xf78da58, 0x8321, 0x11d2, { 0x8e, 0xac, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

class nsIRDFNode;
class nsIRDFCursor;
class nsIRDFObserver;
class nsIRDFDataBase;
class nsString;

// XXX I didn't make any of these methods "const" because it's
// probably pretty likely that many data sources will just make stuff
// up at runtime to answer queries.

class nsIRDFDataSource : public nsISupports {
public:
    /**
     * Specify the URI for the data source: this is the prefix
     * that will be used to register the data source in the
     * data source registry.
     */
    NS_IMETHOD Init(const nsString& uri) = 0;

    /**
     * Find an RDF resource that points to a given node over the
     * specified arc & truth value
     */
    NS_IMETHOD GetSource(nsIRDFNode* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFNode*& source /* out */) = 0;

    /**
     * Find all RDF resources that point to a given node over the
     * specified arc & truth value
     */
    NS_IMETHOD GetSources(nsIRDFNode* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFCursor*& sources /* out */) = 0;

    /**
     * Find a child of that is related to the source by the given arc
     * arc and truth value
     */
    NS_IMETHOD GetTarget(nsIRDFNode* source,
                         nsIRDFNode* property,
                         PRBool tv,
                         nsIRDFNode*& target /* out */) = 0;

    /**
     * Find all children of that are related to the source by the given arc
     * arc and truth value
     */
    NS_IMETHOD GetTargets(nsIRDFNode* source,
                          nsIRDFNode* property,
                          PRBool tv,
                          nsIRDFCursor*& targets /* out */) = 0;

    /**
     * Add an assertion to the graph.
     */
    NS_IMETHOD Assert(nsIRDFNode* source, 
                      nsIRDFNode* property, 
                      nsIRDFNode* target,
                      PRBool tv = PR_TRUE) = 0;

    /**
     * Remove an assertion from the graph.
     */
    NS_IMETHOD Unassert(nsIRDFNode* source,
                        nsIRDFNode* property,
                        nsIRDFNode* target) = 0;

    /**
     * Query whether an assertion exists in this graph.
     *
     */
    NS_IMETHOD HasAssertion(nsIRDFNode* source,
                            nsIRDFNode* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool& hasAssertion /* out */) = 0;

    /**
     * Add an observer to this data source.
     */
    NS_IMETHOD AddObserver(nsIRDFObserver* n) = 0;

    /**
     * Remove an observer from this data source
     */
    NS_IMETHOD RemoveObserver(nsIRDFObserver* n) = 0;

    // XXX individual resource observers?

    /**
     * Get a cursor to iterate over all the arcs that point into a node.
     */
    NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                           nsIRDFCursor*& labels /* out */) = 0;

    /**
     * Get a cursor to iterate over all the arcs that originate in
     * a resource.
     */
    NS_IMETHOD ArcLabelsOut(nsIRDFNode* source,
                            nsIRDFCursor*& labels /* out */) = 0;

    /**
     * Request that a data source write it's contents out to 
     * permanent storage if applicable.
     */
    NS_IMETHOD Flush() = 0;

};


#endif /* nsIRDFDataSource_h__ */
