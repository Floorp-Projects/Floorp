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
#ifdef XP_MAC
#include "nsIDocumentLoader.h"
#define NS_IMPL_IDS
#else
#define NS_IMPL_IDS
#include "nsIDocumentLoader.h"
#endif
#include "prmem.h"
#include "plstr.h"
#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"
#include "nsIPostToServer.h"
#include "nsIFactory.h"
#include "nsIContentViewerContainer.h"
#include "nsIRefreshUrl.h"
#include "nsITimer.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsVoidArray.h"
#include "nsIHttpURL.h"
#include "nsILoadAttribs.h"
#include "nsIURLGroup.h"
#include "nsIServiceManager.h"
#include "nsINetService.h"
#include "nsXPComFactory.h"
#include "nsIStreamLoadableDocument.h"
#include "nsIDocStreamLoaderFactory.h"

// XXX: Only needed for dummy factory...
#include "nsIDocument.h"
#include "nsIDocumentViewer.h"
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsLayoutCID.h"

#include "nsRDFCID.h"
#include "nsIXULParentDocument.h"
#include "nsIXULChildDocument.h"
#include "nsIRDFResource.h"
#include "nsIXULDocumentInfo.h"
#include "nsCOMPtr.h"

/* Forward declarations.... */
class nsDocLoaderImpl;

#ifdef DEBUG
#undef NOISY_CREATE_DOC
#else
#undef NOISY_CREATE_DOC
#endif

#if defined(DEBUG) || defined(FORCE_PR_LOG)
PRLogModuleInfo* gDocLoaderLog = nsnull;
#endif /* DEBUG || FORCE_PR_LOG */


  /* Private IIDs... */
/* eb001fa0-214f-11d2-bec0-00805f8a66dc */
#define NS_DOCUMENTBINDINFO_IID   \
{ 0xeb001fa0, 0x214f, 0x11d2, \
  {0xbe, 0xc0, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }


/* Define IIDs... */
NS_DEFINE_IID(kIStreamObserverIID,        NS_ISTREAMOBSERVER_IID);
NS_DEFINE_IID(kIDocumentLoaderIID,        NS_IDOCUMENTLOADER_IID);
NS_DEFINE_IID(kIDocumentLoaderFactoryIID, NS_IDOCUMENTLOADERFACTORY_IID);
NS_DEFINE_IID(kDocumentBindInfoIID,       NS_DOCUMENTBINDINFO_IID);
NS_DEFINE_IID(kIURLGroupIID,              NS_IURLGROUP_IID);
NS_DEFINE_IID(kRefreshURLIID,             NS_IREFRESHURL_IID);
NS_DEFINE_IID(kHTTPURLIID,                NS_IHTTPURL_IID);
NS_DEFINE_IID(kISupportsIID,              NS_ISUPPORTS_IID);
NS_DEFINE_IID(kIDocumentIID,              NS_IDOCUMENT_IID);
NS_DEFINE_IID(kIStreamListenerIID,        NS_ISTREAMLISTENER_IID);
NS_DEFINE_IID(kINetServiceIID,            NS_INETSERVICE_IID);

/* Define CIDs... */
NS_DEFINE_IID(kCHTMLDocumentCID,          NS_HTMLDOCUMENT_CID);
NS_DEFINE_IID(kCXMLDocumentCID,           NS_XMLDOCUMENT_CID);
NS_DEFINE_IID(kCRDFHTMLBuilderCID,        NS_RDFHTMLBUILDER_CID);
NS_DEFINE_IID(kCXULDocumentCID,           NS_XULDOCUMENT_CID);
NS_DEFINE_IID(kCImageDocumentCID,         NS_IMAGEDOCUMENT_CID);
NS_DEFINE_IID(kNetServiceCID,             NS_NETSERVICE_CID);


/* 
 * The nsDocumentBindInfo contains the state required when a single document
 * is being loaded...  Each instance remains alive until its target URL has 
 * been loaded (or aborted).
 *
 * The Document Loader maintains a list of nsDocumentBindInfo instances which 
 * represents the set of documents actively being loaded...
 */
class nsDocumentBindInfo : public nsIStreamListener, 
                           public nsIRefreshUrl
{
public:
    nsDocumentBindInfo();

    nsresult Init(nsDocLoaderImpl* aDocLoader,
                  const char *aCommand, 
                  nsIContentViewerContainer* aContainer,
                  nsISupports* aExtraInfo,
                  nsIStreamObserver* anObserver);

    NS_DECL_ISUPPORTS

    nsresult Bind(const nsString& aURLSpec, 
                  nsIPostData* aPostData,
                  nsIStreamListener* aListener);

    nsresult Bind(nsIURL* aURL, nsIStreamListener* aListener);

    nsresult Stop(void);

    /* nsIStreamListener interface methods... */
    NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo);
    NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
    NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
    NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
    NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aStream, PRUint32 aLength);
    NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);

    nsresult GetStatus(void) { return mStatus; }

    /* nsIRefreshURL interface methods... */
    NS_IMETHOD RefreshURL(nsIURL* aURL, PRInt32 millis, PRBool repeat);
    NS_IMETHOD CancelRefreshURLTimers(void);

protected:
    virtual ~nsDocumentBindInfo();

protected:
    char*               m_Command;
    nsIURL*             m_Url;
    nsIContentViewerContainer* m_Container;
    nsISupports*        m_ExtraInfo;
    nsIStreamObserver*  m_Observer;
    nsIStreamListener*  m_NextStream;
    nsDocLoaderImpl*    m_DocLoader;

    nsresult            mStatus;
};




/****************************************************************************
 * nsDocFactoryImpl implementation...
 ****************************************************************************/

class nsDocFactoryImpl : public nsIDocumentLoaderFactory,
												 public nsIDocStreamLoaderFactory
{
public:
    nsDocFactoryImpl();

    NS_DECL_ISUPPORTS

			// for nsIDocumentLoaderFactory
    NS_IMETHOD CreateInstance(nsIURL* aURL,
                              const char* aContentType, 
                              const char* aCommand,
                              nsIContentViewerContainer* aContainer,
                              nsISupports* aExtraInfo,
                              nsIStreamListener** aDocListener,
                              nsIContentViewer** aDocViewer);

			// for nsIDocStreamLoaderFactory
		NS_METHOD CreateInstance( nsIInputStream& aInputStream,
															const char* aContentType,
															const char* aCommand,
															nsIContentViewerContainer* aContainer,
															nsISupports* aExtraInfo,
															nsIContentViewer** aDocViewer );

    nsresult InitUAStyleSheet();

    nsresult CreateDefaultDocument(nsIURL* aURL, 
                                   const char* aCommand,
                                   nsIContentViewerContainer* aContainer,
                                   nsIStreamListener** aDocListener,
                                   nsIContentViewer** aDocViewer);

    nsresult CreateXMLDocument(nsIURL* aURL, 
                               const char* aCommand,
                               nsIContentViewerContainer* aContainer,
                               nsIStreamListener** aDocListener,
                               nsIContentViewer** aDocViewer);

    nsresult CreateRDFDocument(const char* aContentType, nsIURL* aURL, 
                               const char* aCommand,
                               nsIContentViewerContainer* aContainer,
                               nsISupports* aExtraInfo,
                               nsIStreamListener** aDocListener,
                               nsIContentViewer** aDocViewer);

		nsresult CreateXULDocumentFromStream( nsIInputStream& aXULStream,
																					const char* aCommand,
																					nsIContentViewerContainer* aContainer,
																					nsISupports* aExtraInfo,
																					nsIContentViewer** aDocViewer );


    nsresult CreateImageDocument(nsIURL* aURL, 
                                 const char* aCommand,
                                 nsIContentViewerContainer* aContainer,
                                 nsIStreamListener** aDocListener,
                                 nsIContentViewer** aDocViewer);

    nsresult CreatePluginDocument(nsIURL* aURL, 
                                  const char* aCommand,
                                  const char* aContentType,
                                  nsIContentViewerContainer* aContainer,
                                  nsIStreamListener** aDocListener,
                                  nsIContentViewer** aDocViewer);

	protected:

		nsresult do_CreateRDFDocument( nsISupports*, nsCOMPtr<nsIDocument>*, nsCOMPtr<nsIDocumentViewer>* );
};

static nsICSSStyleSheet* gUAStyleSheet;

nsDocFactoryImpl::nsDocFactoryImpl()
{
    NS_INIT_REFCNT();
}

/*
 * Implementation of ISupports methods...
 */
NS_IMPL_ADDREF(nsDocFactoryImpl)
NS_IMPL_RELEASE(nsDocFactoryImpl)

