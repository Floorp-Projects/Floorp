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

  An implementation for the nsIRDFDocument interface. This
  implementation serves as the basis for generating an NGLayout
  content model.

  TO DO

  1) Figure out how to get rid of the DummyListener hack.

 */

#include "nsCOMPtr.h"
#include "nsIArena.h"
#include "nsIContent.h"
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeObserver.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDOMStyleSheetCollection.h"
#include "nsIDTD.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIEnumerator.h"
#include "nsIHTMLContentContainer.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIJSScriptObject.h"
#include "nsINameSpaceManager.h"
#include "nsIParser.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContent.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMSelection.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsIURLGroup.h"
#include "nsIWebShell.h"
#include "nsIXULContentSink.h"
#include "nsDOMCID.h"
#include "nsLayoutCID.h"
#include "nsParserCIID.h"
#include "nsRDFCID.h"
#include "nsRDFDOMNodeList.h"
#include "nsVoidArray.h"
#include "plhash.h"
#include "plstr.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kICSSParserIID,          NS_ICSS_PARSER_IID); // XXX grr..
static NS_DEFINE_IID(kIContentIID,            NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDTDIID,                NS_IDTD_IID);
static NS_DEFINE_IID(kIDOMScriptObjectFactoryIID, NS_IDOM_SCRIPT_OBJECT_FACTORY_IID);
static NS_DEFINE_IID(kIDocumentIID,           NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIHTMLContentContainerIID, NS_IHTMLCONTENTCONTAINER_IID);
static NS_DEFINE_IID(kIHTMLStyleSheetIID,     NS_IHTML_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID,     NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,   NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIParserIID,             NS_IPARSER_IID);
static NS_DEFINE_IID(kIPresShellIID,          NS_IPRESSHELL_IID);
static NS_DEFINE_IID(kIRDFCompositeDataSourceIID, NS_IRDFCOMPOSITEDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFContentModelBuilderIID, NS_IRDFCONTENTMODELBUILDER_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,      NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFDocumentIID,        NS_IRDFDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,         NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFResourceIID,        NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,         NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID,  NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMSelectionIID,       NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kIStreamListenerIID,     NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIStreamObserverIID,     NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellIID,           NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIXMLDocumentIID,        NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIXULContentSinkIID,     NS_IXULCONTENTSINK_IID);

static NS_DEFINE_CID(kCSSParserCID,             NS_CSSPARSER_CID);
static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_CID(kHTMLStyleSheetCID,        NS_HTMLSTYLESHEET_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,      NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kParserCID,                NS_PARSER_IID); // XXX
static NS_DEFINE_CID(kPresShellCID,             NS_PRESSHELL_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,      NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFXULBuilderCID,         NS_RDFXULBUILDER_CID);
static NS_DEFINE_CID(kRangeListCID,             NS_RANGELIST_CID);
static NS_DEFINE_CID(kWellFormedDTDCID,         NS_WELLFORMEDDTD_CID);
static NS_DEFINE_CID(kXULContentSinkCID,        NS_XULCONTENTSINK_CID);
static NS_DEFINE_CID(kXULDataSourceCID,			NS_XULDATASOURCE_CID);

////////////////////////////////////////////////////////////////////////

enum nsContentType {
	TEXT_RDF, 
	TEXT_XUL,
};

////////////////////////////////////////////////////////////////////////
// nsElementMap

class nsElementMap
{
private:
    PLHashTable* mResources;

    class ContentListItem {
    public:
        ContentListItem(nsIContent* aContent)
            : mNext(nsnull), mContent(aContent) {}

        ContentListItem* mNext;
        nsIContent*      mContent;
    };

    static PLHashNumber
    HashPointer(const void* key)
    {
        return (PLHashNumber) key;
    }

    static PRIntn
    ReleaseContentList(PLHashEntry* he, PRIntn index, void* closure)
    {
        ContentListItem* head =
            (ContentListItem*) he->value;

        while (head) {
            ContentListItem* doomed = head;
            head = head->mNext;
            delete doomed;
        }

        return HT_ENUMERATE_NEXT;
    }

public:
    nsElementMap(void)
    {
        // Create a table for mapping RDF resources to elements in the
        // content tree.
        static PRInt32 kInitialResourceTableSize = 1023;
        if ((mResources = PL_NewHashTable(kInitialResourceTableSize,
                                          HashPointer,
                                          PL_CompareValues,
                                          PL_CompareValues,
                                          nsnull,
                                          nsnull)) == nsnull) {
            NS_ERROR("could not create hash table for resources");
        }
    }

    virtual ~nsElementMap()
    {
        if (mResources) {
            PL_HashTableEnumerateEntries(mResources, ReleaseContentList, nsnull);
            PL_HashTableDestroy(mResources);
        }
    }

    nsresult
    Add(nsIRDFResource* aResource, nsIContent* aContent)
    {
        NS_PRECONDITION(mResources != nsnull, "not initialized");
        if (! mResources)
            return NS_ERROR_NOT_INITIALIZED;

        ContentListItem* head =
            (ContentListItem*) PL_HashTableLookup(mResources, aResource);

        if (! head) {
            head = new ContentListItem(aContent);
            if (! head)
                return NS_ERROR_OUT_OF_MEMORY;

            PL_HashTableAdd(mResources, aResource, head);
        }
        else {
            while (1) {
                if (head->mContent == aContent) {
                    NS_ERROR("attempt to add same element twice");
                    return NS_ERROR_ILLEGAL_VALUE;
                }
                if (! head->mNext)
                    break;

                head = head->mNext;
            }

            head->mNext = new ContentListItem(aContent);
            if (! head->mNext)
                return NS_ERROR_OUT_OF_MEMORY;
        }

        return NS_OK;
    }

    nsresult
    Remove(nsIRDFResource* aResource, nsIContent* aContent)
    {
        NS_PRECONDITION(mResources != nsnull, "not initialized");
        if (! mResources)
            return NS_ERROR_NOT_INITIALIZED;

        ContentListItem* head =
            (ContentListItem*) PL_HashTableLookup(mResources, aResource);

        if (head) {
            if (head->mContent == aContent) {
                ContentListItem* newHead = head->mNext;
                if (newHead) {
                    PL_HashTableAdd(mResources, aResource, newHead);
                }
                else {
                    PL_HashTableRemove(mResources, aResource);
                }
                delete head;
                return NS_OK;
            }
            else {
                ContentListItem* doomed = head->mNext;
                while (doomed) {
                    if (doomed->mContent == aContent) {
                        head->mNext = doomed->mNext;
                        delete doomed;
                        return NS_OK;
                    }
                    head = doomed;
                    doomed = doomed->mNext;
                }
            }
        }

        NS_ERROR("attempt to remove an element that was never added");
        return NS_ERROR_ILLEGAL_VALUE;
    }

    nsresult
    Find(nsIRDFResource* aResource, nsISupportsArray* aResults)
    {
        NS_PRECONDITION(mResources != nsnull, "not initialized");
        if (! mResources)
            return NS_ERROR_NOT_INITIALIZED;

        aResults->Clear();
        ContentListItem* head =
            (ContentListItem*) PL_HashTableLookup(mResources, aResource);

        while (head) {
            aResults->AppendElement(head->mContent);
            head = head->mNext;
        }
        return NS_OK;
    }
};



