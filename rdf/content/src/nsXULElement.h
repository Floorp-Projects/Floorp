/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

  The base XUL element class and associates.

*/

#ifndef nsXULElement_h__
#define nsXULElement_h__

// XXX because nsIEventListenerManager has broken includes
#include "nslayout.h" 
#include "nsIDOMEvent.h"

#include "nsForwardReference.h"
#include "nsHTMLValue.h"
#include "nsIAtom.h"
#include "nsIControllers.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULTreeElement.h"
#include "nsIEventListenerManager.h"
#include "nsIFocusableContent.h"
#include "nsIJSScriptObject.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFResource.h"
#include "nsIScriptObjectOwner.h"
#include "nsIStyleRule.h"
#include "nsIStyledContent.h"
#include "nsIBindableContent.h"
#include "nsIXBLBinding.h"
#include "nsIURI.h"
#include "nsIXMLContent.h"
#include "nsIXULContent.h"
#include "nsXULAttributes.h"
#include "nsIChromeEventHandler.h"

class nsIDocument;
class nsIRDFService;
class nsISupportsArray;
class nsIXULContentUtils;
class nsIXULPrototypeDocument;
class nsRDFDOMNodeList;
class nsString;
class nsVoidArray;
class nsXULAttributes;

////////////////////////////////////////////////////////////////////////

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
#define XUL_PROTOTYPE_ATTRIBUTE_METER(counter) (nsXULPrototypeAttribute::counter++)
#else
#define XUL_PROTOTYPE_ATTRIBUTE_METER(counter) ((void) 0)
#endif


/**

  A prototype attribute for an nsXULPrototypeElement.

 */

class nsXULPrototypeAttribute
{
public:
    nsXULPrototypeAttribute()
        : mNameSpaceID(kNameSpaceID_Unknown),
          mEventHandler(nsnull)
    {
        XUL_PROTOTYPE_ATTRIBUTE_METER(gNumAttributes);
    }

    ~nsXULPrototypeAttribute();

    PRInt32           mNameSpaceID;
    nsCOMPtr<nsIAtom> mName;
    nsString          mValue;
    void*             mEventHandler;

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
    /**
      If enough attributes, on average, are event handlers, it pays to keep
      mEventHandler here, instead of maintaining a separate mapping in each
      nsXULElement associating those mName values with their mEventHandlers.
      Assume we don't need to keep mNameSpaceID along with mName in such an
      event-handler-only name-to-function-pointer mapping.

      Let
        minAttrSize  = sizeof(mNameSpaceID) + sizeof(mName) + sizeof(mValue)
        mappingSize  = sizeof(mName) + sizeof(mEventHandler)
        elemOverhead = nElems * sizeof(MappingPtr)

      Then
        nAttrs * minAttrSize + nEventHandlers * mappingSize + elemOverhead
        > nAttrs * (minAttrSize + mappingSize - sizeof(mName))
      which simplifies to
        nEventHandlers * mappingSize + elemOverhead
        > nAttrs * (mappingSize - sizeof(mName))
      or
        nEventHandlers + (nElems * sizeof(MappingPtr)) / mappingSize
        > nAttrs * (1 - sizeof(mName) / mappingSize)

      If nsCOMPtr and all other pointers are the same size, this reduces to
        nEventHandlers + nElems / 2 > nAttrs / 2

      To measure how many attributes are event handlers, compile XUL source
      with XUL_PROTOTYPE_ATTRIBUTE_METERING and watch the counters below.
      Plug into the above relation -- if true, it pays to put mEventHandler
      in nsXULPrototypeAttribute rather than to keep a separate mapping.

      Recent numbers after opening four browser windows:
        nElems 3537, nAttrs 2528, nEventHandlers 1042
      giving 1042 + 3537/2 > 2528/2 or 2810 > 1264.

      As it happens, mEventHandler also makes this struct power-of-2 sized,
      8 words on most architectures, which makes for strength-reduced array
      index-to-pointer calculations.
     */
    static PRUint32   gNumElements;
    static PRUint32   gNumAttributes;
    static PRUint32   gNumEventHandlers;
    static PRUint32   gNumCacheTests;
    static PRUint32   gNumCacheHits;
    static PRUint32   gNumCacheSets;
    static PRUint32   gNumCacheFills;
#endif /* !XUL_PROTOTYPE_ATTRIBUTE_METERING */
};


