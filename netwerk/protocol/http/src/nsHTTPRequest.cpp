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

#include "nspr.h"
#include "nsIURL.h"
#include "nsHTTPRequest.h"
#include "nsHTTPAtoms.h"
#include "nsHTTPEnums.h"
#include "nsIBuffer.h"
#include "nsIBufferInputStream.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsHTTPChannel.h"
#include "nsHTTPResponseListener.h"
#include "nsCRT.h"
#include "nsXPIDLString.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gHTTPLog;
#endif /* PR_LOGGING */

nsHTTPRequest::nsHTTPRequest(nsIURI* i_pURL, HTTPMethod i_Method, 
	nsIChannel* i_pTransport):
    m_Method(i_Method),
    m_Version(HTTP_ONE_ZERO),
    m_Request(nsnull)
{
    NS_INIT_REFCNT();

    m_pURI = do_QueryInterface(i_pURL);

    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("Creating nsHTTPRequest [this=%x].\n", this));

    m_pTransport = i_pTransport;
    NS_IF_ADDREF(m_pTransport);

	// Send Host header by default
	if (HTTP_ZERO_NINE != m_Version)
	{
		nsXPIDLCString host;
		NS_ASSERTION(m_pURI, "No URI for the request!!");
		m_pURI->GetHost(getter_Copies(host));
        SetHeader(nsHTTPAtoms::Host, host);
	}
}

nsHTTPRequest::~nsHTTPRequest()
{
    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("Deleting nsHTTPRequest [this=%x].\n", this));

    NS_IF_RELEASE(m_Request);
    NS_IF_RELEASE(m_pTransport);
/*
    if (m_pConnection)
        NS_RELEASE(m_pConnection);
*/
}

NS_IMPL_ISUPPORTS(nsHTTPRequest, nsCOMTypeInfo<nsIStreamObserver>::GetIID())


// Finally our own methods...
nsresult
nsHTTPRequest::Build()
{
    nsresult rv;

    if (m_Request) {
        NS_ERROR("Request already built!");
        return NS_ERROR_FAILURE;
    }

    if (!m_pURI) {
        NS_ERROR("No URL to build request for!");
        return NS_ERROR_NULL_POINTER;
    }

    // Create the Input Stream for writing the request...
    nsCOMPtr<nsIBuffer> buf;
    rv = NS_NewBuffer(getter_AddRefs(buf), NS_HTTP_REQUEST_SEGMENT_SIZE,
                      NS_HTTP_REQUEST_BUFFER_SIZE, nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewBufferInputStream(&m_Request, buf);
    if (NS_FAILED(rv)) return rv;

    //
    // Write the request into the stream...
    //
    nsXPIDLCString autoBuffer;
    nsString lineBuffer(eOneByte);
    PRUint32 bytesWritten = 0;

    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("nsHTTPRequest::Build() [this=%x].  Building Request string.",
            this));

    // Write the request method and HTTP version.
    lineBuffer.Append(MethodToString(m_Method));

    rv = m_pURI->GetPath(getter_Copies(autoBuffer));
    lineBuffer.Append(autoBuffer);
    
    //Trim off the # portion if any...
    int refLocation = lineBuffer.RFind("#");
    if (-1 != refLocation)
        lineBuffer.Truncate(refLocation);

    lineBuffer.Append(" HTTP/1.0"CRLF);
    
    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("\tnsHTTPRequest.\tFirst line: %s", lineBuffer.GetBuffer()));

    rv = buf->Write(lineBuffer.GetBuffer(), lineBuffer.Length(), 
                         &bytesWritten);
#ifdef DEBUG_gagan    
    printf(lineBuffer.GetBuffer());
#endif
    if (NS_FAILED(rv)) return rv;
    