////////////////////////////////////////////////////////////////////////
// XULDocumentImpl

class XULDocumentImpl : public nsIDocument,
                        public nsIRDFDocument,
                        public nsIDOMDocument,
                        public nsIJSScriptObject,
                        public nsIScriptObjectOwner,
                        public nsIHTMLContentContainer,
                        public nsIDOMNodeObserver
{
public:
    XULDocumentImpl();
    virtual ~XULDocumentImpl();

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

    NS_IMETHOD GetBaseURL(nsIURL*& aURL) const;

    virtual nsString* GetDocumentCharacterSet() const;

    virtual void SetDocumentCharacterSet(nsString* aCharSetID);

    NS_IMETHOD GetHeaderData(nsIAtom* aHeaderField, nsString& aData) const;
    NS_IMETHOD SetHeaderData(nsIAtom* aheaderField, const nsString& aData);

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

    virtual PRInt32 GetIndexOfStyleSheet(nsIStyleSheet* aSheet);

    virtual void AddStyleSheet(nsIStyleSheet* aSheet);

    virtual void SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                            PRBool mDisabled);

    virtual nsIScriptContextOwner *GetScriptContextOwner();

    virtual void SetScriptContextOwner(nsIScriptContextOwner *aScriptContextOwner);

    NS_IMETHOD GetNameSpaceManager(nsINameSpaceManager*& aManager);

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

    NS_IMETHOD GetSelection(nsIDOMSelection** aSelection);

    NS_IMETHOD SelectAll();

    NS_IMETHOD FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound);

    virtual void CreateXIF(nsString & aBuffer, nsIDOMSelection* aSelection);

    virtual void ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual void BeginConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual void ConvertChildrenToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual void FinishConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual PRBool IsInRange(const nsIContent *aStartContent, const nsIContent* aEndContent, const nsIContent* aContent) const;

    virtual PRBool IsBefore(const nsIContent *aNewContent, const nsIContent* aCurrentContent) const;

    virtual PRBool IsInSelection(nsIDOMSelection* aSelection, const nsIContent *aContent) const;

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
    NS_IMETHOD PrologElementAt(PRUint32 aOffset, nsIContent** aContent);
    NS_IMETHOD PrologCount(PRUint32* aCount);
    NS_IMETHOD AppendToProlog(nsIContent* aContent);
    NS_IMETHOD EpilogElementAt(PRUint32 aOffset, nsIContent** aContent);
    NS_IMETHOD EpilogCount(PRUint32* aCount);
    NS_IMETHOD AppendToEpilog(nsIContent* aContent);

    // nsIRDFDocument interface
    NS_IMETHOD SetRootResource(nsIRDFResource* resource);
    NS_IMETHOD SplitProperty(nsIRDFResource* aResource, PRInt32* aNameSpaceID, nsIAtom** aTag);
    NS_IMETHOD AddElementForResource(nsIRDFResource* aResource, nsIRDFContent* aElement);
    NS_IMETHOD RemoveElementForResource(nsIRDFResource* aResource, nsIRDFContent* aElement);
    NS_IMETHOD GetElementsForResource(nsIRDFResource* aResource, nsISupportsArray* aElements);
    NS_IMETHOD CreateContents(nsIRDFContent* aElement);
    NS_IMETHOD AddContentModelBuilder(nsIRDFContentModelBuilder* aBuilder);

    // nsIDOMDocument interface
    NS_IMETHOD    GetDoctype(nsIDOMDocumentType** aDoctype);
    NS_IMETHOD    GetImplementation(nsIDOMDOMImplementation** aImplementation);
    NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement);

    NS_IMETHOD    CreateElement(const nsString& aTagName, nsIDOMElement** aReturn);
    NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn);
    NS_IMETHOD    CreateTextNode(const nsString& aData, nsIDOMText** aReturn);
    NS_IMETHOD    CreateComment(const nsString& aData, nsIDOMComment** aReturn);
    NS_IMETHOD    CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn);
    NS_IMETHOD    CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn);
    NS_IMETHOD    CreateAttribute(const nsString& aName, nsIDOMAttr** aReturn);
    NS_IMETHOD    CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn);
    NS_IMETHOD    GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn);
    NS_IMETHOD    GetStyleSheets(nsIDOMStyleSheetCollection** aStyleSheets);
                     
    // nsIDOMNode interface
    NS_IMETHOD    GetNodeName(nsString& aNodeName);
    NS_IMETHOD    GetNodeValue(nsString& aNodeValue);
    NS_IMETHOD    SetNodeValue(const nsString& aNodeValue);
    NS_IMETHOD    GetNodeType(PRUint16* aNodeType);
    NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode);
    NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes);
    NS_IMETHOD    HasChildNodes(PRBool* aHasChildNodes);
    NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild);
    NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild);
    NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling);
    NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling);
    NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes);
    NS_IMETHOD    GetOwnerDocument(nsIDOMDocument** aOwnerDocument);
    NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn);
    NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
    NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
    NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn);
    NS_IMETHOD    CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

    // nsIJSScriptObject interface
    virtual PRBool    AddProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool    DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool    GetProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool    SetProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool    EnumerateProperty(JSContext *aContext);
    virtual PRBool    Resolve(JSContext *aContext, jsval aID);
    virtual PRBool    Convert(JSContext *aContext, jsval aID);
    virtual void      Finalize(JSContext *aContext);

    // nsIScriptObjectOwner interface
    NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD SetScriptObject(void *aScriptObject);

    // nsIHTMLContentContainer interface
    NS_IMETHOD GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult);
    NS_IMETHOD GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult);

    // nsIDOMNodeObserver interface
    NS_DECL_IDOMNODEOBSERVER

    // Implementation methods
    nsresult Init(void);
    nsresult StartLayout(void);

    nsresult
    GetElementsByTagName(nsIDOMNode* aNode,
                         const nsString& aTagName,
                         nsRDFDOMNodeList* aElements);

protected:
    nsIContent*
    FindContent(const nsIContent* aStartNode,
                const nsIContent* aTest1,
                const nsIContent* aTest2) const;

    nsresult
    LoadCSSStyleSheet(nsIURL* url);

    nsresult
    AddNamedDataSource(const char* uri);


    nsIArena*                  mArena;
    nsVoidArray                mObservers;
    nsAutoString               mDocumentTitle;
    nsIURL*                    mDocumentURL;
    nsIURLGroup*               mDocumentURLGroup;
    nsIContent*                mRootContent;
    nsIDocument*               mParentDocument;
    nsIScriptContextOwner*     mScriptContextOwner;
    void*                      mScriptObject;
    nsString*                  mCharSetID;
    nsVoidArray                mStyleSheets;
    nsIDOMSelection*           mSelection;
    PRBool                     mDisplaySelection;
    nsVoidArray                mPresShells;
    nsINameSpaceManager*       mNameSpaceManager;
    nsIHTMLStyleSheet*         mAttrStyleSheet;
    nsIHTMLCSSStyleSheet*      mInlineStyleSheet;
    nsIRDFService*             mRDFService;
    nsElementMap               mResources;
    nsISupportsArray*          mBuilders;
    nsIRDFContentModelBuilder* mXULBuilder;
    nsIRDFDataSource*          mLocalDataSource;
    nsIRDFDataSource*          mDocumentDataSource;
    nsIParser*                 mParser;
};


