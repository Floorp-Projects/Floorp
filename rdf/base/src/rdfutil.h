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
 * The dreaded is-a function. Uses rdf:instanceOf to determine if aResource is aType.
 */
PR_EXTERN(PRBool)
rdf_IsA(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource, nsIRDFResource* aType);

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


PR_EXTERN(void) SHTtest ();
                       
#endif // rdfutil_h__


