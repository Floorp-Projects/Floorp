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

  A helper class used to implement attributes.

*/


/*
 * Notes
 *
 * A lot of these methods delegate back to the original content node
 * that created them. This is so that we can lazily produce attribute
 * values from the RDF graph as they're asked for.
 *
 */

#include "nsCOMPtr.h"
#include "nsDOMCID.h"
#include "nsIContent.h"
#include "nsICSSParser.h"
#include "nsIDOMElement.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsXULAttributes.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);


////////////////////////////////////////////////////////////////////////
// nsXULAttribute

nsXULAttribute::nsXULAttribute(nsIContent* aContent,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aName,
                               const nsString& aValue)
    : mNameSpaceID(aNameSpaceID),
      mName(aName),
      mValue(aValue),
      mContent(aContent),
      mScriptObject(nsnull)
{
    NS_INIT_REFCNT();
    NS_IF_ADDREF(aName);
}

nsXULAttribute::~nsXULAttribute()
{
    NS_IF_RELEASE(mName);
}

nsresult
NS_NewXULAttribute(nsXULAttribute** aResult,
                   nsIContent* aContent,
                   PRInt32 aNameSpaceID,
                   nsIAtom* aName,
                   const nsString& aValue)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (! (*aResult = new nsXULAttribute(aContent, aNameSpaceID, aName, aValue)))
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult);
    return NS_OK;
}

// nsISupports interface
NS_IMPL_ADDREF(nsXULAttribute);
NS_IMPL_RELEASE(nsXULAttribute);

