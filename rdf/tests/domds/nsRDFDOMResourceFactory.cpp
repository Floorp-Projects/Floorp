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

#include "nscore.h"
#include "nsCOMPtr.h"

#include "rdf.h"
#include "nsRDFResource.h"
#include "nsRDFDOMResourceFactory.h"
#include "nsIDOMViewerElement.h"
#include "nsIDOMNode.h"

class nsRDFDOMViewerElement : nsRDFResource,
                              nsIDOMViewerElement {
public:
  nsRDFDOMViewerElement();
  virtual ~nsRDFDOMViewerElement();
  
  NS_DECL_ISUPPORTS_INHERITED
  
  NS_DECL_NSIDOMVIEWERELEMENT

private:
  
  nsIDOMNode *mNode;


};

nsRDFDOMViewerElement::nsRDFDOMViewerElement() :
  mNode(nsnull)
{

}

nsRDFDOMViewerElement::~nsRDFDOMViewerElement()
{
  if (mNode) NS_RELEASE(mNode);
}

NS_IMPL_ISUPPORTS_INHERITED(nsRDFDOMViewerElement, nsRDFResource, nsIDOMViewerElement)

NS_IMETHODIMP
nsRDFDOMViewerElement::SetNode(nsIDOMNode* node)
{
  if (mNode) NS_RELEASE(mNode);
  mNode = node;
  NS_ADDREF(mNode);
  return NS_OK;
}

NS_IMETHODIMP
nsRDFDOMViewerElement::GetNode(nsIDOMNode** node)
{
  if (!node) return NS_ERROR_NULL_POINTER;

  *node = mNode;
  NS_ADDREF(*node);
  return NS_OK;
}
  
nsresult
NS_NewRDFDOMResourceFactory(nsISupports* aOuter,
                            const nsIID& iid,
                            void **result) {
  
  nsRDFDOMViewerElement* ve = new nsRDFDOMViewerElement();
  if (!ve) return NS_ERROR_NULL_POINTER;
  return ve->QueryInterface(iid, result);
}

