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
 * Original Author(s):
 *   Chris Waterson <waterson@netscape.com>
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Ben Goodger <ben@netscape.com>
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
#include "nsXULDocument.h"

#include "nsDOMCID.h"
#include "nsDOMError.h"
#include "nsIChromeRegistry.h"
#include "nsIComponentManager.h"
#include "nsICodebasePrincipal.h"
#include "nsIContentSink.h" // for NS_CONTENT_ID_COUNTER_BASE
#include "nsIContentViewer.h"
#include "nsICSSStyleSheet.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIDOMText.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMAbstractView.h"
#include "nsIDTD.h"
#include "nsIDocumentObserver.h"
#include "nsIFormControl.h"
#include "nsIHTMLContent.h"
#include "nsIElementFactory.h"
#include "nsIEventStateManager.h"
#include "nsIInputStream.h"
#include "nsILoadGroup.h"
#include "nsINameSpace.h"
#include "nsIObserver.h"
#include "nsIParser.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFNode.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIStyleContext.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsITextContent.h"
#include "nsITimer.h"
#include "nsIURL.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
#include "nsIXMLContent.h"
#include "nsIXULContent.h"
#include "nsIXULContentSink.h"
#include "nsIXULContentUtils.h"
#include "nsIXULKeyListener.h"
#include "nsIXULPrototypeCache.h"
#include "nsLWBrkCIID.h"
#include "nsLayoutCID.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "nsRDFCID.h"
#include "nsRDFDOMNodeList.h"
#include "nsXPIDLString.h"
#include "nsIDOMWindow.h"
#include "nsXULCommandDispatcher.h"
#include "nsXULDocument.h"
#include "nsXULElement.h"
#include "plstr.h"
#include "prlog.h"
#include "rdf.h"
#include "rdfutil.h"
#include "nsIFrame.h"
#include "nsIPrivateDOMImplementation.h"
#include "nsIDOMDOMImplementation.h"
#include "nsINodeInfo.h"
#include "nsIDOMDocumentType.h"
#include "nsIXBLService.h"

#include "nsIXIFConverter.h"

// Used for the temporary DOM Level2 hack
#include "nsIPref.h"
static PRBool kStrictDOMLevel2 = PR_FALSE;


//----------------------------------------------------------------------
//
// CIDs
//

static NS_DEFINE_CID(kCSSLoaderCID,              NS_CSS_LOADER_CID);
static NS_DEFINE_CID(kCSSParserCID,              NS_CSSPARSER_CID);
static NS_DEFINE_CID(kChromeRegistryCID,         NS_CHROMEREGISTRY_CID);
static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_CID(kEventListenerManagerCID,   NS_EVENTLISTENERMANAGER_CID);
static NS_DEFINE_CID(kHTMLCSSStyleSheetCID,      NS_HTML_CSS_STYLESHEET_CID);
static NS_DEFINE_CID(kHTMLElementFactoryCID,     NS_HTML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kHTMLStyleSheetCID,         NS_HTMLSTYLESHEET_CID);
static NS_DEFINE_CID(kLWBrkCID,                  NS_LWBRK_CID);
static NS_DEFINE_CID(kLoadGroupCID,              NS_LOADGROUP_CID);
static NS_DEFINE_CID(kLocalStoreCID,             NS_LOCALSTORE_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,       NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kParserCID,                 NS_PARSER_IID); // XXX
static NS_DEFINE_CID(kPresShellCID,              NS_PRESSHELL_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,      NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,  NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,       NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kTextNodeCID,               NS_TEXTNODE_CID);
static NS_DEFINE_CID(kWellFormedDTDCID,          NS_WELLFORMEDDTD_CID);
static NS_DEFINE_CID(kXMLElementFactoryCID,      NS_XML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kXULContentSinkCID,         NS_XULCONTENTSINK_CID);
static NS_DEFINE_CID(kXULContentUtilsCID,        NS_XULCONTENTUTILS_CID);
static NS_DEFINE_CID(kXULKeyListenerCID,         NS_XULKEYLISTENER_CID);
static NS_DEFINE_CID(kXULPrototypeCacheCID,      NS_XULPROTOTYPECACHE_CID);
static NS_DEFINE_CID(kXULTemplateBuilderCID,     NS_XULTEMPLATEBUILDER_CID);
static NS_DEFINE_CID(kDOMImplementationCID,      NS_DOM_IMPLEMENTATION_CID);
static NS_DEFINE_CID(kXIFConverterCID,           NS_XIFCONVERTER_CID);

static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);

//----------------------------------------------------------------------
//
// Miscellaneous Constants
//

#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"

const nsForwardReference::Phase nsForwardReference::kPasses[] = {
    nsForwardReference::eConstruction,
    nsForwardReference::eHookup,
    nsForwardReference::eDone
};


//----------------------------------------------------------------------
//
// Statics
//

PRInt32 nsXULDocument::gRefCnt = 0;

nsIAtom* nsXULDocument::kAttributeAtom;
nsIAtom* nsXULDocument::kCommandUpdaterAtom;
nsIAtom* nsXULDocument::kContextAtom;
nsIAtom* nsXULDocument::kDataSourcesAtom;
nsIAtom* nsXULDocument::kElementAtom;
nsIAtom* nsXULDocument::kIdAtom;
nsIAtom* nsXULDocument::kKeysetAtom;
nsIAtom* nsXULDocument::kObservesAtom;
nsIAtom* nsXULDocument::kOpenAtom;
nsIAtom* nsXULDocument::kOverlayAtom;
nsIAtom* nsXULDocument::kPersistAtom;
nsIAtom* nsXULDocument::kPositionAtom;
nsIAtom* nsXULDocument::kInsertAfterAtom;
nsIAtom* nsXULDocument::kInsertBeforeAtom;
nsIAtom* nsXULDocument::kPopupAtom;
nsIAtom* nsXULDocument::kRefAtom;
nsIAtom* nsXULDocument::kRuleAtom;
nsIAtom* nsXULDocument::kStyleAtom;
nsIAtom* nsXULDocument::kTemplateAtom;
nsIAtom* nsXULDocument::kTooltipAtom;

nsIAtom* nsXULDocument::kCoalesceAtom;
nsIAtom* nsXULDocument::kAllowNegativesAtom;

nsIRDFService* nsXULDocument::gRDFService;
nsIRDFResource* nsXULDocument::kNC_persist;
nsIRDFResource* nsXULDocument::kNC_attribute;
nsIRDFResource* nsXULDocument::kNC_value;

nsIElementFactory* nsXULDocument::gHTMLElementFactory;
nsIElementFactory*  nsXULDocument::gXMLElementFactory;

nsINameSpaceManager* nsXULDocument::gNameSpaceManager;
PRInt32 nsXULDocument::kNameSpaceID_XUL;

nsIXULContentUtils* nsXULDocument::gXULUtils;
nsIXULPrototypeCache* nsXULDocument::gXULCache;
nsIScriptSecurityManager* nsXULDocument::gScriptSecurityManager;
nsIPrincipal* nsXULDocument::gSystemPrincipal;

PRLogModuleInfo* nsXULDocument::gXULLog;

class nsProxyLoadStream : public nsIInputStream
{
private:
  const char* mBuffer;
  PRUint32    mSize;
  PRUint32    mIndex;

public:
  nsProxyLoadStream(void) : mBuffer(nsnull)
  {
      NS_INIT_REFCNT();
  }

  virtual ~nsProxyLoadStream(void) {
  }

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIBaseStream
  NS_IMETHOD Close(void) {
      return NS_OK;
  }

  // nsIInputStream
  NS_IMETHOD Available(PRUint32 *aLength) {
      *aLength = mSize - mIndex;
      return NS_OK;
  }

  NS_IMETHOD Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount) {
      PRUint32 readCount = 0;
      while (mIndex < mSize && aCount > 0) {
          *aBuf = mBuffer[mIndex];
          aBuf++;
          mIndex++;
          readCount++;
          aCount--;
      }
      *aReadCount = readCount;
      return NS_OK;
  }

  // Implementation
  void SetBuffer(const char* aBuffer, PRUint32 aSize) {
      mBuffer = aBuffer;
      mSize = aSize;
      mIndex = 0;
  }
};

NS_IMPL_ISUPPORTS(nsProxyLoadStream, NS_GET_IID(nsIInputStream));

//----------------------------------------------------------------------
//
// PlaceholderChannel
//
//   This is a dummy channel implementation that we add to the load
//   group. It ensures that EndDocumentLoad() in the docshell doesn't
//   fire before we've finished building the complete document content
//   model.
//

class PlaceholderChannel : public nsIChannel
{
protected:
    PlaceholderChannel();
    virtual ~PlaceholderChannel();

    static PRInt32 gRefCnt;
    static nsIURI* gURI;

    nsCOMPtr<nsILoadGroup> mLoadGroup;

public:
    static nsresult
    Create(nsIChannel** aResult);
	
    NS_DECL_ISUPPORTS

	// nsIRequest
    NS_IMETHOD IsPending(PRBool *_retval) { *_retval = PR_TRUE; return NS_OK; }
    NS_IMETHOD GetStatus(nsresult *status) { *status = NS_OK; return NS_OK; } 
    NS_IMETHOD Cancel(nsresult status)  { return NS_OK; }
    NS_IMETHOD Suspend(void) { return NS_OK; }
    NS_IMETHOD Resume(void)  { return NS_OK; }

	// nsIChannel    
    NS_IMETHOD GetOriginalURI(nsIURI* *aOriginalURI) { *aOriginalURI = gURI; NS_ADDREF(*aOriginalURI); return NS_OK; }
    NS_IMETHOD SetOriginalURI(nsIURI* aOriginalURI) { gURI = aOriginalURI; NS_ADDREF(gURI); return NS_OK; }
    NS_IMETHOD GetURI(nsIURI* *aURI) { *aURI = gURI; NS_ADDREF(*aURI); return NS_OK; }
    NS_IMETHOD SetURI(nsIURI* aURI) { gURI = aURI; NS_ADDREF(gURI); return NS_OK; }
    NS_IMETHOD OpenInputStream(nsIInputStream **_retval) { *_retval = nsnull; return NS_OK; }
	NS_IMETHOD OpenOutputStream(nsIOutputStream **_retval) { *_retval = nsnull; return NS_OK; }
	NS_IMETHOD AsyncOpen(nsIStreamObserver *observer, nsISupports *ctxt) { return NS_OK; }
	NS_IMETHOD AsyncRead(nsIStreamListener *listener, nsISupports *ctxt) { return NS_OK; }
	NS_IMETHOD AsyncWrite(nsIInputStream *fromStream, nsIStreamObserver *observer, nsISupports *ctxt) { return NS_OK; }
	NS_IMETHOD GetLoadAttributes(nsLoadFlags *aLoadAttributes) { *aLoadAttributes = nsIChannel::LOAD_NORMAL; return NS_OK; }
  	NS_IMETHOD SetLoadAttributes(nsLoadFlags aLoadAttributes) { return NS_OK; }
	NS_IMETHOD GetContentType(char * *aContentType) { *aContentType = nsnull; return NS_OK; }
    NS_IMETHOD SetContentType(const char *aContentType) { return NS_OK; }
	NS_IMETHOD GetContentLength(PRInt32 *aContentLength) { *aContentLength = 0; return NS_OK; }
    NS_IMETHOD SetContentLength(PRInt32 aContentLength) { NS_NOTREACHED("SetContentLength"); return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD GetTransferOffset(PRUint32 *aTransferOffset) { NS_NOTREACHED("GetTransferOffset"); return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD SetTransferOffset(PRUint32 aTransferOffset) { NS_NOTREACHED("SetTransferOffset"); return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD GetTransferCount(PRInt32 *aTransferCount) { NS_NOTREACHED("GetTransferCount"); return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD SetTransferCount(PRInt32 aTransferCount) { NS_NOTREACHED("SetTransferCount"); return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD GetBufferSegmentSize(PRUint32 *aBufferSegmentSize) { NS_NOTREACHED("GetBufferSegmentSize"); return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD SetBufferSegmentSize(PRUint32 aBufferSegmentSize) { NS_NOTREACHED("SetBufferSegmentSize"); return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD GetBufferMaxSize(PRUint32 *aBufferMaxSize) { NS_NOTREACHED("GetBufferMaxSize"); return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD SetBufferMaxSize(PRUint32 aBufferMaxSize) { NS_NOTREACHED("SetBufferMaxSize"); return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD GetLocalFile(nsIFile* *result) { NS_NOTREACHED("GetLocalFile"); return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD GetPipeliningAllowed(PRBool *aPipeliningAllowed) { *aPipeliningAllowed = PR_FALSE; return NS_OK; }
    NS_IMETHOD SetPipeliningAllowed(PRBool aPipeliningAllowed) { NS_NOTREACHED("SetPipeliningAllowed"); return NS_ERROR_NOT_IMPLEMENTED; }
	NS_IMETHOD GetOwner(nsISupports * *aOwner) { *aOwner = nsnull; return NS_OK; }
	NS_IMETHOD SetOwner(nsISupports * aOwner) { return NS_OK; }
	NS_IMETHOD GetLoadGroup(nsILoadGroup * *aLoadGroup) { *aLoadGroup = mLoadGroup; NS_IF_ADDREF(*aLoadGroup); return NS_OK; }
	NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup) { mLoadGroup = aLoadGroup; return NS_OK; }
	NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks) { *aNotificationCallbacks = nsnull; return NS_OK; }
	NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks) { return NS_OK; }
    NS_IMETHOD GetSecurityInfo(nsISupports **info) {*info = nsnull; return NS_OK;}
};

PRInt32 PlaceholderChannel::gRefCnt;
nsIURI* PlaceholderChannel::gURI;

NS_IMPL_ADDREF(PlaceholderChannel);
NS_IMPL_RELEASE(PlaceholderChannel);
NS_IMPL_QUERY_INTERFACE2(PlaceholderChannel, nsIRequest, nsIChannel);

nsresult
PlaceholderChannel::Create(nsIChannel** aResult)
{
    PlaceholderChannel* channel = new PlaceholderChannel();
    if (! channel)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = channel;
    NS_ADDREF(*aResult);
    return NS_OK;
}


PlaceholderChannel::PlaceholderChannel()
{
    NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        nsresult rv;
        rv = NS_NewURI(&gURI, "about:xul-master-placeholder", nsnull);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create about:xul-master-placeholder");
    }
}


PlaceholderChannel::~PlaceholderChannel()
{
    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gURI);
    }
}


//----------------------------------------------------------------------
//
// ctors & dtors
//

nsXULDocument::nsXULDocument(void)
    : mParentDocument(nsnull),
      mScriptGlobalObject(nsnull),
      mScriptObject(nsnull),
      mNextSrcLoadWaiter(nsnull),
      mDisplaySelection(PR_FALSE),
      mIsKeyBindingDoc(PR_FALSE),
      mIsPopup(PR_FALSE),
      mResolutionPhase(nsForwardReference::eStart),
      mNextContentID(NS_CONTENT_ID_COUNTER_BASE),
      mState(eState_Master),
      mCurrentScriptProto(nsnull)
{
    NS_INIT_REFCNT();
    mCharSetID.AssignWithConversion("UTF-8");

// Temporary hack that tells if some new DOM Level 2 features are on or off
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
    if (NS_SUCCEEDED(rv)) {
        prefs->GetBoolPref("temp.DOMLevel2update.enabled", &kStrictDOMLevel2);
    }
// End of temp hack.
}

nsXULDocument::~nsXULDocument()
{
    NS_ASSERTION(mNextSrcLoadWaiter == nsnull,
        "unreferenced document still waiting for script source to load?");

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
        NS_IF_RELEASE(kAttributeAtom);
        NS_IF_RELEASE(kCommandUpdaterAtom);
        NS_IF_RELEASE(kContextAtom);
        NS_IF_RELEASE(kDataSourcesAtom);
        NS_IF_RELEASE(kElementAtom);
        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kKeysetAtom);
        NS_IF_RELEASE(kObservesAtom);
        NS_IF_RELEASE(kOpenAtom);
        NS_IF_RELEASE(kOverlayAtom);
        NS_IF_RELEASE(kPersistAtom);
        NS_IF_RELEASE(kPositionAtom);
        NS_IF_RELEASE(kInsertAfterAtom);
        NS_IF_RELEASE(kInsertBeforeAtom);
        NS_IF_RELEASE(kPopupAtom);
        NS_IF_RELEASE(kRefAtom);
        NS_IF_RELEASE(kRuleAtom);
        NS_IF_RELEASE(kStyleAtom);
        NS_IF_RELEASE(kTemplateAtom);
        NS_IF_RELEASE(kTooltipAtom);

	NS_IF_RELEASE(kCoalesceAtom);
	NS_IF_RELEASE(kAllowNegativesAtom);

        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }

        NS_IF_RELEASE(kNC_persist);
        NS_IF_RELEASE(kNC_attribute);
        NS_IF_RELEASE(kNC_value);

        NS_IF_RELEASE(gHTMLElementFactory);
        NS_IF_RELEASE(gXMLElementFactory);

        if (gNameSpaceManager) {
            nsServiceManager::ReleaseService(kNameSpaceManagerCID, gNameSpaceManager);
            gNameSpaceManager = nsnull;
        }

        if (gXULUtils) {
            nsServiceManager::ReleaseService(kXULContentUtilsCID, gXULUtils);
            gXULUtils = nsnull;
        }

        if (gXULCache) {
            nsServiceManager::ReleaseService(kXULPrototypeCacheCID, gXULCache);
            gXULCache = nsnull;
        }

        if (gScriptSecurityManager) {
            nsServiceManager::ReleaseService(NS_SCRIPTSECURITYMANAGER_PROGID, gScriptSecurityManager);
            gScriptSecurityManager = nsnull;
        }

        NS_IF_RELEASE(gSystemPrincipal);
    }
}


nsresult
NS_NewXULDocument(nsIXULDocument** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    nsXULDocument* doc = new nsXULDocument();
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


//----------------------------------------------------------------------
//
// nsISupports interface
//

NS_IMETHODIMP
nsXULDocument::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(NS_GET_IID(nsIDocument)) ||
        iid.Equals(NS_GET_IID(nsISupports))) {
        *result = NS_STATIC_CAST(nsIDocument*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIXULDocument)) ||
             iid.Equals(NS_GET_IID(nsIXMLDocument))) {
        *result = NS_STATIC_CAST(nsIXULDocument*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMXULDocument)) ||
             iid.Equals(NS_GET_IID(nsIDOMDocument)) ||
             iid.Equals(NS_GET_IID(nsIDOMNode))) {
        *result = NS_STATIC_CAST(nsIDOMXULDocument*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMNSDocument))) {
        *result = NS_STATIC_CAST(nsIDOMNSDocument*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMDocumentEvent))) {
        *result = NS_STATIC_CAST(nsIDOMDocumentEvent*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMDocumentView))) {
        *result = NS_STATIC_CAST(nsIDOMDocumentView*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMDocumentXBL))) {
        *result = NS_STATIC_CAST(nsIDOMDocumentXBL*, this);
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
    else if (iid.Equals(NS_GET_IID(nsIStreamLoadableDocument))) {
        *result = NS_STATIC_CAST(nsIStreamLoadableDocument*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsISupportsWeakReference))) {
        *result = NS_STATIC_CAST(nsISupportsWeakReference*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIStreamLoaderObserver))) {
        *result = NS_STATIC_CAST(nsIStreamLoaderObserver*, this);
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }
    NS_ADDREF(this);
    return NS_OK;
}