/*    switch (m_Method)
    {
        case HM_GET:
            PL_strncpy(lineBuffer, MethodToString(m_Method)
            break;
        case HM_DELETE:
        case HM_HEAD:
        case HM_INDEX:
        case HM_LINK:
        case HM_OPTIONS:
        case HM_POST:
        case HM_PUT:
        case HM_PATCH:
        case HM_TRACE:
        case HM_UNLINK:
            NS_ERROR_NOT_IMPLEMENTED;
            break;
        default: NS_ERROR("No method set on request!");
            break;
    }
*/

    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("\tnsHTTPRequest.\tRequest Headers:\n"));

    // Write the request headers, if any...
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = mHeaders.GetEnumerator(getter_AddRefs(enumerator));

    if (NS_SUCCEEDED(rv)) {
        PRBool bMoreHeaders;
        nsCOMPtr<nsISupports>   item;
        nsCOMPtr<nsIHTTPHeader> header;
        nsCOMPtr<nsIAtom>       headerAtom;

        enumerator->HasMoreElements(&bMoreHeaders);
        while (bMoreHeaders) {
            enumerator->GetNext(getter_AddRefs(item));
            header = do_QueryInterface(item);

            NS_ASSERTION(header, "Bad HTTP header.");
            if (header) {
                header->GetField(getter_AddRefs(headerAtom));
                header->GetValue(getter_Copies(autoBuffer));

                headerAtom->ToString(lineBuffer);
                lineBuffer.Append(": ");
                lineBuffer.Append(autoBuffer);
                lineBuffer.Append(CRLF);

                PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
                       ("\tnsHTTPRequest [this=%x].\t\t%s\n",
                        this, lineBuffer.GetBuffer()));

                buf->Write(lineBuffer.GetBuffer(), lineBuffer.Length(), 
                           &bytesWritten);
            }
            enumerator->HasMoreElements(&bMoreHeaders);
        }
    }

	nsCOMPtr<nsIInputStream> stream;
	NS_ASSERTION(m_pConnection, "Hee ha!");
	if (NS_FAILED(m_pConnection->GetPostDataStream(getter_AddRefs(stream))))
			return NS_ERROR_FAILURE;

    // Currently nsIPostStreamData contains the header info and the data.
    // So we are forced to putting this here in the end. 
    // This needs to change! since its possible for someone to modify it
    // TODO- Gagan

    if (stream)
	{
		NS_ASSERTION(m_Method == HM_POST, "Post data without a POST method?");

		PRUint32 length;
		stream->GetLength(&length);

		// TODO Change reading from nsIInputStream to nsIBuffer
		char* tempBuff = new char[length+1];
		if (!tempBuff)
			return NS_ERROR_OUT_OF_MEMORY;
		if (NS_FAILED(stream->Read(tempBuff, length, &length)))
        {
            NS_ASSERTION(0, "Failed to read post data!");
            return NS_ERROR_FAILURE;
        }
        else
		{
            tempBuff[length] = '\0';
            PRUint32 writtenLength;
			buf->Write(tempBuff, length, &writtenLength);
#ifdef DEBUG_gagan    
    printf(tempBuff);
#endif
			// ASSERT that you wrote length = stream's length
            NS_ASSERTION(writtenLength == length, "Failed to write post data!");
		}
		delete[] tempBuff;
	}
    else 
    {

        // Write the final \r\n
        rv = buf->Write(CRLF, PL_strlen(CRLF), &bytesWritten);
#ifdef DEBUG_gagan    
    printf(CRLF);
#endif
        if (NS_FAILED(rv)) return rv;
    }

    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("nsHTTPRequest::Build() [this=%x].\tFinished building request.\n",
            this));

    return rv;
}

NS_METHOD 
nsHTTPRequest::Clone(const nsHTTPRequest* *o_Request) const
{
	return NS_ERROR_FAILURE;
}
                        
NS_METHOD 
nsHTTPRequest::SetMethod(HTTPMethod i_Method)
{
    m_Method = i_Method;
	return NS_OK;
}