////////////////////////////////////////////////////////////////////////
// ctors & dtors

XULDocumentImpl::XULDocumentImpl(void)
    : mArena(nsnull),
      mDocumentURL(nsnull),
      mDocumentURLGroup(nsnull),
      mRootContent(nsnull),
      mParentDocument(nsnull),
      mScriptContextOwner(nsnull),
      mScriptObject(nsnull),
      mSelection(nsnull),
      mDisplaySelection(PR_FALSE),
      mNameSpaceManager(nsnull),
      mAttrStyleSheet(nsnull),
      mRDFService(nsnull),
      mBuilders(nsnull),
      mXULBuilder(nsnull),
      mLocalDataSource(nsnull),
      mDocumentDataSource(nsnull),
      mParser(nsnull)
{
    NS_INIT_REFCNT();

    nsresult rv;

    // construct a selection object
    if (NS_FAILED(rv = nsRepository::CreateInstance(kRangeListCID,
                                                    nsnull,
                                                    kIDOMSelectionIID,
                                                    (void**) &mSelection)))
        PR_ASSERT(0);

    
}

XULDocumentImpl::~XULDocumentImpl()
{
    NS_IF_RELEASE(mDocumentDataSource);
    NS_IF_RELEASE(mLocalDataSource);

    if (mRDFService) {
        nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);
        mRDFService = nsnull;
    }

    // mParentDocument is never refcounted
    NS_IF_RELEASE(mBuilders);
    NS_IF_RELEASE(mXULBuilder);
    NS_IF_RELEASE(mSelection);
    NS_IF_RELEASE(mScriptContextOwner);
    NS_IF_RELEASE(mAttrStyleSheet);
    NS_IF_RELEASE(mRootContent);
    NS_IF_RELEASE(mDocumentURLGroup);
    NS_IF_RELEASE(mDocumentURL);
    NS_IF_RELEASE(mArena);
    NS_IF_RELEASE(mNameSpaceManager);
    NS_IF_RELEASE(mParser);
}


