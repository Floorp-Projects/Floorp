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

class nsIRDFDataBase;
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
                nsIRDFDataBase* db,
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


nsresult
rdf_CreateAnonymousNode(nsIRDFResourceManager* mgr,
                        nsIRDFNode*& result);


nsresult
rdf_CreateBag(nsIRDFResourceManager* mgr,
              nsIRDFDataSource* ds,
              nsIRDFNode*& result);


nsresult
rdf_ContainerGetNextValue(nsIRDFResourceManager* mgr,
                          nsIRDFDataSource* ds,
                          nsIRDFNode* container,
                          PRInt32& result);


nsresult
rdf_ContainerAddElement(nsIRDFResourceManager* mgr,
                        nsIRDFDataSource* ds,
                        nsIRDFNode* container,
                        nsIRDFNode* element);

nsresult
rdf_ContainerAddElement(nsIRDFResourceManager* mgr,
                        nsIRDFDataSource* ds,
                        nsIRDFNode* container,
                        const nsString& literalOrURI);

PRBool
rdf_ResourceEquals(nsIRDFResourceManager* mgr,
                   nsIRDFNode* resource,
                   const nsString& uri);

#endif // rdfutil_h__


