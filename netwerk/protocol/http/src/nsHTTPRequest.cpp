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
#include "nsVoidArray.h"
#include "nsHeaderPair.h"
#include "kHTTPHeaders.h"
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
    m_pArray(new nsVoidArray()),
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
		SetHost(host);
	}
}

nsHTTPRequest::~nsHTTPRequest()
{
    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("Deleting nsHTTPRequest [this=%x].\n", this));

    if (m_pArray) {
        PRInt32 cnt = m_pArray->Count();
        for (PRInt32 i = cnt-1; i >= 0; i--) {
            nsHeaderPair* element = NS_STATIC_CAST(nsHeaderPair*, m_pArray->ElementAt(i));
            m_pArray->RemoveElementAt(i);

            delete element;
        }

        delete m_pArray;
        m_pArray = nsnull;
    }
    
    NS_IF_RELEASE(m_Request);
    NS_IF_RELEASE(m_pTransport);
/*
    if (m_pConnection)
        NS_RELEASE(m_pConnection);
*/
}

NS_IMPL_ADDREF(nsHTTPRequest);

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
    nsString lineBuffer(eOneByte);
    PRUint32 bytesWritten = 0;

    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("nsHTTPRequest::Build() [this=%x].  Building Request string.",
            this));

    // Write the request method and HTTP version.
    char* name;
    lineBuffer.Append(MethodToString(m_Method));

    rv = m_pURI->GetPath(&name);
    lineBuffer.Append(name);
    nsCRT::free(name);
    name = nsnull;
    
    //Trim off the # portion if any...
    int refLocation = lineBuffer.RFind("#");
    if (-1 != refLocation)
        lineBuffer.Truncate(refLocation);

    lineBuffer.Append(" HTTP/1.0"CRLF);
    
    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("\tnsHTTPRequest [this=%x].\tFirst line: %s",
            this, lineBuffer.GetBuffer()));

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
           ("\tnsHTTPRequest [this=%x].\tRequest Headers:\n", this));
    
    // Write the request headers, if any...
    NS_ASSERTION(m_pArray, "header array is null");

    for (PRInt32 i = m_pArray->Count() - 1; i >= 0; --i) 
    {
        nsHeaderPair* element = NS_STATIC_CAST(nsHeaderPair*, m_pArray->ElementAt(i));

        element->atom->ToString(lineBuffer);
        lineBuffer.Append(": ");
        lineBuffer.Append((const nsString&)*element->value);
        lineBuffer.Append(CRLF);

        PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
               ("\tnsHTTPRequest [this=%x].\t\t%s\n",
                this, lineBuffer.GetBuffer()));

        rv = buf->Write(lineBuffer.GetBuffer(), lineBuffer.Length(),
                             &bytesWritten);
#ifdef DEBUG_gagan    
    printf(lineBuffer.GetBuffer());
#endif
        if (NS_FAILED(rv)) return rv;

        lineBuffer.Truncate();
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

