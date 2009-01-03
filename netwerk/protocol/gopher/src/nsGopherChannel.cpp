/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 cin et:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Gopher protocol code.
 *
 * The Initial Developer of the Original Code is
 * Bradley Baetz.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bradley Baetz <bbaetz@student.usyd.edu.au>
 *   Darin Fisher <darin@netscape.com>
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

#include "nsGopherChannel.h"
#include "nsGopherHandler.h"
#include "nsBaseContentStream.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIStringBundle.h"
#include "nsITXTToHTMLConv.h"
#include "nsIPrompt.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsStreamUtils.h"
#include "nsMimeTypes.h"
#include "nsNetCID.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsEscape.h"
#include "nsCRT.h"
#include "netCore.h"

// Specifies the maximum number of output stream buffer segments that we can
// allocate before giving up.  At 4k per segment, this corresponds to a max
// gopher request of 400k, which should be plenty.
#define GOPHER_MAX_WRITE_SEGMENT_COUNT 100

//-----------------------------------------------------------------------------

class nsGopherContentStream : public nsBaseContentStream
                            , public nsIInputStreamCallback
                            , public nsIOutputStreamCallback
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINPUTSTREAMCALLBACK
    NS_DECL_NSIOUTPUTSTREAMCALLBACK

    // stream methods that we override:
    NS_IMETHOD Available(PRUint32 *result);
    NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void *closure,
                            PRUint32 count, PRUint32 *result);
    NS_IMETHOD CloseWithStatus(nsresult status);

    nsGopherContentStream(nsGopherChannel *channel)
        : nsBaseContentStream(PR_TRUE)  // non-blocking
        , mChannel(channel) {
    }

    nsresult OpenSocket(nsIEventTarget *target);
    nsresult OnSocketWritable();
    nsresult ParseTypeAndSelector(char &type, nsCString &selector);
    nsresult PromptForQueryString(nsCString &result);
    void     UpdateContentType(char type);
    nsresult SendRequest();

protected:
    virtual void OnCallbackPending();

private:
    nsRefPtr<nsGopherChannel>      mChannel;
    nsCOMPtr<nsISocketTransport>   mSocket;
    nsCOMPtr<nsIAsyncOutputStream> mSocketOutput;
    nsCOMPtr<nsIAsyncInputStream>  mSocketInput;
};

NS_IMPL_ISUPPORTS_INHERITED2(nsGopherContentStream,
                             nsBaseContentStream,
                             nsIInputStreamCallback,
                             nsIOutputStreamCallback)

NS_IMETHODIMP
nsGopherContentStream::Available(PRUint32 *result)
{
    if (mSocketInput)
        return mSocketInput->Available(result);

    return nsBaseContentStream::Available(result);
}

NS_IMETHODIMP
nsGopherContentStream::ReadSegments(nsWriteSegmentFun writer, void *closure,
                                    PRUint32 count, PRUint32 *result)
{
    // Insert a thunk here so that the input stream passed to the writer is
    // this input stream instead of mSocketInput.
    if (mSocketInput) {
        nsWriteSegmentThunk thunk = { this, writer, closure };
        return mSocketInput->ReadSegments(NS_WriteSegmentThunk, &thunk, count,
                                          result);
    }

    return nsBaseContentStream::ReadSegments(writer, closure, count, result);
}

NS_IMETHODIMP
nsGopherContentStream::CloseWithStatus(nsresult status)
{
    if (mSocket) {
        mSocket->Close(status);
        mSocket = nsnull;
        mSocketInput = nsnull;
        mSocketOutput = nsnull;
    }
    return nsBaseContentStream::CloseWithStatus(status);
}

NS_IMETHODIMP
nsGopherContentStream::OnInputStreamReady(nsIAsyncInputStream *stream)
{
    // Forward this notification
    DispatchCallbackSync();
    return NS_OK;
}

NS_IMETHODIMP
nsGopherContentStream::OnOutputStreamReady(nsIAsyncOutputStream *stream)
{
    // If we're already closed, mSocketOutput is going to be null and we'll
    // just be getting notified that it got closed (by outselves).  In that
    // case, nothing to do here.
    if (!mSocketOutput) {
        NS_ASSERTION(NS_FAILED(Status()), "How did that happen?");
        return NS_OK;
    }
    
    // We have to close ourselves if we hit an error here in order to propagate
    // the error to our consumer.  Otherwise, just forward the notification so
    // that the consumer will know to start reading.

    nsresult rv = OnSocketWritable();
    if (NS_FAILED(rv))
        CloseWithStatus(rv);

    return NS_OK;
}

void
nsGopherContentStream::OnCallbackPending()
{
    nsresult rv;

    // We have a callback, so failure means we should close the stream.
    if (!mSocket) {
        rv = OpenSocket(CallbackTarget());
    } else if (mSocketInput) {
        rv = mSocketInput->AsyncWait(this, 0, 0, CallbackTarget());
    }
 
    if (NS_FAILED(rv))
        CloseWithStatus(rv);
}

