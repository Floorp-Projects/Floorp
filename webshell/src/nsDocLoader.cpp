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
#include "prmem.h"
#include "plstr.h"
#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"
#include "nsIPostToServer.h"
#include "nsIFactory.h"
#include "nsIDocumentLoader.h"
#include "nsIContentViewerContainer.h"

// XXX: Only needed for dummy factory...
#include "nsIDocument.h"
#include "nsIDocumentViewer.h"
#include "nsICSSParser.h"

/* Forward declarations.... */
class nsDocLoaderImpl;


/* 
 * The nsDocumentBindInfo contains the state required when a single document
 * is being loaded...  Each instance remains alive until its target URL has 
 * been loaded (or aborted).
 *
 * The Document Loader maintains a list of nsDocumentBindInfo instances which 
 * represents the set of documents actively being loaded...
 */
class nsDocumentBindInfo : public nsIStreamListener
{
public:
    nsDocumentBindInfo(nsDocLoaderImpl* aDocLoader,
                       const char *aCommand, 
                       nsIContentViewerContainer* aContainer,
                       nsISupports* aExtraInfo,
                       nsIStreamObserver* anObserver);

    NS_DECL_ISUPPORTS

    nsresult Bind(const nsString& aURLSpec, nsIPostData* aPostData);

    /* nsIStreamListener interface methods... */
    NS_IMETHOD GetBindInfo(nsIURL* aURL);
    NS_IMETHOD OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax, 
                          const nsString& aMsg);
    NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
    NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aStream, PRInt32 aLength);
    NS_IMETHOD OnStopBinding(nsIURL* aURL, PRInt32 aStatus, const nsString& aMsg);

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
};


class nsDocFactoryImpl : public nsIDocumentLoaderFactory
{
public:
    nsDocFactoryImpl();

    NS_DECL_ISUPPORTS

    NS_IMETHOD CreateInstance(nsIURL* aURL,
                              const char* aContentType, 
                              const char* aCommand,
                              nsIContentViewerContainer* aContainer,
                              nsIStreamListener** aDocListener,
                              nsIContentViewer** aDocViewer);

    nsresult InitUAStyleSheet();
};

static nsIStyleSheet* gUAStyleSheet;

nsDocFactoryImpl::nsDocFactoryImpl()
{
    NS_INIT_REFCNT();
}

/*
 * Implementation of ISupports methods...
 */
NS_DEFINE_IID(kIDocumentLoaderFactoryIID, NS_IDOCUMENTLOADERFACTORY_IID);
NS_IMPL_ISUPPORTS(nsDocFactoryImpl,kIDocumentLoaderFactoryIID);

static char* gValidTypes[] = {"text/html","text/xml","application/rtf",0};

