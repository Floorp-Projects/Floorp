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
      mType(-1),
      mStatus(NS_OK)
{
    NS_INIT_REFCNT();
}

nsGopherChannel::~nsGopherChannel()
{
#ifdef PR_LOGGING
    nsXPIDLCString spec;
    mUrl->GetSpec(getter_Copies(spec));
    PR_LOG(gGopherLog, PR_LOG_ALWAYS, ("~nsGopherChannel() for %s", spec.get()));
#endif
}

NS_IMPL_THREADSAFE_ISUPPORTS4(nsGopherChannel,
                              nsIChannel,
                              nsIRequest,
                              nsIStreamListener,
                              nsIRequestObserver);

nsresult
nsGopherChannel::Init(nsIURI* uri, nsIProxyInfo* proxyInfo)
{
    nsresult rv;

    nsCOMPtr<nsIURL> url = do_QueryInterface(uri, &rv);
    if (NS_FAILED(rv))
        return NS_ERROR_MALFORMED_URI;
    
    mUrl = uri;
    mProxyInfo = proxyInfo;
    
    nsXPIDLCString buffer;

    rv = url->GetPath(getter_Copies(buffer));
    if (NS_FAILED(rv))
        return rv;

    rv = url->GetHost(getter_Copies(mHost));
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
        mSelector.Adopt(nsCRT::strdup(""));
    } else {
        mType = buffer[1]; // Ignore leading '/'
        mSelector.Adopt(nsCRT::strdup(nsUnescape(NS_CONST_CAST(char*,buffer.get()+2))));
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
nsGopherChannel::GetName(PRUnichar* *result)
{
    nsString name;
    name.AppendWithConversion(mHost);
    name.AppendWithConversion(":");
    name.AppendInt(mPort);
    *result = ToNewUnicode(name);
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
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
nsGopherChannel::GetContentType(char* *aContentType)
{
    if (!aContentType) return NS_ERROR_NULL_POINTER;

    if (!mContentType.IsEmpty()) {
        *aContentType = ToNewCString(mContentType);
        return NS_OK;
    }

    switch(mType) {
    case '0':
        *aContentType = nsCRT::strdup(TEXT_HTML);
        break;
    case '1':
        *aContentType = nsCRT::strdup(APPLICATION_HTTP_INDEX_FORMAT);
        break;
    case '2': // CSO search - unhandled, should not be selectable
        *aContentType = nsCRT::strdup(TEXT_HTML);
        break;
    case '3': // "Error" - should not be selectable
        *aContentType = nsCRT::strdup(TEXT_HTML);
        break;
    case '4': // "BinHexed Macintosh file"
        *aContentType = nsCRT::strdup(APPLICATION_BINHEX);
        break;
    case '5':
        // "DOS binary archive of some sort" - is the mime-type correct?
        *aContentType = nsCRT::strdup(APPLICATION_OCTET_STREAM);
        break;
    case '6':
        *aContentType = nsCRT::strdup(APPLICATION_UUENCODE);
        break;
    case '7': // search - returns a directory listing
        *aContentType = nsCRT::strdup(APPLICATION_HTTP_INDEX_FORMAT);
        break;
    case '8': // telnet - type doesn't make sense
        *aContentType = nsCRT::strdup(TEXT_PLAIN);
        break;
    case '9': // "Binary file!"
        *aContentType = nsCRT::strdup(APPLICATION_OCTET_STREAM);
        break;
    case 'g':
        *aContentType = nsCRT::strdup(IMAGE_GIF);
        break;
    case 'i': // info line- should not be selectable
        *aContentType = nsCRT::strdup(TEXT_HTML);
        break;
    case 'I':
        *aContentType = nsCRT::strdup(IMAGE_GIF);
        break;
    case 'T': // tn3270 - type doesn't make sense
        *aContentType = nsCRT::strdup(TEXT_PLAIN);
        break;
    default:
        NS_WARNING("Unknown gopher type");
        *aContentType = nsCRT::strdup(UNKNOWN_CONTENT_TYPE);
    }
    if (!*aContentType)
        return NS_ERROR_OUT_OF_MEMORY;

    PR_LOG(gGopherLog,PR_LOG_DEBUG,
           ("GetContentType returning %s\n",*aContentType));

    return NS_OK;
}

NS_IMETHODIMP
nsGopherChannel::SetContentType(const char *aContentType)
{
    mContentType.Assign(aContentType);
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
            rv = StreamConvService->AsyncConvertData(NS_LITERAL_STRING("text/gopher-dir").get(),
                                                     NS_LITERAL_STRING("application/http-index-format").get(),
                                                     this,
                                                     mUrl,
                                                     getter_AddRefs(converterListener));
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
                nsXPIDLCString spec;
                rv = mUrl->GetSpec(getter_Copies(spec));
                nsUnescape(NS_CONST_CAST(char*, spec.get()));
                converter->SetTitle(NS_ConvertASCIItoUCS2(spec).get());
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
                NS_ERROR("We need a prompter!");
                return NS_ERROR_FAILURE;
            }
            nsXPIDLString search;
            PRBool res;
            mPrompter->Prompt(NS_LITERAL_STRING("Search").get(),
                              NS_LITERAL_STRING("Enter a search term:").get(),
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
            nsXPIDLCString spec;
            rv = mUrl->GetSpec(getter_Copies(spec));
            if (NS_FAILED(rv))
                return rv;

            nsCString strSpec(spec.get());
            strSpec.Append('?');
            strSpec.AppendWithConversion(search.get());
            rv = mUrl->SetSpec(strSpec.get());
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

