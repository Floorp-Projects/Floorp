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

#include "nsIArena.h"
#include "nsICollection.h"
#include "nsIContent.h"
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsIDTD.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIEnumerator.h"
#include "nsIHTMLStyleSheet.h"
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
#include "nsIRDFObserver.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLDataSource.h"
#include "nsIScriptContextOwner.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsIURLGroup.h"
#include "nsIWebShell.h"
#include "nsLayoutCID.h"
#include "nsParserCIID.h"
#include "nsRDFCID.h"
#include "nsRDFContentSink.h"
#include "nsVoidArray.h"
#include "plhash.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kICSSParserIID,          NS_ICSS_PARSER_IID); // XXX grr..
static NS_DEFINE_IID(kICollectionIID,         NS_ICOLLECTION_IID);
static NS_DEFINE_IID(kIContentIID,            NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDTDIID,                NS_IDTD_IID);
static NS_DEFINE_IID(kIDocumentIID,           NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIHTMLStyleSheetIID,     NS_IHTML_STYLE_SHEET_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,   NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIParserIID,             NS_IPARSER_IID);
static NS_DEFINE_IID(kIPresShellIID,          NS_IPRESSHELL_IID);
static NS_DEFINE_IID(kIRDFCompositeDataSourceIID, NS_IRDFCOMPOSITEDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,      NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFDocumentIID,        NS_IRDFDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,         NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFResourceIID,        NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,         NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIRDFXMLDataSourceIID,   NS_IRDFXMLDATASOURCE_IID);
static NS_DEFINE_IID(kIStreamListenerIID,     NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIStreamObserverIID,     NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellIID,           NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIXMLDocumentIID,        NS_IXMLDOCUMENT_IID);

static NS_DEFINE_CID(kCSSParserCID,             NS_CSSPARSER_CID);
static NS_DEFINE_CID(kHTMLStyleSheetCID,        NS_HTMLSTYLESHEET_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,      NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kParserCID,                NS_PARSER_IID); // XXX
static NS_DEFINE_CID(kPresShellCID,             NS_PRESSHELL_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,      NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRangeListCID,             NS_RANGELIST_CID);
static NS_DEFINE_CID(kWellFormedDTDCID,         NS_WELLFORMEDDTD_CID);

////////////////////////////////////////////////////////////////////////

static PLHashNumber
rdf_HashPointer(const void* key)
{
    return (PLHashNumber) key;
}


////////////////////////////////////////////////////////////////////////

class RDFDocumentImpl : public nsIDocument,
                        public nsIRDFDocument,
                        public nsIRDFObserver,
                        public nsIRDFXMLDataSourceObserver
{
public:
    RDFDocumentImpl();
    virtual ~RDFDocumentImpl();

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

    NS_IMETHOD GetSelection(nsICollection** aSelection);

    NS_IMETHOD SelectAll();

    NS_IMETHOD FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound);

    virtual void CreateXIF(nsString & aBuffer, nsISelection* aSelection);

    virtual void ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual void BeginConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual void ConvertChildrenToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual void FinishConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual PRBool IsInRange(const nsIContent *aStartContent, const nsIContent* aEndContent, const nsIContent* aContent) const;

    virtual PRBool IsBefore(const nsIContent *aNewContent, const nsIContent* aCurrentContent) const;

    virtual PRBool IsInSelection(nsISelection* aSelection, const nsIContent *aContent) const;

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
    NS_IMETHOD Init(nsIRDFContentModelBuilder* aBuilder);
    NS_IMETHOD SetRootResource(nsIRDFResource* resource);
    NS_IMETHOD GetDataBase(nsIRDFCompositeDataSource*& result);
    NS_IMETHOD CreateChildren(nsIRDFContent* element);
    NS_IMETHOD AddTreeProperty(nsIRDFResource* resource);
    NS_IMETHOD RemoveTreeProperty(nsIRDFResource* resource);
    NS_IMETHOD IsTreeProperty(nsIRDFResource* aProperty, PRBool* aResult) const;
    NS_IMETHOD MapResource(nsIRDFResource* aResource, nsIRDFContent* aContent);
    NS_IMETHOD UnMapResource(nsIRDFResource* aResource, nsIRDFContent* aContent);
    NS_IMETHOD SplitProperty(nsIRDFResource* aResource, PRInt32* aNameSpaceID, nsIAtom** aTag);

    // nsIRDFObserver interface
    NS_IMETHOD OnAssert(nsIRDFResource* subject,
                        nsIRDFResource* predicate,
                        nsIRDFNode* object);

    NS_IMETHOD OnUnassert(nsIRDFResource* subject,
                          nsIRDFResource* predicate,
                          nsIRDFNode* object);

    // nsIRDFXMLDataSourceObserver interface
    NS_IMETHOD OnBeginLoad(nsIRDFXMLDataSource* aDataSource);
    NS_IMETHOD OnInterrupt(nsIRDFXMLDataSource* aDataSource);
    NS_IMETHOD OnResume(nsIRDFXMLDataSource* aDataSource);
    NS_IMETHOD OnEndLoad(nsIRDFXMLDataSource* aDataSource);

    NS_IMETHOD OnRootResourceFound(nsIRDFXMLDataSource* aDataSource, nsIRDFResource* aResource);
    NS_IMETHOD OnCSSStyleSheetAdded(nsIRDFXMLDataSource* aDataSource, nsIURL* aStyleSheetURI);
    NS_IMETHOD OnNamedDataSourceAdded(nsIRDFXMLDataSource* aDataSource, const char* aNamedDataSourceURI);

    // Implementation methods
    nsresult StartLayout(void);