HTTPMethod 
nsHTTPRequest::GetMethod(void) const
{
    return m_Method;
}
                        
NS_METHOD 
nsHTTPRequest::SetPriority()
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD 
nsHTTPRequest::GetPriority()
{
	return NS_ERROR_NOT_IMPLEMENTED;
}


NS_METHOD
nsHTTPRequest::SetHeader(nsIAtom* i_Header, const char* i_Value)
{
    return mHeaders.SetHeader(i_Header, i_Value);
}

NS_METHOD
nsHTTPRequest::GetHeader(nsIAtom* i_Header, char* *o_Value)
{
    return mHeaders.GetHeader(i_Header, o_Value);
}


NS_METHOD
nsHTTPRequest::SetHTTPVersion(HTTPVersion i_Version)
{
    m_Version = i_Version;
    return NS_OK;
}

NS_METHOD
nsHTTPRequest::GetHTTPVersion(HTTPVersion* o_Version) 
{
    *o_Version = m_Version;
    return NS_OK;
}

NS_METHOD
nsHTTPRequest::GetInputStream(nsIInputStream* *o_Stream)
{
    if (o_Stream)
    {
        if (!m_Request)
        {
            Build();
        }
        m_Request->QueryInterface(nsCOMTypeInfo<nsIInputStream>::GetIID(), (void**)o_Stream);
        return NS_OK;
    }
    else
        return NS_ERROR_NULL_POINTER;

}

NS_IMETHODIMP
nsHTTPRequest::OnStartRequest(nsIChannel* channel, nsISupports* i_pContext)
{
    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("nsHTTPRequest [this=%x]. Starting to write request to server.\n",
            this));
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPRequest::OnStopRequest(nsIChannel* channel, nsISupports* i_pContext,
                             nsresult iStatus,
                             const PRUnichar* i_pMsg)
{
    nsresult rv = iStatus;
    
    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("nsHTTPRequest [this=%x]. Finished writing request to server."
            "\tStatus: %x\n", 
            this, iStatus));

    if (NS_SUCCEEDED(rv)) {
        nsHTTPResponseListener* pListener;
        
        //Prepare to receive the response!
        pListener = new nsHTTPResponseListener(m_pConnection);
        if (pListener) {
            NS_ADDREF(pListener);
            rv = m_pTransport->AsyncRead(0, -1,
                                         i_pContext, 
                                         pListener);
            NS_RELEASE(pListener);
        } else {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    }
    //
    // An error occurred when trying to write the request to the server!
    //
    // Call the consumer OnStopRequest(...) to end the request...
    //
    else {
        nsCOMPtr<nsIStreamListener> consumer;
        nsCOMPtr<nsISupports> consumerContext;

        PR_LOG(gHTTPLog, PR_LOG_ERROR, 
               ("nsHTTPRequest [this=%x]. Error writing request to server."
                "\tStatus: %x\n", 
                this, iStatus));

        (void) m_pConnection->GetResponseContext(getter_AddRefs(consumerContext));
        rv = m_pConnection->GetResponseDataListener(getter_AddRefs(consumer));
        if (consumer) {
            consumer->OnStopRequest(m_pConnection, consumerContext, iStatus, i_pMsg);
        }

        rv = iStatus;
    }
 
    return rv;
}

NS_IMETHODIMP
nsHTTPRequest::SetTransport(nsIChannel* i_pTransport)
{
    NS_ASSERTION(!m_pTransport, "Transport being overwritten!");
    m_pTransport = i_pTransport;
    NS_ADDREF(m_pTransport);
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPRequest::SetConnection(nsHTTPChannel* i_pConnection)
{
    m_pConnection = i_pConnection;
    return NS_OK;
}


nsresult nsHTTPRequest::GetHeaderEnumerator(nsISimpleEnumerator** aResult)
{
    return mHeaders.GetEnumerator(aResult);
}