/**

  A prototype content model element that holds the "primordial" values
  that have been parsed from the original XUL document. A
  'lightweight' nsXULElement may delegate its representation to this
  structure, which is shared.

 */

class nsXULPrototypeElement;

class nsXULPrototypeNode
{
public:
    enum Type { eType_Element, eType_Script, eType_Text };

    Type                     mType;
    PRInt32                  mLineNo;

    virtual ~nsXULPrototypeNode() {}

protected:
    nsXULPrototypeNode(Type aType, PRInt32 aLineNo)
        : mType(aType), mLineNo(aLineNo) {}
};

class nsXULPrototypeElement : public nsXULPrototypeNode
{
public:
    nsXULPrototypeElement(PRInt32 aLineNo)
        : nsXULPrototypeNode(eType_Element, aLineNo),
          mDocument(nsnull),
          mNumChildren(0),
          mChildren(nsnull),
          mNumAttributes(0),
          mAttributes(nsnull),
          mClassList(nsnull)
    {
        MOZ_COUNT_CTOR(nsXULPrototypeElement);
    }

    virtual ~nsXULPrototypeElement()
    {
        MOZ_COUNT_DTOR(nsXULPrototypeElement);

        delete[] mAttributes;
        delete mClassList;

        for (PRInt32 i = mNumChildren - 1; i >= 0; --i)
            delete mChildren[i];

        delete[] mChildren;
    }


    nsIXULPrototypeDocument* mDocument;           // [WEAK] because doc is refcounted
    PRInt32                  mNumChildren;
    nsXULPrototypeNode**     mChildren;           // [OWNER]

    nsCOMPtr<nsINameSpace>   mNameSpace;          // [OWNER]
    nsCOMPtr<nsIAtom>        mNameSpacePrefix;    // [OWNER]
    PRInt32                  mNameSpaceID;
    nsCOMPtr<nsIAtom>        mTag;                // [OWNER]

    PRInt32                  mNumAttributes;
    nsXULPrototypeAttribute* mAttributes;         // [OWNER]

    nsCOMPtr<nsIStyleRule>   mInlineStyleRule;    // [OWNER]
    nsClassList*             mClassList;

    nsresult GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, nsString& aValue);
};

struct JSRuntime;
struct JSObject;
class nsXULDocument;

class nsXULPrototypeScript : public nsXULPrototypeNode
{
public:
    nsXULPrototypeScript(PRInt32 aLineNo, const char *aVersion);
    virtual ~nsXULPrototypeScript();

    nsresult Compile(const PRUnichar* aText, PRInt32 aTextLength,
                     nsIURI* aURI, PRInt32 aLineNo,
                     nsIDocument* aDocument,
                     nsIXULPrototypeDocument* aPrototypeDocument);

    nsCOMPtr<nsIURI>         mSrcURI;
    PRBool                   mSrcLoading;
    nsXULDocument*           mSrcLoadWaiters;   // [OWNER] but not COMPtr
    JSObject*                mScriptObject;
    const char*              mLangVersion;
};

class nsXULPrototypeText : public nsXULPrototypeNode
{
public:
    nsXULPrototypeText(PRInt32 aLineNo)
        : nsXULPrototypeNode(eType_Text, aLineNo)
    {
        MOZ_COUNT_CTOR(nsXULPrototypeText);
    }

    virtual ~nsXULPrototypeText()
    {
        MOZ_COUNT_DTOR(nsXULPrototypeText);
    }

    nsString                 mValue;
};


////////////////////////////////////////////////////////////////////////

/**

  This class serves as a base for aggregates that will implement a
  per-element XUL API.

 */

class nsXULAggregateElement : public nsISupports
{
protected:
    nsIDOMXULElement* mOuter;
    nsXULAggregateElement(nsIDOMXULElement* aOuter) : mOuter(aOuter) {}
    
public:
    virtual ~nsXULAggregateElement() {};

    // nsISupports interface. Subclasses should use the
    // NS_DECL/IMPL_ISUPPORTS_INHERITED macros to implement the
    // nsISupports interface.
    NS_IMETHOD_(nsrefcnt) AddRef() {
        return mOuter->AddRef();
    }

    NS_IMETHOD_(nsrefcnt) Release() {
        return mOuter->Release();
    }

    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aResult) {
        return mOuter->QueryInterface(aIID, aResult);
    }
};


////////////////////////////////////////////////////////////////////////

/**

  The XUL element.

 */