protected:
    nsIContent*
    FindContent(const nsIContent* aStartNode,
                const nsIContent* aTest1,
                const nsIContent* aTest2) const;

    nsresult
    LoadCSSStyleSheet(nsIURL* url);

    nsresult
    AddNamedDataSource(const char* uri);


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
    nsINameSpaceManager*   mNameSpaceManager;
    nsIStyleSheet*         mAttrStyleSheet;
    nsIRDFCompositeDataSource*        mDB;
    nsIRDFService*         mRDFService;
    nsISupportsArray*      mTreeProperties;
    nsIRDFContentModelBuilder* mBuilder;
    PLHashTable*           mResources;
    nsIRDFDataSource*      mLocalDataSource;
    nsIRDFXMLDataSource*   mDocumentDataSource;
};


////////////////////////////////////////////////////////////////////////
// DummyListener
//
//   This is a _total_ hack that is used to get stuff to draw right
//   when a second copy is loaded. I need to talk to Guha about what
//   the expected behavior should be...
//
class DummyListener : public nsIStreamListener
{
private:
    RDFDocumentImpl* mRDFDocument;
    PRBool mWasNotifiedOnce; // XXX why do we get _two_ OnStart/StopBinding() calls?

public:
    DummyListener(RDFDocumentImpl* aRDFDocument)
        : mRDFDocument(aRDFDocument),
          mWasNotifiedOnce(PR_FALSE)
    {
        NS_INIT_REFCNT();
        NS_ADDREF(mRDFDocument);
    }

    virtual ~DummyListener(void) {
        NS_RELEASE(mRDFDocument);
    }

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIStreamObserver interface
    NS_IMETHOD
    OnStartBinding(nsIURL* aURL, const char *aContentType) {
        if (! mWasNotifiedOnce) {
            mRDFDocument->BeginLoad();
            mRDFDocument->StartLayout();
        }
        return NS_OK;
    }

    NS_IMETHOD
    OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax) {
        return NS_OK;
    }

    NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg) {
        return NS_OK;
    }

    NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg) {
        if (! mWasNotifiedOnce) {
            mRDFDocument->EndLoad();
            mWasNotifiedOnce = PR_TRUE;
        }
        return NS_OK;
    }
    

    // nsIStreamListener interface
    NS_IMETHOD
    GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo) {
        aInfo->seekable = PR_FALSE;
        return NS_OK;
    }

    NS_IMETHOD
    OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength) {
        return NS_OK;
    }
};

NS_IMPL_ADDREF(DummyListener);
NS_IMPL_RELEASE(DummyListener);

NS_IMETHODIMP
DummyListener::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(kIStreamListenerIID) ||
        aIID.Equals(kIStreamObserverIID) ||
        aIID.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsIStreamListener*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }
}

////////////////////////////////////////////////////////////////////////
// ctors & dtors

RDFDocumentImpl::RDFDocumentImpl(void)
    : mArena(nsnull),
      mDocumentURL(nsnull),
      mDocumentURLGroup(nsnull),
      mRootContent(nsnull),
      mParentDocument(nsnull),
      mScriptContextOwner(nsnull),
      mSelection(nsnull),
      mDisplaySelection(PR_FALSE),
      mNameSpaceManager(nsnull),
      mAttrStyleSheet(nsnull),
      mDB(nsnull),
      mRDFService(nsnull),
      mTreeProperties(nsnull),
      mBuilder(nsnull),
      mResources(nsnull),
      mLocalDataSource(nsnull),
      mDocumentDataSource(nsnull)
{
    NS_INIT_REFCNT();

    nsresult rv;

    // construct a selection object
    if (NS_FAILED(rv = nsRepository::CreateInstance(kRangeListCID,
                                                    nsnull,
                                                    kICollectionIID,
                                                    (void**) &mSelection)))
        PR_ASSERT(0);

    
}