nsresult
NS_NewXULDocument(nsIRDFDocument** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    XULDocumentImpl* doc = new XULDocumentImpl();
    if (! doc)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(doc);

    nsresult rv;
    if (NS_FAILED(rv = doc->Init())) {
        NS_RELEASE(doc);
        return rv;
    }

    *result = doc;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMETHODIMP 
XULDocumentImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIDocumentIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIDocument*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (iid.Equals(kIRDFDocumentIID) ||
             iid.Equals(kIXMLDocumentIID)) {
        *result = NS_STATIC_CAST(nsIRDFDocument*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (iid.Equals(nsIDOMDocument::IID()) ||
             iid.Equals(nsIDOMNode::IID())) {
        *result = NS_STATIC_CAST(nsIDOMDocument*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (iid.Equals(kIJSScriptObjectIID)) {
        *result = NS_STATIC_CAST(nsIJSScriptObject*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (iid.Equals(kIScriptObjectOwnerIID)) {
        *result = NS_STATIC_CAST(nsIScriptObjectOwner*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (iid.Equals(kIHTMLContentContainerIID)) {
        *result = NS_STATIC_CAST(nsIHTMLContentContainer*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (iid.Equals(nsIDOMNodeObserver::IID())) {
        *result = NS_STATIC_CAST(nsIDOMNodeObserver*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(XULDocumentImpl);
NS_IMPL_RELEASE(XULDocumentImpl);

////////////////////////////////////////////////////////////////////////
// nsIDocument interface

nsIArena*
XULDocumentImpl::GetArena()
{
    NS_IF_ADDREF(mArena);
    return mArena;
}

NS_IMETHODIMP 
XULDocumentImpl::StartDocumentLoad(nsIURL *aURL, 
                                   nsIContentViewerContainer* aContainer,
                                   nsIStreamListener **aDocListener,
                                   const char* aCommand)
{
    NS_ASSERTION(aURL != nsnull, "null ptr");
    if (! aURL)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    mDocumentTitle.Truncate();

    NS_IF_RELEASE(mDocumentURL);
    mDocumentURL = aURL;
    NS_ADDREF(aURL);

    NS_IF_RELEASE(mDocumentURLGroup);
    (void)aURL->GetURLGroup(&mDocumentURLGroup);

    // Delete references to style sheets - this should be done in superclass...
    PRInt32 index = mStyleSheets.Count();
    while (--index >= 0) {
        nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(index);
        sheet->SetOwningDocument(nsnull);
        NS_RELEASE(sheet);
    }
    mStyleSheets.Clear();

    // Create an HTML style sheet for the HTML content.
    nsIHTMLStyleSheet* sheet;
    if (NS_SUCCEEDED(rv = nsRepository::CreateInstance(kHTMLStyleSheetCID,
                                                       nsnull,
                                                       kIHTMLStyleSheetIID,
                                                       (void**) &sheet))) {
        if (NS_SUCCEEDED(rv = sheet->Init(aURL, this))) {
            mAttrStyleSheet = sheet;
            NS_ADDREF(mAttrStyleSheet);

            AddStyleSheet(mAttrStyleSheet);
        }
        NS_RELEASE(sheet);
    }

    if (NS_FAILED(rv)) {
        NS_ERROR("unable to add HTML style sheet");
        return rv;
    }
      
    // Create a composite data source that'll tie together local and
    // remote stores.
    nsIRDFCompositeDataSource* db;
    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFCompositeDataSourceCID,
                                                    nsnull,
                                                    kIRDFCompositeDataSourceIID,
                                                    (void**) &db))) {
        NS_ERROR("couldn't create composite datasource");
        return rv;
    }

    // Create a XUL content model builder
    NS_IF_RELEASE(mXULBuilder);
    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFXULBuilderCID,
                                                    nsnull,                     
                                                    kIRDFContentModelBuilderIID,
                                                    (void**) &mXULBuilder))) {
        NS_ERROR("couldn't create XUL builder");
        return rv;
    }

    if (NS_FAILED(rv = mXULBuilder->SetDataBase(db))) {
        NS_ERROR("couldn't set builder's db");
        return rv;
    }

    NS_IF_RELEASE(mBuilders);
    if (NS_FAILED(rv = AddContentModelBuilder(mXULBuilder))) {
        NS_ERROR("could't add XUL builder");
        return rv;
    }

    // Create a "scratch" in-memory data store to associate with the
    // document to be a catch-all for any doc-specific info that we
    // need to store (e.g., current sort order, etc.)
    //
    // XXX This needs to be cloned across windows, and the final
    // instance needs to be flushed to disk. It may be that this is
    // really an RDFXML data source...
    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFInMemoryDataSourceCID,
                                                    nsnull,
                                                    kIRDFDataSourceIID,
                                                    (void**) &mLocalDataSource))) {
        NS_ERROR("couldn't create local data source");
        return rv;
    }

    if (NS_FAILED(rv = db->AddDataSource(mLocalDataSource))) {
        NS_ERROR("couldn't add local data source to db");
        return rv;
    }

    // Now load the actual RDF/XML document data source. First, we'll
    // see if the data source has been loaded and is registered with
    // the RDF service. If so, do some monkey business to "pretend" to
    // load it: really, we'll just walk its graph to generate the
    // content model.
    const char* uri;
    if (NS_FAILED(rv = aURL->GetSpec(&uri)))
        return rv;

    // We need to construct a new stream and load it. The stream will
    // automagically register itself as a named data source, so if
    // subsequent docs ask for it, they'll get the real deal. In the
    // meantime, add us as an nsIRDFXMLDataSourceObserver so that
    // we'll be notified when we need to load style sheets, etc.
    NS_IF_RELEASE(mDocumentDataSource);
    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFInMemoryDataSourceCID,
                                                    nsnull,
                                                    kIRDFDataSourceIID,
                                                    (void**) &mDocumentDataSource))) {
        NS_ERROR("unable to create XUL datasource");
        return rv;
    }

    if (NS_FAILED(rv = db->AddDataSource(mDocumentDataSource))) {
        NS_ERROR("unable to add XUL datasource to db");
        return rv;
    }

    if (NS_FAILED(rv = mDocumentDataSource->Init(uri))) {
        NS_ERROR("unable to initialize XUL data source");
        return rv;
    }

    nsCOMPtr<nsIXULContentSink> sink;
    if (NS_FAILED(rv = nsRepository::CreateInstance(kXULContentSinkCID,
                                                    nsnull,
                                                    kIXULContentSinkIID,
                                                    (void**) getter_AddRefs(sink)))) {
        NS_ERROR("unable to create XUL content sink");
        return rv;
    }

    {
        nsCOMPtr<nsIWebShell> webShell;
        aContainer->QueryInterface(kIWebShellIID, getter_AddRefs(webShell));

        if (NS_FAILED(rv = sink->Init(this, webShell, mDocumentDataSource))) {
            NS_ERROR("Unable to initialize XUL content sink");
            return rv;
        }
    }

    if (NS_FAILED(rv = nsRepository::CreateInstance(kParserCID,
                                                    nsnull,
                                                    kIParserIID,
                                                    (void**) &mParser))) {
        NS_ERROR("unable to create parser");
        return rv;
    }

    if (NS_FAILED(rv = mParser->QueryInterface(kIStreamListenerIID, (void**) aDocListener))) {
        NS_ERROR("parser doesn't support nsIStreamListener");
        return rv;
    }

    nsCOMPtr<nsIDTD> dtd;
    if (NS_FAILED(rv = nsRepository::CreateInstance(kWellFormedDTDCID,
                                                    nsnull,
                                                    kIDTDIID,
                                                    (void**) getter_AddRefs(dtd)))) {
        NS_ERROR("unable to construct DTD");
        return rv;
    }

    mParser->RegisterDTD(dtd);
    mParser->SetCommand(aCommand);
    mParser->SetContentSink(sink);
    mParser->Parse(aURL);
   
    return NS_OK;
}

const nsString*
XULDocumentImpl::GetDocumentTitle() const
{
    return &mDocumentTitle;
}

nsIURL* 
XULDocumentImpl::GetDocumentURL() const
{
    NS_IF_ADDREF(mDocumentURL);
    return mDocumentURL;
}

nsIURLGroup* 
XULDocumentImpl::GetDocumentURLGroup() const
{
    NS_IF_ADDREF(mDocumentURLGroup);
    return mDocumentURLGroup;
}

NS_IMETHODIMP 
XULDocumentImpl::GetBaseURL(nsIURL*& aURL) const
{
    NS_IF_ADDREF(mDocumentURL);
    aURL = mDocumentURL;
    return NS_OK;
}

nsString* 
XULDocumentImpl::GetDocumentCharacterSet() const
{
    return mCharSetID;
}

void 
XULDocumentImpl::SetDocumentCharacterSet(nsString* aCharSetID)
{
    mCharSetID = aCharSetID;
}

NS_IMETHODIMP
XULDocumentImpl::GetHeaderData(nsIAtom* aHeaderField, nsString& aData) const
{
  return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl:: SetHeaderData(nsIAtom* aheaderField, const nsString& aData)
{
  return NS_OK;
}


nsresult 
XULDocumentImpl::CreateShell(nsIPresContext* aContext,
                             nsIViewManager* aViewManager,
                             nsIStyleSet* aStyleSet,
                             nsIPresShell** aInstancePtrResult)
{
    NS_PRECONDITION(aInstancePtrResult, "null ptr");
    if (! aInstancePtrResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsIPresShell* shell;
    if (NS_FAILED(rv = nsRepository::CreateInstance(kPresShellCID,
                                                    nsnull,
                                                    kIPresShellIID,
                                                    (void**) &shell)))
        return rv;

    if (NS_FAILED(rv = shell->Init(this, aContext, aViewManager, aStyleSet))) {
        NS_RELEASE(shell);
        return rv;
    }

    mPresShells.AppendElement(shell);
    *aInstancePtrResult = shell; // addref implicit

    return NS_OK;
}

PRBool 
XULDocumentImpl::DeleteShell(nsIPresShell* aShell)
{
    return mPresShells.RemoveElement(aShell);
}

PRInt32 
XULDocumentImpl::GetNumberOfShells()
{
    return mPresShells.Count();
}

nsIPresShell* 
XULDocumentImpl::GetShellAt(PRInt32 aIndex)
{
    nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[aIndex]);
    NS_IF_ADDREF(shell);
    return shell;
}

nsIDocument* 
XULDocumentImpl::GetParentDocument()
{
    NS_IF_ADDREF(mParentDocument);
    return mParentDocument;
}

void 
XULDocumentImpl::SetParentDocument(nsIDocument* aParent)
{
    // Note that we do *not* AddRef our parent because that would
    // create a circular reference.
    mParentDocument = aParent;
}

void 
XULDocumentImpl::AddSubDocument(nsIDocument* aSubDoc)
{
    // we don't do subdocs.
    PR_ASSERT(0);
}

PRInt32 
XULDocumentImpl::GetNumberOfSubDocuments()
{
    return 0;
}

nsIDocument* 
XULDocumentImpl::GetSubDocumentAt(PRInt32 aIndex)
{
    // we don't do subdocs.
    PR_ASSERT(0);
    return nsnull;
}

nsIContent* 
XULDocumentImpl::GetRootContent()
{
    NS_IF_ADDREF(mRootContent);
    return mRootContent;
}

void 
XULDocumentImpl::SetRootContent(nsIContent* aRoot)
{
    if (mRootContent) {
        mRootContent->SetDocument(nsnull, PR_TRUE);
        NS_RELEASE(mRootContent);
    }
    mRootContent = aRoot;
    if (mRootContent) {
        mRootContent->SetDocument(this, PR_TRUE);
        NS_ADDREF(mRootContent);
    }
}

PRInt32 
XULDocumentImpl::GetNumberOfStyleSheets()
{
    return mStyleSheets.Count();
}

nsIStyleSheet* 
XULDocumentImpl::GetStyleSheetAt(PRInt32 aIndex)
{
    nsIStyleSheet* sheet = NS_STATIC_CAST(nsIStyleSheet*, mStyleSheets[aIndex]);
    NS_IF_ADDREF(sheet);
    return sheet;
}

PRInt32 
XULDocumentImpl::GetIndexOfStyleSheet(nsIStyleSheet* aSheet)
{
  return mStyleSheets.IndexOf(aSheet);
}

void 
XULDocumentImpl::AddStyleSheet(nsIStyleSheet* aSheet)
{
    NS_PRECONDITION(aSheet, "null arg");
    if (!aSheet)
        return;

    mStyleSheets.AppendElement(aSheet);
    NS_ADDREF(aSheet);

    aSheet->SetOwningDocument(this);

    PRBool enabled;
    aSheet->GetEnabled(enabled);

    if (enabled) {
        PRInt32 count, index;

        count = mPresShells.Count();
        for (index = 0; index < count; index++) {
            nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[index]);
            nsCOMPtr<nsIStyleSet> set;
            shell->GetStyleSet(getter_AddRefs(set));
            if (set) {
                set->AddDocStyleSheet(aSheet, this);
            }
        }

        // XXX should observers be notified for disabled sheets??? I think not, but I could be wrong
        count = mObservers.Count();
        for (index = 0; index < count; index++) {
            nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
            observer->StyleSheetAdded(this, aSheet);
        }
    }
}

void 
XULDocumentImpl::SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                          PRBool aDisabled)
{
    NS_PRECONDITION(nsnull != aSheet, "null arg");
    PRInt32 count;
    PRInt32 index = mStyleSheets.IndexOf((void *)aSheet);
    // If we're actually in the document style sheet list
    if (-1 != index) {
        count = mPresShells.Count();
        PRInt32 index;
        for (index = 0; index < count; index++) {
            nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(index);
            nsCOMPtr<nsIStyleSet> set;
            shell->GetStyleSet(getter_AddRefs(set));
            if (set) {
                if (aDisabled) {
                    set->RemoveDocStyleSheet(aSheet);
                }
                else {
                    set->AddDocStyleSheet(aSheet, this);  // put it first
                }
            }
        }
    }  

    count = mObservers.Count();
    for (index = 0; index < count; index++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
        observer->StyleSheetDisabledStateChanged(this, aSheet, aDisabled);
    }
}

