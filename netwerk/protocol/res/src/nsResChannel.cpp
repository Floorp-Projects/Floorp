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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsResChannel.h"
#include "nsCRT.h"
#include "netCore.h"
#include "nsIServiceManager.h"
#include "nsIFileTransportService.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsXPIDLString.h"
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#if defined(PR_LOGGING)
//
// Log module for nsResChannel logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsResChannel:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
PRLogModuleInfo* gResChannelLog = nsnull;

#endif /* PR_LOGGING */

////////////////////////////////////////////////////////////////////////////////

nsResChannel::nsResChannel()
: mLoadFlags(nsIResChannel::LOAD_NORMAL),
      mState(QUIESCENT),
      mStatus(NS_OK)
#ifdef DEBUG
      ,mInitiator(nsnull)
#endif
{
    NS_INIT_REFCNT();
#if defined(PR_LOGGING)
    //
    // Initialize the global PRLogModule for socket transport logging
    // if necessary...
    //
    if (nsnull == gResChannelLog) {
        gResChannelLog = PR_NewLogModule("nsResChannel");
    }
#endif /* PR_LOGGING */
}

nsresult
nsResChannel::Init(nsIResProtocolHandler* handler, nsIURI* uri)
{
    mHandler = handler;
    mResourceURI = uri;
    return NS_OK;
}

nsResChannel::~nsResChannel()
{
}


NS_IMPL_THREADSAFE_ADDREF(nsResChannel)
NS_IMPL_THREADSAFE_RELEASE(nsResChannel)

NS_INTERFACE_MAP_BEGIN(nsResChannel)
    NS_INTERFACE_MAP_ENTRY(nsIResChannel)
    NS_INTERFACE_MAP_ENTRY(nsIFileChannel)
    NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
    NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIResChannel)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIRequest, nsIResChannel)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIChannel, nsIResChannel)
NS_INTERFACE_MAP_END_THREADSAFE