nsresult
nsHTTPRequest::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    if (aIID.Equals(nsCOMTypeInfo<nsIHTTPRequest>::GetIID())) {
        *aInstancePtr = (void*) ((nsIHTTPRequest*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsCOMTypeInfo<nsIStreamObserver>::GetIID())) {
        *aInstancePtr = (void*) ((nsIStreamObserver*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsCOMTypeInfo<nsIHTTPCommonHeaders>::GetIID())) {
        *aInstancePtr = (void*) ((nsIHTTPCommonHeaders*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsCOMTypeInfo<nsIHeader>::GetIID())) {
        *aInstancePtr = (void*) ((nsIHeader*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}
 
NS_IMPL_RELEASE(nsHTTPRequest);

//TODO make these inlines...
NS_METHOD 
nsHTTPRequest::SetAllow(const char* i_Value)
{
    return SetHeader(kHH_ALLOW, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetAllow(char* *o_Value) 
{
    return GetHeader(kHH_ALLOW, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentBase(const char* i_Value)
{
    return SetHeader(kHH_CONTENT_BASE, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentBase(char* *o_Value) 
{
	return GetHeader(kHH_CONTENT_BASE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentEncoding(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_ENCODING, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentEncoding(char* *o_Value) 
{
	return GetHeader(kHH_CONTENT_ENCODING, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentLanguage(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_LANGUAGE, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentLanguage(char* *o_Value) 
{
	return GetHeader(kHH_CONTENT_LANGUAGE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentLength(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_LENGTH, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentLength(char* *o_Value) 
{
	return GetHeader(kHH_CONTENT_LENGTH, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentLocation(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_LOCATION, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentLocation(char* *o_Value) 
{
	return GetHeader(kHH_CONTENT_LOCATION, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentMD5(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_MD5, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentMD5(char* *o_Value) 
{
	return GetHeader(kHH_CONTENT_MD5, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentRange(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_RANGE, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentRange(char* *o_Value) 
{
	return GetHeader(kHH_CONTENT_RANGE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentTransferEncoding(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_TRANSFER_ENCODING, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentTransferEncoding(char* *o_Value) 
{
	return GetHeader(kHH_CONTENT_TRANSFER_ENCODING, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentType(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_TYPE, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentType(char* *o_Value) 
{
	return GetHeader(kHH_CONTENT_TYPE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetDerivedFrom(const char* i_Value)
{
	return SetHeader(kHH_DERIVED_FROM, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetDerivedFrom(char* *o_Value) 
{
	return GetHeader(kHH_DERIVED_FROM, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetETag(const char* i_Value)
{
	return SetHeader(kHH_ETAG, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetETag(char* *o_Value) 
{
	return GetHeader(kHH_ETAG, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetExpires(const char* i_Value)
{
	return SetHeader(kHH_EXPIRES, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetExpires(char* *o_Value) 
{
	return GetHeader(kHH_EXPIRES, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetLastModified(const char* i_Value)
{
	return SetHeader(kHH_LAST_MODIFIED, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetLastModified(char* *o_Value) 
{
	return GetHeader(kHH_LAST_MODIFIED, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetLink(const char* i_Value)
{
	return SetHeader(kHH_LINK, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetLink(char* *o_Value) 
{
	return GetHeader(kHH_LINK, o_Value);
}

NS_METHOD 
nsHTTPRequest::GetLinkMultiple(
                            const char** *o_ValueArray, 
                            int count) const
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD 
nsHTTPRequest::SetTitle(const char* i_Value)
{
	return SetHeader(kHH_TITLE, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetTitle(char* *o_Value) 
{
	return GetHeader(kHH_TITLE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetURI(const char* i_Value)
{
	return SetHeader(kHH_URI, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetURI(char* *o_Value) 
{
	return GetHeader(kHH_URI, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetVersion(const char* i_Value)
{
	return SetHeader(kHH_VERSION, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetVersion(char* *o_Value) 
{
	return GetHeader(kHH_VERSION, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetConnection(const char* i_Value)
{
	return SetHeader(kHH_CONNECTION, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetConnection(char* *o_Value) 
{
	return GetHeader(kHH_CONNECTION, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetDate(const char* i_Value)
{
	return SetHeader(kHH_DATE, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetDate(char* *o_Value) 
{
	return GetHeader(kHH_DATE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetPragma(const char* i_Value)
{
	return SetHeader(kHH_PRAGMA,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetPragma(char* *o_Value) 
{
	return GetHeader(kHH_PRAGMA,o_Value);
}

NS_METHOD 
nsHTTPRequest::SetForwarded(const char* i_Value)
{
	return SetHeader(kHH_FORWARDED,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetForwarded(char* *o_Value) 
{
	return GetHeader(kHH_FORWARDED,o_Value);
}

NS_METHOD 
nsHTTPRequest::SetMessageID(const char* i_Value)
{
	return SetHeader(kHH_MESSAGE_ID,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetMessageID(char* *o_Value) 
{
	return GetHeader(kHH_MESSAGE_ID,o_Value);
}

NS_METHOD 
nsHTTPRequest::SetMIME(const char* i_Value)
{
	return SetHeader(kHH_MIME,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetMIME(char* *o_Value) 
{
	return GetHeader(kHH_MIME,o_Value);
}

NS_METHOD 
nsHTTPRequest::SetTrailer(const char* i_Value)
{
	return SetHeader(kHH_TRAILER,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetTrailer(char* *o_Value)
{
	return GetHeader(kHH_TRAILER,o_Value);
}

NS_METHOD 
nsHTTPRequest::SetTransfer(const char* i_Value)
{
	return SetHeader(kHH_TRANSFER,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetTransfer(char* *o_Value)
{
	return GetHeader(kHH_TRANSFER,o_Value);
}

    // Methods from nsIHTTPRequest
NS_METHOD 
nsHTTPRequest::SetAccept(const char* i_Value)
{
	return SetHeader(kHH_ACCEPT,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetAccept(char* *o_Value)
{
	return GetHeader(kHH_ACCEPT,o_Value);
}
                    
NS_METHOD 
nsHTTPRequest::SetAcceptChar(const char* i_Value)
{
	return SetHeader(kHH_ACCEPT_CHAR,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetAcceptChar(char* *o_Value)
{
	return GetHeader(kHH_ACCEPT_CHAR,o_Value);
}
                    
NS_METHOD 
nsHTTPRequest::SetAcceptEncoding(const char* i_Value)
{
	return SetHeader(kHH_ACCEPT_ENCODING,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetAcceptEncoding(char* *o_Value)
{
	return GetHeader(kHH_ACCEPT_ENCODING,o_Value);
}
                    
NS_METHOD 
nsHTTPRequest::SetAcceptLanguage(const char* i_Value)
{
	return SetHeader(kHH_ACCEPT_LANGUAGE,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetAcceptLanguage(char* *o_Value)
{
	return GetHeader(kHH_ACCEPT_LANGUAGE,o_Value);
}
                    
NS_METHOD 
nsHTTPRequest::SetAuthentication(const char* i_Value)
{
	return SetHeader(kHH_AUTHENTICATION,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetAuthentication(char* *o_Value)
{
	return GetHeader(kHH_AUTHENTICATION,o_Value);
}
                    
NS_METHOD 
nsHTTPRequest::SetExpect(const char* i_Value)
{
	return SetHeader(kHH_EXPECT,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetExpect(char** o_Value) 
{
	return GetHeader(kHH_EXPECT, o_Value);
}
                    
NS_METHOD 
nsHTTPRequest::SetFrom(const char* i_Value)
{
	return SetHeader(kHH_FROM,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetFrom(char** o_Value) 
{
	return GetHeader(kHH_FROM,o_Value);
}

    /*
        This is the actual Host for connection. Not necessarily the
        host in the url (as in the cases of proxy connection)
    */
NS_METHOD 
nsHTTPRequest::SetHost(const char* i_Value)
{
	return SetHeader(kHH_HOST,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetHost(char** o_Value) 
{
	return GetHeader(kHH_HOST, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetIfModifiedSince(const char* i_Value)
{
	return SetHeader(kHH_IF_MODIFIED_SINCE,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfModifiedSince(char* *o_Value)
{
	return GetHeader(kHH_IF_MODIFIED_SINCE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetIfMatch(const char* i_Value)
{
	return SetHeader(kHH_IF_MATCH,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfMatch(char* *o_Value)
{
	return GetHeader(kHH_IF_MATCH, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetIfMatchAny(const char* i_Value)
{
	return SetHeader(kHH_IF_MATCH_ANY,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfMatchAny(char* *o_Value)
{
	return GetHeader(kHH_IF_MATCH_ANY,o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetIfNoneMatch(const char* i_Value)
{
	return SetHeader(kHH_IF_NONE_MATCH,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfNoneMatch(char* *o_Value)
{
	return GetHeader(kHH_IF_NONE_MATCH,o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetIfNoneMatchAny(const char* i_Value)
{
	return SetHeader(kHH_IF_NONE_MATCH_ANY,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfNoneMatchAny(char* *o_Value)
{
	return GetHeader(kHH_IF_NONE_MATCH_ANY,o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetIfRange(const char* i_Value)
{
	return SetHeader(kHH_IF_RANGE,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfRange(char* *o_Value)
{
	return GetHeader(kHH_IF_RANGE,o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetIfUnmodifiedSince(const char* i_Value)
{
	return SetHeader(kHH_IF_UNMODIFIED_SINCE,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfUnmodifiedSince(char* *o_Value)
{
	return GetHeader(kHH_IF_UNMODIFIED_SINCE, o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetMaxForwards(const char* i_Value)
{
	return SetHeader(kHH_MAX_FORWARDS,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetMaxForwards(char* *o_Value)
{
	return GetHeader(kHH_MAX_FORWARDS,o_Value);
}

/* 
    Range information for byte-range requests 
    may have an overloaded one. TODO later
*/
NS_METHOD 
nsHTTPRequest::SetRange(const char* i_Value)
{
	return SetHeader(kHH_RANGE,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetRange(char* *o_Value)
{
	return GetHeader(kHH_RANGE, o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetReferer(const char* i_Value)
{
	return SetHeader(kHH_REFERER, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetReferer(char* *o_Value)
{
	return GetHeader(kHH_REFERER, o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetUserAgent(const char* i_Value)
{
	return SetHeader(kHH_USER_AGENT, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetUserAgent(char* *o_Value)
{
	return GetHeader(kHH_USER_AGENT, o_Value);
}

    // Finally our own methods...
NS_METHOD 
nsHTTPRequest::Clone(const nsHTTPRequest* *o_Request) const
{
	return NS_OK;
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
nsHTTPRequest::SetHeader(const char* i_Header, const char* i_Value)
{
    NS_ASSERTION(m_pArray, "header array doesn't exist.");
    if (i_Value)
    {
        //The tempValue gets copied so we can do away with it...
        nsString tempValue(i_Value);
        nsHeaderPair* pair = new nsHeaderPair(i_Header, &tempValue);
        if (pair)
        {
            //TODO set uniqueness? how... 
            return m_pArray->AppendElement(pair);
        }
        else
            return NS_ERROR_OUT_OF_MEMORY;
    }
    else if (i_Header)
    {
        nsIAtom* header = NS_NewAtom(i_Header);
        if (!header)
            return NS_ERROR_OUT_OF_MEMORY;

        PRInt32 cnt = m_pArray->Count();
        for (PRInt32 i = 0; i < cnt; i++) {
            nsHeaderPair* element = NS_STATIC_CAST(nsHeaderPair*, m_pArray->ElementAt(i));
            if (header == element->atom) {
                m_pArray->RemoveElementAt(i);
                cnt = m_pArray->Count();
                i = -1; // reset the counter so we can start from the top again
            }
        }
        return NS_OK;
    }
    return NS_ERROR_NULL_POINTER;
}

NS_METHOD
nsHTTPRequest::GetHeader(const char* i_Header, char* *o_Value)
{
    NS_ASSERTION(m_pArray, "header array doesn't exist.");

    if (!i_Header || !o_Value)
        return NS_ERROR_NULL_POINTER;

    nsIAtom* header = NS_NewAtom(i_Header);
    if (!header)
        return NS_ERROR_OUT_OF_MEMORY;

    for (PRInt32 i = m_pArray->Count() - 1; i >= 0; --i) 
    {
        nsHeaderPair* element = NS_STATIC_CAST(nsHeaderPair*, m_pArray->ElementAt(i));
        if ((header == element->atom))
        {
            *o_Value = (element->value) ? element->value->ToNewCString() : nsnull;
            return NS_OK;
        }
    }

    *o_Value = nsnull;
    return NS_ERROR_NOT_FOUND;
}

NS_METHOD
nsHTTPRequest::GetHeaderMultiple(const char* i_Header, 
                                 char** *o_ValueArray,
                                 int *o_Count)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
        pListener = new nsHTTPResponseListener();
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
