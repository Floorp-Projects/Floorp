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

// Note the ALPHABETICAL ORDERING
#include "nsCOMPtr.h"
#include "nsDOMCID.h"
#include "nsIArena.h"
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsIContent.h"
#include "nsIDOMElementObserver.h"
#include "nsIDOMNodeObserver.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDOMSelection.h"
#include "nsIDOMStyleSheetCollection.h"
#include "nsIDOMText.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDTD.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIJSScriptObject.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIParser.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIStyleContext.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsISupportsArray.h"
#include "nsITextContent.h"
#include "nsIURL.h"
#include "nsIURLGroup.h"
#include "nsIWebShell.h"
#include "nsIXMLContent.h"
#include "nsIXULChildDocument.h"
#include "nsIXULContentSink.h"
#include "nsIXULParentDocument.h"
#include "nsLayoutCID.h"
#include "nsParserCIID.h"
#include "nsRDFCID.h"
#include "nsRDFContentUtils.h"
#include "nsRDFDOMNodeList.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h" // XXX should go away
#include "plhash.h"
#include "plstr.h"
#include "prlog.h"
#include "rdfutil.h"
#include "rdf.h"

#include "nsILineBreakerFactory.h"
#include "nsIWordBreakerFactory.h"
#include "nsLWBrkCIID.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kICSSParserIID,              NS_ICSS_PARSER_IID); // XXX grr..
static NS_DEFINE_IID(kIContentIID,                NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDTDIID,                    NS_IDTD_IID);
static NS_DEFINE_IID(kIDOMScriptObjectFactoryIID, NS_IDOM_SCRIPT_OBJECT_FACTORY_IID);
static NS_DEFINE_IID(kIDocumentIID,               NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIHTMLContentContainerIID,   NS_IHTMLCONTENTCONTAINER_IID);
static NS_DEFINE_IID(kIHTMLStyleSheetIID,         NS_IHTML_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIHTMLCSSStyleSheetIID,      NS_IHTML_CSS_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID,         NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,       NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIParserIID,                 NS_IPARSER_IID);
static NS_DEFINE_IID(kIPresShellIID,              NS_IPRESSHELL_IID);
static NS_DEFINE_IID(kIRDFCompositeDataSourceIID, NS_IRDFCOMPOSITEDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFContentModelBuilderIID, NS_IRDFCONTENTMODELBUILDER_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,          NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFDocumentIID,            NS_IRDFDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,             NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFResourceIID,            NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,             NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID,      NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMSelectionIID,           NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kIStreamListenerIID,         NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIStreamObserverIID,         NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID,               NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellIID,               NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIXMLDocumentIID,            NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIXULContentSinkIID,         NS_IXULCONTENTSINK_IID);

static NS_DEFINE_CID(kCSSParserCID,              NS_CSSPARSER_CID);
static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_CID(kHTMLStyleSheetCID,         NS_HTMLSTYLESHEET_CID);
static NS_DEFINE_CID(kHTMLCSSStyleSheetCID,      NS_HTML_CSS_STYLESHEET_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,       NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kParserCID,                 NS_PARSER_IID); // XXX
static NS_DEFINE_CID(kPresShellCID,              NS_PRESSHELL_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,  NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kLocalStoreCID,             NS_LOCALSTORE_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,       NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFXULBuilderCID,          NS_RDFXULBUILDER_CID);
static NS_DEFINE_CID(kRangeListCID,              NS_RANGELIST_CID);
static NS_DEFINE_CID(kTextNodeCID,               NS_TEXTNODE_CID);
static NS_DEFINE_CID(kWellFormedDTDCID,          NS_WELLFORMEDDTD_CID);
static NS_DEFINE_CID(kXULContentSinkCID,         NS_XULCONTENTSINK_CID);
static NS_DEFINE_CID(kXULDataSourceCID,		     NS_XULDATASOURCE_CID);

static NS_DEFINE_IID(kLWBrkCID, NS_LWBRK_CID);
static NS_DEFINE_IID(kILineBreakerFactoryIID, NS_ILINEBREAKERFACTORY_IID);
static NS_DEFINE_IID(kIWordBreakerFactoryIID, NS_IWORDBREAKERFACTORY_IID);

////////////////////////////////////////////////////////////////////////
// Standard vocabulary items

DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, type);

#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
#define XUL_NAMESPACE_URI_PREFIX XUL_NAMESPACE_URI "#"
DEFINE_RDF_VOCAB(XUL_NAMESPACE_URI_PREFIX, XUL, element);