NS_IMETHODIMP
nsDocFactoryImpl::QueryInterface( REFNSIID aIID, void** aInstancePtr )
	{
		nsISupports* temp;
		nsISupports** result = aInstancePtr ? NS_REINTERPRET_CAST(nsISupports**, aInstancePtr) : &temp;

		if ( aIID.Equals(nsIDocumentLoaderFactory::GetIID()) )
			*result = NS_STATIC_CAST(nsIDocumentLoaderFactory*, this);
	  else if ( aIID.Equals(nsIDocStreamLoaderFactory::GetIID()) )
			*result = NS_STATIC_CAST(nsIDocStreamLoaderFactory*, this);
		else if ( aIID.Equals(nsISupports::GetIID()) )
			*result = NS_STATIC_CAST(nsISupports*, NS_STATIC_CAST(nsIDocumentLoaderFactory*, this));
		else
			*result = 0;

		nsresult status;
		if ( !aInstancePtr )
			status = NS_ERROR_NULL_POINTER;
		else if ( !*aInstancePtr )
			status = NS_NOINTERFACE;
		else
			{
				NS_ADDREF(*result);
				status = NS_OK;
			}

		return status;
	}

static char* gValidTypes[] = {"text/html","application/rtf","text/plain",0};
static char* gXMLTypes[] = {"text/xml", "application/xml", 0};
static char* gRDFTypes[] = {"text/rdf", "text/xul", 0};

static char* gImageTypes[] = {"image/gif", "image/jpeg", "image/png", 0 };

static char* gPluginTypes[] = {
    "video/quicktime",
    "video/msvideo",
    "video/x-msvideo",
    "application/vnd.netfpx",
    "image/vnd.fpx",
    "model/vrml",
    "x-world/x-vrml",
    "audio/midi",
    "audio/x-midi",
    "audio/wav",
    "audio/x-wav", 
    "audio/aiff",
    "audio/x-aiff",
    "audio/basic",
    "application/x-shockwave-flash",
	"application/pdf",
	"application/npapi-test",
    0
};

NS_IMETHODIMP
nsDocFactoryImpl::CreateInstance(nsIURL* aURL, 
                                 const char* aContentType, 
                                 const char *aCommand,
                                 nsIContentViewerContainer* aContainer,
                                 nsISupports* aExtraInfo,
                                 nsIStreamListener** aDocListener,
                                 nsIContentViewer** aDocViewer)
{
    nsresult rv = NS_ERROR_FAILURE;

    if(0==PL_strcmp(gXMLTypes[0],aContentType))
      if(0==PL_strcmp(aCommand,"view-source"))
        aContentType=gValidTypes[0];

    int typeIndex=0;
    while(gValidTypes[typeIndex]) {
      if (0== PL_strcmp(gValidTypes[typeIndex++], aContentType)) {
          return CreateDefaultDocument(aURL, aCommand,
                                       aContainer,
                                       aDocListener,
                                       aDocViewer);
      }
    }

    // Try XML
    typeIndex = 0;
    while(gXMLTypes[typeIndex]) {
      if (0== PL_strcmp(gXMLTypes[typeIndex++], aContentType)) {
          return CreateXMLDocument(aURL, aCommand,
                                   aContainer,
                                   aDocListener,
                                   aDocViewer);
      }
    }

    // Try RDF
    typeIndex = 0;
    while (gRDFTypes[typeIndex]) {
        if (0 == PL_strcmp(gRDFTypes[typeIndex++], aContentType)) {
            return CreateRDFDocument(aContentType, aURL, aCommand,
                                     aContainer,
                                     aExtraInfo,
                                     aDocListener,
                                     aDocViewer);
        }
    }

    // Try image types
    typeIndex = 0;
    while(gImageTypes[typeIndex]) {
      if (0== PL_strcmp(gImageTypes[typeIndex++], aContentType)) {
          return CreateImageDocument(aURL, aCommand,
                                     aContainer,
                                     aDocListener,
                                     aDocViewer);
      }
    }

    // Try plugin types
    typeIndex = 0;
    while(gPluginTypes[typeIndex]) {
      if (0== PL_strcmp(gPluginTypes[typeIndex++], aContentType)) {
          return CreatePluginDocument(aURL, aCommand, aContentType,
                                      aContainer,
                                      aDocListener,
                                      aDocViewer);
      }
    }

    return rv;
}

NS_IMETHODIMP
nsDocFactoryImpl::CreateInstance( nsIInputStream& aInputStream,
																	const char* aContentType,
																	const char* aCommand,
																	nsIContentViewerContainer* aContainer,
																	nsISupports* aExtraInfo,
																	nsIContentViewer** aDocViewer )
	{
		nsresult status = NS_ERROR_FAILURE;

    	// Try RDF
    int typeIndex = 0;
    while (gRDFTypes[typeIndex]) {
        if (0 == PL_strcmp(gRDFTypes[typeIndex++], aContentType)) {
            return CreateXULDocumentFromStream(aInputStream,
            																	 aCommand,
					                                     aContainer,
					                                     aExtraInfo,
					                                     aDocViewer);
        }
    }

		return status;
	}


nsresult
nsDocFactoryImpl::CreateDefaultDocument(nsIURL* aURL, 
                                        const char* aCommand,
                                        nsIContentViewerContainer* aContainer,
                                        nsIStreamListener** aDocListener,
                                        nsIContentViewer** aDocViewer)
{
    nsresult rv = NS_ERROR_FAILURE;
    nsIDocument* doc = nsnull;
    nsIDocumentViewer* docv = nsnull;

#ifdef NOISY_CREATE_DOC
    if (nsnull != aURL) {
        nsAutoString tmp;
        aURL->ToString(tmp);
        fputs(tmp, stdout);
        printf(": creating document\n");
    }
#endif

    // Load the UA style sheet if we haven't already done that
    if (nsnull == gUAStyleSheet) {
        InitUAStyleSheet();
    }

    /*
     * Create the HTML document...
     */
    rv = nsComponentManager::CreateInstance(kCHTMLDocumentCID,
                                      nsnull,
                                      kIDocumentIID,
                                      (void **)&doc);
    if (NS_FAILED(rv)) {
        goto done;
    }

    /*
     * Create the HTML Content Viewer...
     */
    rv = NS_NewDocumentViewer(docv);
    if (NS_FAILED(rv)) {
        goto done;
    }
    docv->SetUAStyleSheet(gUAStyleSheet);

    /* 
     * Initialize the document to begin loading the data...
     *
     * An nsIStreamListener connected to the parser is returned in
     * aDocListener.
     */
    rv = doc->StartDocumentLoad(aURL, aContainer, aDocListener, aCommand);
    if (NS_FAILED(rv)) {
        NS_IF_RELEASE(docv);
        goto done;
    }

    /*
     * Bind the document to the Content Viewer...
     */
    rv = docv->BindToDocument(doc, aCommand);
    *aDocViewer = docv;

done:
    NS_IF_RELEASE(doc);
    return rv;
}

nsresult
nsDocFactoryImpl::CreateXMLDocument(nsIURL* aURL, 
                                    const char* aCommand,
                                    nsIContentViewerContainer* aContainer,
                                    nsIStreamListener** aDocListener,
                                    nsIContentViewer** aDocViewer)
{
    nsresult rv = NS_ERROR_FAILURE;
    nsIDocument* doc = nsnull;
    nsIDocumentViewer* docv = nsnull;

    // Load the UA style sheet if we haven't already done that
    if (nsnull == gUAStyleSheet) {
        InitUAStyleSheet();
    }

    /*
     * Create the image document...
     */
    rv = nsComponentManager::CreateInstance(kCXMLDocumentCID,
                                      nsnull,
                                      kIDocumentIID,
                                      (void **)&doc);
    if (NS_FAILED(rv)) {
        goto done;
    }

    /*
     * Create the image content viewer...
     */
    rv = NS_NewDocumentViewer(docv);
    if (NS_FAILED(rv)) {
        goto done;
    }
    docv->SetUAStyleSheet(gUAStyleSheet);

    /* 
     * Initialize the document to begin loading the data...
     *
     * An nsIStreamListener connected to the parser is returned in
     * aDocListener.
     */
    rv = doc->StartDocumentLoad(aURL, aContainer, aDocListener, aCommand);
    if (NS_FAILED(rv)) {
        NS_IF_RELEASE(docv);
        goto done;
    }

    /*
     * Bind the document to the Content Viewer...
     */
    rv = docv->BindToDocument(doc, aCommand);
    *aDocViewer = docv;

done:
    NS_IF_RELEASE(doc);
    return rv;
}



