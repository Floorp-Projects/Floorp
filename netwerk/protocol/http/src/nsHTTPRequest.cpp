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

#include "nsIUrl.h"
#include "nsHTTPRequest.h"
#include "nsVoidArray.h"
#include "nsHeaderPair.h"
#include "kHTTPHeaders.h"
#include "nsHTTPEnums.h"
#include "nsIByteBufferInputStream.h"
#include "plstr.h"
#include "nsString.h"
#include "nsITransport.h"
#include "nsHTTPConnection.h"
#include "nsHTTPResponseListener.h"
#include "nsCRT.h"

nsHTTPRequest::nsHTTPRequest(nsIUrl* i_pURL, HTTPMethod i_Method, nsITransport* i_pTransport):
    m_pURI(i_pURL),
    m_Method(i_Method),
    m_pArray(new nsVoidArray()),
    m_Version(HTTP_ONE_ZERO),
    m_Request(nsnull),
    m_pTransport(i_pTransport),
    mRefCnt(0)
{
    
    //Build();
}

nsHTTPRequest::~nsHTTPRequest()
{
    if (m_Request)
    {
        delete[] m_Request;
        m_Request = 0;
    }
    if (m_pArray)
    {
        delete m_pArray;
        m_pArray = 0;
    }
    if (m_Request)
    {
        delete m_Request;
        m_Request = 0;
    }
    if (m_pTransport)
        NS_RELEASE(m_pTransport);
/*
    if (m_pConnection)
        NS_RELEASE(m_pConnection);
*/
}

NS_IMPL_ADDREF(nsHTTPRequest);

