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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsIAccessible.h"
#include "nsIAccessibleDocument.h"
#include "Accessible.h"
#include "SimpleDOMNode.h"
#include "ISimpleDOMNode_iid.h"
#include "ISimpleDOMDocument_iid.h"
#include "nsIWidget.h"
#include "nsWindow.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIAccessibleEventReceiver.h"
#include "nsReadableUtils.h"
#include "nsITextContent.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsILink.h"
#include "nsIAccessibilityService.h"
#include "nsIServiceManager.h"
#include "nsIWeakReference.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPresContext.h"
#include "nsPromiseFlatString.h"
#include "nsINameSpaceManager.h"

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#define DEBUG_LEAKS 1

#ifdef DEBUG_LEAKS
static gSimpleDOMNodes = 0;
#endif

/*
 * Class Accessible
 */


//-----------------------------------------------------
// construction 
//-----------------------------------------------------

SimpleDOMNode::SimpleDOMNode(nsIAccessible* aNSAcc, nsIDOMNode *aNode, HWND aWnd): mWnd(aWnd), m_cRef(0)
{
  MOZ_COUNT_CTOR(SimpleDOMNode); // For catching leaks on tinderbox
  mDOMNode = aNode;
  if (!aNode && aNSAcc)
    aNSAcc->AccGetDOMNode(getter_AddRefs(mDOMNode));
 
#ifdef DEBUG_LEAKS
  printf("SimpleDOMNodes=%d\n", ++gSimpleDOMNodes);
#endif
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
SimpleDOMNode::~SimpleDOMNode()
{
  MOZ_COUNT_DTOR(SimpleDOMNode);  // For catching leaks on tinderbox
  m_cRef = 0;
#ifdef DEBUG_LEAKS
  printf("SimpleDOMNodes=%d\n", --gSimpleDOMNodes);
#endif
}


//-----------------------------------------------------
// IUnknown interface methods - see iunknown.h for documentation
//-----------------------------------------------------
STDMETHODIMP SimpleDOMNode::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = nsnull;

  if (IID_IUnknown == iid || IID_ISimpleDOMNode == iid)
    *ppv = NS_STATIC_CAST(ISimpleDOMNode*, this);

  if (nsnull == *ppv)
    return E_NOINTERFACE;      //iid not supported.
   
  (NS_REINTERPRET_CAST(IUnknown*, *ppv))->AddRef(); 
  return S_OK;
}

//-----------------------------------------------------
STDMETHODIMP_(ULONG) SimpleDOMNode::AddRef()
{
  return ++m_cRef;
}


//-----------------------------------------------------
STDMETHODIMP_(ULONG) SimpleDOMNode::Release()
{
  if (0 != --m_cRef)
    return m_cRef;

  delete this;

  return 0;
}


//-----------------------------------------------------
// ISimpleDOMNode methods
//-----------------------------------------------------