nsresult
nsDocFactoryImpl::do_CreateRDFDocument( nsISupports* aExtraInfo,
															nsCOMPtr<nsIDocument>* doc,
												nsCOMPtr<nsIDocumentViewer>* docv )
		// ...common work for |CreateRDFDocument| and |CreateXULDocumentFromStream|
	{
    nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsIXULDocumentInfo> xulDocumentInfo;
    xulDocumentInfo = do_QueryInterface(aExtraInfo);
    
    // Load the UA style sheet if we haven't already done that
    if (nsnull == gUAStyleSheet) {
        InitUAStyleSheet();
    }

    /*
     * Create the image document...
     */
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kCXULDocumentCID,
                                                    nsnull,
                                                    kIDocumentIID,
                                                    getter_AddRefs(*doc))))
        goto done;

    /*
     * Create the image content viewer...
     */
    if (NS_FAILED(rv = NS_NewDocumentViewer( *getter_AddRefs(*docv) )))
        goto done;

    (*docv)->SetUAStyleSheet(gUAStyleSheet);

    // We are capable of being a XUL child document. If 
    // we have extra info that supports the XUL document info
    // interface, then we'll know for sure.
    if (xulDocumentInfo) {
        // We are a XUL fragment. Retrieve the parent document
        // and the fragment's root position within the parent
        // document.
        nsCOMPtr<nsIDocument> parentDocument;
        nsCOMPtr<nsIRDFResource> fragmentRoot;
        if (NS_SUCCEEDED(xulDocumentInfo->GetDocument(getter_AddRefs(parentDocument))) &&
            NS_SUCCEEDED(xulDocumentInfo->GetResource(getter_AddRefs(fragmentRoot)))) {
            // We were able to obtain a resource and a parent document.
            parentDocument->AddSubDocument(*doc);
            (*doc)->SetParentDocument(parentDocument);

            // We need to set our fragment root as well.  The
            // XUL child document interface is required to do this.
            nsCOMPtr<nsIXULChildDocument> xulChildDoc;
            xulChildDoc = do_QueryInterface(*doc);
            if (xulChildDoc) {
                xulChildDoc->SetFragmentRoot(fragmentRoot);
            }
        }
    }

done:
   return rv;
	}


nsresult
nsDocFactoryImpl::CreateRDFDocument(const char* aContentType, nsIURL* aURL, 
                                    const char* aCommand,
                                    nsIContentViewerContainer* aContainer,
                                    nsISupports* aExtraInfo,
                                    nsIStreamListener** aDocListener,
                                    nsIContentViewer** aDocViewer)
	// ...note, this RDF document _may_ be XUL :-)
{
	nsCOMPtr<nsIDocument> doc;
	nsCOMPtr<nsIDocumentViewer> docv;
	nsresult rv = do_CreateRDFDocument(aExtraInfo, &doc, &docv);

    /* 
     * Initialize the document to begin loading the data...
     *
     * An nsIStreamListener connected to the parser is returned in
     * aDocListener.
     */
    if (NS_SUCCEEDED(rv = doc->StartDocumentLoad(aURL, aContainer, aDocListener, aCommand))) {
	    /*
	     * Bind the document to the Content Viewer...
	     */
	    rv = docv->BindToDocument(doc, aCommand);
	    *aDocViewer = docv;
	    NS_IF_ADDREF(*aDocViewer);
    }
   
   return rv;
}

nsresult
nsDocFactoryImpl::CreateXULDocumentFromStream( nsIInputStream& aXULStream,
																							 const char* aCommand,
																							 nsIContentViewerContainer* aContainer,
																							 nsISupports* aExtraInfo,
																							 nsIContentViewer** aDocViewer )
	{
		nsresult status = NS_OK;

		do
			{
				nsCOMPtr<nsIDocument> doc;
				nsCOMPtr<nsIDocumentViewer> docv;
				if ( NS_FAILED(status = do_CreateRDFDocument(aExtraInfo, &doc, &docv)) )
					break;

				if ( NS_FAILED(status = docv->BindToDocument(doc, aCommand)) )
					break;

				*aDocViewer = docv;
				NS_IF_ADDREF(*aDocViewer);

				nsCOMPtr<nsIStreamLoadableDocument> loader = do_QueryInterface(doc, &status);
				if ( NS_FAILED(status) )
					break;

				status = loader->LoadFromStream(aXULStream, aContainer, aCommand);
			}
		while ( false );

		return status;
	}

nsresult
nsDocFactoryImpl::CreateImageDocument(nsIURL* aURL, 
                                      const char* aCommand,
                                      nsIContentViewerContainer* aContainer,
                                      nsIStreamListener** aDocListener,
                                      nsIContentViewer** aDocViewer)
{
    nsresult rv = NS_ERROR_FAILURE;
    nsIDocument* doc = nsnull;
    nsIDocumentViewer* docv = nsnull;

    // Load the UA style sheet if we haven't already done that
    if (nsnull == gUAStyleSheet) {
        InitUAStyleSheet();
    }

    /*
     * Create the image document...
     */
    rv = nsComponentManager::CreateInstance(kCImageDocumentCID,
                                      nsnull,
                                      kIDocumentIID,
                                      (void **)&doc);
    if (NS_FAILED(rv)) {
        goto done;
    }

    /*
     * Create the image content viewer...
     */
    rv = NS_NewDocumentViewer(docv);
    if (NS_FAILED(rv)) {
        goto done;
    }
    docv->SetUAStyleSheet(gUAStyleSheet);

    /* 
     * Initialize the document to begin loading the data...
     *
     * An nsIStreamListener connected to the parser is returned in
     * aDocListener.
     */
    rv = doc->StartDocumentLoad(aURL, aContainer, aDocListener,aCommand);
    if (NS_FAILED(rv)) {
        NS_IF_RELEASE(docv);
        goto done;
    }

    /*
     * Bind the document to the Content Viewer...
     */
    rv = docv->BindToDocument(doc, aCommand);
    *aDocViewer = docv;

done:
    NS_IF_RELEASE(doc);
    return rv;
}

extern nsresult
NS_NewPluginContentViewer(const char* aCommand,
                          nsIStreamListener** aDocListener,
                          nsIContentViewer** aDocViewer);

nsresult
nsDocFactoryImpl::CreatePluginDocument(nsIURL* aURL, 
                                       const char* aCommand,
                                       const char* aContentType,
                                       nsIContentViewerContainer* aContainer,
                                       nsIStreamListener** aDocListener,
                                       nsIContentViewer** aDocViewer)
{
    /*
     * Create the plugin content viewer and stream listener...
     */
    nsresult rv = NS_NewPluginContentViewer(aCommand,
                                            aDocListener,
                                            aDocViewer);
    return rv;
}

#define UA_CSS_URL "resource:/res/ua.css"

nsresult nsDocFactoryImpl::InitUAStyleSheet()
{
  nsresult rv = NS_OK;

  if (nsnull == gUAStyleSheet) {  // snarf one
    nsIURL* uaURL;
    rv = NS_NewURL(&uaURL, nsString(UA_CSS_URL)); // XXX this bites, fix it
    if (NS_SUCCEEDED(rv)) {
      // Get an input stream from the url
      nsIInputStream* in;
      rv = NS_OpenURL(uaURL, &in);
      if (NS_SUCCEEDED(rv)) {
        // Translate the input using the argument character set id into unicode
        nsIUnicharInputStream* uin;
        rv = NS_NewConverterStream(&uin, nsnull, in);
        if (NS_SUCCEEDED(rv)) {
          // Create parser and set it up to process the input file
          nsICSSParser* css;
          rv = NS_NewCSSParser(&css);
          if (NS_SUCCEEDED(rv)) {
            // Parse the input and produce a style set
            // XXX note: we are ignoring rv until the error code stuff in the
            // input routines is converted to use nsresult's
            css->Parse(uin, uaURL, gUAStyleSheet);
            NS_RELEASE(css);
          }
          NS_RELEASE(uin);
        }
        NS_RELEASE(in);
      }
      else {
        printf("open of %s failed: error=%x\n", UA_CSS_URL, rv);
        rv = NS_ERROR_ILLEGAL_VALUE;  // XXX need a better error code here
      }

      NS_RELEASE(uaURL);
    }
  }
  return rv;
}


/****************************************************************************
 * nsDocLoaderImpl implementation...
 ****************************************************************************/

