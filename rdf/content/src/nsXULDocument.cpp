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

  An implementation for the XUL document. This implementation serves
  as the basis for generating an NGLayout content model.

  To Do
  -----

  1. Implement DOM range constructors.

  2. Implement XIF conversion (this is really low priority).

  Notes
  -----

  1. We do some monkey business in the document observer methods to
     keep the element map in sync for HTML elements. Why don't we just
     do it for _all_ elements? Well, in the case of XUL elements,
     which may be lazily created during frame construction, the
     document observer methods will never be called because we'll be
     adding the XUL nodes into the content model "quietly".

  2. The "element map" maps an RDF resource to the elements whose 'id'
     or 'ref' attributes refer to that resource. We re-use the element
     map to support the HTML-like 'getElementById()' method.

*/

// Note the ALPHABETICAL ORDERING
#include "nsCOMPtr.h"
#include "nsDOMCID.h"
#include "nsElementMap.h"
#include "nsForwardReference.h"
#include "nsIArena.h"
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsIContent.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventCapturer.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMNSDocument.h"
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
#include "nsIHTMLContent.h"
#include "nsIHTMLContentContainer.h"
#include "nsIHTMLElementFactory.h"
#include "nsIHTMLStyleSheet.h"
#include "nsICSSLoader.h"
#include "nsIJSScriptObject.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIParser.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsIContentViewer.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIEventListenerManager.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIStreamLoadableDocument.h"
#include "nsIStyleContext.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsISupportsArray.h"
#include "nsITextContent.h"
#include "nsIURL.h"
#include "nsNeckoUtil.h"
#include "nsILoadGroup.h"
#include "nsIWebShell.h"
#include "nsIXMLContent.h"
#include "nsIXULChildDocument.h"
#include "nsIXULContentSink.h"
#include "nsIXULDocument.h"
#include "nsIXULParentDocument.h"
#include "nsLayoutCID.h"
#include "nsParserCIID.h"
#include "nsRDFCID.h"
#include "nsIXULContentUtils.h"
#include "nsRDFDOMNodeList.h"
#include "nsWeakPtr.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h" // XXX should go away
#include "nsXULElement.h"
#include "plhash.h"
#include "plstr.h"
#include "prlog.h"
#include "rdfutil.h"
#include "rdf.h"

#include "nsIFrameReflow.h"
#include "nsIBrowserWindow.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIXULCommandDispatcher.h"
#include "nsIXULContent.h"
#include "nsIDOMEventCapturer.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"

#include "nsILineBreakerFactory.h"
#include "nsIWordBreakerFactory.h"
#include "nsLWBrkCIID.h"

#include "nsIInputStream.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kEventListenerManagerCID,   NS_EVENTLISTENERMANAGER_CID);
static NS_DEFINE_CID(kCSSParserCID,              NS_CSSPARSER_CID);
static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_CID(kHTMLElementFactoryCID,     NS_HTML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kHTMLStyleSheetCID,         NS_HTMLSTYLESHEET_CID);
static NS_DEFINE_CID(kHTMLCSSStyleSheetCID,      NS_HTML_CSS_STYLESHEET_CID);
static NS_DEFINE_CID(kCSSLoaderCID,              NS_CSS_LOADER_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,       NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kParserCID,                 NS_PARSER_IID); // XXX
static NS_DEFINE_CID(kPresShellCID,              NS_PRESSHELL_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,  NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kLocalStoreCID,             NS_LOCALSTORE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,      NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,       NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kTextNodeCID,               NS_TEXTNODE_CID);
static NS_DEFINE_CID(kWellFormedDTDCID,          NS_WELLFORMEDDTD_CID);
static NS_DEFINE_CID(kXULContentSinkCID,         NS_XULCONTENTSINK_CID);
static NS_DEFINE_CID(kXULContentUtilsCID,        NS_XULCONTENTUTILS_CID);
static NS_DEFINE_CID(kXULCommandDispatcherCID,   NS_XULCOMMANDDISPATCHER_CID);
static NS_DEFINE_CID(kLWBrkCID,                  NS_LWBRK_CID);

static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID); // no comment

////////////////////////////////////////////////////////////////////////

#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"

static PRLogModuleInfo* gXULLog;

////////////////////////////////////////////////////////////////////////
// XULDocumentImpl

class XULDocumentImpl : public nsIDocument,
                        public nsIXULDocument,
                        public nsIStreamLoadableDocument,
                        public nsIDOMXULDocument,
                        public nsIDOMNSDocument,
                        public nsIDOMEventCapturer, 
                        public nsIJSScriptObject,
                        public nsIScriptObjectOwner,
                        public nsIHTMLContentContainer,
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

    NS_IMETHOD GetContentType(nsString& aContentType) const;

    NS_IMETHOD StartDocumentLoad(const char* aCommand,
#ifdef NECKO
                                 nsIChannel* aChannel,
                                 nsILoadGroup* aLoadGroup,
#else
                                 nsIURI *aUrl, 
#endif
                                 nsIContentViewerContainer* aContainer,
                                 nsIStreamListener **aDocListener);

    NS_IMETHOD LoadFromStream(nsIInputStream& xulStream,
                              nsIContentViewerContainer* aContainer,
                              const char* aCommand );

    virtual const nsString* GetDocumentTitle() const;

    virtual nsIURI* GetDocumentURL() const;

    virtual nsIPrincipal* GetDocumentPrincipal();

    NS_IMETHOD GetDocumentLoadGroup(nsILoadGroup **aGroup) const;

    NS_IMETHOD GetBaseURL(nsIURI*& aURL) const;

    NS_IMETHOD GetDocumentCharacterSet(nsString& oCharSetID);

    NS_IMETHOD SetDocumentCharacterSet(const nsString& aCharSetID);

    NS_IMETHOD GetLineBreaker(nsILineBreaker** aResult) ;
    NS_IMETHOD SetLineBreaker(nsILineBreaker* aLineBreaker) ;
    NS_IMETHOD GetWordBreaker(nsIWordBreaker** aResult) ;
    NS_IMETHOD SetWordBreaker(nsIWordBreaker* aWordBreaker) ;

    NS_IMETHOD GetHeaderData(nsIAtom* aHeaderField, nsString& aData) const;
    NS_IMETHOD SetHeaderData(nsIAtom* aheaderField, const nsString& aData);

    NS_IMETHOD CreateShell(nsIPresContext* aContext,
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
    NS_IMETHOD InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex, PRBool aNotify);

    virtual void SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                            PRBool mDisabled);

    NS_IMETHOD GetCSSLoader(nsICSSLoader*& aLoader);

    virtual nsIScriptContextOwner *GetScriptContextOwner();

    virtual void SetScriptContextOwner(nsIScriptContextOwner *aScriptContextOwner);

    NS_IMETHOD GetNameSpaceManager(nsINameSpaceManager*& aManager);

    virtual void AddObserver(nsIDocumentObserver* aObserver);

    virtual PRBool RemoveObserver(nsIDocumentObserver* aObserver);

    NS_IMETHOD BeginLoad();

    NS_IMETHOD EndLoad();

    NS_IMETHOD ContentChanged(nsIContent* aContent,
                              nsISupports* aSubContent);

    NS_IMETHOD ContentStatesChanged(nsIContent* aContent1, nsIContent* aContent2);

    NS_IMETHOD AttributeChanged(nsIContent* aChild,
                                PRInt32 aNameSpaceID,
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

    NS_IMETHOD CreateXIF(nsString & aBuffer, nsIDOMSelection* aSelection);

    NS_IMETHOD ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

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
#ifdef XSL
    NS_IMETHOD SetTransformMediator(nsITransformMediator* aMediator);
#endif

    // nsIXULDocument interface
    NS_IMETHOD AddElementForID(const nsString& aID, nsIContent* aElement);
    NS_IMETHOD RemoveElementForID(const nsString& aID, nsIContent* aElement);
    NS_IMETHOD GetElementsForID(const nsString& aID, nsISupportsArray* aElements);
    NS_IMETHOD CreateContents(nsIContent* aElement);
    NS_IMETHOD AddContentModelBuilder(nsIRDFContentModelBuilder* aBuilder);
    NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
    NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm);
    NS_IMETHOD AddForwardReference(nsForwardReference* aRef);
    NS_IMETHOD ResolveForwardReferences();

    // nsIDOMEventCapturer interface
    NS_IMETHOD    CaptureEvent(const nsString& aType);
    NS_IMETHOD    ReleaseEvent(const nsString& aType);

    // nsIDOMEventReceiver interface (yuck. inherited from nsIDOMEventCapturer)
    NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
    NS_IMETHOD GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult);

    // nsIDOMEventTarget interface
    NS_IMETHOD AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                PRBool aUseCapture);
    NS_IMETHOD RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                   PRBool aUseCapture);

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

    // nsIDOMNSDocument interface
    NS_IMETHOD    GetStyleSheets(nsIDOMStyleSheetCollection** aStyleSheets);
    NS_IMETHOD    CreateElementWithNameSpace(const nsString& aTagName, const nsString& aNameSpace, nsIDOMElement** aResult);
    NS_IMETHOD    CreateRange(nsIDOMRange** aRange);
    NS_IMETHOD    GetWidth(PRInt32* aWidth);
    NS_IMETHOD    GetHeight(PRInt32* aHeight);

    // nsIDOMXULDocument interface
    NS_DECL_IDOMXULDOCUMENT
                   
    // nsIXULParentDocument interface
    NS_IMETHOD    GetContentViewerContainer(nsIContentViewerContainer** aContainer);
    NS_IMETHOD    GetCommand(nsString& aCommand);
    
    // nsIXULChildDocument Interface
    NS_IMETHOD    SetContentSink(nsIXULContentSink* aContentSink);
    NS_IMETHOD    GetContentSink(nsIXULContentSink** aContentSink);

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

    // needed by the XUL Content Sink to do URL conversions
    NS_IMETHOD GetChannel(nsIChannel **aResult);