STDMETHODIMP SimpleDOMNode::get_nodeInfo( 
    /* [out] */ BSTR __RPC_FAR *aNodeName,
    /* [out] */ short __RPC_FAR *aNameSpaceID,
    /* [out] */ BSTR __RPC_FAR *aNodeValue,
    /* [out] */ unsigned int __RPC_FAR *aNumChildren,
    /* [out] */ unsigned short __RPC_FAR *aNodeType)
{
  *aNodeName = nsnull;
  nsCOMPtr<nsIDOMElement> domElement;
  nsCOMPtr<nsIContent> content;
  GetElementAndContentFor(domElement, content);
  
  PRUint16 nodeType = 0;
  mDOMNode->GetNodeType(&nodeType);
  *aNodeType=NS_STATIC_CAST(unsigned short, nodeType);

  if (*aNodeType !=  NODETYPE_TEXT) {
    nsAutoString nodeName;
    mDOMNode->GetNodeName(nodeName);
    *aNodeName =   ::SysAllocString(nodeName.get());
  }

  nsAutoString nodeValue;

  mDOMNode->GetNodeValue(nodeValue);
  *aNodeValue = ::SysAllocString(nodeValue.get());

  PRInt32 nameSpaceID = 0;
  if (content)
    content->GetNameSpaceID(nameSpaceID);
  *aNameSpaceID = NS_STATIC_CAST(short, nameSpaceID);

  *aNumChildren = 0;
  PRUint32 numChildren = 0;
  nsCOMPtr<nsIDOMNodeList> nodeList;
  mDOMNode->GetChildNodes(getter_AddRefs(nodeList));
  if (nodeList && NS_OK == nodeList->GetLength(&numChildren))
    *aNumChildren = NS_STATIC_CAST(unsigned int, numChildren);

  return S_OK;
}


       
STDMETHODIMP SimpleDOMNode::get_attributes( 
    /* [in] */ unsigned short aMaxAttribs,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR *aAttribNames,
    /* [length_is][size_is][out] */ short __RPC_FAR *aNameSpaceIDs,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR *aAttribValues,
    /* [out] */ unsigned short __RPC_FAR *aNumAttribs)
{
  nsCOMPtr<nsIDOMElement> domElement;
  nsCOMPtr<nsIContent> content;
  GetElementAndContentFor(domElement, content);
  *aNumAttribs = 0;

  if (!content || !domElement) 
    return E_FAIL;
  PRInt32 numAttribs;
  content->GetAttrCount(numAttribs);
  if (numAttribs > aMaxAttribs)
    numAttribs = aMaxAttribs;
  *aNumAttribs = NS_STATIC_CAST(unsigned short, numAttribs);

  PRInt32 index, nameSpaceID;
  nsCOMPtr<nsIAtom> nameAtom, prefixAtom;

  for (index = 0; index < numAttribs; index++) {
    aNameSpaceIDs[index] = 0; aAttribValues[index] = aAttribNames[index] = nsnull;
    nsAutoString attributeValue;
    const PRUnichar *pszAttributeName; 

    if (NS_SUCCEEDED(content->GetAttrNameAt(index, nameSpaceID, *getter_AddRefs(nameAtom), *getter_AddRefs(prefixAtom)))) {
      aNameSpaceIDs[index] = NS_STATIC_CAST(short, nameSpaceID);
      nameAtom->GetUnicode(&pszAttributeName);
      aAttribNames[index] = ::SysAllocString(pszAttributeName);
      if (NS_SUCCEEDED(content->GetAttr(nameSpaceID, nameAtom, attributeValue))) 
        aAttribValues[index] = ::SysAllocString(attributeValue.get());
    }
  }

  return S_OK; 
}
        

STDMETHODIMP SimpleDOMNode::get_attributesForNames( 
    /* [in] */ unsigned short aNumAttribs,
    /* [length_is][size_is][in] */ BSTR __RPC_FAR *aAttribNames,
    /* [length_is][size_is][in] */ short __RPC_FAR *aNameSpaceID,
    /* [length_is][size_is][retval] */ BSTR __RPC_FAR *aAttribValues)
{
  nsCOMPtr<nsIDOMElement> domElement;
  nsCOMPtr<nsIContent> content;
  GetElementAndContentFor(domElement, content);

  if (!domElement || !content) 
    return E_FAIL;

  nsCOMPtr<nsIDocument> doc;
  content->GetDocument(*getter_AddRefs(doc));
  
  if (!doc)
    return E_FAIL;

  nsCOMPtr<nsINameSpaceManager> nameSpaceManager;
  doc->GetNameSpaceManager(*getter_AddRefs(nameSpaceManager));

  PRInt32 index;

  for (index = 0; index < aNumAttribs; index++) {
    aAttribValues[index] = nsnull;
    if (aAttribNames[index]) {
      nsAutoString attributeValue, nameSpaceURI;
      nsAutoString attributeName(nsDependentString(NS_STATIC_CAST(PRUnichar*,aAttribNames[index])));
      nsresult rv;

      if (aNameSpaceID[index]>0 && 
        NS_SUCCEEDED(nameSpaceManager->GetNameSpaceURI(aNameSpaceID[index], nameSpaceURI)))
          rv = domElement->GetAttributeNS(nameSpaceURI, attributeName, attributeValue);
      else 
        rv = domElement->GetAttribute(attributeName, attributeValue);

      if (NS_SUCCEEDED(rv))
        aAttribValues[index] = ::SysAllocString(attributeValue.get());
    }
  }

  return S_OK; 
}