class nsDocLoaderImpl : public nsIDocumentLoader,
                        public nsIURLGroup
{
public:

    nsDocLoaderImpl();

    NS_DECL_ISUPPORTS

    // nsIDocumentLoader interface
    NS_IMETHOD LoadDocument(const nsString& aURLSpec, 
                            const char *aCommand,
                            nsIContentViewerContainer* aContainer,
                            nsIPostData* aPostData = nsnull,
                            nsISupports* aExtraInfo = nsnull,
                            nsIStreamObserver* anObserver = nsnull,
                            nsURLReloadType aType = nsURLReload,
                            const PRUint32 aLocalIP = 0);

    NS_IMETHOD LoadSubDocument(const nsString& aURLSpec,
                               nsISupports* aExtraInfo = nsnull,
                               nsURLReloadType type = nsURLReload,
                               const PRUint32 aLocalIP = 0);

    NS_IMETHOD Stop(void);

    NS_IMETHOD IsBusy(PRBool& aResult);

    NS_IMETHOD CreateDocumentLoader(nsIDocumentLoader** anInstance);
    NS_IMETHOD SetDocumentFactory(nsIDocumentLoaderFactory* aFactory);
    NS_IMETHOD GetDocumentFactory(nsIDocumentLoaderFactory** aResult);

    NS_IMETHOD AddObserver(nsIDocumentLoaderObserver *aObserver);
    NS_IMETHOD RemoveObserver(nsIDocumentLoaderObserver *aObserver);

    NS_IMETHOD SetContainer(nsIContentViewerContainer* aContainer);
    NS_IMETHOD GetContainer(nsIContentViewerContainer** aResult);

    // nsIURLGroup interface...
    NS_IMETHOD CreateURL(nsIURL** aInstancePtrResult, 
                         nsIURL* aBaseURL,
                         const nsString& aSpec,
                         nsISupports* aContainer);

    NS_IMETHOD OpenStream(nsIURL *aUrl, 
                          nsIStreamListener *aConsumer);

    NS_IMETHOD GetDefaultLoadAttributes(nsILoadAttribs*& aLoadAttribs);
    NS_IMETHOD SetDefaultLoadAttributes(nsILoadAttribs*  aLoadAttribs);

    NS_IMETHOD AddChildGroup(nsIURLGroup* aGroup);
    NS_IMETHOD RemoveChildGroup(nsIURLGroup* aGroup);

    // Implementation specific methods...
    void FireOnStartDocumentLoad(nsIURL* aURL, const char* aCOmmand);
    void FireOnEndDocumentLoad(PRInt32 aStatus);

    void FireOnStartURLLoad(nsIURL* aURL, const char* aContentType, 
                            nsIContentViewer* aViewer);

    void FireOnProgressURLLoad(nsIURL* aURL, PRUint32 aProgress, 
                               PRUint32 aProgressMax);

    void FireOnStatusURLLoad(nsIURL* aURL, nsString& aMsg);

    void FireOnEndURLLoad(nsIURL* aURL, PRInt32 aStatus);
    //    void AreAllConnectionsComplete(void);
    void LoadURLComplete(nsIURL* aURL, nsISupports* aLoader, PRInt32 aStatus);
    void SetParent(nsDocLoaderImpl* aParent);
    void SetDocumentUrl(nsIURL* aUrl);
protected:
    virtual ~nsDocLoaderImpl();

private:
    static PRBool StopBindInfoEnumerator (nsISupports* aElement, void* aData);
    static PRBool StopDocLoaderEnumerator(void* aElement, void* aData);
    static PRBool IsBusyEnumerator(void* aElement, void* aData);

public:
    nsIDocumentLoaderFactory* m_DocFactory;

protected:
    nsIURL*                    mDocumentUrl;
    nsISupportsArray*          m_LoadingDocsList;

    nsVoidArray                mChildGroupList;
    nsVoidArray                mDocObservers;
    nsILoadAttribs*            m_LoadAttrib;
    nsIStreamObserver*         mStreamObserver;
    nsIContentViewerContainer* mContainer;

    nsDocLoaderImpl*           mParent;
    /*
     * The following counts are for the current document loader only.  They
     * do not take into account URLs being loaded by child document loaders.
     */
    PRInt32 mForegroundURLs;
    PRInt32 mTotalURLs;
    /*
     * This flag indicates that the loader is loading a document.  It is set
     * from the call to LoadDocument(...) until the OnConnectionsComplete(...)
     * notification is fired...
     */
    PRBool mIsLoadingDocument;
};


nsDocLoaderImpl::nsDocLoaderImpl()
{
    NS_INIT_REFCNT();

#if defined(DEBUG) || defined(FORCE_PR_LOG)
    if (nsnull == gDocLoaderLog) {
      gDocLoaderLog = PR_NewLogModule("DocLoader");
    }
#endif /* DEBUG || FORCE_PR_LOG */

    mDocumentUrl    = nsnull;
    mParent         = nsnull;
    mStreamObserver = nsnull;
    mContainer      = nsnull;
    mForegroundURLs = 0;
    mTotalURLs      = 0;

    mIsLoadingDocument = PR_FALSE;

    NS_NewISupportsArray(&m_LoadingDocsList);
    NS_NewLoadAttribs(&m_LoadAttrib);

    m_DocFactory = new nsDocFactoryImpl();
    NS_ADDREF(m_DocFactory);


    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader - DocLoader [%p] created.\n", this));
}


nsDocLoaderImpl::~nsDocLoaderImpl()
{
  Stop();
  if (nsnull != mParent) {
    mParent->RemoveChildGroup(this);
    NS_RELEASE(mParent);
  }

  NS_IF_RELEASE(m_LoadingDocsList);
  NS_IF_RELEASE(m_DocFactory);
  NS_IF_RELEASE(m_LoadAttrib);
  NS_IF_RELEASE(mStreamObserver);
  NS_IF_RELEASE(mContainer);
  NS_IF_RELEASE(mDocumentUrl);

  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader - DocLoader [%p] deleted.\n", this));

  NS_PRECONDITION((0 == mChildGroupList.Count()), "Document loader has children...");
}


/*
 * Implementation of ISupports methods...
 */
NS_IMPL_ADDREF(nsDocLoaderImpl);
NS_IMPL_RELEASE(nsDocLoaderImpl);

NS_IMETHODIMP
nsDocLoaderImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDocumentLoaderIID)) {
    *aInstancePtr = (void*)(nsIDocumentLoader*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIURLGroupIID)) {
    *aInstancePtr = (void*)(nsIURLGroup*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}



NS_IMETHODIMP
nsDocLoaderImpl::CreateDocumentLoader(nsIDocumentLoader** anInstance)
{
  nsDocLoaderImpl* newLoader = nsnull;
  nsresult rv = NS_OK;

  /* Check for initial error conditions... */
  if (nsnull == anInstance) {
    rv = NS_ERROR_NULL_POINTER;
     goto done;
  }

  NS_NEWXPCOM(newLoader, nsDocLoaderImpl);
  if (nsnull == newLoader) {
    *anInstance = nsnull;
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }
  rv = newLoader->QueryInterface(kIDocumentLoaderIID, (void**)anInstance);
  if (NS_SUCCEEDED(rv)) {
    AddChildGroup(newLoader);
    newLoader->SetParent(this);
  }

done:
  return rv;
}



NS_IMETHODIMP
nsDocLoaderImpl::SetDocumentFactory(nsIDocumentLoaderFactory* aFactory)
{
  NS_IF_RELEASE(m_DocFactory);
  m_DocFactory = aFactory;
  NS_IF_ADDREF(m_DocFactory);

  return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::GetDocumentFactory(nsIDocumentLoaderFactory** aResult)
{
	NS_IF_ADDREF(m_DocFactory);
	*aResult = m_DocFactory;
	return NS_OK;
}


NS_IMETHODIMP
nsDocLoaderImpl::LoadDocument(const nsString& aURLSpec, 
                              const char* aCommand,
                              nsIContentViewerContainer* aContainer,
                              nsIPostData* aPostData,
                              nsISupports* aExtraInfo,
                              nsIStreamObserver* anObserver,
                              nsURLReloadType aType,
                              const PRUint32 aLocalIP)
{
  nsresult rv;
  nsURLLoadType loadType;
  nsDocumentBindInfo* loader = nsnull;

#if defined(DEBUG)
  char buffer[256];

  aURLSpec.ToCString(buffer, sizeof(buffer));
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader - LoadDocument(...) called for %s.", 
          buffer));
#endif /* DEBUG */

  /* Check for initial error conditions... */
  if (nsnull == aContainer) {
      rv = NS_ERROR_NULL_POINTER;
      goto done;
  }

  NS_NEWXPCOM(loader, nsDocumentBindInfo);
  if (nsnull == loader) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
  }
  loader->Init(this,           // DocLoader
               aCommand,       // Command
               aContainer,     // Viewer Container
               aExtraInfo,     // Extra Info
               anObserver);    // Observer

  /* The DocumentBindInfo reference is only held by the Array... */
  m_LoadingDocsList->AppendElement((nsIStreamListener *)loader);

  /* Initialize the URL counters... */
  NS_PRECONDITION(((mTotalURLs == 0) && (mForegroundURLs == 0)), "DocuemntLoader is busy...");
  rv = m_LoadAttrib->GetLoadType(&loadType);
  if (NS_FAILED(rv)) {
    loadType = nsURLLoadNormal;
  }
  if (nsURLLoadBackground != loadType) {
    mForegroundURLs = 1;
  }
  mTotalURLs = 1;
  /*
   * Set the flag indicating that the document loader is in the process of
   * loading a document.  This flag will remain set until the 
   * OnConnectionsComplete(...) notification is fired for the loader...
   */
  mIsLoadingDocument = PR_TRUE;

  m_LoadAttrib->SetReloadType(aType);
  // If we've got special loading instructions, mind them.
  if ((aType == nsURLReloadBypassProxy) || 
      (aType == nsURLReloadBypassCacheAndProxy)) {
      m_LoadAttrib->SetBypassProxy(PR_TRUE);
  }
  if ( aLocalIP ) {
      m_LoadAttrib->SetLocalIP(aLocalIP);
  }

  NS_IF_RELEASE(mStreamObserver);
  mStreamObserver = anObserver;
  NS_IF_ADDREF(mStreamObserver);

  rv = loader->Bind(aURLSpec, aPostData, nsnull);