class nsXULElement : public nsIStyledContent,
                     public nsIXMLContent,
                     public nsIXULContent,
                     public nsIFocusableContent,
                     public nsIBindableContent,
                     public nsIDOMXULElement,
                     public nsIDOMEventReceiver,
                     public nsIScriptEventHandlerOwner,
                     public nsIJSScriptObject,
                     public nsIStyleRule,
                     public nsIChromeEventHandler
{
public:
protected:
    // pseudo-constants
    static nsrefcnt             gRefCnt;
    static nsIRDFService*       gRDFService;
    static nsINameSpaceManager* gNameSpaceManager;
    static nsIXULContentUtils*  gXULUtils;
    static PRInt32              kNameSpaceID_RDF;
    static PRInt32              kNameSpaceID_XUL;

    static nsIAtom*             kClassAtom;
    static nsIAtom*             kContextAtom;
    static nsIAtom*             kHeightAtom;
    static nsIAtom*             kHiddenAtom;
    static nsIAtom*             kIdAtom;
    static nsIAtom*             kObservesAtom;
    static nsIAtom*             kPopupAtom;
    static nsIAtom*             kMenuPopupAtom;
    static nsIAtom*             kRefAtom;
    static nsIAtom*             kSelectedAtom;
    static nsIAtom*             kStyleAtom;
    static nsIAtom*             kTitledButtonAtom;
    static nsIAtom*             kCheckboxAtom;
    static nsIAtom*             kRadioAtom;
    static nsIAtom*             kRadioGroupAtom;
    static nsIAtom*             kTooltipAtom;
    static nsIAtom*             kTreeAtom;
    static nsIAtom*             kTreeCellAtom;
    static nsIAtom*             kTreeChildrenAtom;
    static nsIAtom*             kTreeColAtom;
    static nsIAtom*             kTreeItemAtom;
    static nsIAtom*             kTreeRowAtom;
    static nsIAtom*             kValueAtom;
    static nsIAtom*             kIFrameAtom;
    static nsIAtom*             kBrowserAtom;
    static nsIAtom*             kEditorAtom;
    static nsIAtom*             kWidthAtom;
    static nsIAtom*             kWindowAtom;
    static nsIAtom*             kMenuButtonAtom;
    static nsIAtom*             kMenuListAtom;
    static nsIAtom*             kTextFieldAtom;

public:
    static nsresult
    Create(nsXULPrototypeElement* aPrototype, nsIDocument* aDocument, PRBool aIsScriptable, nsIContent** aResult);

    static nsresult
    Create(PRInt32 aNameSpaceID, nsIAtom* aTag, nsIContent** aResult);

    // nsISupports
    NS_DECL_ISUPPORTS
       
    // nsIContent (from nsIStyledContent)
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
    NS_IMETHOD GetNameSpaceID(PRInt32& aNameSpeceID) const;
    NS_IMETHOD GetTag(nsIAtom*& aResult) const;
    NS_IMETHOD ParseAttributeString(const nsString& aStr, nsIAtom*& aName, PRInt32& aNameSpaceID);
    NS_IMETHOD GetNameSpacePrefixFromId(PRInt32 aNameSpaceID, nsIAtom*& aPrefix);
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
    NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
    NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus* aEventStatus);

    NS_IMETHOD GetContentID(PRUint32* aID);
    NS_IMETHOD SetContentID(PRUint32 aID);

    NS_IMETHOD RangeAdd(nsIDOMRange& aRange);
    NS_IMETHOD RangeRemove(nsIDOMRange& aRange); 
    NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const;

    // nsIStyledContent
    NS_IMETHOD GetID(nsIAtom*& aResult) const;
    NS_IMETHOD GetClasses(nsVoidArray& aArray) const;
    NS_IMETHOD HasClass(nsIAtom* aClass) const;

    NS_IMETHOD GetContentStyleRules(nsISupportsArray* aRules);
    NS_IMETHOD GetInlineStyleRules(nsISupportsArray* aRules);
    NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                        PRInt32& aHint) const;

    // nsIXMLContent
    NS_IMETHOD SetContainingNameSpace(nsINameSpace* aNameSpace);
    NS_IMETHOD GetContainingNameSpace(nsINameSpace*& aNameSpace) const;
    NS_IMETHOD SetNameSpacePrefix(nsIAtom* aNameSpace);
    NS_IMETHOD GetNameSpacePrefix(nsIAtom*& aNameSpace) const;
    NS_IMETHOD SetNameSpaceID(PRInt32 aNameSpaceID);

    // nsIXULContent
    NS_IMETHOD PeekChildCount(PRInt32& aCount) const;
    NS_IMETHOD SetAnonymousState(PRBool aState);
    NS_IMETHOD GetAnonymousState(PRBool& aState);
    NS_IMETHOD SetLazyState(PRInt32 aFlags);
    NS_IMETHOD ClearLazyState(PRInt32 aFlags);
    NS_IMETHOD GetLazyState(PRInt32 aFlag, PRBool& aValue);
    NS_IMETHOD AddScriptEventListener(nsIAtom* aName, const nsString& aValue, REFNSIID aIID);
    NS_IMETHOD ForceElementToOwnResource(PRBool aForce);

    // nsIFocusableContent interface
    NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
    NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);

    // nsIBindableContent interface
    NS_IMETHOD SetBinding(nsIXBLBinding* aBinding);
    NS_IMETHOD GetBinding(nsIXBLBinding** aResult);
    NS_IMETHOD GetBaseTag(nsIAtom** aResult);

    // nsIDOMNode (from nsIDOMElement)
    NS_DECL_IDOMNODE
  
    // nsIDOMElement
    NS_DECL_IDOMELEMENT

    // nsIDOMXULElement
    NS_DECL_IDOMXULELEMENT

    // nsIDOMEventTarget interface (from nsIDOMEventReceiver)
    NS_IMETHOD AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                PRBool aUseCapture);
    NS_IMETHOD RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                   PRBool aUseCapture);

    // nsIDOMEventReceiver
    NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
    NS_IMETHOD GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult);
    NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

    // nsIScriptObjectOwner
    NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
    NS_IMETHOD SetScriptObject(void *aScriptObject);

    // nsIScriptEventHandlerOwner
    NS_IMETHOD CompileEventHandler(nsIScriptContext* aContext,
                                   void* aTarget,
                                   nsIAtom *aName,
                                   const nsString& aBody,
                                   void** aHandler);
    NS_IMETHOD GetCompiledEventHandler(nsIAtom *aName, void** aHandler);

    // nsIJSScriptObject
    virtual PRBool AddProperty(JSContext *aContext, JSObject *aObj, 
                            jsval aID, jsval *aVp);
    virtual PRBool DeleteProperty(JSContext *aContext, JSObject *aObj, 
                            jsval aID, jsval *aVp);
    virtual PRBool GetProperty(JSContext *aContext, JSObject *aObj, 
                            jsval aID, jsval *aVp);
    virtual PRBool SetProperty(JSContext *aContext, JSObject *aObj, 
                            jsval aID, jsval *aVp);
    virtual PRBool EnumerateProperty(JSContext *aContext, JSObject *aObj);
    virtual PRBool Resolve(JSContext *aContext, JSObject *aObj, jsval aID);
    virtual PRBool Convert(JSContext *aContext, JSObject *aObj, jsval aID);
    virtual void   Finalize(JSContext *aContext, JSObject *aObj);

    // nsIStyleRule interface. The node implements this to deal with attributes that
    // need to be mapped into style contexts (e.g., width in treecols).
    NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
    NS_IMETHOD GetStrength(PRInt32& aStrength) const;
    NS_IMETHOD MapFontStyleInto(nsIMutableStyleContext* aContext, nsIPresContext* aPresContext);
    NS_IMETHOD MapStyleInto(nsIMutableStyleContext* aContext, nsIPresContext* aPresContext);
    
    // nsIChromeEventHandler
    NS_DECL_NSICHROMEEVENTHANDLER