static PRLogModuleInfo* gMapLog;

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

        // XXX Don't comment out this assert: if you get here,
        // something has gone dreadfully, horribly
        // wrong. Curse. Scream. File a bug against
        // waterson@netscape.com.
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
                        public nsIDOMXULDocument,
                        public nsIJSScriptObject,
                        public nsIScriptObjectOwner,
                        public nsIHTMLContentContainer,
                        public nsIDOMNodeObserver,
                        public nsIDOMElementObserver,
                        public nsIXULParentDocument,
                        public nsIXULChildDocument
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

    NS_IMETHOD GetLineBreaker(nsILineBreaker** aResult) ;
    NS_IMETHOD SetLineBreaker(nsILineBreaker* aLineBreaker) ;
    NS_IMETHOD GetWordBreaker(nsIWordBreaker** aResult) ;
    NS_IMETHOD SetWordBreaker(nsIWordBreaker* aWordBreaker) ;

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

    NS_IMETHOD AppendToProlog(nsIContent* aContent);
    NS_IMETHOD AppendToEpilog(nsIContent* aContent);
    NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
    NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const;
    NS_IMETHOD GetChildCount(PRInt32& aCount);

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

    NS_IMETHOD ContentStateChanged(nsIContent* aContent);

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
    NS_IMETHOD GetContentById(const nsString& aName, nsIContent** aContent);

    // nsIRDFDocument interface
    NS_IMETHOD SetRootResource(nsIRDFResource* resource);
    NS_IMETHOD SplitProperty(nsIRDFResource* aResource,
                             PRInt32* aNameSpaceID,
                             nsIAtom** aTag);

    NS_IMETHOD AddElementForResource(nsIRDFResource* aResource, nsIContent* aElement);
    NS_IMETHOD RemoveElementForResource(nsIRDFResource* aResource, nsIContent* aElement);
    NS_IMETHOD GetElementsForResource(nsIRDFResource* aResource, nsISupportsArray* aElements);
    NS_IMETHOD CreateContents(nsIContent* aElement);
    NS_IMETHOD AddContentModelBuilder(nsIRDFContentModelBuilder* aBuilder);
    NS_IMETHOD GetDocumentDataSource(nsIRDFDataSource** aDatasource);

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

    // nsIDOMXULDocument interface
    NS_DECL_IDOMXULDOCUMENT
                   
    // nsIXULParentDocument interface
    NS_IMETHOD    GetContentViewerContainer(nsIContentViewerContainer** aContainer);
    NS_IMETHOD    GetCommand(nsString& aCommand);

    // nsIXULChildDocument Interface
    NS_IMETHOD    SetFragmentRoot(nsIRDFResource* aFragmentRoot);
    NS_IMETHOD    GetFragmentRoot(nsIRDFResource** aFragmentRoot);

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

    // nsIDOMElementObserver interface
    NS_DECL_IDOMELEMENTOBSERVER

    // Implementation methods
    nsresult Init(void);
    nsresult StartLayout(void);

    void SearchForNodeByID(const nsString& anID, nsIContent* anElement, nsIDOMElement** aReturn);

    static nsresult
    GetElementsByTagName(nsIDOMNode* aNode,
                         const nsString& aTagName,
                         nsRDFDOMNodeList* aElements);

    static nsresult
    GetElementsByAttribute(nsIDOMNode* aNode,
                           const nsString& aAttribute,
                           const nsString& aValue,
                           nsRDFDOMNodeList* aElements);

    nsresult
    ParseTagString(const nsString& aTagName, nsIAtom*& aName, PRInt32& aNameSpaceID);

    nsresult
    MakeProperty(PRInt32 aNameSpaceID, nsIAtom* aTag, nsIRDFResource** aResult);

protected:
    // pseudo constants
    static PRInt32 gRefCnt;
    static nsIAtom* kIdAtom;
    static nsIAtom* kObservesAtom;

    static nsIRDFService* gRDFService;
    static nsIRDFResource* kRDF_instanceOf;
    static nsIRDFResource* kRDF_type;
    static nsIRDFResource* kXUL_element;

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
    nsElementMap               mResources;
    nsISupportsArray*          mBuilders;
    nsIRDFContentModelBuilder* mXULBuilder;
    nsIRDFDataSource*          mLocalDataSource;
    nsIRDFDataSource*          mDocumentDataSource;
    nsILineBreaker*            mLineBreaker;
    nsIWordBreaker*            mWordBreaker;
    nsIContentViewerContainer* mContentViewerContainer;
    nsString                   mCommand;
    nsIRDFResource*            mFragmentRoot;
    nsVoidArray                mSubDocuments;
};

PRInt32 XULDocumentImpl::gRefCnt = 0;
nsIAtom* XULDocumentImpl::kIdAtom;
nsIAtom* XULDocumentImpl::kObservesAtom;

nsIRDFService* XULDocumentImpl::gRDFService;
nsIRDFResource* XULDocumentImpl::kRDF_instanceOf;
nsIRDFResource* XULDocumentImpl::kRDF_type;
nsIRDFResource* XULDocumentImpl::kXUL_element;

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
      mBuilders(nsnull),
      mXULBuilder(nsnull),
      mLocalDataSource(nsnull),
      mDocumentDataSource(nsnull),
      mLineBreaker(nsnull),
      mWordBreaker(nsnull),
      mContentViewerContainer(nsnull),
      mCommand(""),
      mFragmentRoot(nsnull)
{
    NS_INIT_REFCNT();

    nsresult rv;

    // construct a selection object
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRangeListCID,
                                                    nsnull,
                                                    kIDOMSelectionIID,
                                                    (void**) &mSelection))) {
        NS_ERROR("unable to create DOM selection");
    }

    if (gRefCnt++ == 0) {
        kIdAtom        = NS_NewAtom("id");
        kObservesAtom  = NS_NewAtom("observes");

        // Keep the RDF service cached in a member variable to make using
        // it a bit less painful
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          kIRDFServiceIID,
                                          (nsISupports**) &gRDFService);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF Service");

        if (gRDFService) {
            gRDFService->GetResource(kURIRDF_instanceOf, &kRDF_instanceOf);
            gRDFService->GetResource(kURIRDF_type,       &kRDF_type);
            gRDFService->GetResource(kURIXUL_element,    &kXUL_element);
        }
    }

#ifdef PR_LOGGING
    if (! gMapLog)
        gMapLog = PR_NewLogModule("nsXULDocumentElementMap");
#endif
}

