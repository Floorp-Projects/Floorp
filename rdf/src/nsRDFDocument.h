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

#ifndef nsRDFDocument_h___
#define nsRDFDocument_h___

#include "nsIDocument.h"
#include "nsIRDFDocument.h"
#include "nsVoidArray.h"

class nsIArena;
class nsIParser;
class nsISupportsArray;

/**
 * An NGLayout document context for displaying an RDF graph.
 */
class nsRDFDocument : public nsIDocument,
                      public nsIRDFDocument
{
public:
    nsRDFDocument();
    virtual ~nsRDFDocument();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDocument interface
    virtual nsIArena* GetArena();

    NS_IMETHOD StartDocumentLoad(nsIURL *aUrl, 
                                 nsIContentViewerContainer* aContainer,
                                 nsIStreamListener **aDocListener,
                                 const char* aCommand);

    virtual const nsString* GetDocumentTitle() const;

    virtual nsIURL* GetDocumentURL() const;

    virtual nsIURLGroup* GetDocumentURLGroup() const;

    virtual nsCharSetID GetDocumentCharacterSet() const;

    virtual void SetDocumentCharacterSet(nsCharSetID aCharSetID);

    virtual nsresult CreateShell(nsIPresContext* aContext,
                                 nsIViewManager* aViewManager,
                                 nsIStyleSet* aStyleSet,
                                 nsIPresShell** aInstancePtrResult);

    virtual PRBool DeleteShell(nsIPresShell* aShell);

    virtual PRInt32 GetNumberOfShells();

    virtual nsIPresShell* GetShellAt(PRInt32 aIndex);

    virtual nsIDocument* GetParentDocument();

    virtual void SetParentDocument(nsIDocument* aParent);

    virtual void AddSubDocument(nsIDocument* aSubDoc);

    virtual PRInt32 GetNumberOfSubDocuments();

    virtual nsIDocument* GetSubDocumentAt(PRInt32 aIndex);

    virtual nsIContent* GetRootContent();

    virtual void SetRootContent(nsIContent* aRoot);

    virtual PRInt32 GetNumberOfStyleSheets();

    virtual nsIStyleSheet* GetStyleSheetAt(PRInt32 aIndex);

    virtual void AddStyleSheet(nsIStyleSheet* aSheet);

    virtual void SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                            PRBool mDisabled);

    virtual nsIScriptContextOwner *GetScriptContextOwner();

    virtual void SetScriptContextOwner(nsIScriptContextOwner *aScriptContextOwner);

    virtual void AddObserver(nsIDocumentObserver* aObserver);

    virtual PRBool RemoveObserver(nsIDocumentObserver* aObserver);

    NS_IMETHOD BeginLoad();

    NS_IMETHOD EndLoad();

    NS_IMETHOD ContentChanged(nsIContent* aContent,
                              nsISupports* aSubContent);

    NS_IMETHOD AttributeChanged(nsIContent* aChild,
                                nsIAtom* aAttribute,
                                PRInt32 aHint); // See nsStyleConsts fot hint values

    NS_IMETHOD ContentAppended(nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer);

    NS_IMETHOD ContentInserted(nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer);

    NS_IMETHOD ContentReplaced(nsIContent* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aNewChild,
                               PRInt32 aIndexInContainer);

    NS_IMETHOD ContentRemoved(nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer);

    NS_IMETHOD StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule,
                                PRInt32 aHint); // See nsStyleConsts fot hint values

    NS_IMETHOD StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);

    NS_IMETHOD StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule);

    NS_IMETHOD GetSelection(nsICollection** aSelection);

    NS_IMETHOD SelectAll();

    NS_IMETHOD FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound);

    virtual void CreateXIF(nsString & aBuffer, PRBool aUseSelection);

    virtual void ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual void BeginConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual void ConvertChildrenToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual void FinishConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual PRBool IsInRange(const nsIContent *aStartContent, const nsIContent* aEndContent, const nsIContent* aContent) const;

    virtual PRBool IsBefore(const nsIContent *aNewContent, const nsIContent* aCurrentContent) const;

    virtual PRBool IsInSelection(const nsIContent *aContent) const;

    virtual nsIContent* GetPrevContent(const nsIContent *aContent) const;

    virtual nsIContent* GetNextContent(const nsIContent *aContent) const;

    virtual void SetDisplaySelection(PRBool aToggle);

    virtual PRBool GetDisplaySelection() const;

    NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext, 
                              nsEvent* aEvent, 
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus& aEventStatus);


    // nsIXMLDocument interface
    NS_IMETHOD RegisterNameSpace(nsIAtom* aPrefix, const nsString& aURI, 
                                 PRInt32& aNameSpaceId);

    NS_IMETHOD GetNameSpaceURI(PRInt32 aNameSpaceId, nsString& aURI);
    NS_IMETHOD GetNameSpacePrefix(PRInt32 aNameSpaceId, nsIAtom*& aPrefix);

    NS_IMETHOD PrologElementAt(PRInt32 aOffset, nsIContent** aContent);
    NS_IMETHOD PrologCount(PRInt32* aCount);
    NS_IMETHOD AppendToProlog(nsIContent* aContent);

    NS_IMETHOD EpilogElementAt(PRInt32 aOffset, nsIContent** aContent);
    NS_IMETHOD EpilogCount(PRInt32* aCount);
    NS_IMETHOD AppendToEpilog(nsIContent* aContent);

    // nsIRDFDocument interface
    NS_IMETHOD Init(void);
    NS_IMETHOD SetRootResource(nsIRDFNode* resource);
    NS_IMETHOD GetDataBase(nsIRDFDataBase*& result);
    NS_IMETHOD CreateChildren(nsIRDFContent* element, nsISupportsArray* children);
    NS_IMETHOD AddTreeProperty(nsIRDFNode* resource);
    NS_IMETHOD RemoveTreeProperty(nsIRDFNode* resource);

protected:
    nsIContent*
    FindContent(const nsIContent* aStartNode,
                const nsIContent* aTest1,
                const nsIContent* aTest2) const;

    PRBool IsTreeProperty(const nsIRDFNode* resource) const;

    nsresult AttachTextNode(nsIContent* parent,
                            nsIRDFNode* value);

    nsresult NewChild(const nsString& tag,
                      nsIRDFNode* resource,
                      nsIRDFContent*& result,
                      PRBool childrenMustBeGenerated);

    virtual nsresult CreateChild(nsIRDFNode* property,
                                 nsIRDFNode* value,
                                 nsIRDFContent*& result) = 0;

    nsIArena*              mArena;
    nsVoidArray            mObservers;
    nsAutoString           mDocumentTitle;
    nsIURL*                mDocumentURL;
    nsIURLGroup*           mDocumentURLGroup;
    nsIContent*            mRootContent;
    nsIDocument*           mParentDocument;
    nsIScriptContextOwner* mScriptContextOwner;
    nsCharSetID            mCharSetID;
    nsVoidArray            mStyleSheets;
    nsICollection*         mSelection;
    PRBool                 mDisplaySelection;
    nsVoidArray            mPresShells;
    nsVoidArray            mNameSpaces;
    nsIStyleSheet*         mAttrStyleSheet;
    nsIParser*             mParser;
    nsIRDFDataBase*        mDB;
    nsIRDFResourceManager* mResourceMgr;
    nsISupportsArray*      mTreeProperties;
};


#endif // nsRDFDocument_h___