NS_IMPL_ADDREF(nsXULDocument);
NS_IMPL_RELEASE(nsXULDocument);

//----------------------------------------------------------------------
//
// nsIDocument interface
//

nsIArena*
nsXULDocument::GetArena()
{
    nsIArena* result = mArena;
    NS_IF_ADDREF(result);
    return result;
}

NS_IMETHODIMP
nsXULDocument::GetContentType(nsString& aContentType) const
{
    aContentType.AssignWithConversion("text/xul");
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::PrepareStyleSheets(nsIURI* anURL)
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

NS_IMETHODIMP
nsXULDocument::SetIsKeybindingDocument(PRBool aIsKeyBindingDoc)
{
    mIsKeyBindingDoc = aIsKeyBindingDoc;
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetDocumentURL(nsIURI* anURL)
{
    mDocumentURL = dont_QueryInterface(anURL);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::StartDocumentLoad(const char* aCommand,
                                 nsIChannel* aChannel,
                                 nsILoadGroup* aLoadGroup,
                                 nsISupports* aContainer,
                                 nsIStreamListener **aDocListener,
                                 PRBool aReset)
{
    nsresult rv;
    mCommand.AssignWithConversion(aCommand);

    mDocumentLoadGroup = getter_AddRefs(NS_GetWeakReference(aLoadGroup));

    mDocumentTitle.Truncate();

    rv = aChannel->GetOriginalURI(getter_AddRefs(mDocumentURL));
    if (NS_FAILED(rv)) return rv;

    rv = PrepareStyleSheets(mDocumentURL);
    if (NS_FAILED(rv)) return rv;

    // Get the content type, if possible, to see if it's a cached XUL
    // load. We explicitly ignore failure at this point, because
    // certain hacks (cough, the directory viewer) need to be able to
    // StartDocumentLoad() before the channel's content type has been
    // detected.
    nsXPIDLCString contentType;
    (void) aChannel->GetContentType(getter_Copies(contentType));

    if (contentType && PL_strcmp(contentType, "text/cached-xul") == 0) {
        // Look in the chrome cache: we've got this puppy loaded
        // already.
        nsCOMPtr<nsIXULPrototypeDocument> proto;
        rv = gXULCache->GetPrototype(mDocumentURL, getter_AddRefs(proto));
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(proto != nsnull, "no prototype on cached load");
        if (! proto)
            return NS_ERROR_UNEXPECTED;

        mMasterPrototype = mCurrentPrototype = proto;

        rv = AddPrototypeSheets();
        if (NS_FAILED(rv)) return rv;

        *aDocListener = new CachedChromeStreamListener(this);
        if (! *aDocListener)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    else {
        // It's just a vanilla document load. Create a parser to deal
        // with the stream n' stuff.
        nsCOMPtr<nsIParser> parser;
        rv = PrepareToLoad(aContainer, aCommand, aChannel, aLoadGroup, getter_AddRefs(parser));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(parser, &rv);
        NS_ASSERTION(NS_SUCCEEDED(rv), "parser doesn't support nsIStreamListener");
        if (NS_FAILED(rv)) return rv;

        *aDocListener = listener;

        parser->Parse(mDocumentURL);
    }

    NS_IF_ADDREF(*aDocListener);
    return NS_OK;
}

NS_IMETHODIMP 
nsXULDocument::StopDocumentLoad()
{
    return NS_OK;
}

const nsString*
nsXULDocument::GetDocumentTitle() const
{
    return &mDocumentTitle;
}

nsIURI*
nsXULDocument::GetDocumentURL() const
{
    nsIURI* result = mDocumentURL;
    NS_IF_ADDREF(result);
    return result;
}

NS_IMETHODIMP
nsXULDocument::GetPrincipal(nsIPrincipal **aPrincipal)
{
    return mMasterPrototype->GetDocumentPrincipal(aPrincipal);
}

NS_IMETHODIMP
nsXULDocument::AddPrincipal(nsIPrincipal *aPrincipal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::GetDocumentLoadGroup(nsILoadGroup **aGroup) const
{
    nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);

    *aGroup = group;
    NS_IF_ADDREF(*aGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetBaseURL(nsIURI*& aURL) const
{
    aURL = mDocumentURL;
    NS_IF_ADDREF(aURL);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetDocumentCharacterSet(nsString& oCharSetID)
{
    oCharSetID = mCharSetID;
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetDocumentCharacterSet(const nsString& aCharSetID)
{
  if (mCharSetID != aCharSetID) {
    mCharSetID = aCharSetID;
    nsAutoString charSetTopic;
    charSetTopic.AssignWithConversion("charset");
    PRInt32 n = mCharSetObservers.Count();
    for (PRInt32 i = 0; i < n; i++) {
      nsIObserver* observer = (nsIObserver*) mCharSetObservers.ElementAt(i);
      observer->Observe((nsIDocument*) this, charSetTopic.GetUnicode(),
                        aCharSetID.GetUnicode());
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::AddCharSetObserver(nsIObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  NS_ENSURE_TRUE(mCharSetObservers.AppendElement(aObserver), NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::RemoveCharSetObserver(nsIObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  NS_ENSURE_TRUE(mCharSetObservers.RemoveElement(aObserver), NS_ERROR_FAILURE);
  return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetLineBreaker(nsILineBreaker** aResult)
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
      nsAutoString lbarg;
      result = lf->GetBreaker(lbarg, &lb);
      if(NS_SUCCEEDED(result)) {
         mLineBreaker = dont_AddRef(lb);
      }
      result = nsServiceManager::ReleaseService(kLWBrkCID, lf);
     }
  }
  *aResult = mLineBreaker;
  NS_IF_ADDREF(*aResult);
  return NS_OK; // XXX we should do error handling here
}

NS_IMETHODIMP
nsXULDocument::SetLineBreaker(nsILineBreaker* aLineBreaker)
{
  mLineBreaker = dont_QueryInterface(aLineBreaker);
  return NS_OK;
}
NS_IMETHODIMP
nsXULDocument::GetWordBreaker(nsIWordBreaker** aResult)
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
      nsAutoString lbarg;
      result = lf->GetBreaker(lbarg, &lb);
      if(NS_SUCCEEDED(result)) {
         mWordBreaker = dont_AddRef(lb);
      }
      result = nsServiceManager::ReleaseService(kLWBrkCID, lf);
     }
  }
  *aResult = mWordBreaker;
  NS_IF_ADDREF(*aResult);
  return NS_OK; // XXX we should do error handling here
}

NS_IMETHODIMP
nsXULDocument::SetWordBreaker(nsIWordBreaker* aWordBreaker)
{
  mWordBreaker = dont_QueryInterface(aWordBreaker);
  return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetHeaderData(nsIAtom* aHeaderField, nsString& aData) const
{
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument:: SetHeaderData(nsIAtom* aHeaderField, const nsString& aData)
{
  return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::CreateShell(nsIPresContext* aContext,
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
nsXULDocument::DeleteShell(nsIPresShell* aShell)
{
    return mPresShells.RemoveElement(aShell);
}

PRInt32
nsXULDocument::GetNumberOfShells()
{
    return mPresShells.Count();
}

nsIPresShell*
nsXULDocument::GetShellAt(PRInt32 aIndex)
{
    nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[aIndex]);
    NS_IF_ADDREF(shell);
    return shell;
}

nsIDocument*
nsXULDocument::GetParentDocument()
{
    NS_IF_ADDREF(mParentDocument);
    return mParentDocument;
}

void
nsXULDocument::SetParentDocument(nsIDocument* aParent)
{
    // Note that we do *not* AddRef our parent because that would
    // create a circular reference.
    mParentDocument = aParent;
}

void
nsXULDocument::AddSubDocument(nsIDocument* aSubDoc)
{
    NS_ADDREF(aSubDoc);
    mSubDocuments.AppendElement(aSubDoc);
}

PRInt32
nsXULDocument::GetNumberOfSubDocuments()
{
    return mSubDocuments.Count();
}

nsIDocument*
nsXULDocument::GetSubDocumentAt(PRInt32 aIndex)
{
    nsIDocument* doc = (nsIDocument*) mSubDocuments.ElementAt(aIndex);
    if (nsnull != doc) {
        NS_ADDREF(doc);
    }
    return doc;
}

nsIContent*
nsXULDocument::GetRootContent()
{
    nsIContent* result = mRootContent;
    NS_IF_ADDREF(result);
    return result;
}

void
nsXULDocument::SetRootContent(nsIContent* aRoot)
{
    if (mRootContent) {
        mRootContent->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    }
    mRootContent = aRoot;
    if (mRootContent) {
        mRootContent->SetDocument(this, PR_TRUE, PR_TRUE);
    }
}

NS_IMETHODIMP
nsXULDocument::AppendToProlog(nsIContent* aContent)
{
    NS_NOTREACHED("nsXULDocument::AppendToProlog");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::AppendToEpilog(nsIContent* aContent)
{
    NS_NOTREACHED("nsXULDocument::AppendToEpilog");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
    NS_NOTREACHED("nsXULDocument::ChildAt");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const
{
    NS_NOTREACHED("nsXULDocument::IndexOf");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::GetChildCount(PRInt32& aCount)
{
    NS_NOTREACHED("nsXULDocument::GetChildCount");
    return NS_ERROR_NOT_IMPLEMENTED;
}

PRInt32
nsXULDocument::GetNumberOfStyleSheets()
{
    return mStyleSheets.Count();
}

nsIStyleSheet*
nsXULDocument::GetStyleSheetAt(PRInt32 aIndex)
{
    nsIStyleSheet* sheet = NS_STATIC_CAST(nsIStyleSheet*, mStyleSheets[aIndex]);
    NS_IF_ADDREF(sheet);
    return sheet;
}

PRInt32
nsXULDocument::GetIndexOfStyleSheet(nsIStyleSheet* aSheet)
{
  return mStyleSheets.IndexOf(aSheet);
}

void 
nsXULDocument::AddStyleSheetToStyleSets(nsIStyleSheet* aSheet)
{
  PRInt32 count = mPresShells.Count();
  PRInt32 index;
  for (index = 0; index < count; index++) {
    nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(index);
    nsCOMPtr<nsIStyleSet> set;
    if (NS_SUCCEEDED(shell->GetStyleSet(getter_AddRefs(set)))) {
      if (set) {
        set->AddDocStyleSheet(aSheet, this);
      }
    }
  }
}

void
nsXULDocument::AddStyleSheet(nsIStyleSheet* aSheet)
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
        AddStyleSheetToStyleSets(aSheet);

        // XXX should observers be notified for disabled sheets??? I think not, but I could be wrong
        for (PRInt32 i = 0; i < mObservers.Count(); i++) {
            nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(i);
            observer->StyleSheetAdded(this, aSheet);
            if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
              i--;
            }
        }
    }
}

NS_IMETHODIMP
nsXULDocument::UpdateStyleSheets(nsISupportsArray* aOldSheets, nsISupportsArray* aNewSheets)
{
  PRUint32 oldCount;
  aOldSheets->Count(&oldCount);
  nsCOMPtr<nsIStyleSheet> sheet;
  PRUint32 i;
  for (i = 0; i < oldCount; i++) {
    nsCOMPtr<nsISupports> supp;
    aOldSheets->GetElementAt(i, getter_AddRefs(supp));
    sheet = do_QueryInterface(supp);
    if (sheet) {
      mStyleSheets.RemoveElement(sheet);
      PRBool enabled = PR_TRUE;
      sheet->GetEnabled(enabled);
      if (enabled) {
        RemoveStyleSheetFromStyleSets(sheet);
      }

      sheet->SetOwningDocument(nsnull);
      nsIStyleSheet* sheetPtr = sheet.get();
      NS_RELEASE(sheetPtr);
    }
  }

  PRUint32 newCount;
  aNewSheets->Count(&newCount);
  for (i = 0; i < newCount; i++) {
    nsCOMPtr<nsISupports> supp;
    aNewSheets->GetElementAt(i, getter_AddRefs(supp));
    sheet = do_QueryInterface(supp);
    if (sheet) {
      if (sheet == mAttrStyleSheet.get()) {  // always first
        mStyleSheets.InsertElementAt(sheet, 0);
      }
      else if (sheet == (nsIHTMLCSSStyleSheet*)mInlineStyleSheet) {  // always last
        mStyleSheets.AppendElement(sheet);
      }
      else {
        if ((nsIHTMLCSSStyleSheet*)mInlineStyleSheet == mStyleSheets.ElementAt(mStyleSheets.Count() - 1)) {
          // keep attr sheet last
          mStyleSheets.InsertElementAt(sheet, mStyleSheets.Count() - 1);
        }
        else {
          mStyleSheets.AppendElement(sheet);
        }
      }
      
      nsIStyleSheet* sheetPtr = sheet;
      NS_ADDREF(sheetPtr);
      sheet->SetOwningDocument(this);

      PRBool enabled = PR_TRUE;
      sheet->GetEnabled(enabled);
      if (enabled) {
        AddStyleSheetToStyleSets(sheet);
        sheet->SetOwningDocument(nsnull);
      }
    }
  }

  for (PRInt32 index = 0; index < mObservers.Count(); index++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
    observer->StyleSheetRemoved(this, sheet);
    if (observer != (nsIDocumentObserver*)mObservers.ElementAt(index)) {
      index--;
    }
  }

  return NS_OK;
}

void 
nsXULDocument::RemoveStyleSheetFromStyleSets(nsIStyleSheet* aSheet)
{
  PRInt32 count = mPresShells.Count();
  PRInt32 index;
  for (index = 0; index < count; index++) {
    nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(index);
    nsCOMPtr<nsIStyleSet> set;
    if (NS_SUCCEEDED(shell->GetStyleSet(getter_AddRefs(set)))) {
      if (set) {
        set->RemoveDocStyleSheet(aSheet);
      }
    }
  }
}

void 
nsXULDocument::RemoveStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  mStyleSheets.RemoveElement(aSheet);
  
  PRBool enabled = PR_TRUE;
  aSheet->GetEnabled(enabled);

  if (enabled) {
    RemoveStyleSheetFromStyleSets(aSheet);

    // XXX should observers be notified for disabled sheets??? I think not, but I could be wrong
    for (PRInt32 index = 0; index < mObservers.Count(); index++) {
      nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
      observer->StyleSheetRemoved(this, aSheet);
      if (observer != (nsIDocumentObserver*)mObservers.ElementAt(index)) {
        index--;
      }
    }
  }

  aSheet->SetOwningDocument(nsnull);
  NS_RELEASE(aSheet);
}

NS_IMETHODIMP
nsXULDocument::InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex, PRBool aNotify)
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
nsXULDocument::SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
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
nsXULDocument::GetCSSLoader(nsICSSLoader*& aLoader)
{
  nsresult result = NS_OK;
  if (! mCSSLoader) {
    result = nsComponentManager::CreateInstance(kCSSLoaderCID,
                                                nsnull,
                                                NS_GET_IID(nsICSSLoader),
                                                getter_AddRefs(mCSSLoader));
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

NS_IMETHODIMP
nsXULDocument::GetScriptGlobalObject(nsIScriptGlobalObject** aScriptGlobalObject)
{
   *aScriptGlobalObject = mScriptGlobalObject;
   NS_IF_ADDREF(*aScriptGlobalObject);
   return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject)
{
    if (!aScriptGlobalObject) {
        // XXX HACK ALERT! If the script context owner is null, the
        // document will soon be going away. So tell our content that
        // to lose its reference to the document. This has to be done
        // before we actually set the script context owner to null so
        // that the content elements can remove references to their
        // script objects.
        if (mRootContent)
            mRootContent->SetDocument(nsnull, PR_TRUE, PR_TRUE);

        // Break circular reference for the case where the currently
        // focused window is ourself.
        if (mCommandDispatcher)
            mCommandDispatcher->SetFocusedWindow(nsnull);

        // set all builder references to document to nsnull -- out of band notification
        // to break ownership cycle
        if (mBuilders) {
            PRUint32 cnt = 0;
            nsresult rv = mBuilders->Count(&cnt);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");

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
    }

    mScriptGlobalObject = aScriptGlobalObject;
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetNameSpaceManager(nsINameSpaceManager*& aManager)
{
  aManager = mNameSpaceManager;
  NS_IF_ADDREF(aManager);
  return NS_OK;
}


// Note: We don't hold a reference to the document observer; we assume
// that it has a live reference to the document.
void
nsXULDocument::AddObserver(nsIDocumentObserver* aObserver)
{
    // XXX Make sure the observer isn't already in the list
    if (mObservers.IndexOf(aObserver) == -1) {
        mObservers.AppendElement(aObserver);
    }
}

PRBool
nsXULDocument::RemoveObserver(nsIDocumentObserver* aObserver)
{
    return mObservers.RemoveElement(aObserver);
}

NS_IMETHODIMP
nsXULDocument::BeginUpdate()
{
    // XXX Never called. Does this matter?
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->BeginUpdate(this);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::EndUpdate()
{
    // XXX Never called. Does this matter?
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->EndUpdate(this);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::BeginLoad()
{
    // XXX Never called. Does this matter?
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
nsXULDocument::EndLoad()
{
    nsresult rv;
        
    // Whack the prototype document into the cache so that the next
    // time somebody asks for it, they don't need to load it by hand.
    nsCOMPtr<nsIURI> uri;
    rv = mCurrentPrototype->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIChromeRegistry, reg, kChromeRegistryCID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsISupportsArray> sheets;
    reg->GetStyleSheets(uri, getter_AddRefs(sheets));
   
    // Walk the sheets and add them to the prototype. Also put them into the document.
    if (sheets) {
      nsCOMPtr<nsICSSStyleSheet> sheet;
      PRUint32 count;
      sheets->Count(&count);
      for (PRUint32 i = 0; i < count; i++) {
        nsCOMPtr<nsISupports> supp;
        sheets->GetElementAt(i, getter_AddRefs(supp));
        sheet = do_QueryInterface(supp);
        if (sheet) {
          nsCOMPtr<nsIURI> sheetURL;
          sheet->GetURL(*getter_AddRefs(sheetURL));
          if (gXULUtils->UseXULCache() && IsChromeURI(sheetURL))
            mCurrentPrototype->AddStyleSheetReference(sheetURL);
          AddStyleSheet(sheet);
        }
      }
    }

    if (gXULUtils->UseXULCache() && IsChromeURI(mDocumentURL)) {
        // If it's a 'chrome:' prototype document, then put it into
        // the prototype cache; other XUL documents will be reloaded
        // each time.
        rv = gXULCache->PutPrototype(mCurrentPrototype);
        if (NS_FAILED(rv)) return rv;
    }

    // Now walk the prototype to build content.
    rv = PrepareToWalk();
    if (NS_FAILED(rv)) return rv;

    if (mIsKeyBindingDoc && (mCurrentPrototype != mMasterPrototype))
      return NS_OK; // If we're a keybinding doc and we're loading an overlay
    return ResumeWalk();
}


NS_IMETHODIMP
nsXULDocument::ContentChanged(nsIContent* aContent,
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
nsXULDocument::ContentStatesChanged(nsIContent* aContent1, nsIContent* aContent2)
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
nsXULDocument::AttributeChanged(nsIContent* aElement,
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

    // Handle "open" and "close" cases. We do this handling before
    // we've notified the observer, so that content is already created
    // for the frame system to walk.
    if ((nameSpaceID == kNameSpaceID_XUL) && (aAttribute == kOpenAtom)) {
        nsAutoString open;
        rv = aElement->GetAttribute(kNameSpaceID_None, kOpenAtom, open);
        if (NS_FAILED(rv)) return rv;

        if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (open.EqualsWithConversion("true"))) {
            OpenWidgetItem(aElement);
        }
        else {
            CloseWidgetItem(aElement);
        }
    }

    // Now notify external observers
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->AttributeChanged(this, aElement, aNameSpaceID, aAttribute, aHint);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
            i--;
        }
    }

    // Check for a change to the 'ref' attribute on an atom, in which
    // case we may need to nuke and rebuild the entire content model
    // beneath the element.
    if (aAttribute == kRefAtom) {
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
nsXULDocument::ContentAppended(nsIContent* aContainer,
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
nsXULDocument::ContentInserted(nsIContent* aContainer,
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
nsXULDocument::ContentReplaced(nsIContent* aContainer,
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
nsXULDocument::ContentRemoved(nsIContent* aContainer,
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
nsXULDocument::StyleRuleChanged(nsIStyleSheet* aStyleSheet,
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
nsXULDocument::StyleRuleAdded(nsIStyleSheet* aStyleSheet,
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
nsXULDocument::StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
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
nsXULDocument::GetSelection(nsIDOMSelection** aSelection)
{
    if (!mSelection) {
        NS_ASSERTION(0,"not initialized");
        *aSelection = nsnull;
        return NS_ERROR_NOT_INITIALIZED;
    }
    *aSelection = mSelection;
    NS_ADDREF(*aSelection);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SelectAll()
{

    nsIContent * start = nsnull;
    nsIContent * end   = nsnull;
    nsIContent * body  = nsnull;

    nsAutoString bodyStr; bodyStr.AssignWithConversion("BODY");
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
    SetDisplaySelection(nsISelectionController::SELECTION_ON);

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
    aIsFound = PR_FALSE;
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
nsXULDocument::FlushPendingNotifications()
{
  PRInt32 i, count = mPresShells.Count();

  for (i = 0; i < count; i++) {
    nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);
    if (shell) {
      shell->FlushPendingNotifications();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetAndIncrementContentID(PRInt32* aID)
{
  *aID = mNextContentID++;
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetBindingManager(nsIBindingManager** aResult)
{
  nsresult rv;
  if (!mBindingManager) {
    mBindingManager = do_CreateInstance("component://netscape/xbl/binding-manager", &rv);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
  }

  *aResult = mBindingManager;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetNodeInfoManager(class nsINodeInfoManager *&aNodeInfoManager)
{
    NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_NOT_INITIALIZED);

    aNodeInfoManager = mNodeInfoManager;
    NS_ADDREF(aNodeInfoManager);

    return NS_OK;
}


PRBool
nsXULDocument::IsInRange(const nsIContent *aStartContent, const nsIContent* aEndContent, const nsIContent* aContent) const
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
nsXULDocument::IsBefore(const nsIContent *aNewContent, const nsIContent* aCurrentContent) const
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
nsXULDocument::IsInSelection(nsIDOMSelection* aSelection, const nsIContent *aContent) const
{
  PRBool aYes = PR_FALSE;
  nsCOMPtr<nsIDOMNode> node (do_QueryInterface((nsIContent *) aContent));
  aSelection->ContainsNode(node, PR_FALSE, &aYes);
  return aYes;
}

nsIContent*
nsXULDocument::GetPrevContent(const nsIContent *aContent) const
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
nsXULDocument::GetNextContent(const nsIContent *aContent) const
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
nsXULDocument::SetDisplaySelection(PRInt8 aToggle)
{
    mDisplaySelection = aToggle;
}

PRInt8
nsXULDocument::GetDisplaySelection() const
{
    return mDisplaySelection;
}

NS_IMETHODIMP
nsXULDocument::HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus)
{
  nsresult ret = NS_OK;
  nsIDOMEvent* domEvent = nsnull;

  if (NS_EVENT_FLAG_INIT & aFlags) {
    aDOMEvent = &domEvent;
    aEvent->flags = aFlags;
    aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
  }

  //Capturing stage
  if (NS_EVENT_FLAG_BUBBLE != aFlags && mScriptGlobalObject) {
    mScriptGlobalObject->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_CAPTURE, aEventStatus);
  }

  //Local handling stage
  if (mListenerManager && !(aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH)) {
    aEvent->flags |= aFlags;
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, this, aFlags, aEventStatus);
    aEvent->flags &= ~aFlags;
  }

  //Bubbling stage
  if (NS_EVENT_FLAG_CAPTURE != aFlags && mScriptGlobalObject) {
    mScriptGlobalObject->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_BUBBLE, aEventStatus);
  }

  if (NS_EVENT_FLAG_INIT & aFlags) {
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


//----------------------------------------------------------------------
//
// nsIXMLDocument interface
//

#ifdef MOZ_XSL
NS_IMETHODIMP
nsXULDocument::SetTransformMediator(nsITransformMediator* aMediator)
{
    NS_ASSERTION(0,"not implemented");
    NS_NOTREACHED("nsXULDocument::SetTransformMediator");
    return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

//----------------------------------------------------------------------
//
// nsIXULDocument interface
//

NS_IMETHODIMP
nsXULDocument::AddElementForID(const nsString& aID, nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    mElementMap.Add(aID, aElement);
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::RemoveElementForID(const nsString& aID, nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    mElementMap.Remove(aID, aElement);
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetElementsForID(const nsString& aID, nsISupportsArray* aElements)
{
    NS_PRECONDITION(aElements != nsnull, "null ptr");
    if (! aElements)
        return NS_ERROR_NULL_POINTER;

    mElementMap.Find(aID, aElements);
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::CreateContents(nsIContent* aElement)
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
nsXULDocument::AddContentModelBuilder(nsIRDFContentModelBuilder* aBuilder)
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
nsXULDocument::GetForm(nsIDOMHTMLFormElement** aForm)
{
  *aForm = mHiddenForm;
  NS_IF_ADDREF(*aForm);
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetForm(nsIDOMHTMLFormElement* aForm)
{
    mHiddenForm = dont_QueryInterface(aForm);

    // Set the document.
    nsCOMPtr<nsIContent> formContent = do_QueryInterface(aForm);
    formContent->SetDocument(this, PR_TRUE, PR_TRUE);

    // Forms are containers, and as such take up a bit of space.
    // Set a style attribute to keep the hidden form from showing up.
    mHiddenForm->SetAttribute(NS_ConvertASCIItoUCS2("style"), NS_ConvertASCIItoUCS2("margin:0em"));
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::AddForwardReference(nsForwardReference* aRef)
{
    if (mResolutionPhase < aRef->GetPhase()) {
        mForwardReferences.AppendElement(aRef);
    }
    else {
        NS_ERROR("forward references have already been resolved");
        delete aRef;
    }

    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::ResolveForwardReferences()
{
    if (mResolutionPhase == nsForwardReference::eDone)
        return NS_OK;

    // Resolve each outstanding 'forward' reference. We iterate
    // through the list of forward references until no more forward
    // references can be resolved. This annealing process is
    // guaranteed to converge because we've "closed the gate" to new
    // forward references.

    const nsForwardReference::Phase* pass = nsForwardReference::kPasses;
    while ((mResolutionPhase = *pass) != nsForwardReference::eDone) {
        PRInt32 previous = 0;
        while (mForwardReferences.Count() && mForwardReferences.Count() != previous) {
            previous = mForwardReferences.Count();

            for (PRInt32 i = 0; i < mForwardReferences.Count(); ++i) {
                nsForwardReference* fwdref = NS_REINTERPRET_CAST(nsForwardReference*, mForwardReferences[i]);

                if (fwdref->GetPhase() == *pass) {
                    nsForwardReference::Result result = fwdref->Resolve();

                    switch (result) {
                    case nsForwardReference::eResolve_Succeeded:
                    case nsForwardReference::eResolve_Error:
                        mForwardReferences.RemoveElementAt(i);
                        delete fwdref;

                        // fixup because we removed from list
                        --i;
                        break;

                    case nsForwardReference::eResolve_Later:
                        // do nothing. we'll try again later
                        ;
                    }
                }
            }
        }

        ++pass;
    }

    DestroyForwardReferences();
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetMasterPrototype(nsIXULPrototypeDocument* aDocument)
{
  mMasterPrototype = aDocument;
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetMasterPrototype(nsIXULPrototypeDocument** aDocument)
{
  *aDocument = mMasterPrototype;
  NS_IF_ADDREF(*aDocument);
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetCurrentPrototype(nsIXULPrototypeDocument* aDocument)
{
  mCurrentPrototype = aDocument;
  return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIStreamLoadableDocument interface
//

NS_IMETHODIMP
nsXULDocument::LoadFromStream(nsIInputStream& xulStream,
                              nsISupports* aContainer,
                              const char* aCommand)
{
    nsresult rv;

    // XXXbe this is dead code, eliminate
    nsCOMPtr<nsIParser> parser;
    rv = PrepareToLoad(aContainer, aCommand, nsnull, nsnull, getter_AddRefs(parser));
    if (NS_FAILED(rv)) return rv;

    parser->Parse(xulStream,NS_ConvertASCIItoUCS2(kXULTextContentType));
    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsIDOMDocument interface
//

NS_IMETHODIMP
nsXULDocument::GetDoctype(nsIDOMDocumentType** aDoctype)
{
    NS_NOTREACHED("nsXULDocument::GetDoctype");
    NS_ENSURE_ARG_POINTER(aDoctype);
    *aDoctype = nsnull;
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::GetImplementation(nsIDOMDOMImplementation** aImplementation)
{
  nsresult rv;
  rv = nsComponentManager::CreateInstance(kDOMImplementationCID,
                                          nsnull,
                                          NS_GET_IID(nsIDOMDOMImplementation),
                                          (void**) aImplementation);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIPrivateDOMImplementation> impl = do_QueryInterface(*aImplementation, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = impl->Init(mDocumentURL);
  return rv;
}

NS_IMETHODIMP
nsXULDocument::GetDocumentElement(nsIDOMElement** aDocumentElement)
{
    NS_PRECONDITION(aDocumentElement != nsnull, "null ptr");
    if (! aDocumentElement)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        return mRootContent->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aDocumentElement);
    }
    else {
        *aDocumentElement = nsnull;
        return NS_OK;
    }
}



NS_IMETHODIMP
nsXULDocument::CreateElement(const nsString& aTagName, nsIDOMElement** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsIAtom> name, prefix;

    if (kStrictDOMLevel2) {
        PRInt32 pos = aTagName.FindChar(':');
        if (pos >= 0) {
          nsCAutoString tmp; tmp.AssignWithConversion(aTagName);
          printf ("Possible DOM Error: CreateElement(\"%s\") called, use CreateElementNS() in stead!\n", (const char *)tmp);
        }

        name = dont_AddRef(NS_NewAtom(aTagName));
        NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);
    } else {
        // parse the user-provided string into a tag name and a namespace ID
        rv = ParseTagString(aTagName, *getter_AddRefs(name),
                            *getter_AddRefs(prefix));
        if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
            char* tagNameStr = aTagName.ToNewCString();
            PR_LOG(gXULLog, PR_LOG_ERROR,
                   ("xul[CreateElement] unable to parse tag '%s'; no such namespace.", tagNameStr));
            nsCRT::free(tagNameStr);
#endif
            return rv;
        }
    }

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
      char* tagCStr = aTagName.ToNewCString();

      PR_LOG(gXULLog, PR_LOG_DEBUG,
             ("xul[CreateElement] %s", tagCStr));

      nsCRT::free(tagCStr);
    }
#endif

    *aReturn = nsnull;

    nsCOMPtr<nsINodeInfo> ni;

    // CreateElement in the XUL document defaults to the XUL namespace.
    mNodeInfoManager->GetNodeInfo(name, prefix, kNameSpaceID_XUL,
                                  *getter_AddRefs(ni));

    nsCOMPtr<nsIContent> result;
    rv = CreateElement(ni, getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;

    // get the DOM interface
    rv = result->QueryInterface(NS_GET_IID(nsIDOMElement), (void**) aReturn);
    NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM element");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)
{
    NS_NOTREACHED("nsXULDocument::CreateDocumentFragment");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::CreateTextNode(const nsString& aData, nsIDOMText** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsITextContent> text;
    rv = nsComponentManager::CreateInstance(kTextNodeCID, nsnull, NS_GET_IID(nsITextContent), getter_AddRefs(text));
    if (NS_FAILED(rv)) return rv;

    rv = text->SetText(aData.GetUnicode(), aData.Length(), PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    rv = text->QueryInterface(NS_GET_IID(nsIDOMText), (void**) aReturn);
    NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOMText");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::CreateComment(const nsString& aData, nsIDOMComment** aReturn)
{
    NS_NOTREACHED("nsXULDocument::CreateComment");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn)
{
    NS_NOTREACHED("nsXULDocument::CreateCDATASection");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn)
{
    NS_NOTREACHED("nsXULDocument::CreateProcessingInstruction");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::CreateAttribute(const nsString& aName, nsIDOMAttr** aReturn)
{
    NS_NOTREACHED("nsXULDocument::CreateAttribute");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn)
{
    NS_NOTREACHED("nsXULDocument::CreateEntityReference");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::GetElementsByTagName(const nsString& aTagName, nsIDOMNodeList** aReturn)
{
    if (kStrictDOMLevel2) {
        PRInt32 pos = aTagName.FindChar(':');
        if (pos >= 0) {
          nsCAutoString tmp; tmp.AssignWithConversion(aTagName);
          printf ("Possible DOM Error: GetElementsByTagName(\"%s\") called, use GetElementsByTagNameNS() in stead!\n", (const char *)tmp);
        }
    }

    nsresult rv;
    nsRDFDOMNodeList* elements;
    if (NS_FAILED(rv = nsRDFDOMNodeList::Create(&elements))) {
        NS_ERROR("unable to create node list");
        return rv;
    }

    nsIContent* root = GetRootContent();
    NS_ASSERTION(root != nsnull, "no doc root");

    if (root != nsnull) {
        rv = GetElementsByTagName(root, aTagName, kNameSpaceID_Unknown,
                                  elements);

        NS_RELEASE(root);
    }

    *aReturn = elements;
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetElementsByAttribute(const nsString& aAttribute, const nsString& aValue,
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
        if (NS_SUCCEEDED(rv = root->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) &domRoot))) {
            rv = GetElementsByAttribute(domRoot, aAttribute, aValue, elements);
            NS_RELEASE(domRoot);
        }
        NS_RELEASE(root);
    }

    *aReturn = elements;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::Persist(const nsString& aID, const nsString& aAttr)
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
nsXULDocument::Persist(nsIContent* aElement, PRInt32 aNameSpaceID, nsIAtom* aAttribute)
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
    nsCAutoString temp_attrstr; temp_attrstr.AssignWithConversion(attrstr);
    rv = gRDFService->GetResource(temp_attrstr, getter_AddRefs(attr));
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
nsXULDocument::DestroyForwardReferences()
{
    for (PRInt32 i = mForwardReferences.Count() - 1; i >= 0; --i) {
        nsForwardReference* fwdref = NS_REINTERPRET_CAST(nsForwardReference*, mForwardReferences[i]);
        delete fwdref;
    }

    mForwardReferences.Clear();
    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsIDOMNSDocument interface
//

NS_IMETHODIMP
nsXULDocument::GetStyleSheets(nsIDOMStyleSheetList** aStyleSheets)
{
    NS_NOTREACHED("nsXULDocument::GetStyleSheets");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsXULDocument::GetCharacterSet(nsString& aCharacterSet)
{
  return GetDocumentCharacterSet(aCharacterSet);
}

NS_IMETHODIMP    
nsXULDocument::CreateElementWithNameSpace(const nsString& aTagName,
                                            const nsString& aNameSpace,
                                            nsIDOMElement** aResult)
{
  printf ("Deprecated method CreateElementWithNameSpace() used, use CreateElementNS() in stead!\n");

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

    nsCOMPtr<nsINodeInfo> ni;
    // XXX This whole method is depricated!
    mNodeInfoManager->GetNodeInfo(name, nsnull, nameSpaceID,
                                  *getter_AddRefs(ni));

    nsCOMPtr<nsIContent> result;
    rv = CreateElement(ni, getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;

    // get the DOM interface
    rv = result->QueryInterface(NS_GET_IID(nsIDOMElement), (void**) aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM element");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::CreateRange(nsIDOMRange** aRange)
{
    NS_NOTREACHED("nsXULDocument::CreateRange");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::GetDefaultView(nsIDOMAbstractView** aDefaultView)
{
  NS_ENSURE_ARG_POINTER(aDefaultView);
  *aDefaultView = nsnull;

  nsIPresShell *shell = NS_STATIC_CAST(nsIPresShell *,
                                       mPresShells.ElementAt(0));
  NS_ENSURE_TRUE(shell, NS_OK);

  nsCOMPtr<nsIPresContext> ctx;
  nsresult rv = shell->GetPresContext(getter_AddRefs(ctx));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && ctx, rv);

  nsCOMPtr<nsISupports> container;
  rv = ctx->GetContainer(getter_AddRefs(container));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && container, rv);

  nsCOMPtr<nsIInterfaceRequestor> ifrq(do_QueryInterface(container));
  NS_ENSURE_TRUE(ifrq, NS_OK);

  nsCOMPtr<nsIDOMWindow> window;
  ifrq->GetInterface(NS_GET_IID(nsIDOMWindow), getter_AddRefs(window));
  NS_ENSURE_TRUE(window, NS_OK);

  window->QueryInterface(NS_GET_IID(nsIDOMAbstractView),
                         (void **)aDefaultView);

  return NS_OK;
}

nsresult
nsXULDocument::GetPixelDimensions(nsIPresShell* aShell,
                               PRInt32* aWidth,
                               PRInt32* aHeight)
{
    nsresult result = NS_OK;
    nsSize size;
    nsIFrame* frame;
  
    result = FlushPendingNotifications();
    if (NS_FAILED(result)) {
        return result;
    }
    
    result = aShell->GetPrimaryFrameFor(mRootContent, &frame);
    if (NS_SUCCEEDED(result) && frame) {
        nsIView*                  view;
        nsCOMPtr<nsIPresContext>  presContext;
        
        aShell->GetPresContext(getter_AddRefs(presContext));
        result = frame->GetView(presContext, &view);
        if (NS_SUCCEEDED(result)) {
            // If we have a view check if it's scrollable. If not,
            // just use the view size itself
            if (view) {
                nsIScrollableView* scrollableView;
                
                if (NS_SUCCEEDED(view->QueryInterface(NS_GET_IID(nsIScrollableView),
                                                      (void**)&scrollableView))) {
                    scrollableView->GetScrolledView(view);
                }
                
                result = view->GetDimensions(&size.width, &size.height);
            }
            // If we don't have a view, use the frame size
            else {
                result = frame->GetSize(size);
            }
        }
        
        // Convert from twips to pixels
        if (NS_SUCCEEDED(result)) {
            nsCOMPtr<nsIPresContext> context;
            
            result = aShell->GetPresContext(getter_AddRefs(context));
            
            if (NS_SUCCEEDED(result)) {
                float scale;
                context->GetTwipsToPixels(&scale);
                
                *aWidth = NSTwipsToIntPixels(size.width, scale);
                *aHeight = NSTwipsToIntPixels(size.height, scale);
            }
        }
    }
    else {
        *aWidth = 0;
        *aHeight = 0;
    }
    
    return result;
    
}

NS_IMETHODIMP
nsXULDocument::GetWidth(PRInt32* aWidth)
{
    NS_ENSURE_ARG_POINTER(aWidth);

    nsCOMPtr<nsIPresShell> shell;
    nsresult result = NS_OK;

    // We make the assumption that the first presentation shell
    // is the one for which we need information.
    shell = getter_AddRefs(GetShellAt(0));
    if (shell) {
        PRInt32 width, height;
        
        result = GetPixelDimensions(shell, &width, &height);
        *aWidth = width;
    } else
        *aWidth = 0;

    return result;
}

NS_IMETHODIMP
nsXULDocument::GetHeight(PRInt32* aHeight)
{
    NS_ENSURE_ARG_POINTER(aHeight);

    nsCOMPtr<nsIPresShell> shell;
    nsresult result = NS_OK;

    // We make the assumption that the first presentation shell
    // is the one for which we need information.
    shell = getter_AddRefs(GetShellAt(0));
    if (shell) {
        PRInt32 width, height;

        result = GetPixelDimensions(shell, &width, &height);
        *aHeight = height;
    } else
        *aHeight = 0;

    return result;
}

NS_IMETHODIMP
nsXULDocument::AddBinding(nsIDOMElement* aContent, const nsString& aURL)
{
  nsCOMPtr<nsIBindingManager> bm;
  GetBindingManager(getter_AddRefs(bm));
  nsCOMPtr<nsIContent> content(do_QueryInterface(aContent));

  return bm->AddLayeredBinding(content, aURL);
}

NS_IMETHODIMP
nsXULDocument::RemoveBinding(nsIDOMElement* aContent, const nsString& aURL)
{
  if (mBindingManager) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aContent));
    return mBindingManager->RemoveLayeredBinding(content, aURL);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULDocument::LoadBindingDocument(const nsString& aURL)
{
  if (mBindingManager)
    return mBindingManager->LoadBindingDocument(aURL);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULDocument::GetAnonymousNodes(nsIDOMElement* aElement, nsIDOMNodeList** aResult)
{
  nsresult rv;
  nsRDFDOMNodeList* elements;
  // Addref happens on following line in the Create call.
  if (NS_FAILED(rv = nsRDFDOMNodeList::Create(&elements))) {
    NS_ERROR("unable to create node list");
    return rv;
  }

  *aResult = elements;

  // Use the XBL service to get a content list.
  NS_WITH_SERVICE(nsIXBLService, xblService, "component://netscape/xbl", &rv);
  if (!xblService)
    return rv;

  // Retrieve the anonymous content that we should build.
  nsCOMPtr<nsISupportsArray> anonymousItems;
  nsCOMPtr<nsIContent> dummyElt;
  nsCOMPtr<nsIContent> element(do_QueryInterface(aElement));
  if (!element)
    return rv;

  PRBool dummy;
  xblService->GetContentList(element, getter_AddRefs(anonymousItems), 
                             getter_AddRefs(dummyElt), &dummy);
  
  if (!anonymousItems)
    return NS_OK;

  PRUint32 count = 0;
  anonymousItems->Count(&count);

  for (PRUint32 i=0; i < count; i++)
  {
    // get our child's content and set its parent to our content
    nsCOMPtr<nsISupports> node;
    anonymousItems->GetElementAt(i,getter_AddRefs(node));

    nsCOMPtr<nsIDOMNode> content(do_QueryInterface(node));
    
    if (content)
      elements->AppendNode(content);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::Load(const nsString& aUrl)
{
    NS_NOTREACHED("nsXULDocument::Load");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsXULDocument::GetPlugins(nsIDOMPluginArray** aPlugins)
{
    NS_NOTREACHED("nsXULDocument::GetPlugins");
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
//
// nsIDOMXULDocument interface
//

NS_IMETHODIMP
nsXULDocument::GetPopupNode(nsIDOMNode** aNode)
{
    *aNode = mPopupNode;
    NS_IF_ADDREF(*aNode);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetPopupNode(nsIDOMNode* aNode)
{
    mPopupNode = dont_QueryInterface(aNode);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetTooltipNode(nsIDOMNode** aNode)
{
    *aNode = mTooltipNode;
    NS_IF_ADDREF(*aNode);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetTooltipNode(nsIDOMNode* aNode)
{
    mTooltipNode = dont_QueryInterface(aNode);
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetCommandDispatcher(nsIDOMXULCommandDispatcher** aTracker)
{
  *aTracker = mCommandDispatcher;
  NS_IF_ADDREF(*aTracker);
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetControls(nsIDOMHTMLCollection ** aResult) {
    NS_ENSURE_ARG_POINTER(aResult);
    
    // for now, just return the elements in the HTML form...
    // eventually we could return other XUL elements
    if (mHiddenForm)
        return mHiddenForm->GetElements(aResult);
    else
        *aResult = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::ImportNode(nsIDOMNode* aImportedNode,
                          PRBool aDeep,
                          nsIDOMNode** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::CreateElementNS(const nsString& aNamespaceURI,
                               const nsString& aQualifiedName,
                               nsIDOMElement** aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);

    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
      char* namespaceCStr = aNamespaceURI.ToNewCString();
      char* tagCStr = aQualifiedName.ToNewCString();

      PR_LOG(gXULLog, PR_LOG_DEBUG,
             ("xul[CreateElementWithNameSpace] [%s]:%s", namespaceCStr, tagCStr));

      nsCRT::free(tagCStr);
      nsCRT::free(namespaceCStr);
    }
#endif

    nsCOMPtr<nsIAtom> name, prefix;

    // parse the user-provided string into a tag name and prefix
    rv = ParseTagString(aQualifiedName, *getter_AddRefs(name),
                        *getter_AddRefs(prefix));
    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        char* tagNameStr = aQualifiedName.ToNewCString();
        PR_LOG(gXULLog, PR_LOG_ERROR,
               ("xul[CreateElement] unable to parse tag '%s'; no such namespace.", tagNameStr));
        nsCRT::free(tagNameStr);
#endif
        return rv;
    }

    // Get The real namespace ID
    PRInt32 nameSpaceID;
    rv = mNameSpaceManager->GetNameSpaceID(aNamespaceURI, nameSpaceID);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsINodeInfo> ni;
    // XXX This whole method is depricated!
    mNodeInfoManager->GetNodeInfo(name, prefix, nameSpaceID,
                                  *getter_AddRefs(ni));

    nsCOMPtr<nsIContent> result;
    rv = CreateElement(ni, getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;

    // get the DOM interface
    rv = result->QueryInterface(NS_GET_IID(nsIDOMElement), (void**) aReturn);
    NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM element");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::CreateAttributeNS(const nsString& aNamespaceURI,
                                 const nsString& aQualifiedName,
                                 nsIDOMAttr** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::GetElementById(const nsString& aId, nsIDOMElement** aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);
    *aReturn = nsnull;

    NS_WARN_IF_FALSE(!aId.IsEmpty(),"getElementById(\"\"), fix caller?");
    if (aId.IsEmpty())
      return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIContent> element;
    rv = mElementMap.FindFirst(aId, getter_AddRefs(element));
    if (NS_FAILED(rv)) return rv;

    if (element) {
        rv = element->QueryInterface(NS_GET_IID(nsIDOMElement), (void**) aReturn);
    }
    else {
        rv = NS_OK;
    }

    return rv;
}

NS_IMETHODIMP
nsXULDocument::GetElementsByTagNameNS(const nsString& aNamespaceURI,
                                      const nsString& aLocalName,
                                      nsIDOMNodeList** aReturn)
{
    nsresult rv;

    nsRDFDOMNodeList* elements;
    if (NS_FAILED(rv = nsRDFDOMNodeList::Create(&elements))) {
        NS_ERROR("unable to create node list");
        return rv;
    }

    *aReturn = elements;

    nsCOMPtr<nsIContent> root = dont_AddRef(GetRootContent());
    NS_ASSERTION(root, "no doc root");

    if (root) {
        PRInt32 nsid = kNameSpaceID_Unknown;
        if (!aNamespaceURI.EqualsWithConversion("*")) {
            rv = mNameSpaceManager->GetNameSpaceID(aNamespaceURI, nsid);
            NS_ENSURE_SUCCESS(rv, rv);

            if (nsid == kNameSpaceID_Unknown) {
                // Namespace not found, then there can't be any elements to
                // be found.
                return NS_OK;
            }
        }

        rv = GetElementsByTagName(root, aLocalName, nsid,
                                  elements);
    }

    return NS_OK;
}

nsresult
nsXULDocument::AddSubtreeToDocument(nsIContent* aElement)
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
    if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && value.EqualsWithConversion("true")) {
        rv = gXULUtils->SetCommandUpdater(this, aElement);
        if (NS_FAILED(rv)) return rv;
    }

    // 3. Check for a broadcaster hookup attribute, in which case
    // we'll hook the node up as a listener on a broadcaster.
    PRBool listener, resolved;
    rv = CheckBroadcasterHookup(this, aElement, &listener, &resolved);
    if (NS_FAILED(rv)) return rv;

    // If it's not there yet, we may be able to defer hookup until
    // later.
    if (listener && !resolved && (mResolutionPhase != nsForwardReference::eDone)) {
        BroadcasterHookup* hookup = new BroadcasterHookup(this, aElement);
        if (! hookup)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = AddForwardReference(hookup);
        if (NS_FAILED(rv)) return rv;
    }

    // 4. Recurse to children.
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
nsXULDocument::RemoveSubtreeFromDocument(nsIContent* aElement)
{
    // Do a bunch of cleanup to remove an element from the XUL
    // document.
    nsresult rv;

    // 1. Remove any children from the document.
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

    // 2. Remove the element from the resource-to-element map
    rv = RemoveElementFromMap(aElement);
    if (NS_FAILED(rv)) return rv;

    // 3. If the element is a 'command updater', then remove the
    // element from the document's command dispatcher.
    nsAutoString value;
    rv = aElement->GetAttribute(kNameSpaceID_None, kCommandUpdaterAtom, value);
    if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && value.EqualsWithConversion("true")) {
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
nsIAtom** nsXULDocument::kIdentityAttrs[] = { &kIdAtom, &kRefAtom, nsnull };

nsresult
nsXULDocument::AddElementToMap(nsIContent* aElement)
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
nsXULDocument::RemoveElementFromMap(nsIContent* aElement)
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
nsXULDocument::RemoveElementsFromMapByContent(const PRUnichar* aID,
                                              nsIContent* aElement,
                                              void* aClosure)
{
    nsIContent* content = NS_REINTERPRET_CAST(nsIContent*, aClosure);
    return (aElement == content) ? HT_ENUMERATE_REMOVE : HT_ENUMERATE_NEXT;
}



//----------------------------------------------------------------------
//
// nsIDOMNode interface
//

NS_IMETHODIMP
nsXULDocument::GetNodeName(nsString& aNodeName)
{
    aNodeName.AssignWithConversion("#document");
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetNodeValue(nsString& aNodeValue)
{
    aNodeValue.Truncate();
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::SetNodeValue(const nsString& aNodeValue)
{
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetNodeType(PRUint16* aNodeType)
{
    *aNodeType = nsIDOMNode::DOCUMENT_NODE;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetParentNode(nsIDOMNode** aParentNode)
{
    *aParentNode = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetChildNodes(nsIDOMNodeList** aChildNodes)
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
            rv = mRootContent->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)&domNode);
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
nsXULDocument::HasChildNodes(PRBool* aHasChildNodes)
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
nsXULDocument::GetFirstChild(nsIDOMNode** aFirstChild)
{
    NS_PRECONDITION(aFirstChild != nsnull, "null ptr");
    if (! aFirstChild)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        return mRootContent->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aFirstChild);
    }
    else {
        *aFirstChild = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
nsXULDocument::GetLastChild(nsIDOMNode** aLastChild)
{
    NS_PRECONDITION(aLastChild != nsnull, "null ptr");
    if (! aLastChild)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        return mRootContent->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aLastChild);
    }
    else {
        *aLastChild = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
nsXULDocument::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    NS_PRECONDITION(aPreviousSibling != nsnull, "null ptr");
    if (! aPreviousSibling)
        return NS_ERROR_NULL_POINTER;

    *aPreviousSibling = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetNextSibling(nsIDOMNode** aNextSibling)
{
    NS_PRECONDITION(aNextSibling != nsnull, "null ptr");
    if (! aNextSibling)
        return NS_ERROR_NULL_POINTER;

    *aNextSibling = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
    NS_PRECONDITION(aAttributes != nsnull, "null ptr");
    if (! aAttributes)
        return NS_ERROR_NULL_POINTER;

    *aAttributes = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    NS_PRECONDITION(aOwnerDocument != nsnull, "null ptr");
    if (! aOwnerDocument)
        return NS_ERROR_NULL_POINTER;

    *aOwnerDocument = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetNamespaceURI(nsString& aNamespaceURI)
{ 
  aNamespaceURI.Truncate();
  return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetPrefix(nsString& aPrefix)
{
  aPrefix.Truncate();
  return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::SetPrefix(const nsString& aPrefix)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsXULDocument::GetLocalName(nsString& aLocalName)
{
  aLocalName.Truncate();
  return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
    NS_NOTREACHED("nsXULDocument::InsertBefore");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    NS_NOTREACHED("nsXULDocument::ReplaceChild");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    NS_NOTREACHED("nsXULDocument::RemoveChild");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
    NS_NOTREACHED("nsXULDocument::AppendChild");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    // We don't allow cloning of a document
    *aReturn = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::Normalize()
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::Supports(const nsString& aFeature, const nsString& aVersion,
                        PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}


//----------------------------------------------------------------------
//
// nsIJSScriptObject interface
//

PRBool
nsXULDocument::AddProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


PRBool
nsXULDocument::DeleteProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


PRBool
nsXULDocument::GetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
    if (JSVAL_IS_STRING(aID)) {
        JSString *jsString = JS_ValueToString(aContext, aID);
        if (!jsString)
            return PR_FALSE;

        if (PL_strcmp("location", JS_GetStringBytes(jsString)) == 0) {
           nsCOMPtr<nsIJSScriptObject> window = do_QueryInterface(mScriptGlobalObject);
           if (nsnull != window) {
               return window->GetProperty(aContext, aObj, aID, aVp);
           }
        }
    }

    return PR_TRUE;
}


PRBool
nsXULDocument::SetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
    nsresult rv;

    if (JSVAL_IS_STRING(aID)) {
        char* s = JS_GetStringBytes(JS_ValueToString(aContext, aID));
        if (PL_strcmp("title", s) == 0) {
            JSString* jsString = JS_ValueToString(aContext, *aVp);
            if (!jsString)
                return PR_FALSE;
            nsAutoString title(NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars(jsString)));
            for (PRInt32 i = mPresShells.Count() - 1; i >= 0; --i) {
                nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);
                nsCOMPtr<nsIPresContext> context;
                rv = shell->GetPresContext(getter_AddRefs(context));
                if (NS_FAILED(rv)) return PR_FALSE;

                nsCOMPtr<nsISupports> container;
                rv = context->GetContainer(getter_AddRefs(container));
                if (NS_FAILED(rv)) return PR_FALSE;

                if (! container) continue;

                nsCOMPtr<nsIBaseWindow> docShellWin = do_QueryInterface(container);
                if(!docShellWin) continue;

                rv = docShellWin->SetTitle(title.GetUnicode());
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
nsXULDocument::EnumerateProperty(JSContext *aContext, JSObject *aObj)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


PRBool
nsXULDocument::Resolve(JSContext *aContext, JSObject *aObj, jsval aID)
{
    return PR_TRUE;
}


PRBool
nsXULDocument::Convert(JSContext *aContext, JSObject *aObj, jsval aID)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


void
nsXULDocument::Finalize(JSContext *aContext, JSObject *aObj)
{
    NS_NOTYETIMPLEMENTED("write me");
}



//----------------------------------------------------------------------
//
// nsIScriptObjectOwner interface
//

NS_IMETHODIMP
nsXULDocument::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    if (! mScriptObject) {
        // ...we need to instantiate our script object for the first
        // time.

        // Make sure that we've got our script context owner; this
        // assertion will fire if we've tried to get the script object
        // before our scope has been set up.
        NS_ASSERTION(mScriptGlobalObject != nsnull, "no script object");
        if (! mScriptGlobalObject)
            return NS_ERROR_NOT_INITIALIZED;

        nsresult rv;

        // Use the global object from our script context owner (the
        // window) as the parent of our own script object. (Using the
        // global object from aContext would make our script object
        // dynamically scoped in the first context that ever tried to
        // use us!)

        rv = NS_NewScriptXULDocument(aContext,
                                     NS_STATIC_CAST(nsISupports*, NS_STATIC_CAST(nsIDOMXULDocument*, this)),
                                     mScriptGlobalObject, &mScriptObject);
        if (NS_FAILED(rv)) return rv;
    }

    *aScriptObject = mScriptObject;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::SetScriptObject(void *aScriptObject)
{
    mScriptObject = aScriptObject;
    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsIHTMLContentContainer interface
//

NS_IMETHODIMP
nsXULDocument::GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult)
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
nsXULDocument::GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult)
{
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

//----------------------------------------------------------------------
//
// Implementation methods
//

nsIContent*
nsXULDocument::FindContent(const nsIContent* aStartNode,
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
nsXULDocument::Init()
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

    rv = nsComponentManager::CreateInstance(NS_NODEINFOMANAGER_PROGID,
                                            nsnull,
                                            NS_GET_IID(nsINodeInfoManager),
                                            getter_AddRefs(mNodeInfoManager));

    if (NS_FAILED(rv)) return rv;

    mNodeInfoManager->Init(mNameSpaceManager);

    if (!mIsKeyBindingDoc) {
        // Create our focus tracker and hook it up.
        rv = nsXULCommandDispatcher::Create(getter_AddRefs(mCommandDispatcher));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a focus tracker");
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIDOMEventListener> CommandDispatcher =
            do_QueryInterface(mCommandDispatcher);

        if (CommandDispatcher) {
            // Take the focus tracker and add it as an event listener for focus and blur events.
            AddEventListener(NS_ConvertASCIItoUCS2("focus"), CommandDispatcher, PR_TRUE);
            AddEventListener(NS_ConvertASCIItoUCS2("blur"), CommandDispatcher, PR_TRUE);
        }
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

    // Create a new nsISupportsArray for dealing with overlay references
    rv = NS_NewISupportsArray(getter_AddRefs(mUnloadedOverlays));
    if (NS_FAILED(rv)) return rv;

    // Create a new nsISupportsArray that will hold owning references
    // to each of the prototype documents used to construct this
    // document. That will ensure that prototype elements will remain
    // alive.
    rv = NS_NewISupportsArray(getter_AddRefs(mPrototypes));
    if (NS_FAILED(rv)) return rv;

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
        kAttributeAtom      = NS_NewAtom("attribute");
        kCommandUpdaterAtom = NS_NewAtom("commandupdater");
        kContextAtom        = NS_NewAtom("context");
        kDataSourcesAtom    = NS_NewAtom("datasources");
        kElementAtom        = NS_NewAtom("element");
        kIdAtom             = NS_NewAtom("id");
        kKeysetAtom         = NS_NewAtom("keyset");
        kObservesAtom       = NS_NewAtom("observes");
        kOpenAtom           = NS_NewAtom("open");
        kOverlayAtom        = NS_NewAtom("overlay");
        kPersistAtom        = NS_NewAtom("persist");
        kPopupAtom          = NS_NewAtom("popup");
        kPositionAtom       = NS_NewAtom("position");
        kInsertAfterAtom    = NS_NewAtom("insertafter");
        kInsertBeforeAtom   = NS_NewAtom("insertbefore");
        kRefAtom            = NS_NewAtom("ref");
        kRuleAtom           = NS_NewAtom("rule");
        kStyleAtom          = NS_NewAtom("style");
        kTemplateAtom       = NS_NewAtom("template");
        kTooltipAtom        = NS_NewAtom("tooltip");

        kCoalesceAtom       = NS_NewAtom("coalesceduplicatearcs");
        kAllowNegativesAtom = NS_NewAtom("allownegativeassertions");

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
                                                NS_GET_IID(nsIElementFactory),
                                                (void**) &gHTMLElementFactory);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get HTML element factory");
        if (NS_FAILED(rv)) return rv;

        rv = nsComponentManager::CreateInstance(kXMLElementFactoryCID,
                                                nsnull,
                                                NS_GET_IID(nsIElementFactory),
                                                (void**) &gXMLElementFactory);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get XML element factory");
        if (NS_FAILED(rv)) return rv;

        rv = nsServiceManager::GetService(kNameSpaceManagerCID,
                                          NS_GET_IID(nsINameSpaceManager),
                                          (nsISupports**) &gNameSpaceManager);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get namespace manager");
        if (NS_FAILED(rv)) return rv;

#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
static const char kXULNameSpaceURI[] = XUL_NAMESPACE_URI;
        gNameSpaceManager->RegisterNameSpace(NS_ConvertASCIItoUCS2(kXULNameSpaceURI), kNameSpaceID_XUL);


        rv = nsServiceManager::GetService(kXULContentUtilsCID,
                                          NS_GET_IID(nsIXULContentUtils),
                                          (nsISupports**) &gXULUtils);
        if (NS_FAILED(rv)) return rv;

        rv = nsServiceManager::GetService(kXULPrototypeCacheCID,
                                          NS_GET_IID(nsIXULPrototypeCache),
                                          (nsISupports**) &gXULCache);
        if (NS_FAILED(rv)) return rv;

        rv = nsServiceManager::GetService(NS_SCRIPTSECURITYMANAGER_PROGID,
                                          NS_GET_IID(nsIScriptSecurityManager),
                                          (nsISupports**) &gScriptSecurityManager);
        if (NS_FAILED(rv)) return rv;

        rv = gScriptSecurityManager->GetSystemPrincipal(&gSystemPrincipal);
        if (NS_FAILED(rv)) return rv;
    }

#ifdef PR_LOGGING
    if (! gXULLog)
        gXULLog = PR_NewLogModule("nsXULDocument");
#endif

    return NS_OK;
}


nsresult
nsXULDocument::StartLayout(void)
{
    if (! mRootContent) {
#ifdef PR_LOGGING
        nsXPIDLCString urlspec;
        mDocumentURL->GetSpec(getter_Copies(urlspec));

        PR_LOG(gXULLog, PR_LOG_ALWAYS,
               ("xul: unable to layout '%s'; no root content", (const char*) urlspec));
#endif
        return NS_OK;
    }

    PRInt32 count = GetNumberOfShells();
    for (PRInt32 i = 0; i < count; i++) {
      nsCOMPtr<nsIPresShell> shell = getter_AddRefs(GetShellAt(i));
      if (nsnull == shell)
          continue;

      // Resize-reflow this time
      nsCOMPtr<nsIPresContext> cx;
      shell->GetPresContext(getter_AddRefs(cx));
      NS_ASSERTION(cx != nsnull, "no pres context");
      if (! cx)
          return NS_ERROR_UNEXPECTED;


      nsCOMPtr<nsISupports> container;
      cx->GetContainer(getter_AddRefs(container));
      NS_ASSERTION(container != nsnull, "pres context has no container");
      if (! container)
          return NS_ERROR_UNEXPECTED;

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
      NS_ASSERTION(docShell != nsnull, "container is not a docshell");
      if (! docShell)
          return NS_ERROR_UNEXPECTED;

      nsRect r;
      cx->GetVisibleArea(r);

      // Trigger a refresh before the call to InitialReflow(), because
      // the view manager's UpdateView() function is dropping dirty rects if
      // refresh is disabled rather than accumulating them until refresh is
      // enabled and then triggering a repaint...
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        nsCOMPtr<nsIContentViewer> contentViewer;
        nsresult rv = docShell->GetContentViewer(getter_AddRefs(contentViewer));
        if (NS_SUCCEEDED(rv) && (contentViewer != nsnull)) {
          PRBool enabled;
          contentViewer->GetEnableRendering(&enabled);
          if (enabled) {
            vm->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
          }
        }
      }

      shell->InitialReflow(r.width, r.height);

      // Start observing the document _after_ we do the initial
      // reflow. Otherwise, we'll get into an trouble trying to
      // create kids before the root frame is established.
      shell->BeginObservingDocument();
    }
    return NS_OK;
}


nsresult
nsXULDocument::GetElementsByTagName(nsIContent *aContent,
                                    const nsString& aName,
                                    PRInt32 aNamespaceID,
                                    nsRDFDOMNodeList* aElements)
{
    NS_ENSURE_ARG_POINTER(aContent);
    NS_ENSURE_ARG_POINTER(aElements);

    nsresult rv = NS_OK;

    nsCOMPtr<nsIDOMElement> element(do_QueryInterface(aContent));
    if (!element)
      return NS_OK;

    nsCOMPtr<nsINodeInfo> ni;
    aContent->GetNodeInfo(*getter_AddRefs(ni));
    NS_ENSURE_TRUE(ni, NS_OK);

    if (aName.EqualsWithConversion("*")) {
        if (aNamespaceID == kNameSpaceID_Unknown ||
            ni->NamespaceEquals(aNamespaceID)) {
            if (NS_FAILED(rv = aElements->AppendNode(element))) {
                NS_ERROR("unable to append element to node list");
                return rv;
            }
        }
    }
    else {
        if (ni->Equals(aName) &&
            (aNamespaceID == kNameSpaceID_Unknown ||
             ni->NamespaceEquals(aNamespaceID))) {
            if (NS_FAILED(rv = aElements->AppendNode(element))) {
                NS_ERROR("unable to append element to node list");
                return rv;
            }
        }
    }

    PRInt32 length;
    if (NS_FAILED(rv = aContent->ChildCount(length))) {
        NS_ERROR("unable to get childcount");
        return rv;
    }

    for (PRInt32 i = 0; i < length; ++i) {
        nsCOMPtr<nsIContent> child;
        if (NS_FAILED(rv = aContent->ChildAt(i, *getter_AddRefs(child) ))) {
            NS_ERROR("unable to get child from content");
            return rv;
        }

        if (NS_FAILED(rv = GetElementsByTagName(child, aName, aNamespaceID,
                                                aElements))) {
            NS_ERROR("unable to recursively get elements by tag name");
            return rv;
        }
    }

    return NS_OK;
}

nsresult
nsXULDocument::GetElementsByAttribute(nsIDOMNode* aNode,
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

    if ((attrValue == aValue) || (attrValue.Length() > 0 && aValue.EqualsWithConversion("*"))) {
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
nsXULDocument::ParseTagString(const nsString& aTagName, nsIAtom*& aName,
                              nsIAtom*& aPrefix)
{
    // Parse the tag into a name and prefix

    static char kNameSpaceSeparator = ':';

    nsAutoString prefix;
    nsAutoString name(aTagName);
    PRInt32 nsoffset = name.FindChar(kNameSpaceSeparator);
    if (-1 != nsoffset) {
        name.Left(prefix, nsoffset);
        name.Cut(0, nsoffset+1);
    }

    if (0 < prefix.Length())
        aPrefix = NS_NewAtom(prefix);

    aName = NS_NewAtom(name);

    return NS_OK;
}


// nsIDOMEventCapturer and nsIDOMEventReceiver Interface Implementations

NS_IMETHODIMP
nsXULDocument::AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
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
nsXULDocument::RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    if (mListenerManager) {
        mListenerManager->RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULDocument::AddEventListener(const nsString& aType, nsIDOMEventListener* aListener,
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
nsXULDocument::RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener,
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
nsXULDocument::DispatchEvent(nsIDOMEvent* aEvent)
{
  // Obtain a presentation context
  PRInt32 count = GetNumberOfShells();
  if (count == 0)
    return NS_OK;

  nsCOMPtr<nsIPresShell> shell = getter_AddRefs(GetShellAt(0));
  
  // Retrieve the context
  nsCOMPtr<nsIPresContext> presContext;
  shell->GetPresContext(getter_AddRefs(presContext));

  nsCOMPtr<nsIEventStateManager> esm;
  if (NS_SUCCEEDED(presContext->GetEventStateManager(getter_AddRefs(esm)))) {
    return esm->DispatchNewEvent(NS_STATIC_CAST(nsIDocument*, this), aEvent);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULDocument::CreateEvent(const nsString& aEventType, nsIDOMEvent** aReturn)
{
  // Obtain a presentation context
  PRInt32 count = GetNumberOfShells();
  if (count == 0)
    return NS_OK;

  nsCOMPtr<nsIPresShell> shell = getter_AddRefs(GetShellAt(0));
  
  // Retrieve the context
  nsCOMPtr<nsIPresContext> presContext;
  shell->GetPresContext(getter_AddRefs(presContext));

  if (presContext) {
    nsCOMPtr<nsIEventListenerManager> lm;
    if (NS_SUCCEEDED(GetListenerManager(getter_AddRefs(lm)))) {
      return lm->CreateEvent(presContext, nsnull, aEventType, aReturn);
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULDocument::GetListenerManager(nsIEventListenerManager** aResult)
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
nsXULDocument::GetNewListenerManager(nsIEventListenerManager **aResult)
{
    return nsComponentManager::CreateInstance(kEventListenerManagerCID,
                                        nsnull,
                                        NS_GET_IID(nsIEventListenerManager),
                                        (void**) aResult);
}

NS_IMETHODIMP
nsXULDocument::HandleEvent(nsIDOMEvent *aEvent)
{
  return DispatchEvent(aEvent);
}

nsresult
nsXULDocument::CaptureEvent(const nsString& aType)
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
nsXULDocument::ReleaseEvent(const nsString& aType)
{
  if (mListenerManager) {
    //mListenerManager->ReleaseEvent(aListener);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsXULDocument::OpenWidgetItem(nsIContent* aElement)
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
        nsCAutoString tagstrC;
        tagstrC.AssignWithConversion(tagStr);
        PR_LOG(gXULLog, PR_LOG_DEBUG,
               ("xuldoc open-widget-item %s",
                (const char*) tagstrC));
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
nsXULDocument::CloseWidgetItem(nsIContent* aElement)
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
        nsCAutoString tagstrC;
        tagstrC.AssignWithConversion(tagStr);

        PR_LOG(gXULLog, PR_LOG_DEBUG,
               ("xuldoc close-widget-item %s",
                (const char*) tagstrC));
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
nsXULDocument::RebuildWidgetItem(nsIContent* aElement)
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

        nsCAutoString tagstrC;
        tagstrC.AssignWithConversion(tagStr);
        PR_LOG(gXULLog, PR_LOG_DEBUG,
               ("xuldoc close-widget-item %s",
                (const char*) tagstrC));
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
nsXULDocument::CreateElement(nsINodeInfo *aNodeInfo, nsIContent** aResult)
{
    NS_ENSURE_ARG_POINTER(aNodeInfo);
    NS_ENSURE_ARG_POINTER(aResult);

    nsresult rv;
    nsCOMPtr<nsIContent> result;

    if (aNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
        rv = nsXULElement::Create(aNodeInfo, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }
    else if (aNodeInfo->NamespaceEquals(kNameSpaceID_HTML)) {
        rv = gHTMLElementFactory->CreateInstanceByTag(aNodeInfo,
                                                      getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        if (! result)
            return NS_ERROR_UNEXPECTED;
    }
    else {
        PRInt32 namespaceID;
        aNodeInfo->GetNamespaceID(namespaceID);

        nsCOMPtr<nsIElementFactory> elementFactory;
        GetElementFactory(namespaceID, getter_AddRefs(elementFactory));

        rv = elementFactory->CreateInstanceByTag(aNodeInfo,
                                                 getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        if (! result)
            return NS_ERROR_UNEXPECTED;
    }

    result->SetContentID(mNextContentID++);

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}


nsresult
nsXULDocument::PrepareToLoad(nsISupports* aContainer,
                             const char* aCommand,
                             nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup,
                             nsIParser** aResult)
{
    nsresult rv;

    // Get the document's principal
    nsCOMPtr<nsISupports> owner;
    rv = aChannel->GetOwner(getter_AddRefs(owner));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(owner);

    return PrepareToLoadPrototype(mDocumentURL, aCommand, principal, aResult);
}


nsresult
nsXULDocument::PrepareToLoadPrototype(nsIURI* aURI, const char* aCommand,
                                      nsIPrincipal* aDocumentPrincipal,
                                      nsIParser** aResult)
{
    nsresult rv;

    // Create a new prototype document
    rv = NS_NewXULPrototypeDocument(nsnull, NS_GET_IID(nsIXULPrototypeDocument), getter_AddRefs(mCurrentPrototype));
    if (NS_FAILED(rv)) return rv;

    // Bootstrap the master document prototype
    if (! mMasterPrototype) {
        mMasterPrototype = mCurrentPrototype;
        mMasterPrototype->SetDocumentPrincipal(aDocumentPrincipal);
    }

    rv = mCurrentPrototype->SetURI(aURI);
    if (NS_FAILED(rv)) return rv;

    // Create a XUL content sink, a parser, and kick off a load for
    // the overlay.
    nsCOMPtr<nsIXULContentSink> sink;
    rv = nsComponentManager::CreateInstance(kXULContentSinkCID,
                                            nsnull,
                                            NS_GET_IID(nsIXULContentSink),
                                            getter_AddRefs(sink));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create XUL content sink");
    if (NS_FAILED(rv)) return rv;

    rv = sink->Init(this, mCurrentPrototype);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to initialize datasource sink");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIParser> parser;
    rv = nsComponentManager::CreateInstance(kParserCID,
                                            nsnull,
                                            kIParserIID,
                                            getter_AddRefs(parser));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create parser");
    if (NS_FAILED(rv)) return rv;

    parser->SetCommand(nsCRT::strcmp(aCommand, "view-source") ? eViewNormal :
      eViewSource);

    nsAutoString charset(NS_ConvertASCIItoUCS2("UTF-8"));
    parser->SetDocumentCharset(charset, kCharsetFromDocTypeDefault);
    parser->SetContentSink(sink); // grabs a reference to the parser

    *aResult = parser;
    NS_ADDREF(*aResult);
    return NS_OK;
}


nsresult
nsXULDocument::ApplyPersistentAttributes()
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
        rv = gXULUtils->MakeElementID(this, NS_ConvertASCIItoUCS2(uri), id);
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
nsXULDocument::ApplyPersistentAttributesToElements(nsIRDFResource* aResource, nsISupportsArray* aElements)
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

//----------------------------------------------------------------------
//
// nsXULDocument::ContextStack
//

nsXULDocument::ContextStack::ContextStack()
    : mTop(nsnull), mDepth(0)
{
}

nsXULDocument::ContextStack::~ContextStack()
{
    while (mTop) {
        Entry* doomed = mTop;
        mTop = mTop->mNext;
        NS_IF_RELEASE(doomed->mElement);
        delete doomed;
    }
}

nsresult
nsXULDocument::ContextStack::Push(nsXULPrototypeElement* aPrototype, nsIContent* aElement)
{
    Entry* entry = new Entry;
    if (! entry)
        return NS_ERROR_OUT_OF_MEMORY;

    entry->mPrototype = aPrototype;
    entry->mElement   = aElement;
    NS_IF_ADDREF(entry->mElement);
    entry->mIndex     = 0;

    entry->mNext = mTop;
    mTop = entry;

    ++mDepth;
    return NS_OK;
}

nsresult
nsXULDocument::ContextStack::Pop()
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    Entry* doomed = mTop;
    mTop = mTop->mNext;
    --mDepth;

    NS_IF_RELEASE(doomed->mElement);
    delete doomed;
    return NS_OK;
}

nsresult
nsXULDocument::ContextStack::Peek(nsXULPrototypeElement** aPrototype,
                                           nsIContent** aElement,
                                           PRInt32* aIndex)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    *aPrototype = mTop->mPrototype;
    *aElement   = mTop->mElement;
    NS_IF_ADDREF(*aElement);
    *aIndex     = mTop->mIndex;

    return NS_OK;
}


nsresult
nsXULDocument::ContextStack::SetTopIndex(PRInt32 aIndex)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    mTop->mIndex = aIndex;
    return NS_OK;
}


PRBool
nsXULDocument::ContextStack::IsInsideXULTemplate()
{
    if (mDepth) {
        nsCOMPtr<nsIContent> element = dont_QueryInterface(mTop->mElement);
        while (element) {
            PRInt32 nameSpaceID;
            element->GetNameSpaceID(nameSpaceID);
            if (nameSpaceID == kNameSpaceID_XUL) {
                nsCOMPtr<nsIAtom> tag;
                element->GetTag(*getter_AddRefs(tag));
                if (tag.get() == kTemplateAtom) {
                    return PR_TRUE;
                }
            }

            nsCOMPtr<nsIContent> parent;
            element->GetParent(*getter_AddRefs(parent));
            element = parent;
        }
    }
    return PR_FALSE;
}


//----------------------------------------------------------------------
//
// Content model walking routines
//

nsresult
nsXULDocument::PrepareToWalk()
{
    // Prepare to walk the mCurrentPrototype
    nsresult rv;

    // Keep an owning reference to the prototype document so that its
    // elements aren't yanked from beneath us.
    mPrototypes->AppendElement(mCurrentPrototype);

    // Push the overlay references onto our overlay processing
    // stack. GetOverlayReferences() will return an ordered array of
    // overlay references...
    nsCOMPtr<nsISupportsArray> overlays;
    rv = mCurrentPrototype->GetOverlayReferences(getter_AddRefs(overlays));
    if (NS_FAILED(rv)) return rv;

    // ...and we preserve this ordering by appending to our
    // mUnloadedOverlays array in reverse order
    PRUint32 count;
    overlays->Count(&count);
    for (PRInt32 i = count - 1; i >= 0; --i) {
        nsISupports* isupports = overlays->ElementAt(i);
        mUnloadedOverlays->AppendElement(isupports);
        NS_IF_RELEASE(isupports);
    }


    // Now check the chrome registry for any additional overlays.
    rv = AddChromeOverlays();

    // Get the prototype's root element and initialize the context
    // stack for the prototype walk.
    nsXULPrototypeElement* proto;
    rv = mCurrentPrototype->GetRootElement(&proto);
    if (NS_FAILED(rv)) return rv;


    if (! proto) {
#ifdef PR_LOGGING
        nsCOMPtr<nsIURI> url;
        rv = mCurrentPrototype->GetURI(getter_AddRefs(url));
        if (NS_FAILED(rv)) return rv;

        nsXPIDLCString urlspec;
        rv = url->GetSpec(getter_Copies(urlspec));
        if (NS_FAILED(rv)) return rv;

        PR_LOG(gXULLog, PR_LOG_ALWAYS,
               ("xul: error parsing '%s'", (const char*) urlspec));
#endif

        return NS_OK;
    }

    // Do one-time initialization if we're preparing to walk the
    // master document's prototype.
    nsCOMPtr<nsIContent> root;

    if (mState == eState_Master) {
        rv = CreateElement(proto, getter_AddRefs(root));
        if (NS_FAILED(rv)) return rv;

        SetRootContent(root);

        nsCOMPtr<nsINodeInfo> nodeInfo;
        mNodeInfoManager->GetNodeInfo(NS_ConvertASCIItoUCS2("form"), nsnull,
                                      kNameSpaceID_HTML,
                                      *getter_AddRefs(nodeInfo));
        // Create the document's "hidden form" element which will wrap all
        // HTML form elements that turn up.
        nsCOMPtr<nsIContent> form;
        rv = gHTMLElementFactory->CreateInstanceByTag(nodeInfo,
                                                      getter_AddRefs(form));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create form element");
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIDOMHTMLFormElement> htmlFormElement = do_QueryInterface(form);
        NS_ASSERTION(htmlFormElement != nsnull, "not an nSIDOMHTMLFormElement");
        if (! htmlFormElement)
            return NS_ERROR_UNEXPECTED;

        rv = SetForm(htmlFormElement);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set form element");
        if (NS_FAILED(rv)) return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIContent> content = do_QueryInterface(form);
        NS_ASSERTION(content != nsnull, "not an nsIContent");
        if (! content)
            return NS_ERROR_UNEXPECTED;

        // XXX Would like to make this anonymous, but still need the
        // form's frame to get built. For now make it explicit.
        rv = root->InsertChildAt(content, 0, PR_FALSE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add anonymous form element");
        if (NS_FAILED(rv)) return rv;

        // Add the root element to the XUL document's ID-to-element map.
        rv = AddElementToMap(root);
        if (NS_FAILED(rv)) return rv;

        // Add a dummy channel to the load group as a placeholder for the document
        // load
        rv = PlaceholderChannel::Create(getter_AddRefs(mPlaceholderChannel));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);
        
        if (group) {
            rv = mPlaceholderChannel->SetLoadGroup(group);
            if (NS_FAILED(rv)) return rv;

            rv = group->AddChannel(mPlaceholderChannel, nsnull);
            if (NS_FAILED(rv)) return rv;
        }
    }

    rv = mContextStack.Push(proto, root);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
nsXULDocument::AddChromeOverlays()
{
    nsresult rv;
    NS_WITH_SERVICE(nsIChromeRegistry, reg, kChromeRegistryCID, &rv);

    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsISimpleEnumerator> oe;

    {
        nsCOMPtr<nsIURI> uri;
        rv = mCurrentPrototype->GetURI(getter_AddRefs(uri));
        if (NS_FAILED(rv)) return rv;

        reg->GetOverlays(uri, getter_AddRefs(oe));
    }

    if (!oe)
        return NS_OK;

    PRBool moreElements;
    oe->HasMoreElements(&moreElements);

    while (moreElements) {
        nsCOMPtr<nsISupports> next;
        oe->GetNext(getter_AddRefs(next));
        if (!next)
            return NS_OK;

        nsCOMPtr<nsIURI> uri = do_QueryInterface(next);
        if (!uri)
            return NS_OK;

        mUnloadedOverlays->AppendElement(uri);

        oe->HasMoreElements(&moreElements);
    }

    return NS_OK;
}


nsresult
nsXULDocument::ResumeWalk()
{
    // Walk the prototype and build the delegate content model. The
    // walk is performed in a top-down, left-to-right fashion. That
    // is, a parent is built before any of its children; a node is
    // only built after all of its siblings to the left are fully
    // constructed.
    //
    // It is interruptable so that transcluded documents (e.g.,
    // <html:script src="..." />) can be properly re-loaded if the
    // cached copy of the document becomes stale.
    nsresult rv;

    while (1) {
        // Begin (or resume) walking the current prototype.

        while (mContextStack.Depth() > 0) {
            // Look at the top of the stack to determine what we're
            // currently working on.
            nsXULPrototypeElement* proto;
            nsCOMPtr<nsIContent> element;
            PRInt32 indx;
            rv = mContextStack.Peek(&proto, getter_AddRefs(element), &indx);
            if (NS_FAILED(rv)) return rv;

            if (indx >= proto->mNumChildren) {
                if (element && ((mState == eState_Master) || (mContextStack.Depth() > 2))) {
                    // We've processed all of the prototype's children.
                    // Check the element for a 'datasources' attribute, in
                    // which case we'll need to create a template builder
                    // and construct the first 'ply' of elements beneath
                    // it.
                    //
                    // N.B. that we do this -after- all other XUL children
                    // have been created: this ensures that there'll be a
                    // <template> tag available when we try to build that
                    // first ply of generated elements.
                    //
                    // N.B. that we do -not- do this if we are dealing
                    // with an 'overlay element'; that is, an element
                    // in the first ply of an overlay
                    // document. OverlayForwardReference::Merge() will
                    // handle that case.
                    rv = CheckTemplateBuilder(element);
                    if (NS_FAILED(rv)) return rv;
                }

                // Now pop the context stack back up to the parent
                // element and continue the prototype walk.
                mContextStack.Pop();
                continue;
            }

            // Grab the next child, and advance the current context stack
            // to the next sibling to our right.
            nsXULPrototypeNode* childproto = proto->mChildren[indx];
            mContextStack.SetTopIndex(++indx);

            switch (childproto->mType) {
            case nsXULPrototypeNode::eType_Element: {
                // An 'element', which may contain more content.
                nsXULPrototypeElement* protoele =
                    NS_REINTERPRET_CAST(nsXULPrototypeElement*, childproto);

                nsCOMPtr<nsIContent> child;

                if ((mState == eState_Master) || (mContextStack.Depth() > 1)) {
                    // We're in the master document -or -we're in an
                    // overlay, and far enough down into the overlay's
                    // content that we can simply build the delegates
                    // and attach them to the parent node.
                    rv = CreateElement(protoele, getter_AddRefs(child));
                    if (NS_FAILED(rv)) return rv;

                    // ...and append it to the content model.
                    rv = element->AppendChildTo(child, PR_FALSE);
                    if (NS_FAILED(rv)) return rv;

                    // ...but only do document-level hookup if we're
                    // in the master document. For an overlay, this
                    // will happend when the overlay is successfully
                    // resolved.
                    if (mState == eState_Master) {
                        rv = AddSubtreeToDocument(child);
                        if (NS_FAILED(rv)) return rv;
                    }
                }
                else {
                    // We're in the "first ply" of an overlay: the
                    // "hookup" nodes. Create an 'overlay' element so
                    // that we can continue to build content, and
                    // enter a forward reference so we can hook it up
                    // later.
                    rv = CreateOverlayElement(protoele, getter_AddRefs(child));
                    if (NS_FAILED(rv)) return rv;
                }

                // If it has children, push the element onto the context
                // stack and begin to process them.
                if (protoele->mNumChildren > 0) {
                    rv = mContextStack.Push(protoele, child);
                    if (NS_FAILED(rv)) return rv;
                }
            }
            break;

            case nsXULPrototypeNode::eType_Script: {
                // A script reference. Execute the script immediately;
                // this may have side effects in the content model.
                nsXULPrototypeScript* scriptproto =
                    NS_REINTERPRET_CAST(nsXULPrototypeScript*, childproto);

                if (scriptproto->mSrcURI) {
                    // A transcluded script reference; this may
                    // "block" our prototype walk if the script isn't
                    // cached, or the cached copy of the script is
                    // stale and must be reloaded.
                    PRBool blocked;
                    rv = LoadScript(scriptproto, &blocked);
                    if (NS_FAILED(rv)) return rv;

                    if (blocked)
                        return NS_OK;
                }
                else if (scriptproto->mScriptObject) {
                    // An inline script
                    rv = ExecuteScript(scriptproto->mScriptObject);
                    if (NS_FAILED(rv)) return rv;
                }
            }
            break;

            case nsXULPrototypeNode::eType_Text: {
                // A simple text node.
                nsCOMPtr<nsITextContent> text;
                rv = nsComponentManager::CreateInstance(kTextNodeCID,
                                                        nsnull,
                                                        NS_GET_IID(nsITextContent),
                                                        getter_AddRefs(text));
                if (NS_FAILED(rv)) return rv;

                nsXULPrototypeText* textproto =
                    NS_REINTERPRET_CAST(nsXULPrototypeText*, childproto);

                rv = text->SetText(textproto->mValue.GetUnicode(),
                                   textproto->mValue.Length(),
                                   PR_FALSE);

                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsIContent> child = do_QueryInterface(text);
                if (! child)
                    return NS_ERROR_UNEXPECTED;

				NS_ASSERTION(element,"element is null");
				if (!element) return NS_ERROR_FAILURE;

                rv = element->AppendChildTo(child, PR_FALSE);
                if (NS_FAILED(rv)) return rv;
            }
            break;

            }
        }

        // Once we get here, the context stack will have been
        // depleted. That means that the entire prototype has been
        // walked and content has been constructed.

        // If we're not already, mark us as now processing overlays.
        mState = eState_Overlay;

        PRUint32 count;
        mUnloadedOverlays->Count(&count);

        // If there are no overlay URIs, then we're done.
        if (! count)
            break;

        nsCOMPtr<nsIURI> uri =
            dont_AddRef(NS_REINTERPRET_CAST(nsIURI*, mUnloadedOverlays->ElementAt(count - 1)));

        mUnloadedOverlays->RemoveElementAt(count - 1);

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
            nsXPIDLCString urlspec;
            uri->GetSpec(getter_Copies(urlspec));

            PR_LOG(gXULLog, PR_LOG_DEBUG,
                   ("xul: loading overlay %s", (const char*) urlspec));
        }
#endif

        // Look in the prototype cache for the prototype document with
        // the specified URI.
        rv = gXULCache->GetPrototype(uri, getter_AddRefs(mCurrentPrototype));
        if (NS_FAILED(rv)) return rv;

        if (gXULUtils->UseXULCache() && mCurrentPrototype) {
            // Found the overlay's prototype in the cache.
            rv = AddPrototypeSheets();
            if (NS_FAILED(rv)) return rv;

            // Now prepare to walk the prototype to create its content
            rv = PrepareToWalk();
            if (NS_FAILED(rv)) return rv;

            PR_LOG(gXULLog, PR_LOG_DEBUG, ("xul: overlay was cached"));
        }
        else {
            // Not there. Initiate a load.
            PR_LOG(gXULLog, PR_LOG_DEBUG, ("xul: overlay was not cached"));

            nsCOMPtr<nsIParser> parser;
            rv = PrepareToLoadPrototype(uri, "view", nsnull, getter_AddRefs(parser));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(parser);
            if (! listener)
                return NS_ERROR_UNEXPECTED;

            // Add an observer to the parser; this'll get called when
            // Necko fires its On[Start|Stop]Request() notifications,
            // and will let us recover from a missing overlay.
            ParserObserver* parserObserver = new ParserObserver(this);
            if (! parserObserver)
                return NS_ERROR_OUT_OF_MEMORY;

            NS_ADDREF(parserObserver);
            parser->Parse(uri, parserObserver);
            NS_RELEASE(parserObserver);

            // If we're a keybinding document, the overlay load must
            // occur synchronously.
            if (mIsKeyBindingDoc) {
              nsCOMPtr<nsIChannel> channel;
              rv = NS_OpenURI(getter_AddRefs(channel), uri, nsnull);
              if (NS_FAILED(rv)) return rv;
              nsCOMPtr<nsIInputStream> in;
              PRUint32 sourceOffset = 0;
              rv = channel->OpenInputStream(getter_AddRefs(in));

              // If we couldn't open the channel, then just return.
              if (NS_FAILED(rv)) return NS_OK;

              NS_ASSERTION(in != nsnull, "no input stream");
              if (! in) return NS_ERROR_FAILURE;

              rv = NS_ERROR_OUT_OF_MEMORY;
              nsProxyLoadStream* proxy = new nsProxyLoadStream();
              if (! proxy)
                return NS_ERROR_FAILURE;
  
              listener->OnStartRequest(channel, nsnull);
              while (PR_TRUE) {
                char buf[1024];
                PRUint32 readCount;

                if (NS_FAILED(rv = in->Read(buf, sizeof(buf), &readCount)))
                    break; // error

                if (readCount == 0)
                    break; // eof

                proxy->SetBuffer(buf, readCount);

                rv = listener->OnDataAvailable(channel, nsnull, proxy, sourceOffset, readCount);
                sourceOffset += readCount;
                if (NS_FAILED(rv))
                    break;
              }

              listener->OnStopRequest(channel, nsnull, NS_OK, nsnull);

              // don't leak proxy!
              proxy->Close();
              delete proxy;
            }
            else {
              nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);
              rv = NS_OpenURI(listener, nsnull, uri, nsnull, group);
              if (NS_FAILED(rv)) return rv;
            }

            // Return to the main event loop and eagerly await the
            // overlay load's completion. When the content sink
            // completes, it will trigger an EndLoad(), which'll wind
            // us back up here, in ResumeWalk().
            return NS_OK;
        }
    }

    // If we get here, there is nothing left for us to walk. The content
    // model is built and ready for layout.
    rv = ResolveForwardReferences();
    if (NS_FAILED(rv)) return rv;

    rv = ApplyPersistentAttributes();
    if (NS_FAILED(rv)) return rv;

    // Everything after this point we only want to do once we're
    // certain that we've been embedded in a presentation shell.

    StartLayout();

    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->EndLoad(this);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }

    // Remove the placeholder channel; if we're the last channel in the
    // load group, this will fire the OnEndDocumentLoad() method in the
    // docshell, and run the onload handlers, etc.
    nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);
    if (group) {
        rv = group->RemoveChannel(mPlaceholderChannel, nsnull, NS_OK, nsnull);
        if (NS_FAILED(rv)) return rv;

        mPlaceholderChannel = nsnull;
    }

    return rv;
}


nsresult
nsXULDocument::LoadScript(nsXULPrototypeScript* aScriptProto, PRBool* aBlock)
{
    // Load a transcluded script
    nsresult rv;

    if (aScriptProto->mScriptObject) {
        rv = ExecuteScript(aScriptProto->mScriptObject);

        // Ignore return value from execution, and don't block
        *aBlock = PR_FALSE;
    }
    else {
        // Set the current script prototype so that OnUnicharStreamComplete
        // can get report the right file if there are errors in the script.
        NS_ASSERTION(!mCurrentScriptProto,
                     "still loading a script when starting another load?");
        mCurrentScriptProto = aScriptProto;

        if (aScriptProto->mSrcLoading) {
            // Another XULDocument load has started, which is still in progress.
            // Remember to ResumeWalk this document when the load completes.
            mNextSrcLoadWaiter = aScriptProto->mSrcLoadWaiters;
            aScriptProto->mSrcLoadWaiters = this;
            NS_ADDREF_THIS();
        }
        else {
            nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);

            // N.B., the loader will be released in OnUnicharStreamComplete
            nsIStreamLoader* loader;
            rv = NS_NewStreamLoader(&loader, aScriptProto->mSrcURI, this, nsnull, group);
            if (NS_FAILED(rv)) return rv;

            aScriptProto->mSrcLoading = PR_TRUE;
        }

        // Block until OnUnicharStreamComplete resumes us.
        *aBlock = PR_TRUE;
    }
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::OnStreamComplete(nsIStreamLoader* aLoader,
                                nsISupports* context,
                                nsresult aStatus,
                                PRUint32 stringLen,
                                const char* string)
{
    // print a load error on bad status
    if (NS_FAILED(aStatus))
    {
      if (aLoader)
      {
        nsCOMPtr<nsIChannel> channel;
        aLoader->GetChannel(getter_AddRefs(channel));
        if (channel)
        {
          nsCOMPtr<nsIURI> uri;
          channel->GetURI(getter_AddRefs(uri));
          if (uri)
          {
            char* uriSpec;
            uri->GetSpec(&uriSpec);
            printf("Failed to load %s\n", uriSpec ? uriSpec : "");
            nsCRT::free(uriSpec);
          }
        }
      }
    }
    
    // This is the completion routine that will be called when a
    // transcluded script completes. Compile and execute the script
    // if the load was successful, then continue building content
    // from the prototype.
    nsresult rv;

    NS_ASSERTION(mCurrentScriptProto && mCurrentScriptProto->mSrcLoading,
                 "script source not loading on unichar stream complete?");

    // Clear mCurrentScriptProto now, but save it first for use below in
    // the compile/execute code, and in the while loop that resumes walks
    // of other documents that raced to load this script
    nsXULPrototypeScript* scriptProto = mCurrentScriptProto;
    mCurrentScriptProto = nsnull;

    // Clear the prototype's loading flag before executing the script or
    // resuming document walks, in case any of those control flows starts a
    // new script load.
    scriptProto->mSrcLoading = PR_FALSE;

    if (NS_SUCCEEDED(aStatus)) {
        nsString stringStr; stringStr.AssignWithConversion(string, stringLen);
        rv = scriptProto->Compile(stringStr.GetUnicode(), stringLen,
                                  scriptProto->mSrcURI, 1,
                                  this, mMasterPrototype);
        aStatus = rv;
        if (NS_SUCCEEDED(rv)) {
            rv = ExecuteScript(scriptProto->mScriptObject);
        }
        // ignore any evaluation errors
    }

    // balance the addref we added in LoadScript()
    NS_RELEASE(aLoader);

    rv = ResumeWalk();

    // Load a pointer to the prototype-script's list of nsXULDocuments who
    // raced to load the same script
    nsXULDocument** docp = &scriptProto->mSrcLoadWaiters;

    // Resume walking other documents that waited for this one's load, first
    // executing the script we just compiled, in each doc's script context
    nsXULDocument* doc;
    while ((doc = *docp) != nsnull) {
        NS_ASSERTION(doc->mCurrentScriptProto == scriptProto,
                     "waiting for wrong script to load?");
        doc->mCurrentScriptProto = nsnull;

        // Unlink doc from scriptProto's list before executing and resuming
        *docp = doc->mNextSrcLoadWaiter;
        doc->mNextSrcLoadWaiter = nsnull;

        // Execute only if we loaded and compiled successfully, then resume
        if (NS_SUCCEEDED(aStatus)) {
            doc->ExecuteScript(scriptProto->mScriptObject);
        }
        doc->ResumeWalk();
        NS_RELEASE(doc);
    }

    return rv;
}


nsresult
nsXULDocument::ExecuteScript(JSObject* aScriptObject)
{
    // Execute the precompiled script with the given version
    nsresult rv;

    NS_ASSERTION(mScriptGlobalObject != nsnull, "no script global object");
    if (! mScriptGlobalObject)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIScriptContext> context;
    rv = mScriptGlobalObject->GetContext(getter_AddRefs(context));
    if (NS_FAILED(rv)) return rv;

    rv = context->ExecuteScript(aScriptObject, nsnull, nsnull, nsnull);
    return rv;
}


nsresult
nsXULDocument::CreateElement(nsXULPrototypeElement* aPrototype, nsIContent** aResult)
{
    // Create a content model element from a prototype element.
    NS_PRECONDITION(aPrototype != nsnull, "null ptr");
    if (! aPrototype)
        return NS_ERROR_NULL_POINTER;

    nsresult rv = NS_OK;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_ALWAYS)) {
        nsAutoString tagstr;
        aPrototype->mNodeInfo->GetQualifiedName(tagstr);

        nsCAutoString tagstrC;
        tagstrC.AssignWithConversion(tagstr);
        PR_LOG(gXULLog, PR_LOG_ALWAYS,
               ("xul: creating <%s> from prototype",
                (const char*) tagstrC));
    }
#endif

    nsCOMPtr<nsIContent> result;

    if (aPrototype->mNodeInfo->NamespaceEquals(kNameSpaceID_HTML)) {
        // If it's an HTML element, it's gonna be heavyweight no matter
        // what. So we need to copy everything out of the prototype
        // into the element.

        gHTMLElementFactory->CreateInstanceByTag(aPrototype->mNodeInfo,
                                                 getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        if (! result)
            return NS_ERROR_UNEXPECTED;

        rv = result->SetDocument(this, PR_FALSE, PR_TRUE);
        if (NS_FAILED(rv)) return rv;

        rv = AddAttributes(aPrototype, result);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIFormControl> htmlformctrl = do_QueryInterface(result);
        if (htmlformctrl) {
            nsCOMPtr<nsIDOMHTMLFormElement> docform;
            rv = GetForm(getter_AddRefs(docform));
            if (NS_FAILED(rv)) return rv;

            NS_ASSERTION(docform != nsnull, "no document form");
            if (! docform)
                return NS_ERROR_UNEXPECTED;

            htmlformctrl->SetForm(docform);
        }
    }
    else if (!aPrototype->mNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
        // If it's not a XUL element, it's gonna be heavyweight no matter
        // what. So we need to copy everything out of the prototype
        // into the element.

        PRInt32 namespaceID;
        aPrototype->mNodeInfo->GetNamespaceID(namespaceID);

        nsCOMPtr<nsIElementFactory> elementFactory;
        GetElementFactory(namespaceID,
                          getter_AddRefs(elementFactory));
        elementFactory->CreateInstanceByTag(aPrototype->mNodeInfo,
                                            getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        if (! result)
            return NS_ERROR_UNEXPECTED;

        rv = result->SetDocument(this, PR_FALSE, PR_TRUE);
        if (NS_FAILED(rv)) return rv;

        rv = AddAttributes(aPrototype, result);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // If it's a XUL element, it'll be lightweight until somebody
        // monkeys with it.
        rv = nsXULElement::Create(aPrototype, this, PR_TRUE, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        // We also need to pay special attention to the keyset tag to set up a listener
        if (aPrototype->mNodeInfo->Equals(kKeysetAtom, kNameSpaceID_XUL) &&
            ! mIsKeyBindingDoc) {
            // Create our nsXULKeyListener and hook it up.
            nsCOMPtr<nsIXULKeyListener> keyListener;
            rv = nsComponentManager::CreateInstance(kXULKeyListenerCID,
                                                    nsnull,
                                                    NS_GET_IID(nsIXULKeyListener),
                                                    getter_AddRefs(keyListener));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a key listener");
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIDOMEventListener> domEventListener = do_QueryInterface(keyListener);
            if (domEventListener) {
                // Init the listener with the keyset node
                nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(result);
                keyListener->Init(domElement, this);

                AddEventListener(NS_ConvertASCIItoUCS2("keypress"), domEventListener, PR_FALSE);
                AddEventListener(NS_ConvertASCIItoUCS2("keydown"),  domEventListener, PR_FALSE);
                AddEventListener(NS_ConvertASCIItoUCS2("keyup"),    domEventListener, PR_FALSE);
            }
        }
    }

    result->SetContentID(mNextContentID++);

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}

nsresult
nsXULDocument::CreateOverlayElement(nsXULPrototypeElement* aPrototype, nsIContent** aResult)
{
    nsresult rv;

    // This doesn't really do anything except create a placeholder
    // element. I'd use an XML element, but it gets its knickers in a
    // knot with DOM ranges when you try to remove its children.
    nsCOMPtr<nsIContent> element;
    rv = nsXULElement::Create(aPrototype, this, PR_FALSE, getter_AddRefs(element));
    if (NS_FAILED(rv)) return rv;

    OverlayForwardReference* fwdref = new OverlayForwardReference(this, element);
    if (! fwdref)
        return NS_ERROR_OUT_OF_MEMORY;

    // transferring ownership to ya...
    rv = AddForwardReference(fwdref);
    if (NS_FAILED(rv)) return rv;

    *aResult = element;
    NS_ADDREF(*aResult);
    return NS_OK;
}


nsresult
nsXULDocument::AddAttributes(nsXULPrototypeElement* aPrototype, nsIContent* aElement)
{
    nsresult rv;

    for (PRInt32 i = 0; i < aPrototype->mNumAttributes; ++i) {
        nsXULPrototypeAttribute* protoattr = &(aPrototype->mAttributes[i]);

        rv = aElement->SetAttribute(protoattr->mNodeInfo,
                                    protoattr->mValue,
                                    PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
nsXULDocument::CheckTemplateBuilder(nsIContent* aElement)
{
    // Check aElement for a 'datasources' attribute, and if it has
    // one, create and initialize a template builder.
    nsresult rv;

    nsAutoString datasources;
    rv = aElement->GetAttribute(kNameSpaceID_None, kDataSourcesAtom, datasources);
    if (NS_FAILED(rv)) return rv;

    if (rv != NS_CONTENT_ATTR_HAS_VALUE)
        return NS_OK;

    // Get the document and its URL
    nsCOMPtr<nsIDocument> doc;
    rv = aElement->GetDocument(*getter_AddRefs(doc));
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(doc != nsnull, "no document");
    if (! doc)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIURI> docurl = dont_AddRef(doc->GetDocumentURL());

    // construct a new builder
    nsCOMPtr<nsIRDFContentModelBuilder> builder;
    rv = nsComponentManager::CreateInstance(kXULTemplateBuilderCID,
                                            nsnull,
                                            NS_GET_IID(nsIRDFContentModelBuilder),
                                            getter_AddRefs(builder));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create tree content model builder");
    if (NS_FAILED(rv)) return rv;

    rv = builder->SetRootContent(aElement);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set builder's root content element");
    if (NS_FAILED(rv)) return rv;

    // create a database for the builder
    nsCOMPtr<nsIRDFCompositeDataSource> db;
    rv = nsComponentManager::CreateInstance(kRDFCompositeDataSourceCID,
                                            nsnull,
                                            NS_GET_IID(nsIRDFCompositeDataSource),
                                            getter_AddRefs(db));

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to construct new composite data source");
    if (NS_FAILED(rv)) return rv;

	// check for magical attributes
	nsAutoString		attrib;
	if (NS_SUCCEEDED(rv = aElement->GetAttribute(kNameSpaceID_None, kCoalesceAtom, attrib))
		&& (rv == NS_CONTENT_ATTR_HAS_VALUE) && (attrib.EqualsWithConversion("false")))
	{
		db->SetCoalesceDuplicateArcs(PR_FALSE);
	}
	if (NS_SUCCEEDED(rv = aElement->GetAttribute(kNameSpaceID_None, kAllowNegativesAtom, attrib))
		&& (rv == NS_CONTENT_ATTR_HAS_VALUE) && (attrib.EqualsWithConversion("false")))
	{
		db->SetAllowNegativeAssertions(PR_FALSE);
	}

    // Grab the doc's principal...
    nsCOMPtr<nsIPrincipal> docPrincipal;
    rv = doc->GetPrincipal(getter_AddRefs(docPrincipal));
    if (NS_FAILED(rv)) return rv;

    // If we're an untrusted document, this will get the codebase
    // principal of the document for comparison to each URL that the
    // XUL wants to load. If we're a trusted document, this will just
    // be null.
    nsCOMPtr<nsICodebasePrincipal> codebase;

    if (docPrincipal.get() == gSystemPrincipal) {
        // If we're a privileged (e.g., chrome) document, then add the
        // local store as the first data source in the db. Note that
        // we _might_ not be able to get a local store if we haven't
        // got a profile to read from yet.
        nsCOMPtr<nsIRDFDataSource> localstore;
        rv = gRDFService->GetDataSource("rdf:local-store", getter_AddRefs(localstore));
        if (NS_SUCCEEDED(rv)) {
            rv = db->AddDataSource(localstore);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add local store to db");
            if (NS_FAILED(rv)) return rv;
        }
    }
    else {
        // We're not privileged. So grab our codebase for comparison
        // with the pricipals of the datasource's we're about to
        // load. If, for some reason, we don't have a codebase
        // principal, then panic and abort the template setup.
        codebase = do_QueryInterface(docPrincipal);

        NS_ASSERTION(codebase != nsnull, "no codebase principal for non-privileged XUL doc");
        if (! codebase)
            return NS_ERROR_UNEXPECTED;
    }

    // Parse datasources: they are assumed to be a whitespace
    // separated list of URIs; e.g.,
    //
    //     rdf:bookmarks rdf:history http://foo.bar.com/blah.cgi?baz=9
    //
    PRUint32 first = 0;

    while(1) {
        while (first < datasources.Length() && nsCRT::IsAsciiSpace(datasources.CharAt(first)))
            ++first;

        if (first >= datasources.Length())
            break;

        PRUint32 last = first;
        while (last < datasources.Length() && !nsCRT::IsAsciiSpace(datasources.CharAt(last)))
            ++last;

        nsAutoString uriStr;
        datasources.Mid(uriStr, first, last - first);
        first = last + 1;

        // A special 'dummy' datasource
        if (uriStr.EqualsWithConversion("rdf:null"))
            continue;

        rv = rdf_MakeAbsoluteURI(docurl, uriStr);
        if (NS_FAILED(rv)) return rv;

        if (codebase) {
            // Our document is untrusted, so check to see if we can
            // load the datasource that they've asked for.
            nsCOMPtr<nsIURI> uri;
            rv = NS_NewURI(getter_AddRefs(uri), uriStr);
            if (NS_FAILED(rv) || !uri)
                continue; // Necko will barf if our URI is weird

            nsCOMPtr<nsIPrincipal> principal;
            rv = gScriptSecurityManager->GetCodebasePrincipal(uri, getter_AddRefs(principal));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get codebase principal");
            if (NS_FAILED(rv)) return rv;

            PRBool same;
            rv = codebase->SameOrigin(principal, &same);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to test same origin");
            if (NS_FAILED(rv)) return rv;

            if (! same)
                continue;

            // If we get here, we've run the gauntlet, and the
            // datasource's URI has the same origin as our
            // document. Let it load!
        }

        nsCOMPtr<nsIRDFDataSource> ds;
        nsCAutoString uristrC;
        uristrC.AssignWithConversion(uriStr);

        rv = gRDFService->GetDataSource(uristrC, getter_AddRefs(ds));

        if (NS_FAILED(rv)) {
            // This is only a warning because the data source may not
            // be accessable for any number of reasons, including
            // security, a bad URL, etc.
#ifdef DEBUG
            nsCAutoString msg;
            msg.Append("unable to load datasource '");
            msg.AppendWithConversion(uriStr);
            msg.Append('\'');
            NS_WARNING((const char*) msg);
#endif
            continue;
        }

        rv = db->AddDataSource(ds);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add datasource to composite data source");
        if (NS_FAILED(rv)) return rv;
    }

    // add it to the set of builders in use by the document
    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(doc);
    if (! xuldoc)
        return NS_ERROR_UNEXPECTED;

    rv = xuldoc->AddContentModelBuilder(builder);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add builder to the document");
    if (NS_FAILED(rv)) return rv;

    rv = builder->SetDataBase(db);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set builder's database");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
    if (xulcontent) {
        // Mark the XUL element as being lazy, so the template builder
        // will run when layout first asks for these nodes.
        //
        //rv = xulcontent->ClearLazyState(eTemplateContentsBuilt | eContainerContentsBuilt);
        //if (NS_FAILED(rv)) return rv;

        xulcontent->SetLazyState(nsIXULContent::eChildrenMustBeRebuilt);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // Force construction of immediate template sub-content _now_.
        rv = builder->CreateContents(aElement);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create template contents");
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
nsXULDocument::AddPrototypeSheets()
{
    // Add mCurrentPrototype's style sheets to the document.
    nsresult rv;

    nsCOMPtr<nsISupportsArray> sheets;
    rv = mCurrentPrototype->GetStyleSheetReferences(getter_AddRefs(sheets));
    if (NS_FAILED(rv)) return rv;

    PRUint32 count;
    sheets->Count(&count);
    for (PRUint32 i = 0; i < count; ++i) {
        nsISupports* isupports = sheets->ElementAt(i);
        nsCOMPtr<nsIURI> uri = do_QueryInterface(isupports);
        NS_IF_RELEASE(isupports);

        NS_ASSERTION(uri != nsnull, "not a URI!!!");
        if (! uri)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsICSSStyleSheet> sheet;
        rv = gXULCache->GetStyleSheet(uri, getter_AddRefs(sheet));
        if (NS_FAILED(rv)) return rv;

        // If we don't get a style sheet from the cache, then the
        // really rigorous thing to do here would be to go out and try
        // to load it again. (This would allow us to do partial
        // invalidation of the cache, which would be cool, but would
        // also require some more thinking.)
        //
        // Reality is, we end up in this situation if, when parsing
        // the original XUL document, there -was- no style sheet at
        // the specified URL, or the stylesheet was empty. So, just
        // skip it.
        if (! sheet)
            continue;

        nsCOMPtr<nsICSSStyleSheet> newsheet;
        rv = sheet->Clone(*getter_AddRefs(newsheet));
        if (NS_FAILED(rv)) return rv;

        AddStyleSheet(newsheet);
    }

    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsXULDocument::OverlayForwardReference
//

nsForwardReference::Result
nsXULDocument::OverlayForwardReference::Resolve()
{
    // Resolve a forward reference from an overlay element; attempt to
    // hook it up into the main document.
    nsresult rv;

    nsAutoString id;
    rv = mOverlay->GetAttribute(kNameSpaceID_None, kIdAtom, id);
    if (NS_FAILED(rv)) return eResolve_Error;

    nsCOMPtr<nsIDOMElement> domtarget;
    rv = mDocument->GetElementById(id, getter_AddRefs(domtarget));
    if (NS_FAILED(rv)) return eResolve_Error;

    // If we can't find the element in the document, defer the hookup
    // until later.
    if (! domtarget)
        return eResolve_Later;

    nsCOMPtr<nsIContent> target = do_QueryInterface(domtarget);
    NS_ASSERTION(target != nsnull, "not an nsIContent");
    if (! target)
        return eResolve_Error;

    rv = Merge(target, mOverlay);
    if (NS_FAILED(rv)) return eResolve_Error;
    nsCAutoString idC;
    idC.AssignWithConversion(id);
    PR_LOG(gXULLog, PR_LOG_ALWAYS,
           ("xul: overlay resolved '%s'",
            (const char*) idC));

    mResolved = PR_TRUE;
    return eResolve_Succeeded;
}



nsresult
nsXULDocument::OverlayForwardReference::Merge(nsIContent* aTargetNode,
                                              nsIContent* aOverlayNode)
{
    nsresult rv;

    // This'll get set to PR_TRUE if a new 'datasources' attribute is
    // set on the element. In which case, we need to add a template
    // builder.
    PRBool datasources = PR_FALSE;

    {
        // Whack the attributes from aOverlayNode onto aTargetNode
        PRInt32 count;
        rv = aOverlayNode->GetAttributeCount(count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = 0; i < count; ++i) {
            PRInt32 nameSpaceID;
            nsCOMPtr<nsIAtom> attr, prefix;
            rv = aOverlayNode->GetAttributeNameAt(i, nameSpaceID, *getter_AddRefs(attr), *getter_AddRefs(prefix));
            if (NS_FAILED(rv)) return rv;

            if (nameSpaceID == kNameSpaceID_None && attr.get() == kIdAtom)
                continue;

            nsAutoString value;
            rv = aOverlayNode->GetAttribute(nameSpaceID, attr, value);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsINodeInfo> ni;
            aTargetNode->GetNodeInfo(*getter_AddRefs(ni));

            if (ni) {
                nsCOMPtr<nsINodeInfoManager> nimgr;
                ni->GetNodeInfoManager(*getter_AddRefs(nimgr));

                nimgr->GetNodeInfo(attr, prefix, nameSpaceID,
                                   *getter_AddRefs(ni));
            }

            rv = aTargetNode->SetAttribute(ni, value, PR_FALSE);
            if (NS_FAILED(rv)) return rv;

            if (/* ignore namespace && */ attr.get() == kDataSourcesAtom) {
                datasources = PR_TRUE;
            }
        }
    }

    {
        // Now move any kids
        PRInt32 count;
        rv = aOverlayNode->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = 0; i < count; ++i) {
            // Remove the child from the temporary "overlay
            // placeholder" node, and insert into the content model.
            nsCOMPtr<nsIContent> child;
            rv = aOverlayNode->ChildAt(0, *getter_AddRefs(child));
            if (NS_FAILED(rv)) return rv;

            rv = aOverlayNode->RemoveChildAt(0, PR_FALSE);
            if (NS_FAILED(rv)) return rv;

            rv = InsertElement(aTargetNode, child);
            if (NS_FAILED(rv)) return rv;
        }
    }

    // Add child and any descendants to the element map
    rv = mDocument->AddSubtreeToDocument(aTargetNode);
    if (NS_FAILED(rv)) return rv;

    if (datasources) {
        // If a new 'datasources' attribute was added via the
        // overlay, then initialize the <template> builder on the
        // element.
        rv = CheckTemplateBuilder(aTargetNode);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}



nsXULDocument::OverlayForwardReference::~OverlayForwardReference()
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_ALWAYS) && !mResolved) {
        nsAutoString id;
        mOverlay->GetAttribute(kNameSpaceID_None, kIdAtom, id);

        nsCAutoString idC;
        idC.AssignWithConversion(id);
        PR_LOG(gXULLog, PR_LOG_ALWAYS,
               ("xul: overlay failed to resolve '%s'",
                (const char*) idC));
    }
#endif
}


//----------------------------------------------------------------------
//
// nsXULDocument::BroadcasterHookup
//

nsForwardReference::Result
nsXULDocument::BroadcasterHookup::Resolve()
{
    nsresult rv;

    PRBool listener;
    rv = CheckBroadcasterHookup(mDocument, mObservesElement, &listener, &mResolved);
    if (NS_FAILED(rv)) return eResolve_Error;

    return mResolved ? eResolve_Succeeded : eResolve_Later;
}


nsXULDocument::BroadcasterHookup::~BroadcasterHookup()
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_ALWAYS) && !mResolved) {
        // Tell the world we failed
        nsresult rv;

        nsCOMPtr<nsIAtom> tag;
        rv = mObservesElement->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return;

        nsAutoString broadcasterID;
        nsAutoString attribute;

        if (tag.get() == kObservesAtom) {
            rv = mObservesElement->GetAttribute(kNameSpaceID_None, kElementAtom, broadcasterID);
            if (NS_FAILED(rv)) return;

            rv = mObservesElement->GetAttribute(kNameSpaceID_None, kAttributeAtom, attribute);
            if (NS_FAILED(rv)) return;
        }
        else {
            rv = mObservesElement->GetAttribute(kNameSpaceID_None, kObservesAtom, broadcasterID);
            if (NS_FAILED(rv)) return;

            attribute.AssignWithConversion("*");
        }

        nsAutoString tagStr;
        rv = tag->ToString(tagStr);
        if (NS_FAILED(rv)) return;

        nsCAutoString tagstrC, attributeC,broadcasteridC;
        tagstrC.AssignWithConversion(tagStr);
        attributeC.AssignWithConversion(attribute);
        broadcasteridC.AssignWithConversion(broadcasterID);
        PR_LOG(gXULLog, PR_LOG_ALWAYS,
               ("xul: broadcaster hookup failed <%s attribute='%s'> to %s",
                (const char*) tagstrC,
                (const char*) attributeC,
                (const char*) broadcasteridC));
    }
#endif
}


//----------------------------------------------------------------------


nsresult
nsXULDocument::CheckBroadcasterHookup(nsXULDocument* aDocument,
                                      nsIContent* aElement,
                                      PRBool* aNeedsHookup,
                                      PRBool* aDidResolve)
{
    // Resolve a broadcaster hookup. Look at the element that we're
    // trying to resolve: it could be an '<observes>' element, or just
    // a vanilla element with an 'observes' attribute on it.
    nsresult rv;

    *aDidResolve = PR_FALSE;

    PRInt32 nameSpaceID;
    rv = aElement->GetNameSpaceID(nameSpaceID);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMElement> listener;
    nsAutoString broadcasterID;
    nsAutoString attribute;

    if ((nameSpaceID == kNameSpaceID_XUL) && (tag.get() == kObservesAtom)) {
        // It's an <observes> element, which means that the actual
        // listener is the _parent_ node. This element should have an
        // 'element' attribute that specifies the ID of the
        // broadcaster element, and an 'attribute' element, which
        // specifies the name of the attribute to observe.
        nsCOMPtr<nsIContent> parent;
        rv = aElement->GetParent(*getter_AddRefs(parent));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIAtom> parentTag;
        rv = parent->GetTag(*getter_AddRefs(parentTag));
        if (NS_FAILED(rv)) return rv;

        // If we're still parented by an 'overlay' tag, then we haven't
        // made it into the real document yet. Defer hookup.
        if (parentTag.get() == kOverlayAtom) {
            *aNeedsHookup = PR_TRUE;
            return NS_OK;
        }

        listener = do_QueryInterface(parent);

        rv = aElement->GetAttribute(kNameSpaceID_None, kElementAtom, broadcasterID);
        if (NS_FAILED(rv)) return rv;

        rv = aElement->GetAttribute(kNameSpaceID_None, kAttributeAtom, attribute);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // It's a generic element, which means that we'll use the
        // value of the 'observes' attribute to determine the ID of
        // the broadcaster element, and we'll watch _all_ of its
        // values.
        rv = aElement->GetAttribute(kNameSpaceID_None, kObservesAtom, broadcasterID);
        if (NS_FAILED(rv)) return rv;

        // Bail if there's no broadcasterID
        if ((rv != NS_CONTENT_ATTR_HAS_VALUE) || (broadcasterID.Length() == 0)) {
            *aNeedsHookup = PR_FALSE;
            return NS_OK;
        }

        listener = do_QueryInterface(aElement);

        attribute.AssignWithConversion("*");
    }

    // Make sure we got a valid listener.
    NS_ASSERTION(listener != nsnull, "no listener");
    if (! listener)
        return NS_ERROR_UNEXPECTED;

    // Try to find the broadcaster element in the document.
    nsCOMPtr<nsIDOMElement> target;
    rv = aDocument->GetElementById(broadcasterID, getter_AddRefs(target));
    if (NS_FAILED(rv)) return rv;

    // If we can't find the broadcaster, then we'll need to defer the
    // hookup. We may need to resolve some of the other overlays
    // first.
    if (! target) {
        *aNeedsHookup = PR_TRUE;
        return NS_OK;
    }

    nsCOMPtr<nsIDOMXULElement> broadcaster = do_QueryInterface(target);
    if (! broadcaster) {
        *aNeedsHookup = PR_FALSE;
        return NS_OK; // not a XUL element, so we can't subscribe
    }

    rv = broadcaster->AddBroadcastListener(attribute, listener);
    if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
    // Tell the world we succeeded
    if (PR_LOG_TEST(gXULLog, PR_LOG_ALWAYS)) {
        nsCOMPtr<nsIContent> content =
            do_QueryInterface(listener);

        NS_ASSERTION(content != nsnull, "not an nsIContent");
        if (! content)
            return rv;

        nsCOMPtr<nsIAtom> tag2;
        rv = content->GetTag(*getter_AddRefs(tag2));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tagStr;
        rv = tag2->ToString(tagStr);
        if (NS_FAILED(rv)) return rv;

        nsCAutoString tagstrC, attributeC,broadcasteridC;
        tagstrC.AssignWithConversion(tagStr);
        attributeC.AssignWithConversion(attribute);
        broadcasteridC.AssignWithConversion(broadcasterID);
        PR_LOG(gXULLog, PR_LOG_ALWAYS,
               ("xul: broadcaster hookup <%s attribute='%s'> to %s",
                (const char*) tagstrC,
                (const char*) attributeC,
                (const char*) broadcasteridC));
    }
#endif

    *aNeedsHookup = PR_FALSE;
    *aDidResolve = PR_TRUE;
    return NS_OK;
}

nsresult
nsXULDocument::InsertElement(nsIContent* aParent, nsIContent* aChild)
{
    // Insert aChild appropriately into aParent, accountinf for a
    // 'pos' attribute set on aChild.
    nsresult rv;

    nsAutoString posStr;
    PRBool wasInserted = PR_FALSE;

    // insert after an element of a given id
    rv = aChild->GetAttribute(kNameSpaceID_None, kInsertAfterAtom, posStr);
    if (NS_FAILED(rv)) return rv;
    PRBool isInsertAfter = PR_TRUE;

    if (rv != NS_CONTENT_ATTR_HAS_VALUE) {
        rv = aChild->GetAttribute(kNameSpaceID_None, kInsertBeforeAtom, posStr);
        if (NS_FAILED(rv)) return rv;
        isInsertAfter = PR_FALSE;
    }
    
    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        nsCOMPtr<nsIDocument> document;
        rv = aParent->GetDocument(*getter_AddRefs(document));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIDOMXULDocument> xulDocument(do_QueryInterface(document));
        nsCOMPtr<nsIDOMElement> domElement;

        char* str = posStr.ToNewCString();
        char* remainder;
        char* token = nsCRT::strtok(str, ", ", &remainder);

        while (token) {
            rv = xulDocument->GetElementById(NS_ConvertASCIItoUCS2(token), getter_AddRefs(domElement));
            if (domElement) break;

            token = nsCRT::strtok(remainder, ", ", &remainder);
        }
        nsMemory::Free(str);
        if (NS_FAILED(rv)) return rv;

        if (domElement) {
            nsCOMPtr<nsIContent> content(do_QueryInterface(domElement));
            NS_ASSERTION(content != nsnull, "null ptr");
            if (!content)
                return NS_ERROR_UNEXPECTED;

            PRInt32 pos;
            aParent->IndexOf(content, pos);

            if (pos != -1) {
                pos = isInsertAfter ? pos + 1 : pos;
                rv = aParent->InsertChildAt(aChild, pos, PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                wasInserted = PR_TRUE;
            }
        }
    }

    if (!wasInserted) {
    
        rv = aChild->GetAttribute(kNameSpaceID_None, kPositionAtom, posStr);
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            // Positions are one-indexed.
            PRInt32 pos = posStr.ToInteger(NS_REINTERPRET_CAST(PRInt32*, &rv));
            if (NS_SUCCEEDED(rv)) {
                rv = aParent->InsertChildAt(aChild, pos - 1, PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                wasInserted = PR_TRUE;
            }
        }
    }

    if (! wasInserted) {
        rv = aParent->AppendChildTo(aChild, PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    // Both InsertChildAt() and AppendChildTo() only do a "shallow"
    // SetDocument(); make sure that we do a "deep" one now...
    nsCOMPtr<nsIDocument> doc;
    rv = aParent->GetDocument(*getter_AddRefs(doc));
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(doc != nsnull, "merging into null document");

    rv = aChild->SetDocument(doc, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}



PRBool
nsXULDocument::IsChromeURI(nsIURI* aURI)
{
    nsresult rv;
    nsXPIDLCString protocol;
    rv = aURI->GetScheme(getter_Copies(protocol));
    if (NS_SUCCEEDED(rv)) {
        if (PL_strcmp(protocol, "chrome") == 0) {
            return PR_TRUE;
        }
    }

    return PR_FALSE;
}

void 
nsXULDocument::GetElementFactory(PRInt32 aNameSpaceID, nsIElementFactory** aResult)
{
  nsresult rv;
  nsAutoString nameSpace;
  gNameSpaceManager->GetNameSpaceURI(aNameSpaceID, nameSpace);

  nsCAutoString progID = NS_ELEMENT_FACTORY_PROGID_PREFIX;
  progID.AppendWithConversion(nameSpace);

  // Retrieve the appropriate factory.
  NS_WITH_SERVICE(nsIElementFactory, elementFactory, progID, &rv);

  if (!elementFactory)
    elementFactory = gXMLElementFactory; // Nothing found. Use generic XML element.

  *aResult = elementFactory;
  NS_IF_ADDREF(*aResult);
}

//----------------------------------------------------------------------
//
// CachedChromeStreamListener
//

nsXULDocument::CachedChromeStreamListener::CachedChromeStreamListener(nsXULDocument* aDocument)
    : mDocument(aDocument)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mDocument);
}


nsXULDocument::CachedChromeStreamListener::~CachedChromeStreamListener()
{
    NS_RELEASE(mDocument);
}


NS_IMPL_ISUPPORTS2(nsXULDocument::CachedChromeStreamListener, nsIStreamObserver, nsIStreamListener);

NS_IMETHODIMP
nsXULDocument::CachedChromeStreamListener::OnStartRequest(nsIChannel* aChannel, nsISupports* acontext)
{
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::CachedChromeStreamListener::OnStopRequest(nsIChannel* aChannel,
                                                         nsISupports* aContext,
                                                         nsresult aStatus,
                                                         const PRUnichar* aErrorMsg)
{
    nsresult rv;
    rv = mDocument->PrepareToWalk();
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to prepare for walk");
    if (NS_FAILED(rv)) return rv;

    return mDocument->ResumeWalk();
}


NS_IMETHODIMP
nsXULDocument::CachedChromeStreamListener::OnDataAvailable(nsIChannel* aChannel,
                                                           nsISupports* aContext,
                                                           nsIInputStream* aInStr,
                                                           PRUint32 aSourceOffset,
                                                           PRUint32 aCount)
{
    NS_NOTREACHED("CachedChromeStream doesn't receive data");
    return NS_ERROR_UNEXPECTED;
}

//----------------------------------------------------------------------
//
// ParserObserver
//

nsXULDocument::ParserObserver::ParserObserver(nsXULDocument* aDocument)
    : mDocument(aDocument)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mDocument);
}

nsXULDocument::ParserObserver::~ParserObserver()
{
    NS_IF_RELEASE(mDocument);
}

NS_IMPL_ISUPPORTS(nsXULDocument::ParserObserver, NS_GET_IID(nsIStreamObserver));

NS_IMETHODIMP
nsXULDocument::ParserObserver::OnStartRequest(nsIChannel* aChannel,
                                              nsISupports* aContext)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::ParserObserver::OnStopRequest(nsIChannel* aChannel,
                                             nsISupports* aContext,
                                             nsresult aStatus,
                                             const PRUnichar* aErrorMsg)
{
    nsresult rv = NS_OK;

    if (NS_FAILED(aStatus)) {
        // If an overlay load fails, we need to nudge the prototype
        // walk along.
#define YELL_IF_MISSING_OVERLAY 1
#if defined(DEBUG) || defined(YELL_IF_MISSING_OVERLAY)
        nsCOMPtr<nsIURI> uri;
        aChannel->GetOriginalURI(getter_AddRefs(uri));

        nsXPIDLCString spec;
        uri->GetSpec(getter_Copies(spec));

        printf("*** Failed to load overlay %s\n", (const char*) spec);
#endif

        rv = mDocument->ResumeWalk();
    }

    // Drop the reference to the document to break cycle between the
    // document, the parser, the content sink, and the parser
    // observer.
    NS_RELEASE(mDocument);

    return rv;
}


//XIF ADDITIONS CODE REPLICATION FROM NSDOCUMENT


void nsXULDocument::BeginConvertToXIF(nsIXIFConverter *aConverter, nsIDOMNode* aNode)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  PRBool      isSynthetic = PR_TRUE;

  // Begin Conversion
  if (content) 
  {
    content->IsSynthetic(isSynthetic);
    if (PR_FALSE == isSynthetic)
    {
      content->BeginConvertToXIF(aConverter);
      content->ConvertContentToXIF(aConverter);
    }
  }
}

void nsXULDocument::ConvertChildrenToXIF(nsIXIFConverter * aConverter, nsIDOMNode* aNode)
{
  // Iterate through the children, convertion child nodes
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> child;
  result = aNode->GetFirstChild(getter_AddRefs(child));
    
  while ((result == NS_OK) && (child != nsnull))
  { 
    nsCOMPtr<nsIDOMNode> temp(child);
    result=ToXIF(aConverter,child);    
    result = temp->GetNextSibling(getter_AddRefs(child));
  }
}

void nsXULDocument::FinishConvertToXIF(nsIXIFConverter* aConverter, nsIDOMNode* aNode)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  PRBool      isSynthetic = PR_TRUE;

  if (content) 
  {
    content->IsSynthetic(isSynthetic);
    if (PR_FALSE == isSynthetic)
      content->FinishConvertToXIF(aConverter);
  }
}


NS_IMETHODIMP
nsXULDocument::ToXIF(nsIXIFConverter* aConverter, nsIDOMNode* aNode)
{
  nsresult result=NS_OK;
  nsCOMPtr<nsIDOMSelection> sel;
  aConverter->GetSelection(getter_AddRefs(sel));
  if (sel)
  {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));

    if (NS_SUCCEEDED(result) && content)
    {
      PRBool  isInSelection = IsInSelection(sel,content);

      if (isInSelection == PR_TRUE)
      {
        BeginConvertToXIF(aConverter,aNode);
        ConvertChildrenToXIF(aConverter,aNode);
        FinishConvertToXIF(aConverter,aNode);
      }
      else
      {
        ConvertChildrenToXIF(aConverter,aNode);
      }
    }
  }
  else
  {
    BeginConvertToXIF(aConverter,aNode);
    ConvertChildrenToXIF(aConverter,aNode);
    FinishConvertToXIF(aConverter,aNode);
  }
  return result;
} 

NS_IMETHODIMP
nsXULDocument::CreateXIF(nsString & aBuffer, nsIDOMSelection* aSelection)
{
  nsresult result=NS_OK;

  nsCOMPtr<nsIXIFConverter> converter;
  nsComponentManager::CreateInstance(kXIFConverterCID,
                           nsnull,
                           NS_GET_IID(nsIXIFConverter),
                           getter_AddRefs(converter));
  NS_ENSURE_TRUE(converter,NS_ERROR_FAILURE);
  converter->Init(aBuffer);

  converter->SetSelection(aSelection);

  converter->AddStartTag( NS_ConvertToString("section") , PR_TRUE); 
  converter->AddStartTag( NS_ConvertToString("section_head") , PR_TRUE);

  converter->BeginStartTag( NS_ConvertToString("document_info") );
  converter->AddAttribute(NS_ConvertToString("charset"),mCharSetID);
/*  nsCOMPtr<nsIURI> uri (getter_AddRefs(GetDocumentURL()));
  if (uri)
  {
    char* spec = 0;
    if (NS_SUCCEEDED(uri->GetSpec(&spec)) && spec)
    {
      converter->AddAttribute(NS_ConvertToString("uri"), NS_ConvertToString(spec));
      Recycle(spec);
    }
  }*/
  converter->FinishStartTag(NS_ConvertToString("document_info"),PR_TRUE,PR_TRUE);

  converter->AddEndTag(NS_ConvertToString("section_head"), PR_TRUE, PR_TRUE);
  converter->AddStartTag(NS_ConvertToString("section_body"), PR_TRUE);
//HACKHACKHACK DOCTYPE NOT DONE FOR XULDOCUMENTS>ASSIGN IN HTML
  nsString hack;
  hack.AssignWithConversion("DOCTYPE html PUBLIC \"-//w3c//dtd html 4.0 transitional//en\"");
  converter->AddMarkupDeclaration(hack);
  
  nsCOMPtr<nsIDOMDocumentType> doctype;
//if we have a selection to iterate find the root of the selection.
  nsCOMPtr<nsIDOMElement> rootElement;
  if (aSelection)
  {
    PRInt32 rangeCount;
    if (NS_SUCCEEDED(aSelection->GetRangeCount(&rangeCount)) && rangeCount == 1) //getter_AddRefs(node));
    {
      nsCOMPtr<nsIDOMNode> anchor;
      nsCOMPtr<nsIDOMNode> focus;
      if (NS_SUCCEEDED(aSelection->GetAnchorNode(getter_AddRefs(anchor))))
      {
        if (NS_SUCCEEDED(aSelection->GetFocusNode(getter_AddRefs(focus))))
        {
          if (focus.get() == anchor.get())
            rootElement = do_QueryInterface(focus);//set root to top of selection
          if (!rootElement)//maybe its a text node since both are the same. both parents are the same. pick one
          {
            nsCOMPtr<nsIDOMNode> parent;
            anchor->GetParentNode(getter_AddRefs(parent));
            rootElement = do_QueryInterface(parent);//set root to top of selection
          }
        }
      }
    }
  }
  if (!rootElement)
    result=GetDocumentElement(getter_AddRefs(rootElement));
  if (NS_SUCCEEDED(result) && rootElement)
  {  
    result=ToXIF(converter,rootElement);
  }
  converter->AddEndTag(NS_ConvertToString("section_body"), PR_TRUE, PR_TRUE);
  converter->AddEndTag(NS_ConvertToString("section"), PR_TRUE, PR_TRUE);
  return result;
}


//----------------------------------------------------------------------
//
// The XUL element factory
//

class XULElementFactoryImpl : public nsIElementFactory
{
protected:
  XULElementFactoryImpl();
  virtual ~XULElementFactoryImpl();

public:
  friend
  nsresult
  NS_NewXULElementFactory(nsIElementFactory** aResult);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIElementFactory interface
  NS_IMETHOD CreateInstanceByTag(nsINodeInfo *aNodeInfo, nsIContent** aResult);

  static PRUint32 gRefCnt;
  static PRInt32 kNameSpaceID_XUL;
  static nsINameSpaceManager* gNameSpaceManager;
};

PRUint32 XULElementFactoryImpl::gRefCnt = 0;
PRInt32 XULElementFactoryImpl::kNameSpaceID_XUL;
nsINameSpaceManager* XULElementFactoryImpl::gNameSpaceManager = nsnull;

XULElementFactoryImpl::XULElementFactoryImpl()
{
  NS_INIT_REFCNT();
  gRefCnt++;
  if (gRefCnt == 1) {
     nsresult rv = nsServiceManager::GetService(kNameSpaceManagerCID,
                                          NS_GET_IID(nsINameSpaceManager),
                                          (nsISupports**) &gNameSpaceManager);
     NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get namespace manager");
     if (NS_FAILED(rv)) return;

#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
static const char kXULNameSpaceURI[] = XUL_NAMESPACE_URI;
        gNameSpaceManager->RegisterNameSpace(NS_ConvertASCIItoUCS2(kXULNameSpaceURI), kNameSpaceID_XUL);
  }
}

XULElementFactoryImpl::~XULElementFactoryImpl()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_IF_RELEASE(gNameSpaceManager);
  }
}


NS_IMPL_ISUPPORTS(XULElementFactoryImpl, NS_GET_IID(nsIElementFactory));


nsresult
NS_NewXULElementFactory(nsIElementFactory** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  XULElementFactoryImpl* result = new XULElementFactoryImpl();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *aResult = result;
  return NS_OK;
}



NS_IMETHODIMP
XULElementFactoryImpl::CreateInstanceByTag(nsINodeInfo *aNodeInfo,
                                           nsIContent** aResult)
{
  return nsXULElement::Create(aNodeInfo, aResult); 
}