nsresult
nsGopherContentStream::OpenSocket(nsIEventTarget *target)
{
    // This function is called to get things started.

    // We begin by opening a socket to the specified host and wait for the
    // socket to become writable.

    nsCAutoString host;
    nsresult rv = mChannel->URI()->GetAsciiHost(host);
    if (NS_FAILED(rv))
        return rv;
    if (host.IsEmpty())
        return NS_ERROR_MALFORMED_URI;

    // For security reasons, don't allow anything expect the default
    // gopher port (70). See bug 71916 - bbaetz@cs.mcgill.ca
    PRInt32 port = GOPHER_PORT;

    // Create socket tranport
    nsCOMPtr<nsISocketTransportService> sts = 
             do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;
    rv = sts->CreateTransport(nsnull, 0, host, port, mChannel->ProxyInfo(),
                              getter_AddRefs(mSocket));
    if (NS_FAILED(rv))
        return rv;

    // Setup progress and status notifications
    rv = mSocket->SetEventSink(mChannel, target);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIOutputStream> output;
    rv = mSocket->OpenOutputStream(0, 0, GOPHER_MAX_WRITE_SEGMENT_COUNT,
                                   getter_AddRefs(output));
    if (NS_FAILED(rv))
        return rv;
    mSocketOutput = do_QueryInterface(output);
    NS_ENSURE_STATE(mSocketOutput);

    return mSocketOutput->AsyncWait(this, 0, 0, target);
}

nsresult
nsGopherContentStream::OnSocketWritable()
{
    // Write to output stream (we can do this in one big chunk)
    nsresult rv = SendRequest();
    if (NS_FAILED(rv))
        return rv;

    // Open input stream
    nsCOMPtr<nsIInputStream> input;
    rv = mSocket->OpenInputStream(0, 0, 0, getter_AddRefs(input));
    if (NS_FAILED(rv))
        return rv;
    mSocketInput = do_QueryInterface(input, &rv);

    NS_ASSERTION(CallbackTarget(), "where is my pending callback?");
    rv = mSocketInput->AsyncWait(this, 0, 0, CallbackTarget());

    return rv;
}

nsresult
nsGopherContentStream::ParseTypeAndSelector(char &type, nsCString &selector)
{
    nsCAutoString buffer;
    nsresult rv = mChannel->URI()->GetPath(buffer); // unescaped down below
    if (NS_FAILED(rv))
        return rv;

    // No path given
    if (buffer[0] == '\0' || (buffer[0] == '/' && buffer[1] == '\0')) {
        type = '1';
        selector.Truncate();
    } else {
        NS_ENSURE_STATE(buffer[1] != '\0');

        type = buffer[1]; // Ignore leading '/'

        // Do it this way in case selector contains embedded nulls after
        // unescaping.
        char *sel = buffer.BeginWriting() + 2;
        PRInt32 count = nsUnescapeCount(sel);
        selector.Assign(sel, count);

        // NOTE: FindCharInSet cannot be used to search for a null byte.
        if (selector.FindCharInSet("\t\n\r") != kNotFound ||
            selector.FindChar('\0') != kNotFound) {
            // gopher selectors cannot containt tab, cr, lf, or \0
            return NS_ERROR_MALFORMED_URI;
        }
    }

    return NS_OK;
}

nsresult
nsGopherContentStream::PromptForQueryString(nsCString &result)
{
    nsCOMPtr<nsIPrompt> prompter;
    mChannel->GetCallback(prompter);
    if (!prompter) {
        NS_ERROR("We need a prompter!");
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIStringBundle> bundle;
    nsCOMPtr<nsIStringBundleService> bundleSvc =
            do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    if (bundleSvc)
        bundleSvc->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(bundle));

    nsXPIDLString promptTitle, promptText;
    if (bundle) {
        bundle->GetStringFromName(NS_LITERAL_STRING("GopherPromptTitle").get(),
                                  getter_Copies(promptTitle));
        bundle->GetStringFromName(NS_LITERAL_STRING("GopherPromptText").get(),
                                  getter_Copies(promptText));
    }
    if (promptTitle.IsEmpty())
        promptTitle.AssignLiteral("Search");
    if (promptText.IsEmpty())
        promptText.AssignLiteral("Enter a search term:");

    nsXPIDLString value;
    PRBool res = PR_FALSE;
    prompter->Prompt(promptTitle.get(), promptText.get(),
                     getter_Copies(value), NULL, NULL, &res);
    if (!res || value.IsEmpty())
        return NS_ERROR_FAILURE;

    CopyUTF16toUTF8(value, result);  // XXX Is UTF-8 the right thing?
    return NS_OK;
}

