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

#ifndef nsRDFElement_h___
#define nsRDFElement_h___

#include "nsString.h"
#include "nsIDOMElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIRDFContent.h"
#include "nsIJSScriptObject.h"

class nsHashtable;
class nsIContent;
class nsIRDFDocument;
class nsIAtom;
class nsIEventListenerManager;
class nsISupportsArray;

// XXX should we make this inheirit from the nsXMLElement
// implementation???

class nsRDFElement : public nsIDOMElement,
		     public nsIDOMEventReceiver,
		     public nsIScriptObjectOwner,
		     public nsIJSScriptObject,
		     public nsIRDFContent
{
public:
    nsRDFElement(void);
    ~nsRDFElement(void);

    // nsISupports
    NS_DECL_ISUPPORTS
       
    // nsIDOMNode (from nsIDOMElement)
    NS_DECL_IDOMNODE
  
    // nsIDOMElement
    NS_DECL_IDOMELEMENT

    // nsIScriptObjectOwner
    NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
    NS_IMETHOD SetScriptObject(void *aScriptObject);

    // nsIContent (from nsIRDFContent via nsIXMLContent)

    // Any of the nsIContent methods that directly manipulate content
    // (e.g., AppendChildTo()), are assumed to "know what they're
    // doing" to the content model. No attempt is made to muck with
    // the underlying RDF representation.
    NS_IMETHOD GetDocument(nsIDocument*& aResult) const;
    NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep);
    NS_IMETHOD GetParent(nsIContent*& aResult) const;
    NS_IMETHOD SetParent(nsIContent* aParent);
    NS_IMETHOD CanContainChildren(PRBool& aResult) const;
    NS_IMETHOD ChildCount(PRInt32& aResult) const;
    NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
    NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const;
    NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
    NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
    NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify);
    NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);
    NS_IMETHOD IsSynthetic(PRBool& aResult);
    NS_IMETHOD GetTag(nsIAtom*& aResult) const;
    NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, const nsString& aValue, PRBool aNotify);
    NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, nsString& aResult) const;
    NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify);
    NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex, PRInt32& aNameSpaceID, 
                                  nsIAtom*& aName) const;
    NS_IMETHOD GetAttributeCount(PRInt32& aResult) const;
    NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
    NS_IMETHOD BeginConvertToXIF(nsXIFConverter& aConverter) const;
    NS_IMETHOD ConvertContentToXIF(nsXIFConverter& aConverter) const;
    NS_IMETHOD FinishConvertToXIF(nsXIFConverter& aConverter) const;
    NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;
    NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus& aEventStatus);
    NS_IMETHOD RangeAdd(nsIDOMRange& aRange);
    NS_IMETHOD RangeRemove(nsIDOMRange& aRange); 
    NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const;

    // nsIXMLContent (from nsIRDFContent)
    NS_IMETHOD SetNameSpacePrefix(nsIAtom* aNameSpace);
    NS_IMETHOD GetNameSpacePrefix(nsIAtom*& aNameSpace) const;
    NS_IMETHOD SetNameSpaceID(PRInt32 aNameSpaceID);
    NS_IMETHOD GetNameSpaceID(PRInt32& aNameSpeceID) const;

    // nsIRDFContent
    NS_IMETHOD Init(nsIRDFDocument* doc,
                    const nsString& tag,
                    nsIRDFResource* resource,
                    PRBool childrenMustBeGenerated);

    NS_IMETHOD SetResource(const nsString& aURI);
    NS_IMETHOD GetResource(nsString& rURI) const;

    NS_IMETHOD SetResource(nsIRDFResource* aResource);
    NS_IMETHOD GetResource(nsIRDFResource*& aResource);

    NS_IMETHOD SetProperty(const nsString& aPropertyURI, const nsString& aValue);
    NS_IMETHOD GetProperty(const nsString& aPropertyURI, nsString& rValue) const;

    // nsIDOMEventReceiver
    NS_IMETHOD AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
    NS_IMETHOD GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult);


    // nsIJSScriptObject
    virtual PRBool AddProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool GetProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool SetProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool EnumerateProperty(JSContext *aContext);
    virtual PRBool Resolve(JSContext *aContext, jsval aID);
    virtual PRBool Convert(JSContext *aContext, jsval aID);
    virtual void   Finalize(JSContext *aContext);

protected:
    /** The document in which the element lives. */
    nsIRDFDocument*   mDocument;

    /** The tag's namespace prefix */
    nsIAtom*          mNameSpacePrefix;

    /** The tag's namespace ID */
    PRInt32           mNameSpaceID;

    /** XXX */
    void*             mScriptObject;

    /** The RDF resource that the element corresponds to */
    nsIRDFResource*   mResource;

    /** An array of child nodes */
    nsISupportsArray* mChildren;

    /** On initialization, set to PR_TRUE if the child nodes must
        be generated by walking the RDF graph. Set to PR_FALSE if
        the constructor will do it "by hand". */
    PRBool            mChildrenMustBeGenerated;

    /** The element's parent. NOT refcounted. */
    nsIContent*       mParent;

    /** The element's tag */
    nsAutoString      mTag;

    /** An array of attribute data. Instantiated
        lazily if attributes are required */
    nsVoidArray*      mAttributes;

    /**
     * Dynamically generate the element's children from the RDF graph
     */
    nsresult GenerateChildren(void) const;
};

#endif // nsRDFElement_h___