nsIScriptContextOwner *
XULDocumentImpl::GetScriptContextOwner()
{
    NS_IF_ADDREF(mScriptContextOwner);
    return mScriptContextOwner;
}

void 
XULDocumentImpl::SetScriptContextOwner(nsIScriptContextOwner *aScriptContextOwner)
{
    // XXX HACK ALERT! If the script context owner is null, the document
    // will soon be going away. So tell our content that to lose its
    // reference to the document. This has to be done before we
    // actually set the script context owner to null so that the
    // content elements can remove references to their script objects.
    if (!aScriptContextOwner && mRootContent)
        mRootContent->SetDocument(nsnull, PR_TRUE);

    NS_IF_RELEASE(mScriptContextOwner);
    mScriptContextOwner = aScriptContextOwner;
    NS_IF_ADDREF(mScriptContextOwner);
}

NS_IMETHODIMP
XULDocumentImpl::GetNameSpaceManager(nsINameSpaceManager*& aManager)
{
  aManager = mNameSpaceManager;
  NS_IF_ADDREF(aManager);
  return NS_OK;
}


// Note: We don't hold a reference to the document observer; we assume
// that it has a live reference to the document.
void 
XULDocumentImpl::AddObserver(nsIDocumentObserver* aObserver)
{
    // XXX Make sure the observer isn't already in the list
    if (mObservers.IndexOf(aObserver) == -1) {
        mObservers.AppendElement(aObserver);
    }
}

PRBool 
XULDocumentImpl::RemoveObserver(nsIDocumentObserver* aObserver)
{
    return mObservers.RemoveElement(aObserver);
}

NS_IMETHODIMP 
XULDocumentImpl::BeginLoad()
{
    PRInt32 i, count = mObservers.Count();
    for (i = 0; i < count; i++) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->BeginLoad(this);
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::EndLoad()
{
    StartLayout();

    PRInt32 i, count = mObservers.Count();
    for (i = 0; i < count; i++) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->EndLoad(this);
    }
    return NS_OK;
}