NS_IMETHODIMP
nsDocFactoryImpl::CreateInstance(nsIURL* aURL, 
                                 const char* aContentType, 
                                 const char *aCommand,
                                 nsIContentViewerContainer* aContainer,
                                 nsIStreamListener** aDocListener,
                                 nsIContentViewer** aDocViewer)
{
    nsresult rv = NS_ERROR_FAILURE;
    nsIDocument* doc = nsnull;
    nsIDocumentViewer* docv = nsnull;

    int typeIndex=0;
    while(gValidTypes[typeIndex]) {
      if (0== PL_strcmp(gValidTypes[typeIndex++], aContentType)) {
        goto nextstep;
      }
    }
    goto done;

nextstep:
    // Load the UA style sheet if we haven't already done that
    if (nsnull == gUAStyleSheet) {
        InitUAStyleSheet();
    }

    /*
     * Create the HTML document...
     */
    rv = NS_NewHTMLDocument(&doc);
    if (NS_OK != rv) {
        goto done;
    }

    /*
     * Create the HTML Content Viewer...
     */
    rv = NS_NewDocumentViewer(docv);
    if (NS_OK != rv) {
        goto done;
    }
    docv->SetUAStyleSheet(gUAStyleSheet);

    /* 
     * Initialize the document to begin loading the data...
     *
     * An nsIStreamListener connected to the parser is returned in
     * aDocListener.
     */
    rv = doc->StartDocumentLoad(aURL, aContainer, aDocListener);
    if (NS_OK != rv) {
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

#define UA_CSS_URL "resource:/res/ua.css"

nsresult nsDocFactoryImpl::InitUAStyleSheet()
{
  nsresult rv = NS_OK;

  if (nsnull == gUAStyleSheet) {  // snarf one
    nsIURL* uaURL;
    rv = NS_NewURL(&uaURL, nsnull, UA_CSS_URL); // XXX this bites, fix it
    if (NS_OK == rv) {
      // Get an input stream from the url
      PRInt32 ec;
      nsIInputStream* in = uaURL->Open(&ec);
      if (nsnull != in) {
        // Translate the input using the argument character set id into unicode
        nsIUnicharInputStream* uin;
        rv = NS_NewConverterStream(&uin, nsnull, in);
        if (NS_OK == rv) {
          // Create parser and set it up to process the input file
          nsICSSParser* css;
          rv = NS_NewCSSParser(&css);
          if (NS_OK == rv) {
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
//        printf("open of %s failed: error=%x\n", UA_CSS_URL, ec);
        rv = NS_ERROR_ILLEGAL_VALUE;  // XXX need a better error code here
      }

      NS_RELEASE(uaURL);
    }
  }
  return rv;
}



class nsDocLoaderImpl : public nsIDocumentLoader
{
public:

    nsDocLoaderImpl();

    NS_DECL_ISUPPORTS

    NS_IMETHOD SetDocumentFactory(nsIDocumentLoaderFactory* aFactory);

    NS_IMETHOD LoadURL(const nsString& aURLSpec, 
                       const char *aCommand,
                       nsIContentViewerContainer* aContainer,
                       nsIPostData* aPostData = nsnull,
                       nsISupports* aExtraInfo = nsnull, 
                       nsIStreamObserver *anObserver = nsnull);

    void LoadURLComplete(nsISupports* loader);

protected:
    virtual ~nsDocLoaderImpl();

public:
    nsIDocumentLoaderFactory* m_DocFactory;

protected:
    nsISupportsArray* m_LoadingDocsList;
};


nsDocLoaderImpl::nsDocLoaderImpl()
{
    NS_INIT_REFCNT();

    NS_NewISupportsArray(&m_LoadingDocsList);

    m_DocFactory = new nsDocFactoryImpl();
    NS_ADDREF(m_DocFactory);
}


nsDocLoaderImpl::~nsDocLoaderImpl()
{
    NS_IF_RELEASE(m_LoadingDocsList);
    NS_IF_RELEASE(m_DocFactory);
}


/*
 * Implementation of ISupports methods...
 */
NS_DEFINE_IID(kIDocumentLoaderIID, NS_IDOCUMENTLOADER_IID);
NS_IMPL_ISUPPORTS(nsDocLoaderImpl,kIDocumentLoaderIID);


NS_IMETHODIMP
nsDocLoaderImpl::SetDocumentFactory(nsIDocumentLoaderFactory* aFactory)
{
    NS_IF_RELEASE(m_DocFactory);
    m_DocFactory = aFactory;
    NS_IF_ADDREF(m_DocFactory);

    return NS_OK;
}


NS_IMETHODIMP
nsDocLoaderImpl::LoadURL(const nsString& aURLSpec, 
                            const char* aCommand,
                            nsIContentViewerContainer* aContainer,
                            nsIPostData* aPostData,
                            nsISupports* aExtraInfo,
                            nsIStreamObserver* anObserver)
{
    nsresult rv;
    nsDocumentBindInfo* loader = nsnull;

    /* Check for initial error conditions... */
    if (nsnull == aContainer) {
        rv = NS_ERROR_NULL_POINTER;
        goto done;
    }

    loader = new nsDocumentBindInfo(this, aCommand, aContainer, 
                                    aExtraInfo, anObserver);
    if (nsnull == loader) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto done;
    }
    /* The DocumentBindInfo reference is only held by the Array... */
    m_LoadingDocsList->AppendElement(loader);

    rv = loader->Bind(aURLSpec, aPostData);

done:
    return rv;
}


void nsDocLoaderImpl::LoadURLComplete(nsISupports* docInfo)
{
    PRBool rv;

    rv = m_LoadingDocsList->RemoveElement(docInfo);
    NS_POSTCONDITION(PR_TRUE == rv, "DocLoader: Unable to remove completed document");
}





nsDocumentBindInfo::nsDocumentBindInfo(nsDocLoaderImpl* aDocLoader,
                                       const char *aCommand, 
                                       nsIContentViewerContainer* aContainer,
                                       nsISupports* aExtraInfo,
                                       nsIStreamObserver* anObserver)
{
    NS_INIT_REFCNT();

    m_Url        = nsnull;
    m_NextStream = nsnull;
    m_Command    = (nsnull != aCommand) ? PL_strdup(aCommand) : nsnull;
    m_ExtraInfo  = nsnull;

    m_DocLoader = aDocLoader;
    NS_ADDREF(m_DocLoader);

    m_Container = aContainer;
    NS_IF_ADDREF(m_Container);

    m_Observer = anObserver;
    NS_IF_ADDREF(m_Observer);

    m_ExtraInfo = aExtraInfo;
    NS_IF_ADDREF(m_ExtraInfo);
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
NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
NS_IMPL_ISUPPORTS(nsDocumentBindInfo,kIStreamListenerIID);


nsresult nsDocumentBindInfo::Bind(const nsString& aURLSpec, 
                                  nsIPostData* aPostData)
{
    nsresult rv;

    rv = NS_NewURL(&m_Url, aURLSpec);
    if (NS_OK != rv) {
        return rv;
    }

    /* Store any POST data into the URL */
    if (nsnull != aPostData) {
        static NS_DEFINE_IID(kPostToServerIID, NS_IPOSTTOSERVER_IID);
        nsIPostToServer* pts;

        rv = m_Url->QueryInterface(kPostToServerIID, (void **)&pts);
        if (NS_OK == rv) {
            const char* data = aPostData->GetData();

            if (aPostData->IsFile()) {
                pts->SendDataFromFile(data);
            }
            else {
                pts->SendData(data, aPostData->GetDataLength());
            }
        }
    }

    /* Start the URL binding process... */
    rv = m_Url->Open(this);

    return rv;
}


NS_METHOD nsDocumentBindInfo::GetBindInfo(nsIURL* aURL)
{
    nsresult rv = NS_OK;

    NS_PRECONDITION(nsnull !=m_NextStream, "DocLoader: No stream for document");

    if (nsnull != m_NextStream) {
        rv = m_NextStream->GetBindInfo(aURL);
    }

    return rv;
}


NS_METHOD nsDocumentBindInfo::OnProgress(nsIURL* aURL, PRInt32 aProgress, 
                                         PRInt32 aProgressMax, 
                                         const nsString& aMsg)
{
    nsresult rv = NS_OK;

    /* Pass the notification out to the next stream listener... */
    if (nsnull != m_NextStream) {
        rv = m_NextStream->OnProgress(aURL, aProgress, aProgressMax, aMsg);
    }

    /* Pass the notification out to the Observer... */
    if (nsnull != m_Observer) {
        /* XXX: Should we ignore the return value? */
        (void) m_Observer->OnProgress(aURL, aProgress, aProgressMax, aMsg);
    }

    return rv;
}


NS_METHOD nsDocumentBindInfo::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
    nsresult rv = NS_OK;
    nsIContentViewer* viewer = nsnull;

    /*
     * Now that the content type is available, create a document (and viewer)
     * of the appropriate type...
     */
    if (m_DocLoader->m_DocFactory) {
        rv = m_DocLoader->m_DocFactory->CreateInstance(m_Url,
                                                       aContentType, 
                                                       m_Command, 
                                                       m_Container,
                                                       &m_NextStream, 
                                                       &viewer);
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    if (NS_OK != rv) {
        goto done;
    }

    /*
     * Give the document container the new viewer...
     */
    viewer->SetContainer(m_Container);

    rv = m_Container->Embed(viewer, m_Command, m_ExtraInfo);
    if (NS_OK != rv) {
        goto done;
    }

    /*
     * Pass the OnStartBinding(...) notification out to the document 
     * IStreamListener.
     */
    NS_ASSERTION((nsnull != m_NextStream), "No stream was created!");

    if (nsnull != m_NextStream) {
        rv = m_NextStream->OnStartBinding(aURL, aContentType);
    }

    /* Pass the notification out to the Observer... */
    if (nsnull != m_Observer) {
        /* XXX: Should we ignore the return value? */
        (void) m_Observer->OnStartBinding(aURL, aContentType);
    }

done:
    NS_IF_RELEASE(viewer);

    return rv;
}


NS_METHOD nsDocumentBindInfo::OnDataAvailable(nsIURL* aURL, 
                                              nsIInputStream *aStream, PRInt32 aLength)
{
    nsresult rv = NS_OK;

    NS_PRECONDITION(nsnull !=m_NextStream, "DocLoader: No stream for document");

    if (nsnull != m_NextStream) {
        rv = m_NextStream->OnDataAvailable(aURL, aStream, aLength);
    }

    return rv;
}


NS_METHOD nsDocumentBindInfo::OnStopBinding(nsIURL* aURL, PRInt32 aStatus, 
                                            const nsString& aMsg)
{
    nsresult rv = NS_OK;

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
     *
     * This should cause the nsDocumentBindInfo instance to be deleted, so
     * DO NOT assume this is valid after the call!!
     */
    m_DocLoader->LoadURLComplete(this);

    return rv;
}



/*******************************************
 *  nsDocLoaderFactory
 *******************************************/

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kCDocumentLoader, NS_DOCUMENTLOADER_CID);

class nsDocumentLoaderFactory : public nsIFactory
{   
public:   
    nsDocumentLoaderFactory();

    NS_DECL_ISUPPORTS

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter,   
                              const nsIID &aIID,   
                              void **aResult);   

    NS_IMETHOD LockFactory(PRBool aLock);   


protected:
    virtual ~nsDocumentLoaderFactory();   
};   

nsDocumentLoaderFactory::nsDocumentLoaderFactory()
{   
    NS_INIT_REFCNT();
}   

nsDocumentLoaderFactory::~nsDocumentLoaderFactory()   
{   
}   

/*
 * Implementation of ISupports methods...
 */
NS_IMPL_ISUPPORTS(nsDocumentLoaderFactory,kIFactoryIID);


NS_IMETHODIMP
nsDocumentLoaderFactory::CreateInstance(nsISupports* aOuter,  
                                        const nsIID& aIID,  
                                        void** aResult)  
{  
    nsresult rv;
    nsDocLoaderImpl* inst;

    if (nsnull == aResult) {  
        rv = NS_ERROR_NULL_POINTER;
        goto done;
    }  
    *aResult = nsnull;

    if (nsnull != aOuter) {
        rv = NS_ERROR_NO_AGGREGATION;
        goto done;
    }

    inst = new nsDocLoaderImpl();
    if (nsnull == inst) {  
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto done;
    }  

    NS_ADDREF(inst);    // RefCount = 1
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);

done:
    return rv;
}  

NS_IMETHODIMP
nsDocumentLoaderFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  


extern "C" NS_WEB nsresult NS_NewDocumentLoaderFactory(nsIFactory** aFactory)
{
    if (nsnull == aFactory) {
        return NS_ERROR_NULL_POINTER;
    }

    *aFactory = new nsDocumentLoaderFactory();
    if (nsnull == *aFactory) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(*aFactory);

    return NS_OK;
}


