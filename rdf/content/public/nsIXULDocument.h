/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*

  An XUL-specific extension to nsIXMLDocument. Includes methods for
  setting the root resource of the document content model, a factory
  method for constructing the children of a node, etc.

  XXX This should really be called nsIXULDocument.

 */

#ifndef nsIXULDocument_h___
#define nsIXULDocument_h___

class nsIContent; // XXX nsIXMLDocument.h is bad and doesn't declare this class...

#include "nsIXMLDocument.h"

class nsForwardReference;
class nsIAtom;
class nsIDOMElement;
class nsIDOMHTMLFormElement;
class nsIPrincipal;
class nsIRDFContentModelBuilder;
class nsIRDFResource;
class nsISupportsArray;
class nsIXULPrototypeDocument;
class nsIURI;

// {954F0811-81DC-11d2-B52A-000000000000}
#define NS_IRDFDOCUMENT_IID \
{ 0x954f0811, 0x81dc, 0x11d2, { 0xb5, 0x2a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }

/**
 * RDF document extensions to nsIDocument
 */

class nsIRDFDataSource;
class nsIXULPrototypeDocument;

class nsIXULDocument : public nsIXMLDocument
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IRDFDOCUMENT_IID; return iid; }

  // The resource-to-element map is a one-to-many mapping of RDF
  // resources to content elements.

  /**
   * Add an entry to the ID-to-element map.
   */
  NS_IMETHOD AddElementForID(const nsAReadableString& aID, nsIContent* aElement) = 0;

  /**
   * Remove an entry from the ID-to-element map.
   */
  NS_IMETHOD RemoveElementForID(const nsAReadableString& aID, nsIContent* aElement) = 0;

  /**
   * Get the elements for a particular resource in the resource-to-element
   * map. The nsISupportsArray will be truncated and filled in with
   * nsIContent pointers.
   */
  NS_IMETHOD GetElementsForID(const nsAReadableString& aID, nsISupportsArray* aElements) = 0;

  NS_IMETHOD CreateContents(nsIContent* aElement) = 0;

  /**
   * Add a content model builder to the document.
   */
  NS_IMETHOD AddContentModelBuilder(nsIRDFContentModelBuilder* aBuilder) = 0;

  /**
   * Manipulate the XUL document's form element
   */
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm) = 0;
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm) = 0;

  /**
   * Add a "forward declaration" of a XUL observer. Such declarations
   * will be resolved when document loading completes.
   */
  NS_IMETHOD AddForwardReference(nsForwardReference* aForwardReference) = 0;

  /**
   * Resolve the all of the document's forward references.
   */
  NS_IMETHOD ResolveForwardReferences() = 0;

  /**
   * Set the master prototype.
   */
  NS_IMETHOD SetMasterPrototype(nsIXULPrototypeDocument* aDocument) = 0;

  /**
   * Get the master prototype.
   */
  NS_IMETHOD GetMasterPrototype(nsIXULPrototypeDocument** aPrototypeDocument) = 0;

  /**
   * Set the current prototype
   */
  NS_IMETHOD SetCurrentPrototype(nsIXULPrototypeDocument* aDocument) = 0;

  /**
   * Set the doc's URL
   */
  NS_IMETHOD SetDocumentURL(nsIURI* aURI) = 0;

  /**
   * Load inline and attribute style sheets
   */
  NS_IMETHOD PrepareStyleSheets(nsIURI* aURI) = 0;

  /**
   * Indicate that this doc will be used only to load a key binding
   * document.
   */
  NS_IMETHOD SetIsKeybindingDocument(PRBool aIsKeyBindingDoc) = 0;
};

// factory functions
nsresult NS_NewXULDocument(nsIXULDocument** result);

#endif // nsIXULDocument_h___