protected:
    nsXULElement();
    nsresult Init();
    virtual ~nsXULElement(void);


    // Implementation methods
    nsresult EnsureContentsGenerated(void) const;

    nsresult ExecuteOnBroadcastHandler(nsIDOMElement* anElement, const nsString& attrName);

    static nsresult
    ExecuteJSCode(nsIDOMElement* anElement, nsEvent* aEvent);

    // Used with treecol width attributes
    static PRBool ParseNumericValue(const nsString& aString,
                                  PRInt32& aIntValue,
                                  float& aFloatValue,
                                  nsHTMLUnit& aValueUnit);

    // Static helpers
    static nsresult
    GetElementsByTagName(nsIDOMNode* aNode,
                         const nsString& aTagName,
                         nsRDFDOMNodeList* aElements);

    static nsresult
    GetElementsByAttribute(nsIDOMNode* aNode,
                           const nsString& aAttributeName,
                           const nsString& aAttributeValue,
                           nsRDFDOMNodeList* aElements);

    static PRBool IsAncestor(nsIDOMNode* aParentNode, nsIDOMNode* aChildNode);

    // Helper routine that crawls a parent chain looking for a tree element.
    NS_IMETHOD GetParentTree(nsIDOMXULTreeElement** aTreeElement);

    // XXX Both of these methods must die.
    static PRBool IsFocusable(nsIAtom* aTag);
    PRBool IsFocusableContent();

    nsresult AddPopupListener(nsIAtom* aName);

