/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Gopher protocol code.
 *
 * The Initial Developer of the Original Code is Bradley Baetz.
 * Portions created by Bradley Baetz are Copyright (C) 2000 Bradley Baetz.
 * All Rights Reserved.
 *
 * Contributor(s): 
 *  Bradley Baetz <bbaetz@student.usyd.edu.au>
 */

// gopher implementation - based on datetime and finger implimentations
// and documentation

#include "nsGopherChannel.h"
#include "nsIServiceManager.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsISocketTransportService.h"
#include "nsIStringStream.h"
#include "nsMimeTypes.h"
#include "nsIStreamConverterService.h"
#include "nsEscape.h"
#include "nsITXTToHTMLConv.h"
#include "nsIProgressEventSink.h"
#include "nsNetUtil.h"
#include "prlog.h"
#include "nsIPref.h"
#include "nsCRT.h"

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
      mActAsObserver(PR_TRUE),
      mListFormat(FORMAT_HTML),
      mType(-1),
      mStatus(NS_OK)
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
                              nsIDirectoryListing);

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


NS_METHOD
nsGopherChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsGopherChannel* gc = new nsGopherChannel();
    if (!gc)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(gc);
    nsresult rv = gc->QueryInterface(aIID, aResult);
    NS_RELEASE(gc);
    return rv;
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
    NS_NOTREACHED("nsGopherChannel::IsPending");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGopherChannel::GetStatus(nsresult *status)
{
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
    if (mTransportRequest) {
        return mTransportRequest->Cancel(status);
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGopherChannel::Suspend(void)
{
    NS_NOTREACHED("nsGopherChannel::Suspend");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGopherChannel::Resume(void)
{
    NS_NOTREACHED("nsGopherChannel::Resume");
    return NS_ERROR_NOT_IMPLEMENTED;
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
    nsresult rv = NS_OK;
    
    PRInt32 port;
    rv = mUrl->GetPort(&port);
    if (NS_FAILED(rv))
        return rv;
 
    rv = NS_CheckPortSafety(port, "gopher");
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsISocketTransportService> socketService = 
             do_GetService(kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = socketService->CreateTransport(mHost,
                                        mPort,
                                        mProxyInfo,
                                        BUFFER_SEG_SIZE,
                                        BUFFER_MAX_SIZE,
                                        getter_AddRefs(mTransport));
    
    if (NS_FAILED(rv)) return rv;
    
    mTransport->SetNotificationCallbacks(mCallbacks,
                                         (mLoadFlags & LOAD_BACKGROUND));
    
    return mTransport->OpenInputStream(0, (PRUint32)-1, 0, _retval);
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
    
    mListener = aListener;
    mResponseContext = ctxt;

    nsCOMPtr<nsISocketTransportService> socketService = 
             do_GetService(kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = socketService->CreateTransport(mHost,
                                        mPort,
                                        mProxyInfo,
                                        BUFFER_SEG_SIZE,
                                        BUFFER_MAX_SIZE,
                                        getter_AddRefs(mTransport));
    if (NS_FAILED(rv)) return rv;

    mTransport->SetNotificationCallbacks(mCallbacks,
                                         (mLoadFlags & LOAD_BACKGROUND));    
    
    return SendRequest(mTransport);
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
        aContentType = NS_LITERAL_CSTRING(TEXT_HTML);
        break;
    case '1':
        switch (mListFormat) {
        case nsIDirectoryListing::FORMAT_RAW:
            aContentType = NS_LITERAL_CSTRING("text/gopher-dir");
            break;
        default:
            NS_WARNING("Unknown directory type");
            // fall through
        case nsIDirectoryListing::FORMAT_HTML:
            aContentType = NS_LITERAL_CSTRING(TEXT_HTML);
            break;
        case nsIDirectoryListing::FORMAT_HTTP_INDEX:
            aContentType = NS_LITERAL_CSTRING(APPLICATION_HTTP_INDEX_FORMAT);
            break;
        }
        break;
    case '2': // CSO search - unhandled, should not be selectable
        aContentType = NS_LITERAL_CSTRING(TEXT_HTML);
        break;
    case '3': // "Error" - should not be selectable
        aContentType = NS_LITERAL_CSTRING(TEXT_HTML);
        break;
    case '4': // "BinHexed Macintosh file"
        aContentType = NS_LITERAL_CSTRING(APPLICATION_BINHEX);
        break;
    case '5':
        // "DOS binary archive of some sort" - is the mime-type correct?
        aContentType = NS_LITERAL_CSTRING(APPLICATION_OCTET_STREAM);
        break;
    case '6':
        aContentType = NS_LITERAL_CSTRING(APPLICATION_UUENCODE);
        break;
    case '7': // search - returns a directory listing
        aContentType = NS_LITERAL_CSTRING(APPLICATION_HTTP_INDEX_FORMAT);
        break;
    case '8': // telnet - type doesn't make sense
        aContentType = NS_LITERAL_CSTRING(TEXT_PLAIN);
        break;
    case '9': // "Binary file!"
        aContentType = NS_LITERAL_CSTRING(APPLICATION_OCTET_STREAM);
        break;
    case 'g':
        aContentType = NS_LITERAL_CSTRING(IMAGE_GIF);
        break;
    case 'i': // info line- should not be selectable
        aContentType = NS_LITERAL_CSTRING(TEXT_HTML);
        break;
    case 'I':
        aContentType = NS_LITERAL_CSTRING(IMAGE_GIF);
        break;
    case 'T': // tn3270 - type doesn't make sense
        aContentType = NS_LITERAL_CSTRING(TEXT_PLAIN);
        break;
    default:
        NS_WARNING("Unknown gopher type");
        aContentType = NS_LITERAL_CSTRING(UNKNOWN_CONTENT_TYPE);
    }

    PR_LOG(gGopherLog,PR_LOG_DEBUG,
           ("GetContentType returning %s\n", PromiseFlatCString(aContentType).get()));

    // XXX do we want to cache this result?
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::SetContentType(const nsACString &aContentType)
{
    // only changes mContentCharset if a charset is parsed
    NS_ParseContentType(aContentType, mContentType, mContentCharset);
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
        mCallbacks->GetInterface(NS_GET_IID(nsIPrompt),
                                 getter_AddRefs(mPrompter));
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsGopherChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

// nsIRequestObserver methods
NS_IMETHODIMP
nsGopherChannel::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
    PR_LOG(gGopherLog,
           PR_LOG_DEBUG,
           ("nsGopherChannel::OnStartRequest called [this=%x, aRequest=%x]\n",
            this, aRequest));

    if (!mActAsObserver) {
        // acting as a listener
        return mListener->OnStartRequest(this, aContext);
    } else {
        // we don't want to pass our AsyncWrite's OnStart through
        // we just ignore this
        return NS_OK;
    }
}

NS_IMETHODIMP
nsGopherChannel::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                               nsresult aStatus)
{
    PR_LOG(gGopherLog,
           PR_LOG_DEBUG,
           ("nsGopherChannel::OnStopRequest called [this=%x, aRequest=%x, aStatus=%x]\n",
            this,aRequest,aStatus));

    nsresult rv = NS_OK;

    if (NS_FAILED(aStatus) || !mActAsObserver) {
        if (mListener) {
            rv = mListener->OnStopRequest(this, mResponseContext, aStatus);
            if (NS_FAILED(rv)) return rv;
        }

        if (mLoadGroup) {
            rv = mLoadGroup->RemoveRequest(this, nsnull, aStatus);
        }
        
        mTransport = 0;
        return rv;
    } else {
        // at this point we know the request has been sent.
        // we're no longer acting as an observer.
        mActAsObserver = PR_FALSE;

        nsCOMPtr<nsIStreamListener> converterListener;
        
        nsCOMPtr<nsIStreamConverterService> StreamConvService = 
                 do_GetService(kStreamConverterServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
     
        // What we now do depends on what type of file we have
        if (mType=='1' || mType=='7') {
            // Send the directory format back for a directory
            switch (mListFormat) {
            case nsIDirectoryListing::FORMAT_RAW:
                converterListener = this;
                break;
            default:
                // fall through
            case nsIDirectoryListing::FORMAT_HTML:
                // XXX - work arround bug 126417. We have to do the chaining
                // manually so that we don't crash
                {
                    nsCOMPtr<nsIStreamListener> tmpListener;
                    rv = StreamConvService->AsyncConvertData(
                           NS_LITERAL_STRING(APPLICATION_HTTP_INDEX_FORMAT).get(),
                           NS_LITERAL_STRING(TEXT_HTML).get(),
                           this,
                           mUrl, 
                           getter_AddRefs(tmpListener));
                    if (NS_FAILED(rv)) break;
                    rv = StreamConvService->AsyncConvertData(NS_LITERAL_STRING("text/gopher-dir").get(),
                           NS_LITERAL_STRING(APPLICATION_HTTP_INDEX_FORMAT).get(),
                           tmpListener,
                           mUrl,
                           getter_AddRefs(converterListener));
                }
            break;
            case nsIDirectoryListing::FORMAT_HTTP_INDEX:
                rv = StreamConvService->AsyncConvertData(NS_LITERAL_STRING("text/gopher-dir").get(), 
                       NS_LITERAL_STRING(APPLICATION_HTTP_INDEX_FORMAT).get(),
                       this,
                       mUrl,
                       getter_AddRefs(converterListener));
                break;
            }
            if (NS_FAILED(rv)) return rv;
        } else if (mType=='0') {
            // Convert general file
            rv = StreamConvService->AsyncConvertData(NS_LITERAL_STRING("text/plain").get(),
                                                     NS_LITERAL_STRING("text/html").get(),
                                                     this,
                                                     mResponseContext,
                                                     getter_AddRefs(converterListener));
            if (NS_FAILED(rv)) return rv;
            
            nsCOMPtr<nsITXTToHTMLConv> converter(do_QueryInterface(converterListener));
            if (converter) {
                nsCAutoString spec;
                rv = mUrl->GetSpec(spec);
                converter->SetTitle(NS_ConvertUTF8toUCS2(spec).get());
                converter->PreFormatHTML(PR_TRUE);
            }
        } else 
            // other file type - no conversions
            converterListener = this;
        
        return mTransport->AsyncRead(converterListener, mResponseContext, 0,
                                     (PRUint32)-1, 0, getter_AddRefs(mTransportRequest));
    }
}

// nsIStreamListener method
NS_IMETHODIMP
nsGopherChannel::OnDataAvailable(nsIRequest* aRequest, nsISupports* aContext,
                                 nsIInputStream *aInputStream,
                                 PRUint32 aSourceOffset, PRUint32 aLength)
{
    PR_LOG(gGopherLog, PR_LOG_DEBUG,
           ("OnDataAvailable called - [this=%x, aLength=%d]\n",this,aLength));
    mContentLength = aLength;
    return mListener->OnDataAvailable(this, aContext, aInputStream,
                                      aSourceOffset, aLength);
}

nsresult
nsGopherChannel::SendRequest(nsITransport* aTransport)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsISupports> result;
    nsCOMPtr<nsIInputStream> charstream;

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
                promptTitle.Assign(NS_LITERAL_STRING("Search"));


            if (mStringBundle)
                rv = mStringBundle->GetStringFromName(NS_LITERAL_STRING("GopherPromptText").get(),
                                                      getter_Copies(promptText));

            if (NS_FAILED(rv) || !mStringBundle)
                promptText.Assign(NS_LITERAL_STRING("Enter a search term:"));


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
    
            if (mLoadGroup) {
                rv = mLoadGroup->AddRequest(this, nsnull);
                if (NS_FAILED(rv)) return rv;
            }
            
            mRequest.Append('\t');
            mRequest.AppendWithConversion(search.get());

            // and update our uri
            nsCAutoString spec;
            rv = mUrl->GetAsciiSpec(spec);
            if (NS_FAILED(rv))
                return rv;

            spec.Append('?');
            spec.AppendWithConversion(search.get());
            rv = mUrl->SetSpec(spec);
            if (NS_FAILED(rv))
                return rv;
        } else {
            // Just replace it with a tab
            mRequest.SetCharAt('\t',pos);
        }
    }

    mRequest.Append(CRLF);
    
    rv = NS_NewCharInputStream(getter_AddRefs(result), mRequest.get());
    if (NS_FAILED(rv)) return rv;

    charstream = do_QueryInterface(result, &rv);
    if (NS_FAILED(rv)) return rv;

    PR_LOG(gGopherLog,PR_LOG_DEBUG,
           ("Sending: %s\n", mRequest.get()));

    rv = NS_AsyncWriteFromStream(getter_AddRefs(mTransportRequest),
                                 aTransport, charstream,
                                 0, mRequest.Length(), 0,
                                 this, nsnull);
    return rv;
}

NS_IMETHODIMP
nsGopherChannel::SetListFormat(PRUint32 format) {
    if (format != FORMAT_PREF &&
        format != FORMAT_RAW &&
        format != FORMAT_HTML &&
        format != FORMAT_HTTP_INDEX) {
        return NS_ERROR_FAILURE;
    }

    // Convert the pref value
    if (format == FORMAT_PREF) {
        nsresult rv;
        nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
        PRInt32 sFormat;
        rv = prefs->GetIntPref("network.dir.format", &sFormat);
        if (NS_FAILED(rv))
            format = FORMAT_HTML; // default
        else
            format = sFormat;

        if (format == FORMAT_PREF) {
            NS_WARNING("Who set the directory format pref to 'read from prefs'??");
            return NS_ERROR_FAILURE;
        }
    }
    
    mListFormat = format;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::GetListFormat(PRUint32 *format) {
    *format = mListFormat;
    return NS_OK;
}

