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
#include "nsHTTPResponseListener.h"
#include "nsITransport.h"
#include "nsIInputStream.h"
#include "nsIHTTPConnection.h"
#include "nsHTTPResponse.h"
#include "nsIHTTPEventSink.h"
#include "nsCRT.h"
#include "stdio.h" //sscanf

static const int kMAX_FIRST_LINE_SIZE= 256;

nsHTTPResponseListener::nsHTTPResponseListener(): 
    m_pConnection(nsnull),
    m_bFirstLineParsed(PR_FALSE),
    m_bHeadersDone(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsHTTPResponseListener::~nsHTTPResponseListener()
{
    if (m_pConnection)
        NS_RELEASE(m_pConnection);
}

NS_IMPL_ISUPPORTS(nsHTTPResponseListener,nsIStreamListener::GetIID());

NS_IMETHODIMP
nsHTTPResponseListener::OnDataAvailable(nsISupports* context,
                            nsIInputStream *i_pStream, 
                            PRUint32 i_SourceOffset,
                            PRUint32 i_Length)
{
    //move these to member variables later TODO
    static char extrabuffer[kMAX_FIRST_LINE_SIZE];
    static int extrabufferlen = 0;

    NS_ASSERTION(i_pStream, "Fake stream!");
    // Set up the response
    if (!m_pResponse)
    {
        m_pResponse = new nsHTTPResponse (m_pConnection, i_pStream);
        if (!m_pResponse)
        {
            NS_ERROR("Failed to create the response object!");
            return NS_ERROR_OUT_OF_MEMORY;
        }
        NS_ADDREF(m_pResponse);
    }
 
    //printf("nsHTTPResponseListener::OnDataAvailable...\n");
    /* 
        Check its current state, 
        if we have already found the end of headers mark,
        then just stream this on to the listener
        else read
    */
    if (m_bHeadersDone)
    {
        // TODO push extrabuffer up the stream too.. How?

        // Should I save this as a member variable? yes... todo
        nsIHTTPEventSink* pSink= nsnull;
        NS_VERIFY(m_pConnection->EventSink(&pSink), "No HTTP Event Sink!");
        return pSink->OnDataAvailable(context, i_pStream, i_SourceOffset, i_Length);
    }
    else if (!m_bFirstLineParsed)
    {
        //TODO optimize this further!
        char server_version[8]; // HTTP/1.1 
        char buffer[kMAX_FIRST_LINE_SIZE];  
        PRUint32 length;

        nsresult stat = i_pStream->Read(buffer, kMAX_FIRST_LINE_SIZE, &length);
        NS_ASSERTION(buffer, "Argh...");

        char* p = buffer;
        while ((*p != LF) && buffer+length > p)
            ++p;

        if (p != (buffer+length))
        {
            PRUint32 stat = 0;
            char stat_str[kMAX_FIRST_LINE_SIZE];
            *p = '\0';
            sscanf(buffer, "%8s %d %s", server_version, &stat, stat_str);
            m_pResponse->SetServerVersion(server_version);
            m_pResponse->SetStatus(stat);
            m_pResponse->SetStatusString(stat_str);
            p++;
        }
        
        // we read extra so save it for the other headers
        if (buffer+length > p)
        {
            extrabufferlen = length - (buffer - p);
            PL_strncpy(extrabuffer, p, extrabufferlen);
        }
        m_bFirstLineParsed = PR_TRUE;
        return NS_OK;
    }
    else
    {
        if (extrabufferlen > 0)
        {
            // TODO - revive!
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPResponseListener::OnStartBinding(nsISupports* i_pContext)
{
    //TODO globally replace printf with trace calls. 
    //printf("nsHTTPResponseListener::OnStartBinding...\n");
    m_pConnection = NS_STATIC_CAST(nsIHTTPConnection*, i_pContext);
    NS_ADDREF(m_pConnection);

    nsIHTTPEventSink* pSink= nsnull;
    nsresult rv = m_pConnection->EventSink(&pSink);
    if (NS_FAILED(rv))
        NS_ERROR("No HTTP Event Sink!");
    rv = pSink->OnStartBinding(i_pContext);
   
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPResponseListener::OnStopBinding(nsISupports* i_pContext,
                                 nsresult i_Status,
                                 nsIString* i_pMsg)
{
    //printf("nsHTTPResponseListener::OnStopBinding...\n");
    NS_ASSERTION(m_pResponse, "Response object vanished!");
    // Should I save this as a member variable? yes... todo
    nsIHTTPEventSink* pSink= nsnull;
    NS_VERIFY(m_pConnection->EventSink(&pSink), "No HTTP Event Sink!");
    nsresult rv = pSink->OnStopBinding(i_pContext, i_Status,i_pMsg);
    NS_RELEASE(m_pResponse);

    return rv;
}
