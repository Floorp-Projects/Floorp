/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include "nsJARProtocolHandler.h"
#include "nsIIOService.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsJARURI.h"
#include "nsIURL.h"
#include "nsJARChannel.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsNetCID.h"
#include "nsIMIMEService.h"
#include "nsMimeTypes.h"
#include "nsIRemoteOpenFileListener.h"
#include "nsIHashable.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nsTArray.h"

static NS_DEFINE_CID(kZipReaderCacheCID, NS_ZIPREADERCACHE_CID);

#define NS_JAR_CACHE_SIZE 32

//-----------------------------------------------------------------------------

nsJARProtocolHandler *gJarHandler = nullptr;

nsJARProtocolHandler::nsJARProtocolHandler()
: mIsMainProcess(XRE_IsParentProcess())
{
    MOZ_ASSERT(NS_IsMainThread());
}

nsJARProtocolHandler::~nsJARProtocolHandler()
{
    MOZ_ASSERT(gJarHandler == this);
    gJarHandler = nullptr;
}

nsresult
nsJARProtocolHandler::Init()
{
    nsresult rv;

    mJARCache = do_CreateInstance(kZipReaderCacheCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = mJARCache->Init(NS_JAR_CACHE_SIZE);
    return rv;
}

nsIMIMEService * 
nsJARProtocolHandler::MimeService()
{
    if (!mMimeService)
        mMimeService = do_GetService("@mozilla.org/mime;1");

    return mMimeService.get();
}

bool
nsJARProtocolHandler::RemoteOpenFileInProgress(
                                           nsIHashable *aRemoteFile,
                                           nsIRemoteOpenFileListener *aListener)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aRemoteFile);
    MOZ_ASSERT(aListener);

    if (IsMainProcess()) {
        MOZ_CRASH("Shouldn't be called in the main process!");
    }

    RemoteFileListenerArray *listeners;
    if (mRemoteFileListeners.Get(aRemoteFile, &listeners)) {
        listeners->AppendElement(aListener);
        return true;
    }

    // We deliberately don't put the listener in the new array since the first
    // load is handled differently.
    mRemoteFileListeners.Put(aRemoteFile, new RemoteFileListenerArray());
    return false;
}

void
nsJARProtocolHandler::RemoteOpenFileComplete(nsIHashable *aRemoteFile,
                                             nsresult aStatus)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aRemoteFile);

    if (IsMainProcess()) {
        MOZ_CRASH("Shouldn't be called in the main process!");
    }

    RemoteFileListenerArray *tempListeners;
    if (!mRemoteFileListeners.Get(aRemoteFile, &tempListeners)) {
        return;
    }

    // Save the listeners in a stack array. The call to Remove() below will
    // delete the tempListeners array.
    RemoteFileListenerArray listeners;
    tempListeners->SwapElements(listeners);

    mRemoteFileListeners.Remove(aRemoteFile);

    // Technically we must fail OnRemoteFileComplete() since OpenNSPRFileDesc()
    // won't succeed here. We've trained nsJARChannel to recognize
    // NS_ERROR_ALREADY_OPENED in this case as "proceed to JAR cache hit."
    nsresult status = NS_SUCCEEDED(aStatus) ? NS_ERROR_ALREADY_OPENED : aStatus;

    uint32_t count = listeners.Length();
    for (uint32_t index = 0; index < count; index++) {
        listeners[index]->OnRemoteFileOpenComplete(status);
    }
}

NS_IMPL_ISUPPORTS(nsJARProtocolHandler,
                  nsIJARProtocolHandler,
                  nsIProtocolHandler,
                  nsISupportsWeakReference)

nsJARProtocolHandler*
nsJARProtocolHandler::GetSingleton()
{
    if (!gJarHandler) {
        gJarHandler = new nsJARProtocolHandler();
        if (!gJarHandler)
            return nullptr;

        NS_ADDREF(gJarHandler);
        nsresult rv = gJarHandler->Init();
        if (NS_FAILED(rv)) {
            NS_RELEASE(gJarHandler);
            return nullptr;
        }
    }
    NS_ADDREF(gJarHandler);
    return gJarHandler;
}

NS_IMETHODIMP
nsJARProtocolHandler::GetJARCache(nsIZipReaderCache* *result)
{
    *result = mJARCache;
    NS_ADDREF(*result);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsJARProtocolHandler::GetScheme(nsACString &result)
{
    result.AssignLiteral("jar");
    return NS_OK;
}

NS_IMETHODIMP
nsJARProtocolHandler::GetDefaultPort(int32_t *result)
{
    *result = -1;        // no port for JAR: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsJARProtocolHandler::GetProtocolFlags(uint32_t *result)
{
    // URI_LOADABLE_BY_ANYONE, since it's our inner URI that will matter
    // anyway.
    *result = URI_NORELATIVE | URI_NOAUTH | URI_LOADABLE_BY_ANYONE;
    /* Although jar uris have their own concept of relative urls
       it is very different from the standard behaviour, so we
       have to say norelative here! */
    return NS_OK;
}

NS_IMETHODIMP
nsJARProtocolHandler::NewURI(const nsACString &aSpec,
                             const char *aCharset,
                             nsIURI *aBaseURI,
                             nsIURI **result)
{
    nsresult rv = NS_OK;

    nsRefPtr<nsJARURI> jarURI = new nsJARURI();
    if (!jarURI)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = jarURI->Init(aCharset);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = jarURI->SetSpecWithBase(aSpec, aBaseURI);
    if (NS_FAILED(rv))
        return rv;

    NS_ADDREF(*result = jarURI);
    return rv;
}

NS_IMETHODIMP
nsJARProtocolHandler::NewChannel2(nsIURI* uri,
                                  nsILoadInfo* aLoadInfo,
                                  nsIChannel** result)
{
    nsJARChannel *chan = new nsJARChannel();
    if (!chan)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(chan);

    nsresult rv = chan->Init(uri);
    if (NS_FAILED(rv)) {
        NS_RELEASE(chan);
        return rv;
    }

    // set the loadInfo on the new channel
    rv = chan->SetLoadInfo(aLoadInfo);
    if (NS_FAILED(rv)) {
        NS_RELEASE(chan);
        return rv;
    }

    *result = chan;
    return NS_OK;
}

NS_IMETHODIMP
nsJARProtocolHandler::NewChannel(nsIURI *uri, nsIChannel **result)
{
    return NewChannel2(uri, nullptr, result);
}


NS_IMETHODIMP
nsJARProtocolHandler::AllowPort(int32_t port, const char *scheme, bool *_retval)
{
    // don't override anything.
    *_retval = false;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