done:
  return rv;
}

NS_IMETHODIMP
nsDocLoaderImpl::LoadSubDocument(const nsString& aURLSpec,
                                 nsISupports* aExtraInfo,
                                 nsURLReloadType type,
                                 const PRUint32 aLocalIP)
{
  nsresult rv;
  nsDocumentBindInfo* loader = nsnull;

#ifdef DEBUG
  char buffer[256];

  aURLSpec.ToCString(buffer, sizeof(buffer));
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader - LoadSubDocument(...) called for %s.", 
          buffer));
#endif /* DEBUG */

  NS_NEWXPCOM(loader, nsDocumentBindInfo);
  if (nsnull == loader) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      return rv;
  }
  loader->Init(this,           // DocLoader
               nsnull,         // Command
               nsnull,     // Viewer Container
               aExtraInfo,     // Extra Info
               mStreamObserver);    // Observer

  /* The DocumentBindInfo reference is only held by the Array... */
  m_LoadingDocsList->AppendElement((nsIStreamListener *)loader);

  /* Increment the URL counters... */
  mForegroundURLs++;
  mTotalURLs++;
  
  rv = loader->Bind(aURLSpec, nsnull, nsnull);

  return rv;
}

NS_IMETHODIMP
nsDocLoaderImpl::Stop(void)
{
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader - Stop() called\n"));

  m_LoadingDocsList->EnumerateForwards(nsDocLoaderImpl::StopBindInfoEnumerator, nsnull);

  /* 
   * Now the only reference to each nsDocumentBindInfo instance is held by 
   * Netlib via the nsIStreamListener interface...
   * 
   * When each connection is aborted, Netlib will release its reference to 
   * the StreamListener and the DocumentBindInfo object will be deleted...
   */
  m_LoadingDocsList->Clear();

  /*
   * Now Stop() all documents being loaded by child DocumentLoaders...
   */
  mChildGroupList.EnumerateForwards(nsDocLoaderImpl::StopDocLoaderEnumerator, nsnull);

  /* Reset the URL counters... */
  mForegroundURLs = 0;
  mTotalURLs      = 0;

  /* 
   * Release the Stream Observer...  
   * It will be set on the next LoadDocument(...) 
   */
  NS_IF_RELEASE(mStreamObserver);

  return NS_OK;
}       


NS_IMETHODIMP
nsDocLoaderImpl::IsBusy(PRBool& aResult)
{
  aResult = PR_FALSE;

  /* If this document loader is busy? */
  if (0 != mForegroundURLs) {
    aResult = PR_TRUE;
  } 
  /* Otherwise, check its child document loaders... */
  else {
    mChildGroupList.EnumerateForwards(nsDocLoaderImpl::IsBusyEnumerator, 
                                      (void*)&aResult);
  }

  return NS_OK;
}


/*
 * Do not hold refs to the objects in the observer lists.  Observers
 * are expected to remove themselves upon their destruction if they
 * have not removed themselves previously
 */
NS_IMETHODIMP
nsDocLoaderImpl::AddObserver(nsIDocumentLoaderObserver* aObserver)
{
  // Make sure the observer isn't already in the list
  if (mDocObservers.IndexOf(aObserver) == -1) {
    mDocObservers.AppendElement(aObserver);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::RemoveObserver(nsIDocumentLoaderObserver* aObserver)
{
  if (PR_TRUE == mDocObservers.RemoveElement(aObserver)) {
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocLoaderImpl::SetContainer(nsIContentViewerContainer* aContainer)
{
  NS_IF_RELEASE(mContainer);
  mContainer = aContainer;
  NS_IF_ADDREF(mContainer);

  return NS_OK;
}

NS_IMETHODIMP
nsDocLoaderImpl::GetContainer(nsIContentViewerContainer** aResult)
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    *aResult = mContainer;
    NS_IF_ADDREF(mContainer);
  }
  return rv;
}

NS_IMETHODIMP
nsDocLoaderImpl::CreateURL(nsIURL** aInstancePtrResult, 
                           nsIURL* aBaseURL,
                           const nsString& aURLSpec,
                           nsISupports* aContainer)
{
  nsresult rv;
  nsIURL* url = nsnull;

    /* Check for initial error conditions... */
  if (nsnull == aInstancePtrResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    rv = NS_NewURL(&url, aURLSpec, aBaseURL, aContainer, this);
    if (NS_SUCCEEDED(rv)) {
      nsILoadAttribs* loadAttributes;

      rv = url->GetLoadAttribs(&loadAttributes);
      if (NS_SUCCEEDED(rv)) {
        loadAttributes->Clone(m_LoadAttrib);
        NS_RELEASE(loadAttributes);
      }
    }
    *aInstancePtrResult = url;
  }

  return rv;
}


NS_IMETHODIMP
nsDocLoaderImpl::OpenStream(nsIURL *aUrl, nsIStreamListener *aConsumer)
{
  nsresult rv;
  nsDocumentBindInfo* loader = nsnull;
  nsURLLoadType loadType = nsURLLoadNormal;

#if defined(DEBUG)
  const char* buffer;

  aUrl->GetSpec(&buffer);
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader - OpenStream(...) called for %s.", 
          buffer));
#endif /* DEBUG */


  NS_NEWXPCOM(loader, nsDocumentBindInfo);
  if (nsnull == loader) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }
  loader->Init(this,              // DocLoader
               nsnull,            // Command
               mContainer,        // Viewer Container
               nsnull,            // Extra Info
               mStreamObserver);  // Observer

  /* The DocumentBindInfo reference is only held by the Array... */
  m_LoadingDocsList->AppendElement(((nsISupports*)(nsIStreamObserver*)loader));

  /* Update the URL counters... */
  nsILoadAttribs* loadAttributes;

  rv = aUrl->GetLoadAttribs(&loadAttributes);
  if (NS_SUCCEEDED(rv)) {
    rv = loadAttributes->GetLoadType(&loadType);
    if (NS_FAILED(rv)) {
      loadType = nsURLLoadNormal;
    }
    NS_RELEASE(loadAttributes);
  }
  if (nsURLLoadBackground != loadType) {
    mForegroundURLs += 1;
  }
  mTotalURLs += 1;

  rv = loader->Bind(aUrl, aConsumer);
done:
  return rv;
}


NS_IMETHODIMP
nsDocLoaderImpl::GetDefaultLoadAttributes(nsILoadAttribs*& aLoadAttribs)
{
  aLoadAttribs = m_LoadAttrib;
  NS_IF_ADDREF(m_LoadAttrib);

  return NS_OK;;
}


