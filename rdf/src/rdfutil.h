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
rdf_IsOrdinalProperty(const nsIRDFNode* property);


/**
 * Returns PR_TRUE if the resource is a container resource; e.g., an
 * rdf:Bag.
 */
PRBool
rdf_IsContainer(nsIRDFResourceManager* mgr,
                nsIRDFDataSource* db,
                nsIRDFNode* resource);

/**
 * Tries to guess whether the specified node is a resource or a literal.
 */
PRBool
rdf_IsResource(nsIRDFNode* node);


/**
 * Various utilities routines for making assertions in a data source
 */

// 0. node, node, node
nsresult
rdf_Assert(nsIRDFDataSource* ds,
           nsIRDFNode* subject,
           nsIRDFNode* predicate,
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
           nsIRDFNode* subject,
           nsIRDFNode* predicate,
           const nsString& objectURI);

// 3. node, string, string
nsresult
rdf_Assert(nsIRDFResourceManager* mgr,
           nsIRDFDataSource* ds,
           nsIRDFNode* subject,
           const nsString& predicateURI,
           const nsString& objectURI);

// 4. node, string, node
nsresult
rdf_Assert(nsIRDFResourceManager* mgr,
           nsIRDFDataSource* ds,
           nsIRDFNode* subject,
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
rdf_CreateAnonymousNode(nsIRDFResourceManager* mgr,
                        nsIRDFNode*& result);


/**
 * Create a bag resource.
 */
nsresult
rdf_CreateBag(nsIRDFResourceManager* mgr,
              nsIRDFDataSource* ds,
              nsIRDFNode*& result);

/**
 * Create a sequence resource.
 */
nsresult
rdf_CreateSequence(nsIRDFResourceManager* mgr,
                   nsIRDFDataSource* ds,
                   nsIRDFNode*& result);

/**
 * Create an alternation resource.
 */
nsresult
rdf_CreateAlternation(nsIRDFResourceManager* mgr,
                      nsIRDFDataSource* ds,
                      nsIRDFNode*& result);


/**
 * Add an element to the container.
 */
nsresult
rdf_ContainerAddElement(nsIRDFResourceManager* mgr,
                        nsIRDFDataSource* ds,
                        nsIRDFNode* container,
                        nsIRDFNode* element);

/**
 * Add an element to the container.
 */
nsresult
rdf_ContainerAddElement(nsIRDFResourceManager* mgr,
                        nsIRDFDataSource* ds,
                        nsIRDFNode* container,
                        const nsString& literalOrURI);

/**
 * Return <tt>PR_TRUE</tt> if the specified resources are equal.
 */
PRBool
rdf_ResourceEquals(nsIRDFResourceManager* mgr,
                   nsIRDFNode* resource,
                   const nsString& uri);


/**
 * Create a cursor on a container that enumerates its contents in
 * order
 */
nsresult
NS_NewContainerCursor(nsIRDFDataSource* ds,
                      nsIRDFNode* container,
                      nsIRDFCursor*& cursor);


// XXX need to move nsEmptyCursor stuff here.
                       
#endif // rdfutil_h__