nsresult
nsHTTPRequest::Build()
{
    if (m_Request)
        NS_ERROR("Request already built!");
    nsresult rv = NS_NewByteBufferInputStream(&m_Request);
    if (m_Request)
    {

        char lineBuffer[1024]; // verify this length!
        PRUint32 bytesWritten = 0;

        // Do the first line 
        const char* methodString = MethodToString(m_Method);
        PL_strncpyz(lineBuffer, methodString, PL_strlen(methodString) +1);
        const char* filename;
        NS_ASSERTION(m_pURI, "No URL to build request for!");
        rv = m_pURI->GetPath(&filename);
        PL_strcat(lineBuffer, filename);
        PL_strcat(lineBuffer, " HTTP/1.0");
        PL_strcat(lineBuffer, CRLF);
        
        rv = m_Request->Fill(lineBuffer, PL_strlen(lineBuffer), &bytesWritten);
        if (NS_FAILED(rv)) return rv;
        
/*        switch (m_Method)
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

        // Write the request method and HTTP version
        
        // Add additional headers if any
        if (m_pArray && (0< m_pArray->Count()))
        {
            for (PRInt32 i = m_pArray->Count() - 1; i >= 0; --i) 
            {
                nsHeaderPair* element = NS_STATIC_CAST(nsHeaderPair*, m_pArray->ElementAt(i));
                //Copy header, put a ": " and then the value + LF
                // sprintf would be easier... todo change
                nsString lineBuffStr;
                element->atom->ToString(lineBuffStr);
                lineBuffStr.Append(": ");
                lineBuffStr.Append((const nsString&)*element->value);
                lineBuffStr.Append(CRLF);
                NS_ASSERTION((lineBuffStr.Length() <= 1024), "Increase line buffer length!");
                lineBuffStr.ToCString(lineBuffer, lineBuffStr.Length());
                lineBuffer[lineBuffStr.Length()] = '\0';
                rv = m_Request->Fill(lineBuffer, PL_strlen(lineBuffer), &bytesWritten);
                if (NS_FAILED(rv)) return rv;
                lineBuffer[0] = '\0';
            }
        }
        // Send the final \n
        lineBuffer[0] = CR;
        lineBuffer[1] = LF;
        lineBuffer[2] = '\0';
        rv = m_Request->Fill(lineBuffer, PL_strlen(lineBuffer), &bytesWritten);
        if (NS_FAILED(rv)) return rv;

    }

    return rv;
}

nsresult
nsHTTPRequest::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    if (aIID.Equals(nsIHTTPRequest::GetIID())) {
        *aInstancePtr = (void*) ((nsIHTTPRequest*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIStreamObserver::GetIID())) {
        *aInstancePtr = (void*) ((nsIStreamObserver*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIHTTPCommonHeaders::GetIID())) {
        *aInstancePtr = (void*) ((nsIHTTPCommonHeaders*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIHeader::GetIID())) {
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
nsHTTPRequest::GetAllow(const char* *o_Value) const
{
    return GetHeader(kHH_ALLOW, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentBase(const char* i_Value)
{
    return SetHeader(kHH_CONTENT_BASE, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentBase(const char* *o_Value) const
{
	return GetHeader(kHH_CONTENT_BASE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentEncoding(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_ENCODING, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentEncoding(const char* *o_Value) const
{
	return GetHeader(kHH_CONTENT_ENCODING, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentLanguage(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_LANGUAGE, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentLanguage(const char* *o_Value) const
{
	return GetHeader(kHH_CONTENT_LANGUAGE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentLength(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_LENGTH, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentLength(const char* *o_Value) const
{
	return GetHeader(kHH_CONTENT_LENGTH, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentLocation(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_LOCATION, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentLocation(const char* *o_Value) const
{
	return GetHeader(kHH_CONTENT_LOCATION, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentMD5(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_MD5, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentMD5(const char* *o_Value) const
{
	return GetHeader(kHH_CONTENT_MD5, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentRange(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_RANGE, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentRange(const char* *o_Value) const
{
	return GetHeader(kHH_CONTENT_RANGE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentTransferEncoding(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_TRANSFER_ENCODING, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentTransferEncoding(const char* *o_Value) const
{
	return GetHeader(kHH_CONTENT_TRANSFER_ENCODING, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetContentType(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_TYPE, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetContentType(const char* *o_Value) const
{
	return GetHeader(kHH_CONTENT_TYPE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetDerivedFrom(const char* i_Value)
{
	return SetHeader(kHH_DERIVED_FROM, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetDerivedFrom(const char* *o_Value) const
{
	return GetHeader(kHH_DERIVED_FROM, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetETag(const char* i_Value)
{
	return SetHeader(kHH_ETAG, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetETag(const char* *o_Value) const
{
	return GetHeader(kHH_ETAG, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetExpires(const char* i_Value)
{
	return SetHeader(kHH_EXPIRES, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetExpires(const char* *o_Value) const
{
	return GetHeader(kHH_EXPIRES, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetLastModified(const char* i_Value)
{
	return SetHeader(kHH_LAST_MODIFIED, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetLastModified(const char* *o_Value) const
{
	return GetHeader(kHH_LAST_MODIFIED, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetLink(const char* i_Value)
{
	return SetHeader(kHH_LINK, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetLink(const char* *o_Value) const
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
nsHTTPRequest::GetTitle(const char* *o_Value) const
{
	return GetHeader(kHH_TITLE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetURI(const char* i_Value)
{
	return SetHeader(kHH_URI, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetURI(const char* *o_Value) const
{
	return GetHeader(kHH_URI, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetVersion(const char* i_Value)
{
	return SetHeader(kHH_VERSION, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetVersion(const char* *o_Value) const
{
	return GetHeader(kHH_VERSION, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetConnection(const char* i_Value)
{
	return SetHeader(kHH_CONNECTION, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetConnection(const char* *o_Value) const
{
	return GetHeader(kHH_CONNECTION, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetDate(const char* i_Value)
{
	return SetHeader(kHH_DATE, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetDate(const char* *o_Value) const
{
	return GetHeader(kHH_DATE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetPragma(const char* i_Value)
{
	return SetHeader(kHH_PRAGMA,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetPragma(const char* *o_Value) const
{
	return GetHeader(kHH_PRAGMA,o_Value);
}

NS_METHOD 
nsHTTPRequest::SetForwarded(const char* i_Value)
{
	return SetHeader(kHH_FORWARDED,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetForwarded(const char* *o_Value) const
{
	return GetHeader(kHH_FORWARDED,o_Value);
}

NS_METHOD 
nsHTTPRequest::SetMessageID(const char* i_Value)
{
	return SetHeader(kHH_MESSAGE_ID,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetMessageID(const char* *o_Value) const
{
	return GetHeader(kHH_MESSAGE_ID,o_Value);
}

NS_METHOD 
nsHTTPRequest::SetMIME(const char* i_Value)
{
	return SetHeader(kHH_MIME,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetMIME(const char* *o_Value) const
{
	return GetHeader(kHH_MIME,o_Value);
}

NS_METHOD 
nsHTTPRequest::SetTrailer(const char* i_Value)
{
	return SetHeader(kHH_TRAILER,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetTrailer(const char* *o_Value) const
{
	return GetHeader(kHH_TRAILER,o_Value);
}

NS_METHOD 
nsHTTPRequest::SetTransfer(const char* i_Value)
{
	return SetHeader(kHH_TRANSFER,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetTransfer(const char* *o_Value) const
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
nsHTTPRequest::GetAccept(const char* *o_Value) const
{
	return GetHeader(kHH_ACCEPT,o_Value);
}
                    
NS_METHOD 
nsHTTPRequest::SetAcceptChar(const char* i_Value)
{
	return SetHeader(kHH_ACCEPT_CHAR,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetAcceptChar(const char* *o_Value) const
{
	return GetHeader(kHH_ACCEPT_CHAR,o_Value);
}
                    
NS_METHOD 
nsHTTPRequest::SetAcceptEncoding(const char* i_Value)
{
	return SetHeader(kHH_ACCEPT_ENCODING,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetAcceptEncoding(const char* *o_Value) const
{
	return GetHeader(kHH_ACCEPT_ENCODING,o_Value);
}
                    
NS_METHOD 
nsHTTPRequest::SetAcceptLanguage(const char* i_Value)
{
	return SetHeader(kHH_ACCEPT_LANGUAGE,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetAcceptLanguage(const char* *o_Value) const
{
	return GetHeader(kHH_ACCEPT_LANGUAGE,o_Value);
}
                    
NS_METHOD 
nsHTTPRequest::SetAuthentication(const char* i_Value)
{
	return SetHeader(kHH_AUTHENTICATION,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetAuthentication(const char* *o_Value) const
{
	return GetHeader(kHH_AUTHENTICATION,o_Value);
}
                    
NS_METHOD 
nsHTTPRequest::SetExpect(const char* i_Value)
{
	return SetHeader(kHH_EXPECT,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetExpect(const char** o_Value) const
{
	return GetHeader(kHH_EXPECT, o_Value);
}
                    
NS_METHOD 
nsHTTPRequest::SetFrom(const char* i_Value)
{
	return SetHeader(kHH_FROM,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetFrom(const char** o_Value) const
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
nsHTTPRequest::GetHost(const char** o_Value) const
{
	return GetHeader(kHH_HOST, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetIfModifiedSince(const char* i_Value)
{
	return SetHeader(kHH_IF_MODIFIED_SINCE,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfModifiedSince(const char* *o_Value) const
{
	return GetHeader(kHH_IF_MODIFIED_SINCE, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetIfMatch(const char* i_Value)
{
	return SetHeader(kHH_IF_MATCH,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfMatch(const char* *o_Value) const
{
	return GetHeader(kHH_IF_MATCH, o_Value);
}

NS_METHOD 
nsHTTPRequest::SetIfMatchAny(const char* i_Value)
{
	return SetHeader(kHH_IF_MATCH_ANY,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfMatchAny(const char* *o_Value) const
{
	return GetHeader(kHH_IF_MATCH_ANY,o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetIfNoneMatch(const char* i_Value)
{
	return SetHeader(kHH_IF_NONE_MATCH,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfNoneMatch(const char* *o_Value) const
{
	return GetHeader(kHH_IF_NONE_MATCH,o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetIfNoneMatchAny(const char* i_Value)
{
	return SetHeader(kHH_IF_NONE_MATCH_ANY,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfNoneMatchAny(const char* *o_Value) const
{
	return GetHeader(kHH_IF_NONE_MATCH_ANY,o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetIfRange(const char* i_Value)
{
	return SetHeader(kHH_IF_RANGE,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfRange(const char* *o_Value) const
{
	return GetHeader(kHH_IF_RANGE,o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetIfUnmodifiedSince(const char* i_Value)
{
	return SetHeader(kHH_IF_UNMODIFIED_SINCE,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetIfUnmodifiedSince(const char* *o_Value) const
{
	return GetHeader(kHH_IF_UNMODIFIED_SINCE, o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetMaxForwards(const char* i_Value)
{
	return SetHeader(kHH_MAX_FORWARDS,i_Value);
}

NS_METHOD 
nsHTTPRequest::GetMaxForwards(const char* *o_Value) const
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
nsHTTPRequest::GetRange(const char* *o_Value) const
{
	return GetHeader(kHH_RANGE, o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetReferer(const char* i_Value)
{
	return SetHeader(kHH_REFERER, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetReferer(const char* *o_Value) const
{
	return GetHeader(kHH_REFERER, o_Value);
}
                        
NS_METHOD 
nsHTTPRequest::SetUserAgent(const char* i_Value)
{
	return SetHeader(kHH_USER_AGENT, i_Value);
}

NS_METHOD 
nsHTTPRequest::GetUserAgent(const char* *o_Value) const
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
    else
    {
        // This should delete any existing headers! TODO
        return NS_ERROR_NOT_IMPLEMENTED;
    }
}

NS_METHOD
nsHTTPRequest::GetHeader(const char* i_Header, const char* *o_Value) const
{
    if (m_pArray && (0< m_pArray->Count()))
    {
        for (PRInt32 i = m_pArray->Count() - 1; i >= 0; --i) 
        {
            nsHeaderPair* element = NS_STATIC_CAST(nsHeaderPair*, m_pArray->ElementAt(i));
            if ((element->atom == NS_NewAtom(i_Header)) && o_Value)
            {
                *o_Value = (element->value) ? element->value->ToNewCString() : nsnull;
                return NS_OK;
            }
        }
    }

    *o_Value = nsnull;
    return NS_ERROR_NOT_FOUND;
}

NS_METHOD
nsHTTPRequest::GetHeaderMultiple(const char* i_Header, 
                    const char** *o_ValueArray,
                    int o_Count) const
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
nsHTTPRequest::GetHTTPVersion(HTTPVersion* o_Version) const
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
        m_Request->QueryInterface(nsIInputStream::GetIID(), (void**)o_Stream);
        return NS_OK;
    }
    else
        return NS_ERROR_NULL_POINTER;

}

NS_IMETHODIMP
nsHTTPRequest::OnStartBinding(nsISupports* i_pContext)
{
    //TODO globally replace printf with trace calls. 
    //printf("nsHTTPRequest::OnStartBinding...\n");

    return NS_OK;
}

NS_IMETHODIMP
nsHTTPRequest::OnStopBinding(nsISupports* i_pContext,
                                 nsresult iStatus,
                                 nsIString* i_pMsg)
{
    //printf("nsHTTPRequest::OnStopBinding...\n");
    // if we could write successfully... 
    if (NS_SUCCEEDED(iStatus)) 
    {
        //Prepare to receive the response!
        nsHTTPResponseListener* pListener = new nsHTTPResponseListener();
        m_pTransport->AsyncRead(
            i_pContext, 
            m_pConnection->EventQueue(), 
            pListener);
        //TODO check this portion here...
        return pListener ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
    else
    {
        NS_ERROR("Failed to write to server!");
    }

    /*
        Somewhere here we need to send a message up the event sink 
        that we successfully (or not) have sent request to the 
        server. TODO
    */
    return iStatus;
}

NS_IMETHODIMP
nsHTTPRequest::SetTransport(nsITransport* i_pTransport)
{
    NS_ASSERTION(!m_pTransport, "Transport being overwritten!");
    m_pTransport = i_pTransport;
    NS_ADDREF(m_pTransport);
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPRequest::SetConnection(nsHTTPConnection* i_pConnection)
{
    m_pConnection = i_pConnection;
    return NS_OK;
}