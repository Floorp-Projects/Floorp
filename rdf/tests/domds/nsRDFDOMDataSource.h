/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

#ifndef __nsRDFDOMDataSource_h
#define __nsRDFDOMDataSource_h

#include "nsIRDFDataSource.h"

#include "nsIDOMDataSource.h"
#include "nsIRDFService.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"

#include "nsIDOMDocument.h"
// 
#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsIStyledContent.h"
#include "nsIDOMCSSRule.h"
#include "nsIDOMCSSStyleRule.h"
#include "nsIDOMHTMLElement.h"

#include "nsICSSStyleRule.h"
#include "nsIStyleRule.h"
#include "nsIFrame.h"

/* {c7cf77e8-245a-11d3-80f0-006008948010} */
#define NS_RDF_DOMDATASOURCE_CID \
  {0xc7cf77e8, 0x245a, 0x11d3, \
    {0x80, 0xf0, 0x00, 0x60, 0x08, 0x94, 0x80, 0x10}}

class nsRDFDOMDataSource : public nsIRDFDataSource,
                           public nsIDOMDataSource
{
 public:
  nsRDFDOMDataSource();
  virtual ~nsRDFDOMDataSource();
  
  NS_DECL_ISUPPORTS

  NS_DECL_NSIRDFDATASOURCE

  NS_DECL_NSIDOMDATASOURCE
    
  static NS_METHOD  Create(nsISupports* aOuter,
                       const nsIID& iid,
                       void **result);

 protected:
    char *mURI;

	nsIRDFService *getRDFService();
	static PRBool assertEnumFunc(void *aElement, void *aData);
	static PRBool unassertEnumFunc(void *aElement, void *aData);
	nsresult  NotifyObservers(nsIRDFResource *subject, nsIRDFResource *property,
								nsIRDFNode *object, PRBool assert);

 private:

    // HTML stuff
    nsresult createHTMLElementArcs(nsIDOMHTMLElement *element,
                                   nsISupportsArray *arcs);

    nsresult createFrameArcs(nsIFrame *frame,
                             nsISupportsArray *arcs);
    // DOM stuff
    nsresult createDOMNodeArcs(nsIDOMNode *node,
                               nsISupportsArray *arcs);
    nsresult createDOMAttributeArcs(nsIDOMNode *node,
                               nsISupportsArray *arcs);
    nsresult createDOMChildArcs(nsIDOMNode *node,
                                nsISupportsArray *arcs);

    nsresult createDOMNodeListArcs(nsIDOMNodeList *nodelist,
                                   nsISupportsArray *arcs);
    nsresult createDOMNamedNodeMapArcs(nsIDOMNamedNodeMap *nodelist,
                                       nsISupportsArray *arcs);


    nsresult createFrameTarget(nsIFrame *frame, 
                                 nsIRDFResource *property,
                                 nsIRDFNode **aResult);
    nsresult createDOMNodeTarget(nsIDOMNode *node,
                                 nsIRDFResource *property,
                                 nsIRDFNode **aResult);
    // nsIContent stuff
    nsresult createContentArcs(nsIContent *content,
                               nsISupportsArray *arcs);
    nsresult createContentChildArcs(nsIContent *content,
                                    nsISupportsArray *arcs);
    nsresult createContentAttributeArcs(nsIContent *content,
                                        nsISupportsArray *arcs);
    nsresult createContentMiscArcs(nsIContent* content,
                                   nsISupportsArray *arcs);
    nsresult createContentTarget(nsIContent *content,
                                 nsIRDFResource *property,
                                 nsIRDFNode **aResult);
    
    
    // nsIStyledContent stuff
    nsresult createStyledContentArcs(nsIStyledContent *content,
                                     nsISupportsArray *arcs);
    nsresult createStyledContentClassArcs(nsIStyledContent *content,
                                          nsISupportsArray *arcs);

    nsresult createStyledContentTarget(nsIStyledContent *content,
                                       nsIRDFResource *property,
                                       nsIRDFNode **aResult);

    // nsIDOMCSSStyleDeclaration
    nsresult getDOMCSSStyleDeclTarget(nsIDOMCSSStyleDeclaration *decl,
                                          nsIRDFResource *property,
                                          nsIRDFNode **aResult);
    
    // nsIDOMCSSStyleRule
    // nsIDOMCSSRule
    nsresult getDOMCSSStyleRuleTarget(nsIDOMCSSStyleRule *rule,
                                      nsIRDFResource *property,
                                      nsIRDFNode **result);
    nsresult getDOMCSSRuleTarget(nsIDOMCSSRule *rule,
                                 nsIRDFResource *property,
                                 nsIRDFNode **result);
    
    
    // nsICSSStyleRule
    // nsICSSRule
    // nsIStyleRule
    nsresult getCSSStyleRuleTarget(nsICSSStyleRule *rule,
                              nsIRDFResource *property,
                              nsIRDFNode **result);
    
    nsresult getCSSRuleTarget(nsICSSRule *rule,
                              nsIRDFResource *property,
                              nsIRDFNode **result);
    
    nsresult getStyleRuleTarget(nsIStyleRule *rule,
                                   nsIRDFResource *property,
                                   nsIRDFNode **result);
    
    // helper routines
    nsresult createArcsFromSupportsArray(nsISupportsArray *rules,
                                          nsISupportsArray *arcs);
    nsresult getResourceForObject(nsISupports *object,
                                  nsIRDFResource **resource);

    nsresult appendLeafObject(nsString& name,
                              nsString& value,
                              nsISupportsArray *arcs);

    nsresult getTargetForKnownObject(nsISupports *object,
                                     nsIRDFResource* aProperty,
                                     nsIRDFNode **result);
    
    nsresult getTargetsForKnownObject(nsISupports *object,
                                      nsIRDFResource *aProperty,
                                      PRBool useDOM,
                                      nsISupportsArray *arcs);
    
    nsresult createLiteral(nsString& str, nsIRDFNode **result);
    // member variables
    PRBool init;
    nsIRDFService *mRDFService;
    PRInt32 mMode;
    nsVoidArray *mObservers;

    nsSupportsHashtable objectTable;
    nsHashtable mModeTable;
    
    nsCOMPtr<nsIDOMDocument> mDocument;
    nsIFrame *mRootFrame;

    nsIRDFResource* kNC_Name;
    nsIRDFResource* kNC_Value;
    nsIRDFResource* kNC_Type;
    nsIRDFResource* kNC_Child;
    nsIRDFResource* kNC_DOMRoot;

};

#endif