RDFDocumentImpl::~RDFDocumentImpl()
{
    if (mDocumentDataSource) {
        mDocumentDataSource->RemoveXMLStreamObserver(this);
        NS_RELEASE(mDocumentDataSource);
    }
    NS_IF_RELEASE(mLocalDataSource);

    if (mResources)
        PL_HashTableDestroy(mResources);

    if (mRDFService) {
        nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);
        mRDFService = nsnull;
    }

    // mParentDocument is never refcounted
    NS_IF_RELEASE(mBuilder);
    NS_IF_RELEASE(mSelection);
    NS_IF_RELEASE(mScriptContextOwner);
    NS_IF_RELEASE(mAttrStyleSheet);
    NS_IF_RELEASE(mTreeProperties);
    NS_IF_RELEASE(mRootContent);
    if (mDB) {
        mDB->RemoveObserver(this);
        NS_RELEASE(mDB);
    }
    NS_IF_RELEASE(mDocumentURLGroup);
    NS_IF_RELEASE(mDocumentURL);
    NS_IF_RELEASE(mArena);
    NS_IF_RELEASE(mNameSpaceManager);
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMETHODIMP 
RDFDocumentImpl::QueryInterface(REFNSIID iid, void** result)
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
    return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(RDFDocumentImpl);
NS_IMPL_RELEASE(RDFDocumentImpl);

////////////////////////////////////////////////////////////////////////
// nsIDocument interface

nsIArena*
RDFDocumentImpl::GetArena()
{
    NS_IF_ADDREF(mArena);
    return mArena;
}

NS_IMETHODIMP 
RDFDocumentImpl::StartDocumentLoad(nsIURL *aURL, 
                                   nsIContentViewerContainer* aContainer,
                                   nsIStreamListener **aDocListener,
                                   const char* aCommand)
{
    nsresult rv;

    // Delete references to style sheets - this should be done in superclass...
    PRInt32 index = mStyleSheets.Count();
    while (--index >= 0) {
        nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(index);
        sheet->SetOwningDocument(nsnull);
        NS_RELEASE(sheet);
    }
    mStyleSheets.Clear();

    NS_IF_RELEASE(mDocumentURL);
    NS_IF_RELEASE(mDocumentURLGroup);
    mDocumentTitle.Truncate();

    mDocumentURL = aURL;
    NS_ADDREF(aURL);

    (void)aURL->GetURLGroup(&mDocumentURLGroup);

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

    if (NS_FAILED(rv))
        return rv;
      
    // Create a "scratch" in-memory data store to associate with the
    // document to be a catch-all for any doc-specific info that we
    // need to store (e.g., current sort order, etc.)
    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFInMemoryDataSourceCID,
                                                    nsnull,
                                                    kIRDFDataSourceIID,
                                                    (void**) &mLocalDataSource)))
        return rv;

    if (NS_FAILED(rv = mDB->AddDataSource(mLocalDataSource)))
        return rv;

    const char* uri;
    if (NS_FAILED(rv = aURL->GetSpec(&uri)))
        return rv;

    // Now load the actual XML/RDF document data source.
    nsIRDFDataSource* ds;
    if (NS_SUCCEEDED(rv = mRDFService->GetDataSource(uri, &ds))) {
        if (NS_FAILED(rv = ds->QueryInterface(kIRDFXMLDataSourceIID, (void**) &mDocumentDataSource))) {
            NS_RELEASE(ds);
            return rv;
        }

        NS_RELEASE(ds);

        if (NS_FAILED(rv = mDB->AddDataSource(mDocumentDataSource)))
            return rv;

        // we found the data source already loaded locally. Load it's
        // style sheets and attempt to include any named data sources
        // that it references into this document.
        nsIURL** styleSheetURLs;
        PRInt32 count;
        if (NS_SUCCEEDED(rv = mDocumentDataSource->GetCSSStyleSheetURLs(&styleSheetURLs, &count))) {
            for (PRInt32 i = 0; i < count; ++i) {
                if (NS_FAILED(rv = LoadCSSStyleSheet(styleSheetURLs[i]))) {
                    NS_ASSERTION(PR_FALSE, "couldn't load style sheet");
                }
            }
        }

        const char* const* namedDataSourceURIs;
        if (NS_SUCCEEDED(rv = mDocumentDataSource->GetNamedDataSourceURIs(&namedDataSourceURIs, &count))) {
            for (PRInt32 i = 0; i < count; ++i) {
                if (NS_FAILED(rv = AddNamedDataSource(namedDataSourceURIs[i]))) {
#ifdef DEBUG
                    printf("error adding named data source %s\n", namedDataSourceURIs[i]);
#endif
                }
            }
        }

        nsIRDFResource* root;
        if (NS_SUCCEEDED(rv = mDocumentDataSource->GetRootResource(&root))) {
            SetRootResource(root);
            StartLayout();
        }

        // XXX Allright, this is an atrocious hack. Basically, we
        // construct a dummy listener object so that we can load the
        // URL, which allows us to receive StartLayout() and EndLoad()
        // calls asynchronously. If we don't do this, then there is no
        // way (that I could figure out) to force the content model to
        // be traversed so that a document is laid out again.
        //
        // Looking at the "big picture" here, the real problem is that
        // the "registered" data sources mechanism is really just a
        // cache of stream data sources. It's not really clear what
        // should happen when somebody opens a second document on the
        // same source. You'd kinda like both to refer to the same
        // thing, so changes to one are immediately reflected in the
        // other. On the other hand, you'd also like to be able to
        // _unload_ and reload a content model, say by doing
        // "shift+reload". _So_, that kinda ties into the real cache,
        // etc. etc.
        //
        // What I guess I'm saying is, maybe it doesn't make that much
        // sense to register stream data sources when they're
        // created...I dunno...
        if (aDocListener) {
            nsIStreamListener* lsnr = new DummyListener(this);
            if (! lsnr)
                return NS_ERROR_OUT_OF_MEMORY;

            NS_ADDREF(lsnr);
            *aDocListener = lsnr;

            if (NS_FAILED(rv = NS_OpenURL(aURL, lsnr)))
                return rv;
        }
    }
    else if (NS_SUCCEEDED(rv = nsRepository::CreateInstance(kRDFXMLDataSourceCID,
                                                            nsnull,
                                                            kIRDFXMLDataSourceIID,
                                                            (void**) &mDocumentDataSource))) {
        if (NS_FAILED(rv = mDB->AddDataSource(mDocumentDataSource)))
            return rv;

        // We need to construct a new stream and load it. The stream
        // will automagically register itself as a named data source,
        // so if subsequent docs ask for it, they'll get the real
        // deal. In the meantime, add us as an
        // nsIRDFXMLDataSourceObserver so that we'll be notified when we
        // need to load style sheets, etc.

        nsIRDFXMLDataSource* doc;
        if (NS_SUCCEEDED(rv = mDocumentDataSource->QueryInterface(kIRDFXMLDataSourceIID, (void**) &doc))) {
            doc->AddXMLStreamObserver(this);
            NS_RELEASE(doc);
        }

        if (NS_FAILED(rv = mDocumentDataSource->Init(uri)))
            return rv;

        if (aDocListener) {
            *aDocListener = nsnull;
        }
    }
    else {
        // an error occurred
        PR_ASSERT(0);
    }

    return NS_OK;
}