NS_IMETHODIMP
nsDocLoaderImpl::SetDefaultLoadAttributes(nsILoadAttribs*  aLoadAttribs)
{
  m_LoadAttrib->Clone(aLoadAttribs);

  /*
   * Now set the default attributes for all child DocumentLoaders...
   */
  PRInt32 count = mChildGroupList.Count();
  PRInt32 index;

  for (index = 0; index < count; index++) {
    nsIURLGroup* child = (nsIURLGroup*)mChildGroupList.ElementAt(index);
    child->SetDefaultLoadAttributes(m_LoadAttrib);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsDocLoaderImpl::AddChildGroup(nsIURLGroup* aGroup)
{
  mChildGroupList.AppendElement(aGroup);
  return NS_OK;
}


NS_IMETHODIMP
nsDocLoaderImpl::RemoveChildGroup(nsIURLGroup* aGroup)
{
  nsresult rv = NS_OK;

  if (PR_FALSE == mChildGroupList.RemoveElement(aGroup)) {
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}


void nsDocLoaderImpl::FireOnStartDocumentLoad(nsIURL* aURL, 
                                              const char* aCommand)
{
  PRInt32 count = mDocObservers.Count();
  PRInt32 index;

  /*
   * First notify any observers that the URL load has begun...
   */
  for (index = 0; index < count; index++) {
    nsIDocumentLoaderObserver* observer = (nsIDocumentLoaderObserver*)mDocObservers.ElementAt(index);
    observer->OnStartDocumentLoad(aURL, aCommand);
  }

  /*
   * Finally notify the parent...
   */
  if (nsnull != mParent) {
    mParent->FireOnStartDocumentLoad(aURL, aCommand);
  }
}

void nsDocLoaderImpl::FireOnEndDocumentLoad(PRInt32 aStatus)
{
  PRInt32 count = mDocObservers.Count();
  PRInt32 index;

  /*
   * First notify any observers that the URL load has begun...
   */
  for (index = 0; index < count; index++) {
    nsIDocumentLoaderObserver* observer = (nsIDocumentLoaderObserver*)mDocObservers.ElementAt(index);
    observer->OnEndDocumentLoad(mDocumentUrl, aStatus);
  }

  /*
   * Finally notify the parent...
   */
  if (nsnull != mParent) {
    mParent->FireOnEndDocumentLoad(aStatus);
  }
}

void nsDocLoaderImpl::FireOnStartURLLoad(nsIURL* aURL, const char* aContentType, 
                                         nsIContentViewer* aViewer)
{
  PRInt32 count = mDocObservers.Count();
  PRInt32 index;

  /*
   * First notify any observers that the URL load has begun...
   */
  for (index = 0; index < count; index++) {
    nsIDocumentLoaderObserver* observer = (nsIDocumentLoaderObserver*)mDocObservers.ElementAt(index);
    observer->OnStartURLLoad(aURL, aContentType, aViewer);
  }

  /*
   * Finally notify the parent...
   */
  if (nsnull != mParent) {
    mParent->FireOnStartURLLoad(aURL, aContentType, aViewer);
  }
}

void nsDocLoaderImpl::FireOnProgressURLLoad(nsIURL* aURL, PRUint32 aProgress,
                                            PRUint32 aProgressMax)
{
  PRInt32 count = mDocObservers.Count();
  PRInt32 index;

  /*
   * First notify any observers that there is progress information available...
   */
  for (index = 0; index < count; index++) {
    nsIDocumentLoaderObserver* observer = (nsIDocumentLoaderObserver*)mDocObservers.ElementAt(index);
    observer->OnProgressURLLoad(aURL, aProgress, aProgressMax);
  }

  /*
   * Finally notify the parent...
   */
  if (nsnull != mParent) {
    mParent->FireOnProgressURLLoad(aURL, aProgress, aProgressMax);
  }
}

void nsDocLoaderImpl::FireOnStatusURLLoad(nsIURL* aURL, nsString& aMsg)
{
  PRInt32 count = mDocObservers.Count();
  PRInt32 index;

  /*
   * First notify any observers that there is status text available...
   */
  for (index = 0; index < count; index++) {
    nsIDocumentLoaderObserver* observer = (nsIDocumentLoaderObserver*)mDocObservers.ElementAt(index);
    observer->OnStatusURLLoad(aURL, aMsg);
  }

  /*
   * Finally notify the parent...
   */
  if (nsnull != mParent) {
    mParent->FireOnStatusURLLoad(aURL, aMsg);
  }
}

void nsDocLoaderImpl::FireOnEndURLLoad(nsIURL* aURL, PRInt32 aStatus)
{
  PRInt32 count = mDocObservers.Count();
  PRInt32 index;

  /*
   * First notify any observers that the URL load has begun...
   */
  for (index = 0; index < count; index++) {
    nsIDocumentLoaderObserver* observer = (nsIDocumentLoaderObserver*)mDocObservers.ElementAt(index);
    observer->OnEndURLLoad(aURL, aStatus);
  }

  /*
   * Finally notify the parent...
   */
  if (nsnull != mParent) {
    mParent->FireOnEndURLLoad(aURL, aStatus);
  }
}



void nsDocLoaderImpl::LoadURLComplete(nsIURL* aURL, nsISupports* aBindInfo, PRInt32 aStatus)
{
  PRBool rv;
  PRBool bIsForegroundURL = PR_FALSE;
  PRBool bIsBusy = PR_TRUE;

  /*
   * If the entry is not found in the list, then it must have been cancelled
   * via Stop(...). So ignore just it... 
   */
  rv = m_LoadingDocsList->RemoveElement(aBindInfo);
  if (PR_FALSE != rv) {
    nsILoadAttribs* loadAttributes;
    nsURLLoadType loadType = nsURLLoadNormal;

    rv = aURL->GetLoadAttribs(&loadAttributes);
    if (NS_SUCCEEDED(rv)) {
      rv = loadAttributes->GetLoadType(&loadType);
      if (NS_FAILED(rv)) {
        loadType = nsURLLoadNormal;
      }
      NS_RELEASE(loadAttributes);
    }
    if (nsURLLoadBackground != loadType) {
      mForegroundURLs -= 1;
      bIsForegroundURL = PR_TRUE;
    }
    mTotalURLs -= 1;

    NS_ASSERTION((mTotalURLs >= mForegroundURLs), "Foreground URL count is wrong.");

#if defined(DEBUG)
  const char* buffer;

  aURL->GetSpec(&buffer);
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader - LoadURLComplete(...) called for %s.\nForeground URLs: %d\nTotal URLs: %d\n", 
            buffer, mForegroundURLs, mTotalURLs));
#endif /* DEBUG */
  }
  /*
   * Fire the OnEndURLLoad notification to any observers...
   */
   FireOnEndURLLoad(aURL, aStatus);

  /*
   * Fire the OnEndDocumentLoad notification to any observers...
   */
   IsBusy(bIsBusy);

  if (bIsForegroundURL && !bIsBusy) {
#if defined(DEBUG)
    const char* buffer;

    mDocumentUrl->GetSpec(&buffer);
    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
          ("DocLoader - OnEndDocumentLoad(...) called for %s.\n", buffer));
#endif /* DEBUG */

    FireOnEndDocumentLoad(aStatus);
  }

}

#if 0
void nsDocLoaderImpl::AreAllConnectionsComplete(void)
{
  PRBool bIsBusy = PR_TRUE;

  /*
   * Fire an OnConnectionsComplete(...) notification if the document loader 
   * is looading a document, but all of the document URLs have been completed.
   */
  if (mIsLoadingDocument) {
    IsBusy(bIsBusy);

    if (!bIsBusy) {
      PRInt32 count = mDocObservers.Count();
      PRInt32 index;

      PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
             ("DocLoader [%p] - OnConnectionsComplete(...) called.\n", this));
      /*
       * Clear the flag indicating that the document loader is still loading a 
       * document...
       */
      mIsLoadingDocument = PR_FALSE;

      /* Notify all observers that all connections have been loaded... */
      for (index = 0; index < count; index++) {
        nsIDocumentLoaderObserver* observer = (nsIDocumentLoaderObserver*)mDocObservers.ElementAt(index);
        observer->OnConnectionsComplete();
      }
    }
  }
  /*
   * See if the parent document loader has finished all of its connections 
   * too..
   */
  if ((!bIsBusy) && (nsnull != mParent)) {
    mParent->AreAllConnectionsComplete();
  }
}
#endif  /* 0 */

void nsDocLoaderImpl::SetParent(nsDocLoaderImpl* aParent)
{
  NS_IF_RELEASE(mParent);
  mParent = aParent;
  NS_IF_ADDREF(mParent);
}

void nsDocLoaderImpl::SetDocumentUrl(nsIURL* aUrl)
{
  NS_IF_RELEASE(mDocumentUrl);
  mDocumentUrl = aUrl;
  NS_IF_ADDREF(mDocumentUrl);
}

