/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 sw=4 sts=4 cin: */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net> (original author)
 *   Christian Biesinger <cbiesinger@web.de>
 *   Google Inc.
 *   Jan Wrobel <wrobel@blues.ath.cx>
 *   Jan Odvarko <odvarko@gmail.com>
 *   Dave Camp <dcamp@mozilla.com>
 *   Honza Bambas <honzab@firemni.cz>
 *   Wellington Fernando de Macedo <wfernandom2004@gmail.com>
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

#include "nsHttpChannelAuthProvider.h"
#include "nsNetUtil.h"
#include "nsHttpHandler.h"
#include "nsIHttpAuthenticator.h"
#include "nsIAuthPrompt2.h"
#include "nsIAuthPromptProvider.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsEscape.h"
#include "nsAuthInformationHolder.h"
#include "nsIStringBundle.h"
#include "nsIPrompt.h"

nsHttpChannelAuthProvider::nsHttpChannelAuthProvider()
    : mAuthChannel(nsnull)
    , mProxyAuthContinuationState(nsnull)
    , mAuthContinuationState(nsnull)
    , mProxyAuth(PR_FALSE)
    , mTriedProxyAuth(PR_FALSE)
    , mTriedHostAuth(PR_FALSE)
    , mSuppressDefensiveAuth(PR_FALSE)
{
    // grab a reference to the handler to ensure that it doesn't go away.
    nsHttpHandler *handler = gHttpHandler;
    NS_ADDREF(handler);
}

nsHttpChannelAuthProvider::~nsHttpChannelAuthProvider()
{
    NS_ASSERTION(!mAuthChannel, "Disconnect wasn't called");

    // release our reference to the handler
    nsHttpHandler *handler = gHttpHandler;
    NS_RELEASE(handler);
}