const nsString*
RDFDocumentImpl::GetDocumentTitle() const
{
    return &mDocumentTitle;
}

nsIURL* 
RDFDocumentImpl::GetDocumentURL() const
{
    NS_IF_ADDREF(mDocumentURL);
    return mDocumentURL;
}

nsIURLGroup* 
RDFDocumentImpl::GetDocumentURLGroup() const
{
    NS_IF_ADDREF(mDocumentURLGroup);
    return mDocumentURLGroup;
}

NS_IMETHODIMP 
RDFDocumentImpl::GetBaseURL(nsIURL*& aURL) const
{
    NS_IF_ADDREF(mDocumentURL);
    aURL = mDocumentURL;
    return NS_OK;
}

nsCharSetID 
RDFDocumentImpl::GetDocumentCharacterSet() const
{
    return mCharSetID;
}

void 
RDFDocumentImpl::SetDocumentCharacterSet(nsCharSetID aCharSetID)
{
    mCharSetID = aCharSetID;
}

nsresult 
RDFDocumentImpl::CreateShell(nsIPresContext* aContext,
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
RDFDocumentImpl::DeleteShell(nsIPresShell* aShell)
{
    return mPresShells.RemoveElement(aShell);
}

PRInt32 
RDFDocumentImpl::GetNumberOfShells()
{
    return mPresShells.Count();
}

nsIPresShell* 
RDFDocumentImpl::GetShellAt(PRInt32 aIndex)
{
    nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[aIndex]);
    NS_IF_ADDREF(shell);
    return shell;
}

nsIDocument* 
RDFDocumentImpl::GetParentDocument()
{
    NS_IF_ADDREF(mParentDocument);
    return mParentDocument;
}

void 
RDFDocumentImpl::SetParentDocument(nsIDocument* aParent)
{
    // Note that we do *not* AddRef our parent because that would
    // create a circular reference.
    mParentDocument = aParent;
}

void 
RDFDocumentImpl::AddSubDocument(nsIDocument* aSubDoc)
{
    // we don't do subdocs.
    PR_ASSERT(0);
}

PRInt32 
RDFDocumentImpl::GetNumberOfSubDocuments()
{
    return 0;
}

nsIDocument* 
RDFDocumentImpl::GetSubDocumentAt(PRInt32 aIndex)
{
    // we don't do subdocs.
    PR_ASSERT(0);
    return nsnull;
}