NS_IMETHODIMP
nsXULAttribute::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(nsIDOMAttr::GetIID()) ||
        aIID.Equals(nsIDOMNode::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsIDOMAttr*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (aIID.Equals(nsIScriptObjectOwner::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIScriptObjectOwner*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }
}

// nsIDOMNode interface

NS_IMETHODIMP
nsXULAttribute::GetNodeName(nsString& aNodeName)
{
    aNodeName.SetString(mName->GetUnicode());
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetNodeValue(nsString& aNodeValue)
{
    aNodeValue.SetString(mValue);
    return NS_OK;
}
NS_IMETHODIMP
nsXULAttribute::SetNodeValue(const nsString& aNodeValue)
{
    return SetValue(aNodeValue);
}

NS_IMETHODIMP
nsXULAttribute::GetNodeType(PRUint16* aNodeType)
{
    *aNodeType = (PRUint16)nsIDOMNode::ATTRIBUTE_NODE;
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetParentNode(nsIDOMNode** aParentNode)
{
    *aParentNode = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttribute::GetFirstChild(nsIDOMNode** aFirstChild)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttribute::GetLastChild(nsIDOMNode** aLastChild)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttribute::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    *aPreviousSibling = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetNextSibling(nsIDOMNode** aNextSibling)
{
    *aNextSibling = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
    *aAttributes = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttribute::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULAttribute::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULAttribute::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULAttribute::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULAttribute::HasChildNodes(PRBool* aReturn)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttribute::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}



// nsIDOMAttr interface

NS_IMETHODIMP
nsXULAttribute::GetName(nsString& aName)
{
    aName.SetString(mName->GetUnicode());
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetSpecified(PRBool* aSpecified)
{
    // XXX this'll break when we make Clone() work
    *aSpecified = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetValue(nsString& aValue)
{
    aValue.SetString(mValue);
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::SetValue(const nsString& aValue)
{
    nsCOMPtr<nsIDOMElement> element( do_QueryInterface(mContent) );
    if (element) {
        nsAutoString qualifiedName;
        GetQualifiedName(qualifiedName);
        return element->SetAttribute(qualifiedName, aValue);
    }
    else {
        return NS_ERROR_FAILURE;
    }
}


// nsIScriptObjectOwner interface

NS_IMETHODIMP
nsXULAttribute::GetScriptObject(nsIScriptContext* aContext, void** aScriptObject)
{
    nsresult rv = NS_OK;
    if (! mScriptObject) {
        nsIDOMScriptObjectFactory *factory;
    
        rv = nsServiceManager::GetService(kDOMScriptObjectFactoryCID,
                                          nsIDOMScriptObjectFactory::GetIID(),
                                          (nsISupports **)&factory);

        if (NS_FAILED(rv))
            return rv;

        rv = factory->NewScriptAttr(aContext, 
                                    (nsISupports*)(nsIDOMAttr*) this,
                                    (nsISupports*) mContent,
                                    (void**) &mScriptObject);

        nsServiceManager::ReleaseService(kDOMScriptObjectFactoryCID, factory);
    }

    *aScriptObject = mScriptObject;
    return rv;
}


NS_IMETHODIMP
nsXULAttribute::SetScriptObject(void *aScriptObject)
{
    mScriptObject = aScriptObject;
    return NS_OK;
}


// Implementation methods

void
nsXULAttribute::GetQualifiedName(nsString& aQualifiedName)
{
    aQualifiedName.Truncate();
    if ((mNameSpaceID != kNameSpaceID_None) &&
        (mNameSpaceID != kNameSpaceID_Unknown)) {
        nsIAtom* prefix;
        if (NS_SUCCEEDED(mContent->GetNameSpacePrefixFromId(mNameSpaceID, prefix))) {
            aQualifiedName.Append(prefix->GetUnicode());
            aQualifiedName.Append(':');
            NS_RELEASE(prefix);
        }
    }
    aQualifiedName.Append(mName->GetUnicode());
}


////////////////////////////////////////////////////////////////////////
// nsXULAttributes

nsXULAttributes::nsXULAttributes(nsIContent* aContent)
    : mContent(aContent),
      mClassList(nsnull),
      mStyleRule(nsnull),
      mScriptObject(nsnull)
{
    NS_INIT_REFCNT();
}


nsXULAttributes::~nsXULAttributes()
{
    PRInt32 count = mAttributes.Count();
    PRInt32 index;
    for (index = 0; index < count; index++) {
        nsXULAttribute* attr = (nsXULAttribute*)mAttributes.ElementAt(index);
        NS_RELEASE(attr);
    }
}


nsresult
NS_NewXULAttributes(nsXULAttributes** aResult, nsIContent* aContent)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (! (*aResult = new nsXULAttributes(aContent)))
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult);
    return NS_OK;
}


// nsISupports interface

NS_IMPL_ADDREF(nsXULAttributes);
NS_IMPL_RELEASE(nsXULAttributes);

NS_IMETHODIMP
nsXULAttributes::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(nsIDOMNamedNodeMap::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsIDOMNamedNodeMap*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (aIID.Equals(nsIScriptObjectOwner::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIScriptObjectOwner*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }
}


// nsIDOMNamedNodeMap interface

NS_IMETHODIMP
nsXULAttributes::GetLength(PRUint32* aLength)
{
    NS_PRECONDITION(aLength != nsnull, "null ptr");
    if (! aLength)
        return NS_ERROR_NULL_POINTER;

    *aLength = mAttributes.Count();
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttributes::GetNamedItem(const nsString& aName, nsIDOMNode** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    *aReturn = nsnull;

    PRInt32 nameSpaceID;
    nsIAtom* name;

    if (NS_FAILED(rv = mContent->ParseAttributeString(aName, name, nameSpaceID)))
        return rv;

    // XXX doing this instead of calling mContent->GetAttribute() will
    // make it a lot harder to lazily instantiate properties from the
    // graph. The problem is, how else do we get the named item?
    for (PRInt32 i = mAttributes.Count() - 1; i >= 0; --i) {
        nsXULAttribute* attr = (nsXULAttribute*) mAttributes[i];
        if (((nameSpaceID == attr->mNameSpaceID) ||
             (nameSpaceID == kNameSpaceID_Unknown) ||
             (nameSpaceID == kNameSpaceID_None)) &&
            (name == attr->mName)) {
            NS_ADDREF(attr);
            *aReturn = attr;
            break;
        }
    }

    NS_RELEASE(name);
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttributes::SetNamedItem(nsIDOMNode* aArg, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttributes::RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn)
{
    nsCOMPtr<nsIDOMElement> element( do_QueryInterface(mContent) );
    if (element) {
        return element->RemoveAttribute(aName);
        *aReturn = nsnull; // XXX should be the element we just removed
        return NS_OK;
    }
    else {
        return NS_ERROR_FAILURE;
    }
}

NS_IMETHODIMP
nsXULAttributes::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
    *aReturn = (nsXULAttribute*) mAttributes[aIndex];
    NS_IF_ADDREF(*aReturn);
    return NS_OK;
}




// nsIScriptObjectOwner interface

NS_IMETHODIMP
nsXULAttributes::GetScriptObject(nsIScriptContext* aContext, void** aScriptObject)
{
    nsresult rv = NS_OK;
    if (! mScriptObject) {
        nsIDOMScriptObjectFactory *factory;
    
        rv = nsServiceManager::GetService(kDOMScriptObjectFactoryCID,
                                          nsIDOMScriptObjectFactory::GetIID(),
                                          (nsISupports **)&factory);

        if (NS_FAILED(rv))
            return rv;

        rv = factory->NewScriptNamedNodeMap(aContext, 
                                            (nsISupports*)(nsIDOMNamedNodeMap*) this, 
                                            (nsISupports*) mContent,
                                            (void**) &mScriptObject);

        nsServiceManager::ReleaseService(kDOMScriptObjectFactoryCID, factory);
    }

    *aScriptObject = mScriptObject;
    return rv;
}

NS_IMETHODIMP
nsXULAttributes::SetScriptObject(void *aScriptObject)
{
    mScriptObject = aScriptObject;
    return NS_OK;
}

// Implementation methods

nsresult 
nsXULAttributes::GetClasses(nsVoidArray& aArray) const
{
  aArray.Clear();
  const nsClassList* classList = mClassList;
  while (nsnull != classList) {
    aArray.AppendElement(classList->mAtom); // NOTE atom is not addrefed
    classList = classList->mNext;
  }
  return NS_OK;
}

nsresult 
nsXULAttributes::HasClass(nsIAtom* aClass) const
{
  const nsClassList* classList = mClassList;
  while (nsnull != classList) {
    if (classList->mAtom == aClass) {
      return NS_OK;
    }
    classList = classList->mNext;
  }
  return NS_COMFALSE;
}

nsresult nsXULAttributes::UpdateClassList(const nsString& aValue)
{
  if (mClassList != nsnull)
  {
    delete mClassList;
    mClassList = nsnull;
  }

  if (aValue != "")
    ParseClasses(aValue, &mClassList);
  
  return NS_OK;
}

nsresult nsXULAttributes::UpdateStyleRule(nsIURL* aDocURL, const nsString& aValue)
{
    if (aValue == "")
    {
      // XXX: Removing the rule. Is this sufficient?
      NS_IF_RELEASE(mStyleRule);
      mStyleRule = nsnull;
      return NS_OK;
    }

    nsICSSParser* css;
    nsresult result = NS_NewCSSParser(&css);
    if (NS_OK != result) {
      return result;
    }

    nsIStyleRule* rule;
    result = css->ParseDeclarations(aValue, aDocURL, rule);
    //NS_IF_RELEASE(docURL);
    
    if ((NS_OK == result) && (nsnull != rule)) {
      mStyleRule = rule; //Addrefed already during parse, so don't need to addref again.
      //result = SetHTMLAttribute(aAttribute, nsHTMLValue(rule), aNotify);
    }
    //else {
    //  result = SetHTMLAttribute(aAttribute, nsHTMLValue(aValue), aNotify);
    //}
    NS_RELEASE(css);
    return NS_OK;
}

nsresult nsXULAttributes::GetInlineStyleRule(nsIStyleRule*& aRule)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (mStyleRule != nsnull)
  {
    aRule = mStyleRule;
    NS_ADDREF(aRule);
    result = NS_OK;
  }

  return result;
}


void
nsXULAttributes::ParseClasses(const nsString& aClassString, nsClassList** aClassList)
{
static const PRUnichar kNullCh = PRUnichar('\0');

  NS_ASSERTION(nsnull == *aClassList, "non null start list");

  nsAutoString  classStr(aClassString);  // copy to work buffer
  classStr.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)classStr;
  PRUnichar* end   = start;

  while (kNullCh != *start) {
    while ((kNullCh != *start) && nsString::IsSpace(*start)) {  // skip leading space
      start++;
    }
    end = start;

    while ((kNullCh != *end) && (PR_FALSE == nsString::IsSpace(*end))) { // look for space or end
      end++;
    }
    *end = kNullCh; // end string here

    if (start < end) {
      *aClassList = new nsClassList(NS_NewAtom(start));
      aClassList = &((*aClassList)->mNext);
    }

    start = ++end;
  }
}

