
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

#include "nsBaseAppCore.h"
#include "nsIScriptContextOwner.h"
#include "nsAppCoresManager.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIScriptContext.h"
#include "nsIDOMWindow.h"
#include "nsIDocument.h"

// Globals
static NS_DEFINE_IID(kISupportsIID,              NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID,     NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMBaseAppCoreIID,        nsIDOMBaseAppCore::GetIID());
static NS_DEFINE_IID(kIDocumentIID,              nsIDocument::GetIID());


/////////////////////////////////////////////////////////////////////////
// nsBaseAppCore
/////////////////////////////////////////////////////////////////////////

nsBaseAppCore::nsBaseAppCore()
{
  mScriptObject   = nsnull;
  IncInstanceCount();
  NS_INIT_REFCNT();
}

nsBaseAppCore::~nsBaseAppCore()
{
  DecInstanceCount();  
}


NS_IMPL_ADDREF(nsBaseAppCore)
NS_IMPL_RELEASE(nsBaseAppCore)


NS_IMETHODIMP 
nsBaseAppCore::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
      return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIScriptObjectOwnerIID)) {
      *aInstancePtr = (void*) ((nsIScriptObjectOwner*)this);
      AddRef();
      return NS_OK;
  }
  if ( aIID.Equals(kIDOMBaseAppCoreIID)) {
      *aInstancePtr = (void*) ((nsIDOMBaseAppCore*)this);
      AddRef();
      return NS_OK;
  }
  else if ( aIID.Equals(kISupportsIID) ) {
      *aInstancePtr = (void*)(nsISupports*)(nsIScriptObjectOwner*)this;
      AddRef();
      return NS_OK;
  }

   return NS_NOINTERFACE;
}


NS_IMETHODIMP 
nsBaseAppCore::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}


NS_IMETHODIMP    
nsBaseAppCore::Init(const nsString& aId)
{
   
	mId = aId;

	return NS_OK;
}

NS_IMETHODIMP    
nsBaseAppCore::GetId(nsString& aId)
{
    aId = mId;
    return NS_OK;
}

NS_IMETHODIMP    
nsBaseAppCore::SetDocumentCharset(const nsString& aCharset)
{
  return NS_OK;
}

//----------------------------------------
nsIScriptContext *    
nsBaseAppCore::GetScriptContext(nsIDOMWindow * aWin)
{
  nsIScriptContext * scriptContext = nsnull;
  if (nsnull != aWin) {
    nsIDOMDocument * domDoc;
    aWin->GetDocument(&domDoc);
    if (nsnull != domDoc) {
      nsIDocument * doc;
      if (NS_OK == domDoc->QueryInterface(kIDocumentIID,(void**)&doc)) {
        nsIScriptContextOwner * owner = doc->GetScriptContextOwner();
        if (nsnull != owner) {
          owner->GetScriptContext(&scriptContext);
          NS_RELEASE(owner);
        }
        NS_RELEASE(doc);
      }
      NS_RELEASE(domDoc);
    }
  }
  return scriptContext;
}

//----------------------------------------
nsCOMPtr<nsIDOMNode> nsBaseAppCore::FindNamedDOMNode(const nsString &aName, nsIDOMNode * aParent, PRInt32 & aCount, PRInt32 aEndCount)
{
  nsCOMPtr<nsIDOMNode> node;

  if (nsnull == aParent) {
    return node;
	}

  aParent->GetFirstChild(getter_AddRefs(node));
  while (node) {
    nsString name;
    node->GetNodeName(name);
    //printf("FindNamedDOMNode[%s] %d == %d\n", name.ToNewCString(), aCount, aEndCount);
    if (name.Equals(aName)) {
      aCount++;
      if (aCount == aEndCount)
        return node;
    }
    PRBool hasChildren;
    node->HasChildNodes(&hasChildren);
    if (hasChildren) {
      nsCOMPtr<nsIDOMNode> found(FindNamedDOMNode(aName, node, aCount, aEndCount));
      if (found)
        return found;
    }
    nsCOMPtr<nsIDOMNode> oldNode = node;
    oldNode->GetNextSibling(getter_AddRefs(node));
  }
  node = do_QueryInterface(nsnull);
  return node;

} // nsToolbarCore::FindNamedDOMNode

//----------------------------------------
nsCOMPtr<nsIDOMNode> nsBaseAppCore::GetParentNodeFromDOMDoc(nsIDOMDocument * aDOMDoc)
{
  nsCOMPtr<nsIDOMNode> node; // null

  if (nsnull == aDOMDoc) {
    return node;
  }

  nsCOMPtr<nsIDOMElement> element;
  aDOMDoc->GetDocumentElement(getter_AddRefs(element));
  if (element)
    return nsCOMPtr<nsIDOMNode>(do_QueryInterface(element));
  return node;
} // nsToolbarCore::GetParentNodeFromDOMDoc
