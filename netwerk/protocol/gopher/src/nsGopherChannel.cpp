/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

// gopher implementation - based on datetime and finger implimentations
// and documentation

#include "nsGopherChannel.h"
#include "nsMimeTypes.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "netCore.h"
#include "nsCRT.h"
#include "prlog.h"

#include "nsIServiceManager.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsISocketTransportService.h"
#include "nsIStringStream.h"
#include "nsIStreamConverterService.h"
#include "nsITXTToHTMLConv.h"
#include "nsIEventQueue.h"
#include "nsEventQueueUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);

#ifdef PR_LOGGING
extern PRLogModuleInfo* gGopherLog;
#endif

#define BUFFER_SEG_SIZE (4*1024)
#define BUFFER_MAX_SIZE (64*1024)

// nsGopherChannel methods
nsGopherChannel::nsGopherChannel()
    : mContentLength(-1),
      mType(-1),
      mStatus(NS_OK),
      mIsPending(PR_FALSE)
{
}

nsGopherChannel::~nsGopherChannel()
{
#ifdef PR_LOGGING
    nsCAutoString spec;
    mUrl->GetAsciiSpec(spec);
    PR_LOG(gGopherLog, PR_LOG_ALWAYS, ("~nsGopherChannel() for %s", spec.get()));
#endif
}

NS_IMPL_THREADSAFE_ISUPPORTS5(nsGopherChannel,
                              nsIChannel,
                              nsIRequest,
                              nsIStreamListener,
                              nsIRequestObserver,
                              nsITransportEventSink)