XULDocumentImpl::~XULDocumentImpl()
{
    NS_IF_RELEASE(mDocumentDataSource);
    NS_IF_RELEASE(mLocalDataSource);

    // mParentDocument is never refcounted
    // Delete references to sub-documents
    PRInt32 index = mSubDocuments.Count();
    while (--index >= 0) {
      nsIDocument* subdoc = (nsIDocument*) mSubDocuments.ElementAt(index);
      NS_RELEASE(subdoc);
    }

    // set all builder references to document to nsnull
    if (mBuilders)
    {

#ifdef	DEBUG
	printf("# of builders: %lu\n", (unsigned long)mBuilders->Count());
#endif

	    for (PRUint32 i = 0; i < mBuilders->Count(); ++i)
	    {
		// XXX we should QueryInterface() here
		nsIRDFContentModelBuilder* builder
		    = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

		NS_ASSERTION(builder != nsnull, "null ptr");
		if (! builder)
		    continue;

		nsresult rv = builder->SetDocument(nsnull);
		NS_ASSERTION(NS_SUCCEEDED(rv), "error creating content");
		// XXX ignore error code?

		NS_RELEASE(builder);
	    }
    }

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
    NS_IF_RELEASE(mLineBreaker);
    NS_IF_RELEASE(mWordBreaker);
    NS_IF_RELEASE(mContentViewerContainer);
    NS_IF_RELEASE(mFragmentRoot);
    
    if (--gRefCnt == 0) {
        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kObservesAtom);

        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }

        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kRDF_type);
        NS_IF_RELEASE(kXUL_element);
    }
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
    }
    else if (iid.Equals(nsIXULParentDocument::GetIID())) {
        *result = NS_STATIC_CAST(nsIXULParentDocument*, this);
    }
    else if (iid.Equals(nsIXULChildDocument::GetIID())) {
        *result = NS_STATIC_CAST(nsIXULChildDocument*, this);
    }
    else if (iid.Equals(kIRDFDocumentIID) ||
             iid.Equals(kIXMLDocumentIID)) {
        *result = NS_STATIC_CAST(nsIRDFDocument*, this);
    }
    else if (iid.Equals(nsIDOMXULDocument::GetIID()) ||
             iid.Equals(nsIDOMDocument::GetIID()) ||
             iid.Equals(nsIDOMNode::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMXULDocument*, this);
    }
    else if (iid.Equals(kIJSScriptObjectIID)) {
        *result = NS_STATIC_CAST(nsIJSScriptObject*, this);
    }
    else if (iid.Equals(kIScriptObjectOwnerIID)) {
        *result = NS_STATIC_CAST(nsIScriptObjectOwner*, this);
    }
    else if (iid.Equals(kIHTMLContentContainerIID)) {
        *result = NS_STATIC_CAST(nsIHTMLContentContainer*, this);
    }
    else if (iid.Equals(nsIDOMNodeObserver::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMNodeObserver*, this);
    }
    else if (iid.Equals(nsIDOMElementObserver::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMElementObserver*, this);
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }
    NS_ADDREF(this);
    return NS_OK;
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

    if (aContainer && aContainer != mContentViewerContainer)
    {
      // AddRef and hold the container
      NS_ADDREF(aContainer);
      mContentViewerContainer = aContainer;
    }

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
    if (NS_SUCCEEDED(rv = nsComponentManager::CreateInstance(kHTMLStyleSheetCID,
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

    // Create an inline style sheet for inline content that contains a style 
    // attribute.
    nsIHTMLCSSStyleSheet* inlineSheet;
    if (NS_SUCCEEDED(rv = nsComponentManager::CreateInstance(kHTMLCSSStyleSheetCID,
                                                       nsnull,
                                                       kIHTMLCSSStyleSheetIID,
                                                       (void**) &inlineSheet))) {
        if (NS_SUCCEEDED(rv = inlineSheet->Init(aURL, this))) {
            mInlineStyleSheet = inlineSheet;
            NS_ADDREF(mInlineStyleSheet);

            AddStyleSheet(mInlineStyleSheet);
        }
        NS_RELEASE(inlineSheet);
    }

    if (NS_FAILED(rv)) {
        NS_ERROR("unable to add inline style sheet");
        return rv;
    }
      
    // Create the composite data source and builder, but only do this if we're
    // not a XUL fragment.
    if (mFragmentRoot == nsnull) {

        nsCOMPtr<nsIRDFCompositeDataSource> db;
        rv = nsComponentManager::CreateInstance(kRDFCompositeDataSourceCID,
                                                nsnull,
                                                kIRDFCompositeDataSourceIID,
                                                (void**) getter_AddRefs(db));

        if (NS_FAILED(rv)) {
            NS_ERROR("couldn't create composite datasource");
            return rv;
        }

        // Create a XUL content model builder
        NS_IF_RELEASE(mXULBuilder);
        rv = nsComponentManager::CreateInstance(kRDFXULBuilderCID,
                                                nsnull,
                                                kIRDFContentModelBuilderIID,
                                                (void**) &mXULBuilder);

        if (NS_FAILED(rv)) {
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
        rv = gRDFService->GetDataSource("rdf:local-store", &mLocalDataSource);

        if (NS_FAILED(rv)) {
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
        if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
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
    }

    nsCOMPtr<nsIXULContentSink> sink;
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kXULContentSinkCID,
                                                    nsnull,
                                                    kIXULContentSinkIID,
                                                    (void**) getter_AddRefs(sink)))) {
        NS_ERROR("unable to create XUL content sink");
        return rv;
    }

    {
        if (NS_FAILED(rv = sink->Init(this, mDocumentDataSource))) {
            NS_ERROR("Unable to initialize XUL content sink");
            return rv;
        }
    }

    nsCOMPtr<nsIParser> parser;
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kParserCID,
                                                    nsnull,
                                                    kIParserIID,
                                                    (void**) getter_AddRefs(parser)))) {
        NS_ERROR("unable to create parser");
        return rv;
    }

    if (NS_FAILED(rv = parser->QueryInterface(kIStreamListenerIID, (void**) aDocListener))) {
        NS_ERROR("parser doesn't support nsIStreamListener");
        return rv;
    }

    nsCOMPtr<nsIDTD> dtd;
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kWellFormedDTDCID,
                                                    nsnull,
                                                    kIDTDIID,
                                                    (void**) getter_AddRefs(dtd)))) {
        NS_ERROR("unable to construct DTD");
        return rv;
    }

    mCommand = aCommand;

    parser->RegisterDTD(dtd);
    parser->SetCommand(aCommand);
    parser->SetContentSink(sink); // grabs a reference to the parser
    parser->Parse(aURL);
   
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
XULDocumentImpl::GetLineBreaker(nsILineBreaker** aResult) 
{
  if(nsnull == mLineBreaker ) {
     // no line breaker, find a default one
     nsILineBreakerFactory *lf;
     nsresult result;
     result = nsServiceManager::GetService(kLWBrkCID,
                                          kILineBreakerFactoryIID,
                                          (nsISupports **)&lf);
     if (NS_SUCCEEDED(result)) {
      nsILineBreaker *lb = nsnull ;
      nsAutoString lbarg("");
      result = lf->GetBreaker(lbarg, &lb);
      if(NS_SUCCEEDED(result)) {
         mLineBreaker = lb;
      }
      result = nsServiceManager::ReleaseService(kLWBrkCID, lf);
     }
  }
  *aResult = mLineBreaker;
  NS_IF_ADDREF(mLineBreaker);
  return NS_OK; // XXX we should do error handling here
}