PRBool nsDocLoaderImpl::StopBindInfoEnumerator(nsISupports* aElement, void* aData)
{
    nsresult rv;
    nsDocumentBindInfo* bindInfo;

    rv = aElement->QueryInterface(kDocumentBindInfoIID, (void**)&bindInfo);
    if (NS_SUCCEEDED(rv)) {
        bindInfo->Stop();
        NS_RELEASE(bindInfo);
    }

    return PR_TRUE;
}


PRBool nsDocLoaderImpl::StopDocLoaderEnumerator(void* aElement, void* aData)
{
  nsresult rv;
  nsIDocumentLoader* docLoader;
    
  rv = ((nsISupports*)aElement)->QueryInterface(kIDocumentLoaderIID, (void**)&docLoader);
  if (NS_SUCCEEDED(rv)) {
    docLoader->Stop();
    NS_RELEASE(docLoader);
  }

  return PR_TRUE;
}


PRBool nsDocLoaderImpl::IsBusyEnumerator(void* aElement, void* aData)
{
  nsresult rv;
  nsIDocumentLoader* docLoader;
  PRBool* result = (PRBool*)aData;
    
  rv = ((nsISupports*)aElement)->QueryInterface(kIDocumentLoaderIID, (void**)&docLoader);
  if (NS_SUCCEEDED(rv)) {
    docLoader->IsBusy(*result);
    NS_RELEASE(docLoader);
  }

  return !(*result);
}

/****************************************************************************
 * nsDocumentBindInfo implementation...
 ****************************************************************************/

nsDocumentBindInfo::nsDocumentBindInfo()
{
    NS_INIT_REFCNT();

    m_Command = nsnull;
    m_Url = nsnull;
    m_Container = nsnull;
    m_ExtraInfo = nsnull;
    m_Observer = nsnull;
    m_NextStream = nsnull;
    m_DocLoader = nsnull;
    mStatus = NS_OK;
}

nsresult
nsDocumentBindInfo::Init(nsDocLoaderImpl* aDocLoader,
                         const char *aCommand, 
                         nsIContentViewerContainer* aContainer,
                         nsISupports* aExtraInfo,
                         nsIStreamObserver* anObserver)
{

    m_Url        = nsnull;
    m_NextStream = nsnull;
    m_Command    = (nsnull != aCommand) ? PL_strdup(aCommand) : nsnull;

    m_DocLoader = aDocLoader;
    NS_ADDREF(m_DocLoader);

    m_Container = aContainer;
    NS_IF_ADDREF(m_Container);

    m_Observer = anObserver;
    NS_IF_ADDREF(m_Observer);

    m_ExtraInfo = aExtraInfo;
    NS_IF_ADDREF(m_ExtraInfo);

    mStatus = NS_OK;
    return NS_OK;
}

nsDocumentBindInfo::~nsDocumentBindInfo()
{
    if (m_Command) {
        PR_Free(m_Command);
    }
    m_Command = nsnull;

    NS_RELEASE   (m_DocLoader);
    NS_IF_RELEASE(m_Url);
    NS_IF_RELEASE(m_NextStream);
    NS_IF_RELEASE(m_Container);
    NS_IF_RELEASE(m_Observer);
    NS_IF_RELEASE(m_ExtraInfo);
}

/*
 * Implementation of ISupports methods...
 */
NS_IMPL_ADDREF(nsDocumentBindInfo);
NS_IMPL_RELEASE(nsDocumentBindInfo);

nsresult
nsDocumentBindInfo::QueryInterface(const nsIID& aIID,
                                   void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtrResult = NULL;

  if (aIID.Equals(kIStreamObserverIID)) {
    *aInstancePtrResult = (void*) ((nsIStreamObserver*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStreamListenerIID)) {
    *aInstancePtrResult = (void*) ((nsIStreamListener*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kDocumentBindInfoIID)) {
    *aInstancePtrResult = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kRefreshURLIID)) {
    *aInstancePtrResult = (void*) ((nsIRefreshUrl*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult nsDocumentBindInfo::Bind(const nsString& aURLSpec, 
                                  nsIPostData* aPostData,
                                  nsIStreamListener* aListener)
{
    nsresult rv;
    nsIURL* url = nsnull;

    /* If this nsDocumentBindInfo was created with a container pointer.
     * extract the nsISupports iface from it and create the url with 
     * the nsISupports pointer so the backend can have access to the front
     * end nsIContentViewerContainer for refreshing urls.
     */
    rv = m_DocLoader->CreateURL(&url, nsnull, aURLSpec, m_Container);
    if (NS_FAILED(rv)) {
      return rv;
    }

    /* Store any POST data into the URL */
    if (nsnull != aPostData) {
        static NS_DEFINE_IID(kPostToServerIID, NS_IPOSTTOSERVER_IID);
        nsIPostToServer* pts;

        rv = url->QueryInterface(kPostToServerIID, (void **)&pts);
        if (NS_SUCCEEDED(rv)) {
            const char* data = aPostData->GetData();

            if (aPostData->IsFile()) {
                pts->SendDataFromFile(data);
            }
            else {
                pts->SendData(data, aPostData->GetDataLength());
            }
        }
    }

    /*
     * Set the URL has the current "document" being loaded...
     */
    m_DocLoader->SetDocumentUrl(url);
    /*
     * Fire the OnStarDocumentLoad interfaces 
     */
    m_DocLoader->FireOnStartDocumentLoad(url, m_Command);
    /*
     * Initiate the network request...
     */
    rv = Bind(url, aListener);
    NS_RELEASE(url);

    return rv;
}


nsresult nsDocumentBindInfo::Bind(nsIURL* aURL, nsIStreamListener* aListener)
{
  nsresult rv = NS_OK;
  nsINetService *inet = nsnull;

  m_Url = aURL;
  NS_ADDREF(m_Url);

#if defined(DEBUG)
  const char *buffer;

  aURL->GetSpec(&buffer);
  PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
         ("DocLoader - OnStartDocumentLoad(...) called for %s.\n", buffer));
#endif /* DEBUG */

  //  m_DocLoader->FireOnStartDocumentLoad(aURL, m_Command);

  /* Set up the stream listener (if provided)... */
  if (nsnull != aListener) {
    m_NextStream = aListener;
    NS_ADDREF(m_NextStream);
  }

  /* Start the URL binding process... */
  rv = nsServiceManager::GetService(kNetServiceCID,
                                    kINetServiceIID,
                                    (nsISupports **)&inet);
  if (NS_SUCCEEDED(rv)) {
    rv = inet->OpenStream(m_Url, this);
    nsServiceManager::ReleaseService(kNetServiceCID, inet);
  }

  return rv;
}


nsresult nsDocumentBindInfo::Stop(void)
{
  nsresult rv;
  nsINetService* inet;
  if (m_Url == nsnull) return NS_OK;

#if defined(DEBUG)
  const char* spec;

  rv = m_Url->GetSpec(&spec);
  if (NS_SUCCEEDED(rv))
      PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
             ("DocLoader - Stop(...) called for %s.\n", spec));
#endif /* DEBUG */

  /* 
   * Mark the IStreamListener as being aborted...  If more data is pushed
   * down the stream, the connection will be aborted...
   */
  mStatus = NS_BINDING_ABORTED;

  /* Stop the URL binding process... */
  rv = nsServiceManager::GetService(kNetServiceCID,
                                    kINetServiceIID,
                                    (nsISupports **)&inet);
  if (NS_SUCCEEDED(rv)) {
    rv = inet->InterruptStream(m_Url);
    nsServiceManager::ReleaseService(kNetServiceCID, inet);
  }

  return rv;
}


NS_METHOD nsDocumentBindInfo::GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo)
{
    nsresult rv = NS_OK;

    NS_PRECONDITION(nsnull !=m_NextStream, "DocLoader: No stream for document");

    if (nsnull != m_NextStream) {
        rv = m_NextStream->GetBindInfo(aURL, aInfo);
    }

    return rv;
}


NS_METHOD nsDocumentBindInfo::OnProgress(nsIURL* aURL, PRUint32 aProgress, 
                                         PRUint32 aProgressMax)
{
    nsresult rv = NS_OK;

#if defined(DEBUG)
    const char* spec;
    (void)aURL->GetSpec(&spec);

    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader - OnProgress(...) called for %s.  Progress: %d.  ProgressMax: %d\n", 
            spec, aProgress, aProgressMax));
#endif /* DEBUG */

    /* Pass the notification out to the next stream listener... */
    if (nsnull != m_NextStream) {
        rv = m_NextStream->OnProgress(aURL, aProgress, aProgressMax);
    }

    /* Pass the notification out to any observers... */
    m_DocLoader->FireOnProgressURLLoad(aURL, aProgress, aProgressMax);

    /* Pass the notification out to the Observer... */
    if (nsnull != m_Observer) {
        /* XXX: Should we ignore the return value? */
        (void) m_Observer->OnProgress(aURL, aProgress, aProgressMax);
    }

    return rv;
}