nsIContent* 
RDFDocumentImpl::GetRootContent()
{
    NS_IF_ADDREF(mRootContent);
    return mRootContent;
}

void 
RDFDocumentImpl::SetRootContent(nsIContent* aRoot)
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
RDFDocumentImpl::GetNumberOfStyleSheets()
{
    return mStyleSheets.Count();
}

nsIStyleSheet* 
RDFDocumentImpl::GetStyleSheetAt(PRInt32 aIndex)
{
    nsIStyleSheet* sheet = NS_STATIC_CAST(nsIStyleSheet*, mStyleSheets[aIndex]);
    NS_IF_ADDREF(sheet);
    return sheet;
}

void 
RDFDocumentImpl::AddStyleSheet(nsIStyleSheet* aSheet)
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
            nsIStyleSet* set = shell->GetStyleSet();
            if (nsnull != set) {
                set->InsertDocStyleSheetBefore(aSheet, nsnull); // put it first
                NS_RELEASE(set);
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
RDFDocumentImpl::SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
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
            nsIStyleSet* set = shell->GetStyleSet();
            if (nsnull != set) {
                if (aDisabled) {
                    set->RemoveDocStyleSheet(aSheet);
                }
                else {
                    set->InsertDocStyleSheetBefore(aSheet, nsnull);  // put it first
                }
                NS_RELEASE(set);
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
RDFDocumentImpl::GetScriptContextOwner()
{
    NS_IF_ADDREF(mScriptContextOwner);
    return mScriptContextOwner;
}

void 
RDFDocumentImpl::SetScriptContextOwner(nsIScriptContextOwner *aScriptContextOwner)
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
RDFDocumentImpl::GetNameSpaceManager(nsINameSpaceManager*& aManager)
{
  aManager = mNameSpaceManager;
  NS_IF_ADDREF(aManager);
  return NS_OK;
}


// Note: We don't hold a reference to the document observer; we assume
// that it has a live reference to the document.
void 
RDFDocumentImpl::AddObserver(nsIDocumentObserver* aObserver)
{
    // XXX Make sure the observer isn't already in the list
    if (mObservers.IndexOf(aObserver) == -1) {
        mObservers.AppendElement(aObserver);
    }
}

PRBool 
RDFDocumentImpl::RemoveObserver(nsIDocumentObserver* aObserver)
{
    return mObservers.RemoveElement(aObserver);
}

NS_IMETHODIMP 
RDFDocumentImpl::BeginLoad()
{
    PRInt32 i, count = mObservers.Count();
    for (i = 0; i < count; i++) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->BeginLoad(this);
    }
    return NS_OK;
}

NS_IMETHODIMP 
RDFDocumentImpl::EndLoad()
{
    PRInt32 i, count = mObservers.Count();
    for (i = 0; i < count; i++) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->EndLoad(this);
    }
    return NS_OK;
}


