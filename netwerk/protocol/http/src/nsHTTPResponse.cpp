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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "prlog.h"
#include "nsHTTPResponse.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsIHTTPChannel.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsHTTPAtoms.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gHTTPLog;
#endif /* PR_LOGGING */

nsHTTPResponse::nsHTTPResponse()
{
    NS_INIT_REFCNT();

    mStatus = 0;
    mServerVersion = HTTP_ONE_ZERO;

    // The content length is unknown...
    mContentLength = -1;
}

nsHTTPResponse::~nsHTTPResponse()
{
}

NS_IMPL_ISUPPORTS(nsHTTPResponse, NS_GET_IID(nsISupports))


nsresult nsHTTPResponse::GetCharset(char* *o_Charset)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(o_Charset);

    // Check if status header has been parsed yet
    if (mCharset.Length() == 0)
        return NS_ERROR_NOT_AVAILABLE;

    *o_Charset = mCharset.ToNewCString();
    if (!*o_Charset)
        rv = NS_ERROR_OUT_OF_MEMORY;

    return rv;
}

nsresult nsHTTPResponse::SetCharset(const char* i_Charset)
{
    mCharset = i_Charset;
    return NS_OK;
}

nsresult nsHTTPResponse::GetContentType(char* *o_ContentType)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(o_ContentType);

    // Check if status header has been parsed yet
    if (mContentType.Length() == 0)
        return NS_ERROR_NOT_AVAILABLE;

    *o_ContentType = mContentType.ToNewCString();
    if (!*o_ContentType)
        rv = NS_ERROR_OUT_OF_MEMORY;

    return rv;
}

nsresult nsHTTPResponse::SetContentType(const char* i_ContentType)
{
    nsCAutoString cType(i_ContentType);
    cType.ToLowerCase();
    mContentType = cType.GetBuffer();
    return NS_OK;
}

nsresult nsHTTPResponse::GetContentLength(PRInt32* o_ContentLength)
{
    NS_ENSURE_ARG_POINTER(o_ContentLength);

    // Check if content-length header was received yet
    if (mContentLength == -1)
        return NS_ERROR_NOT_AVAILABLE;

    *o_ContentLength = mContentLength;
    return NS_OK;
}
    
nsresult nsHTTPResponse::SetContentLength(PRInt32 i_ContentLength)
{
    mContentLength = i_ContentLength;
    return NS_OK;
}

nsresult nsHTTPResponse::GetStatus(PRUint32* o_Value)
{
    nsresult rv = NS_OK;

    if (o_Value) {
        *o_Value = mStatus;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

nsresult nsHTTPResponse::GetStatusString(char* *o_String)
{
    nsresult rv = NS_OK;

    if (o_String) {
        *o_String = mStatusString.ToNewCString();
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

nsresult nsHTTPResponse::GetServer(char* *o_String)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// Finally our own methods...

nsresult nsHTTPResponse::SetHeader(nsIAtom* i_Header, const char* i_Value)
{
    return mHeaders.SetHeader(i_Header, i_Value);
}


nsresult nsHTTPResponse::GetHeader(nsIAtom* i_Header, char* *o_Value)
{
    return mHeaders.GetHeader(i_Header, o_Value);
}

nsresult nsHTTPResponse::SetServerVersion(const char* i_Version)
{
    // convert it to HTTP Version
    // TODO
    mServerVersion = HTTP_ONE_ZERO;
    return NS_OK;

}

nsresult nsHTTPResponse::SetStatusString(const char* i_Status)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(mStatusString.Length() == 0, "Overwriting status string!");
    mStatusString = i_Status;

    return rv;
}

nsresult nsHTTPResponse::GetHeaderEnumerator(nsISimpleEnumerator** aResult)
{
    return mHeaders.GetEnumerator(aResult);
}

nsresult nsHTTPResponse::ParseStatusLine(nsCString& aStatusLine)
{
    //
    // The Status Line has the following: format:
    //    HTTP-Version SP Status-Code SP Reason-Phrase CRLF
    //

    const char *token;
    nsCAutoString str;
    PRInt32 offset, error;

    //
    // Parse the HTTP-Version:: "HTTP" "/" 1*DIGIT "." 1*DIGIT
    //

    offset = aStatusLine.FindChar(' ');
    (void) aStatusLine.Left(str, offset);
    if (!str.Length()) {
        // The status line is bogus...
        return NS_ERROR_FAILURE;
    }
    token = str.GetBuffer();
    SetServerVersion(token);

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("\tParseStatusLine [this=%x].\tHTTP-Version: %s\n",
            this, token));

    aStatusLine.Cut(0, offset+1);

    //
    // Parse the Status-Code:: 3DIGIT
    //
    PRInt32 statusCode;

    offset = aStatusLine.FindChar(' ');
    (void) aStatusLine.Left(str, offset);
    if (3 != str.Length()) {
        // The status line is bogus...
        return NS_ERROR_FAILURE;
    }

    statusCode = str.ToInteger(&error);
    if (NS_FAILED(error)) return NS_ERROR_FAILURE;

    SetStatus(statusCode);

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("\tParseStatusLine [this=%x].\tStatus-Code: %d\n",
            this, statusCode));

    aStatusLine.Cut(0, offset+1);

    //
    // Parse the Reason-Phrase:: *<TEXT excluding CR,LF>
    //
    token = aStatusLine.GetBuffer();
    SetStatusString(token);

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("\tParseStatusLine [this=%x].\tReason-Phrase: %s\n",
            this, token));

    aStatusLine.Truncate();
  
    return NS_OK;
}

