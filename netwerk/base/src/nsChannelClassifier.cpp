/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsChannelClassifier.h"

#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIProtocolHandler.h"
#include "nsICachingChannel.h"
#include "nsICacheEntryDescriptor.h"
#include "prlog.h"
#include "nsIScriptSecurityManager.h"

#if defined(PR_LOGGING)
//
// NSPR_LOG_MODULES=nsChannelClassifier:5
//
static PRLogModuleInfo *gChannelClassifierLog;
#endif
#define LOG(args)     PR_LOG(gChannelClassifierLog, PR_LOG_DEBUG, args)

NS_IMPL_ISUPPORTS1(nsChannelClassifier,
                   nsIURIClassifierCallback)

nsChannelClassifier::nsChannelClassifier()
{
#if defined(PR_LOGGING)
    if (!gChannelClassifierLog)
        gChannelClassifierLog = PR_NewLogModule("nsChannelClassifier");
#endif
}

nsresult
nsChannelClassifier::Start(nsIChannel *aChannel)
{
    // Don't bother to run the classifier on a load that has already failed.
    // (this might happen after a redirect)
    PRUint32 status;
    aChannel->GetStatus(&status);
    if (NS_FAILED(status))
        return NS_OK;

    // Don't bother to run the classifier on a cached load that was
    // previously classified.
    if (HasBeenClassified(aChannel)) {
        return NS_OK;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    // Don't bother checking certain types of URIs.
    bool hasFlags;
    rv = NS_URIChainHasFlags(uri,
                             nsIProtocolHandler::URI_DANGEROUS_TO_LOAD,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) return NS_OK;

    rv = NS_URIChainHasFlags(uri,
                             nsIProtocolHandler::URI_IS_LOCAL_FILE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) return NS_OK;

    rv = NS_URIChainHasFlags(uri,
                             nsIProtocolHandler::URI_IS_UI_RESOURCE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) return NS_OK;

    rv = NS_URIChainHasFlags(uri,
                             nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) return NS_OK;

    nsCOMPtr<nsIURIClassifier> uriClassifier =
        do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
    if (rv == NS_ERROR_FACTORY_NOT_REGISTERED ||
        rv == NS_ERROR_NOT_AVAILABLE) {
        // no URI classifier, ignore this failure.
        return NS_OK;
    }
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIScriptSecurityManager> securityManager =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> principal;
    rv = securityManager->GetChannelPrincipal(aChannel,
                                              getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    bool expectCallback;
    rv = uriClassifier->Classify(principal, this, &expectCallback);
    if (NS_FAILED(rv)) return rv;

    if (expectCallback) {
        // Suspend the channel, it will be resumed when we get the classifier
        // callback.
        rv = aChannel->Suspend();
        if (NS_FAILED(rv)) {
            // Some channels (including nsJSChannel) fail on Suspend.  This
            // shouldn't be fatal, but will prevent malware from being
            // blocked on these channels.
            return NS_OK;
        }

        mSuspendedChannel = aChannel;
#ifdef DEBUG
        LOG(("nsChannelClassifier[%p]: suspended channel %p",
             this, mSuspendedChannel.get()));
#endif
    }

    return NS_OK;
}

// Note in the cache entry that this URL was classified, so that future
// cached loads don't need to be checked.
void
nsChannelClassifier::MarkEntryClassified(nsresult status)
{
    nsCOMPtr<nsICachingChannel> cachingChannel =
        do_QueryInterface(mSuspendedChannel);
    if (!cachingChannel) {
        return;
    }

    nsCOMPtr<nsISupports> cacheToken;
    cachingChannel->GetCacheToken(getter_AddRefs(cacheToken));
    if (!cacheToken) {
        return;
    }

    nsCOMPtr<nsICacheEntryDescriptor> cacheEntry =
        do_QueryInterface(cacheToken);
    if (!cacheEntry) {
        return;
    }

    cacheEntry->SetMetaDataElement("necko:classified",
                                   NS_SUCCEEDED(status) ? "1" : nsnull);
}

bool
nsChannelClassifier::HasBeenClassified(nsIChannel *aChannel)
{
    nsCOMPtr<nsICachingChannel> cachingChannel =
        do_QueryInterface(aChannel);
    if (!cachingChannel) {
        return false;
    }

    // Only check the tag if we are loading from the cache without
    // validation.
    bool fromCache;
    if (NS_FAILED(cachingChannel->IsFromCache(&fromCache)) || !fromCache) {
        return false;
    }

    nsCOMPtr<nsISupports> cacheToken;
    cachingChannel->GetCacheToken(getter_AddRefs(cacheToken));
    if (!cacheToken) {
        return false;
    }

    nsCOMPtr<nsICacheEntryDescriptor> cacheEntry =
        do_QueryInterface(cacheToken);
    if (!cacheEntry) {
        return false;
    }

    nsXPIDLCString tag;
    cacheEntry->GetMetaDataElement("necko:classified", getter_Copies(tag));
    return tag.EqualsLiteral("1");
}

NS_IMETHODIMP
nsChannelClassifier::OnClassifyComplete(nsresult aErrorCode)
{
    if (mSuspendedChannel) {
        MarkEntryClassified(aErrorCode);

        if (NS_FAILED(aErrorCode)) {
#ifdef DEBUG
            LOG(("nsChannelClassifier[%p]: cancelling channel %p with error "
                 "code: %x", this, mSuspendedChannel.get(), aErrorCode));
#endif
            mSuspendedChannel->Cancel(aErrorCode);
        }
#ifdef DEBUG
        LOG(("nsChannelClassifier[%p]: resuming channel %p from "
             "OnClassifyComplete", this, mSuspendedChannel.get()));
#endif
        mSuspendedChannel->Resume();
        mSuspendedChannel = nsnull;
    }

    return NS_OK;
}