protected:
    // Implementation methods
    friend nsresult
    NS_NewXULDocument(nsIXULDocument** aResult);

    nsresult Init(void);
    nsresult StartLayout(void);

    nsresult OpenWidgetItem(nsIContent* aElement);
    nsresult CloseWidgetItem(nsIContent* aElement);
    nsresult RebuildWidgetItem(nsIContent* aElement);

    nsresult
    AddSubtreeToDocument(nsIContent* aElement);

    nsresult
    RemoveSubtreeFromDocument(nsIContent* aElement);

    nsresult
    AddElementToMap(nsIContent* aElement);

    nsresult
    RemoveElementFromMap(nsIContent* aElement);

    static PRIntn
    RemoveElementsFromMapByContent(const nsString& aID,
                                   nsIContent* aElement,
                                   void* aClosure);

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

    NS_IMETHOD PrepareStyleSheets(nsIURI* anURL);
    
    void SetDocumentURLAndGroup(nsIURI* anURL);
    void SetIsPopup(PRBool isPopup) { mIsPopup = isPopup; };

    nsresult CreateElement(PRInt32 aNameSpaceID,
                           nsIAtom* aTag,
                           nsIContent** aResult);

    nsresult PrepareToLoad(nsCOMPtr<nsIParser>* created_parser,
                           nsIContentViewerContainer* aContainer,
                           const char* aCommand,
                           nsIChannel* aChannel, nsILoadGroup* aLoadGroup);

    nsresult ApplyPersistentAttributes();
    nsresult ApplyPersistentAttributesToElements(nsIRDFResource* aResource, nsISupportsArray* aElements);

protected:
    // pseudo constants
    static PRInt32 gRefCnt;

    static nsIAtom*  kCommandUpdaterAtom;
    static nsIAtom*  kIdAtom;
    static nsIAtom*  kObservesAtom;
    static nsIAtom*  kOpenAtom;
    static nsIAtom*  kPersistAtom;
    static nsIAtom*  kRefAtom;
    static nsIAtom*  kRuleAtom;
    static nsIAtom*  kTemplateAtom;

    static nsIAtom** kIdentityAttrs[];

    static nsIRDFService* gRDFService;
    static nsIRDFResource* kNC_persist;
    static nsIRDFResource* kNC_attribute;
    static nsIRDFResource* kNC_value;

    static nsIHTMLElementFactory* gHTMLElementFactory;

    static nsINameSpaceManager* gNameSpaceManager;
    static PRInt32 kNameSpaceID_XUL;

    static nsIXULContentUtils* gXULUtils;

    nsIContent*
    FindContent(const nsIContent* aStartNode,
                const nsIContent* aTest1,
                const nsIContent* aTest2) const;

    nsresult
    Persist(nsIContent* aElement, PRInt32 aNameSpaceID, nsIAtom* aAttribute);

    nsresult
    DestroyForwardReferences();

    // IMPORTANT: The ownership implicit in the following member variables has been 
    // explicitly checked and set using nsCOMPtr for owning pointers and raw COM interface 
    // pointers for weak (ie, non owning) references. If you add any members to this
    // class, please make the ownership explicit (pinkerton, scc).
    // NOTE, THIS IS STILL IN PROGRESS, TALK TO PINK OR SCC BEFORE CHANGING

    nsCOMPtr<nsIArena>         mArena;
    nsVoidArray                mObservers;
    nsAutoString               mDocumentTitle;
    nsCOMPtr<nsIURI>           mDocumentURL;        // [OWNER] ??? compare with loader
    nsWeakPtr                  mDocumentLoadGroup;  // [WEAK] leads to loader
    nsCOMPtr<nsIPrincipal>     mDocumentPrincipal;  // [OWNER]
    nsCOMPtr<nsIContent>       mRootContent;        // [OWNER] 
    nsIDocument*               mParentDocument;     // [WEAK]
    nsIScriptContextOwner*     mScriptContextOwner; // [WEAK] it owns me! (indirectly)
    void*                      mScriptObject;       // ????
    nsString                   mCharSetID;
    nsVoidArray                mStyleSheets;
    nsCOMPtr<nsIDOMSelection>  mSelection;          // [OWNER] 
    PRBool                     mDisplaySelection;
    nsVoidArray                mPresShells;
    nsCOMPtr<nsIEventListenerManager> mListenerManager;   // [OWNER]
    nsCOMPtr<nsINameSpaceManager>     mNameSpaceManager;  // [OWNER] 
    nsCOMPtr<nsIHTMLStyleSheet>       mAttrStyleSheet;    // [OWNER] 
    nsCOMPtr<nsIHTMLCSSStyleSheet>    mInlineStyleSheet;  // [OWNER]
    nsCOMPtr<nsICSSLoader>            mCSSLoader;         // [OWNER]
    nsElementMap               mElementMap;
    nsCOMPtr<nsISupportsArray> mBuilders;        // [OWNER] of array, elements shouldn't own this, but they do
    nsCOMPtr<nsIRDFDataSource>          mLocalStore;
    nsCOMPtr<nsILineBreaker>            mLineBreaker;    // [OWNER] 
    nsCOMPtr<nsIWordBreaker>            mWordBreaker;    // [OWNER] 
    nsIContentViewerContainer* mContentViewerContainer;  // [WEAK] it owns me! (indirectly)
    nsString                   mCommand;
    nsIXULContentSink*         mParentContentSink;     // [WEAK] 
    nsVoidArray                mSubDocuments;     // [OWNER] of subelements
    PRBool                     mIsPopup; 
    nsCOMPtr<nsIDOMHTMLFormElement>     mHiddenForm;   // [OWNER] of this content element
    nsCOMPtr<nsIDOMXULCommandDispatcher>     mCommandDispatcher; // [OWNER] of the focus tracker


    nsCOMPtr<nsIChannel> mChannel; // ?? channel we are currently using


    nsVoidArray mForwardReferences;
    PRBool mForwardReferencesResolved;

    // The following are pointers into the content model which provide access to
    // the objects triggering either a popup or a tooltip. These are marked as
    // [OWNER] only because someone could, through DOM calls, delete the object from the
    // content model while the popup/tooltip was visible. If we didn't have a reference
    // to it, the object would go away and we'd be left pointing to garbage. This
    // does not introduce cycles into the ownership model because this is still
    // parent/child ownership. Just wanted the reader to know hyatt and I had thought about
    // this (pinkerton).
    nsCOMPtr<nsIDOMNode>    mPopupNode;            // [OWNER] element triggering the popup
    nsCOMPtr<nsIDOMNode>    mTooltipNode;          // [OWNER] element triggering the tooltip
};

PRInt32 XULDocumentImpl::gRefCnt = 0;

nsIAtom* XULDocumentImpl::kCommandUpdaterAtom;
nsIAtom* XULDocumentImpl::kIdAtom;
nsIAtom* XULDocumentImpl::kObservesAtom;
nsIAtom* XULDocumentImpl::kOpenAtom;
nsIAtom* XULDocumentImpl::kPersistAtom;
nsIAtom* XULDocumentImpl::kRefAtom;
nsIAtom* XULDocumentImpl::kRuleAtom;
nsIAtom* XULDocumentImpl::kTemplateAtom;

nsIRDFService* XULDocumentImpl::gRDFService;
nsIRDFResource* XULDocumentImpl::kNC_persist;
nsIRDFResource* XULDocumentImpl::kNC_attribute;
nsIRDFResource* XULDocumentImpl::kNC_value;

nsIHTMLElementFactory* XULDocumentImpl::gHTMLElementFactory;

nsINameSpaceManager* XULDocumentImpl::gNameSpaceManager;
PRInt32 XULDocumentImpl::kNameSpaceID_XUL;

nsIXULContentUtils* XULDocumentImpl::gXULUtils;

////////////////////////////////////////////////////////////////////////
// ctors & dtors


XULDocumentImpl::XULDocumentImpl(void)
    : mParentDocument(nsnull),
      mScriptContextOwner(nsnull),
      mScriptObject(nsnull),
      mCharSetID("UTF-8"),
      mDisplaySelection(PR_FALSE),
      mContentViewerContainer(nsnull),
      mParentContentSink(nsnull),
      mIsPopup(PR_FALSE),
      mForwardReferencesResolved(PR_FALSE)
{
    NS_INIT_REFCNT();
}

XULDocumentImpl::~XULDocumentImpl()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: XULDocumentImpl\n", gInstanceCount);
#endif

    // In case we failed somewhere early on and the forward observer
    // decls never got resolved.
    DestroyForwardReferences();

    // mParentDocument is never refcounted
    // Delete references to sub-documents
    {
        PRInt32 i = mSubDocuments.Count();
        while (--i >= 0) {
            nsIDocument* subdoc = (nsIDocument*) mSubDocuments.ElementAt(i);
            NS_RELEASE(subdoc);
        }
    }

    // Delete references to style sheets but only if we aren't a popup document.
    if (!mIsPopup) {
        PRInt32 i = mStyleSheets.Count();
        while (--i >= 0) {
            nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(i);
            sheet->SetOwningDocument(nsnull);
            NS_RELEASE(sheet);
        }
    }

    // set all builder references to document to nsnull -- out of band notification
    // to break ownership cycle
    if (mBuilders)
    {
        PRUint32 cnt = 0;
        nsresult rv = mBuilders->Count(&cnt);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");

#ifdef	DEBUG
        printf("# of builders: %lu\n", (unsigned long)cnt);
#endif

        for (PRUint32 i = 0; i < cnt; ++i) {
          nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

          NS_ASSERTION(builder != nsnull, "null ptr");
          if (! builder) continue;

          rv = builder->SetDocument(nsnull);
          NS_ASSERTION(NS_SUCCEEDED(rv), "error unlinking builder from document");
          // XXX ignore error code?

          rv = builder->SetDataBase(nsnull);
          NS_ASSERTION(NS_SUCCEEDED(rv), "error unlinking builder from database");

          NS_RELEASE(builder);
        }
    }

    if (mLocalStore) {
        nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(mLocalStore);
        if (remote)
            remote->Flush();
    }

    if (mCSSLoader) {
      mCSSLoader->DropDocumentReference();
    }

#if 0
    PRInt32 i;
    for (i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(i);
        observer->DocumentWillBeDestroyed(this);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
#endif

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(kCommandUpdaterAtom);
        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kObservesAtom);
        NS_IF_RELEASE(kOpenAtom);
        NS_IF_RELEASE(kPersistAtom);
        NS_IF_RELEASE(kRefAtom);
        NS_IF_RELEASE(kRuleAtom);
        NS_IF_RELEASE(kTemplateAtom);

        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }

        NS_IF_RELEASE(kNC_persist);
        NS_IF_RELEASE(kNC_attribute);
        NS_IF_RELEASE(kNC_value);

        NS_IF_RELEASE(gHTMLElementFactory);

        if (gXULUtils) {
            nsServiceManager::ReleaseService(kXULContentUtilsCID, gXULUtils);
            gXULUtils = nsnull;
        }
    }
}


