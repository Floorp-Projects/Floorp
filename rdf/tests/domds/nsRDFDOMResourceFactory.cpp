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
#include "nsWeakPtr.h"
#include "rdf.h"
#include "nsRDFResource.h"
#include "nsRDFDOMResourceFactory.h"
#include "nsIDOMViewerElement.h"
#include "nsIDOMNode.h"

class nsRDFDOMViewerElement : nsRDFResource,
                              nsIDOMViewerElement
{
public:
  nsRDFDOMViewerElement();
  virtual ~nsRDFDOMViewerElement();
  
  NS_DECL_ISUPPORTS_INHERITED
  
  NS_DECL_NSIDOMVIEWERELEMENT

private:
      // weak reference to DOM node
      //nsWeakPtr mNode;
      nsCOMPtr<nsIDOMNode> mNode;
};

nsRDFDOMViewerElement::nsRDFDOMViewerElement()
{

}

nsRDFDOMViewerElement::~nsRDFDOMViewerElement()
{
}

NS_IMPL_ISUPPORTS_INHERITED(nsRDFDOMViewerElement, nsRDFResource, nsIDOMViewerElement)

NS_IMETHODIMP
nsRDFDOMViewerElement::SetNode(nsIDOMNode* node)
{
  mNode = node;
  return NS_OK;
}

NS_IMETHODIMP
nsRDFDOMViewerElement::GetNode(nsIDOMNode** node)
{
  if (!node) return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  if (mNode) {
      *node = mNode;
      NS_ADDREF(*node);
  }
  else {
      *node = nsnull;
  }
  return rv;
}
  
nsresult
NS_NewRDFDOMResourceFactory(nsISupports* aOuter,
                            const nsIID& iid,
                            void **result) {
  
  nsRDFDOMViewerElement* ve = new nsRDFDOMViewerElement();
  if (!ve) return NS_ERROR_NULL_POINTER;
  NS_ADDREF(ve);
  nsresult rv = ve->QueryInterface(iid, result);
  NS_RELEASE(ve);
  return rv;
}