nsresult
nsGopherChannel::Init(nsIURI* uri, nsIProxyInfo* proxyInfo)
{
    nsresult rv;

    nsCOMPtr<nsIURL> url = do_QueryInterface(uri, &rv);
    if (NS_FAILED(rv))
        return NS_ERROR_MALFORMED_URI;
    
    mUrl = uri;
    mProxyInfo = proxyInfo;
    
    nsCAutoString buffer;

    rv = url->GetPath(buffer); // unescaped down below
    if (NS_FAILED(rv))
        return rv;

    rv = url->GetAsciiHost(mHost);
    if (NS_FAILED(rv))
        return rv;
    rv = url->GetPort(&mPort);
    if (NS_FAILED(rv))
        return rv;

    // For security reasons, don't allow anything expect the default
    // gopher port (70). See bug 71916 - bbaetz@cs.mcgill.ca
/*
    if (mPort==-1)
        mPort=GOPHER_PORT;
*/
    mPort=GOPHER_PORT;

    // No path given
    if (buffer[0]=='\0' || (buffer[0]=='/' && buffer[1]=='\0')) {
        mType = '1';
        mSelector.Truncate();
    } else {
        mType = buffer[1]; // Ignore leading '/'

        // Do it this way in case selector contains embedded nulls after
        // unescaping
        char* selector = nsCRT::strdup(buffer.get()+2);
        PRInt32 count = nsUnescapeCount(selector);
        mSelector.Assign(selector,count);
        nsCRT::free(selector);

        if (mSelector.FindCharInSet(nsCString("\t\n\r\0",4)) != -1) {
            // gopher selectors cannot containt tab, cr, lf, or \0
            return NS_ERROR_MALFORMED_URI;
        }
    }

    PR_LOG(gGopherLog,
           PR_LOG_DEBUG,
           ("Host: mHost = %s, mPort = %d\n", mHost.get(), mPort));
    PR_LOG(gGopherLog,
           PR_LOG_DEBUG,
           ("Status: mType = %c, mSelector = %s\n", mType, mSelector.get()));
    
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsGopherChannel::GetName(nsACString &result)
{
    return mUrl->GetHostPort(result);
}

NS_IMETHODIMP
nsGopherChannel::IsPending(PRBool *result)
{
    *result = mIsPending;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::GetStatus(nsresult *status)
{
    if (mPump && NS_SUCCEEDED(mStatus))
        mPump->GetStatus(status);
    else
        *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::Cancel(nsresult status)
{
    PR_LOG(gGopherLog,
           PR_LOG_DEBUG,
           ("nsGopherChannel::Cancel() called [this=%x, status=%x]\n",
            this,status));
    
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
 
    mStatus = status;
    if (mPump)
        return mPump->Cancel(status);

    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::Suspend()
{
    if (mPump)
        return mPump->Suspend();

    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::Resume()
{
    if (mPump)
        return mPump->Resume();

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsGopherChannel::GetOriginalURI(nsIURI* *aURI)
{
    *aURI = mOriginalURI ? mOriginalURI : mUrl;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::SetOriginalURI(nsIURI* aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::GetURI(nsIURI* *aURI)
{
    *aURI = mUrl;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::Open(nsIInputStream **_retval)
{
    return NS_ImplementChannelOpen(this, _retval);
}

NS_IMETHODIMP
nsGopherChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
    PR_LOG(gGopherLog, PR_LOG_DEBUG, ("nsGopherChannel::AsyncOpen() called [this=%x]\n",
                                      this));

    nsresult rv;

    PRInt32 port;
    rv = mUrl->GetPort(&port);
    if (NS_FAILED(rv))
        return rv;
 
    rv = NS_CheckPortSafety(port, "gopher");
    if (NS_FAILED(rv))
        return rv;

    // push stream converters in front of the consumer...
    nsCOMPtr<nsIStreamListener> converter;
    rv = PushStreamConverters(aListener, getter_AddRefs(converter));
    if (NS_FAILED(rv)) return rv;
    
    // create socket transport
    nsCOMPtr<nsISocketTransportService> socketService = 
             do_GetService(kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = socketService->CreateTransport(nsnull, 0,
                                        mHost,
                                        mPort,
                                        mProxyInfo,
                                        getter_AddRefs(mTransport));
    if (NS_FAILED(rv)) return rv;

    // setup notification callbacks...
    if (!(mLoadFlags & LOAD_BACKGROUND)) {
        nsCOMPtr<nsIEventQueue> eventQ;
        NS_GetCurrentEventQ(getter_AddRefs(eventQ));
        if (eventQ)
            mTransport->SetEventSink(this, eventQ);
    }
    mTransport->SetSecurityCallbacks(mCallbacks);

    // open buffered, asynchronous socket input stream, and use a input stream
    // pump to read from it.
    nsCOMPtr<nsIInputStream> input;
    rv = mTransport->OpenInputStream(0, 0, 0, getter_AddRefs(input));
    if (NS_FAILED(rv)) return rv;

    rv = SendRequest();
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewInputStreamPump(getter_AddRefs(mPump), input);
    if (NS_FAILED(rv)) return rv;

    rv = mPump->AsyncRead(this, nsnull);
    if (NS_FAILED(rv)) return rv;

    if (mLoadGroup)
        mLoadGroup->AddRequest(this, nsnull);

    mIsPending = PR_TRUE;
    if (converter)
        mListener = converter;
    else
        mListener = aListener;
    mListenerContext = ctxt;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::GetLoadFlags(PRUint32 *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::SetLoadFlags(PRUint32 aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::GetContentType(nsACString &aContentType)
{
    if (!mContentType.IsEmpty()) {
        aContentType = mContentType;
        return NS_OK;
    }

    switch(mType) {
    case '0':
    case 'h':
        aContentType.AssignLiteral(TEXT_HTML);
        break;
    case '1':
        aContentType.AssignLiteral(APPLICATION_HTTP_INDEX_FORMAT);
        break;
    case '2': // CSO search - unhandled, should not be selectable
        aContentType.AssignLiteral(TEXT_HTML);
        break;
    case '3': // "Error" - should not be selectable
        aContentType.AssignLiteral(TEXT_HTML);
        break;
    case '4': // "BinHexed Macintosh file"
        aContentType.AssignLiteral(APPLICATION_BINHEX);
        break;
    case '5':
        // "DOS binary archive of some sort" - is the mime-type correct?
        aContentType.AssignLiteral(APPLICATION_OCTET_STREAM);
        break;
    case '6':
        aContentType.AssignLiteral(APPLICATION_UUENCODE);
        break;
    case '7': // search - returns a directory listing
        aContentType.AssignLiteral(APPLICATION_HTTP_INDEX_FORMAT);
        break;
    case '8': // telnet - type doesn't make sense
        aContentType.AssignLiteral(TEXT_PLAIN);
        break;
    case '9': // "Binary file!"
        aContentType.AssignLiteral(APPLICATION_OCTET_STREAM);
        break;
    case 'g':
        aContentType.AssignLiteral(IMAGE_GIF);
        break;
    case 'i': // info line- should not be selectable
        aContentType.AssignLiteral(TEXT_HTML);
        break;
    case 'I':
        aContentType.AssignLiteral(IMAGE_GIF);
        break;
    case 'T': // tn3270 - type doesn't make sense
        aContentType.AssignLiteral(TEXT_PLAIN);
        break;
    default:
        if (!mContentTypeHint.IsEmpty()) {
            aContentType = mContentTypeHint;
        } else {
            NS_WARNING("Unknown gopher type");
            aContentType.AssignLiteral(UNKNOWN_CONTENT_TYPE);
        }
        break;
    }

    PR_LOG(gGopherLog,PR_LOG_DEBUG,
           ("GetContentType returning %s\n", PromiseFlatCString(aContentType).get()));

    // XXX do we want to cache this result?
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::SetContentType(const nsACString &aContentType)
{
    if (mIsPending) {
        // only changes mContentCharset if a charset is parsed
        NS_ParseContentType(aContentType, mContentType, mContentCharset);
    } else {
        // We are being given a content-type hint.  Since we have no ways of
        // determining a charset on our own, just set mContentCharset from the
        // charset part of this.        
        nsCAutoString charsetBuf;
        NS_ParseContentType(aContentType, mContentTypeHint, mContentCharset);
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::GetContentCharset(nsACString &aContentCharset)
{
    aContentCharset = mContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::SetContentCharset(const nsACString &aContentCharset)
{
    // If someone gives us a charset hint we should just use that charset.
    // So we don't care when this is being called.
    mContentCharset = aContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::SetContentLength(PRInt32 aContentLength)
{
    NS_NOTREACHED("nsGopherChannel::SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGopherChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    *aNotificationCallbacks = mCallbacks.get();
    NS_IF_ADDREF(*aNotificationCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
    mCallbacks = aNotificationCallbacks;
    if (mCallbacks) {
        mPrompter = do_GetInterface(mCallbacks);
        mProgressSink = do_GetInterface(mCallbacks);
    }
    else {
        mPrompter = 0;
        mProgressSink = 0;
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsGopherChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    if (mTransport)
        return mTransport->GetSecurityInfo(aSecurityInfo);

    return NS_ERROR_NOT_AVAILABLE;
}

// nsIRequestObserver methods
NS_IMETHODIMP
nsGopherChannel::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
    PR_LOG(gGopherLog,
           PR_LOG_DEBUG,
           ("nsGopherChannel::OnStartRequest called [this=%x, aRequest=%x]\n",
            this, aRequest));

    return mListener->OnStartRequest(this, mListenerContext);
}

NS_IMETHODIMP
nsGopherChannel::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                               nsresult aStatus)
{
    PR_LOG(gGopherLog,
           PR_LOG_DEBUG,
           ("nsGopherChannel::OnStopRequest called [this=%x, aRequest=%x, aStatus=%x]\n",
            this,aRequest,aStatus));

    if (NS_SUCCEEDED(mStatus))
        mStatus = aStatus;

    if (mListener) {
        mListener->OnStopRequest(this, mListenerContext, mStatus);
        mListener = 0;
        mListenerContext = 0;
    }

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);
    
    mTransport->Close(mStatus);
    mTransport = 0;
    mPump = 0;
    return NS_OK;
}


// nsIStreamListener method
NS_IMETHODIMP
nsGopherChannel::OnDataAvailable(nsIRequest *req, nsISupports *ctx,
                                 nsIInputStream *stream,
                                 PRUint32 offset, PRUint32 count)
{
    PR_LOG(gGopherLog, PR_LOG_DEBUG,
           ("OnDataAvailable called - [this=%x, aLength=%d]\n",this,count));

    return mListener->OnDataAvailable(this, mListenerContext, stream,
                                      offset, count);
}

nsresult
nsGopherChannel::SendRequest()
{
    nsresult rv = NS_OK;

    // Note - you have to keep this as a class member, because the char input
    // stream doesn't copy its buffer
    mRequest.Assign(mSelector);

    // So, we use the selector as is unless it is a search url
    if (mType=='7') {
        // Note that we don't use the "standard" nsIURL parsing stuff here
        // because the only special character is ?, and its possible to search
        // for a string containing a #, and so on
        
        // XXX - should this find the last or first entry?
        // '?' is valid in both the search string and the url
        // so no matter what this does, it may be incorrect
        // This only affects people codeing the query directly into the URL
        PRInt32 pos = mRequest.RFindChar('?');
        if (pos == -1) {
            // We require a query string here - if we don't have one,
            // then we need to ask the user
            if (!mPrompter) {
                if (mLoadGroup) {
                    nsCOMPtr<nsIInterfaceRequestor> cbs;
                    rv = mLoadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
                    if (NS_SUCCEEDED(rv))
                        mPrompter = do_GetInterface(cbs);
                }
                if (!mPrompter) {
                    NS_ERROR("We need a prompter!");
                    return NS_ERROR_FAILURE;
                }
            }

            if (!mStringBundle) {

                nsCOMPtr<nsIStringBundleService> bundleSvc =
                        do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
                if (NS_FAILED(rv)) return rv;

                rv = bundleSvc->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(mStringBundle));
                if (NS_FAILED(rv)) return rv;

            }

            nsXPIDLString  promptTitle;
            nsXPIDLString  promptText;

            if (mStringBundle)
                rv = mStringBundle->GetStringFromName(NS_LITERAL_STRING("GopherPromptTitle").get(),
                                                      getter_Copies(promptTitle));

            if (NS_FAILED(rv) || !mStringBundle)
                promptTitle.AssignLiteral("Search");


            if (mStringBundle)
                rv = mStringBundle->GetStringFromName(NS_LITERAL_STRING("GopherPromptText").get(),
                                                      getter_Copies(promptText));

            if (NS_FAILED(rv) || !mStringBundle)
                promptText.AssignLiteral("Enter a search term:");


            nsXPIDLString search;
            PRBool res;
            mPrompter->Prompt(promptTitle.get(),
                              promptText.get(),
                              getter_Copies(search),
                              NULL,
                              NULL,
                              &res);
            if (!res || !(*search.get()))
                return NS_ERROR_FAILURE;
    
            mRequest.Append('\t');
            AppendUTF16toUTF8(search, mRequest); // XXX Is UTF-8 the right thing?

            // and update our uri
            nsCAutoString spec;
            rv = mUrl->GetAsciiSpec(spec);
            if (NS_FAILED(rv))
                return rv;

            spec.Append('?');
            AppendUTF16toUTF8(search, spec);
            rv = mUrl->SetSpec(spec);
            if (NS_FAILED(rv))
                return rv;
        } else {
            // Just replace it with a tab
            mRequest.SetCharAt('\t',pos);
        }
    }

    mRequest.Append(CRLF);
    
    PR_LOG(gGopherLog,PR_LOG_DEBUG,
           ("Sending: %s\n", mRequest.get()));

    // open a buffered, blocking output stream.  (it should never block because
    // the buffer is big enough for our entire request.)
    nsCOMPtr<nsIOutputStream> output;
    rv = mTransport->OpenOutputStream(nsITransport::OPEN_BLOCKING,
                                      mRequest.Length(), 1,
                                      getter_AddRefs(output));
    if (NS_FAILED(rv)) return rv;

    PRUint32 n;
    rv = output->Write(mRequest.get(), mRequest.Length(), &n);
    if (NS_FAILED(rv)) return rv;

    if (n != mRequest.Length())
        return NS_ERROR_UNEXPECTED;

    return NS_OK;
}

nsresult
nsGopherChannel::PushStreamConverters(nsIStreamListener *listener, nsIStreamListener **result)
{
    nsresult rv;
    nsCOMPtr<nsIStreamListener> converterListener;
    
    nsCOMPtr<nsIStreamConverterService> StreamConvService = 
             do_GetService(kStreamConverterServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
 
    // What we now do depends on what type of file we have
    if (mType=='1' || mType=='7') {
        // Send the directory format back for a directory
        rv = StreamConvService->AsyncConvertData("text/gopher-dir", 
               APPLICATION_HTTP_INDEX_FORMAT,
               listener,
               mUrl,
               getter_AddRefs(converterListener));
        if (NS_FAILED(rv)) return rv;
    } else if (mType=='0') {
        // Convert general file
        rv = StreamConvService->AsyncConvertData("text/plain",
                                                 "text/html",
                                                 listener,
                                                 mListenerContext,
                                                 getter_AddRefs(converterListener));
        if (NS_FAILED(rv)) return rv;
        
        nsCOMPtr<nsITXTToHTMLConv> converter(do_QueryInterface(converterListener));
        if (converter) {
            nsCAutoString spec;
            rv = mUrl->GetSpec(spec);
            converter->SetTitle(NS_ConvertUTF8toUCS2(spec).get());
            converter->PreFormatHTML(PR_TRUE);
        }
    }

    NS_IF_ADDREF(*result = converterListener);
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::OnTransportStatus(nsITransport *trans, nsresult status,
                                   PRUint32 progress, PRUint32 progressMax)
{
    // suppress status notification if channel is no longer pending!
    if (mProgressSink && NS_SUCCEEDED(mStatus) && mPump && !(mLoadFlags & LOAD_BACKGROUND)) {
        NS_ConvertUTF8toUCS2 host(mHost);
        mProgressSink->OnStatus(this, nsnull, status, host.get());

        if (status == nsISocketTransport::STATUS_RECEIVING_FROM ||
            status == nsISocketTransport::STATUS_SENDING_TO) {
            mProgressSink->OnProgress(this, nsnull, progress, progressMax);
        }
    }
    return NS_OK;
}