nsresult
NS_NewXULDocument(nsIXULDocument** result)
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
    if (iid.Equals(NS_GET_IID(nsIDocument)) ||
        iid.Equals(NS_GET_IID(nsISupports))) {
        *result = NS_STATIC_CAST(nsIDocument*, this);
    }
    else if (iid.Equals(nsIXULParentDocument::GetIID())) {
        *result = NS_STATIC_CAST(nsIXULParentDocument*, this);
    }
    else if (iid.Equals(nsIXULChildDocument::GetIID())) {
        *result = NS_STATIC_CAST(nsIXULChildDocument*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIXULDocument)) ||
             iid.Equals(NS_GET_IID(nsIXMLDocument))) {
        *result = NS_STATIC_CAST(nsIXULDocument*, this);
    }
    else if (iid.Equals(nsIDOMXULDocument::GetIID()) ||
             iid.Equals(nsIDOMDocument::GetIID()) ||
             iid.Equals(nsIDOMNode::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMXULDocument*, this);
    }
    else if (iid.Equals(nsIDOMNSDocument::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMNSDocument*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIJSScriptObject))) {
        *result = NS_STATIC_CAST(nsIJSScriptObject*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIScriptObjectOwner))) {
        *result = NS_STATIC_CAST(nsIScriptObjectOwner*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIHTMLContentContainer))) {
        *result = NS_STATIC_CAST(nsIHTMLContentContainer*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMEventReceiver))) {
        *result = NS_STATIC_CAST(nsIDOMEventReceiver*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMEventTarget))) {
        *result = NS_STATIC_CAST(nsIDOMEventTarget*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMEventCapturer))) {
        *result = NS_STATIC_CAST(nsIDOMEventCapturer*, this);
    }
    else if (iid.Equals(nsIStreamLoadableDocument::GetIID())) {
    		*result = NS_STATIC_CAST(nsIStreamLoadableDocument*, this);
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
    nsIArena* result = mArena;
    NS_IF_ADDREF(result);
    return result;
}

NS_IMETHODIMP 
XULDocumentImpl::GetContentType(nsString& aContentType) const
{
    // XXX Is this right, Chris?
    aContentType.SetString("text/xul");
    return NS_OK;
}

static
nsresult
generate_RDF_seed( nsString* result, nsIURI* aOptionalURL )
	{
		nsresult status = NS_OK;

		if ( aOptionalURL )
			{
#ifdef NECKO
                char* s = 0;
#else
				const char* s = 0;
#endif
                status = aOptionalURL->GetSpec(&s);
				if ( NS_SUCCEEDED(status) ) {
					(*result) = s;      // copied by nsString
#ifdef NECKO
                    nsCRT::free(s);
#endif
                }
			}
		else
			{
				static int unique_per_session_index = 0;

				result->Append("x-anonymous-xul://");
				result->Append(PRInt32(++unique_per_session_index), /*base*/ 10);
			}

		return status;
	}

nsresult
XULDocumentImpl::PrepareToLoad(nsCOMPtr<nsIParser>* created_parser,
                               nsIContentViewerContainer* aContainer,
                               const char* aCommand,
                               nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
    nsCOMPtr<nsIURI> syntheticURL;
    if ( aChannel ) {
        (void)aChannel->GetURI(getter_AddRefs(syntheticURL));
    }
    else {
        nsAutoString seedString;
        generate_RDF_seed(&seedString, 0);
        NS_NewURI(getter_AddRefs(syntheticURL), seedString);
    }

    if (aContainer && aContainer != mContentViewerContainer)
        mContentViewerContainer = aContainer;

    nsresult rv;

    mDocumentTitle.Truncate();

    mDocumentURL = syntheticURL;

    nsCOMPtr<nsISupports> owner;
    rv = aChannel->GetOwner(getter_AddRefs(owner));
    mDocumentPrincipal = do_QueryInterface(owner);
    if (NS_FAILED(rv)) return rv;

    mDocumentLoadGroup = getter_AddRefs(NS_GetWeakReference(aLoadGroup));

    SetDocumentURLAndGroup(syntheticURL);

    rv = PrepareStyleSheets(syntheticURL);
    if (NS_FAILED(rv)) return rv;

    // Create a XUL content sink, a parser, and kick off the load.
    nsCOMPtr<nsIXULContentSink> sink;
    rv = nsComponentManager::CreateInstance(kXULContentSinkCID,
                                            nsnull,
                                            NS_GET_IID(nsIXULContentSink),
                                            getter_AddRefs(sink));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create XUL content sink");
    if (NS_FAILED(rv)) return rv;

    rv = sink->Init(this);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to initialize datasource sink");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIParser> parser;
    rv = nsComponentManager::CreateInstance(kParserCID,
                                            nsnull,
                                            kIParserIID,
                                            getter_AddRefs(parser));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create parser");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDTD> dtd;
    rv = nsComponentManager::CreateInstance(kWellFormedDTDCID,
                                            nsnull,
                                            NS_GET_IID(nsIDTD),
                                            getter_AddRefs(dtd));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to construct DTD");
    if (NS_FAILED(rv)) return rv;

    mCommand = aCommand;

    parser->RegisterDTD(dtd);
    parser->SetCommand(aCommand);
    nsAutoString utf8("UTF-8");
    parser->SetDocumentCharset(utf8, kCharsetFromDocTypeDefault);
    parser->SetContentSink(sink); // grabs a reference to the parser

    *created_parser = parser;

    return rv;
}

NS_IMETHODIMP 
XULDocumentImpl::PrepareStyleSheets(nsIURI* anURL)
{
    nsresult rv;
    
    // Delete references to style sheets - this should be done in superclass...
    PRInt32 i = mStyleSheets.Count();
    while (--i >= 0) {
        nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(i);
        sheet->SetOwningDocument(nsnull);
        NS_RELEASE(sheet);
    }
    mStyleSheets.Clear();

    // Create an HTML style sheet for the HTML content.
    nsCOMPtr<nsIHTMLStyleSheet> sheet;
    if (NS_SUCCEEDED(rv = nsComponentManager::CreateInstance(kHTMLStyleSheetCID,
                                                       nsnull,
                                                       NS_GET_IID(nsIHTMLStyleSheet),
                                                       getter_AddRefs(sheet)))) {
        if (NS_SUCCEEDED(rv = sheet->Init(anURL, this))) {
            mAttrStyleSheet = sheet;
            AddStyleSheet(mAttrStyleSheet);
        }
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
                                                       NS_GET_IID(nsIHTMLCSSStyleSheet),
                                                       (void**)&inlineSheet))) {
        if (NS_SUCCEEDED(rv = inlineSheet->Init(anURL, this))) {
            mInlineStyleSheet = dont_QueryInterface(inlineSheet);
            AddStyleSheet(mInlineStyleSheet);
        }
        NS_RELEASE(inlineSheet);
    }

    if (NS_FAILED(rv)) {
        NS_ERROR("unable to add inline style sheet");
        return rv;
    }

    return NS_OK;
}

void
XULDocumentImpl::SetDocumentURLAndGroup(nsIURI* anURL)
{
    mDocumentURL = dont_QueryInterface(anURL);
#ifdef NECKO
    // XXX help
#else
    anURL->GetLoadGroup(getter_AddRefs(mDocumentLoadGroup));
#endif
}

NS_IMETHODIMP 
XULDocumentImpl::StartDocumentLoad(const char* aCommand,
#ifdef NECKO
                                   nsIChannel* aChannel,
                                   nsILoadGroup* aLoadGroup,
#else
                                   nsIURI *aURL,
#endif
                                   nsIContentViewerContainer* aContainer,
                                   nsIStreamListener **aDocListener)
{
		nsresult status;
		nsCOMPtr<nsIParser> parser;
#ifdef NECKO
    mChannel = aChannel;
    nsCOMPtr<nsIURI> aURL;
    status = aChannel->GetURI(getter_AddRefs(aURL));
    if (NS_FAILED(status)) return status;
#endif

		do
			{
#ifdef NECKO
        status = PrepareToLoad(&parser, aContainer, aCommand, aChannel, aLoadGroup);
#else
        status = PrepareToLoad(&parser, aContainer, aCommand, aURL);
#endif
				if ( NS_FAILED(status) )
					break;

				{
					nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(parser, &status);
					if ( NS_FAILED(status) )
						{
		        	NS_ERROR("parser doesn't support nsIStreamListener");
							break;
						}

					*aDocListener = listener;
					NS_IF_ADDREF(*aDocListener);
				}

				parser->Parse(aURL);
			}
		while(0);
   
    return status;
}

nsresult
XULDocumentImpl::LoadFromStream( nsIInputStream& xulStream,
																 nsIContentViewerContainer* aContainer,
																 const char* aCommand )
	{
		nsresult status;
		nsCOMPtr<nsIParser> parser;
#ifdef NECKO
		if ( NS_SUCCEEDED(status = PrepareToLoad(&parser, aContainer, aCommand, nsnull, nsnull)) )
#else
		if ( NS_SUCCEEDED(status = PrepareToLoad(&parser, aContainer, aCommand, nsnull)) )
#endif
			parser->Parse(xulStream);

		return status;
	}

const nsString*
XULDocumentImpl::GetDocumentTitle() const
{
    return &mDocumentTitle;
}

nsIURI* 
XULDocumentImpl::GetDocumentURL() const
{
    nsIURI* result = mDocumentURL;
    NS_IF_ADDREF(result);
    return result;
}

nsIPrincipal* 
XULDocumentImpl::GetDocumentPrincipal()
{
  if (!mDocumentPrincipal) {
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager,
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) 
        return nsnull;
    if (NS_FAILED(securityManager->CreateCodebasePrincipal(mDocumentURL, 
                    getter_AddRefs(mDocumentPrincipal))))
    {
        return nsnull;
    }
  }
  nsIPrincipal *result = mDocumentPrincipal;
  NS_ADDREF(result);
  return result;
}


