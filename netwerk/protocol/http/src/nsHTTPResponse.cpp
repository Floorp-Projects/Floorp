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

#include "nsHTTPResponse.h"
#include "nsIURL.h"
#include "nsIHTTPChannel.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsHeaderPair.h"
#include "nsVoidArray.h"
#include "nsIInputStream.h"
#include "kHTTPHeaders.h"
#include "plstr.h"

nsHTTPResponse::nsHTTPResponse(nsIInputStream* i_InputStream):
    m_pArray(new nsVoidArray()),
    m_pStatusString(nsnull)
{
    NS_INIT_REFCNT();

    m_pInputStream = i_InputStream;
    NS_IF_ADDREF(m_pInputStream);
}

nsHTTPResponse::~nsHTTPResponse()
{
    NS_IF_RELEASE(m_pInputStream);

    if (m_pStatusString) {
        nsCRT::free(m_pStatusString);
        m_pStatusString = nsnull;
    }

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

}

NS_IMPL_ADDREF(nsHTTPResponse);

nsresult
nsHTTPResponse::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    if (aIID.Equals(nsCOMTypeInfo<nsIHTTPResponse>::GetIID())) {
        *aInstancePtr = (void*) ((nsIHTTPResponse*)this);
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
 
NS_IMPL_RELEASE(nsHTTPResponse);

//TODO make these inlines...
NS_METHOD 
nsHTTPResponse::SetAllow(const char* i_Value)
{
    return SetHeader(kHH_ALLOW, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetAllow(char* *o_Value)
{
    return GetHeader(kHH_ALLOW, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetContentBase(const char* i_Value)
{
    return SetHeader(kHH_CONTENT_BASE, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetContentBase(char* *o_Value)
{
	return GetHeader(kHH_CONTENT_BASE, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetContentEncoding(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_ENCODING, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetContentEncoding(char* *o_Value)
{
	return GetHeader(kHH_CONTENT_ENCODING, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetContentLanguage(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_LANGUAGE, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetContentLanguage(char* *o_Value)
{
	return GetHeader(kHH_CONTENT_LANGUAGE, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetContentLength(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_LENGTH, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetContentLength(char* *o_Value)
{
	return GetHeader(kHH_CONTENT_LENGTH, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetContentLocation(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_LOCATION, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetContentLocation(char* *o_Value)
{
	return GetHeader(kHH_CONTENT_LOCATION, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetContentMD5(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_MD5, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetContentMD5(char* *o_Value)
{
	return GetHeader(kHH_CONTENT_MD5, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetContentRange(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_RANGE, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetContentRange(char* *o_Value)
{
	return GetHeader(kHH_CONTENT_RANGE, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetContentTransferEncoding(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_TRANSFER_ENCODING, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetContentTransferEncoding(char* *o_Value)
{
	return GetHeader(kHH_CONTENT_TRANSFER_ENCODING, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetContentType(const char* i_Value)
{
	return SetHeader(kHH_CONTENT_TYPE, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetContentType(char* *o_Value)
{
	return GetHeader(kHH_CONTENT_TYPE, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetDerivedFrom(const char* i_Value)
{
	return SetHeader(kHH_DERIVED_FROM, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetDerivedFrom(char* *o_Value)
{
	return GetHeader(kHH_DERIVED_FROM, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetETag(const char* i_Value)
{
	return SetHeader(kHH_ETAG, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetETag(char* *o_Value)
{
	return GetHeader(kHH_ETAG, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetExpires(const char* i_Value)
{
	return SetHeader(kHH_EXPIRES, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetExpires(char* *o_Value)
{
	return GetHeader(kHH_EXPIRES, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetLastModified(const char* i_Value)
{
	return SetHeader(kHH_LAST_MODIFIED, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetLastModified(char* *o_Value)
{
	return GetHeader(kHH_LAST_MODIFIED, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetLink(const char* i_Value)
{
	return SetHeader(kHH_LINK, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetLink(char* *o_Value)
{
	return GetHeader(kHH_LINK, o_Value);
}

NS_METHOD 
nsHTTPResponse::GetLinkMultiple(
                            const char** *o_ValueArray, 
                            int count) const
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD 
nsHTTPResponse::SetTitle(const char* i_Value)
{
	return SetHeader(kHH_TITLE, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetTitle(char* *o_Value)
{
	return GetHeader(kHH_TITLE, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetURI(const char* i_Value)
{
	return SetHeader(kHH_URI, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetURI(char* *o_Value)
{
	return GetHeader(kHH_URI, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetVersion(const char* i_Value)
{
	return SetHeader(kHH_VERSION, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetVersion(char* *o_Value)
{
	return GetHeader(kHH_VERSION, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetConnection(const char* i_Value)
{
	return SetHeader(kHH_CONNECTION, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetConnection(char* *o_Value)
{
	return GetHeader(kHH_CONNECTION, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetDate(const char* i_Value)
{
	return SetHeader(kHH_DATE, i_Value);
}

NS_METHOD 
nsHTTPResponse::GetDate(char* *o_Value)
{
	return GetHeader(kHH_DATE, o_Value);
}

NS_METHOD 
nsHTTPResponse::SetPragma(const char* i_Value)
{
	return SetHeader(kHH_PRAGMA,i_Value);
}

NS_METHOD 
nsHTTPResponse::GetPragma(char* *o_Value)
{
	return GetHeader(kHH_PRAGMA,o_Value);
}

NS_METHOD 
nsHTTPResponse::SetForwarded(const char* i_Value)
{
	return SetHeader(kHH_FORWARDED,i_Value);
}

NS_METHOD 
nsHTTPResponse::GetForwarded(char* *o_Value)
{
	return GetHeader(kHH_FORWARDED,o_Value);
}

NS_METHOD 
nsHTTPResponse::SetMessageID(const char* i_Value)
{
	return SetHeader(kHH_MESSAGE_ID,i_Value);
}

NS_METHOD 
nsHTTPResponse::GetMessageID(char* *o_Value)
{
	return GetHeader(kHH_MESSAGE_ID,o_Value);
}

NS_METHOD 
nsHTTPResponse::SetMIME(const char* i_Value)
{
	return SetHeader(kHH_MIME,i_Value);
}

NS_METHOD 
nsHTTPResponse::GetMIME(char* *o_Value)
{
	return GetHeader(kHH_MIME,o_Value);
}

NS_METHOD 
nsHTTPResponse::SetTrailer(const char* i_Value)
{
	return SetHeader(kHH_TRAILER,i_Value);
}

NS_METHOD 
nsHTTPResponse::GetTrailer(char* *o_Value)
{
	return GetHeader(kHH_TRAILER,o_Value);
}

NS_METHOD 
nsHTTPResponse::SetTransfer(const char* i_Value)
{
	return SetHeader(kHH_TRANSFER,i_Value);
}

NS_METHOD 
nsHTTPResponse::GetTransfer(char* *o_Value)
{
	return GetHeader(kHH_TRANSFER,o_Value);
}

NS_METHOD
nsHTTPResponse::GetContentLength(PRInt32* o_Value) const
{
    if (o_Value)
        *o_Value = -1;
    return NS_OK;
}

NS_METHOD
nsHTTPResponse::GetStatus(PRUint32* o_Value) const
{
    if (o_Value)
        *o_Value = m_Status;
    else 
        return NS_ERROR_NULL_POINTER;
    return NS_OK;
}

NS_METHOD
nsHTTPResponse::GetStatusString(char* *o_String) const
{
    if (o_String)
        *o_String = m_pStatusString;
    return NS_OK;
}

NS_METHOD
nsHTTPResponse::GetServer(char* *o_String) const
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// Finally our own methods...

NS_METHOD
nsHTTPResponse::SetHeader(const char* i_Header, const char* i_Value)
{
    return NS_ERROR_NOT_IMPLEMENTED; // and should not be used on responses
}

NS_METHOD
nsHTTPResponse::SetHeaderInternal(const char* i_Header, const char* i_Value)
{
    NS_ASSERTION(m_pArray, "header array doesn't exist.");
    if (i_Value)
    {
        //The tempValue gets copied so we can do away with it...
        nsString tempValue(i_Value);
        tempValue.Trim(" \r\n");
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
nsHTTPResponse::GetHeader(const char* i_Header, char* *o_Value)
{
    NS_ASSERTION(m_pArray, "header array doesn't exist.");

    if (!i_Header || !o_Value)
        return NS_ERROR_NULL_POINTER;

    nsAutoString lowerHeader(i_Header);
    lowerHeader.ToLowerCase();    

    nsIAtom* header = NS_NewAtom(lowerHeader);
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
nsHTTPResponse::GetHeaderMultiple(const char* i_Header, 
                                  char** *o_ValueArray,
                                  int *o_Count)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPResponse::SetServerVersion(const char* i_Version)
{
    // convert it to HTTP Version
    // TODO
    m_ServerVersion = HTTP_ONE_ZERO;
    return NS_OK;

}

NS_METHOD
nsHTTPResponse::SetStatusString(const char* i_Status)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(!m_pStatusString, "Overwriting status string!");
    m_pStatusString = nsCRT::strdup(i_Status);
    if (!m_pStatusString) {
      rv = NS_ERROR_FAILURE;
    }

    return rv;
}

NS_METHOD
nsHTTPResponse::GetInputStream(nsIInputStream* *o_Stream)
{
    *o_Stream = m_pInputStream;
    return NS_OK;
}
