/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  targets.Put(&propKey, aValue);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMViewerObject::SetTargetLiteral(nsIRDFResource *aProperty,
                                    nsString& str)
{
  nsresult rv;
  nsCOMPtr<nsIRDFService> rdf = 
           do_GetService(NS_RDF_CONTRACTID "/rdf-service;1", &rv);

  nsCOMPtr<nsIRDFLiteral> literal;
  rv = rdf->GetLiteral(str.get(), getter_AddRefs(literal));

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