NS_METHOD
nsResChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsResChannel* fc = new nsResChannel();
    if (fc == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(fc);
    nsresult rv = fc->QueryInterface(aIID, aResult);
    NS_RELEASE(fc);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsResChannel::Substitutions
////////////////////////////////////////////////////////////////////////////////

nsresult
nsResChannel::Substitutions::Init()
{
    nsresult rv;
    nsResChannel* channel = GET_SUBSTITUTIONS_CHANNEL(this);
    
    if (mSubstitutions)
        return NS_ERROR_FAILURE;

    char* root;
    rv = channel->mResourceURI->GetHost(&root);
    if (NS_SUCCEEDED(rv)) {
		char* strRoot = root;
		if (strRoot == nsnull) strRoot = "";	// don't pass null to GetSubstitutions
        rv = channel->mHandler->GetSubstitutions(strRoot, getter_AddRefs(mSubstitutions));
        nsCRT::free(root);
    }
    return rv;
}

nsresult
nsResChannel::Substitutions::Next(char* *result)
{
    nsresult rv;
    nsResChannel* channel = GET_SUBSTITUTIONS_CHANNEL(this);

    nsCOMPtr<nsIURI> substURI;
    rv = mSubstitutions->GetElementAt(mCurrentIndex++, getter_AddRefs(substURI));
    if (NS_FAILED(rv)) return rv;

    if (substURI == nsnull)
        return NS_ERROR_FAILURE;

    char* path = nsnull;
    rv = channel->mResourceURI->GetPath(&path);
    if (NS_FAILED(rv)) return rv;

    // XXX this path[0] check is a hack -- it seems to me that GetPath 
    // shouldn't include the leading slash:
    char* aResolvedURI;
    rv = substURI->Resolve(path[0] == '/' ? path+1 : path, &aResolvedURI);
    nsCRT::free(path);
    if (NS_FAILED(rv)) return rv;
    
    *result = aResolvedURI;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsResChannel::GetName(PRUnichar* *result)
{
    nsresult rv;
    nsXPIDLCString urlStr;
    rv = mResourceURI->GetSpec(getter_Copies(urlStr));
    if (NS_FAILED(rv)) return rv;
    nsString name;
    name.AppendWithConversion(urlStr);
    *result = name.ToNewUnicode();
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsResChannel::IsPending(PRBool *result)
{
    if (mResolvedChannel)
        return mResolvedChannel->IsPending(result);
    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
#endif
    nsresult rv = NS_OK;
    if (mResolvedChannel) {
        rv = mResolvedChannel->Cancel(status);
    }
    mStatus = status;
    mResolvedChannel = nsnull;        // remove the resolution
    return rv;
}

NS_IMETHODIMP
nsResChannel::Suspend()
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
#endif
    if (mResolvedChannel)
        return mResolvedChannel->Suspend();
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::Resume()
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
#endif
    if (mResolvedChannel)
        return mResolvedChannel->Resume();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsResChannel::GetOriginalURI(nsIURI* *aURI)
{
    *aURI = mOriginalURI ? mOriginalURI : mResourceURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::SetOriginalURI(nsIURI* aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::GetURI(nsIURI* *aURI)
{
    if (mResolvedChannel) {
        return mResolvedChannel->GetURI(aURI);
    }
    *aURI = mResourceURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

nsresult
nsResChannel::EnsureNextResolvedChannel()
{
    nsresult rv;
    nsXPIDLCString resolvedURI;

    nsCOMPtr<nsIIOService> serv(do_GetService(kIOServiceCID, &rv));
    if (NS_FAILED(rv)) goto done;

    rv = mSubstitutions.Next(getter_Copies(resolvedURI));
    if (NS_FAILED(rv)) goto done;

    rv = serv->NewChannel(resolvedURI, nsnull,
                          getter_AddRefs(mResolvedChannel));
    if (NS_FAILED(rv)) {
        rv = NS_OK;     // returning NS_OK lets us try again
        goto done;
    }

    if (mOriginalURI) {
        rv = mResolvedChannel->SetOriginalURI(mOriginalURI);
        if (NS_FAILED(rv)) goto done;
    }
    if (mLoadGroup) {
        rv = mResolvedChannel->SetLoadGroup(mLoadGroup);
        if (NS_FAILED(rv)) goto done;
    }
    if (mLoadFlags != nsIResChannel::LOAD_NORMAL) {
        rv = mResolvedChannel->SetLoadFlags(mLoadFlags);
        if (NS_FAILED(rv)) goto done;
    }
    if (mCallbacks) {
        rv = mResolvedChannel->SetNotificationCallbacks(mCallbacks);
        if (NS_FAILED(rv)) goto done;
    }
    
  done:
#if defined(PR_LOGGING)
    nsXPIDLCString resURI;
    (void)mResourceURI->GetSpec(getter_Copies(resURI));
    PR_LOG(gResChannelLog, PR_LOG_DEBUG,
           ("nsResChannel: resolving %s ",
            (const char*)resURI));
    PR_LOG(gResChannelLog, PR_LOG_DEBUG,
           ("                     to %s => status %x\n",
            (const char*)resolvedURI, rv));
#endif /* PR_LOGGING */
    return rv;
}

NS_IMETHODIMP
nsResChannel::Open(nsIInputStream **result)
{
    nsresult rv;

    if (mResolvedChannel)
        return NS_ERROR_IN_PROGRESS;

    rv = mSubstitutions.Init();
    if (NS_FAILED(rv)) return rv;

    do {
        rv = EnsureNextResolvedChannel();
        if (NS_FAILED(rv)) break;

        if (mResolvedChannel)
            rv = mResolvedChannel->Open(result);
    } while (NS_FAILED(rv));

    return rv;
}

// XXX What does OpenOutputStream mean for a res "channel"
#if 0
NS_IMETHODIMP
nsResChannel::OpenOutputStream(PRUint32 transferOffset, PRUint32 transferCount, nsIOutputStream **result)
{
    nsresult rv;

    if (mResolvedChannel)
        return NS_ERROR_IN_PROGRESS;

    rv = mSubstitutions.Init();
    if (NS_FAILED(rv)) return rv;

    do {
        rv = EnsureNextResolvedChannel();
        if (NS_FAILED(rv)) break;

        if (mResolvedChannel)
            rv = mResolvedChannel->OpenOutputStream(transferOffset, transferCount, result);
    } while (NS_FAILED(rv));

    return rv;
}
#endif

NS_IMETHODIMP
nsResChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
    nsresult rv;

#ifdef DEBUG
    NS_ASSERTION(mInitiator == nsnull || mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
    mInitiator = PR_CurrentThread();
#endif

    switch (mState) {
      case QUIESCENT:
        if (mResolvedChannel)
            return NS_ERROR_IN_PROGRESS;

        // first time through
        rv = mSubstitutions.Init();
        if (NS_FAILED(rv)) return rv;
        // fall through
        mState = ASYNC_READ;

      case ASYNC_READ:
        break;

      default:
        return NS_ERROR_IN_PROGRESS;
    }
    NS_ASSERTION(mState == ASYNC_READ, "wrong state");

    mUserContext = ctxt;
    mUserObserver = listener;   // cast back to nsIStreamListener later

    do {
        rv = EnsureNextResolvedChannel();
        if (NS_FAILED(rv)) break;

        if (mResolvedChannel)
            rv = mResolvedChannel->AsyncOpen(this, nsnull);
        // Later, this AsyncRead will call back our OnStopRequest
        // method. The action resumes there...
    } while (NS_FAILED(rv));

    if (NS_FAILED(rv))
        (void)EndRequest(rv);

    return rv;
}

NS_IMETHODIMP
nsResChannel::GetLoadFlags(PRUint32 *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::SetLoadFlags(PRUint32 aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::GetContentType(char * *aContentType)
{
    if (mResolvedChannel)
        return mResolvedChannel->GetContentType(aContentType);
    
    // if we have not created a mResolvedChannel, use the mime service
    nsCOMPtr<nsIMIMEService> MIMEService (do_GetService(NS_MIMESERVICE_CONTRACTID));
    if (!MIMEService) return NS_ERROR_FAILURE;
    return MIMEService->GetTypeFromURI(mResourceURI, aContentType);
}

NS_IMETHODIMP
nsResChannel::SetContentType(const char *aContentType)
{
    if (mResolvedChannel)
        return mResolvedChannel->SetContentType(aContentType);

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsResChannel::GetContentLength(PRInt32 *aContentLength)
{
    if (mResolvedChannel)
        return mResolvedChannel->GetContentLength(aContentLength);

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsResChannel::SetContentLength(PRInt32 aContentLength)
{
    NS_NOTREACHED("nsResChannel::SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsResChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    *aNotificationCallbacks = mCallbacks.get();
    NS_IF_ADDREF(*aNotificationCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
    mCallbacks = aNotificationCallbacks;
    return NS_OK;
}

NS_IMETHODIMP 
nsResChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    NS_NOTREACHED("nsResChannel::GetSecurityInfo");
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsResChannel::OnStartRequest(nsIRequest* request, nsISupports* context)
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
#endif
    NS_ASSERTION(mUserObserver, "No observer...");
    return mUserObserver->OnStartRequest(NS_STATIC_CAST(nsIResChannel*, this), mUserContext);
}

NS_IMETHODIMP
nsResChannel::OnStopRequest(nsIRequest* request, nsISupports* context,
                            nsresult aStatus)
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
#endif
    if (NS_FAILED(aStatus) && aStatus != NS_BINDING_ABORTED) {
        nsCOMPtr<nsIRequest> dummyRequest; 

        // if we failed to process this channel, then try the next one:
        switch (mState) {
          case ASYNC_READ: 
            return AsyncOpen(GetUserListener(), mUserContext);
          default:
            break;
        }
    }
    return EndRequest(aStatus);
}

nsresult
nsResChannel::EndRequest(nsresult aStatus)
{
    nsresult rv;
    rv = mUserObserver->OnStopRequest(NS_STATIC_CAST(nsIResChannel*, this),
                                      mUserContext, aStatus);
#if 0 // we don't add the resource channel to the group (although maybe we should)
    if (mLoadGroup) {
        if (NS_SUCCEEDED(rv)) {
            mLoadGroup->RemoveChannel(this, context, notif);
        }
    }
#endif
    // Release the reference to the consumer stream listener...
    mUserObserver = 0;
    mUserContext = 0;
    mResolvedChannel = 0;
    return rv;
}

NS_IMETHODIMP
nsResChannel::OnDataAvailable(nsIRequest* request, nsISupports* context,
                               nsIInputStream *aIStream, PRUint32 aSourceOffset,
                               PRUint32 aLength)
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
#endif
    return GetUserListener()->OnDataAvailable(NS_STATIC_CAST(nsIResChannel*, this),
                                              mUserContext, aIStream,
                                              aSourceOffset, aLength);
}

////////////////////////////////////////////////////////////////////////////////


/* void init (in nsIFile file, in long ioFlags, in long perm); */
NS_IMETHODIMP nsResChannel::Init(nsIFile *file, PRInt32 ioFlags, PRInt32 perm)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIFile file; */
NS_IMETHODIMP nsResChannel::GetFile(nsIFile * *result)
{
    nsresult rv;
    rv = mSubstitutions.Init();
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFile> file;
    do {
        rv = EnsureNextResolvedChannel();
        if (NS_FAILED(rv)) break;

        if (mResolvedChannel) {
            nsCOMPtr<nsIFileChannel> fc(do_QueryInterface(mResolvedChannel));
            if (fc) {
                rv = fc->GetFile(getter_AddRefs(file));
                if (file) {
                    PRBool exists;
                    rv = file->Exists(&exists);
                    if (NS_SUCCEEDED(rv) && exists) {
                        *result = file;
                        NS_ADDREF(*result);
                        return NS_OK;
                    }
                }
            }
        }
    } while (NS_FAILED(rv));

    *result = nsnull;
    return NS_ERROR_FAILURE;
}

/* attribute long ioFlags; */
NS_IMETHODIMP nsResChannel::GetIoFlags(PRInt32 *aIoFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsResChannel::SetIoFlags(PRInt32 aIoFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long permissions; */
NS_IMETHODIMP nsResChannel::GetPermissions(PRInt32 *aPermissions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsResChannel::SetPermissions(PRInt32 aPermissions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
