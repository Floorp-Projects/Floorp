/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIString.h"
#include "nsIStreamListener.h"
#include "nsHTTPResponseListener.h"
#include "nsIChannel.h"
#include "nsIBufferInputStream.h"
#include "nsHTTPChannel.h"
#include "nsHTTPResponse.h"
#include "nsIHttpEventSink.h"
#include "nsCRT.h"
#include "stdio.h" //sscanf

#include "nsIHTTPNotify.h"
#include "nsINetModRegEntry.h"
#include "nsProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsINetModuleMgr.h"
#include "nsIEventQueueService.h"

static const int kMAX_FIRST_LINE_SIZE= 256;

nsHTTPResponseListener::nsHTTPResponseListener(): 
    m_pConnection(nsnull),
    m_bFirstLineParsed(PR_FALSE),
    m_pResponse(nsnull),
    m_bHeadersDone(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsHTTPResponseListener::~nsHTTPResponseListener()
{
    if (m_pConnection)
        NS_RELEASE(m_pConnection);
    if (m_pResponse)
        NS_RELEASE(m_pResponse);
}

NS_IMPL_ISUPPORTS(nsHTTPResponseListener,nsIStreamListener::GetIID());

static NS_DEFINE_IID(kProxyObjectManagerIID, NS_IPROXYEVENT_MANAGER_IID);
static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kNetModuleMgrCID, NS_NETMODULEMGR_CID);

