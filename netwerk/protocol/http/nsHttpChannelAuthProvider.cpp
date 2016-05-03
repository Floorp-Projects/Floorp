/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 sw=4 sts=4 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/Preferences.h"
#include "nsHttpChannelAuthProvider.h"
#include "nsNetUtil.h"
#include "nsHttpHandler.h"
#include "nsIHttpAuthenticator.h"
#include "nsIHttpChannelInternal.h"
#include "nsIAuthPrompt2.h"
#include "nsIAuthPromptProvider.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsEscape.h"
#include "nsAuthInformationHolder.h"
#include "nsIStringBundle.h"
#include "nsIPrompt.h"
#include "netCore.h"
#include "nsIHttpAuthenticableChannel.h"
#include "nsIURI.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsILoadContext.h"
#include "nsIURL.h"
#include "mozilla/Telemetry.h"

namespace mozilla {
namespace net {

#define SUBRESOURCE_AUTH_DIALOG_DISALLOW_ALL 0
#define SUBRESOURCE_AUTH_DIALOG_DISALLOW_CROSS_ORIGIN 1
#define SUBRESOURCE_AUTH_DIALOG_ALLOW_ALL 2

#define HTTP_AUTH_DIALOG_TOP_LEVEL_DOC 0
#define HTTP_AUTH_DIALOG_SAME_ORIGIN_SUBRESOURCE 1
#define HTTP_AUTH_DIALOG_CROSS_ORIGIN_SUBRESOURCE 2

#define HTTP_AUTH_BASIC_INSECURE 0
#define HTTP_AUTH_BASIC_SECURE 1
#define HTTP_AUTH_DIGEST_INSECURE 2
#define HTTP_AUTH_DIGEST_SECURE 3
#define HTTP_AUTH_NTLM_INSECURE 4
#define HTTP_AUTH_NTLM_SECURE 5
#define HTTP_AUTH_NEGOTIATE_INSECURE 6
#define HTTP_AUTH_NEGOTIATE_SECURE 7

static void
GetOriginAttributesSuffix(nsIChannel* aChan, nsACString &aSuffix)
{
    NeckoOriginAttributes oa;

    // Deliberately ignoring the result and going with defaults
    if (aChan) {
        NS_GetOriginAttributes(aChan, oa);
    }

    oa.CreateSuffix(aSuffix);
}

nsHttpChannelAuthProvider::nsHttpChannelAuthProvider()
    : mAuthChannel(nullptr)
    , mIsPrivate(false)
    , mProxyAuthContinuationState(nullptr)
    , mAuthContinuationState(nullptr)
    , mProxyAuth(false)
    , mTriedProxyAuth(false)
    , mTriedHostAuth(false)
    , mSuppressDefensiveAuth(false)
    , mHttpHandler(gHttpHandler)
{
}

nsHttpChannelAuthProvider::~nsHttpChannelAuthProvider()
{
    MOZ_ASSERT(!mAuthChannel, "Disconnect wasn't called");
}

uint32_t nsHttpChannelAuthProvider::sAuthAllowPref =
    SUBRESOURCE_AUTH_DIALOG_ALLOW_ALL;

void
nsHttpChannelAuthProvider::InitializePrefs()
{
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::Preferences::AddUintVarCache(&sAuthAllowPref,
                                        "network.auth.subresource-http-auth-allow",
                                        SUBRESOURCE_AUTH_DIALOG_ALLOW_ALL);
}

NS_IMETHODIMP
nsHttpChannelAuthProvider::Init(nsIHttpAuthenticableChannel *channel)
{
    MOZ_ASSERT(channel, "channel expected!");

    mAuthChannel = channel;

    nsresult rv = mAuthChannel->GetURI(getter_AddRefs(mURI));
    if (NS_FAILED(rv)) return rv;

    mAuthChannel->GetIsSSL(&mUsingSSL);
    if (NS_FAILED(rv)) return rv;

    rv = mURI->GetAsciiHost(mHost);
    if (NS_FAILED(rv)) return rv;

    // reject the URL if it doesn't specify a host
    if (mHost.IsEmpty())
        return NS_ERROR_MALFORMED_URI;

    rv = mURI->GetPort(&mPort);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> bareChannel = do_QueryInterface(channel);
    mIsPrivate = NS_UsePrivateBrowsing(bareChannel);

    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannelAuthProvider::ProcessAuthentication(uint32_t httpStatus,
                                                 bool     SSLConnectFailed)
{
    LOG(("nsHttpChannelAuthProvider::ProcessAuthentication "
         "[this=%p channel=%p code=%u SSLConnectFailed=%d]\n",
         this, mAuthChannel, httpStatus, SSLConnectFailed));

    MOZ_ASSERT(mAuthChannel, "Channel not initialized");

    nsCOMPtr<nsIProxyInfo> proxyInfo;
    nsresult rv = mAuthChannel->GetProxyInfo(getter_AddRefs(proxyInfo));
    if (NS_FAILED(rv)) return rv;
    if (proxyInfo) {
        mProxyInfo = do_QueryInterface(proxyInfo);
        if (!mProxyInfo) return NS_ERROR_NO_INTERFACE;
    }

    nsAutoCString challenges;
    mProxyAuth = (httpStatus == 407);

    rv = PrepareForAuthentication(mProxyAuth);
    if (NS_FAILED(rv))
        return rv;

    if (mProxyAuth) {
        // only allow a proxy challenge if we have a proxy server configured.
        // otherwise, we could inadvertently expose the user's proxy
        // credentials to an origin server.  We could attempt to proceed as
        // if we had received a 401 from the server, but why risk flirting
        // with trouble?  IE similarly rejects 407s when a proxy server is
        // not configured, so there's no reason not to do the same.
        if (!UsingHttpProxy()) {
            LOG(("rejecting 407 when proxy server not configured!\n"));
            return NS_ERROR_UNEXPECTED;
        }
        if (UsingSSL() && !SSLConnectFailed) {
            // we need to verify that this challenge came from the proxy
            // server itself, and not some server on the other side of the
            // SSL tunnel.
            LOG(("rejecting 407 from origin server!\n"));
            return NS_ERROR_UNEXPECTED;
        }
        rv = mAuthChannel->GetProxyChallenges(challenges);
    }
    else
        rv = mAuthChannel->GetWWWChallenges(challenges);
    if (NS_FAILED(rv)) return rv;

    nsAutoCString creds;
    rv = GetCredentials(challenges.get(), mProxyAuth, creds);
    if (rv == NS_ERROR_IN_PROGRESS)
        return rv;
    if (NS_FAILED(rv))
        LOG(("unable to authenticate\n"));
    else {
        // set the authentication credentials
        if (mProxyAuth)
            rv = mAuthChannel->SetProxyCredentials(creds);
        else
            rv = mAuthChannel->SetWWWCredentials(creds);
    }
    return rv;
}

NS_IMETHODIMP
nsHttpChannelAuthProvider::AddAuthorizationHeaders(bool aDontUseCachedWWWCreds)
{
    LOG(("nsHttpChannelAuthProvider::AddAuthorizationHeaders? "
         "[this=%p channel=%p]\n", this, mAuthChannel));

    MOZ_ASSERT(mAuthChannel, "Channel not initialized");

    nsCOMPtr<nsIProxyInfo> proxyInfo;
    nsresult rv = mAuthChannel->GetProxyInfo(getter_AddRefs(proxyInfo));
    if (NS_FAILED(rv)) return rv;
    if (proxyInfo) {
        mProxyInfo = do_QueryInterface(proxyInfo);
        if (!mProxyInfo) return NS_ERROR_NO_INTERFACE;
    }

    uint32_t loadFlags;
    rv = mAuthChannel->GetLoadFlags(&loadFlags);
    if (NS_FAILED(rv)) return rv;

    // this getter never fails
    nsHttpAuthCache *authCache = gHttpHandler->AuthCache(mIsPrivate);

    // check if proxy credentials should be sent
    const char *proxyHost = ProxyHost();
    if (proxyHost && UsingHttpProxy()) {
        SetAuthorizationHeader(authCache, nsHttp::Proxy_Authorization,
                               "http", proxyHost, ProxyPort(),
                               nullptr, // proxy has no path
                               mProxyIdent);
    }

    if (loadFlags & nsIRequest::LOAD_ANONYMOUS) {
        LOG(("Skipping Authorization header for anonymous load\n"));
        return NS_OK;
    }

    if (aDontUseCachedWWWCreds) {
        LOG(("Authorization header already present:"
             " skipping adding auth header from cache\n"));
        return NS_OK;
    }

    // check if server credentials should be sent
    nsAutoCString path, scheme;
    if (NS_SUCCEEDED(GetCurrentPath(path)) &&
        NS_SUCCEEDED(mURI->GetScheme(scheme))) {
        SetAuthorizationHeader(authCache, nsHttp::Authorization,
                               scheme.get(),
                               Host(),
                               Port(),
                               path.get(),
                               mIdent);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannelAuthProvider::CheckForSuperfluousAuth()
{
    LOG(("nsHttpChannelAuthProvider::CheckForSuperfluousAuth? "
         "[this=%p channel=%p]\n", this, mAuthChannel));

    MOZ_ASSERT(mAuthChannel, "Channel not initialized");

    // we've been called because it has been determined that this channel is
    // getting loaded without taking the userpass from the URL.  if the URL
    // contained a userpass, then (provided some other conditions are true),
    // we'll give the user an opportunity to abort the channel as this might be
    // an attempt to spoof a different site (see bug 232567).
    if (!ConfirmAuth(NS_LITERAL_STRING("SuperfluousAuth"), true)) {
        // calling cancel here sets our mStatus and aborts the HTTP
        // transaction, which prevents OnDataAvailable events.
        mAuthChannel->Cancel(NS_ERROR_ABORT);
        return NS_ERROR_ABORT;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannelAuthProvider::Cancel(nsresult status)
{
    MOZ_ASSERT(mAuthChannel, "Channel not initialized");

    if (mAsyncPromptAuthCancelable) {
        mAsyncPromptAuthCancelable->Cancel(status);
        mAsyncPromptAuthCancelable = nullptr;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannelAuthProvider::Disconnect(nsresult status)
{
    mAuthChannel = nullptr;

    if (mAsyncPromptAuthCancelable) {
        mAsyncPromptAuthCancelable->Cancel(status);
        mAsyncPromptAuthCancelable = nullptr;
    }

    NS_IF_RELEASE(mProxyAuthContinuationState);
    NS_IF_RELEASE(mAuthContinuationState);

    return NS_OK;
}

// buf contains "domain\user"
static void
ParseUserDomain(char16_t *buf,
                const char16_t **user,
                const char16_t **domain)
{
    char16_t *p = buf;
    while (*p && *p != '\\') ++p;
    if (!*p)
        return;
    *p = '\0';
    *domain = buf;
    *user = p + 1;
}

// helper function for setting identity from raw user:pass
static void
SetIdent(nsHttpAuthIdentity &ident,
         uint32_t authFlags,
         char16_t *userBuf,
         char16_t *passBuf)
{
    const char16_t *user = userBuf;
    const char16_t *domain = nullptr;

    if (authFlags & nsIHttpAuthenticator::IDENTITY_INCLUDES_DOMAIN)
        ParseUserDomain(userBuf, &user, &domain);

    ident.Set(domain, user, passBuf);
}

// helper function for getting an auth prompt from an interface requestor
static void
GetAuthPrompt(nsIInterfaceRequestor *ifreq, bool proxyAuth,
              nsIAuthPrompt2 **result)
{
    if (!ifreq)
        return;

    uint32_t promptReason;
    if (proxyAuth)
        promptReason = nsIAuthPromptProvider::PROMPT_PROXY;
    else
        promptReason = nsIAuthPromptProvider::PROMPT_NORMAL;

    nsCOMPtr<nsIAuthPromptProvider> promptProvider = do_GetInterface(ifreq);
    if (promptProvider)
        promptProvider->GetAuthPrompt(promptReason,
                                      NS_GET_IID(nsIAuthPrompt2),
                                      reinterpret_cast<void**>(result));
    else
        NS_QueryAuthPrompt2(ifreq, result);
}

// generate credentials for the given challenge, and update the auth cache.
nsresult
nsHttpChannelAuthProvider::GenCredsAndSetEntry(nsIHttpAuthenticator *auth,
                                               bool                  proxyAuth,
                                               const char           *scheme,
                                               const char           *host,
                                               int32_t               port,
                                               const char           *directory,
                                               const char           *realm,
                                               const char           *challenge,
                                               const nsHttpAuthIdentity &ident,
                                               nsCOMPtr<nsISupports>    &sessionState,
                                               char                    **result)
{
    nsresult rv;
    uint32_t authFlags;

    rv = auth->GetAuthFlags(&authFlags);
    if (NS_FAILED(rv)) return rv;

    nsISupports *ss = sessionState;

    // set informations that depend on whether
    // we're authenticating against a proxy
    // or a webserver
    nsISupports **continuationState;

    if (proxyAuth) {
        continuationState = &mProxyAuthContinuationState;
    } else {
        continuationState = &mAuthContinuationState;
    }

    uint32_t generateFlags;
    rv = auth->GenerateCredentials(mAuthChannel,
                                   challenge,
                                   proxyAuth,
                                   ident.Domain(),
                                   ident.User(),
                                   ident.Password(),
                                   &ss,
                                   &*continuationState,
                                   &generateFlags,
                                   result);

    sessionState.swap(ss);
    if (NS_FAILED(rv)) return rv;

    // don't log this in release build since it could contain sensitive info.
#ifdef DEBUG
    LOG(("generated creds: %s\n", *result));
#endif

    // find out if this authenticator allows reuse of credentials and/or
    // challenge.
    bool saveCreds =
        0 != (authFlags & nsIHttpAuthenticator::REUSABLE_CREDENTIALS);
    bool saveChallenge =
        0 != (authFlags & nsIHttpAuthenticator::REUSABLE_CHALLENGE);

    bool saveIdentity =
        0 == (generateFlags & nsIHttpAuthenticator::USING_INTERNAL_IDENTITY);

    // this getter never fails
    nsHttpAuthCache *authCache = gHttpHandler->AuthCache(mIsPrivate);

    nsCOMPtr<nsIChannel> chan = do_QueryInterface(mAuthChannel);
    nsAutoCString suffix;
    GetOriginAttributesSuffix(chan, suffix);

    // create a cache entry.  we do this even though we don't yet know that
    // these credentials are valid b/c we need to avoid prompting the user
    // more than once in case the credentials are valid.
    //
    // if the credentials are not reusable, then we don't bother sticking
    // them in the auth cache.
    rv = authCache->SetAuthEntry(scheme, host, port, directory, realm,
                                 saveCreds ? *result : nullptr,
                                 saveChallenge ? challenge : nullptr,
                                 suffix,
                                 saveIdentity ? &ident : nullptr,
                                 sessionState);
    return rv;
}

nsresult
nsHttpChannelAuthProvider::PrepareForAuthentication(bool proxyAuth)
{
    LOG(("nsHttpChannelAuthProvider::PrepareForAuthentication "
         "[this=%p channel=%p]\n", this, mAuthChannel));

    if (!proxyAuth) {
        // reset the current proxy continuation state because our last
        // authentication attempt was completed successfully.
        NS_IF_RELEASE(mProxyAuthContinuationState);
        LOG(("  proxy continuation state has been reset"));
    }

    if (!UsingHttpProxy() || mProxyAuthType.IsEmpty())
        return NS_OK;

    // We need to remove any Proxy_Authorization header left over from a
    // non-request based authentication handshake (e.g., for NTLM auth).

    nsAutoCString contractId;
    contractId.Assign(NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX);
    contractId.Append(mProxyAuthType);

    nsresult rv;
    nsCOMPtr<nsIHttpAuthenticator> precedingAuth =
        do_GetService(contractId.get(), &rv);
    if (NS_FAILED(rv))
        return rv;

    uint32_t precedingAuthFlags;
    rv = precedingAuth->GetAuthFlags(&precedingAuthFlags);
    if (NS_FAILED(rv))
        return rv;

    if (!(precedingAuthFlags & nsIHttpAuthenticator::REQUEST_BASED)) {
        nsAutoCString challenges;
        rv = mAuthChannel->GetProxyChallenges(challenges);
        if (NS_FAILED(rv)) {
            // delete the proxy authorization header because we weren't
            // asked to authenticate
            rv = mAuthChannel->SetProxyCredentials(EmptyCString());
            if (NS_FAILED(rv)) return rv;
            LOG(("  cleared proxy authorization header"));
        }
    }

    return NS_OK;
}

nsresult
nsHttpChannelAuthProvider::GetCredentials(const char     *challenges,
                                          bool            proxyAuth,
                                          nsAFlatCString &creds)
{
    nsCOMPtr<nsIHttpAuthenticator> auth;
    nsAutoCString challenge;

    nsCString authType; // force heap allocation to enable string sharing since
                        // we'll be assigning this value into mAuthType.

    // set informations that depend on whether we're authenticating against a
    // proxy or a webserver
    nsISupports **currentContinuationState;
    nsCString *currentAuthType;

    if (proxyAuth) {
        currentContinuationState = &mProxyAuthContinuationState;
        currentAuthType = &mProxyAuthType;
    } else {
        currentContinuationState = &mAuthContinuationState;
        currentAuthType = &mAuthType;
    }

    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    bool gotCreds = false;

    // figure out which challenge we can handle and which authenticator to use.
    for (const char *eol = challenges - 1; eol; ) {
        const char *p = eol + 1;

        // get the challenge string (LF separated -- see nsHttpHeaderArray)
        if ((eol = strchr(p, '\n')) != nullptr)
            challenge.Assign(p, eol - p);
        else
            challenge.Assign(p);

        rv = GetAuthenticator(challenge.get(), authType, getter_AddRefs(auth));
        if (NS_SUCCEEDED(rv)) {
            //
            // if we've already selected an auth type from a previous challenge
            // received while processing this channel, then skip others until
            // we find a challenge corresponding to the previously tried auth
            // type.
            //
            if (!currentAuthType->IsEmpty() && authType != *currentAuthType)
                continue;

            //
            // we allow the routines to run all the way through before we
            // decide if they are valid.
            //
            // we don't worry about the auth cache being altered because that
            // would have been the last step, and if the error is from updating
            // the authcache it wasn't really altered anyway. -CTN
            //
            // at this point the code is really only useful for client side
            // errors (it will not automatically fail over to do a different
            // auth type if the server keeps rejecting what is being sent, even
            // if a particular auth method only knows 1 thing, like a
            // non-identity based authentication method)
            //
            rv = GetCredentialsForChallenge(challenge.get(), authType.get(),
                                            proxyAuth, auth, creds);
            if (NS_SUCCEEDED(rv)) {
                gotCreds = true;
                *currentAuthType = authType;

                break;
            }
            else if (rv == NS_ERROR_IN_PROGRESS) {
                // authentication prompt has been invoked and result is
                // expected asynchronously, save current challenge being
                // processed and all remaining challenges to use later in
                // OnAuthAvailable and now immediately return
                mCurrentChallenge = challenge;
                mRemainingChallenges = eol ? eol+1 : nullptr;
                return rv;
            }

            // reset the auth type and continuation state
            NS_IF_RELEASE(*currentContinuationState);
            currentAuthType->Truncate();
        }
    }

    if (!gotCreds && !currentAuthType->IsEmpty()) {
        // looks like we never found the auth type we were looking for.
        // reset the auth type and continuation state, and try again.
        currentAuthType->Truncate();
        NS_IF_RELEASE(*currentContinuationState);

        rv = GetCredentials(challenges, proxyAuth, creds);
    }

    return rv;
}

nsresult
nsHttpChannelAuthProvider::GetAuthorizationMembers(bool                 proxyAuth,
                                                   nsCSubstring&        scheme,
                                                   const char*&         host,
                                                   int32_t&             port,
                                                   nsCSubstring&        path,
                                                   nsHttpAuthIdentity*& ident,
                                                   nsISupports**&       continuationState)
{
    if (proxyAuth) {
        MOZ_ASSERT (UsingHttpProxy(),
                    "proxyAuth is true, but no HTTP proxy is configured!");

        host = ProxyHost();
        port = ProxyPort();
        ident = &mProxyIdent;
        scheme.AssignLiteral("http");

        continuationState = &mProxyAuthContinuationState;
    }
    else {
        host = Host();
        port = Port();
        ident = &mIdent;

        nsresult rv;
        rv = GetCurrentPath(path);
        if (NS_FAILED(rv)) return rv;

        rv = mURI->GetScheme(scheme);
        if (NS_FAILED(rv)) return rv;

        continuationState = &mAuthContinuationState;
    }

    return NS_OK;
}

nsresult
nsHttpChannelAuthProvider::GetCredentialsForChallenge(const char *challenge,
                                                      const char *authType,
                                                      bool        proxyAuth,
                                                      nsIHttpAuthenticator *auth,
                                                      nsAFlatCString     &creds)
{
    LOG(("nsHttpChannelAuthProvider::GetCredentialsForChallenge "
         "[this=%p channel=%p proxyAuth=%d challenges=%s]\n",
        this, mAuthChannel, proxyAuth, challenge));

    // this getter never fails
    nsHttpAuthCache *authCache = gHttpHandler->AuthCache(mIsPrivate);

    uint32_t authFlags;
    nsresult rv = auth->GetAuthFlags(&authFlags);
    if (NS_FAILED(rv)) return rv;

    nsAutoCString realm;
    ParseRealm(challenge, realm);

    // if no realm, then use the auth type as the realm.  ToUpperCase so the
    // ficticious realm stands out a bit more.
    // XXX this will cause some single signon misses!
    // XXX this was meant to be used with NTLM, which supplies no realm.
    /*
    if (realm.IsEmpty()) {
        realm = authType;
        ToUpperCase(realm);
    }
    */

    // set informations that depend on whether
    // we're authenticating against a proxy
    // or a webserver
    const char *host;
    int32_t port;
    nsHttpAuthIdentity *ident;
    nsAutoCString path, scheme;
    bool identFromURI = false;
    nsISupports **continuationState;

    rv = GetAuthorizationMembers(proxyAuth, scheme, host, port,
                                 path, ident, continuationState);
    if (NS_FAILED(rv)) return rv;

    uint32_t loadFlags;
    rv = mAuthChannel->GetLoadFlags(&loadFlags);
    if (NS_FAILED(rv)) return rv;

    if (!proxyAuth) {
        // if this is the first challenge, then try using the identity
        // specified in the URL.
        if (mIdent.IsEmpty()) {
            GetIdentityFromURI(authFlags, mIdent);
            identFromURI = !mIdent.IsEmpty();
        }

        if ((loadFlags & nsIRequest::LOAD_ANONYMOUS) && !identFromURI) {
            LOG(("Skipping authentication for anonymous non-proxy request\n"));
            return NS_ERROR_NOT_AVAILABLE;
        }

        // Let explicit URL credentials pass
        // regardless of the LOAD_ANONYMOUS flag
    }
    else if ((loadFlags & nsIRequest::LOAD_ANONYMOUS) && !UsingHttpProxy()) {
        LOG(("Skipping authentication for anonymous non-proxy request\n"));
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsIChannel> chan = do_QueryInterface(mAuthChannel);
    nsAutoCString suffix;
    GetOriginAttributesSuffix(chan, suffix);

    //
    // if we already tried some credentials for this transaction, then
    // we need to possibly clear them from the cache, unless the credentials
    // in the cache have changed, in which case we'd want to give them a
    // try instead.
    //
    nsHttpAuthEntry *entry = nullptr;
    authCache->GetAuthEntryForDomain(scheme.get(), host, port,
                                     realm.get(), suffix, &entry);

    // hold reference to the auth session state (in case we clear our
    // reference to the entry).
    nsCOMPtr<nsISupports> sessionStateGrip;
    if (entry)
        sessionStateGrip = entry->mMetaData;

    // for digest auth, maybe our cached nonce value simply timed out...
    bool identityInvalid;
    nsISupports *sessionState = sessionStateGrip;
    rv = auth->ChallengeReceived(mAuthChannel,
                                 challenge,
                                 proxyAuth,
                                 &sessionState,
                                 &*continuationState,
                                 &identityInvalid);
    sessionStateGrip.swap(sessionState);
    if (NS_FAILED(rv)) return rv;

    LOG(("  identity invalid = %d\n", identityInvalid));

    if (identityInvalid) {
        if (entry) {
            if (ident->Equals(entry->Identity())) {
                if (!identFromURI) {
                    LOG(("  clearing bad auth cache entry\n"));
                    // ok, we've already tried this user identity, so clear the
                    // corresponding entry from the auth cache.
                    authCache->ClearAuthEntry(scheme.get(), host,
                                              port, realm.get(),
                                              suffix);
                    entry = nullptr;
                    ident->Clear();
                }
            }
            else if (!identFromURI ||
                     (nsCRT::strcmp(ident->User(),
                                    entry->Identity().User()) == 0 &&
                     !(loadFlags &
                       (nsIChannel::LOAD_ANONYMOUS |
                        nsIChannel::LOAD_EXPLICIT_CREDENTIALS)))) {
                LOG(("  taking identity from auth cache\n"));
                // the password from the auth cache is more likely to be
                // correct than the one in the URL.  at least, we know that it
                // works with the given username.  it is possible for a server
                // to distinguish logons based on the supplied password alone,
                // but that would be quite unusual... and i don't think we need
                // to worry about such unorthodox cases.
                ident->Set(entry->Identity());
                identFromURI = false;
                if (entry->Creds()[0] != '\0') {
                    LOG(("    using cached credentials!\n"));
                    creds.Assign(entry->Creds());
                    return entry->AddPath(path.get());
                }
            }
        }
        else if (!identFromURI) {
            // hmm... identity invalid, but no auth entry!  the realm probably
            // changed (see bug 201986).
            ident->Clear();
        }

        if (!entry && ident->IsEmpty()) {
            uint32_t level = nsIAuthPrompt2::LEVEL_NONE;
            if (mUsingSSL)
                level = nsIAuthPrompt2::LEVEL_SECURE;
            else if (authFlags & nsIHttpAuthenticator::IDENTITY_ENCRYPTED)
                level = nsIAuthPrompt2::LEVEL_PW_ENCRYPTED;

            // Collect statistics on how frequently the various types of HTTP
            // authentication are used over SSL and non-SSL connections.
            if (gHttpHandler->IsTelemetryEnabled()) {
              if (NS_LITERAL_CSTRING("basic").LowerCaseEqualsASCII(authType)) {
                Telemetry::Accumulate(Telemetry::HTTP_AUTH_TYPE_STATS,
                  UsingSSL() ? HTTP_AUTH_BASIC_SECURE : HTTP_AUTH_BASIC_INSECURE);
              } else if (NS_LITERAL_CSTRING("digest").LowerCaseEqualsASCII(authType)) {
                Telemetry::Accumulate(Telemetry::HTTP_AUTH_TYPE_STATS,
                  UsingSSL() ? HTTP_AUTH_DIGEST_SECURE : HTTP_AUTH_DIGEST_INSECURE);
              } else if (NS_LITERAL_CSTRING("ntlm").LowerCaseEqualsASCII(authType)) {
                Telemetry::Accumulate(Telemetry::HTTP_AUTH_TYPE_STATS,
                  UsingSSL() ? HTTP_AUTH_NTLM_SECURE : HTTP_AUTH_NTLM_INSECURE);
              } else if (NS_LITERAL_CSTRING("negotiate").LowerCaseEqualsASCII(authType)) {
                Telemetry::Accumulate(Telemetry::HTTP_AUTH_TYPE_STATS,
                  UsingSSL() ? HTTP_AUTH_NEGOTIATE_SECURE : HTTP_AUTH_NEGOTIATE_INSECURE);
              }
            }

            // Depending on the pref setting, the authentication dialog may be
            // blocked for all sub-resources, blocked for cross-origin
            // sub-resources, or always allowed for sub-resources.
            // For more details look at the bug 647010.
            if (BlockPrompt()) {
              return NS_ERROR_ABORT;
            }

            // at this point we are forced to interact with the user to get
            // their username and password for this domain.
            rv = PromptForIdentity(level, proxyAuth, realm.get(),
                                   authType, authFlags, *ident);
            if (NS_FAILED(rv)) return rv;
            identFromURI = false;
        }
    }

    if (identFromURI) {
        // Warn the user before automatically using the identity from the URL
        // to automatically log them into a site (see bug 232567).
        if (!ConfirmAuth(NS_LITERAL_STRING("AutomaticAuth"), false)) {
            // calling cancel here sets our mStatus and aborts the HTTP
            // transaction, which prevents OnDataAvailable events.
            mAuthChannel->Cancel(NS_ERROR_ABORT);
            // this return code alone is not equivalent to Cancel, since
            // it only instructs our caller that authentication failed.
            // without an explicit call to Cancel, our caller would just
            // load the page that accompanies the HTTP auth challenge.
            return NS_ERROR_ABORT;
        }
    }

    //
    // get credentials for the given user:pass
    //
    // always store the credentials we're trying now so that they will be used
    // on subsequent links.  This will potentially remove good credentials from
    // the cache.  This is ok as we don't want to use cached credentials if the
    // user specified something on the URI or in another manner.  This is so
    // that we don't transparently authenticate as someone they're not
    // expecting to authenticate as.
    //
    nsXPIDLCString result;
    rv = GenCredsAndSetEntry(auth, proxyAuth, scheme.get(), host, port,
                             path.get(), realm.get(), challenge, *ident,
                             sessionStateGrip, getter_Copies(result));
    if (NS_SUCCEEDED(rv))
        creds = result;
    return rv;
}

bool
nsHttpChannelAuthProvider::BlockPrompt()
{
    // Verify that it's ok to prompt for credentials here, per spec
    // http://xhr.spec.whatwg.org/#the-send%28%29-method

    nsCOMPtr<nsIHttpChannelInternal> chanInternal = do_QueryInterface(mAuthChannel);
    MOZ_ASSERT(chanInternal);

    if (chanInternal->GetBlockAuthPrompt()) {
        return true;
    }

    nsCOMPtr<nsIChannel> chan = do_QueryInterface(mAuthChannel);
    nsCOMPtr<nsILoadInfo> loadInfo;
    chan->GetLoadInfo(getter_AddRefs(loadInfo));
    if (!loadInfo) {
        return false;
    }

    if (gHttpHandler->IsTelemetryEnabled()) {
      if (loadInfo->GetExternalContentPolicyType() == nsIContentPolicy::TYPE_DOCUMENT) {
        Telemetry::Accumulate(Telemetry::HTTP_AUTH_DIALOG_STATS,
                              HTTP_AUTH_DIALOG_TOP_LEVEL_DOC);
      } else {
        nsCOMPtr<nsIPrincipal> loadingPrincipal = loadInfo->LoadingPrincipal();
        if (loadingPrincipal) {
          if (NS_SUCCEEDED(loadingPrincipal->CheckMayLoad(mURI, false, false))) {
            Telemetry::Accumulate(Telemetry::HTTP_AUTH_DIALOG_STATS,
              HTTP_AUTH_DIALOG_SAME_ORIGIN_SUBRESOURCE);
          } else {
            Telemetry::Accumulate(Telemetry::HTTP_AUTH_DIALOG_STATS,
              HTTP_AUTH_DIALOG_CROSS_ORIGIN_SUBRESOURCE);
          }
        }
      }
    }

    // Allow if it is the top-level document or xhr.
    if ((loadInfo->GetExternalContentPolicyType() == nsIContentPolicy::TYPE_DOCUMENT) ||
        (loadInfo->GetExternalContentPolicyType() == nsIContentPolicy::TYPE_XMLHTTPREQUEST)) {
        return false;
    }

    switch (sAuthAllowPref) {
    case SUBRESOURCE_AUTH_DIALOG_DISALLOW_ALL:
        // Do not open the http-authentication credentials dialog for
        // the sub-resources.
        return true;
        break;
    case SUBRESOURCE_AUTH_DIALOG_DISALLOW_CROSS_ORIGIN:
        // Do not open the http-authentication credentials dialog for
        // the sub-resources only if they are not cross-origin.
        {
            nsCOMPtr<nsIPrincipal> loadingPrincipal =
                loadInfo->LoadingPrincipal();
            if (!loadingPrincipal) {
                return false;
            }

            if (NS_FAILED(loadingPrincipal->CheckMayLoad(mURI, false, false))) {
                return true;
            }
        }
        break;
    case SUBRESOURCE_AUTH_DIALOG_ALLOW_ALL:
        // Allow the http-authentication dialog.
        return false;
    default:
        // This is an invalid value.
        MOZ_ASSERT(false, "A non valid value!");
    }
    return false;
}

inline void
GetAuthType(const char *challenge, nsCString &authType)
{
    const char *p;

    // get the challenge type
    if ((p = strchr(challenge, ' ')) != nullptr)
        authType.Assign(challenge, p - challenge);
    else
        authType.Assign(challenge);
}

nsresult
nsHttpChannelAuthProvider::GetAuthenticator(const char            *challenge,
                                            nsCString             &authType,
                                            nsIHttpAuthenticator **auth)
{
    LOG(("nsHttpChannelAuthProvider::GetAuthenticator [this=%p channel=%p]\n",
        this, mAuthChannel));

    GetAuthType(challenge, authType);

    // normalize to lowercase
    ToLowerCase(authType);

    nsAutoCString contractid;
    contractid.Assign(NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX);
    contractid.Append(authType);

    return CallGetService(contractid.get(), auth);
}

void
nsHttpChannelAuthProvider::GetIdentityFromURI(uint32_t            authFlags,
                                              nsHttpAuthIdentity &ident)
{
    LOG(("nsHttpChannelAuthProvider::GetIdentityFromURI [this=%p channel=%p]\n",
        this, mAuthChannel));

    nsAutoString userBuf;
    nsAutoString passBuf;

    // XXX i18n
    nsAutoCString buf;
    mURI->GetUsername(buf);
    if (!buf.IsEmpty()) {
        NS_UnescapeURL(buf);
        CopyASCIItoUTF16(buf, userBuf);
        mURI->GetPassword(buf);
        if (!buf.IsEmpty()) {
            NS_UnescapeURL(buf);
            CopyASCIItoUTF16(buf, passBuf);
        }
    }

    if (!userBuf.IsEmpty()) {
        SetIdent(ident, authFlags, (char16_t *) userBuf.get(),
                 (char16_t *) passBuf.get());
    }
}

void
nsHttpChannelAuthProvider::ParseRealm(const char *challenge,
                                      nsACString &realm)
{
    //
    // From RFC2617 section 1.2, the realm value is defined as such:
    //
    //    realm       = "realm" "=" realm-value
    //    realm-value = quoted-string
    //
    // but, we'll accept anything after the the "=" up to the first space, or
    // end-of-line, if the string is not quoted.
    //

    const char *p = PL_strcasestr(challenge, "realm=");
    if (p) {
        bool has_quote = false;
        p += 6;
        if (*p == '"') {
            has_quote = true;
            p++;
        }

        const char *end;
        if (has_quote) {
            end = p;
            while (*end) {
                if (*end == '\\') {
                    // escaped character, store that one instead if not zero
                    if (!*++end)
                        break;
                }
                else if (*end == '\"')
                    // end of string
                    break;

                realm.Append(*end);
                ++end;
            }
        }
        else {
            // realm given without quotes
            end = strchr(p, ' ');
            if (end)
                realm.Assign(p, end - p);
            else
                realm.Assign(p);
        }
    }
}


class nsHTTPAuthInformation : public nsAuthInformationHolder {
public:
    nsHTTPAuthInformation(uint32_t aFlags, const nsString& aRealm,
                          const nsCString& aAuthType)
        : nsAuthInformationHolder(aFlags, aRealm, aAuthType) {}

    void SetToHttpAuthIdentity(uint32_t authFlags,
                               nsHttpAuthIdentity& identity);
};

void
nsHTTPAuthInformation::SetToHttpAuthIdentity(uint32_t authFlags,
                                             nsHttpAuthIdentity& identity)
{
    identity.Set(Domain().get(), User().get(), Password().get());
}

nsresult
nsHttpChannelAuthProvider::PromptForIdentity(uint32_t            level,
                                             bool                proxyAuth,
                                             const char         *realm,
                                             const char         *authType,
                                             uint32_t            authFlags,
                                             nsHttpAuthIdentity &ident)
{
    LOG(("nsHttpChannelAuthProvider::PromptForIdentity [this=%p channel=%p]\n",
        this, mAuthChannel));

    nsresult rv;

    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    rv = mAuthChannel->GetNotificationCallbacks(getter_AddRefs(callbacks));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsILoadGroup> loadGroup;
    rv = mAuthChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIAuthPrompt2> authPrompt;
    GetAuthPrompt(callbacks, proxyAuth, getter_AddRefs(authPrompt));
    if (!authPrompt && loadGroup) {
        nsCOMPtr<nsIInterfaceRequestor> cbs;
        loadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
        GetAuthPrompt(cbs, proxyAuth, getter_AddRefs(authPrompt));
    }
    if (!authPrompt)
        return NS_ERROR_NO_INTERFACE;

    // XXX i18n: need to support non-ASCII realm strings (see bug 41489)
    NS_ConvertASCIItoUTF16 realmU(realm);

    // prompt the user...
    uint32_t promptFlags = 0;
    if (proxyAuth)
    {
        promptFlags |= nsIAuthInformation::AUTH_PROXY;
        if (mTriedProxyAuth)
            promptFlags |= nsIAuthInformation::PREVIOUS_FAILED;
        mTriedProxyAuth = true;
    }
    else {
        promptFlags |= nsIAuthInformation::AUTH_HOST;
        if (mTriedHostAuth)
            promptFlags |= nsIAuthInformation::PREVIOUS_FAILED;
        mTriedHostAuth = true;
    }

    if (authFlags & nsIHttpAuthenticator::IDENTITY_INCLUDES_DOMAIN)
        promptFlags |= nsIAuthInformation::NEED_DOMAIN;

    RefPtr<nsHTTPAuthInformation> holder =
        new nsHTTPAuthInformation(promptFlags, realmU,
                                  nsDependentCString(authType));
    if (!holder)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIChannel> channel(do_QueryInterface(mAuthChannel, &rv));
    if (NS_FAILED(rv)) return rv;

    rv =
        authPrompt->AsyncPromptAuth(channel, this, nullptr, level, holder,
                                    getter_AddRefs(mAsyncPromptAuthCancelable));

    if (NS_SUCCEEDED(rv)) {
        // indicate using this error code that authentication prompt
        // result is expected asynchronously
        rv = NS_ERROR_IN_PROGRESS;
    }
    else {
        // Fall back to synchronous prompt
        bool retval = false;
        rv = authPrompt->PromptAuth(channel, level, holder, &retval);
        if (NS_FAILED(rv))
            return rv;

        if (!retval)
            rv = NS_ERROR_ABORT;
        else
            holder->SetToHttpAuthIdentity(authFlags, ident);
    }

    // remember that we successfully showed the user an auth dialog
    if (!proxyAuth)
        mSuppressDefensiveAuth = true;

    return rv;
}

NS_IMETHODIMP nsHttpChannelAuthProvider::OnAuthAvailable(nsISupports *aContext,
                                                         nsIAuthInformation *aAuthInfo)
{
    LOG(("nsHttpChannelAuthProvider::OnAuthAvailable [this=%p channel=%p]",
        this, mAuthChannel));

    mAsyncPromptAuthCancelable = nullptr;
    if (!mAuthChannel)
        return NS_OK;

    nsresult rv;

    const char *host;
    int32_t port;
    nsHttpAuthIdentity *ident;
    nsAutoCString path, scheme;
    nsISupports **continuationState;
    rv = GetAuthorizationMembers(mProxyAuth, scheme, host, port,
                                 path, ident, continuationState);
    if (NS_FAILED(rv))
        OnAuthCancelled(aContext, false);

    nsAutoCString realm;
    ParseRealm(mCurrentChallenge.get(), realm);

    nsCOMPtr<nsIChannel> chan = do_QueryInterface(mAuthChannel);
    nsAutoCString suffix;
    GetOriginAttributesSuffix(chan, suffix);

    nsHttpAuthCache *authCache = gHttpHandler->AuthCache(mIsPrivate);
    nsHttpAuthEntry *entry = nullptr;
    authCache->GetAuthEntryForDomain(scheme.get(), host, port,
                                     realm.get(), suffix,
                                     &entry);

    nsCOMPtr<nsISupports> sessionStateGrip;
    if (entry)
        sessionStateGrip = entry->mMetaData;

    nsAuthInformationHolder* holder =
            static_cast<nsAuthInformationHolder*>(aAuthInfo);
    ident->Set(holder->Domain().get(),
               holder->User().get(),
               holder->Password().get());

    nsAutoCString unused;
    nsCOMPtr<nsIHttpAuthenticator> auth;
    rv = GetAuthenticator(mCurrentChallenge.get(), unused,
                          getter_AddRefs(auth));
    if (NS_FAILED(rv)) {
        MOZ_ASSERT(false, "GetAuthenticator failed");
        OnAuthCancelled(aContext, true);
        return NS_OK;
    }

    nsXPIDLCString creds;
    rv = GenCredsAndSetEntry(auth, mProxyAuth,
                             scheme.get(), host, port, path.get(),
                             realm.get(), mCurrentChallenge.get(), *ident,
                             sessionStateGrip, getter_Copies(creds));

    mCurrentChallenge.Truncate();
    if (NS_FAILED(rv)) {
        OnAuthCancelled(aContext, true);
        return NS_OK;
    }

    return ContinueOnAuthAvailable(creds);
}

NS_IMETHODIMP nsHttpChannelAuthProvider::OnAuthCancelled(nsISupports *aContext,
                                                         bool        userCancel)
{
    LOG(("nsHttpChannelAuthProvider::OnAuthCancelled [this=%p channel=%p]",
        this, mAuthChannel));

    mAsyncPromptAuthCancelable = nullptr;
    if (!mAuthChannel)
        return NS_OK;

    if (userCancel) {
        if (!mRemainingChallenges.IsEmpty()) {
            // there are still some challenges to process, do so
            nsresult rv;

            nsAutoCString creds;
            rv = GetCredentials(mRemainingChallenges.get(), mProxyAuth, creds);
            if (NS_SUCCEEDED(rv)) {
                // GetCredentials loaded the credentials from the cache or
                // some other way in a synchronous manner, process those
                // credentials now
                mRemainingChallenges.Truncate();
                return ContinueOnAuthAvailable(creds);
            }
            else if (rv == NS_ERROR_IN_PROGRESS) {
                // GetCredentials successfully queued another authprompt for
                // a challenge from the list, we are now waiting for the user
                // to provide the credentials
                return NS_OK;
            }

            // otherwise, we failed...
        }

        mRemainingChallenges.Truncate();
    }

    mAuthChannel->OnAuthCancelled(userCancel);

    return NS_OK;
}

nsresult
nsHttpChannelAuthProvider::ContinueOnAuthAvailable(const nsCSubstring& creds)
{
    nsresult rv;
    if (mProxyAuth)
        rv = mAuthChannel->SetProxyCredentials(creds);
    else
        rv = mAuthChannel->SetWWWCredentials(creds);
    if (NS_FAILED(rv)) return rv;

    // drop our remaining list of challenges.  We don't need them, because we
    // have now authenticated against a challenge and will be sending that
    // information to the server (or proxy).  If it doesn't accept our
    // authentication it'll respond with failure and resend the challenge list
    mRemainingChallenges.Truncate();

    mAuthChannel->OnAuthAvailable();

    return NS_OK;
}

bool
nsHttpChannelAuthProvider::ConfirmAuth(const nsString &bundleKey,
                                       bool            doYesNoPrompt)
{
    // skip prompting the user if
    //   1) we've already prompted the user
    //   2) we're not a toplevel channel
    //   3) the userpass length is less than the "phishy" threshold

    uint32_t loadFlags;
    nsresult rv = mAuthChannel->GetLoadFlags(&loadFlags);
    if (NS_FAILED(rv))
        return true;

    if (mSuppressDefensiveAuth ||
        !(loadFlags & nsIChannel::LOAD_INITIAL_DOCUMENT_URI))
        return true;

    nsAutoCString userPass;
    rv = mURI->GetUserPass(userPass);
    if (NS_FAILED(rv) ||
        (userPass.Length() < gHttpHandler->PhishyUserPassLength()))
        return true;

    // we try to confirm by prompting the user.  if we cannot do so, then
    // assume the user said ok.  this is done to keep things working in
    // embedded builds, where the string bundle might not be present, etc.

    nsCOMPtr<nsIStringBundleService> bundleService =
            do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    if (!bundleService)
        return true;

    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(bundle));
    if (!bundle)
        return true;

    nsAutoCString host;
    rv = mURI->GetHost(host);
    if (NS_FAILED(rv))
        return true;

    nsAutoCString user;
    rv = mURI->GetUsername(user);
    if (NS_FAILED(rv))
        return true;

    NS_ConvertUTF8toUTF16 ucsHost(host), ucsUser(user);
    const char16_t *strs[2] = { ucsHost.get(), ucsUser.get() };

    nsXPIDLString msg;
    bundle->FormatStringFromName(bundleKey.get(), strs, 2, getter_Copies(msg));
    if (!msg)
        return true;

    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    rv = mAuthChannel->GetNotificationCallbacks(getter_AddRefs(callbacks));
    if (NS_FAILED(rv))
        return true;

    nsCOMPtr<nsILoadGroup> loadGroup;
    rv = mAuthChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (NS_FAILED(rv))
        return true;

    nsCOMPtr<nsIPrompt> prompt;
    NS_QueryNotificationCallbacks(callbacks, loadGroup, NS_GET_IID(nsIPrompt),
                                  getter_AddRefs(prompt));
    if (!prompt)
        return true;

    // do not prompt again
    mSuppressDefensiveAuth = true;

    bool confirmed;
    if (doYesNoPrompt) {
        int32_t choice;
        bool checkState = false;
        rv = prompt->ConfirmEx(nullptr, msg,
                               nsIPrompt::BUTTON_POS_1_DEFAULT +
                               nsIPrompt::STD_YES_NO_BUTTONS,
                               nullptr, nullptr, nullptr, nullptr,
                               &checkState, &choice);
        if (NS_FAILED(rv))
            return true;

        confirmed = choice == 0;
    }
    else {
        rv = prompt->Confirm(nullptr, msg, &confirmed);
        if (NS_FAILED(rv))
            return true;
    }

    return confirmed;
}

void
nsHttpChannelAuthProvider::SetAuthorizationHeader(nsHttpAuthCache    *authCache,
                                                  nsHttpAtom          header,
                                                  const char         *scheme,
                                                  const char         *host,
                                                  int32_t             port,
                                                  const char         *path,
                                                  nsHttpAuthIdentity &ident)
{
    nsHttpAuthEntry *entry = nullptr;
    nsresult rv;

    // set informations that depend on whether
    // we're authenticating against a proxy
    // or a webserver
    nsISupports **continuationState;

    if (header == nsHttp::Proxy_Authorization) {
        continuationState = &mProxyAuthContinuationState;
    } else {
        continuationState = &mAuthContinuationState;
    }

    nsCOMPtr<nsIChannel> chan = do_QueryInterface(mAuthChannel);
    nsAutoCString suffix;
    GetOriginAttributesSuffix(chan, suffix);

    rv = authCache->GetAuthEntryForPath(scheme, host, port, path,
                                        suffix, &entry);
    if (NS_SUCCEEDED(rv)) {
        // if we are trying to add a header for origin server auth and if the
        // URL contains an explicit username, then try the given username first.
        // we only want to do this, however, if we know the URL requires auth
        // based on the presence of an auth cache entry for this URL (which is
        // true since we are here).  but, if the username from the URL matches
        // the username from the cache, then we should prefer the password
        // stored in the cache since that is most likely to be valid.
        if (header == nsHttp::Authorization && entry->Domain()[0] == '\0') {
            GetIdentityFromURI(0, ident);
            // if the usernames match, then clear the ident so we will pick
            // up the one from the auth cache instead.
            // when this is undesired, specify LOAD_EXPLICIT_CREDENTIALS load
            // flag.
            if (nsCRT::strcmp(ident.User(), entry->User()) == 0) {
                uint32_t loadFlags;
                if (NS_SUCCEEDED(mAuthChannel->GetLoadFlags(&loadFlags)) &&
                    !(loadFlags & nsIChannel::LOAD_EXPLICIT_CREDENTIALS)) {
                    ident.Clear();
                }
            }
        }
        bool identFromURI;
        if (ident.IsEmpty()) {
            ident.Set(entry->Identity());
            identFromURI = false;
        }
        else
            identFromURI = true;

        nsXPIDLCString temp;
        const char *creds     = entry->Creds();
        const char *challenge = entry->Challenge();
        // we can only send a preemptive Authorization header if we have either
        // stored credentials or a stored challenge from which to derive
        // credentials.  if the identity is from the URI, then we cannot use
        // the stored credentials.
        if ((!creds[0] || identFromURI) && challenge[0]) {
            nsCOMPtr<nsIHttpAuthenticator> auth;
            nsAutoCString unused;
            rv = GetAuthenticator(challenge, unused, getter_AddRefs(auth));
            if (NS_SUCCEEDED(rv)) {
                bool proxyAuth = (header == nsHttp::Proxy_Authorization);
                rv = GenCredsAndSetEntry(auth, proxyAuth, scheme, host, port,
                                         path, entry->Realm(), challenge, ident,
                                         entry->mMetaData, getter_Copies(temp));
                if (NS_SUCCEEDED(rv))
                    creds = temp.get();

                // make sure the continuation state is null since we do not
                // support mixing preemptive and 'multirequest' authentication.
                NS_IF_RELEASE(*continuationState);
            }
        }
        if (creds[0]) {
            LOG(("   adding \"%s\" request header\n", header.get()));
            if (header == nsHttp::Proxy_Authorization)
                mAuthChannel->SetProxyCredentials(nsDependentCString(creds));
            else
                mAuthChannel->SetWWWCredentials(nsDependentCString(creds));

            // suppress defensive auth prompting for this channel since we know
            // that we already prompted at least once this session.  we only do
            // this for non-proxy auth since the URL's userpass is not used for
            // proxy auth.
            if (header == nsHttp::Authorization)
                mSuppressDefensiveAuth = true;
        }
        else
            ident.Clear(); // don't remember the identity
    }
}

nsresult
nsHttpChannelAuthProvider::GetCurrentPath(nsACString &path)
{
    nsresult rv;
    nsCOMPtr<nsIURL> url = do_QueryInterface(mURI);
    if (url)
        rv = url->GetDirectory(path);
    else
        rv = mURI->GetPath(path);
    return rv;
}

NS_IMPL_ISUPPORTS(nsHttpChannelAuthProvider, nsICancelable,
                  nsIHttpChannelAuthProvider, nsIAuthPromptCallback)

} // namespace net
} // namespace mozilla