NS_IMETHODIMP 
XULDocumentImpl::ContentChanged(nsIContent* aContent,
                              nsISupports* aSubContent)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentChanged(this, aContent, aSubContent);
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::AttributeChanged(nsIContent* aChild,
                                nsIAtom* aAttribute,
                                PRInt32 aHint)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->AttributeChanged(this, aChild, aAttribute, aHint);
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentAppended(nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentAppended(this, aContainer, aNewIndexInContainer);
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentInserted(nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer)
{
    
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentInserted(this, aContainer, aChild, aIndexInContainer);
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentReplaced(nsIContent* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aNewChild,
                               PRInt32 aIndexInContainer)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentReplaced(this, aContainer, aOldChild, aNewChild,
                                  aIndexInContainer);
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentRemoved(nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentRemoved(this, aContainer, 
                                 aChild, aIndexInContainer);
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleChanged(this, aStyleSheet, aStyleRule, aHint);
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleAdded(this, aStyleSheet, aStyleRule);
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleRemoved(this, aStyleSheet, aStyleRule);
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::GetSelection(nsIDOMSelection** aSelection)
{
    if (!mSelection) {
        PR_ASSERT(0);
        *aSelection = nsnull;
        return NS_ERROR_NOT_INITIALIZED;
    }
    NS_ADDREF(mSelection);
    *aSelection = mSelection;
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::SelectAll()
{

    nsIContent * start = nsnull;
    nsIContent * end   = nsnull;
    nsIContent * body  = nsnull;

    nsString bodyStr("BODY");
    PRInt32 i, n;
    mRootContent->ChildCount(n);
    for (i=0;i<n;i++) {
        nsIContent * child;
        mRootContent->ChildAt(i, child);
        PRBool isSynthetic;
        child->IsSynthetic(isSynthetic);
        if (!isSynthetic) {
            nsIAtom * atom;
            child->GetTag(atom);
            if (bodyStr.EqualsIgnoreCase(atom)) {
                body = child;
                break;
            }

        }
        NS_RELEASE(child);
    }

    if (body == nsnull) {
        return NS_ERROR_FAILURE;
    }

    start = body;
    // Find Very first Piece of Content
    for (;;) {
        start->ChildCount(n);
        if (n <= 0) {
            break;
        }
        nsIContent * child = start;
        child->ChildAt(0, start);
        NS_RELEASE(child);
    }

    end = body;
    // Last piece of Content
    for (;;) {
        end->ChildCount(n);
        if (n <= 0) {
            break;
        }
        nsIContent * child = end;
        child->ChildAt(n-1, end);
        NS_RELEASE(child);
    }

    //NS_RELEASE(start);
    //NS_RELEASE(end);

#if 0 // XXX nsSelectionRange is in another DLL
    nsSelectionRange * range    = mSelection->GetRange();
    nsSelectionPoint * startPnt = range->GetStartPoint();
    nsSelectionPoint * endPnt   = range->GetEndPoint();
    startPnt->SetPoint(start, -1, PR_TRUE);
    endPnt->SetPoint(end, -1, PR_FALSE);
#endif
    SetDisplaySelection(PR_TRUE);

    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
    aIsFound = PR_FALSE;
    return NS_ERROR_FAILURE;
}

void 
XULDocumentImpl::CreateXIF(nsString & aBuffer, nsIDOMSelection* aSelection)
{
    PR_ASSERT(0);
}

void 
XULDocumentImpl::ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

void 
XULDocumentImpl::BeginConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

void 
XULDocumentImpl::ConvertChildrenToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

void 
XULDocumentImpl::FinishConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

PRBool 
XULDocumentImpl::IsInRange(const nsIContent *aStartContent, const nsIContent* aEndContent, const nsIContent* aContent) const
{
    PRBool  result;

    if (aStartContent == aEndContent) {
            return PRBool(aContent == aStartContent);
    }
    else if (aStartContent == aContent || aEndContent == aContent) {
        result = PR_TRUE;
    }
    else {
        result = IsBefore(aStartContent,aContent);
        if (result)
            result = IsBefore(aContent, aEndContent);
    }
    return result;
}

PRBool 
XULDocumentImpl::IsBefore(const nsIContent *aNewContent, const nsIContent* aCurrentContent) const
{
    PRBool result = PR_FALSE;

    if (nsnull != aNewContent && nsnull != aCurrentContent && aNewContent != aCurrentContent) {
        nsIContent* test = FindContent(mRootContent, aNewContent, aCurrentContent);
        if (test == aNewContent)
            result = PR_TRUE;

        NS_RELEASE(test);
    }
    return result;
}

PRBool 
XULDocumentImpl::IsInSelection(nsIDOMSelection* aSelection, const nsIContent *aContent) const
{
    PRBool  result = PR_FALSE;

    if (mSelection != nsnull) {
#if 0 // XXX can't include this because nsSelectionPoint is in another DLL.
        nsSelectionRange* range = mSelection->GetRange();
        if (range != nsnull) {
            nsSelectionPoint* startPoint = range->GetStartPoint();
            nsSelectionPoint* endPoint = range->GetEndPoint();

            nsIContent* startContent = startPoint->GetContent();
            nsIContent* endContent = endPoint->GetContent();
            result = IsInRange(startContent, endContent, aContent);
            NS_IF_RELEASE(startContent);
            NS_IF_RELEASE(endContent);
        }
#endif
    }
    return result;
}

nsIContent* 
XULDocumentImpl::GetPrevContent(const nsIContent *aContent) const
{
    nsIContent* result = nsnull;
 
    // Look at previous sibling

    if (nsnull != aContent) {
        nsIContent* parent; 
        aContent->GetParent(parent);

        if (parent != nsnull && parent != mRootContent) {
            PRInt32 index;
            parent->IndexOf((nsIContent*)aContent, index);
            if (index > 0)
                parent->ChildAt(index-1, result);
            else
                result = GetPrevContent(parent);
        }
        NS_IF_RELEASE(parent);
    }
    return result;
}

nsIContent* 
XULDocumentImpl::GetNextContent(const nsIContent *aContent) const
{
    nsIContent* result = nsnull;
   
    if (nsnull != aContent) {
        // Look at next sibling
        nsIContent* parent;
        aContent->GetParent(parent);

        if (parent != nsnull && parent != mRootContent) {
            PRInt32 index;
            parent->IndexOf((nsIContent*)aContent, index);

            PRInt32 count;
            parent->ChildCount(count);
            if (index+1 < count) {
                parent->ChildAt(index+1, result);
                // Get first child down the tree
                for (;;) {
                    PRInt32 n;
                    result->ChildCount(n);
                    if (n <= 0)
                        break;

                    nsIContent * old = result;
                    old->ChildAt(0, result);
                    NS_RELEASE(old);
                    result->ChildCount(n);
                }
            } else {
                result = GetNextContent(parent);
            }
        }
        NS_IF_RELEASE(parent);
    }
    return result;
}

void 
XULDocumentImpl::SetDisplaySelection(PRBool aToggle)
{
    mDisplaySelection = aToggle;
}

PRBool 
XULDocumentImpl::GetDisplaySelection() const
{
    return mDisplaySelection;
}

NS_IMETHODIMP 
XULDocumentImpl::HandleDOMEvent(nsIPresContext& aPresContext, 
                            nsEvent* aEvent, 
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// nsIXMLDocument interface

NS_IMETHODIMP
XULDocumentImpl::PrologElementAt(PRUint32 aOffset, nsIContent** aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
XULDocumentImpl::PrologCount(PRUint32* aCount)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
XULDocumentImpl::AppendToProlog(nsIContent* aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::EpilogElementAt(PRUint32 aOffset, nsIContent** aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
XULDocumentImpl::EpilogCount(PRUint32* aCount)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
XULDocumentImpl::AppendToEpilog(nsIContent* aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// nsIRDFDocument interface

NS_IMETHODIMP
XULDocumentImpl::SetRootResource(nsIRDFResource* aResource)
{
    NS_PRECONDITION(mXULBuilder != nsnull, "not initialized");
    if (! mXULBuilder)
        return NS_ERROR_NOT_INITIALIZED;

    NS_PRECONDITION(mRootContent == nsnull, "already initialize");
    if (mRootContent)
        return NS_ERROR_ALREADY_INITIALIZED;

    nsresult rv = mXULBuilder->CreateRootContent(aResource);

    NS_POSTCONDITION(mRootContent != nsnull, "root content wasn't set");
    return rv;
}

NS_IMETHODIMP
XULDocumentImpl::SplitProperty(nsIRDFResource* aProperty,
                               PRInt32* aNameSpaceID,
                               nsIAtom** aTag)
{
    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    // XXX okay, this is a total hack. for this to work right, we need
    // to either:
    //
    // 1) Remember what the namespace and the tag were when the
    //    property was created, or
    //
    // 2) Iterate the entire set of namespace prefixes to see if the
    //    specified property's URI has any of them as a substring.
    //

    const char* p;
    aProperty->GetValue(&p);
    nsAutoString uri(p);

    PRInt32 index;

    // First try to split the namespace using the rightmost '#' or '/'
    // character.
    if ((index = uri.RFind('#')) < 0) {
        if ((index = uri.RFind('/')) < 0) {
            NS_ERROR("make this smarter!");
            return NS_ERROR_FAILURE;
        }
    }

    // Using the above info, split the property's URI into a namespace
    // prefix (that includes the trailing '#' or '/' character) and a
    // tag.
    nsAutoString tag;
    PRInt32 count = uri.Length() - (index + 1);
    uri.Right(tag, count);
    uri.Cut(index + 1, count);

    // XXX XUL atoms seem to all be in lower case. HTML atoms are in
    // upper case. This sucks.
    //tag.ToUpperCase();  // All XML is case sensitive even HTML in XML
 
    nsresult rv;
    PRInt32 nameSpaceID;
    rv = mNameSpaceManager->GetNameSpaceID(uri, nameSpaceID);

    // Did we find one?
    if (NS_SUCCEEDED(rv) && nameSpaceID != kNameSpaceID_Unknown) {
        *aNameSpaceID = nameSpaceID;

        *aTag = NS_NewAtom(tag);
        return NS_OK;
    }

    // Nope: so we'll need to be a bit more creative. Let's see
    // what happens if we remove the rightmost '#' and then try...
    if (uri.Last() == '#') {
        uri.Truncate(uri.Length() - 1);
        rv = mNameSpaceManager->GetNameSpaceID(uri, nameSpaceID);

        if (NS_SUCCEEDED(rv) && nameSpaceID != kNameSpaceID_Unknown) {
            *aNameSpaceID = nameSpaceID;

            *aTag = NS_NewAtom(tag);
            return NS_OK;
        }
    }

    // Okay, somebody make this code even more convoluted.
    NS_ERROR("unable to convert URI to namespace/tag pair");
    return NS_ERROR_FAILURE; // XXX?
}


NS_IMETHODIMP
XULDocumentImpl::AddElementForResource(nsIRDFResource* aResource, nsIRDFContent* aElement)
{
    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    mResources.Add(aResource, aElement);
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::RemoveElementForResource(nsIRDFResource* aResource, nsIRDFContent* aElement)
{
    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    mResources.Remove(aResource, aElement);
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetElementsForResource(nsIRDFResource* aResource, nsISupportsArray* aElements)
{
    NS_PRECONDITION(aElements != nsnull, "null ptr");
    if (! aElements)
        return NS_ERROR_NULL_POINTER;

    mResources.Find(aResource, aElements);
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::CreateContents(nsIRDFContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    if (! mBuilders)
        return NS_ERROR_NOT_INITIALIZED;

    for (PRInt32 i = 0; i < mBuilders->Count(); ++i) {
        // XXX we should QueryInterface() here
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        NS_ASSERTION(builder != nsnull, "null ptr");
        if (! builder)
            continue;

        nsresult rv = builder->CreateContents(aElement);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error creating content");
        // XXX ignore error code?

        NS_RELEASE(builder);
    }

    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::AddContentModelBuilder(nsIRDFContentModelBuilder* aBuilder)
{
    NS_PRECONDITION(aBuilder != nsnull, "null ptr");
    if (! aBuilder)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    if (! mBuilders) {
        if (NS_FAILED(rv = NS_NewISupportsArray(&mBuilders)))
            return rv;
    }

    if (NS_FAILED(rv = aBuilder->SetDocument(this))) {
        NS_ERROR("unable to set builder's document");
        return rv;
    }

    return mBuilders->AppendElement(aBuilder) ? NS_OK : NS_ERROR_FAILURE;
}



////////////////////////////////////////////////////////////////////////
// nsIDOMDocument interface
NS_IMETHODIMP
XULDocumentImpl::GetDoctype(nsIDOMDocumentType** aDoctype)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetImplementation(nsIDOMDOMImplementation** aImplementation)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetDocumentElement(nsIDOMElement** aDocumentElement)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
XULDocumentImpl::CreateElement(const nsString& aTagName, nsIDOMElement** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateTextNode(const nsString& aData, nsIDOMText** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateComment(const nsString& aData, nsIDOMComment** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateAttribute(const nsString& aName, nsIDOMAttr** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetElementsByTagName(const nsString& aTagName, nsIDOMNodeList** aReturn)
{
    nsresult rv;
    nsRDFDOMNodeList* elements;
    if (NS_FAILED(rv = nsRDFDOMNodeList::Create(&elements))) {
        NS_ERROR("unable to create node list");
        return rv;
    }

    nsIContent* root = GetRootContent();
    NS_ASSERTION(root != nsnull, "no doc root");

    if (root != nsnull) {
        nsIDOMNode* domRoot;
        if (NS_SUCCEEDED(rv = root->QueryInterface(nsIDOMNode::IID(), (void**) &domRoot))) {
            rv = GetElementsByTagName(domRoot, aTagName, elements);
            NS_RELEASE(domRoot);
        }
        NS_RELEASE(root);
    }

    *aReturn = elements;
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetStyleSheets(nsIDOMStyleSheetCollection** aStyleSheets)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



////////////////////////////////////////////////////////////////////////
// nsIDOMNode interface
NS_IMETHODIMP
XULDocumentImpl::GetNodeName(nsString& aNodeName)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetNodeValue(nsString& aNodeValue)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::SetNodeValue(const nsString& aNodeValue)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetNodeType(PRUint16* aNodeType)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetParentNode(nsIDOMNode** aParentNode)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::HasChildNodes(PRBool* aHasChildNodes)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetFirstChild(nsIDOMNode** aFirstChild)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetLastChild(nsIDOMNode** aLastChild)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetNextSibling(nsIDOMNode** aNextSibling)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// nsIJSScriptObject interface
PRBool
XULDocumentImpl::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


PRBool
XULDocumentImpl::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


PRBool
XULDocumentImpl::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    PRBool result = PR_TRUE;

    if (JSVAL_IS_STRING(aID) && 
        PL_strcmp("location", JS_GetStringBytes(JS_ValueToString(aContext, aID))) == 0) {
        if (nsnull != mScriptContextOwner) {
            nsIScriptGlobalObject *global;
            mScriptContextOwner->GetScriptGlobalObject(&global);
            if (nsnull != global) {
                nsIJSScriptObject *window;
                if (NS_OK == global->QueryInterface(kIJSScriptObjectIID, (void **)&window)) {
                    result = window->GetProperty(aContext, aID, aVp);
                    NS_RELEASE(window);
                }
                else {
                    result = PR_FALSE;
                }
                NS_RELEASE(global);
            }
        }
    }

    return result;
}


PRBool
XULDocumentImpl::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


PRBool
XULDocumentImpl::EnumerateProperty(JSContext *aContext)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


PRBool
XULDocumentImpl::Resolve(JSContext *aContext, jsval aID)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


PRBool
XULDocumentImpl::Convert(JSContext *aContext, jsval aID)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


void
XULDocumentImpl::Finalize(JSContext *aContext)
{
    NS_NOTYETIMPLEMENTED("write me");
}



////////////////////////////////////////////////////////////////////////
// nsIScriptObjectOwner interface
NS_IMETHODIMP
XULDocumentImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    nsresult res = NS_OK;
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();

    if (nsnull == mScriptObject) {
        res = NS_NewScriptDocument(aContext, (nsISupports *)(nsIDOMDocument *)this, global, (void**)&mScriptObject);
    }
    *aScriptObject = mScriptObject;

    NS_RELEASE(global);
    return res;
}


NS_IMETHODIMP
XULDocumentImpl::SetScriptObject(void *aScriptObject)
{
    mScriptObject = aScriptObject;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIHTMLContentContainer interface

NS_IMETHODIMP 
XULDocumentImpl::GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult)
{
    NS_PRECONDITION(nsnull != aResult, "null ptr");
    if (nsnull == aResult) {
        return NS_ERROR_NULL_POINTER;
    }
    *aResult = mAttrStyleSheet;
    if (nsnull == mAttrStyleSheet) {
        return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
    }
    else {
        NS_ADDREF(mAttrStyleSheet);
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult)
{
    NS_NOTYETIMPLEMENTED("get the inline stylesheet!");

    NS_PRECONDITION(nsnull != aResult, "null ptr");
    if (nsnull == aResult) {
        return NS_ERROR_NULL_POINTER;
    }
    *aResult = mInlineStyleSheet;
    if (nsnull == mInlineStyleSheet) {
        return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
    }
    else {
        NS_ADDREF(mInlineStyleSheet);
    }
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIDOMNodeObserver interface

NS_IMETHODIMP
XULDocumentImpl::OnSetNodeValue(nsIDOMNode* aNode, const nsString& aValue)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
XULDocumentImpl::OnInsertBefore(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aRefChild)
{
    for (PRInt32 i = 0; i < mBuilders->Count(); ++i) {
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        nsIDOMNodeObserver* obs;
        if (NS_SUCCEEDED(builder->QueryInterface(nsIDOMNodeObserver::IID(), (void**) &obs))) {
            obs->OnInsertBefore(aParent, aNewChild, aRefChild);
            NS_RELEASE(obs);
        }

        NS_RELEASE(builder);
    }
    return NS_OK;
}



NS_IMETHODIMP
XULDocumentImpl::OnReplaceChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aOldChild)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
XULDocumentImpl::OnRemoveChild(nsIDOMNode* aParent, nsIDOMNode* aOldChild)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
XULDocumentImpl::OnAppendChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}




////////////////////////////////////////////////////////////////////////
// Implementation methods

nsIContent*
XULDocumentImpl::FindContent(const nsIContent* aStartNode,
                             const nsIContent* aTest1, 
                             const nsIContent* aTest2) const
{
    PRInt32 count;
    aStartNode->ChildCount(count);

    PRInt32 index;
    for(index = 0; index < count;index++) {
        nsIContent* child;
        aStartNode->ChildAt(index, child);
        nsIContent* content = FindContent(child,aTest1,aTest2);
        if (content != nsnull) {
            NS_IF_RELEASE(child);
            return content;
        }
        if (child == aTest1 || child == aTest2) {
            NS_IF_RELEASE(content);
            return child;
        }
        NS_IF_RELEASE(child);
        NS_IF_RELEASE(content);
    }
    return nsnull;
}


nsresult
XULDocumentImpl::LoadCSSStyleSheet(nsIURL* url)
{
    nsresult rv;
    nsIInputStream* iin;
    rv = NS_OpenURL(url, &iin);
    if (NS_OK != rv) {
        NS_RELEASE(url);
        return rv;
    }

    nsIUnicharInputStream* uin = nsnull;
    rv = NS_NewConverterStream(&uin, nsnull, iin);
    NS_RELEASE(iin);
    if (NS_OK != rv) {
        NS_RELEASE(url);
        return rv;
    }
      
    nsICSSParser* parser;
    rv = nsRepository::CreateInstance(kCSSParserCID,
                                      nsnull,
                                      kICSSParserIID,
                                      (void**) &parser);

    if (NS_SUCCEEDED(rv)) {
        nsICSSStyleSheet* sheet = nsnull;
        // XXX note: we are ignoring rv until the error code stuff in the
        // input routines is converted to use nsresult's
        parser->SetCaseSensitive(PR_TRUE);
        parser->Parse(uin, url, sheet);
        if (nsnull != sheet) {
            AddStyleSheet(sheet);
            NS_RELEASE(sheet);
            rv = NS_OK;
        } else {
            rv = NS_ERROR_OUT_OF_MEMORY;/* XXX */
        }
        NS_RELEASE(parser);
    }

    NS_RELEASE(uin);
    NS_RELEASE(url);
    return rv;
}


nsresult
XULDocumentImpl::AddNamedDataSource(const char* uri)
{
    NS_PRECONDITION(mXULBuilder != nsnull, "not initialized");
    if (! mXULBuilder)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;
    nsCOMPtr<nsIRDFDataSource> ds;

    if (NS_FAILED(rv = mRDFService->GetDataSource(uri, getter_AddRefs(ds)))) {
        NS_ERROR("unable to get named datasource");
        return rv;
    }

    nsCOMPtr<nsIRDFCompositeDataSource> db;
    if (NS_FAILED(rv = mXULBuilder->GetDataBase(getter_AddRefs(db)))) {
        NS_ERROR("unable to get XUL db");
        return rv;
    }

    if (NS_FAILED(rv = db->AddDataSource(ds))) {
        NS_ERROR("unable to add named data source to XUL db");
        return rv;
    }

    return NS_OK;
}


nsresult
XULDocumentImpl::Init(void)
{
    nsresult rv;

    if (NS_FAILED(rv = NS_NewHeapArena(&mArena, nsnull)))
        return rv;

    // Create a namespace manager so we can manage tags
    if (NS_FAILED(rv = nsRepository::CreateInstance(kNameSpaceManagerCID,
                                                    nsnull,
                                                    kINameSpaceManagerIID,
                                                    (void**) &mNameSpaceManager)))
        return rv;

    // Keep the RDF service cached in a member variable to make using
    // it a bit less painful
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &mRDFService)))
        return rv;

    return NS_OK;
}



nsresult
XULDocumentImpl::StartLayout(void)
{
    PRInt32 count = GetNumberOfShells();
    for (PRInt32 i = 0; i < count; i++) {
        nsIPresShell* shell = GetShellAt(i);
        if (nsnull == shell)
            continue;

        // Resize-reflow this time
        nsCOMPtr<nsIPresContext> cx;
        shell->GetPresContext(getter_AddRefs(cx));
        nsRect r;
        cx->GetVisibleArea(r);
        shell->InitialReflow(r.width, r.height);

        // Now trigger a refresh
        nsCOMPtr<nsIViewManager> vm;
        shell->GetViewManager(getter_AddRefs(vm));
        if (vm) {
            vm->EnableRefresh();
        }

        // Start observing the document _after_ we do the initial
        // reflow. Otherwise, we'll get into an trouble trying to
        // creat kids before the root frame is established.
        shell->BeginObservingDocument();

        NS_RELEASE(shell);
    }
    return NS_OK;
}


nsresult
XULDocumentImpl::GetElementsByTagName(nsIDOMNode* aNode,
                                      const nsString& aTagName,
                                      nsRDFDOMNodeList* aElements)
{
    nsresult rv;

    if (aTagName.Equals("*")) {
        if (NS_FAILED(rv = aElements->AppendNode(aNode))) {
            NS_ERROR("unable to append element to node list");
            return rv;
        }
    }
    else {
        nsAutoString name;
        if (NS_FAILED(rv = aNode->GetNodeName(name))) {
            NS_ERROR("unable to get node name");
            return rv;
        }

        if (aTagName.Equals(name)) {
            if (NS_FAILED(rv = aElements->AppendNode(aNode))) {
                NS_ERROR("unable to append element to node list");
                return rv;
            }
        }
    }

    nsCOMPtr<nsIDOMNodeList> children;
    if (NS_FAILED(rv = aNode->GetChildNodes( getter_AddRefs(children) ))) {
        NS_ERROR("unable to get node's children");
        return rv;
    }

    // no kids: terminate the recursion
    if (! children)
        return NS_OK;

    PRUint32 length;
    if (NS_FAILED(children->GetLength(&length))) {
        NS_ERROR("unable to get node list's length");
        return rv;
    }

    for (PRUint32 i = 0; i < length; ++i) {
        nsCOMPtr<nsIDOMNode> child;
        if (NS_FAILED(rv = children->Item(i, getter_AddRefs(child) ))) {
            NS_ERROR("unable to get child from list");
            return rv;
        }

        if (NS_FAILED(rv = GetElementsByTagName(child, aTagName, aElements))) {
            NS_ERROR("unable to recursively get elements by tag name");
            return rv;
        }
    }

    return NS_OK;
}



