/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nspr.h"
#include "nsFTPChannel.h"
#include "nsFtpProtocolHandler.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIEventSinkGetter.h"
#include "nsIProgressEventSink.h"
#include "nsConnectionCacheObj.h"

#if defined(PR_LOGGING)
//
// Log module for FTP Protocol logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsFTPProtocol:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gFTPLog = nsnull;

#endif /* PR_LOGGING */

static NS_DEFINE_CID(kStandardURLCID,            NS_STANDARDURL_CID);

////////////////////////////////////////////////////////////////////////////////

nsFtpProtocolHandler::nsFtpProtocolHandler() {
    NS_INIT_REFCNT();
}

nsFtpProtocolHandler::~nsFtpProtocolHandler() {
}


NS_IMETHODIMP_(nsrefcnt) nsFtpProtocolHandler::AddRef(void)
{                                                            
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");  
  ++mRefCnt;                                                 
  NS_LOG_ADDREF(this, mRefCnt, "nsFtpProtocolHandler", sizeof(*this));      
  return mRefCnt;                                            
}
                          
NS_IMETHODIMP_(nsrefcnt) nsFtpProtocolHandler::Release(void)               
{                                                            
  NS_PRECONDITION(0 != mRefCnt, "dup release");              
  --mRefCnt;                                                 
  NS_LOG_RELEASE(this, mRefCnt, "nsFtpProtocolHandler");                    
  if (mRefCnt == 0) {                                        
    mRefCnt = 1; /* stabilize */                             
    NS_DELETEXPCOM(this);                                    
    return 0;                                                
  }                                                          
  return mRefCnt;                                            
}

NS_IMPL_QUERY_INTERFACE3(nsFtpProtocolHandler, nsIProtocolHandler, nsIConnectionCache, nsIObserver);

NS_METHOD
nsFtpProtocolHandler::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{

#if defined(PR_LOGGING)
    //
    // Initialize the global PRLogModule for FTP Protocol logging 
    // if necessary...
    //
    if (nsnull == gFTPLog) {
        gFTPLog = PR_NewLogModule("nsFTPProtocol");
    }
#endif /* PR_LOGGING */

    nsFtpProtocolHandler* ph = new nsFtpProtocolHandler();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->Init();
    if (NS_FAILED(rv)) return rv;
    rv = ph->QueryInterface(aIID, aResult);
    NS_RELEASE(ph);
    return rv;
}

nsresult
nsFtpProtocolHandler::Init() {
    nsresult rv;
    NS_NEWXPCOM(mRootConnectionList, nsHashtable);
    if (!mRootConnectionList) return NS_ERROR_OUT_OF_MEMORY;
    rv = NS_NewThreadPool(getter_AddRefs(mPool), NS_FTP_CONNECTION_COUNT,
                          NS_FTP_CONNECTION_COUNT,
                          NS_FTP_CONNECTION_STACK_SIZE);
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIObserverService, obsServ, NS_OBSERVERSERVICE_PROGID, &rv);
    if (NS_SUCCEEDED(rv)) {
        nsAutoString topic(NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        obsServ->AddObserver(this, topic.GetUnicode());
    }
    return rv;
}
    
////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsFtpProtocolHandler::GetScheme(char* *result)
{
    *result = nsCRT::strdup("ftp");
    if (*result == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsFtpProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = 21; 
    return NS_OK;
}

NS_IMETHODIMP
nsFtpProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                             nsIURI **result)
{
    nsresult rv;
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("FTP attempt at %s ", aSpec));

    // Ftp URLs (currently) have no additional structure beyond that provided by standard
    // URLs, so there is no "outer" given to CreateInstance 

    nsIURI* url;
    if (aBaseURI) {
        rv = aBaseURI->Clone(&url);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetRelativePath(aSpec);
    }
    else {
        rv = nsComponentManager::CreateInstance(kStandardURLCID, nsnull,
                                                NS_GET_IID(nsIURI),
                                                (void**)&url);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetSpec((char*)aSpec);
    }
    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
        return rv;
    }

    // XXX this is the default port for ftp. we need to strip out the actual
    // XXX requested port.
    rv = url->SetPort(21);
    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
        return rv;
    }

    *result = url;
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
    return rv;
}

NS_IMETHODIMP
nsFtpProtocolHandler::NewChannel(const char* verb, nsIURI* url,
                                 nsILoadGroup *aGroup,
                                 nsIEventSinkGetter* eventSinkGetter,
                                 nsIURI* originalURI,
                                 nsIChannel* *result)
{
    nsresult rv;
    
    nsFTPChannel* channel;
    rv = nsFTPChannel::Create(nsnull, NS_GET_IID(nsIFTPChannel), (void**)&channel);
    if (NS_FAILED(rv)) return rv;

    rv = channel->Init(verb, url, aGroup, eventSinkGetter, originalURI, this, mPool);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFtpProtocolHandler::NewChannel() FAILED\n"));
        return rv;
    }

    *result = channel;
    return NS_OK;
}

// nsIConnectionCache methods
NS_IMETHODIMP
nsFtpProtocolHandler::RemoveConn(const char *aKey, nsConnectionCacheObj* *_retval) {
    NS_ASSERTION(_retval, "null pointer");
    nsStringKey key(aKey);
    *_retval = (nsConnectionCacheObj*)mRootConnectionList->Remove(&key);
    return NS_OK;
}

NS_IMETHODIMP
nsFtpProtocolHandler::InsertConn(const char *aKey, nsConnectionCacheObj *aConn) {
    NS_ASSERTION(aConn, "null pointer");
    nsStringKey key(aKey);
    mRootConnectionList->Put(&key, aConn);
    return NS_OK;
}

// cleans up a connection list entry
PRBool CleanupConnEntry(nsHashKey *aKey, void *aData, void *closure) {
    // XXX do we need to explicitly close the streams?
    delete (nsConnectionCacheObj*)aData;
    return PR_TRUE;
}

// nsIObserver method
NS_IMETHODIMP
nsFtpProtocolHandler::Observe(nsISupports     *aSubject,
                              const PRUnichar *aTopic,
                              const PRUnichar *someData ) {
    nsresult rv;
    if (mRootConnectionList) {
        mRootConnectionList->Reset(CleanupConnEntry);
        NS_DELETEXPCOM(mRootConnectionList);
        mRootConnectionList = nsnull;
    }
    // remove ourself from the observer service.
    NS_WITH_SERVICE(nsIObserverService, obsServ, NS_OBSERVERSERVICE_PROGID, &rv);
    if (NS_SUCCEEDED(rv)) {
        nsAutoString topic(NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        obsServ->RemoveObserver(this, topic.GetUnicode());
    }
    return NS_OK;
}
////////////////////////////////////////////////////////////////////////////////
