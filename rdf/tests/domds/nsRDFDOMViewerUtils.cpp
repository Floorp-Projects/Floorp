/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFDOMViewerUtils.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

nsDOMViewerObject::nsDOMViewerObject() {}

nsDOMViewerObject::~nsDOMViewerObject()
{
  targets.Reset();
}

NS_IMPL_ISUPPORTS1(nsDOMViewerObject,
                   nsIRDFDOMViewerObject)


NS_IMETHODIMP
nsDOMViewerObject::SetTarget(nsIRDFResource *aProperty,
                             nsIRDFNode *aValue)
{
  nsISupportsKey propKey(aProperty);
  targets.Put(&propKey, (void *)aValue);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMViewerObject::SetTargetLiteral(nsIRDFResource *aProperty,
                                    nsString& str)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIRDFService, rdf, NS_RDF_PROGID "/rdf-service", &rv);

  PRUnichar* uniStr = str.ToNewUnicode();
  nsCOMPtr<nsIRDFLiteral> literal;
  rv = rdf->GetLiteral(uniStr, getter_AddRefs(literal));
  nsAllocator::Free(uniStr);

  SetTarget(aProperty, literal);
  return NS_OK;
}


nsresult
nsDOMViewerObject::GetTarget(nsIRDFResource *aProperty,
                             nsIRDFNode **aValue)
{
  nsISupportsKey propKey(aProperty);
  *aValue = (nsIRDFResource*)targets.Get(&propKey);

  return NS_OK;

}

nsresult
NS_NewDOMViewerObject(const nsIID& iid, void ** aResult) {
  nsresult rv;
  nsDOMViewerObject* obj = new nsDOMViewerObject;
  
  NS_ADDREF(obj);
  rv = obj->QueryInterface(iid, aResult);
  NS_RELEASE(obj);
  return rv;
}

