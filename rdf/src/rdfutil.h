/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef rdfutil_h__
#define rdfutil_h__

#include "prtypes.h"

class nsIRDFCursor;
class nsIRDFCursor2;
class nsIRDFDataBase;
class nsIRDFDataSource;
class nsIRDFNode;
class nsIRDFResourceManager;
class nsString;

/**
 * Returns PR_TRUE if the URI is an RDF ordinal property; e.g., rdf:_1,
 * rdf:_2, etc.
 */
PRBool
rdf_IsOrdinalProperty(const nsIRDFResource* property);


/**
 * Returns PR_TRUE if the resource is a container resource; e.g., an
 * rdf:Bag.
 */
PRBool
rdf_IsContainer(nsIRDFResourceManager* mgr,
                nsIRDFDataSource* db,
                nsIRDFResource* resource);


/**
 * Various utilities routines for making assertions in a data source
 */

// 0. node, node, node
nsresult
rdf_Assert(nsIRDFResourceManager* mgr, // unused
           nsIRDFDataSource* ds,
           nsIRDFResource* subject,
           nsIRDFResource* predicate,
           nsIRDFNode* object);

// 1. string, string, string
nsresult
rdf_Assert(nsIRDFResourceManager* mgr,
           nsIRDFDataSource* ds,
           const nsString& subjectURI,
           const nsString& predicateURI,
           const nsString& objectURI);

// 2. node, node, string
nsresult
rdf_Assert(nsIRDFResourceManager* mgr,
           nsIRDFDataSource* ds,
           nsIRDFResource* subject,
           nsIRDFResource* predicate,
           const nsString& objectURI);

// 3. node, string, string
nsresult
rdf_Assert(nsIRDFResourceManager* mgr,
           nsIRDFDataSource* ds,
           nsIRDFResource* subject,
           const nsString& predicateURI,
           const nsString& objectURI);

// 4. node, string, node
nsresult
rdf_Assert(nsIRDFResourceManager* mgr,
           nsIRDFDataSource* ds,
           nsIRDFResource* subject,
           const nsString& predicateURI,
           nsIRDFNode* object);

// 5. string, string, node
nsresult
rdf_Assert(nsIRDFResourceManager* mgr,
           nsIRDFDataSource* ds,
           const nsString& subjectURI,
           const nsString& predicateURI,
           nsIRDFNode* object);

/**
 * Construct a new, "anonymous" node; that is, a node with an internal
 * resource URI.
 */
nsresult
rdf_CreateAnonymousResource(nsIRDFResourceManager* mgr,
                            nsIRDFResource** result);


/**
 * Create a bag resource.
 */
nsresult
rdf_MakeBag(nsIRDFResourceManager* mgr,
            nsIRDFDataSource* ds,
            nsIRDFResource* resource);

/**
 * Create a sequence resource.
 */
nsresult
rdf_MakeSeq(nsIRDFResourceManager* mgr,
            nsIRDFDataSource* ds,
            nsIRDFResource* resource);

/**
 * Create an alternation resource.
 */
nsresult
rdf_MakeAlt(nsIRDFResourceManager* mgr,
            nsIRDFDataSource* ds,
            nsIRDFResource* resource);


/**
 * Add an element to the container.
 */
nsresult
rdf_ContainerAddElement(nsIRDFResourceManager* mgr,
                        nsIRDFDataSource* ds,
                        nsIRDFResource* container,
                        nsIRDFNode* element);

/**
 * Create a cursor on a container that enumerates its contents in
 * order
 */
nsresult
NS_NewContainerCursor(nsIRDFDataSource* ds,
                      nsIRDFResource* container,
                      nsIRDFAssertionCursor** cursor);


/**
 * Create an empty nsIRDFCursor. This will *never* fail, and will *always*
 * return the same object.
 */
nsresult
NS_NewEmptyRDFAssertionCursor(nsIRDFAssertionCursor** result);

nsresult
NS_NewEmptyRDFArcsInCursor(nsIRDFArcsInCursor** result);

nsresult
NS_NewEmptyRDFArcsOutCursor(nsIRDFArcsOutCursor** result);


// XXX need to move nsEmptyCursor stuff here.
                       
#endif // rdfutil_h__