NS_IMETHODIMP 
XULDocumentImpl::SetLineBreaker(nsILineBreaker* aLineBreaker) 
{
  NS_IF_RELEASE(mLineBreaker);
  mLineBreaker = aLineBreaker;
  NS_IF_ADDREF(mLineBreaker);
  return NS_OK;
}
NS_IMETHODIMP 
XULDocumentImpl::GetWordBreaker(nsIWordBreaker** aResult) 
{
  if(nsnull == mWordBreaker ) {
     // no line breaker, find a default one
     nsIWordBreakerFactory *lf;
     nsresult result;
     result = nsServiceManager::GetService(kLWBrkCID,
                                          kIWordBreakerFactoryIID,
                                          (nsISupports **)&lf);
     if (NS_SUCCEEDED(result)) {
      nsIWordBreaker *lb = nsnull ;
      nsAutoString lbarg("");
      result = lf->GetBreaker(lbarg, &lb);
      if(NS_SUCCEEDED(result)) {
         mWordBreaker = lb;
      }
      result = nsServiceManager::ReleaseService(kLWBrkCID, lf);
     }
  }
  *aResult = mWordBreaker;
  NS_IF_ADDREF(mWordBreaker);
  return NS_OK; // XXX we should do error handling here
}

NS_IMETHODIMP 
XULDocumentImpl::SetWordBreaker(nsIWordBreaker* aWordBreaker) 
{
  NS_IF_RELEASE(mWordBreaker);
  mWordBreaker = aWordBreaker;
  NS_IF_ADDREF(mWordBreaker);
  return NS_OK;
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
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kPresShellCID,
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
    NS_ADDREF(aSubDoc);
    mSubDocuments.AppendElement(aSubDoc);
}

PRInt32 
XULDocumentImpl::GetNumberOfSubDocuments()
{
    return mSubDocuments.Count();
}

nsIDocument* 
XULDocumentImpl::GetSubDocumentAt(PRInt32 aIndex)
{
    nsIDocument* doc = (nsIDocument*) mSubDocuments.ElementAt(aIndex);
    if (nsnull != doc) {
        NS_ADDREF(doc);
    }
    return doc;
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

NS_IMETHODIMP 
XULDocumentImpl::AppendToProlog(nsIContent* aContent)
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

NS_IMETHODIMP 
XULDocumentImpl::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
XULDocumentImpl::IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
XULDocumentImpl::GetChildCount(PRInt32& aCount)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
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
XULDocumentImpl::ContentStateChanged(nsIContent* aContent)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentStateChanged(this, aContent);
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
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// nsIXMLDocument interface
NS_IMETHODIMP 
XULDocumentImpl::GetContentById(const nsString& aName, nsIContent** aContent)
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

    nsXPIDLCString p;
    aProperty->GetValue( getter_Copies(p) );
    nsAutoString uri(p);

    PRInt32 index;

    // First try to split the namespace using the rightmost '#' or '/'
    // character.
    if ((index = uri.RFind('#')) < 0) {
        if ((index = uri.RFind('/')) < 0) {
            *aNameSpaceID = kNameSpaceID_None;
            *aTag = NS_NewAtom(uri);
            return NS_OK;
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

    // allright, if we at least have a tag, then we can return the
    // thing with kNameSpaceID_None for now.
    if (tag.Length()) {
        *aNameSpaceID = kNameSpaceID_None;
        *aTag = NS_NewAtom(tag);
        return NS_OK;
    }

    // Okay, somebody make this code even more convoluted.
    NS_ERROR("unable to convert URI to namespace/tag pair");
    return NS_ERROR_FAILURE; // XXX?
}


NS_IMETHODIMP
XULDocumentImpl::AddElementForResource(nsIRDFResource* aResource, nsIContent* aElement)
{
    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gMapLog, PR_LOG_ALWAYS)) {
        nsXPIDLCString uri;
        aResource->GetValue( getter_Copies(uri) );
        PR_LOG(gMapLog, PR_LOG_ALWAYS,
               ("xulelemap(%p) add    [%p] <-- %s\n",
                this, aElement, (const char*) uri));
    }
#endif

    mResources.Add(aResource, aElement);
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::RemoveElementForResource(nsIRDFResource* aResource, nsIContent* aElement)
{
    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gMapLog, PR_LOG_ALWAYS)) {
        nsXPIDLCString uri;
        aResource->GetValue( getter_Copies(uri) );
        PR_LOG(gMapLog, PR_LOG_ALWAYS,
               ("xulelement(%p) remove [%p] <-- %s\n",
                this, aElement, (const char*) uri));
    }