void
nsGopherContentStream::UpdateContentType(char type)
{
    const char *contentType = nsnull;

    switch(type) {
    case '0':
    case 'h':
    case '2': // CSO search - unhandled, should not be selectable
    case '3': // "Error" - should not be selectable
    case 'i': // info line- should not be selectable
        contentType = TEXT_HTML;
        break;
    case '1':
    case '7': // search - returns a directory listing
        contentType = APPLICATION_HTTP_INDEX_FORMAT;
        break;
    case 'g':
    case 'I':
        contentType = IMAGE_GIF;
        break;
    case 'T': // tn3270 - type doesn't make sense
    case '8': // telnet - type doesn't make sense
        contentType = TEXT_PLAIN;
        break;
    case '5': // "DOS binary archive of some sort" - is the mime-type correct?
    case '9': // "Binary file!"
        contentType = APPLICATION_OCTET_STREAM;
        break;
    case '4': // "BinHexed Macintosh file"
        contentType = APPLICATION_BINHEX;
        break;
    case '6':
        contentType = APPLICATION_UUENCODE;
        break;
    }

    if (contentType)
        mChannel->SetContentType(nsDependentCString(contentType));
}

nsresult
nsGopherContentStream::SendRequest()
{
    char type;
    nsCAutoString request;  // used to build request data

    nsresult rv = ParseTypeAndSelector(type, request);
    if (NS_FAILED(rv))
        return rv;

    // So, we use the selector as is unless it is a search url
    if (type == '7') {
        // Note that we don't use the "standard" nsIURL parsing stuff here
        // because the only special character is ?, and its possible to search
        // for a string containing a #, and so on
        
        // XXX - should this find the last or first entry?
        // '?' is valid in both the search string and the url
        // so no matter what this does, it may be incorrect
        // This only affects people codeing the query directly into the URL
        PRInt32 pos = request.RFindChar('?');
        if (pos != kNotFound) {
            // Just replace it with a tab
            request.SetCharAt('\t', pos);
        } else {
            // We require a query string here - if we don't have one,
            // then we need to ask the user
            nsCAutoString search;
            rv = PromptForQueryString(search);
            if (NS_FAILED(rv))
                return rv;
    
            request.Append('\t');
            request.Append(search);

            // and update our uri (XXX should probably redirect instead to avoid
            //                         confusing consumers of the channel)
            nsCAutoString spec;
            rv = mChannel->URI()->GetAsciiSpec(spec);
            if (NS_FAILED(rv))
                return rv;

            spec.Append('?');
            spec.Append(search);
            rv = mChannel->URI()->SetSpec(spec);
            if (NS_FAILED(rv))
                return rv;
        }
    }

    request.Append(CRLF);
    
    PRUint32 n;
    rv = mSocketOutput->Write(request.get(), request.Length(), &n);
    if (NS_FAILED(rv))
        return rv;
    NS_ENSURE_STATE(n == request.Length());

    // Now, push stream converters appropriately based on our 'type'
    if (type == '1' || type == '7') {
        rv = mChannel->PushStreamConverter("text/gopher-dir",
                                           APPLICATION_HTTP_INDEX_FORMAT);
        if (NS_FAILED(rv))
            return rv;
    } else if (type == '0') {
        nsCOMPtr<nsIStreamListener> converter;
        rv = mChannel->PushStreamConverter(TEXT_PLAIN, TEXT_HTML, PR_TRUE,
                                           getter_AddRefs(converter));
        if (NS_FAILED(rv))
            return rv;
        nsCOMPtr<nsITXTToHTMLConv> config = do_QueryInterface(converter);
        if (config) {
            nsCAutoString spec;
            mChannel->URI()->GetSpec(spec);
            config->SetTitle(NS_ConvertUTF8toUTF16(spec).get());
            config->PreFormatHTML(PR_TRUE);
        }
    }

    UpdateContentType(type);
    return NS_OK;
}

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS_INHERITED1(nsGopherChannel,
                             nsBaseChannel,
                             nsIProxiedChannel)

NS_IMETHODIMP
nsGopherChannel::GetProxyInfo(nsIProxyInfo** aProxyInfo)
{
    *aProxyInfo = ProxyInfo();
    NS_IF_ADDREF(*aProxyInfo);
    return NS_OK;
}

nsresult
nsGopherChannel::OpenContentStream(PRBool async, nsIInputStream **result,
                                   nsIChannel** channel)
{
    // Implement nsIChannel::Open in terms of nsIChannel::AsyncOpen
    if (!async)
        return NS_ERROR_NOT_IMPLEMENTED;

    nsRefPtr<nsIInputStream> stream = new nsGopherContentStream(this);
    if (!stream)
        return NS_ERROR_OUT_OF_MEMORY;

    *result = nsnull;
    stream.swap(*result);
    return NS_OK;
}

PRBool
nsGopherChannel::GetStatusArg(nsresult status, nsString &statusArg)
{
    nsCAutoString host;
    URI()->GetHost(host);
    CopyUTF8toUTF16(host, statusArg);
    return PR_TRUE;
}