NS_IMETHODIMP
nsHttpChannelAuthProvider::Init(nsIHttpAuthenticableChannel *channel)
{
    NS_ASSERTION(channel, "channel expected!");

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

    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannelAuthProvider::ProcessAuthentication(PRUint32 httpStatus,
                                                 bool     SSLConnectFailed)
{
    LOG(("nsHttpChannelAuthProvider::ProcessAuthentication "
         "[this=%p channel=%p code=%u SSLConnectFailed=%d]\n",
         this, mAuthChannel, httpStatus, SSLConnectFailed));

    NS_ASSERTION(mAuthChannel, "Channel not initialized");

    nsCOMPtr<nsIProxyInfo> proxyInfo;
    nsresult rv = mAuthChannel->GetProxyInfo(getter_AddRefs(proxyInfo));
    if (NS_FAILED(rv)) return rv;
    if (proxyInfo) {
        mProxyInfo = do_QueryInterface(proxyInfo);
        if (!mProxyInfo) return NS_ERROR_NO_INTERFACE;
    }

    PRUint32 loadFlags;
    rv = mAuthChannel->GetLoadFlags(&loadFlags);
    if (NS_FAILED(rv)) return rv;

    if (loadFlags & nsIRequest::LOAD_ANONYMOUS) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsCAutoString challenges;
    mProxyAuth = (httpStatus == 407);

    rv = PrepareForAuthentication(mProxyAuth);
    if (NS_FAILED(rv))
        return rv;

    if (mProxyAuth) {
        // only allow a proxy challenge if we have a proxy server configured.
        // otherwise, we could inadvertantly expose the user's proxy
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

    nsCAutoString creds;
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
nsHttpChannelAuthProvider::AddAuthorizationHeaders()
{
    LOG(("nsHttpChannelAuthProvider::AddAuthorizationHeaders? "
         "[this=%p channel=%p]\n", this, mAuthChannel));

    NS_ASSERTION(mAuthChannel, "Channel not initialized");

    nsCOMPtr<nsIProxyInfo> proxyInfo;
    nsresult rv = mAuthChannel->GetProxyInfo(getter_AddRefs(proxyInfo));
    if (NS_FAILED(rv)) return rv;
    if (proxyInfo) {
        mProxyInfo = do_QueryInterface(proxyInfo);
        if (!mProxyInfo) return NS_ERROR_NO_INTERFACE;
    }

    PRUint32 loadFlags;
    rv = mAuthChannel->GetLoadFlags(&loadFlags);
    if (NS_FAILED(rv)) return rv;

    if (loadFlags & nsIRequest::LOAD_ANONYMOUS) {
        return NS_OK;
    }

    // this getter never fails
    nsHttpAuthCache *authCache = gHttpHandler->AuthCache();

    // check if proxy credentials should be sent
    const char *proxyHost = ProxyHost();
    if (proxyHost && UsingHttpProxy())
        SetAuthorizationHeader(authCache, nsHttp::Proxy_Authorization,
                               "http", proxyHost, ProxyPort(),
                               nsnull, // proxy has no path
                               mProxyIdent);

    // check if server credentials should be sent
    nsCAutoString path, scheme;
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

    NS_ASSERTION(mAuthChannel, "Channel not initialized");

    // we've been called because it has been determined that this channel is
    // getting loaded without taking the userpass from the URL.  if the URL
    // contained a userpass, then (provided some other conditions are true),
    // we'll give the user an opportunity to abort the channel as this might be
    // an attempt to spoof a different site (see bug 232567).
    if (!ConfirmAuth(NS_LITERAL_STRING("SuperfluousAuth"), PR_TRUE)) {
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
    NS_ASSERTION(mAuthChannel, "Channel not initialized");

    if (mAsyncPromptAuthCancelable) {
        mAsyncPromptAuthCancelable->Cancel(status);
        mAsyncPromptAuthCancelable = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannelAuthProvider::Disconnect(nsresult status)
{
    mAuthChannel = nsnull;

    if (mAsyncPromptAuthCancelable) {
        mAsyncPromptAuthCancelable->Cancel(status);
        mAsyncPromptAuthCancelable = nsnull;
    }

    NS_IF_RELEASE(mProxyAuthContinuationState);
    NS_IF_RELEASE(mAuthContinuationState);

    return NS_OK;
}

// buf contains "domain\user"
static void
ParseUserDomain(PRUnichar *buf,
                const PRUnichar **user,
                const PRUnichar **domain)
{
    PRUnichar *p = buf;
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
         PRUint32 authFlags,
         PRUnichar *userBuf,
         PRUnichar *passBuf)
{
    const PRUnichar *user = userBuf;
    const PRUnichar *domain = nsnull;

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

    PRUint32 promptReason;
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
                                               PRInt32               port,
                                               const char           *directory,
                                               const char           *realm,
                                               const char           *challenge,
                                               const nsHttpAuthIdentity &ident,
                                               nsCOMPtr<nsISupports>    &sessionState,
                                               char                    **result)
{
    nsresult rv;
    PRUint32 authFlags;

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

    PRUint32 generateFlags;
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
    nsHttpAuthCache *authCache = gHttpHandler->AuthCache();

    // create a cache entry.  we do this even though we don't yet know that
    // these credentials are valid b/c we need to avoid prompting the user
    // more than once in case the credentials are valid.
    //
    // if the credentials are not reusable, then we don't bother sticking
    // them in the auth cache.
    rv = authCache->SetAuthEntry(scheme, host, port, directory, realm,
                                 saveCreds ? *result : nsnull,
                                 saveChallenge ? challenge : nsnull,
                                 saveIdentity ? &ident : nsnull,
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

    nsCAutoString contractId;
    contractId.Assign(NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX);
    contractId.Append(mProxyAuthType);

    nsresult rv;
    nsCOMPtr<nsIHttpAuthenticator> precedingAuth =
        do_GetService(contractId.get(), &rv);
    if (NS_FAILED(rv))
        return rv;

    PRUint32 precedingAuthFlags;
    rv = precedingAuth->GetAuthFlags(&precedingAuthFlags);
    if (NS_FAILED(rv))
        return rv;

    if (!(precedingAuthFlags & nsIHttpAuthenticator::REQUEST_BASED)) {
        nsCAutoString challenges;
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
    nsCAutoString challenge;

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
        if ((eol = strchr(p, '\n')) != nsnull)
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
                gotCreds = PR_TRUE;
                *currentAuthType = authType;

                break;
            }
            else if (rv == NS_ERROR_IN_PROGRESS) {
                // authentication prompt has been invoked and result is
                // expected asynchronously, save current challenge being
                // processed and all remaining challenges to use later in
                // OnAuthAvailable and now immediately return
                mCurrentChallenge = challenge;
                mRemainingChallenges = eol ? eol+1 : nsnull;
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
                                                   PRInt32&             port,
                                                   nsCSubstring&        path,
                                                   nsHttpAuthIdentity*& ident,
                                                   nsISupports**&       continuationState)
{
    if (proxyAuth) {
        NS_ASSERTION (UsingHttpProxy(),
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
    nsHttpAuthCache *authCache = gHttpHandler->AuthCache();

    PRUint32 authFlags;
    nsresult rv = auth->GetAuthFlags(&authFlags);
    if (NS_FAILED(rv)) return rv;

    nsCAutoString realm;
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
    PRInt32 port;
    nsHttpAuthIdentity *ident;
    nsCAutoString path, scheme;
    bool identFromURI = false;
    nsISupports **continuationState;

    rv = GetAuthorizationMembers(proxyAuth, scheme, host, port,
                                 path, ident, continuationState);
    if (NS_FAILED(rv)) return rv;

    if (!proxyAuth) {
        // if this is the first challenge, then try using the identity
        // specified in the URL.
        if (mIdent.IsEmpty()) {
            GetIdentityFromURI(authFlags, mIdent);
            identFromURI = !mIdent.IsEmpty();
        }
    }

    //
    // if we already tried some credentials for this transaction, then
    // we need to possibly clear them from the cache, unless the credentials
    // in the cache have changed, in which case we'd want to give them a
    // try instead.
    //
    nsHttpAuthEntry *entry = nsnull;
    authCache->GetAuthEntryForDomain(scheme.get(), host, port,
                                     realm.get(), &entry);

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
                LOG(("  clearing bad auth cache entry\n"));
                // ok, we've already tried this user identity, so clear the
                // corresponding entry from the auth cache.
                authCache->ClearAuthEntry(scheme.get(), host,
                                          port, realm.get());
                entry = nsnull;
                ident->Clear();
            }
            else if (!identFromURI ||
                     nsCRT::strcmp(ident->User(),
                                   entry->Identity().User()) == 0) {
                LOG(("  taking identity from auth cache\n"));
                // the password from the auth cache is more likely to be
                // correct than the one in the URL.  at least, we know that it
                // works with the given username.  it is possible for a server
                // to distinguish logons based on the supplied password alone,
                // but that would be quite unusual... and i don't think we need
                // to worry about such unorthodox cases.
                ident->Set(entry->Identity());
                identFromURI = PR_FALSE;
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
            PRUint32 level = nsIAuthPrompt2::LEVEL_NONE;
            if (mUsingSSL)
                level = nsIAuthPrompt2::LEVEL_SECURE;
            else if (authFlags & nsIHttpAuthenticator::IDENTITY_ENCRYPTED)
                level = nsIAuthPrompt2::LEVEL_PW_ENCRYPTED;

            // at this point we are forced to interact with the user to get
            // their username and password for this domain.
            rv = PromptForIdentity(level, proxyAuth, realm.get(),
                                   authType, authFlags, *ident);
            if (NS_FAILED(rv)) return rv;
            identFromURI = PR_FALSE;
        }
    }

    if (identFromURI) {
        // Warn the user before automatically using the identity from the URL
        // to automatically log them into a site (see bug 232567).
        if (!ConfirmAuth(NS_LITERAL_STRING("AutomaticAuth"), PR_FALSE)) {
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

inline void
GetAuthType(const char *challenge, nsCString &authType)
{
    const char *p;

    // get the challenge type
    if ((p = strchr(challenge, ' ')) != nsnull)
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

    nsCAutoString contractid;
    contractid.Assign(NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX);
    contractid.Append(authType);

    return CallGetService(contractid.get(), auth);
}

void
nsHttpChannelAuthProvider::GetIdentityFromURI(PRUint32            authFlags,
                                              nsHttpAuthIdentity &ident)
{
    LOG(("nsHttpChannelAuthProvider::GetIdentityFromURI [this=%p channel=%p]\n",
        this, mAuthChannel));

    nsAutoString userBuf;
    nsAutoString passBuf;

    // XXX i18n
    nsCAutoString buf;
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
        SetIdent(ident, authFlags, (PRUnichar *) userBuf.get(),
                 (PRUnichar *) passBuf.get());
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
            has_quote = PR_TRUE;
            p++;
        }

        const char *end = p;
        while (*end && has_quote) {
           // Loop through all the string characters to find the closing
           // quote, ignoring escaped quotes.
            if (*end == '"' && end[-1] != '\\')
                break;
            ++end;
        }

        if (!has_quote)
            end = strchr(p, ' ');
        if (end)
            realm.Assign(p, end - p);
        else
            realm.Assign(p);
    }
}


class nsHTTPAuthInformation : public nsAuthInformationHolder {
public:
    nsHTTPAuthInformation(PRUint32 aFlags, const nsString& aRealm,
                          const nsCString& aAuthType)
        : nsAuthInformationHolder(aFlags, aRealm, aAuthType) {}

    void SetToHttpAuthIdentity(PRUint32 authFlags,
                               nsHttpAuthIdentity& identity);
};

void
nsHTTPAuthInformation::SetToHttpAuthIdentity(PRUint32 authFlags,
                                             nsHttpAuthIdentity& identity)
{
    identity.Set(Domain().get(), User().get(), Password().get());
}

nsresult
nsHttpChannelAuthProvider::PromptForIdentity(PRUint32            level,
                                             bool                proxyAuth,
                                             const char         *realm,
                                             const char         *authType,
                                             PRUint32            authFlags,
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
    PRUint32 promptFlags = 0;
    if (proxyAuth)
    {
        promptFlags |= nsIAuthInformation::AUTH_PROXY;
        if (mTriedProxyAuth)
            promptFlags |= nsIAuthInformation::PREVIOUS_FAILED;
        mTriedProxyAuth = PR_TRUE;
    }
    else {
        promptFlags |= nsIAuthInformation::AUTH_HOST;
        if (mTriedHostAuth)
            promptFlags |= nsIAuthInformation::PREVIOUS_FAILED;
        mTriedHostAuth = PR_TRUE;
    }

    if (authFlags & nsIHttpAuthenticator::IDENTITY_INCLUDES_DOMAIN)
        promptFlags |= nsIAuthInformation::NEED_DOMAIN;

    nsRefPtr<nsHTTPAuthInformation> holder =
        new nsHTTPAuthInformation(promptFlags, realmU,
                                  nsDependentCString(authType));
    if (!holder)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIChannel> channel(do_QueryInterface(mAuthChannel, &rv));
    if (NS_FAILED(rv)) return rv;

    rv =
        authPrompt->AsyncPromptAuth(channel, this, nsnull, level, holder,
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
        mSuppressDefensiveAuth = PR_TRUE;

    return rv;
}

NS_IMETHODIMP nsHttpChannelAuthProvider::OnAuthAvailable(nsISupports *aContext,
                                                         nsIAuthInformation *aAuthInfo)
{
    LOG(("nsHttpChannelAuthProvider::OnAuthAvailable [this=%p channel=%p]",
        this, mAuthChannel));

    mAsyncPromptAuthCancelable = nsnull;
    if (!mAuthChannel)
        return NS_OK;

    nsresult rv;

    const char *host;
    PRInt32 port;
    nsHttpAuthIdentity *ident;
    nsCAutoString path, scheme;
    nsISupports **continuationState;
    rv = GetAuthorizationMembers(mProxyAuth, scheme, host, port,
                                 path, ident, continuationState);
    if (NS_FAILED(rv))
        OnAuthCancelled(aContext, PR_FALSE);

    nsCAutoString realm;
    ParseRealm(mCurrentChallenge.get(), realm);

    nsHttpAuthCache *authCache = gHttpHandler->AuthCache();
    nsHttpAuthEntry *entry = nsnull;
    authCache->GetAuthEntryForDomain(scheme.get(), host, port,
                                     realm.get(), &entry);

    nsCOMPtr<nsISupports> sessionStateGrip;
    if (entry)
        sessionStateGrip = entry->mMetaData;

    nsAuthInformationHolder* holder =
            static_cast<nsAuthInformationHolder*>(aAuthInfo);
    ident->Set(holder->Domain().get(),
               holder->User().get(),
               holder->Password().get());

    nsCAutoString unused;
    nsCOMPtr<nsIHttpAuthenticator> auth;
    rv = GetAuthenticator(mCurrentChallenge.get(), unused,
                          getter_AddRefs(auth));
    if (NS_FAILED(rv)) {
        NS_ASSERTION(PR_FALSE, "GetAuthenticator failed");
        OnAuthCancelled(aContext, PR_TRUE);
        return NS_OK;
    }

    nsXPIDLCString creds;
    rv = GenCredsAndSetEntry(auth, mProxyAuth,
                             scheme.get(), host, port, path.get(),
                             realm.get(), mCurrentChallenge.get(), *ident,
                             sessionStateGrip, getter_Copies(creds));

    mCurrentChallenge.Truncate();
    if (NS_FAILED(rv)) {
        OnAuthCancelled(aContext, PR_TRUE);
        return NS_OK;
    }

    return ContinueOnAuthAvailable(creds);
}

NS_IMETHODIMP nsHttpChannelAuthProvider::OnAuthCancelled(nsISupports *aContext,
                                                         bool        userCancel)
{
    LOG(("nsHttpChannelAuthProvider::OnAuthCancelled [this=%p channel=%p]",
        this, mAuthChannel));

    mAsyncPromptAuthCancelable = nsnull;
    if (!mAuthChannel)
        return NS_OK;

    if (userCancel) {
        if (!mRemainingChallenges.IsEmpty()) {
            // there are still some challenges to process, do so
            nsresult rv;

            nsCAutoString creds;
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

    PRUint32 loadFlags;
    nsresult rv = mAuthChannel->GetLoadFlags(&loadFlags);
    if (NS_FAILED(rv))
        return PR_TRUE;

    if (mSuppressDefensiveAuth ||
        !(loadFlags & nsIChannel::LOAD_INITIAL_DOCUMENT_URI))
        return PR_TRUE;

    nsCAutoString userPass;
    rv = mURI->GetUserPass(userPass);
    if (NS_FAILED(rv) ||
        (userPass.Length() < gHttpHandler->PhishyUserPassLength()))
        return PR_TRUE;

    // we try to confirm by prompting the user.  if we cannot do so, then
    // assume the user said ok.  this is done to keep things working in
    // embedded builds, where the string bundle might not be present, etc.

    nsCOMPtr<nsIStringBundleService> bundleService =
            do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    if (!bundleService)
        return PR_TRUE;

    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(bundle));
    if (!bundle)
        return PR_TRUE;

    nsCAutoString host;
    rv = mURI->GetHost(host);
    if (NS_FAILED(rv))
        return PR_TRUE;

    nsCAutoString user;
    rv = mURI->GetUsername(user);
    if (NS_FAILED(rv))
        return PR_TRUE;

    NS_ConvertUTF8toUTF16 ucsHost(host), ucsUser(user);
    const PRUnichar *strs[2] = { ucsHost.get(), ucsUser.get() };

    nsXPIDLString msg;
    bundle->FormatStringFromName(bundleKey.get(), strs, 2, getter_Copies(msg));
    if (!msg)
        return PR_TRUE;

    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    rv = mAuthChannel->GetNotificationCallbacks(getter_AddRefs(callbacks));
    if (NS_FAILED(rv))
        return PR_TRUE;

    nsCOMPtr<nsILoadGroup> loadGroup;
    rv = mAuthChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (NS_FAILED(rv))
        return PR_TRUE;

    nsCOMPtr<nsIPrompt> prompt;
    NS_QueryNotificationCallbacks(callbacks, loadGroup, NS_GET_IID(nsIPrompt),
                                  getter_AddRefs(prompt));
    if (!prompt)
        return PR_TRUE;

    // do not prompt again
    mSuppressDefensiveAuth = PR_TRUE;

    bool confirmed;
    if (doYesNoPrompt) {
        PRInt32 choice;
        // The actual value is irrelevant but we shouldn't be handing out
        // malformed JSBools to XPConnect.
        bool checkState = false;
        rv = prompt->ConfirmEx(nsnull, msg,
                               nsIPrompt::BUTTON_POS_1_DEFAULT +
                               nsIPrompt::STD_YES_NO_BUTTONS,
                               nsnull, nsnull, nsnull, nsnull,
                               &checkState, &choice);
        if (NS_FAILED(rv))
            return PR_TRUE;

        confirmed = choice == 0;
    }
    else {
        rv = prompt->Confirm(nsnull, msg, &confirmed);
        if (NS_FAILED(rv))
            return PR_TRUE;
    }

    return confirmed;
}

void
nsHttpChannelAuthProvider::SetAuthorizationHeader(nsHttpAuthCache    *authCache,
                                                  nsHttpAtom          header,
                                                  const char         *scheme,
                                                  const char         *host,
                                                  PRInt32             port,
                                                  const char         *path,
                                                  nsHttpAuthIdentity &ident)
{
    nsHttpAuthEntry *entry = nsnull;
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

    rv = authCache->GetAuthEntryForPath(scheme, host, port, path, &entry);
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
            if (nsCRT::strcmp(ident.User(), entry->User()) == 0)
                ident.Clear();
        }
        bool identFromURI;
        if (ident.IsEmpty()) {
            ident.Set(entry->Identity());
            identFromURI = PR_FALSE;
        }
        else
            identFromURI = PR_TRUE;

        nsXPIDLCString temp;
        const char *creds     = entry->Creds();
        const char *challenge = entry->Challenge();
        // we can only send a preemptive Authorization header if we have either
        // stored credentials or a stored challenge from which to derive
        // credentials.  if the identity is from the URI, then we cannot use
        // the stored credentials.
        if ((!creds[0] || identFromURI) && challenge[0]) {
            nsCOMPtr<nsIHttpAuthenticator> auth;
            nsCAutoString unused;
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
                mSuppressDefensiveAuth = PR_TRUE;
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

NS_IMPL_ISUPPORTS3(nsHttpChannelAuthProvider, nsICancelable,
                   nsIHttpChannelAuthProvider, nsIAuthPromptCallback)