nsresult nsHTTPResponse::ParseHeader(nsCString& aHeaderString)
{
    nsresult rv;

    //
    // Extract the key field - everything up to the ':'
    // The header name is case-insensitive...
    //
    PRInt32 colonOffset;
    nsCAutoString headerKey;
    nsCOMPtr<nsIAtom> headerAtom;

    colonOffset = aHeaderString.FindChar(':');
    if (kNotFound == colonOffset) {
        //
        // The header is malformed... Just clear it.
        //
        aHeaderString.Truncate();
        return NS_ERROR_FAILURE;
    }
    (void) aHeaderString.Left(headerKey, colonOffset);
    headerKey.ToLowerCase();
    //
    // Extract the value field - everything past the ':'
    // Trim any leading or trailing whitespace...
    //
    aHeaderString.Cut(0, colonOffset+1);
    aHeaderString.Trim(" ");

    headerAtom = NS_NewAtom(headerKey.GetBuffer());
    if (headerAtom) {
        rv = ProcessHeader(headerAtom, aHeaderString);
    } else {
        rv = NS_ERROR_OUT_OF_MEMORY;
    }

    aHeaderString.Truncate();

    return rv;
}

nsresult nsHTTPResponse::ProcessHeader(nsIAtom* aHeader, nsCString& aValue)
{
    nsresult rv;

    //
    // When the Content-Type response header is processed, the Content-Type
    // and Charset information must be set into the nsHTTPChannel...
    //
    if (nsHTTPAtoms::Content_Type == aHeader) {
        nsCAutoString buffer;
        PRInt32 semicolon;

        // Set the content-type in the HTTPChannel...
        semicolon = aValue.FindChar(';');
        if (kNotFound != semicolon) {
            aValue.Left(buffer, semicolon);
            SetContentType(buffer.GetBuffer());

            // Does the Content-Type contain a charset attribute?
            aValue.Mid(buffer, semicolon+1, -1);
            buffer.Trim(" ");
            if (0 == buffer.Find("charset=", PR_TRUE)) {
                //
                // Set the charset in the HTTPChannel...
                //
                // XXX: Currently, the charset is *everything* past the "charset="
                //      This includes comments :-(
                //
                buffer.Cut(0, 8);
                SetCharset(buffer.GetBuffer());
            }
        } 
        else {
            SetContentType(aValue.GetBuffer());
        }
    }
    //
    // When the Content-Length response header is processed, set the
    // ContentLength in the Channel...
    //
    else if (nsHTTPAtoms::Content_Length == aHeader) {
        PRInt32 length, status;

        length = aValue.ToInteger(&status);
        rv = (nsresult)status;

        if (NS_SUCCEEDED(rv)) {
            SetContentLength(length);
        }
    }

    //
    // Set the response header...
    //
    rv = SetHeader(aHeader, aValue.GetBuffer());

    return rv;
}

