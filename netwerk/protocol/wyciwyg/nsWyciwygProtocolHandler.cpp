/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWyciwyg.h"
#include "nsWyciwygChannel.h"
#include "nsWyciwygProtocolHandler.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "plstr.h"
#include "nsNetUtil.h"
#include "nsIObserverService.h"
#include "mozIApplicationClearPrivateDataParams.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"

#include "mozilla/net/NeckoChild.h"

using namespace mozilla::net;
#include "mozilla/net/WyciwygChannelChild.h"

////////////////////////////////////////////////////////////////////////////////

nsWyciwygProtocolHandler::nsWyciwygProtocolHandler() 
{
#if defined(PR_LOGGING)
  if (!gWyciwygLog)
    gWyciwygLog = PR_NewLogModule("nsWyciwygChannel");
#endif

  LOG(("Creating nsWyciwygProtocolHandler [this=%p].\n", this));
}

nsWyciwygProtocolHandler::~nsWyciwygProtocolHandler() 
{
  LOG(("Deleting nsWyciwygProtocolHandler [this=%p]\n", this));
}

nsresult
nsWyciwygProtocolHandler::Init()
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "webapps-clear-data", true);
  }
  return NS_OK;
}

static void
EvictCacheSession(uint32_t aAppId,
                  bool aInBrowser,
                  bool aPrivateBrowsing)
{
  nsAutoCString clientId;
  nsWyciwygProtocolHandler::GetCacheSessionName(aAppId, aInBrowser,
                                                aPrivateBrowsing,
                                                clientId);
  nsCOMPtr<nsICacheService> serv =
      do_GetService(NS_CACHESERVICE_CONTRACTID);
  nsCOMPtr<nsICacheSession> session;
  nsresult rv = serv->CreateSession(clientId.get(),
                                    nsICache::STORE_ANYWHERE,
                                    nsICache::STREAM_BASED,
                                    getter_AddRefs(session));
  if (NS_SUCCEEDED(rv) && session) {
    session->EvictEntries();
  }
}

void
nsWyciwygProtocolHandler::GetCacheSessionName(uint32_t aAppId,
                                              bool aInBrowser,
                                              bool aPrivateBrowsing,
                                              nsACString& aSessionName)
{
  if (aPrivateBrowsing) {
    aSessionName.AssignLiteral("wyciwyg-private");
  } else {
    aSessionName.AssignLiteral("wyciwyg");
  }
  if (aAppId == NECKO_NO_APP_ID && !aInBrowser) {
    return;
  }

  aSessionName.Append('~');
  aSessionName.AppendInt(aAppId);
  aSessionName.Append('~');
  aSessionName.AppendInt(aInBrowser);
}

NS_IMETHODIMP
nsWyciwygProtocolHandler::Observe(nsISupports *subject,
                                  const char *topic,
                                  const PRUnichar *data)
{
  if (strcmp(topic, "webapps-clear-data") == 0) {
    nsCOMPtr<mozIApplicationClearPrivateDataParams> params =
        do_QueryInterface(subject);
    if (!params) {
      NS_ERROR("'webapps-clear-data' notification's subject should be a mozIApplicationClearPrivateDataParams");
      return NS_ERROR_UNEXPECTED;
    }

    uint32_t appId;
    bool browserOnly;
    nsresult rv = params->GetAppId(&appId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = params->GetBrowserOnly(&browserOnly);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_ASSERT(appId != NECKO_UNKNOWN_APP_ID);

    EvictCacheSession(appId, browserOnly, false);
    EvictCacheSession(appId, browserOnly, true);
    if (!browserOnly) {
      EvictCacheSession(appId, true, false);
      EvictCacheSession(appId, true, true);
    }
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS3(nsWyciwygProtocolHandler,
                   nsIProtocolHandler,
                   nsIObserver,
                   nsISupportsWeakReference)

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsWyciwygProtocolHandler::GetScheme(nsACString &result)
{
  result = "wyciwyg";
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygProtocolHandler::GetDefaultPort(int32_t *result) 
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP 
nsWyciwygProtocolHandler::AllowPort(int32_t port, const char *scheme, bool *_retval)
{
  // don't override anything.  
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygProtocolHandler::NewURI(const nsACString &aSpec,
                                 const char *aCharset, // ignored
                                 nsIURI *aBaseURI,
                                 nsIURI **result) 
{
  nsresult rv;

  nsCOMPtr<nsIURI> url = do_CreateInstance(NS_SIMPLEURI_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = url->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  *result = url;
  NS_ADDREF(*result);

  return rv;
}

NS_IMETHODIMP
nsWyciwygProtocolHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
  if (mozilla::net::IsNeckoChild())
    mozilla::net::NeckoChild::InitNeckoChild();

  NS_ENSURE_ARG_POINTER(url);
  nsresult rv;

  nsCOMPtr<nsIWyciwygChannel> channel;
  if (IsNeckoChild()) {
    NS_ENSURE_TRUE(gNeckoChild != nullptr, NS_ERROR_FAILURE);

    WyciwygChannelChild *wcc = static_cast<WyciwygChannelChild *>(
                                 gNeckoChild->SendPWyciwygChannelConstructor());
    if (!wcc)
      return NS_ERROR_OUT_OF_MEMORY;

    channel = wcc;
    rv = wcc->Init(url);
    if (NS_FAILED(rv))
      PWyciwygChannelChild::Send__delete__(wcc);
  } else
  {
    // If original channel used https, make sure PSM is initialized
    // (this may be first channel to load during a session restore)
    nsAutoCString path;
    rv = url->GetPath(path);
    NS_ENSURE_SUCCESS(rv, rv);
    int32_t slashIndex = path.FindChar('/', 2);
    if (slashIndex == kNotFound)
      return NS_ERROR_FAILURE;
    if (path.Length() < (uint32_t)slashIndex + 1 + 5)
      return NS_ERROR_FAILURE;
    if (!PL_strncasecmp(path.get() + slashIndex + 1, "https", 5))
      net_EnsurePSMInit();

    nsWyciwygChannel *wc = new nsWyciwygChannel();
    channel = wc;
    rv = wc->Init(url);
  }

  if (NS_FAILED(rv))
    return rv;

  *result = channel.forget().get();
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygProtocolHandler::GetProtocolFlags(uint32_t *result) 
{
  // Should this be an an nsINestedURI?  We don't really want random webpages
  // loading these URIs...

  // Note that using URI_INHERITS_SECURITY_CONTEXT here is OK -- untrusted code
  // is not allowed to link to wyciwyg URIs and users shouldn't be able to get
  // at them, and nsDocShell::InternalLoad forbids non-history loads of these
  // URIs.  And when loading from history we end up using the principal from
  // the history entry, which we put there ourselves, so all is ok.
  *result = URI_NORELATIVE | URI_NOAUTH | URI_DANGEROUS_TO_LOAD |
    URI_INHERITS_SECURITY_CONTEXT;
  return NS_OK;
}
