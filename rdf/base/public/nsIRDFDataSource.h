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

/*

  The RDF data source interface. An RDF data source presents a
  graph-like interface to a back-end service.

 */

#ifndef nsIRDFDataSource_h__
#define nsIRDFDataSource_h__

#include "nsISupports.h"
#include "nsISupportsArray.h"
#include "nsIEnumerator.h"

// {0F78DA58-8321-11d2-8EAC-00805F29F370}
#define NS_IRDFDATASOURCE_IID \
{ 0xf78da58, 0x8321, 0x11d2, { 0x8e, 0xac, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

class nsIRDFAssertionCursor;
class nsIRDFArcsInCursor;
class nsIRDFArcsOutCursor;
class nsIRDFDataBase;
class nsIRDFNode;
class nsIRDFObserver;
class nsIRDFResource;
class nsIRDFResourceCursor;

/**
 * An RDF data source.
 */

class nsIRDFDataSource : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IRDFDATASOURCE_IID; return iid; }

    // XXX I didn't make some of these methods "const" because it's
    // probably pretty likely that many data sources will just make
    // stuff up at runtime to answer queries.

    /**
     * Specify the URI for the data source: this is the prefix
     * that will be used to register the data source in the
     * data source registry.
     */
    NS_IMETHOD Init(const char* uri) = 0;

    /**
     * Retrieve the URI of the data source.
     */
    NS_IMETHOD GetURI(const char* *uri) const = 0;

    /**
     * Find an RDF resource that points to a given node over the
     * specified arc & truth value
     *
     * @return NS_ERROR_FAILURE if there is no source that leads
     * to the target with the specified property.
     */
    NS_IMETHOD GetSource(nsIRDFResource* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFResource** source /* out */) = 0;

    /**
     * Find all RDF resources that point to a given node over the
     * specified arc & truth value
     *
     * @return NS_OK unless a catastrophic error occurs. If the
     * method returns NS_OK, you may assume that nsIRDFCursor points
     * to a valid (but possibly empty) cursor.
     */
    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFAssertionCursor** sources /* out */) = 0;

    /**
     * Find a child of that is related to the source by the given arc
     * arc and truth value
     *
     * @return NS_ERROR_FAILURE if there is no target accessable from the
     * source via the specified property.
     */
    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target /* out */) = 0;

    /**
     * Find all children of that are related to the source by the given arc
     * arc and truth value.
     *
     * @return NS_OK unless a catastrophic error occurs. If the
     * method returns NS_OK, you may assume that nsIRDFCursor points
     * to a valid (but possibly empty) cursor.
     */
    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsIRDFAssertionCursor** targets /* out */) = 0;

    /**
     * Add an assertion to the graph.
     */
    NS_IMETHOD Assert(nsIRDFResource* source, 
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv) = 0;

    /**
     * Remove an assertion from the graph.
     */
    NS_IMETHOD Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target) = 0;

    /**
     * Query whether an assertion exists in this graph.
     *
     * @return NS_OK unless a catastrophic error occurs.
     */
    NS_IMETHOD HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion /* out */) = 0;

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
     *
     * @return NS_OK unless a catastrophic error occurs. If the method
     * returns NS_OK, you may assume that labels points to a valid (but
     * possible empty) nsIRDFCursor object.
     */
    NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                           nsIRDFArcsInCursor** labels /* out */) = 0;

    /**
     * Get a cursor to iterate over all the arcs that originate in
     * a resource.
     *
     * @return NS_OK unless a catastrophic error occurs. If the method
     * returns NS_OK, you may assume that labels points to a valid (but
     * possible empty) nsIRDFCursor object.
     */
    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsIRDFArcsOutCursor** labels /* out */) = 0;

    /**
     * Retrieve all of the resources that the data source currently
     * refers to.
     */
    NS_IMETHOD GetAllResources(nsIRDFResourceCursor** aCursor) = 0;

    /**
     * Request that a data source write it's contents out to 
     * permanent storage if applicable.
     */
    NS_IMETHOD Flush(void) = 0;

    /**
     * Returns the set of all commands defined for a given source.
     */
    NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                              nsIEnumerator/*<nsIRDFResource>*/** commands) = 0;

    /**
     * Returns whether a given command is enabled for a set of sources. 
     *
     * @return NS_OK if command is enabled for all sources 
     * @return NS_COMFALSE if command is not enabled for some source 
     */
    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments) = 0;

    /**
     * Perform the specified command on set of sources.
     */
    NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                         nsIRDFResource*   aCommand,
                         nsISupportsArray/*<nsIRDFResource>*/* aArguments) = 0;
};

#endif /* nsIRDFDataSource_h__ */