NS_IMETHODIMP SimpleDOMNode::GetComputedStyleDeclaration(nsIDOMCSSStyleDeclaration **aCssDecl, PRUint32 *aLength)
{
  nsCOMPtr<nsIDOMElement> domElement;
  nsCOMPtr<nsIContent> content;
  GetElementAndContentFor(domElement, content);
  if (!domElement || !content) 
    return NS_ERROR_FAILURE;   

  nsCOMPtr<nsIDocument> doc;
  if (content) 
    content->GetDocument(*getter_AddRefs(doc));

  nsCOMPtr<nsIPresShell> shell;
  if (doc)
    doc->GetShellAt(0, getter_AddRefs(shell)); 

  if (!shell)
    return NS_ERROR_FAILURE;   

  nsCOMPtr<nsIScriptGlobalObject> global;
  doc->GetScriptGlobalObject(getter_AddRefs(global));
  nsCOMPtr<nsIDOMViewCSS> viewCSS(do_QueryInterface(global));

  if (!viewCSS)
    return NS_ERROR_FAILURE;   

  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
  nsAutoString empty;
  viewCSS->GetComputedStyle(domElement, empty, getter_AddRefs(cssDecl));
  if (cssDecl) {
    *aCssDecl = cssDecl;
    NS_ADDREF(*aCssDecl);
    cssDecl->GetLength(aLength);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}


/* To do: use media type if not null */
STDMETHODIMP SimpleDOMNode::get_computedStyle( 
    /* [in] */ unsigned short aMaxStyleProperties,
    /* [in] */ boolean aUseAlternateView,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR *aStyleProperties,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR *aStyleValues,
    /* [out] */ unsigned short __RPC_FAR *aNumStyleProperties)
{
  *aNumStyleProperties = 0;
  PRUint32 length;
  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
  if (NS_FAILED(GetComputedStyleDeclaration(getter_AddRefs(cssDecl), &length)))
    return E_FAIL;

  PRUint32 index, realIndex;
  for (index = realIndex = 0; index < length && realIndex < aMaxStyleProperties; index ++) {
    nsAutoString property, value;
    if (NS_SUCCEEDED(cssDecl->Item(index, property)) && property.CharAt(0) != '-')  // Ignore -moz-* properties
      cssDecl->GetPropertyValue(property, value);  // Get property value
    if (!value.IsEmpty()) {
      aStyleProperties[realIndex] =   ::SysAllocString(property.get());
      aStyleValues[realIndex]     =   ::SysAllocString(value.get());
      ++realIndex;
    }
  }
  *aNumStyleProperties = NS_STATIC_CAST(unsigned short, realIndex);

  return S_OK;
}


STDMETHODIMP SimpleDOMNode::get_computedStyleForProperties( 
    /* [in] */ unsigned short aNumStyleProperties,
    /* [in] */ boolean aUseAlternateView,
    /* [length_is][size_is][in] */ BSTR __RPC_FAR *aStyleProperties,
    /* [length_is][size_is][out] */ BSTR __RPC_FAR *aStyleValues)
{
  PRUint32 length = 0;
  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
  nsresult rv = GetComputedStyleDeclaration(getter_AddRefs(cssDecl), &length);
  if (NS_FAILED(rv))
    return E_FAIL;

  PRUint32 index;
  for (index = 0; index < aNumStyleProperties; index ++) {
    nsAutoString value;
    if (aStyleProperties[index])
      cssDecl->GetPropertyValue(nsDependentString(NS_STATIC_CAST(PRUnichar*,aStyleProperties[index])), value);  // Get property value
    aStyleValues[index] = ::SysAllocString(value.get());
  }

  return S_OK;
}


ISimpleDOMNode* SimpleDOMNode::MakeSimpleDOMNode(nsIDOMNode *node)
{
  if (!node) 
    return NULL;

  ISimpleDOMNode *newNode = NULL;
  
  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  nsCOMPtr<nsIDocument> doc;

  if (content) 
    content->GetDocument(*getter_AddRefs(doc));
  else {
    // Get the document via QueryInterface, since there is no content node
    doc = do_QueryInterface(node);
  }

  if (!doc)
    return NULL;

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  if (!accService)
    return NULL;

  nsCOMPtr<nsIAccessible> nsAcc;
  accService->GetAccessibleFor(node, getter_AddRefs(nsAcc));
  if (nsAcc) {
    nsCOMPtr<nsIAccessibleDocument> nsAccDoc(do_QueryInterface(nsAcc));
    if (nsAccDoc) 
      newNode = new DocAccessible(nsAcc, node, mWnd);
    else 
      newNode = new Accessible(nsAcc, node, mWnd);
  }
  else if (!content) {  // We're on a the root frame
    IAccessible * pAcc = NULL;
    HRESULT hr = Accessible::AccessibleObjectFromWindow(  mWnd, OBJID_CLIENT, IID_IAccessible, (void **) &pAcc );
    if (pAcc) {
      ISimpleDOMNode *testNode;
      pAcc->QueryInterface(IID_ISimpleDOMNode, (void**)&testNode);
      newNode = testNode;
      pAcc->Release();
    }
  }
  else 
    newNode = new SimpleDOMNode(nsnull, node, mWnd);

  if (newNode)
    newNode->AddRef();

  return newNode;
}


STDMETHODIMP SimpleDOMNode::get_parentNode(ISimpleDOMNode __RPC_FAR *__RPC_FAR *aNode)
{
  nsCOMPtr<nsIDOMNode> node;
  mDOMNode->GetParentNode(getter_AddRefs(node));
  *aNode = MakeSimpleDOMNode(node);

  return S_OK;
}

STDMETHODIMP SimpleDOMNode::get_firstChild(ISimpleDOMNode __RPC_FAR *__RPC_FAR *aNode)
{
  nsCOMPtr<nsIDOMNode> node;
  mDOMNode->GetFirstChild(getter_AddRefs(node));
  *aNode = MakeSimpleDOMNode(node);

  return S_OK;
}

STDMETHODIMP SimpleDOMNode::get_lastChild(ISimpleDOMNode __RPC_FAR *__RPC_FAR *aNode)
{
  nsCOMPtr<nsIDOMNode> node;
  mDOMNode->GetLastChild(getter_AddRefs(node));
  *aNode = MakeSimpleDOMNode(node);

  return S_OK;
}

STDMETHODIMP SimpleDOMNode::get_previousSibling(ISimpleDOMNode __RPC_FAR *__RPC_FAR *aNode)
{
  nsCOMPtr<nsIDOMNode> node;
  mDOMNode->GetPreviousSibling(getter_AddRefs(node));
  *aNode = MakeSimpleDOMNode(node);

  return S_OK;
}

STDMETHODIMP SimpleDOMNode::get_nextSibling(ISimpleDOMNode __RPC_FAR *__RPC_FAR *aNode)
{
  nsCOMPtr<nsIDOMNode> node;
  mDOMNode->GetNextSibling(getter_AddRefs(node));
  *aNode = MakeSimpleDOMNode(node);

  return S_OK;
}


        
//------- Helper methods ---------

void SimpleDOMNode::GetElementAndContentFor(nsCOMPtr<nsIDOMElement>& aElement, nsCOMPtr<nsIContent> &aContent)
{
  aElement = do_QueryInterface(mDOMNode);
  aContent = do_QueryInterface(mDOMNode);
}


nsIDOMNode* SimpleDOMNode::GetRealDOMNode()
{
  return mDOMNode;
}