NS_IMETHODIMP
XULDocumentImpl::GetDocumentLoadGroup(nsILoadGroup **aGroup) const
{
    nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);

    *aGroup = group;
    NS_IF_ADDREF(*aGroup);
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::GetBaseURL(nsIURI*& aURL) const
{
    aURL = mDocumentURL;
    NS_IF_ADDREF(aURL);
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::GetDocumentCharacterSet(nsString& oCharSetID) 
{
    oCharSetID = mCharSetID;
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::SetDocumentCharacterSet(const nsString& aCharSetID)
{
    mCharSetID = aCharSetID;
    return NS_OK;
}


NS_IMETHODIMP 
XULDocumentImpl::GetLineBreaker(nsILineBreaker** aResult) 
{
  if(! mLineBreaker) {
     // no line breaker, find a default one
     nsILineBreakerFactory *lf;
     nsresult result;
     result = nsServiceManager::GetService(kLWBrkCID,
                                          NS_GET_IID(nsILineBreakerFactory),
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
  NS_IF_ADDREF(*aResult);
  return NS_OK; // XXX we should do error handling here
}

NS_IMETHODIMP 
XULDocumentImpl::SetLineBreaker(nsILineBreaker* aLineBreaker) 
{
  mLineBreaker = dont_QueryInterface(aLineBreaker);
  return NS_OK;
}
NS_IMETHODIMP 
XULDocumentImpl::GetWordBreaker(nsIWordBreaker** aResult) 
{
  if (! mWordBreaker) {
     // no line breaker, find a default one
     nsIWordBreakerFactory *lf;
     nsresult result;
     result = nsServiceManager::GetService(kLWBrkCID,
                                          NS_GET_IID(nsIWordBreakerFactory),
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
  NS_IF_ADDREF(*aResult);
  return NS_OK; // XXX we should do error handling here
}

NS_IMETHODIMP 
XULDocumentImpl::SetWordBreaker(nsIWordBreaker* aWordBreaker) 
{
  mWordBreaker = dont_QueryInterface(aWordBreaker);
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


NS_IMETHODIMP
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
                                                    NS_GET_IID(nsIPresShell),
                                                    (void**) &shell)))
        return rv;

    if (NS_FAILED(rv = shell->Init(this, aContext, aViewManager, aStyleSet))) {
        NS_RELEASE(shell);
        return rv;
    }

    mPresShells.AppendElement(shell);
    *aInstancePtrResult = shell; // addref implicit in CreateInstance()

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
    nsIContent* result = mRootContent;
    NS_IF_ADDREF(result);
    return result;
}

void 
XULDocumentImpl::SetRootContent(nsIContent* aRoot)
{
    if (mRootContent) {
        mRootContent->SetDocument(nsnull, PR_TRUE);
    }
    mRootContent = aRoot;
    if (mRootContent) {
        mRootContent->SetDocument(this, PR_TRUE);
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

    if (aSheet == mAttrStyleSheet.get()) {  // always first
      mStyleSheets.InsertElementAt(aSheet, 0);
    }
    else if (aSheet == (nsIHTMLCSSStyleSheet*)mInlineStyleSheet) {  // always last
      mStyleSheets.AppendElement(aSheet);
    }
    else {
      if ((nsIHTMLCSSStyleSheet*)mInlineStyleSheet == mStyleSheets.ElementAt(mStyleSheets.Count() - 1)) {
        // keep attr sheet last
        mStyleSheets.InsertElementAt(aSheet, mStyleSheets.Count() - 1);
      }
      else {
        mStyleSheets.AppendElement(aSheet);
      }
    }
    NS_ADDREF(aSheet);

    aSheet->SetOwningDocument(this);

    PRBool enabled;
    aSheet->GetEnabled(enabled);

    if (enabled) {
        PRInt32 count, i;

        count = mPresShells.Count();
        for (i = 0; i < count; i++) {
            nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);
            nsCOMPtr<nsIStyleSet> set;
            shell->GetStyleSet(getter_AddRefs(set));
            if (set) {
                set->AddDocStyleSheet(aSheet, this);
            }
        }

        // XXX should observers be notified for disabled sheets??? I think not, but I could be wrong
        for (i = 0; i < mObservers.Count(); i++) {
            nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(i);
            observer->StyleSheetAdded(this, aSheet);
            if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
              i--;
            }
        }
    }
}

NS_IMETHODIMP
XULDocumentImpl::InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex, PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aSheet, "null ptr");
  mStyleSheets.InsertElementAt(aSheet, aIndex + 1); // offset by one for attribute sheet

  NS_ADDREF(aSheet);
  aSheet->SetOwningDocument(this);

  PRBool enabled = PR_TRUE;
  aSheet->GetEnabled(enabled);

  PRInt32 count;
  PRInt32 i;
  if (enabled) {
    count = mPresShells.Count();
    for (i = 0; i < count; i++) {
      nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(i);
      nsCOMPtr<nsIStyleSet> set;
      shell->GetStyleSet(getter_AddRefs(set));
      if (set) {
        set->AddDocStyleSheet(aSheet, this);
      }
    }
  }
  if (aNotify) {  // notify here even if disabled, there may have been others that weren't notified
    for (i = 0; i < mObservers.Count(); i++) {
      nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(i);
      observer->StyleSheetAdded(this, aSheet);
      if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
        i--;
      }
    }
  }
  return NS_OK;
}

