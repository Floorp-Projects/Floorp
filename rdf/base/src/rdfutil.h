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


/*

  A bunch of useful RDF utility routines.  Many of these will
  eventually be exported outside of RDF.DLL via the nsIRDFService
  interface.

  TO DO

  1) Create an nsIRDFContainerService and lump all the container
     utilities there.

  2) Move the anonymous resource stuff to nsIRDFService

  3) All that's left is rdf_PossiblyMakeRelative() and
     -Absolute(). Maybe those go on nsIRDFService, too.

 */

#ifndef rdfutil_h__
#define rdfutil_h__

#include "prtypes.h"

class nsIRDFDataSource;
class nsISimpleEnumerator;
class nsIRDFResource;
class nsIRDFNode;
class nsIRDFResource;
class nsString;

/**
 * Returns PR_TRUE if the URI is an RDF ordinal property; e.g., rdf:_1,
 * rdf:_2, etc.
 */
PR_EXTERN(PRBool)
rdf_IsOrdinalProperty(nsIRDFResource* property);

/**
 * Converts an ordinal property to an index
 */
PR_EXTERN(nsresult)
rdf_OrdinalResourceToIndex(nsIRDFResource* aOrdinal, PRInt32* aIndex);

/**
 * Converts an index to an ordinal property
 */
PR_EXTERN(nsresult)
rdf_IndexToOrdinalResource(PRInt32 aIndex, nsIRDFResource** aOrdinal);


/**
 * Returns PR_TRUE if the resource is a container resource; e.g., an
 * rdf:Bag.
 */
PR_EXTERN(PRBool)
rdf_IsContainer(nsIRDFDataSource* db,
                nsIRDFResource* resource);

PR_EXTERN(PRBool)
rdf_IsBag(nsIRDFDataSource* aDataSource,
          nsIRDFResource* aResource);

PR_EXTERN(PRBool)
rdf_IsSeq(nsIRDFDataSource* aDataSource,
          nsIRDFResource* aResource);

PR_EXTERN(PRBool)
rdf_IsAlt(nsIRDFDataSource* aDataSource,
          nsIRDFResource* aResource);

/**
 * Construct a new, "anonymous" node; that is, a node with an internal
 * resource URI.
 */
PR_EXTERN(nsresult)
rdf_CreateAnonymousResource(const nsString& aContextURI, nsIRDFResource** result);

/**
 * Determine if a resource is an "anonymous" resource that we've constructed
 * ourselves.
 */
PR_EXTERN(PRBool)
rdf_IsAnonymousResource(const nsString& aContextURI, nsIRDFResource* aResource);

/**
 * Try to convert the absolute URL into a relative URL.
 */
PR_EXTERN(nsresult)
rdf_PossiblyMakeRelative(const nsString& aContextURI, nsString& aURI);


/**
 * Try to convert the possibly-relative URL into an absolute URL.
 */
PR_EXTERN(nsresult)
rdf_PossiblyMakeAbsolute(const nsString& aContextURI, nsString& aURI);

/**
 * Create a bag resource.
 */
PR_EXTERN(nsresult)
rdf_MakeBag(nsIRDFDataSource* ds,
            nsIRDFResource* resource);

/**
 * Create a sequence resource.
 */
PR_EXTERN(nsresult)
rdf_MakeSeq(nsIRDFDataSource* ds,
            nsIRDFResource* resource);

/**
 * Create an alternation resource.
 */
PR_EXTERN(nsresult)
rdf_MakeAlt(nsIRDFDataSource* ds,
            nsIRDFResource* resource);

/**
 * Get the current upper bound of the container
 */
PR_EXTERN(nsresult)
rdf_ContainerGetCount(nsIRDFDataSource* aDataSource,
                      nsIRDFResource* aContainer,
                      PRInt32* aCount);

/**
 * Add an element to the end of container.
 */
PR_EXTERN(nsresult)
rdf_ContainerAppendElement(nsIRDFDataSource* ds,
                           nsIRDFResource* container,
                           nsIRDFNode* element);


/**
 * Remove an element from a container
 */
PR_EXTERN(nsresult)
rdf_ContainerRemoveElement(nsIRDFDataSource* aDataSource,
                           nsIRDFResource* aContainer,
                           nsIRDFNode* aElement,
                           PRBool aRenumber);


/**
 * Insert an element into a container at the specified index.
 */
PR_EXTERN(nsresult)
rdf_ContainerInsertElementAt(nsIRDFDataSource* aDataSource,
                             nsIRDFResource* aContainer,
                             nsIRDFNode* aElement,
                             PRInt32 aIndex,
                             PRBool aRenumber);

/**
 * Determine the index of an element in a container.
 */
PR_EXTERN(nsresult)
rdf_ContainerIndexOf(nsIRDFDataSource* aDataSource,
                     nsIRDFResource* aContainer,
                     nsIRDFNode* aElement,
                     PRInt32* aIndex);


/**
 * Create a cursor on a container that enumerates its contents in
 * order
 */
PR_EXTERN(nsresult)
NS_NewContainerEnumerator(nsIRDFDataSource* aDataSource,
                          nsIRDFResource* aContainer,
                          nsISimpleEnumerator** aResult);


PR_EXTERN(void) SHTtest ();
                       
#endif // rdfutil_h__


