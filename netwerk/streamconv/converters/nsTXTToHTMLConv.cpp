/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsTXTToHTMLConv.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsStringStream.h"
#include "nsAutoPtr.h"

#define TOKEN_DELIMITERS NS_LITERAL_STRING("\t\r\n ").get()

// nsISupports methods
NS_IMPL_ISUPPORTS4(nsTXTToHTMLConv,
                   nsIStreamConverter,
                   nsITXTToHTMLConv,
                   nsIRequestObserver,
                   nsIStreamListener)


// nsIStreamConverter methods
NS_IMETHODIMP
nsTXTToHTMLConv::Convert(nsIInputStream *aFromStream,
                         const char *aFromType, const char *aToType,
                         nsISupports *aCtxt, nsIInputStream * *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTXTToHTMLConv::AsyncConvertData(const char *aFromType,
                                  const char *aToType,
                                  nsIStreamListener *aListener,
                                  nsISupports *aCtxt)
{
    NS_ASSERTION(aListener, "null pointer");
    mListener = aListener;
    return NS_OK;
}


// nsIRequestObserver methods
NS_IMETHODIMP
nsTXTToHTMLConv::OnStartRequest(nsIRequest* request, nsISupports *aContext)
{
    mBuffer.AssignLiteral("<html>\n<head><title>");
    mBuffer.Append(mPageTitle);
    mBuffer.AppendLiteral("</title></head>\n<body>\n");
    if (mPreFormatHTML) {     // Use <pre> tags
        mBuffer.AppendLiteral("<pre>\n");
    }

    // Push mBuffer to the listener now, so the initial HTML will not
    // be parsed in OnDataAvailable().

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    if (channel)
        channel->SetContentType(NS_LITERAL_CSTRING("text/html"));
    // else, assume there is a channel somewhere that knows what it is doing!

    nsresult rv = mListener->OnStartRequest(request, aContext);
    if (NS_FAILED(rv)) return rv;

    // The request may have been canceled, and if that happens, we want to
    // suppress calls to OnDataAvailable.
    request->GetStatus(&rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStream> inputData;
    rv = NS_NewStringInputStream(getter_AddRefs(inputData), mBuffer);
    if (NS_FAILED(rv)) return rv;

    rv = mListener->OnDataAvailable(request, aContext,
                                    inputData, 0, mBuffer.Length());
    if (NS_FAILED(rv)) return rv;
    mBuffer.Truncate();
    return rv;
}

NS_IMETHODIMP
nsTXTToHTMLConv::OnStopRequest(nsIRequest* request, nsISupports *aContext,
                               nsresult aStatus)
{
    nsresult rv = NS_OK;
    if (mToken) {
        // we still have an outstanding token
        NS_ASSERTION(mToken->prepend,
                     "Non prepending tokens should be handled in "
                     "OnDataAvailable. There should only be a single "
                     "prepending token left to be processed.");
        (void)CatHTML(0, mBuffer.Length());
    }
    if (mPreFormatHTML) {
        mBuffer.AppendLiteral("</pre>\n");
    }
    mBuffer.AppendLiteral("\n</body></html>");

    nsCOMPtr<nsIInputStream> inputData;

    rv = NS_NewStringInputStream(getter_AddRefs(inputData), mBuffer);
    if (NS_FAILED(rv)) return rv;

    rv = mListener->OnDataAvailable(request, aContext,
                                    inputData, 0, mBuffer.Length());
    if (NS_FAILED(rv)) return rv;

    return mListener->OnStopRequest(request, aContext, aStatus);
}

// nsITXTToHTMLConv methods
NS_IMETHODIMP
nsTXTToHTMLConv::SetTitle(const PRUnichar *aTitle)
{
    mPageTitle.Assign(aTitle);
    return NS_OK;
}

NS_IMETHODIMP
nsTXTToHTMLConv::PreFormatHTML(bool value)
{
    mPreFormatHTML = value;
    return NS_OK;
}

// nsIStreamListener method
NS_IMETHODIMP
nsTXTToHTMLConv::OnDataAvailable(nsIRequest* request, nsISupports *aContext,
                                 nsIInputStream *aInStream,
                                 PRUint32 aOffset, PRUint32 aCount)
{
    nsresult rv = NS_OK;
    nsString pushBuffer;
    PRUint32 amtRead = 0;
    nsAutoArrayPtr<char> buffer(new char[aCount+1]);
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

    do {
        PRUint32 read = 0;
        // XXX readSegments, to avoid the first copy?
        rv = aInStream->Read(buffer, aCount-amtRead, &read);
        if (NS_FAILED(rv)) return rv;

        buffer[read] = '\0';
        // XXX charsets?? non-latin1 characters?? utf-16??
        AppendASCIItoUTF16(buffer, mBuffer);
        amtRead += read;

        PRInt32 front = -1, back = -1, tokenLoc = -1, cursor = 0;

        while ( (tokenLoc = FindToken(cursor, &mToken)) > -1) {
            if (mToken->prepend) {
                front = mBuffer.RFindCharInSet(TOKEN_DELIMITERS, tokenLoc);
                front++;
                back = mBuffer.FindCharInSet(TOKEN_DELIMITERS, tokenLoc);
            } else {
                front = tokenLoc;
                back = front + mToken->token.Length();
            }
            if (back == -1) {
                // didn't find an ending, buffer up.
                mBuffer.Left(pushBuffer, front);
                cursor = front;
                break;
            }
            // found the end of the token.
            cursor = CatHTML(front, back);
        }

        PRInt32 end = mBuffer.RFind(TOKEN_DELIMITERS, mBuffer.Length());
        mBuffer.Left(pushBuffer, NS_MAX(cursor, end));
        mBuffer.Cut(0, NS_MAX(cursor, end));
        cursor = 0;

        if (!pushBuffer.IsEmpty()) {
            nsCOMPtr<nsIInputStream> inputData;

            rv = NS_NewStringInputStream(getter_AddRefs(inputData), pushBuffer);
            if (NS_FAILED(rv))
                return rv;

            rv = mListener->OnDataAvailable(request, aContext,
                                            inputData, 0, pushBuffer.Length());
            if (NS_FAILED(rv))
                return rv;
        }
    } while (amtRead < aCount);

    return rv;
}

// nsTXTToHTMLConv methods
nsTXTToHTMLConv::nsTXTToHTMLConv()
{
    mToken = nsnull;
    mPreFormatHTML = PR_FALSE;
}

nsTXTToHTMLConv::~nsTXTToHTMLConv()
{
    mTokens.Clear();
}

nsresult
nsTXTToHTMLConv::Init()
{
    nsresult rv = NS_OK;

    // build up the list of tokens to handle
    convToken *token = new convToken;
    if (!token) return NS_ERROR_OUT_OF_MEMORY;
    token->prepend = PR_FALSE;
    token->token.Assign(PRUnichar('<'));
    token->modText.AssignLiteral("&lt;");
    mTokens.AppendElement(token);

    token = new convToken;
    if (!token) return NS_ERROR_OUT_OF_MEMORY;
    token->prepend = PR_FALSE;
    token->token.Assign(PRUnichar('>'));
    token->modText.AssignLiteral("&gt;");
    mTokens.AppendElement(token);

    token = new convToken;
    if (!token) return NS_ERROR_OUT_OF_MEMORY;
    token->prepend = PR_FALSE;
    token->token.Assign(PRUnichar('&'));
    token->modText.AssignLiteral("&amp;");
    mTokens.AppendElement(token);

    token = new convToken;
    if (!token) return NS_ERROR_OUT_OF_MEMORY;
    token->prepend = PR_TRUE;
    token->token.AssignLiteral("http://"); // XXX need to iterate through all protos
    mTokens.AppendElement(token);

    token = new convToken;
    if (!token) return NS_ERROR_OUT_OF_MEMORY;
    token->prepend = PR_TRUE;
    token->token.Assign(PRUnichar('@'));
    token->modText.AssignLiteral("mailto:");
    mTokens.AppendElement(token);

    return rv;
}

PRInt32
nsTXTToHTMLConv::FindToken(PRInt32 cursor, convToken* *_retval)
{
    PRInt32 loc = -1, firstToken = mBuffer.Length();
    PRInt8 token = -1;
    for (PRUint8 i=0; i < mTokens.Length(); i++) {
        loc = mBuffer.Find(mTokens[i]->token, cursor);
        if (loc != -1)
            if (loc < firstToken) {
                firstToken = loc;
                token = i;
            }
    }
    if (token == -1)
        return -1;

    *_retval = mTokens[token];
    return firstToken;
}

PRInt32
nsTXTToHTMLConv::CatHTML(PRInt32 front, PRInt32 back)
{
    PRInt32 cursor = 0;
    PRInt32 modLen = mToken->modText.Length();
    if (!mToken->prepend) {
        // replace the entire token (from delimiter to delimiter)
        mBuffer.Cut(front, back - front);
        mBuffer.Insert(mToken->modText, front);
        cursor = front+modLen;
    } else {
        nsString linkText;
        // href is implied
        mBuffer.Mid(linkText, front, back-front);

        mBuffer.Insert(NS_LITERAL_STRING("<a href=\""), front);
        cursor += front+9;
        if (modLen) {
            mBuffer.Insert(mToken->modText, cursor);
            cursor += modLen;
        }

        NS_ConvertUTF16toUTF8 linkTextUTF8(linkText);
        nsCString escaped;
        if (NS_EscapeURL(linkTextUTF8.Data(), linkTextUTF8.Length(), esc_Minimal, escaped)) {
            mBuffer.Cut(cursor, back - front);
            CopyUTF8toUTF16(escaped, linkText);
            mBuffer.Insert(linkText, cursor);
            back = front + linkText.Length();
        }

        cursor += back-front;
        mBuffer.Insert(NS_LITERAL_STRING("\">"), cursor);
        cursor += 2;
        mBuffer.Insert(linkText, cursor);
        cursor += linkText.Length();
        mBuffer.Insert(NS_LITERAL_STRING("</a>"), cursor);
        cursor += 4;
    }
    mToken = nsnull; // indicates completeness
    return cursor;
}