#endif

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
XULDocumentImpl::CreateContents(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    if (! mBuilders)
        return NS_ERROR_NOT_INITIALIZED;

    for (PRUint32 i = 0; i < mBuilders->Count(); ++i) {
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

    // Now that the contents have been created, perform broadcaster
    // hookups if any of the children are observes nodes.
    // XXX: Initial sync-up doesn't work, since no document observer exists
    // yet.
    PRInt32 childCount;
    aElement->ChildCount(childCount);
    for (PRInt32 j = 0; j < childCount; j++)
    {
        nsIContent* childContent = nsnull;
        aElement->ChildAt(j, childContent);
      
        if (!childContent)
          break;

        nsIAtom* tag = nsnull;
        childContent->GetTag(tag);

        if (!tag)
          break;

        if (tag == kObservesAtom)
        {
            // Find the node that we're supposed to be
            // observing and perform the hookup.
            nsString elementValue;
            nsString attributeValue;
            
            nsCOMPtr<nsIDOMElement> domContent;
            domContent = do_QueryInterface(childContent);

            domContent->GetAttribute("element",
                                     elementValue);
            
            domContent->GetAttribute("attribute",
                                     attributeValue);

            nsIDOMElement* domElement = nsnull;
            GetElementById(elementValue, &domElement);
            
            if (!domElement)
              break;

            // We have a DOM element to bind to.  Add a broadcast
            // listener to that element, but only if it's a XUL element.
            // XXX: Handle context nodes.
            nsCOMPtr<nsIDOMElement> listener( do_QueryInterface(aElement) );
            nsCOMPtr<nsIDOMXULElement> broadcaster( do_QueryInterface(domElement) );
            if (listener)
            {
                broadcaster->AddBroadcastListener(attributeValue,
                                                  listener);
            }

            NS_RELEASE(domElement);
        }

        NS_RELEASE(childContent);
        NS_RELEASE(tag);
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

NS_IMETHODIMP
XULDocumentImpl::GetDocumentDataSource(nsIRDFDataSource** aDataSource) {
    if (mDocumentDataSource) {
        NS_ADDREF(mDocumentDataSource);
        *aDataSource = mDocumentDataSource;
    }
    return NS_OK;
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
  if (nsnull == aDocumentElement) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult res = NS_ERROR_FAILURE;

  if (nsnull != mRootContent) {
    res = mRootContent->QueryInterface(nsIDOMElement::GetIID(), (void**)aDocumentElement);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Element");
  }
  
  return res;
}



NS_IMETHODIMP
XULDocumentImpl::CreateElement(const nsString& aTagName, nsIDOMElement** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    // we need this so that we can create a resource URI
    NS_PRECONDITION(mDocumentURL != nsnull, "not initialized");
    if (! mDocumentURL)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    nsCOMPtr<nsIAtom> name;
    PRInt32 nameSpaceID;

    // parse the user-provided string into a tag name and a namespace ID
    rv = ParseTagString(aTagName, *getter_AddRefs(name), nameSpaceID);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to parse tag name");
    if (NS_FAILED(rv)) return rv;

    // construct an element
    nsCOMPtr<nsIContent> result;
    rv = NS_NewRDFElement(nameSpaceID, name, getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;

    // assign it an "anonymous" identifier so that it can be referred
    // to in the graph.
    nsCOMPtr<nsIRDFResource> resource;
    const char* context;
    rv = mDocumentURL->GetSpec(&context);
    if (NS_FAILED(rv)) return rv;

    rv = rdf_CreateAnonymousResource(context, getter_AddRefs(resource));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString uri;
    rv = resource->GetValue(getter_Copies(uri));
    if (NS_FAILED(rv)) return rv;

    rv = result->SetAttribute(kNameSpaceID_None, kIdAtom, (const char*) uri, PR_FALSE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set element's ID");
    if (NS_FAILED(rv)) return rv;

    // Set it's RDF:type in the graph s.t. the element's tag can be
    // constructed from it.
    nsCOMPtr<nsIRDFResource> type;
    rv = MakeProperty(nameSpaceID, name, getter_AddRefs(type));
    if (NS_FAILED(rv)) return rv;

    rv = mDocumentDataSource->Assert(resource, kRDF_type, type, PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set element's tag info in graph");
    if (NS_FAILED(rv)) return rv;

    // Mark it as a XUL element
    rv = mDocumentDataSource->Assert(resource, kRDF_instanceOf, kXUL_element, PR_TRUE);
    NS_ASSERTION(rv == NS_OK, "unable to mark as XUL element");
    if (NS_FAILED(rv)) return rv;

    rv = rdf_MakeSeq(mDocumentDataSource, resource);
    NS_ASSERTION(rv == NS_OK, "unable to mark as XUL element");
    if (NS_FAILED(rv)) return rv;

    // `this' will be its document
    rv = result->SetDocument(this, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    // get the DOM interface
    rv = result->QueryInterface(nsIDOMElement::GetIID(), (void**) aReturn);
    NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM element");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
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
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsITextContent> text;
    rv = nsComponentManager::CreateInstance(kTextNodeCID, nsnull, nsITextContent::GetIID(), getter_AddRefs(text));
    if (NS_FAILED(rv)) return rv;

    rv = text->SetText(aData.GetUnicode(), aData.Length(), PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    rv = text->QueryInterface(nsIDOMText::GetIID(), (void**) aReturn);
    NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOMText");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
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
        if (NS_SUCCEEDED(rv = root->QueryInterface(nsIDOMNode::GetIID(), (void**) &domRoot))) {
            rv = GetElementsByTagName(domRoot, aTagName, elements);
            NS_RELEASE(domRoot);
        }
        NS_RELEASE(root);
    }

    *aReturn = elements;
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::GetElementsByAttribute(const nsString& aAttribute, const nsString& aValue, 
                                        nsIDOMNodeList** aReturn)
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
        if (NS_SUCCEEDED(rv = root->QueryInterface(nsIDOMNode::GetIID(), (void**) &domRoot))) {
            rv = GetElementsByAttribute(domRoot, aAttribute, aValue, elements);
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
// nsIDOMXULDocument interface

NS_IMETHODIMP
XULDocumentImpl::GetRdf(nsIRDFService** aRDFService)
{
    // XXX this is a temporary hack until the component manager starts
    // to work.
    return nsServiceManager::GetService(kRDFServiceCID,
                                        nsIRDFService::GetIID(),
                                        (nsISupports**) aRDFService);
}

NS_IMETHODIMP
XULDocumentImpl::GetElementById(const nsString& aId, nsIDOMElement** aReturn)
{
    NS_PRECONDITION(mRootContent != nsnull, "document contains no content");
    if (! mRootContent)
        return NS_ERROR_NOT_INITIALIZED; // XXX right error code?

    nsresult rv;

    nsAutoString uri(aId);
    const char* documentURL;
    mDocumentURL->GetSpec(&documentURL);

    rdf_PossiblyMakeAbsolute(documentURL, uri);

    nsCOMPtr<nsIRDFResource> resource;
    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(uri, getter_AddRefs(resource)))) {
        NS_ERROR("unable to get resource");
        return rv;
    }

    nsCOMPtr<nsISupportsArray> elements;
    if (NS_FAILED(rv = NS_NewISupportsArray(getter_AddRefs(elements)))) {
        NS_ERROR("unable to create new ISupportsArray");
        return rv;
    }

    if (NS_FAILED(rv = GetElementsForResource(resource, elements))) {
        NS_ERROR("unable to get elements for resource");
        return rv;
    }

    if (elements->Count() > 0) {
        if (elements->Count() > 1) {
            // This is a scary case that our API doesn't deal with well.
            NS_WARNING("more than one element found with specified ID; returning first");
        }

        nsISupports* element = elements->ElementAt(0);
        rv = element->QueryInterface(nsIDOMElement::GetIID(), (void**) aReturn);
        NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM element");
        NS_RELEASE(element);
        return rv;
    }

    // Didn't find it in our hash table. Walk the tree looking for the
    // node. This happens for elements that aren't nsRDFElement
    // objects (e.g., HTML content).

    *aReturn = nsnull;
    SearchForNodeByID(aId, mRootContent, aReturn);

    return NS_OK;
}

void
XULDocumentImpl::SearchForNodeByID(const nsString& anID, 
                                   nsIContent* anElement,
                                   nsIDOMElement** aReturn)
{
    // See if we match.
    PRInt32 namespaceID;
    anElement->GetNameSpaceID(namespaceID);
    
    nsString idValue;

    anElement->GetAttribute(namespaceID, kIdAtom, idValue);

    if (idValue == anID)
    {
      nsCOMPtr<nsIDOMElement> pDomNode( do_QueryInterface(anElement) );
      if (pDomNode)
      {
        *aReturn = pDomNode;
        NS_ADDREF(*aReturn);
      }

      return;
    }

    // Walk children.
    // XXX: Don't descend into closed tree items (or menu items or buttons?).
    PRInt32 childCount;
    anElement->ChildCount(childCount);
    for (PRInt32 i = 0; i < childCount && !(*aReturn); i++)
    {
        nsIContent* pContent = nsnull;
        anElement->ChildAt(i, pContent);
        if (pContent)
        {
            SearchForNodeByID(anID, pContent, aReturn);
            NS_RELEASE(pContent);
        }
    }
}

////////////////////////////////////////////////////////////////////////
// nsIXULParentDocument interface
NS_IMETHODIMP 
XULDocumentImpl::GetContentViewerContainer(nsIContentViewerContainer** aContainer)
{
    if (mContentViewerContainer != nsnull)
    {
        NS_ADDREF(mContentViewerContainer);
        *aContainer = mContentViewerContainer;
    }

    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetCommand(nsString& aCommand)
{
    aCommand = mCommand;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIXULChildDocument interface
NS_IMETHODIMP 
XULDocumentImpl::SetFragmentRoot(nsIRDFResource* aFragmentRoot)
{
    if (aFragmentRoot != mFragmentRoot)
    {
        NS_IF_RELEASE(mFragmentRoot);
        NS_IF_ADDREF(aFragmentRoot);
        mFragmentRoot = aFragmentRoot;
    }
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::GetFragmentRoot(nsIRDFResource** aFragmentRoot)
{
    if (mFragmentRoot) {
        NS_ADDREF(mFragmentRoot);
        *aFragmentRoot = mFragmentRoot;
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIDOMNode interface
NS_IMETHODIMP
XULDocumentImpl::GetNodeName(nsString& aNodeName)
{
    aNodeName.SetString("#document");
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetNodeValue(nsString& aNodeValue)
{
    aNodeValue.Truncate();
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::SetNodeValue(const nsString& aNodeValue)
{
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetNodeType(PRUint16* aNodeType)
{
    *aNodeType = nsIDOMNode::DOCUMENT_NODE;
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetParentNode(nsIDOMNode** aParentNode)
{
    *aParentNode = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    NS_PRECONDITION(aChildNodes != nsnull, "null ptr");
    if (! aChildNodes)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        nsresult rv;

        *aChildNodes = nsnull;

        nsRDFDOMNodeList* children;
        rv = nsRDFDOMNodeList::Create(&children);

        if (NS_SUCCEEDED(rv)) {
            nsIDOMNode* domNode;
            rv = mRootContent->QueryInterface(nsIDOMNode::GetIID(), (void**) domNode);
            NS_ASSERTION(NS_SUCCEEDED(rv), "root content is not a DOM node");

            if (NS_SUCCEEDED(rv)) {
                rv = children->AppendNode(domNode);
                NS_RELEASE(domNode);

                *aChildNodes = children;
                return NS_OK;
            }
        }

        // If we get here, something bad happened.
        NS_RELEASE(children);
        return rv;
    }
    else {
        *aChildNodes = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
XULDocumentImpl::HasChildNodes(PRBool* aHasChildNodes)
{
    NS_PRECONDITION(aHasChildNodes != nsnull, "null ptr");
    if (! aHasChildNodes)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        *aHasChildNodes = PR_TRUE;
    }
    else {
        *aHasChildNodes = PR_FALSE;
    }
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetFirstChild(nsIDOMNode** aFirstChild)
{
    NS_PRECONDITION(aFirstChild != nsnull, "null ptr");
    if (! aFirstChild)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        return mRootContent->QueryInterface(nsIDOMNode::GetIID(), (void**) aFirstChild);
    }
    else {
        *aFirstChild = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
XULDocumentImpl::GetLastChild(nsIDOMNode** aLastChild)
{
    NS_PRECONDITION(aLastChild != nsnull, "null ptr");
    if (! aLastChild)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        return mRootContent->QueryInterface(nsIDOMNode::GetIID(), (void**) aLastChild);
    }
    else {
        *aLastChild = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
XULDocumentImpl::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    NS_PRECONDITION(aPreviousSibling != nsnull, "null ptr");
    if (! aPreviousSibling)
        return NS_ERROR_NULL_POINTER;

    *aPreviousSibling = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetNextSibling(nsIDOMNode** aNextSibling)
{
    NS_PRECONDITION(aNextSibling != nsnull, "null ptr");
    if (! aNextSibling)
        return NS_ERROR_NULL_POINTER;

    *aNextSibling = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
    NS_PRECONDITION(aAttributes != nsnull, "null ptr");
    if (! aAttributes)
        return NS_ERROR_NULL_POINTER;

    *aAttributes = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    NS_PRECONDITION(aOwnerDocument != nsnull, "null ptr");
    if (! aOwnerDocument)
        return NS_ERROR_NULL_POINTER;

    *aOwnerDocument = nsnull;
    return NS_OK;
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
    // We don't allow cloning of a document
    *aReturn = nsnull;
    return NS_OK;
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
        res = NS_NewScriptXULDocument(aContext, (nsISupports *)(nsIDOMXULDocument *)this, global, (void**)&mScriptObject);

#if defined(XPIDL_JS_STUBS)
        JSContext* cx = (JSContext*) aContext->GetNativeContext();
        nsIRDFNode::InitJSClass(cx);
        nsIRDFResource::InitJSClass(cx);
        nsIRDFLiteral::InitJSClass(cx);
        nsIRDFDate::InitJSClass(cx);
        nsIRDFInt::InitJSClass(cx);
        nsIRDFCursor::InitJSClass(cx);
        nsIRDFAssertionCursor::InitJSClass(cx);
        nsIRDFArcsInCursor::InitJSClass(cx);
        nsIRDFArcsOutCursor::InitJSClass(cx);
        nsIRDFResourceCursor::InitJSClass(cx);
        nsIRDFObserver::InitJSClass(cx);
        nsIRDFDataSource::InitJSClass(cx);
        nsIRDFCompositeDataSource::InitJSClass(cx);
        nsIRDFService::InitJSClass(cx);
#endif

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
    for (PRUint32 i = 0; i < mBuilders->Count(); ++i) {
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        nsIDOMNodeObserver* obs;
        if (NS_SUCCEEDED(builder->QueryInterface(nsIDOMNodeObserver::GetIID(), (void**) &obs))) {
            obs->OnSetNodeValue(aNode, aValue);
            NS_RELEASE(obs);
        }

        NS_RELEASE(builder);
    }
    return NS_OK;
}



NS_IMETHODIMP
XULDocumentImpl::OnInsertBefore(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aRefChild)
{
    for (PRUint32 i = 0; i < mBuilders->Count(); ++i) {
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        nsIDOMNodeObserver* obs;
        if (NS_SUCCEEDED(builder->QueryInterface(nsIDOMNodeObserver::GetIID(), (void**) &obs))) {
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
    for (PRUint32 i = 0; i < mBuilders->Count(); ++i) {
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        nsIDOMNodeObserver* obs;
        if (NS_SUCCEEDED(builder->QueryInterface(nsIDOMNodeObserver::GetIID(), (void**) &obs))) {
            obs->OnReplaceChild(aParent, aNewChild, aOldChild);
            NS_RELEASE(obs);
        }

        NS_RELEASE(builder);
    }
    return NS_OK;
}



NS_IMETHODIMP
XULDocumentImpl::OnRemoveChild(nsIDOMNode* aParent, nsIDOMNode* aOldChild)
{
    for (PRUint32 i = 0; i < mBuilders->Count(); ++i) {
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        nsIDOMNodeObserver* obs;
        if (NS_SUCCEEDED(builder->QueryInterface(nsIDOMNodeObserver::GetIID(), (void**) &obs))) {
            obs->OnRemoveChild(aParent, aOldChild);
            NS_RELEASE(obs);
        }

        NS_RELEASE(builder);
    }
    return NS_OK;
}



NS_IMETHODIMP
XULDocumentImpl::OnAppendChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild)
{
    for (PRUint32 i = 0; i < mBuilders->Count(); ++i) {
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        nsIDOMNodeObserver* obs;
        if (NS_SUCCEEDED(builder->QueryInterface(nsIDOMNodeObserver::GetIID(), (void**) &obs))) {
            obs->OnAppendChild(aParent, aNewChild);
            NS_RELEASE(obs);
        }

        NS_RELEASE(builder);
    }
    return NS_OK;
}



////////////////////////////////////////////////////////////////////////
// nsIDOMElementObserver interface

NS_IMETHODIMP
XULDocumentImpl::OnSetAttribute(nsIDOMElement* aElement, const nsString& aName, const nsString& aValue)
{
    for (PRUint32 i = 0; i < mBuilders->Count(); ++i) {
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        nsIDOMElementObserver* obs;
        if (NS_SUCCEEDED(builder->QueryInterface(nsIDOMElementObserver::GetIID(), (void**) &obs))) {
            obs->OnSetAttribute(aElement, aName, aValue);
            NS_RELEASE(obs);
        }

        NS_RELEASE(builder);
    }
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::OnRemoveAttribute(nsIDOMElement* aElement, const nsString& aName)
{
    for (PRUint32 i = 0; i < mBuilders->Count(); ++i) {
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        nsIDOMElementObserver* obs;
        if (NS_SUCCEEDED(builder->QueryInterface(nsIDOMElementObserver::GetIID(), (void**) &obs))) {
            obs->OnRemoveAttribute(aElement, aName);
            NS_RELEASE(obs);
        }

        NS_RELEASE(builder);
    }
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::OnSetAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aNewAttr)
{
    for (PRUint32 i = 0; i < mBuilders->Count(); ++i) {
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        nsIDOMElementObserver* obs;
        if (NS_SUCCEEDED(builder->QueryInterface(nsIDOMElementObserver::GetIID(), (void**) &obs))) {
            obs->OnSetAttributeNode(aElement, aNewAttr);
            NS_RELEASE(obs);
        }

        NS_RELEASE(builder);
    }
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::OnRemoveAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aOldAttr)
{
    for (PRUint32 i = 0; i < mBuilders->Count(); ++i) {
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        nsIDOMElementObserver* obs;
        if (NS_SUCCEEDED(builder->QueryInterface(nsIDOMElementObserver::GetIID(), (void**) &obs))) {
            obs->OnRemoveAttributeNode(aElement, aOldAttr);
            NS_RELEASE(obs);
        }

        NS_RELEASE(builder);
    }
    return NS_OK;
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
    rv = nsComponentManager::CreateInstance(kCSSParserCID,
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

    if (NS_FAILED(rv = gRDFService->GetDataSource(uri, getter_AddRefs(ds)))) {
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
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                    nsnull,
                                                    kINameSpaceManagerIID,
                                                    (void**) &mNameSpaceManager)))
        return rv;

    return NS_OK;
}



nsresult
XULDocumentImpl::StartLayout(void)
{
    if (mFragmentRoot)
      return NS_OK; // Subdocuments rely on the parent document for layout

    PRInt32 count = GetNumberOfShells();
    for (PRInt32 i = 0; i < count; i++) {
        nsIPresShell* shell = GetShellAt(i);
        if (nsnull == shell)
            continue;

        // Resize-reflow this time
        nsCOMPtr<nsIPresContext> cx;
        shell->GetPresContext(getter_AddRefs(cx));

		if (cx) {
			nsCOMPtr<nsISupports> container;
			cx->GetContainer(getter_AddRefs(container));
			if (container) {
			  nsCOMPtr<nsIWebShell> webShell;
			  webShell = do_QueryInterface(container);
			  if (webShell) {
				nsCOMPtr<nsIStyleContext> styleContext;
				if (NS_SUCCEEDED(cx->ResolveStyleContextFor(mRootContent, nsnull,
										   PR_FALSE,
										   getter_AddRefs(styleContext)))) {

					const nsStyleDisplay* disp = (const nsStyleDisplay*)
						styleContext->GetStyleData(eStyleStruct_Display);

					webShell->SetScrolling(disp->mOverflow);
				}
			  }
			}
		}
        
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
        // create kids before the root frame is established.
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

    nsCOMPtr<nsIDOMElement> element;
    element = do_QueryInterface(aNode);
    if (!element)
      return NS_OK;

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

nsresult
XULDocumentImpl::GetElementsByAttribute(nsIDOMNode* aNode,
                                        const nsString& aAttribute,
                                        const nsString& aValue,
                                        nsRDFDOMNodeList* aElements)
{
    nsresult rv;

    nsCOMPtr<nsIDOMElement> element;
    element = do_QueryInterface(aNode);
    if (!element)
        return NS_OK;

    nsAutoString attrValue;
    if (NS_FAILED(rv = element->GetAttribute(aAttribute, attrValue))) {
        NS_ERROR("unable to get attribute value");
        return rv;
    }

    if ((attrValue == aValue) || (attrValue.Length() > 0 && aValue == "*")) {
        if (NS_FAILED(rv = aElements->AppendNode(aNode))) {
            NS_ERROR("unable to append element to node list");
            return rv;
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

        if (NS_FAILED(rv = GetElementsByAttribute(child, aAttribute, aValue, aElements))) {
            NS_ERROR("unable to recursively get elements by attribute");
            return rv;
        }
    }

    return NS_OK;
}



nsresult
XULDocumentImpl::ParseTagString(const nsString& aTagName, nsIAtom*& aName, PRInt32& aNameSpaceID)
{
    // Parse the tag into a name and a namespace ID. This is slightly
    // different than nsIContent::ParseAttributeString() because we
    // take the default namespace into account (rather than just
    // assigning "no namespace") in the case that there is no
    // namespace prefix present.

static char kNameSpaceSeparator[] = ":";

    // XXX this is a gross hack, but it'll carry us for now. We parse
    // the tag name using the root content, which presumably has all
    // the namespace info we need.
    NS_PRECONDITION(mRootContent != nsnull, "not initialized");
    if (! mRootContent)
        return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(mRootContent) );
    if (! xml) return NS_ERROR_UNEXPECTED;
    
    nsresult rv;
    nsCOMPtr<nsINameSpace> ns;
    rv = xml->GetContainingNameSpace(*getter_AddRefs(ns));
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(ns != nsnull, "expected xml namespace info to be available");
    if (! ns)
        return NS_ERROR_UNEXPECTED;

    nsAutoString prefix;
    nsAutoString name(aTagName);
    PRInt32 nsoffset = name.Find(kNameSpaceSeparator);
    if (-1 != nsoffset) {
        name.Left(prefix, nsoffset);
        name.Cut(0, nsoffset+1);
    }

    // Figure out the namespace ID
    nsCOMPtr<nsIAtom> nameSpaceAtom;
    if (0 < prefix.Length())
        nameSpaceAtom = getter_AddRefs(NS_NewAtom(prefix));

    rv = ns->FindNameSpaceID(nameSpaceAtom, aNameSpaceID);
    if (NS_FAILED(rv))
        aNameSpaceID = kNameSpaceID_None;

    aName = NS_NewAtom(name);
    return NS_OK;
}


nsresult
XULDocumentImpl::MakeProperty(PRInt32 aNameSpaceID, nsIAtom* aTag, nsIRDFResource** aResult)
{
    // Using the namespace ID and the tag, construct a fully-qualified
    // URI and turn it into an RDF property.

    NS_PRECONDITION(mNameSpaceManager != nsnull, "not initialized");
    if (! mNameSpaceManager)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    nsAutoString uri;
    rv = mNameSpaceManager->GetNameSpaceURI(aNameSpaceID, uri);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get URI for namespace");
    if (NS_FAILED(rv)) return rv;

    if (uri.Last() != PRUnichar('#') && uri.Last() != PRUnichar('/'))
        uri.Append('#');

    uri.Append(aTag->GetUnicode());

    rv = gRDFService->GetUnicodeResource(uri, aResult);
    return rv;
}