NS_IMETHODIMP
nsHTTPResponseListener::OnDataAvailable(nsISupports* context,
                                        nsIBufferInputStream *i_pStream, 
                                        PRUint32 i_SourceOffset,
                                        PRUint32 i_Length)
{
    nsIInputStream* inStr = nsnull;

    // Should I save this as a member variable? yes... todo
    nsIHTTPEventSink* pSink= nsnull;
    m_pConnection->GetEventSink(&pSink);
    NS_VERIFY(pSink, "No HTTP Event Sink!");

    NS_ASSERTION(i_pStream, "Fake stream!");
    // Set up the response
    if (!m_pResponse)
    {
 
        // why do I need the connection in the constructor... get rid.. TODO
        m_pResponse = new nsHTTPResponse (m_pConnection, /* inStr */ i_pStream);
        if (!m_pResponse)
        {
            NS_ERROR("Failed to create the response object!");
            return NS_ERROR_OUT_OF_MEMORY;
        }
        NS_ADDREF(m_pResponse);
        nsHTTPChannel* pTestCon = NS_STATIC_CAST(nsHTTPChannel*, m_pConnection);
        pTestCon->SetResponse(m_pResponse);
    }
 
    //printf("nsHTTPResponseListener::OnDataAvailable...\n");

    char extrabuffer[kMAX_FIRST_LINE_SIZE];
    int extrabufferlen = 0;
    
    char partHeader[kMAX_FIRST_LINE_SIZE];
    int partHeaderLen = 0;

    while (!m_bHeadersDone)
    {
        //TODO optimize this further!
        char buffer[kMAX_FIRST_LINE_SIZE];  
        PRUint32 length;

        nsresult stat = i_pStream->Read(buffer, kMAX_FIRST_LINE_SIZE, &length);
        NS_ASSERTION(buffer, "Argh...");

        char* p = buffer;
        while (buffer+length > p)
        {
            char* lineStart = p;
            if (*lineStart == '\0' || *lineStart == CR)
            {
                m_bHeadersDone = PR_TRUE;
                // we read extra so save it for the other headers
                if (buffer+length > p)
                {
                    extrabufferlen = length - (buffer - p);
                    PL_strncpy(extrabuffer, p, extrabufferlen);
                }
                
                //TODO process headers here. 

                pSink->OnHeadersAvailable(context);

                break; // break off this buffer while
            }
            while ((*p != LF) && (buffer+length > p))
                ++p;
            if (!m_bFirstLineParsed)
            {
                char server_version[8]; // HTTP/1.1 
                PRUint32 stat = 0;
                char stat_str[kMAX_FIRST_LINE_SIZE];
                *p = '\0';
                sscanf(lineStart, "%8s %d %s", server_version, &stat, stat_str);
                m_pResponse->SetServerVersion(server_version);
                m_pResponse->SetStatus(stat);
                m_pResponse->SetStatusString(stat_str);
                p++;
                m_bFirstLineParsed = PR_TRUE;
            }
            else
            {
                char* header = lineStart;
                char* value = PL_strchr(lineStart, ':');
                *p = '\0';
                if(value)
                {
                    *value = '\0';
                    value++;
                    if (partHeaderLen == 0)
                        m_pResponse->SetHeaderInternal(header, value);
                    else
                    {
                        //append the header to the partheader
                        header = PL_strcat(partHeader, header);
                        m_pResponse->SetHeaderInternal(header, value);
                        //Reset partHeader now
                        partHeader[0]='\0';
                        partHeaderLen = 0;
                    }

                }
                else // this is just a part of the header so save it for later use...
                {
                    partHeaderLen = p-header;
                    PL_strncpy(partHeader, lineStart, partHeaderLen);
                }
                p++;
            }
        
        }
    }

    if (m_bHeadersDone)
    {
        nsresult rv;

        // Check for any modules that want to receive headers once they've arrived.
        NS_WITH_SERVICE(nsINetModuleMgr, pNetModuleMgr, kNetModuleMgrCID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsISimpleEnumerator* pModules = nsnull;
        rv = pNetModuleMgr->EnumerateModules("http-response", &pModules);
        NS_RELEASE(pNetModuleMgr);
        if (NS_FAILED(rv)) return rv;

        nsIProxyObjectManager*  proxyObjectManager = nsnull; 
        rv = nsServiceManager::GetService( NS_XPCOMPROXY_PROGID, 
                                            kProxyObjectManagerIID,
                                            (nsISupports **)&proxyObjectManager);
        if (NS_FAILED(rv)) {
            NS_RELEASE(pModules);
            return rv;
        }

        nsISupports *supEntry = nsnull;

        // Go through the external modules and notify each one.
        rv = pModules->GetNext(&supEntry);
        while (NS_SUCCEEDED(rv)) {
            nsINetModRegEntry *entry = nsnull;
            rv = supEntry->QueryInterface(nsINetModRegEntry::GetIID(), (void**)&entry);
            NS_RELEASE(supEntry);
            if (NS_FAILED(rv)) {
                NS_RELEASE(pModules);
                NS_RELEASE(proxyObjectManager);
                return rv;
            }

            nsCID *lCID;
            nsIEventQueue* lEventQ = nsnull;

            rv = entry->GetMCID(&lCID);
            if (NS_FAILED(rv)) {
                NS_RELEASE(pModules);
                NS_RELEASE(proxyObjectManager);
                return rv;
            }

            rv = entry->GetMEventQ(&lEventQ);
            if (NS_FAILED(rv)) {
                NS_RELEASE(pModules);
                NS_RELEASE(proxyObjectManager);            
                return rv;
            }

            nsIHTTPNotify *pNotify = nsnull;
            // if this call fails one of the following happened.
            // a) someone registered an object for this topic but didn't
            //    implement the nsIHTTPNotify interface on that object.
            // b) someone registered an object for this topic bud didn't
            //    put the .xpt lib for that object in the components dir
            rv = proxyObjectManager->GetProxyObject(lEventQ, 
                                               *lCID,
                                               nsnull,
                                               nsIHTTPNotify::GetIID(),
                                               PROXY_ASYNC,
                                               (void**)&pNotify);
            NS_RELEASE(proxyObjectManager);
        
            NS_RELEASE(lEventQ);

            if (NS_SUCCEEDED(rv)) {
                // send off the notification, and block.
                nsIHTTPNotify* externMod = nsnull;
                rv = pNotify->QueryInterface(nsIHTTPNotify::GetIID(), (void**)&externMod);
                NS_RELEASE(pNotify);
            
                if (NS_FAILED(rv)) {
                    NS_ASSERTION(0, "proxy object manager found an interface we can not QI for");
                    NS_RELEASE(pModules);
                    return rv;
                }

                // make the nsIHTTPNotify api call
                externMod->AsyncExamineResponse(m_pConnection);
                NS_RELEASE(externMod);
                // we could do something with the return code from the external
                // module, but what????            
            }

            NS_RELEASE(entry);
            rv = pModules->GetNext(&supEntry); // go around again
        }
        NS_RELEASE(pModules);
        NS_IF_RELEASE(proxyObjectManager);

        nsIStreamListener* internalListener = nsnull;
        // Get our end of the pipe between us and the user's GetInputStream()
        rv = m_pConnection->GetResponseDataListener(&internalListener);
        if (internalListener) {
            // post the data to the stream listener
            // XXX this is the wrong data and offsets I think.
            rv = internalListener->OnDataAvailable(context, i_pStream, i_SourceOffset, i_Length);
            NS_RELEASE(internalListener);
            if (NS_FAILED(rv))
                return rv;
        }

        // do whatever we want for the event sink
        return pSink->OnDataAvailable(context, /* inStr */i_pStream, i_SourceOffset, i_Length);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsHTTPResponseListener::OnStartBinding(nsISupports* i_pContext)
{
    nsresult rv;
    nsIHTTPEventSink* pSink= nsnull;

    //TODO globally replace printf with trace calls. 
    //printf("nsHTTPResponseListener::OnStartBinding...\n");

    // Initialize header varaibles...  
    m_bHeadersDone     = PR_FALSE;
    m_bFirstLineParsed = PR_FALSE;

    if (i_pContext) {
        rv = i_pContext->QueryInterface(nsIHTTPChannel::GetIID(), 
                                        (void**)&m_pConnection);
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    if (NS_SUCCEEDED(rv)) {
        // XXX:  EventSink(...) should AddRef the returned pointer...
        rv = m_pConnection->GetEventSink(&pSink);

        if (NS_FAILED(rv)) {
            NS_ERROR("No HTTP Event Sink!");
        }
    }

    if (NS_SUCCEEDED(rv)) {
        rv = pSink->OnStartBinding(i_pContext);
    }

    return rv;
}

NS_IMETHODIMP
nsHTTPResponseListener::OnStopBinding(nsISupports* i_pContext,
                                 nsresult i_Status,
                                 nsIString* i_pMsg)
{
    //printf("nsHTTPResponseListener::OnStopBinding...\n");
    //NS_ASSERTION(m_pResponse, "Response object not created yet or died?!");
    // Should I save this as a member variable? yes... todo
    nsIHTTPEventSink* pSink= nsnull;
    nsresult rv = m_pConnection->GetEventSink(&pSink);
    if (NS_FAILED(rv))
        NS_ERROR("No HTTP Event Sink!");
    
    rv = pSink->OnStopBinding(i_pContext, i_Status,i_pMsg);

    return rv;
}

NS_IMETHODIMP
nsHTTPResponseListener::OnStartRequest(nsISupports* i_pContext)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTTPResponseListener::OnStopRequest(nsISupports* i_pContext,
                                      nsresult iStatus,
                                      nsIString* i_pMsg)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

