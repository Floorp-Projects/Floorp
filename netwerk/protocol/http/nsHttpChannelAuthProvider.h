/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set et cin ts=4 sw=4 sts=4: */
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
 *   Darin Fisher <darin@netscape.com> (original author)
 *   Christian Biesinger <cbiesinger@web.de>
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

#ifndef nsHttpChannelAuthProvider_h__
#define nsHttpChannelAuthProvider_h__

#include "nsHttp.h"
#include "nsIHttpChannelAuthProvider.h"
#include "nsIAuthPromptCallback.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIHttpAuthenticableChannel.h"
#include "nsIURI.h"
#include "nsHttpAuthCache.h"
#include "nsProxyInfo.h"

class nsIHttpAuthenticator;

class nsHttpChannelAuthProvider : public nsIHttpChannelAuthProvider
                                , public nsIAuthPromptCallback
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICANCELABLE
    NS_DECL_NSIHTTPCHANNELAUTHPROVIDER
    NS_DECL_NSIAUTHPROMPTCALLBACK

    nsHttpChannelAuthProvider();
    virtual ~nsHttpChannelAuthProvider();

private:
    const char *ProxyHost() const
    { return mProxyInfo ? mProxyInfo->Host().get() : nsnull; }

    PRInt32     ProxyPort() const
    { return mProxyInfo ? mProxyInfo->Port() : -1; }

    const char *Host() const      { return mHost.get(); }
    PRInt32     Port() const      { return mPort; }
    bool        UsingSSL() const  { return mUsingSSL; }

    bool        UsingHttpProxy() const
    { return !!(mProxyInfo && !nsCRT::strcmp(mProxyInfo->Type(), "http")); }

    nsresult PrepareForAuthentication(bool proxyAuth);
    nsresult GenCredsAndSetEntry(nsIHttpAuthenticator *, bool proxyAuth,
                                 const char *scheme, const char *host,
                                 PRInt32 port, const char *dir,
                                 const char *realm, const char *challenge,
                                 const nsHttpAuthIdentity &ident,
                                 nsCOMPtr<nsISupports> &session, char **result);
    nsresult GetAuthenticator(const char *challenge, nsCString &scheme,
                              nsIHttpAuthenticator **auth);
    void     ParseRealm(const char *challenge, nsACString &realm);
    void     GetIdentityFromURI(PRUint32 authFlags, nsHttpAuthIdentity&);
    /**
     * Following three methods return NS_ERROR_IN_PROGRESS when
     * nsIAuthPrompt2.asyncPromptAuth method is called. This result indicates
     * the user's decision will be gathered in a callback and is not an actual
     * error.
     */
    nsresult GetCredentials(const char *challenges, bool proxyAuth,
                            nsAFlatCString &creds);
    nsresult GetCredentialsForChallenge(const char *challenge,
                                        const char *scheme,  bool proxyAuth,
                                        nsIHttpAuthenticator *auth,
                                        nsAFlatCString &creds);
    nsresult PromptForIdentity(PRUint32 level, bool proxyAuth,
                               const char *realm, const char *authType,
                               PRUint32 authFlags, nsHttpAuthIdentity &);

    bool     ConfirmAuth(const nsString &bundleKey, bool doYesNoPrompt);
    void     SetAuthorizationHeader(nsHttpAuthCache *, nsHttpAtom header,
                                    const char *scheme, const char *host,
                                    PRInt32 port, const char *path,
                                    nsHttpAuthIdentity &ident);
    nsresult GetCurrentPath(nsACString &);
    /**
     * Return all information needed to build authorization information,
     * all parameters except proxyAuth are out parameters. proxyAuth specifies
     * with what authorization we work (WWW or proxy).
     */
    nsresult GetAuthorizationMembers(bool proxyAuth, nsCSubstring& scheme,
                                     const char*& host, PRInt32& port,
                                     nsCSubstring& path,
                                     nsHttpAuthIdentity*& ident,
                                     nsISupports**& continuationState);
    /**
     * Method called to resume suspended transaction after we got credentials
     * from the user. Called from OnAuthAvailable callback or OnAuthCancelled
     * when credentials for next challenge were obtained synchronously.
     */
    nsresult ContinueOnAuthAvailable(const nsCSubstring& creds);

    nsresult DoRedirectChannelToHttps();

    /**
     * A function that takes care of reading STS headers and enforcing STS 
     * load rules.  After a secure channel is erected, STS requires the channel
     * to be trusted or any STS header data on the channel is ignored.
     * This is called from ProcessResponse.
     */
    nsresult ProcessSTSHeader();

private:
    nsIHttpAuthenticableChannel      *mAuthChannel;  // weak ref

    nsCOMPtr<nsIURI>                  mURI;
    nsCOMPtr<nsProxyInfo>             mProxyInfo;
    nsCString                         mHost;
    PRInt32                           mPort;
    bool                              mUsingSSL;

    nsISupports                      *mProxyAuthContinuationState;
    nsCString                         mProxyAuthType;
    nsISupports                      *mAuthContinuationState;
    nsCString                         mAuthType;
    nsHttpAuthIdentity                mIdent;
    nsHttpAuthIdentity                mProxyIdent;

    // Reference to the prompt waiting in prompt queue. The channel is
    // responsible to call its cancel method when user in any way cancels
    // this request.
    nsCOMPtr<nsICancelable>           mAsyncPromptAuthCancelable;
    // Saved in GetCredentials when prompt is asynchronous, the first challenge
    // we obtained from the server with 401/407 response, will be processed in
    // OnAuthAvailable callback.
    nsCString                         mCurrentChallenge;
    // Saved in GetCredentials when prompt is asynchronous, remaning challenges
    // we have to process when user cancels the auth dialog for the current
    // challenge.
    nsCString                         mRemainingChallenges;

    // True when we need to authenticate to proxy, i.e. when we get 407
    // response. Used in OnAuthAvailable and OnAuthCancelled callbacks.
    PRUint32                          mProxyAuth                : 1;
    PRUint32                          mTriedProxyAuth           : 1;
    PRUint32                          mTriedHostAuth            : 1;
    PRUint32                          mSuppressDefensiveAuth    : 1;
};

#endif // nsHttpChannelAuthProvider_h__