NS_METHOD nsDocumentBindInfo::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
    nsresult rv = NS_OK;

    /* Pass the notification out to the next stream listener... */
    if (nsnull != m_NextStream) {
        rv = m_NextStream->OnStatus(aURL, aMsg);
    }

    /* Pass the notification out to any observers... */
    nsString msgStr(aMsg);
    m_DocLoader->FireOnStatusURLLoad(aURL, msgStr);

    /* Pass the notification out to the Observer... */
    if (nsnull != m_Observer) {
        /* XXX: Should we ignore the return value? */
        (void) m_Observer->OnStatus(aURL, aMsg);
    }

    return rv;
}


NS_METHOD nsDocumentBindInfo::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
    nsresult rv = NS_OK;
    nsIContentViewer* viewer = nsnull;

#if defined(DEBUG)
    const char* spec;
    (void)aURL->GetSpec(&spec);

    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader - OnStartBinding(...) called for %s.  Content-type is %s\n",
            spec, aContentType));
#endif /* DEBUG */

    /* If the binding has been canceled via Stop() then abort the load... */
    if (NS_BINDING_ABORTED == mStatus) {
        rv = NS_BINDING_ABORTED;
        goto done;
    }

    if (nsnull == m_NextStream) {

        /*
         * Now that the content type is available, create a document (and viewer)
         * of the appropriate type...
         */
        if (m_DocLoader->m_DocFactory) {
            rv = m_DocLoader->m_DocFactory->CreateInstance(m_Url,
                                                           aContentType, 
                                                           m_Command, 
                                                           m_Container,
                                                           m_ExtraInfo,
                                                           &m_NextStream, 
                                                           &viewer);
        } else {
            rv = NS_ERROR_NULL_POINTER;
        }

        if (NS_FAILED(rv)) {
            printf("DocLoaderFactory: Unable to create ContentViewer for content-type: %s\n", aContentType);
            if ( m_Container ) {
                // Give content container a chance to do something with this URL.
                rv = m_Container->HandleUnknownContentType( aURL, aContentType, m_Command );
            }
            // Stop the binding.
            // This crashes on Unix/Mac... Stop();
            goto done;
        }

        /*
         * Give the document container the new viewer...
         */
        if (m_Container) {
            viewer->SetContainer(m_Container);

            rv = m_Container->Embed(viewer, m_Command, m_ExtraInfo);
            if (NS_FAILED(rv)) {
                goto done;
            }
        }

    }

    /*
     * Pass the OnStartBinding(...) notification out to the document 
     * IStreamListener.
     */
    if (nsnull != m_NextStream) {
        rv = m_NextStream->OnStartBinding(aURL, aContentType);
    }

    /*
     * Notify the DocumentLoadObserver(s) 
     */
    if ((nsnull == viewer) && (nsnull != m_Container)) {
      m_Container->GetContentViewer(&viewer);
    }
    m_DocLoader->FireOnStartURLLoad(m_Url, aContentType, viewer);

    /* Pass the notification out to the Observer... */
    if (nsnull != m_Observer) {
        nsresult rv2 = m_Observer->OnStartBinding(aURL, aContentType);
        if (NS_SUCCEEDED(rv))
        	rv = rv2;
    }

done:
    NS_IF_RELEASE(viewer);

    return rv;
}


NS_METHOD nsDocumentBindInfo::OnDataAvailable(nsIURL* aURL, 
                                              nsIInputStream *aStream, PRUint32 aLength)
{
    nsresult rv = NS_OK;

#if defined(DEBUG)
    const char* spec;
    (void)aURL->GetSpec(&spec);

    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader - OnDataAvailable(...) called for %s.  Bytes available: %d.\n", 
            spec, aLength));
#endif /* DEBUG */

    /* If the binding has been canceled via Stop() then abort the load... */
    if (NS_BINDING_ABORTED == mStatus) {
        rv = NS_BINDING_ABORTED;
        goto done;
    }

    if (nsnull != m_NextStream) {
       /*
        * Bump the refcount in case the stream gets destroyed while the data
        * is being processed...  If Stop(...) is called the stream could be
        * freed prematurely :-(
        *
        * Currently this can happen if javascript loads a new URL 
        * (via nsIWebShell::LoadURL) during the parse phase... 
        */
        nsIStreamListener* listener = m_NextStream;

        NS_ADDREF(listener);
        rv = listener->OnDataAvailable(aURL, aStream, aLength);
        NS_RELEASE(listener);
    } else {
      rv = NS_BINDING_FAILED;
    }

done:
    return rv;
}


NS_METHOD nsDocumentBindInfo::OnStopBinding(nsIURL* aURL, nsresult aStatus, 
                                            const PRUnichar* aMsg)
{
    nsresult rv = NS_OK;

#if defined(DEBUG)
    const char* spec;
    (void)aURL->GetSpec(&spec);

    PR_LOG(gDocLoaderLog, PR_LOG_DEBUG, 
           ("DocLoader - OnStopBinding(...) called for %s.  Status: %d.\n", 
            spec, aStatus));
#endif /* DEBUG */

    if (nsnull != m_NextStream) {
        rv = m_NextStream->OnStopBinding(aURL, aStatus, aMsg);
    }

    /* Pass the notification out to the Observer... */
    if (nsnull != m_Observer) {
        /* XXX: Should we ignore the return value? */
        (void) m_Observer->OnStopBinding(aURL, aStatus, aMsg);
    }

    /*
     * The stream is complete...  Tell the DocumentLoader to release us...
     */
    m_DocLoader->LoadURLComplete(aURL, (nsIStreamListener *)this, aStatus);
    NS_IF_RELEASE(m_NextStream);

    return rv;
}

NS_METHOD
nsDocumentBindInfo::RefreshURL(nsIURL* aURL, PRInt32 millis, PRBool repeat)
{
    if (nsnull != m_Container) {
        nsresult rv;
        nsIRefreshUrl* refresher = nsnull;

        /* Delegate the actual refresh call up-to the container. */
        rv = m_Container->QueryInterface(kRefreshURLIID, (void**)&refresher);

        if (NS_FAILED(rv)) {
            return PR_FALSE;
        }
        rv = refresher->RefreshURL(aURL, millis, repeat);
        NS_RELEASE(refresher);
        return rv;
    }
    return PR_FALSE;
}

NS_METHOD
nsDocumentBindInfo::CancelRefreshURLTimers(void)
{
    if (nsnull != m_Container) {
        nsresult rv;
        nsIRefreshUrl* refresher = nsnull;

        /* Delegate the actual cancel call up-to the container. */
        rv = m_Container->QueryInterface(kRefreshURLIID, (void**)&refresher);

        if (NS_FAILED(rv)) {
            return PR_FALSE;
        }
        rv = refresher->CancelRefreshURLTimers();
        NS_RELEASE(refresher);
        return rv;
    }
    return PR_FALSE;
}

/*******************************************
 *  nsDocLoaderServiceFactory
 *******************************************/
static nsDocLoaderImpl* gServiceInstance = nsnull;

NS_DEF_FACTORY(DocLoaderServiceGen,nsDocLoaderImpl)

class nsDocLoaderServiceFactory : public nsDocLoaderServiceGenFactory
{
public:
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);
};

NS_IMETHODIMP
nsDocLoaderServiceFactory::CreateInstance(nsISupports *aOuter,
                                          const nsIID &aIID,
                                          void **aResult)
{
  nsresult rv;
  nsDocLoaderImpl* inst;

  // Parameter validation...
  if (NULL == aResult) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }
  // Do not support aggregatable components...
  *aResult = NULL;
  if (NULL != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    goto done;
  }

  if (NULL == gServiceInstance) {
    // Create a new instance of the component...
    NS_NEWXPCOM(gServiceInstance, nsDocLoaderImpl);
    if (NULL == gServiceInstance) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }
    NS_ADDREF(gServiceInstance);
  }

  // If the QI fails, the component will be destroyed...
  //
  // Use a local copy so the NS_RELEASE() will not null the global
  // pointer...
  inst = gServiceInstance;

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

done:
  return rv;
}


// Entry point to create nsDocLoaderService factory instances...

nsresult NS_NewDocLoaderServiceFactory(nsIFactory** aResult)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsDocLoaderServiceFactory();
  if (NULL == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aResult = inst;
  return rv;
}