NS_IMETHODIMP 
RDFDocumentImpl::ContentChanged(nsIContent* aContent,
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
RDFDocumentImpl::AttributeChanged(nsIContent* aChild,
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
RDFDocumentImpl::ContentAppended(nsIContent* aContainer,
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
RDFDocumentImpl::ContentInserted(nsIContent* aContainer,
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
RDFDocumentImpl::ContentReplaced(nsIContent* aContainer,
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
RDFDocumentImpl::ContentRemoved(nsIContent* aContainer,
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
RDFDocumentImpl::StyleRuleChanged(nsIStyleSheet* aStyleSheet,
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
RDFDocumentImpl::StyleRuleAdded(nsIStyleSheet* aStyleSheet,
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
RDFDocumentImpl::StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
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
RDFDocumentImpl::GetSelection(nsICollection** aSelection)
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
RDFDocumentImpl::SelectAll()
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
RDFDocumentImpl::FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
    aIsFound = PR_FALSE;
    return NS_ERROR_FAILURE;
}

void 
RDFDocumentImpl::CreateXIF(nsString & aBuffer, nsISelection* aSelection)
{
    PR_ASSERT(0);
}

void 
RDFDocumentImpl::ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

void 
RDFDocumentImpl::BeginConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

void 
RDFDocumentImpl::ConvertChildrenToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

void 
RDFDocumentImpl::FinishConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

PRBool 
RDFDocumentImpl::IsInRange(const nsIContent *aStartContent, const nsIContent* aEndContent, const nsIContent* aContent) const
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
RDFDocumentImpl::IsBefore(const nsIContent *aNewContent, const nsIContent* aCurrentContent) const
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
RDFDocumentImpl::IsInSelection(nsISelection* aSelection, const nsIContent *aContent) const
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
RDFDocumentImpl::GetPrevContent(const nsIContent *aContent) const
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
RDFDocumentImpl::GetNextContent(const nsIContent *aContent) const
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
RDFDocumentImpl::SetDisplaySelection(PRBool aToggle)
{
    mDisplaySelection = aToggle;
}

PRBool 
RDFDocumentImpl::GetDisplaySelection() const
{
    return mDisplaySelection;
}

NS_IMETHODIMP 
RDFDocumentImpl::HandleDOMEvent(nsIPresContext& aPresContext, 
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
RDFDocumentImpl::PrologElementAt(PRUint32 aOffset, nsIContent** aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFDocumentImpl::PrologCount(PRUint32* aCount)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFDocumentImpl::AppendToProlog(nsIContent* aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
RDFDocumentImpl::EpilogElementAt(PRUint32 aOffset, nsIContent** aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFDocumentImpl::EpilogCount(PRUint32* aCount)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFDocumentImpl::AppendToEpilog(nsIContent* aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFDocument interface

NS_IMETHODIMP
RDFDocumentImpl::Init(nsIRDFContentModelBuilder* aBuilder)
{
    NS_PRECONDITION(aBuilder != nsnull, "null ptr");
    if (! aBuilder)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    NS_ADDREF(aBuilder);
    mBuilder = aBuilder;

    if (NS_FAILED(rv = NS_NewHeapArena(&mArena, nsnull)))
        return rv;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kNameSpaceManagerCID,
                                                    nsnull,
                                                    kINameSpaceManagerIID,
                                                    (void**) &mNameSpaceManager)))
        return rv;

    static PRInt32 kInitialResourceTableSize = 1023;

    if ((mResources = PL_NewHashTable(kInitialResourceTableSize,
                                      rdf_HashPointer,
                                      PL_CompareValues,
                                      PL_CompareValues,
                                      nsnull,
                                      nsnull)) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFCompositeDataSourceCID,
                                                    nsnull,
                                                    kIRDFCompositeDataSourceIID,
                                                    (void**) &mDB)))
        return rv;

    if (NS_FAILED(rv = mDB->AddObserver(this)))
        return rv;

    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &mRDFService)))
        return rv;

    // The builder may try to ask us for our DB, so do this last...
    if (NS_FAILED(rv = mBuilder->SetDocument(this)))
        return rv;

    return NS_OK;
}

NS_IMETHODIMP
RDFDocumentImpl::SetRootResource(nsIRDFResource* aResource)
{
    NS_ASSERTION(mBuilder != nsnull, "not initialized");
    if (! mBuilder)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = mBuilder->CreateRoot(aResource);
    return rv;
}

NS_IMETHODIMP
RDFDocumentImpl::GetDataBase(nsIRDFCompositeDataSource*& result)
{
    NS_PRECONDITION(mDB != nsnull, "not initialized");
    if (! mDB)
        return NS_ERROR_NOT_INITIALIZED;

    result = mDB;
    NS_ADDREF(result);

    return NS_OK;
}


// XXX It may make sense to factor the implementation of this method
// into the content model builder: maybe the content model builder can
// do a more efficient implementation?

NS_IMETHODIMP
RDFDocumentImpl::CreateChildren(nsIRDFContent* element)
{
    nsresult rv;

    NS_ASSERTION(mDB, "not initialized");
    if (! mDB)
        return NS_ERROR_NOT_INITIALIZED;

    NS_ASSERTION(mRDFService, "not initialized");
    if (! mRDFService)
        return NS_ERROR_NOT_INITIALIZED;


    nsIRDFResource* resource = nsnull;
    nsIRDFArcsOutCursor* properties = nsnull;

    if (NS_FAILED(rv = element->GetResource(resource)))
        goto done;

    // Create a cursor that'll enumerate all of the outbound arcs
    //
    // XXX This is really horrendous and could use a good does of
    // special casing. For example, we should treat RDF containers
    // specially and _not_ enumerate each of the arcs.
    if (NS_FAILED(rv = mDB->ArcLabelsOut(resource, &properties)))
        goto done;

    while (NS_SUCCEEDED(rv = properties->Advance())) {
        nsIRDFResource* property = nsnull;

        if (NS_FAILED(rv = properties->GetValue((nsIRDFNode**)&property)))
            break;

#ifdef DEBUG
        {
            const char* s;
            property->GetValue(&s);
        }
#endif

        // Create a second cursor that'll enumerate all of the values
        // for all of the arcs.
        nsIRDFAssertionCursor* assertions;
        if (NS_FAILED(rv = mDB->GetTargets(resource, property, PR_TRUE, &assertions))) {
            NS_RELEASE(property);
            break;
        }

        while (NS_SUCCEEDED(rv = assertions->Advance())) {
            nsIRDFNode* value;
            if (NS_SUCCEEDED(rv = assertions->GetValue((nsIRDFNode**)&value))) {
                // At this point, the specific RDFDocumentImpl
                // implementations will create an appropriate child
                // element (or elements).
                rv = mBuilder->OnAssert(element, property, value);
                NS_RELEASE(value);
            }

            if (NS_FAILED(rv))
                break;
        }

        NS_RELEASE(assertions);
        NS_RELEASE(property);
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY)
        // This is a normal return code from nsIRDFCursor::Advance()
        rv = NS_OK;

done:
    NS_IF_RELEASE(resource);
    NS_IF_RELEASE(properties);

    return rv;
}

NS_IMETHODIMP
RDFDocumentImpl::AddTreeProperty(nsIRDFResource* resource)
{
    nsresult rv;
    if (! mTreeProperties) {
        if (NS_FAILED(rv = NS_NewISupportsArray(&mTreeProperties)))
            return rv;
    }

    // ensure uniqueness
    if (mTreeProperties->IndexOf(resource) != -1) {
        PR_ASSERT(0);
        return NS_OK;
    }

    mTreeProperties->AppendElement(resource);
    return NS_OK;
}


NS_IMETHODIMP
RDFDocumentImpl::RemoveTreeProperty(nsIRDFResource* resource)
{
    if (! mTreeProperties) {
        // XXX no properties have ever been inserted!
        PR_ASSERT(0);
        return NS_OK;
    }

    if (! mTreeProperties->RemoveElement(resource)) {
        // XXX that specific property has never been inserted!
        PR_ASSERT(0);
        return NS_OK;
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFDocumentImpl::IsTreeProperty(nsIRDFResource* aProperty, PRBool* aResult) const
{
#define TREE_PROPERTY_HACK
#if defined(TREE_PROPERTY_HACK)
    const char* p;
    aProperty->GetValue(&p);
    nsAutoString s(p);
    if (s.Equals(NC_NAMESPACE_URI "child") ||
        s.Equals(NC_NAMESPACE_URI "Folder")) {
        *aResult = PR_TRUE;
        return NS_OK;
    }
#endif // defined(TREE_PROPERTY_HACK)
    if (rdf_IsOrdinalProperty(aProperty)) {
        *aResult = PR_TRUE;
        return NS_OK;
    }
    if (! mTreeProperties) {
        *aResult = PR_FALSE;
        return NS_OK;
    }
    if (mTreeProperties->IndexOf(aProperty) == -1) {
        *aResult = PR_FALSE;
        return NS_OK;
    }
    // XXX return "true" by default???
    *aResult = PR_TRUE;
    return NS_OK;
}


NS_IMETHODIMP
RDFDocumentImpl::MapResource(nsIRDFResource* aResource, nsIRDFContent* aContent)
{
    // XXX busted: a resource might be located in multiple content
    // elements in the tree. This needs to map to a set, not a single
    // instance.
    PL_HashTableAdd(mResources, aResource, aContent);
    return NS_OK;
}

NS_IMETHODIMP
RDFDocumentImpl::UnMapResource(nsIRDFResource* aResource, nsIRDFContent* aContent)
{
    // XXX busted: a resource might be located in multiple content
    // elements in the tree. This needs to map to a set, not a single
    // instance.
    PL_HashTableRemove(mResources, aResource);
    return NS_OK;
}

NS_IMETHODIMP
RDFDocumentImpl::SplitProperty(nsIRDFResource* aProperty,
                               PRInt32* aNameSpaceID,
                               nsIAtom** aTag)
{
    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    // XXX okay, this is a total hack. for this to work right, we need
    // to remember what the namespace and the tag were when the
    // property was created, which we don't do...

    const char* p;
    aProperty->GetValue(&p);
    nsAutoString uri(p);

    PRInt32 index;

    if ((index = uri.RFind('#')) < 0) {
        if ((index = uri.RFind('/')) < 0) {
            NS_ASSERTION(PR_FALSE, "make this smarter!");
        }
    }

    nsAutoString tag;
    PRInt32 count = uri.Length() - (index + 1);
    uri.Right(tag, count);
    uri.Cut(index + 1, count);
 
    tag.ToUpperCase();
    *aTag = NS_NewAtom(tag);

    nsresult rv = mNameSpaceManager->GetNameSpaceID(uri, *aNameSpaceID);
    NS_ASSERTION(NS_SUCCEEDED(rv) && *aNameSpaceID != kNameSpaceID_Unknown, "unknown namespace");

    return rv;
}


////////////////////////////////////////////////////////////////////////
// nsIRDFObserver methods

NS_IMETHODIMP
RDFDocumentImpl::OnAssert(nsIRDFResource* subject,
                        nsIRDFResource* predicate,
                        nsIRDFNode* object)
{
    NS_PRECONDITION(mResources != nsnull, "not initialized");
    if (! mResources)
        return NS_ERROR_NOT_INITIALIZED;

    nsIRDFContent* element = (nsIRDFContent*) PL_HashTableLookup(mResources, subject);

    if (! element)
        // it's not in the tree: we're done!
        return NS_OK;

    PRBool hasLiveKids;
    if (NS_SUCCEEDED(element->ChildrenHaveBeenGenerated(hasLiveKids)) && hasLiveKids) {
        // that is, if the children have _already_ been generated,
        // manually add this new one. Otherwise, just ignore it: it'll
        // get generated when someone asks the content model for it.
        mBuilder->OnAssert(element, predicate, object);
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFDocumentImpl::OnUnassert(nsIRDFResource* subject,
                          nsIRDFResource* predicate,
                          nsIRDFNode* object)
{
    NS_PRECONDITION(mResources != nsnull, "not initialized");
    if (! mResources)
        return NS_ERROR_NOT_INITIALIZED;

    nsIRDFContent* element = (nsIRDFContent*) PL_HashTableLookup(mResources, subject);
    if (! element)
        // it's not in the tree: we're done!
        return NS_OK;

    PRBool hasLiveKids;
    if (NS_SUCCEEDED(element->ChildrenHaveBeenGenerated(hasLiveKids)) && hasLiveKids) {
        // that is, if the children have _already_ been generated,
        // manually remove this one. Otherwise, just ignore it: it'll
        // never get generated anyway...
        mBuilder->OnUnassert(element, predicate, object);
    }
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIRDFXMLDataSourceObserver interface

NS_IMETHODIMP
RDFDocumentImpl::OnBeginLoad(nsIRDFXMLDataSource* aDataSource)
{
    return BeginLoad();
}


NS_IMETHODIMP
RDFDocumentImpl::OnInterrupt(nsIRDFXMLDataSource* aDataSource)
{
    // flow any content that we have up until now.
    return NS_OK;
}


NS_IMETHODIMP
RDFDocumentImpl::OnResume(nsIRDFXMLDataSource* aDataSource)
{
    return NS_OK;
}


NS_IMETHODIMP
RDFDocumentImpl::OnEndLoad(nsIRDFXMLDataSource* aDataSource)
{
    return EndLoad();
}



NS_IMETHODIMP
RDFDocumentImpl::OnRootResourceFound(nsIRDFXMLDataSource* aDataSource, nsIRDFResource* aResource)
{
    nsresult rv;
    if (NS_SUCCEEDED(rv = SetRootResource(aResource))) {
        rv = StartLayout();
    }
    return rv;
}


NS_IMETHODIMP
RDFDocumentImpl::OnCSSStyleSheetAdded(nsIRDFXMLDataSource* aDataSource, nsIURL* aStyleSheetURL)
{
    return LoadCSSStyleSheet(aStyleSheetURL);
}

NS_IMETHODIMP
RDFDocumentImpl::OnNamedDataSourceAdded(nsIRDFXMLDataSource* aDataSource, const char* aNamedDataSourceURI)
{
    return AddNamedDataSource(aNamedDataSourceURI);
}

////////////////////////////////////////////////////////////////////////
// Implementation methods

nsIContent*
RDFDocumentImpl::FindContent(const nsIContent* aStartNode,
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
RDFDocumentImpl::LoadCSSStyleSheet(nsIURL* url)
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
RDFDocumentImpl::AddNamedDataSource(const char* uri)
{
    nsresult rv;
    nsIRDFDataSource* ds = nsnull;

    if (NS_FAILED(rv = mRDFService->GetDataSource(uri, &ds)))
        goto done;

    if (NS_FAILED(rv = mDB->AddDataSource(ds)))
        goto done;

done:
    NS_IF_RELEASE(ds);
    return rv;
}


nsresult
RDFDocumentImpl::StartLayout(void)
{
    PRInt32 count = GetNumberOfShells();
    for (PRInt32 i = 0; i < count; i++) {
        nsIPresShell* shell = GetShellAt(i);
        if (nsnull == shell)
            continue;

        // Resize-reflow this time
        nsIPresContext* cx = shell->GetPresContext();
        nsRect r;
        cx->GetVisibleArea(r);
        shell->InitialReflow(r.width, r.height);
        NS_RELEASE(cx);

        // Now trigger a refresh
        nsIViewManager* vm = shell->GetViewManager();
        if (nsnull != vm) {
            vm->EnableRefresh();
            NS_RELEASE(vm);
        }

        // Start observing the document _after_ we do the initial
        // reflow. Otherwise, we'll get into an trouble trying to
        // creat kids before the root frame is established.
        shell->BeginObservingDocument();

        NS_RELEASE(shell);
    }
    return NS_OK;
}



////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFDocument(nsIRDFDocument** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    RDFDocumentImpl* doc = new RDFDocumentImpl();
    if (! doc)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(doc);
    *result = doc;
    return NS_OK;
}