protected:
    // Required fields
    nsXULPrototypeElement*              mPrototype;
    nsIDocument*                        mDocument;           // [WEAK]
    nsIContent*                         mParent;             // [WEAK]
    nsCOMPtr<nsISupportsArray>          mChildren;           // [OWNER]
    nsCOMPtr<nsIEventListenerManager>   mListenerManager;    // [OWNER]
    void*                               mScriptObject;       // [OWNER]
    PRInt32                             mContentId;

    // The state of our sloth for lazy content model construction via
    // RDF; see nsIXULContent and nsRDFGenericBuilder.
    PRInt32                             mLazyState;

    // Whether or not we're anonymous
    PRBool                              mIsAnonymous;

    // Lazily instantiated if/when object is mutated. Instantiating
    // the mSlots makes an nsXULElement 'heavyweight'.
    struct Slots {
        Slots(nsXULElement* mElement);
        ~Slots();

        nsXULElement*                       mElement;            // [WEAK]
        PRInt32                             mNameSpaceID;
        nsCOMPtr<nsINameSpace>              mNameSpace;          // [OWNER]
        nsCOMPtr<nsIAtom>                   mNameSpacePrefix;    // [OWNER]
        nsCOMPtr<nsIAtom>                   mTag;                // [OWNER]
        nsVoidArray*                        mBroadcastListeners; // [WEAK]
        nsIDOMXULElement*                   mBroadcaster;        // [WEAK]
        nsCOMPtr<nsIControllers>            mControllers;        // [OWNER]
        nsCOMPtr<nsIRDFCompositeDataSource> mDatabase;           // [OWNER]
        nsCOMPtr<nsIRDFResource>            mOwnedResource;      // [OWNER]
        nsXULAttributes*                    mAttributes;
        nsCOMPtr<nsIXBLBinding>             mBinding;            // [OWNER]

        // An unreferenced bare pointer to an aggregate that can
        // implement element-specific APIs.
        nsXULAggregateElement*              mInnerXULElement;
    };

    friend struct Slots;
    Slots* mSlots;
    nsresult EnsureSlots();


protected:
    // Internal accessors. These shadow the 'Slots', and return
    // appropriate default values if there are no slots defined in the
    // delegate.
    PRInt32                    NameSpaceID() const     { return mSlots ? mSlots->mNameSpaceID           : mPrototype->mNameSpaceID; }
    nsINameSpace*              NameSpace() const       { return mSlots ? mSlots->mNameSpace.get()       : mPrototype->mNameSpace.get(); }
    nsIAtom*                   NameSpacePrefix() const { return mSlots ? mSlots->mNameSpacePrefix.get() : mPrototype->mNameSpacePrefix.get(); }
    nsIAtom*                   Tag() const             { return mSlots ? mSlots->mTag.get()             : mPrototype->mTag.get(); }
    nsVoidArray*               BroadcastListeners() const { return mSlots ? mSlots->mBroadcastListeners       : nsnull; }
    nsIDOMXULElement*          Broadcaster() const        { return mSlots ? mSlots->mBroadcaster              : nsnull; }
    nsIControllers*            Controllers() const        { return mSlots ? mSlots->mControllers.get()        : nsnull; }
    nsIRDFCompositeDataSource* Database() const           { return mSlots ? mSlots->mDatabase.get()           : nsnull; }
    nsIRDFResource*            OwnedResource() const      { return mSlots ? mSlots->mOwnedResource.get()      : nsnull; }
    nsXULAttributes*           Attributes() const         { return mSlots ? mSlots->mAttributes               : nsnull; }
    nsXULAggregateElement*     InnerXULElement() const    { return mSlots ? mSlots->mInnerXULElement          : nsnull; }
    nsIXBLBinding*             Binding() const         { return mSlots ? mSlots->mBinding.get()                     : nsnull; }
};


#endif // nsXULElement_h__