void 
XULDocumentImpl::SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                          PRBool aDisabled)
{
    NS_PRECONDITION(nsnull != aSheet, "null arg");
    PRInt32 count;
    PRInt32 i;

    // If we're actually in the document style sheet list
    if (-1 != mStyleSheets.IndexOf((void *)aSheet)) {
        count = mPresShells.Count();
        for (i = 0; i < count; i++) {
            nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(i);
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

    for (i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(i);
        observer->StyleSheetDisabledStateChanged(this, aSheet, aDisabled);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
}

NS_IMETHODIMP
XULDocumentImpl::GetCSSLoader(nsICSSLoader*& aLoader)
{
  nsresult result = NS_OK;
  if (! mCSSLoader) {
    result = nsComponentManager::CreateInstance(kCSSLoaderCID,
                                                nsnull,
                                                NS_GET_IID(nsICSSLoader),
                                                (void**)&mCSSLoader);
    if (NS_SUCCEEDED(result)) {
      result = mCSSLoader->Init(this);
      mCSSLoader->SetCaseSensitive(PR_TRUE);
      mCSSLoader->SetQuirkMode(PR_FALSE); // no quirks in XUL
    }
  }
  aLoader = mCSSLoader;
  NS_IF_ADDREF(aLoader);
  return result;
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

    mScriptContextOwner = aScriptContextOwner;
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
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->BeginLoad(this);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::EndLoad()
{
    // Set up the document's composite datasource, which will include
    // the main document datasource and the local store. Only do this
    // if we are a bona-fide top-level XUL document; (mParentContentSink !=
    // nsnull) implies we are a XUL overlay.
    NS_PRECONDITION(mParentContentSink == nsnull, "trying to notify an overlay");
    if (mParentContentSink)
        return NS_ERROR_UNEXPECTED;

    nsresult rv;

    // Do any initial hookup that needs to happen.
    rv = ResolveForwardReferences();
    if (NS_FAILED(rv)) return rv;

    rv = ApplyPersistentAttributes();
    if (NS_FAILED(rv)) return rv;

    StartLayout();

    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->EndLoad(this);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }

    return NS_OK;
}


NS_IMETHODIMP 
XULDocumentImpl::ContentChanged(nsIContent* aContent,
                              nsISupports* aSubContent)
{
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentChanged(this, aContent, aSubContent);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentStatesChanged(nsIContent* aContent1, nsIContent* aContent2)
{
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentStatesChanged(this, aContent1, aContent2);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::AttributeChanged(nsIContent* aElement,
                                  PRInt32 aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  PRInt32 aHint)
{
    nsresult rv;

    PRInt32 nameSpaceID;
    rv = aElement->GetNameSpaceID(nameSpaceID);
    if (NS_FAILED(rv)) return rv;

    // First see if we need to update our element map.
    if ((aAttribute == kIdAtom) || (aAttribute == kRefAtom)) {

        rv = mElementMap.Enumerate(RemoveElementsFromMapByContent, aElement);
        if (NS_FAILED(rv)) return rv;

        // That'll have removed _both_ the 'ref' and 'id' entries from
        // the map. So add 'em back now.
        rv = AddElementToMap(aElement);
        if (NS_FAILED(rv)) return rv;
    }

    // Now notify external observers
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->AttributeChanged(this, aElement, aNameSpaceID, aAttribute, aHint);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
            i--;
        }
    }

    // Handle "special" cases. We do this handling _after_ we've
    // notified the observer to ensure that any frames that are
    // caching information (e.g., the tree widget and the 'open'
    // attribute) will notice things properly.
    if ((nameSpaceID == kNameSpaceID_XUL) && (aAttribute == kOpenAtom)) {
        nsAutoString open;
        rv = aElement->GetAttribute(kNameSpaceID_None, kOpenAtom, open);
        if (NS_FAILED(rv)) return rv;

        if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (open.Equals("true"))) {
            OpenWidgetItem(aElement);
        }
        else {
            CloseWidgetItem(aElement);
        }
    }
    else if (aAttribute == kRefAtom) {
        RebuildWidgetItem(aElement);
    }


    // Finally, see if there is anything we need to persist in the
    // localstore.
    //
    // XXX Namespace handling broken :-(
    nsAutoString persist;
    rv = aElement->GetAttribute(kNameSpaceID_None, kPersistAtom, persist);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        nsAutoString attr;
        rv = aAttribute->ToString(attr);
        if (NS_FAILED(rv)) return rv;

        if (persist.Find(attr) >= 0) {
            rv = Persist(aElement, kNameSpaceID_None, aAttribute);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentAppended(nsIContent* aContainer,
                                 PRInt32 aNewIndexInContainer)
{
    // First update our element map
    {
        nsresult rv;

        PRInt32 count;
        rv = aContainer->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = aNewIndexInContainer; i < count; ++i) {
            nsCOMPtr<nsIContent> child;
            rv = aContainer->ChildAt(i, *getter_AddRefs(child));
            if (NS_FAILED(rv)) return rv;

            rv = AddSubtreeToDocument(child);
            if (NS_FAILED(rv)) return rv;
        }
    }

    // Now notify external observers
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentAppended(this, aContainer, aNewIndexInContainer);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentInserted(nsIContent* aContainer,
                                 nsIContent* aChild,
                                 PRInt32 aIndexInContainer)
{
    {
        nsresult rv;
        rv = AddSubtreeToDocument(aChild);
        if (NS_FAILED(rv)) return rv;
    }

    // Now notify external observers
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentInserted(this, aContainer, aChild, aIndexInContainer);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentReplaced(nsIContent* aContainer,
                                 nsIContent* aOldChild,
                                 nsIContent* aNewChild,
                                 PRInt32 aIndexInContainer)
{
    {
        nsresult rv;
        rv = RemoveSubtreeFromDocument(aOldChild);
        if (NS_FAILED(rv)) return rv;

        rv = AddSubtreeToDocument(aNewChild);
        if (NS_FAILED(rv)) return rv;
    }

    // Now notify external observers
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentReplaced(this, aContainer, aOldChild, aNewChild,
                                  aIndexInContainer);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentRemoved(nsIContent* aContainer,
                                nsIContent* aChild,
                                PRInt32 aIndexInContainer)
{
    {
        nsresult rv;
        rv = RemoveSubtreeFromDocument(aChild);
        if (NS_FAILED(rv)) return rv;
    }

    // Now notify external observers
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentRemoved(this, aContainer, 
                                 aChild, aIndexInContainer);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                                  nsIStyleRule* aStyleRule,
                                  PRInt32 aHint)
{
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleChanged(this, aStyleSheet, aStyleRule, aHint);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule)
{
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleAdded(this, aStyleSheet, aStyleRule);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                                  nsIStyleRule* aStyleRule)
{
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleRemoved(this, aStyleSheet, aStyleRule);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
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
    *aSelection = mSelection;
    NS_ADDREF(*aSelection);
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::SelectAll()
{

    nsIContent * start = nsnull;
    nsIContent * end   = nsnull;
    nsIContent * body  = nsnull;

    nsAutoString bodyStr("BODY");
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

NS_IMETHODIMP 
XULDocumentImpl::CreateXIF(nsString & aBuffer, nsIDOMSelection* aSelection)
{
    PR_ASSERT(0);
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
    return NS_OK;
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

        if (parent && parent != mRootContent.get()) {
            PRInt32 i;
            parent->IndexOf((nsIContent*)aContent, i);
            if (i > 0)
                parent->ChildAt(i - 1, result);
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

        if (parent != nsnull && parent != mRootContent.get()) {
            PRInt32 i;
            parent->IndexOf((nsIContent*)aContent, i);

            PRInt32 count;
            parent->ChildCount(count);
            if (i + 1 < count) {
                parent->ChildAt(i + 1, result);
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
  nsresult ret = NS_OK;
  nsIDOMEvent* domEvent = nsnull;

  if (NS_EVENT_FLAG_INIT == aFlags) {
    aDOMEvent = &domEvent;
    aEvent->flags = NS_EVENT_FLAG_NONE;
  }
  
  //Capturing stage
  if (NS_EVENT_FLAG_BUBBLE != aFlags && nsnull != mScriptContextOwner) {
    nsIScriptGlobalObject* global;
    if (NS_OK == mScriptContextOwner->GetScriptGlobalObject(&global)) {
      global->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_CAPTURE, aEventStatus);
      NS_RELEASE(global);
    }
  }
  
  //Local handling stage
  if (mListenerManager && !(aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH)) {
    aEvent->flags = aFlags;
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aFlags, aEventStatus);
  }

  //Bubbling stage
  if (NS_EVENT_FLAG_CAPTURE != aFlags && nsnull != mScriptContextOwner) {
    nsIScriptGlobalObject* global;
    if (NS_OK == mScriptContextOwner->GetScriptGlobalObject(&global)) {
      global->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_BUBBLE, aEventStatus);
      NS_RELEASE(global);
    }
  }

  if (NS_EVENT_FLAG_INIT == aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event, release here.
    if (nsnull != *aDOMEvent) {
      nsrefcnt rc;
      NS_RELEASE2(*aDOMEvent, rc);
      if (0 != rc) {
      //Okay, so someone in the DOM loop (a listener, JS object) still has a ref to the DOM Event but
      //the internal data hasn't been malloc'd.  Force a copy of the data here so the DOM Event is still valid.
        nsIPrivateDOMEvent *privateEvent;
        if (NS_OK == (*aDOMEvent)->QueryInterface(NS_GET_IID(nsIPrivateDOMEvent), (void**)&privateEvent)) {
          privateEvent->DuplicatePrivateData();
          NS_RELEASE(privateEvent);
        }
      }
    }
    aDOMEvent = nsnull;
  }

  return ret;
}


////////////////////////////////////////////////////////////////////////
// nsIXMLDocument interface
NS_IMETHODIMP 
XULDocumentImpl::GetContentById(const nsString& aName, nsIContent** aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef XSL
NS_IMETHODIMP 
XULDocumentImpl::SetTransformMediator(nsITransformMediator* aMediator)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

////////////////////////////////////////////////////////////////////////
// nsIXULDocument interface

NS_IMETHODIMP
XULDocumentImpl::AddElementForID(const nsString& aID, nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    mElementMap.Add(aID, aElement);
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::RemoveElementForID(const nsString& aID, nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    mElementMap.Remove(aID, aElement);
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetElementsForID(const nsString& aID, nsISupportsArray* aElements)
{
    NS_PRECONDITION(aElements != nsnull, "null ptr");
    if (! aElements)
        return NS_ERROR_NULL_POINTER;

    mElementMap.Find(aID, aElements);
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

    PRUint32 cnt = 0;
    nsresult rv = mBuilders->Count(&cnt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
    for (PRUint32 i = 0; i < cnt; ++i) {
        // XXX we should QueryInterface() here
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        NS_ASSERTION(builder != nsnull, "null ptr");
        if (! builder)
            continue;

        rv = builder->CreateContents(aElement);
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
        rv = NS_NewISupportsArray(getter_AddRefs(mBuilders));
        if (NS_FAILED(rv)) return rv;
    }

    rv = aBuilder->SetDocument(this);
    if (NS_FAILED(rv)) return rv;

    return mBuilders->AppendElement(aBuilder) ? NS_OK : NS_ERROR_FAILURE;
}


NS_IMETHODIMP
XULDocumentImpl::GetForm(nsIDOMHTMLFormElement** aForm)
{
  *aForm = mHiddenForm;
  NS_IF_ADDREF(*aForm);
  return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::SetForm(nsIDOMHTMLFormElement* aForm)
{
    mHiddenForm = dont_QueryInterface(aForm);

    // Set the document.
    nsCOMPtr<nsIContent> formContent = do_QueryInterface(aForm);
    formContent->SetDocument(this, PR_TRUE);

    // Forms are containers, and as such take up a bit of space.
    // Set a style attribute to keep the hidden form from showing up.
    mHiddenForm->SetAttribute("style", "margin:0em");
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::AddForwardReference(nsForwardReference* aRef)
{
    if (! mForwardReferencesResolved) {
        mForwardReferences.AppendElement(aRef);
    }
    else {
        NS_ERROR("forward references have already been resolved");
        delete aRef;
    }
        
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::ResolveForwardReferences()
{
    if (mForwardReferencesResolved)
        return NS_OK;

    // So we're monotonic. Ask brendan why.
    mForwardReferencesResolved = PR_TRUE;

    // Resolve each outstanding 'forward' references.
    PRInt32 previous = 0;
    while (mForwardReferences.Count() && mForwardReferences.Count() != previous) {
        previous = mForwardReferences.Count();

        for (PRInt32 i = 0; i < mForwardReferences.Count(); ++i) {
            nsForwardReference* fwdref = NS_REINTERPRET_CAST(nsForwardReference*, mForwardReferences[i]);

            nsForwardReference::Result result = fwdref->Resolve();

            switch (result) {
            case nsForwardReference::eResolveSucceeded:
            case nsForwardReference::eResolveError:
                mForwardReferences.RemoveElementAt(i);
                delete fwdref;

                // fixup because we removed from list
                --i;
                break;

            case nsForwardReference::eResolveLater:
                // do nothing. we'll try again later
                ;
            }
        }
    }

    DestroyForwardReferences();
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
    NS_PRECONDITION(aDocumentElement != nsnull, "null ptr");
    if (! aDocumentElement)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        return mRootContent->QueryInterface(nsIDOMElement::GetIID(), (void**)aDocumentElement);
    }
    else {
        *aDocumentElement = nsnull;
        return NS_OK;
    }
}



NS_IMETHODIMP
XULDocumentImpl::CreateElement(const nsString& aTagName, nsIDOMElement** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
      char* tagCStr = aTagName.ToNewCString();

      PR_LOG(gXULLog, PR_LOG_DEBUG,
             ("xul[CreateElement] %s", tagCStr));

      nsCRT::free(tagCStr);
    }
#endif

    nsCOMPtr<nsIAtom> name;
    PRInt32 nameSpaceID;

    *aReturn = nsnull;

    // parse the user-provided string into a tag name and a namespace ID
    rv = ParseTagString(aTagName, *getter_AddRefs(name), nameSpaceID);
    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        char* tagNameStr = aTagName.ToNewCString();
        PR_LOG(gXULLog, PR_LOG_ERROR,
               ("xul[CreateElement] unable to parse tag '%s'; no such namespace.", tagNameStr));
        nsCRT::free(tagNameStr);
#endif
        return rv;
    }

    nsCOMPtr<nsIContent> result;
    rv = CreateElement(nameSpaceID, name, getter_AddRefs(result));
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
XULDocumentImpl::Persist(const nsString& aID, const nsString& aAttr)
{
    nsresult rv;

    nsCOMPtr<nsIDOMElement> domelement;
    rv = GetElementById(aID, getter_AddRefs(domelement));
    if (NS_FAILED(rv)) return rv;

    if (! domelement)
        return NS_OK;

    nsCOMPtr<nsIContent> element = do_QueryInterface(domelement);
    NS_ASSERTION(element != nsnull, "null ptr");
    if (! element)
        return NS_ERROR_UNEXPECTED;

    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> tag;
    rv = element->ParseAttributeString(aAttr, *getter_AddRefs(tag), nameSpaceID);
    if (NS_FAILED(rv)) return rv;

    rv = Persist(element, nameSpaceID, tag);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
XULDocumentImpl::Persist(nsIContent* aElement, PRInt32 aNameSpaceID, nsIAtom* aAttribute)
{
    // First make sure we _have_ a local store to stuff the persited
    // information into. (We might not have one if profile information
    // hasn't been loaded yet...)
    if (! mLocalStore)
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIRDFResource> element;
    rv = gXULUtils->GetElementResource(aElement, getter_AddRefs(element));
    if (NS_FAILED(rv)) return rv;

    // No ID, so nothing to persist.
    if (! element)
        return NS_OK;

    // Ick. Construct a property from the attribute. Punt on
    // namespaces for now.
    const PRUnichar* attrstr;
    rv = aAttribute->GetUnicode(&attrstr);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> attr;
    rv = gRDFService->GetResource(nsCAutoString(attrstr), getter_AddRefs(attr));
    if (NS_FAILED(rv)) return rv;

    // Turn the value into a literal
    nsAutoString valuestr;
    rv = aElement->GetAttribute(kNameSpaceID_None, aAttribute, valuestr);
    if (NS_FAILED(rv)) return rv;

    PRBool novalue = (rv != NS_CONTENT_ATTR_HAS_VALUE);

    // See if there was an old value...
    nsCOMPtr<nsIRDFNode> oldvalue;
    rv = mLocalStore->GetTarget(element, attr, PR_TRUE, getter_AddRefs(oldvalue));
    if (NS_FAILED(rv)) return rv;

    if (oldvalue && novalue) {
        // ...there was an oldvalue, and they've removed it. XXXThis
        // handling isn't quite right...
        rv = mLocalStore->Unassert(element, attr, oldvalue);
    }
    else {
        // Now either 'change' or 'assert' based on whether there was
        // an old value.
        nsCOMPtr<nsIRDFLiteral> newvalue;
        rv = gRDFService->GetLiteral(valuestr.GetUnicode(), getter_AddRefs(newvalue));
        if (NS_FAILED(rv)) return rv;

        if (oldvalue) {
            rv = mLocalStore->Change(element, attr, oldvalue, newvalue);
        }
        else {
            rv = mLocalStore->Assert(element, attr, newvalue, PR_TRUE);
        }
    }

    if (NS_FAILED(rv)) return rv;

    // Add it to the persisted set for this document (if it's not
    // there already).
    {
        nsXPIDLCString docurl;
        rv = mDocumentURL->GetSpec(getter_Copies(docurl));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> doc;
        rv = gRDFService->GetResource(docurl, getter_AddRefs(doc));
        if (NS_FAILED(rv)) return rv;

        PRBool hasAssertion;
        rv = mLocalStore->HasAssertion(doc, kNC_persist, element, PR_TRUE, &hasAssertion);
        if (NS_FAILED(rv)) return rv;

        if (! hasAssertion) {
            rv = mLocalStore->Assert(doc, kNC_persist, element, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}



nsresult
XULDocumentImpl::DestroyForwardReferences()
{
    for (PRInt32 i = mForwardReferences.Count() - 1; i >= 0; --i) {
        nsForwardReference* fwdref = NS_REINTERPRET_CAST(nsForwardReference*, mForwardReferences[i]);
        delete fwdref;
    }

    mForwardReferences.Clear();
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIDOMNSDocument interface

NS_IMETHODIMP
XULDocumentImpl::GetStyleSheets(nsIDOMStyleSheetCollection** aStyleSheets)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateElementWithNameSpace(const nsString& aTagName,
                                            const nsString& aNameSpace,
                                            nsIDOMElement** aResult)
{
    // Create a DOM element given a namespace URI and a tag name.
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
      char* namespaceCStr = aNameSpace.ToNewCString();
      char* tagCStr = aTagName.ToNewCString();

      PR_LOG(gXULLog, PR_LOG_DEBUG,
             ("xul[CreateElementWithNameSpace] [%s]:%s", namespaceCStr, tagCStr));

      nsCRT::free(tagCStr);
      nsCRT::free(namespaceCStr);
    }
#endif

    nsCOMPtr<nsIAtom> name = dont_AddRef(NS_NewAtom(aTagName.GetUnicode()));
    if (! name)
        return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 nameSpaceID;
    rv = mNameSpaceManager->GetNameSpaceID(aNameSpace, nameSpaceID);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIContent> result;
    rv = CreateElement(nameSpaceID, name, getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;

    // get the DOM interface
    rv = result->QueryInterface(nsIDOMElement::GetIID(), (void**) aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM element");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::CreateRange(nsIDOMRange** aRange)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
XULDocumentImpl::GetWidth(PRInt32* aWidth)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP    
XULDocumentImpl::GetHeight(PRInt32* aHeight)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////
// nsIDOMXULDocument interface

NS_IMETHODIMP
XULDocumentImpl::GetPopupNode(nsIDOMNode** aNode)
{
	*aNode = mPopupNode;
	NS_IF_ADDREF(*aNode);
	return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::SetPopupNode(nsIDOMNode* aNode)
{
	mPopupNode = dont_QueryInterface(aNode);
	return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::GetTooltipNode(nsIDOMNode** aNode)
{
	*aNode = mTooltipNode;
	NS_IF_ADDREF(*aNode);
	return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::SetTooltipNode(nsIDOMNode* aNode)
{
	mTooltipNode = dont_QueryInterface(aNode);
	return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetCommandDispatcher(nsIDOMXULCommandDispatcher** aTracker)
{
  *aTracker = mCommandDispatcher;
  NS_IF_ADDREF(*aTracker);
  return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::GetElementById(const nsString& aId, nsIDOMElement** aReturn)
{
    nsresult rv;

    nsCOMPtr<nsIContent> element;
    rv = mElementMap.FindFirst(aId, getter_AddRefs(element));
    if (NS_FAILED(rv)) return rv;

    if (element) {
        rv = element->QueryInterface(NS_GET_IID(nsIDOMElement), (void**) aReturn);
    }
    else {
        *aReturn = nsnull;
        rv = NS_OK;
    }

    return rv;
}

nsresult
XULDocumentImpl::AddSubtreeToDocument(nsIContent* aElement)
{
    // Do a bunch of work that's necessary when an element gets added
    // to the XUL Document.
    nsresult rv;

    // 1. Add the element to the resource-to-element map
    rv = AddElementToMap(aElement);
    if (NS_FAILED(rv)) return rv;

    // 2. If the element is a 'command updater' (i.e., has a
    // "commandupdater='true'" attribute), then add the element to the
    // document's command dispatcher
    nsAutoString value;
    rv = aElement->GetAttribute(kNameSpaceID_None, kCommandUpdaterAtom, value);
    if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && value.Equals("true")) {
        rv = gXULUtils->SetCommandUpdater(this, aElement);
        if (NS_FAILED(rv)) return rv;
    }

    PRInt32 count;
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
    rv = xulcontent ? xulcontent->PeekChildCount(count) : aElement->ChildCount(count);
    if (NS_FAILED(rv)) return rv;

    while (--count >= 0) {
        nsCOMPtr<nsIContent> child;
        rv = aElement->ChildAt(count, *getter_AddRefs(child));
        if (NS_FAILED(rv)) return rv;

        rv = AddSubtreeToDocument(child);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

nsresult
XULDocumentImpl::RemoveSubtreeFromDocument(nsIContent* aElement)
{
    // Do a bunch of cleanup to remove an element from the XUL
    // document.
    nsresult rv;

    PRInt32 count;
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
    rv = xulcontent ? xulcontent->PeekChildCount(count) : aElement->ChildCount(count);
    if (NS_FAILED(rv)) return rv;

    while (--count >= 0) {
        nsCOMPtr<nsIContent> child;
        rv = aElement->ChildAt(count, *getter_AddRefs(child));
        if (NS_FAILED(rv)) return rv;

        rv = RemoveSubtreeFromDocument(child);
        if (NS_FAILED(rv)) return rv;
    }

    // 1. Remove the element from the resource-to-element map
    rv = RemoveElementFromMap(aElement);
    if (NS_FAILED(rv)) return rv;

    // 2. If the element is a 'command updater', then remove the
    // element from the document's command dispatcher.
    nsAutoString value;
    rv = aElement->GetAttribute(kNameSpaceID_None, kCommandUpdaterAtom, value);
    if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && value.Equals("true")) {
        nsCOMPtr<nsIDOMElement> domelement = do_QueryInterface(aElement);
        NS_ASSERTION(domelement != nsnull, "not a DOM element");
        if (! domelement)
            return NS_ERROR_UNEXPECTED;

        rv = mCommandDispatcher->RemoveCommandUpdater(domelement);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

// Attributes that are used with getElementById() and the
// resource-to-element map.
nsIAtom** XULDocumentImpl::kIdentityAttrs[] = { &kIdAtom, &kRefAtom, nsnull };

nsresult
XULDocumentImpl::AddElementToMap(nsIContent* aElement)
{
    // Look at the element's 'id' and 'ref' attributes, and if set,
    // add pointers in the resource-to-element map to the element.
    nsresult rv;

    for (PRInt32 i = 0; kIdentityAttrs[i] != nsnull; ++i) {
        nsAutoString value;
        rv = aElement->GetAttribute(kNameSpaceID_None, *kIdentityAttrs[i], value);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get attribute");
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            rv = mElementMap.Add(value, aElement);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


nsresult
XULDocumentImpl::RemoveElementFromMap(nsIContent* aElement)
{
    // Remove the element from the resource-to-element map.
    nsresult rv;

    for (PRInt32 i = 0; kIdentityAttrs[i] != nsnull; ++i) {
        nsAutoString value;
        rv = aElement->GetAttribute(kNameSpaceID_None, *kIdentityAttrs[i], value);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get attribute");
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            rv = mElementMap.Remove(value, aElement);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


PRIntn
XULDocumentImpl::RemoveElementsFromMapByContent(const nsString& aID,
                                                nsIContent* aElement,
                                                void* aClosure)
{
    nsIContent* content = NS_REINTERPRET_CAST(nsIContent*, aClosure);
    return (aElement == content) ? HT_ENUMERATE_REMOVE : HT_ENUMERATE_NEXT;
}



////////////////////////////////////////////////////////////////////////
// nsIXULParentDocument interface
NS_IMETHODIMP 
XULDocumentImpl::GetContentViewerContainer(nsIContentViewerContainer** aContainer)
{
    NS_PRECONDITION ( aContainer, "Null Parameter into GetContentViewerContainer" );
    
    *aContainer = mContentViewerContainer;
    NS_IF_ADDREF(*aContainer);

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
XULDocumentImpl::SetContentSink(nsIXULContentSink* aParentContentSink)
{
    mParentContentSink = aParentContentSink;
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::GetContentSink(nsIXULContentSink** aParentContentSink)
{
    NS_IF_ADDREF(mParentContentSink);
    *aParentContentSink = mParentContentSink;
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
            nsIDOMNode* domNode = nsnull;
            rv = mRootContent->QueryInterface(nsIDOMNode::GetIID(), (void**)&domNode);
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
                if (NS_OK == global->QueryInterface(NS_GET_IID(nsIJSScriptObject), (void **)&window)) {
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
    nsresult rv;

    if (JSVAL_IS_STRING(aID)) {
        char* s = JS_GetStringBytes(JS_ValueToString(aContext, aID));
        if (PL_strcmp("title", s) == 0) {
            nsAutoString title("get me out of aVp somehow");
            for (PRInt32 i = mPresShells.Count() - 1; i >= 0; --i) {
                nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);
                nsCOMPtr<nsIPresContext> context;
                rv = shell->GetPresContext(getter_AddRefs(context));
                if (NS_FAILED(rv)) return PR_FALSE;

                nsCOMPtr<nsISupports> container;
                rv = context->GetContainer(getter_AddRefs(container));
                if (NS_FAILED(rv)) return PR_FALSE;

                if (! container) continue;

                nsCOMPtr<nsIWebShell> webshell = do_QueryInterface(container);
                if (! webshell) continue;

                rv = webshell->SetTitle(title.GetUnicode());
                if (NS_FAILED(rv)) return PR_FALSE;
            }
        }
        else if (PL_strcmp("location", s) == 0) {
            NS_NOTYETIMPLEMENTED("write me");
            return PR_FALSE;
        }
    }
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
    if (! mAttrStyleSheet) {
        return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
    }
    else {
        NS_ADDREF(*aResult);
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
    if (!mInlineStyleSheet) {
        return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
    }
    else {
        NS_ADDREF(*aResult);
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

    PRInt32 i;
    for(i = 0; i < count; i++) {
        nsIContent* child;
        aStartNode->ChildAt(i, child);
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
XULDocumentImpl::Init(void)
{
    nsresult rv;

    rv = NS_NewHeapArena(getter_AddRefs(mArena), nsnull);
    if (NS_FAILED(rv)) return rv;

    // Create a namespace manager so we can manage tags
    rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                            nsnull,
                                            NS_GET_IID(nsINameSpaceManager),
                                            getter_AddRefs(mNameSpaceManager));
    if (NS_FAILED(rv)) return rv;

    // Create our focus tracker and hook it up.
    nsCOMPtr<nsIXULCommandDispatcher> commandDis;
    rv = nsComponentManager::CreateInstance(kXULCommandDispatcherCID,
                                            nsnull,
                                            NS_GET_IID(nsIXULCommandDispatcher),
                                            getter_AddRefs(commandDis));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a focus tracker");
    if (NS_FAILED(rv)) return rv;

    mCommandDispatcher = do_QueryInterface(commandDis);

    nsCOMPtr<nsIDOMEventListener> CommandDispatcher =
        do_QueryInterface(mCommandDispatcher);

    if (CommandDispatcher) {
      // Take the focus tracker and add it as an event listener for focus and blur events.
        AddEventListener("focus", CommandDispatcher, PR_TRUE);
        AddEventListener("blur", CommandDispatcher, PR_TRUE);
    }

    // Get the local store. Yeah, I know. I wish GetService() used a
    // 'void**', too.
    nsIRDFDataSource* localstore;
    rv = nsServiceManager::GetService(kLocalStoreCID,
                                      NS_GET_IID(nsIRDFDataSource),
                                      (nsISupports**) &localstore);

    // this _could_ fail; e.g., if we've tried to grab the local store
    // before profiles have initialized. If so, no big deal; nothing
    // will persist.

    if (NS_SUCCEEDED(rv)) {
        mLocalStore = localstore;
        NS_IF_RELEASE(localstore);
    }

#if 0
    // construct a selection object
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRangeListCID,
                                                    nsnull,
                                                    kIDOMSelectionIID,
                                                    (void**) &mSelection))) {
        NS_ERROR("unable to create DOM selection");
    }
#endif

    if (gRefCnt++ == 0) {
        kCommandUpdaterAtom             = NS_NewAtom("commandupdater");
        kIdAtom                         = NS_NewAtom("id");
        kObservesAtom                   = NS_NewAtom("observes");
        kOpenAtom                       = NS_NewAtom("open");
        kPersistAtom                    = NS_NewAtom("persist");
        kRefAtom                        = NS_NewAtom("ref");
        kRuleAtom                       = NS_NewAtom("rule");
        kTemplateAtom                   = NS_NewAtom("template");

        // Keep the RDF service cached in a member variable to make using
        // it a bit less painful
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          NS_GET_IID(nsIRDFService),
                                          (nsISupports**) &gRDFService);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF Service");
        if (NS_FAILED(rv)) return rv;

        gRDFService->GetResource(NC_NAMESPACE_URI "persist",   &kNC_persist);
        gRDFService->GetResource(NC_NAMESPACE_URI "attribute", &kNC_attribute);
        gRDFService->GetResource(NC_NAMESPACE_URI "value",     &kNC_value);

        rv = nsComponentManager::CreateInstance(kHTMLElementFactoryCID,
                                                nsnull,
                                                NS_GET_IID(nsIHTMLElementFactory),
                                                (void**) &gHTMLElementFactory);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get HTML element factory");
        if (NS_FAILED(rv)) return rv;

        rv = nsServiceManager::GetService(kNameSpaceManagerCID,
                                          NS_GET_IID(nsINameSpaceManager),
                                          (nsISupports**) &gNameSpaceManager);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get namespace manager");
        if (NS_FAILED(rv)) return rv;

#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
static const char kXULNameSpaceURI[] = XUL_NAMESPACE_URI;
        gNameSpaceManager->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);


        rv = nsServiceManager::GetService(kXULContentUtilsCID,
                                          NS_GET_IID(nsIXULContentUtils),
                                          (nsISupports**) &gXULUtils);
        if (NS_FAILED(rv)) return rv;
    }

#ifdef PR_LOGGING
    if (! gXULLog)
        gXULLog = PR_NewLogModule("nsXULDocument");
#endif

    return NS_OK;
}



nsresult
XULDocumentImpl::StartLayout(void)
{
    NS_PRECONDITION(mRootContent != nsnull, "Error in XUL file. Love to tell ya where if only I knew.");
    if (!mRootContent)
      return NS_ERROR_UNEXPECTED;
    
    PRInt32 count = GetNumberOfShells();
    for (PRInt32 i = 0; i < count; i++) {
      nsIPresShell* shell = GetShellAt(i);
      if (nsnull == shell)
          continue;

      // Resize-reflow this time
      nsCOMPtr<nsIPresContext> cx;
      shell->GetPresContext(getter_AddRefs(cx));

      PRBool intrinsic = PR_FALSE;
      nsCOMPtr<nsIWebShell> webShell;
      nsCOMPtr<nsIBrowserWindow> browser;

		  if (cx) {
			  nsCOMPtr<nsISupports> container;
			  cx->GetContainer(getter_AddRefs(container));
			  if (container) {
			    webShell = do_QueryInterface(container);
			    if (webShell) {
					  webShell->SetScrolling(NS_STYLE_OVERFLOW_HIDDEN);
            nsCOMPtr<nsIWebShellContainer> webShellContainer;
            webShell->GetContainer(*getter_AddRefs(webShellContainer));
            if (webShellContainer) {
              browser = do_QueryInterface(webShellContainer);
              if (browser)
                browser->IsIntrinsicallySized(intrinsic);
            }
			    }
			  }
		  }
        
		  nsRect r;
      cx->GetVisibleArea(r);
      if (intrinsic) {
        // Flow at an unconstrained width and height
        r.width = NS_UNCONSTRAINEDSIZE;
        r.height = NS_UNCONSTRAINEDSIZE;
      }

      if (browser) {
        // We're top-level.
        // See if we have attributes on our root tag that set the width and height.
        // read "height" attribute// Convert r.width and r.height to twips.
        float p2t;
        cx->GetPixelsToTwips(&p2t);
        
        nsCOMPtr<nsIDOMElement> windowElement = do_QueryInterface(mRootContent);
        nsString sizeString;
        PRInt32 specSize;
        PRInt32 errorCode;
        if (NS_SUCCEEDED(windowElement->GetAttribute("height", sizeString))) {
          specSize = sizeString.ToInteger(&errorCode);
          if (NS_SUCCEEDED(errorCode) && specSize > 0)
            r.height = NSIntPixelsToTwips(specSize, p2t);
        }

        // read "width" attribute
        if (NS_SUCCEEDED(windowElement->GetAttribute("width", sizeString))) {
          specSize = sizeString.ToInteger(&errorCode);
          if (NS_SUCCEEDED(errorCode) || specSize > 0)
            r.width = NSIntPixelsToTwips(specSize, p2t);
        }
      }

      cx->SetVisibleArea(r);
      
      // XXX Copy of the code below. See XXX below for details...
      // Now trigger a refresh
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        nsCOMPtr<nsIContentViewer> contentViewer;
        nsresult rv = webShell->GetContentViewer(getter_AddRefs(contentViewer));
        if (NS_SUCCEEDED(rv) && (contentViewer != nsnull)) {
          PRBool enabled;
          contentViewer->GetEnableRendering(&enabled);
          if (enabled) {
            vm->EnableRefresh();
          }
        }
      }

      shell->InitialReflow(r.width, r.height);

      if (browser) {
        // We're top level.
        // Retrieve the answer.
        cx->GetVisibleArea(r);

        // Perform the resize
        PRInt32 chromeX,chromeY,chromeWidth,chromeHeight;
        webShell->GetBounds(chromeX,chromeY,chromeWidth,chromeHeight);

        float t2p;
        cx->GetTwipsToPixels(&t2p);
        PRInt32 width = PRInt32((float)r.width*t2p);
        PRInt32 height = PRInt32((float)r.height*t2p);
      
        PRInt32 widthDelta = width - chromeWidth;
        PRInt32 heightDelta = height - chromeHeight;

        nsRect windowBounds;
        browser->GetWindowBounds(windowBounds);
        browser->SizeWindowTo(windowBounds.width + widthDelta, 
                              windowBounds.height + heightDelta);
      }
      
      // XXX Moving this call up before the call to InitialReflow(), because
      // the view manager's UpdateView() function is dropping dirty rects if
      // refresh is disabled rather than accumulating them until refresh is
      // enabled and then triggering a repaint...
#if 0
      // Now trigger a refresh
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        nsCOMPtr<nsIContentViewer> contentViewer;
        nsresult rv = webShell->GetContentViewer(getter_AddRefs(contentViewer));
        if (NS_SUCCEEDED(rv) && (contentViewer != nsnull)) {
          PRBool enabled;
          contentViewer->GetEnableRendering(&enabled);
          if (enabled) {
            vm->EnableRefresh();
          }
        }
      }
#endif
 
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

static char kNameSpaceSeparator = ':';

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
    PRInt32 nsoffset = name.FindChar(kNameSpaceSeparator);
    if (-1 != nsoffset) {
        name.Left(prefix, nsoffset);
        name.Cut(0, nsoffset+1);
    }

    // Figure out the namespace ID
    nsCOMPtr<nsIAtom> nameSpaceAtom;
    if (0 < prefix.Length())
        nameSpaceAtom = getter_AddRefs(NS_NewAtom(prefix));

    rv = ns->FindNameSpaceID(nameSpaceAtom, aNameSpaceID);
    if (NS_FAILED(rv)) return rv;

    aName = NS_NewAtom(name);
    return NS_OK;
}


// nsIDOMEventCapturer and nsIDOMEventReceiver Interface Implementations

NS_IMETHODIMP
XULDocumentImpl::AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    nsIEventListenerManager *manager;

    if (NS_OK == GetListenerManager(&manager)) {
        manager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
        NS_RELEASE(manager);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
XULDocumentImpl::RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    if (mListenerManager) {
        mListenerManager->RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
XULDocumentImpl::AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                 PRBool aUseCapture)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    manager->AddEventListenerByType(aListener, aType, flags);
    NS_RELEASE(manager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
XULDocumentImpl::RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                    PRBool aUseCapture)
{
  if (mListenerManager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    mListenerManager->RemoveEventListenerByType(aListener, aType, flags);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
XULDocumentImpl::GetListenerManager(nsIEventListenerManager** aResult)
{
    if (! mListenerManager) {
        nsresult rv;
        rv = nsComponentManager::CreateInstance(kEventListenerManagerCID,
                                                nsnull,
                                                NS_GET_IID(nsIEventListenerManager),
                                                getter_AddRefs(mListenerManager));

        if (NS_FAILED(rv)) return rv;
    }
    *aResult = mListenerManager;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::GetNewListenerManager(nsIEventListenerManager **aResult)
{
    return nsComponentManager::CreateInstance(kEventListenerManagerCID,
                                        nsnull,
                                        NS_GET_IID(nsIEventListenerManager),
                                        (void**) aResult);
}

nsresult
XULDocumentImpl::CaptureEvent(const nsString& aType)
{
  nsIEventListenerManager *mManager;

  if (NS_OK == GetListenerManager(&mManager)) {
    //mManager->CaptureEvent(aListener);
    NS_RELEASE(mManager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
XULDocumentImpl::ReleaseEvent(const nsString& aType)
{
  if (mListenerManager) {
    //mListenerManager->ReleaseEvent(aListener);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
XULDocumentImpl::OpenWidgetItem(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    if (! mBuilders)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
        nsCOMPtr<nsIAtom> tag;
        rv = aElement->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tagStr;
        tag->ToString(tagStr);

        PR_LOG(gXULLog, PR_LOG_DEBUG,
               ("xuldoc open-widget-item %s",
                (const char*) nsCAutoString(tagStr)));
    }
#endif

    PRUint32 cnt = 0;
    rv = mBuilders->Count(&cnt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
    for (PRUint32 i = 0; i < cnt; ++i) {
        // XXX we should QueryInterface() here
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        NS_ASSERTION(builder != nsnull, "null ptr");
        if (! builder)
            continue;

        rv = builder->OpenContainer(aElement);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error opening container");
        // XXX ignore error code?

        NS_RELEASE(builder);
    }

    return NS_OK;
}

nsresult
XULDocumentImpl::CloseWidgetItem(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    if (! mBuilders)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
        nsCOMPtr<nsIAtom> tag;
        rv = aElement->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tagStr;
        tag->ToString(tagStr);

        PR_LOG(gXULLog, PR_LOG_DEBUG,
               ("xuldoc close-widget-item %s",
                (const char*) nsCAutoString(tagStr)));
    }
#endif

    PRUint32 cnt = 0;
    rv = mBuilders->Count(&cnt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
    for (PRUint32 i = 0; i < cnt; ++i) {
        // XXX we should QueryInterface() here
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        NS_ASSERTION(builder != nsnull, "null ptr");
        if (! builder)
            continue;

        rv = builder->CloseContainer(aElement);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error closing container");
        // XXX ignore error code?

        NS_RELEASE(builder);
    }

    return NS_OK;
}


nsresult
XULDocumentImpl::RebuildWidgetItem(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    if (! mBuilders)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
        nsCOMPtr<nsIAtom> tag;
        rv = aElement->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tagStr;
        tag->ToString(tagStr);

        PR_LOG(gXULLog, PR_LOG_DEBUG,
               ("xuldoc close-widget-item %s",
                (const char*) nsCAutoString(tagStr)));
    }
#endif

    PRUint32 cnt = 0;
    rv = mBuilders->Count(&cnt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
    for (PRUint32 i = 0; i < cnt; ++i) {
        // XXX we should QueryInterface() here
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        NS_ASSERTION(builder != nsnull, "null ptr");
        if (! builder)
            continue;

        rv = builder->RebuildContainer(aElement);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error rebuilding container");
        // XXX ignore error code?

        NS_RELEASE(builder);
    }

    return NS_OK;
}


nsresult
XULDocumentImpl::CreateElement(PRInt32 aNameSpaceID,
                               nsIAtom* aTag,
                               nsIContent** aResult)
{
    nsresult rv;
    nsCOMPtr<nsIContent> result;

    if (aNameSpaceID == kNameSpaceID_HTML) {
        nsCOMPtr<nsIHTMLContent> element;
        const PRUnichar *tagName;
        aTag->GetUnicode(&tagName);

        rv = gHTMLElementFactory->CreateInstanceByTag(tagName, getter_AddRefs(element));
        if (NS_FAILED(rv)) return rv;

        result = do_QueryInterface(element);
        if (! result)
            return NS_ERROR_UNEXPECTED;
    }
    else {
        rv = nsXULElement::Create(aNameSpaceID, aTag, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }

    rv = result->SetDocument(this, PR_FALSE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set element's document");
    if (NS_FAILED(rv)) return rv;

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}


nsresult
XULDocumentImpl::ApplyPersistentAttributes()
{
    // Add all of the 'persisted' attributes into the content
    // model.
    if (! mLocalStore)
        return NS_OK;

    nsresult rv;
    nsCOMPtr<nsISupportsArray> array;

    nsXPIDLCString docurl;
    rv = mDocumentURL->GetSpec(getter_Copies(docurl));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> doc;
    rv = gRDFService->GetResource(docurl, getter_AddRefs(doc));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISimpleEnumerator> elements;
    rv = mLocalStore->GetTargets(doc, kNC_persist, PR_TRUE, getter_AddRefs(elements));
    if (NS_FAILED(rv)) return rv;

    while (1) {
        PRBool hasmore;
        rv = elements->HasMoreElements(&hasmore);
        if (NS_FAILED(rv)) return rv;

        if (! hasmore)
            break;

        if (! array) {
            rv = NS_NewISupportsArray(getter_AddRefs(array));
            if (NS_FAILED(rv)) return rv;
        }

        nsCOMPtr<nsISupports> isupports;
        rv = elements->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(isupports);
        if (! resource) {
            NS_WARNING("expected element to be a resource");
            continue;
        }

        const char* uri;
        rv = resource->GetValueConst(&uri);
        if (NS_FAILED(rv)) return rv;

        nsAutoString id;
        rv = gXULUtils->MakeElementID(this, nsAutoString(uri), id);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to compute element ID");
        if (NS_FAILED(rv)) return rv;

        rv = GetElementsForID(id, array);
        if (NS_FAILED(rv)) return rv;

        PRUint32 cnt;
        rv = array->Count(&cnt);
        if (NS_FAILED(rv)) return rv;

        if (! cnt)
            continue;

        rv = ApplyPersistentAttributesToElements(resource, array);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
XULDocumentImpl::ApplyPersistentAttributesToElements(nsIRDFResource* aResource, nsISupportsArray* aElements)
{
    nsresult rv;

    nsCOMPtr<nsISimpleEnumerator> attrs;
    rv = mLocalStore->ArcLabelsOut(aResource, getter_AddRefs(attrs));
    if (NS_FAILED(rv)) return rv;

    while (1) {
        PRBool hasmore;
        rv = attrs->HasMoreElements(&hasmore);
        if (NS_FAILED(rv)) return rv;

        if (! hasmore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = attrs->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);
        if (! property) {
            NS_WARNING("expected a resource");
            continue;
        }

        const char* attrname;
        rv = property->GetValueConst(&attrname);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIAtom> attr = dont_AddRef(NS_NewAtom(attrname));
        if (! attr)
            return NS_ERROR_OUT_OF_MEMORY;

        // XXX could hang namespace off here, as well...

        nsCOMPtr<nsIRDFNode> node;
        rv = mLocalStore->GetTarget(aResource, property, PR_TRUE, getter_AddRefs(node));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(node);
        if (! literal) {
            NS_WARNING("expected a literal");
            continue;
        }

        const PRUnichar* value;
        rv = literal->GetValueConst(&value);
        if (NS_FAILED(rv)) return rv;

        PRInt32 len = nsCRT::strlen(value);
        CBufDescriptor wrapper(value, PR_TRUE, len + 1, len);

        PRUint32 cnt;
        rv = aElements->Count(&cnt);
        if (NS_FAILED(rv)) return rv;
           
        for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
            nsISupports* isupports2 = aElements->ElementAt(i);
            if (! isupports2)
                continue;

            nsCOMPtr<nsIContent> element = do_QueryInterface(isupports2);
            NS_RELEASE(isupports2);

            rv = element->SetAttribute(/* XXX */ kNameSpaceID_None,
                                       attr,
                                       nsAutoString(wrapper),
                                       PR_FALSE);
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetChannel(nsIChannel **aResult)
{
  *aResult = mChannel;
  NS_ADDREF(*aResult);
  return NS_OK;
}