/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

  An RDF-specific extension to nsIXMLDocument. Includes methods for
  setting the root resource of the document content model, a factory
  method for constructing the children of a node, etc.

 */

#ifndef nsIRDFDocument_h___
#define nsIRDFDocument_h___

class nsIContent; // XXX nsIXMLDocument.h is bad and doesn't declare this class...

#include "nsIXMLDocument.h"

class nsIAtom;
class nsIRDFCompositeDataSource;
class nsIRDFContent;
class nsIRDFContentModelBuilder;
class nsISupportsArray;
class nsIRDFResource;

// {954F0811-81DC-11d2-B52A-000000000000}
#define NS_IRDFDOCUMENT_IID \
{ 0x954f0811, 0x81dc, 0x11d2, { 0xb5, 0x2a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }

/**
 * RDF document extensions to nsIDocument
 */

class nsIRDFDataSource;

class nsIRDFDocument : public nsIXMLDocument
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IRDFDOCUMENT_IID; return iid; }

  /**
   * Set the document's "root" resource.
   */
  NS_IMETHOD SetRootResource(nsIRDFResource* aResource) = 0;

  // XXX the following two methods should probably accept strings as
  // parameters so you can mess with them via JS. Also, should they
  // take a "notify" parameter that would control whether any viewers
  // of the content model should be informed that the content model is
  // invalid?

  NS_IMETHOD SplitProperty(nsIRDFResource* aResource,
                           PRInt32* aNameSpaceID,
                           nsIAtom** aTag) = 0;

  NS_IMETHOD AddElementForResource(nsIRDFResource* aResource, nsIContent* aElement) = 0;

  NS_IMETHOD RemoveElementForResource(nsIRDFResource* aResource, nsIContent* aElement) = 0;

  NS_IMETHOD GetElementsForResource(nsIRDFResource* aResource, nsISupportsArray* aElements) = 0;

  NS_IMETHOD CreateContents(nsIContent* aElement) = 0;

  NS_IMETHOD AddContentModelBuilder(nsIRDFContentModelBuilder* aBuilder) = 0;

  NS_IMETHOD GetDocumentDataSource(nsIRDFDataSource** aDatasource) = 0;
};

// factory functions
nsresult NS_NewXULDocument(nsIRDFDocument** result);

#endif // nsIRDFDocument_h___
