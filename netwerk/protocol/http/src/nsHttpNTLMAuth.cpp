/* vim:set ts=4 sw=4 sts=4 et ci: */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#include <stdlib.h>
#include "nsHttp.h"
#include "nsHttpNTLMAuth.h"
#include "nsIComponentManager.h"
#include "nsIAuthModule.h"
#include "nsCOMPtr.h"
#include "plbase64.h"

//-----------------------------------------------------------------------------

#ifdef XP_WIN

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"
#include "nsIHttpChannel.h"
#include "nsIURI.h"

static const char kAllowProxies[] = "network.automatic-ntlm-auth.allow-proxies";
static const char kTrustedURIs[]  = "network.automatic-ntlm-auth.trusted-uris";

// XXX MatchesBaseURI and TestPref are duplicated in nsHttpNegotiateAuth.cpp,
// but since that file lives in a separate library we cannot directly share it.
// bug 236865 addresses this problem.

static PRBool
MatchesBaseURI(const nsCSubstring &matchScheme,
               const nsCSubstring &matchHost,
               PRInt32             matchPort,
               const char         *baseStart,
               const char         *baseEnd)
{
    // check if scheme://host:port matches baseURI

    // parse the base URI
    const char *hostStart, *schemeEnd = strstr(baseStart, "://");
    if (schemeEnd) {
        // the given scheme must match the parsed scheme exactly
        if (!matchScheme.Equals(Substring(baseStart, schemeEnd)))
            return PR_FALSE;
        hostStart = schemeEnd + 3;
    }
    else
        hostStart = baseStart;

    // XXX this does not work for IPv6-literals
    const char *hostEnd = strchr(hostStart, ':');
    if (hostEnd && hostEnd <= baseEnd) {
        // the given port must match the parsed port exactly
        int port = atoi(hostEnd + 1);
        if (matchPort != (PRInt32) port)
            return PR_FALSE;
    }
    else
        hostEnd = baseEnd;


    // if we didn't parse out a host, then assume we got a match.
    if (hostStart == hostEnd)
        return PR_TRUE;

    PRUint32 hostLen = hostEnd - hostStart;

    // matchHost must either equal host or be a subdomain of host
    if (matchHost.Length() < hostLen)
        return PR_FALSE;

    const char *end = matchHost.EndReading();
    if (PL_strncasecmp(end - hostLen, hostStart, hostLen) == 0) {
        // if matchHost ends with host from the base URI, then make sure it is
        // either an exact match, or prefixed with a dot.  we don't want
        // "foobar.com" to match "bar.com"
        if (matchHost.Length() == hostLen ||
            *(end - hostLen) == '.' ||
            *(end - hostLen - 1) == '.')
            return PR_TRUE;
    }

    return PR_FALSE;
}

static PRBool
TestPref(nsIURI *uri, const char *pref)
{
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return PR_FALSE;

    nsCAutoString scheme, host;
    PRInt32 port;

    if (NS_FAILED(uri->GetScheme(scheme)))
        return PR_FALSE;
    if (NS_FAILED(uri->GetAsciiHost(host)))
        return PR_FALSE;
    if (NS_FAILED(uri->GetPort(&port)))
        return PR_FALSE;

    char *hostList;
    if (NS_FAILED(prefs->GetCharPref(pref, &hostList)) || !hostList)
        return PR_FALSE;

    // pseudo-BNF
    // ----------
    //
    // url-list       base-url ( base-url "," LWS )*
    // base-url       ( scheme-part | host-part | scheme-part host-part )
    // scheme-part    scheme "://"
    // host-part      host [":" port]
    //
    // for example:
    //   "https://, http://office.foo.com"
    //

    char *start = hostList, *end;
    for (;;) {
        // skip past any whitespace
        while (*start == ' ' || *start == '\t')
            ++start;
        end = strchr(start, ',');
        if (!end)
            end = start + strlen(start);
        if (start == end)
            break;
        if (MatchesBaseURI(scheme, host, port, start, end))
            return PR_TRUE;
        if (*end == '\0')
            break;
        start = end + 1;
    }
    
    nsMemory::Free(hostList);
    return PR_FALSE;
}

static PRBool
CanUseSysNTLM(nsIHttpChannel *channel, PRBool isProxyAuth)
{
    // check prefs

    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return PR_FALSE;

    PRBool val;
    if (isProxyAuth) {
        if (NS_FAILED(prefs->GetBoolPref(kAllowProxies, &val)))
            val = PR_FALSE;
        return val;
    }
    else {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        if (uri && TestPref(uri, kTrustedURIs))
            return PR_TRUE;
    }

    return PR_FALSE;
}

// Dummy class for session state object.  This class doesn't hold any data.
// Instead we use its existance as a flag.  See ChallengeReceived.
class nsNTLMSessionState : public nsISupports 
{
public:
    NS_DECL_ISUPPORTS
};
NS_IMPL_ISUPPORTS0(nsNTLMSessionState)

#endif

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(nsHttpNTLMAuth, nsIHttpAuthenticator)

NS_IMETHODIMP
nsHttpNTLMAuth::ChallengeReceived(nsIHttpChannel *channel,
                                  const char     *challenge,
                                  PRBool          isProxyAuth,
                                  nsISupports   **sessionState,
                                  nsISupports   **continuationState,
                                  PRBool         *identityInvalid)
{
    LOG(("nsHttpNTLMAuth::ChallengeReceived\n"));

    // NOTE: we don't define any session state

    *identityInvalid = PR_FALSE;
    // start new auth sequence if challenge is exactly "NTLM"
    if (PL_strcasecmp(challenge, "NTLM") == 0) {
        nsCOMPtr<nsISupports> module;
#ifdef XP_WIN
        //
        // our session state is non-null to indicate that we've flagged
        // this auth domain as not accepting the system's default login.
        //
        PRBool trySysNTLM = (*sessionState == nsnull);

        //
        // on windows, we may have access to the built-in SSPI library,
        // which could be used to authenticate the user without prompting.
        // 
        // if the continuationState is null, then we may want to try using
        // the SSPI NTLM module.  however, we need to take care to only use
        // that module when speaking to a trusted host.  because the SSPI
        // may send a weak LMv1 hash of the user's password, we cannot just
        // send it to any server.
        //
        if (trySysNTLM && !*continuationState && CanUseSysNTLM(channel, isProxyAuth))
            module = do_CreateInstance(NS_AUTH_MODULE_CONTRACTID_PREFIX "sys-ntlm");

        // it's possible that there is no ntlm-sspi auth module...
        if (!module) {
            if (!*sessionState) {
                // remember the fact that we cannot use the "sys-ntlm" module,
                // so we don't ever bother trying again for this auth domain.
                *sessionState = new nsNTLMSessionState();
                if (!*sessionState)
                    return NS_ERROR_OUT_OF_MEMORY;
                NS_ADDREF(*sessionState);
            }
#else
        {
#endif 
            module = do_CreateInstance(NS_AUTH_MODULE_CONTRACTID_PREFIX "ntlm");

            // prompt user for domain, username, and password...
            *identityInvalid = PR_TRUE;
        }

        // if this fails, then it means that we cannot do NTLM auth.
        if (!module)
            return NS_ERROR_UNEXPECTED;

        // non-null continuation state implies that we failed to authenticate.
        // blow away the old authentication state, and use the new one.
        module.swap(*continuationState);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHttpNTLMAuth::GenerateCredentials(nsIHttpChannel  *httpChannel,
                                    const char      *challenge,
                                    PRBool           isProxyAuth,
                                    const PRUnichar *domain,
                                    const PRUnichar *user,
                                    const PRUnichar *pass,
                                    nsISupports    **sessionState,
                                    nsISupports    **continuationState,
                                    char           **creds)

{
    LOG(("nsHttpNTLMAuth::GenerateCredentials\n"));

    *creds = nsnull;

    nsresult rv;
    nsCOMPtr<nsIAuthModule> module = do_QueryInterface(*continuationState, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    void *inBuf, *outBuf;
    PRUint32 inBufLen, outBufLen;

    // initial challenge
    if (PL_strcasecmp(challenge, "NTLM") == 0) {
        // initialize auth module
        rv = module->Init(nsnull, nsIAuthModule::REQ_DEFAULT, domain, user, pass);
        if (NS_FAILED(rv))
            return rv;

        inBufLen = 0;
        inBuf = nsnull;
    }
    else {
        // decode challenge; skip past "NTLM " to the start of the base64
        // encoded data.
        int len = strlen(challenge);
        if (len < 6)
            return NS_ERROR_UNEXPECTED; // bogus challenge
        challenge += 5;
        len -= 5;

        // decode into the input secbuffer
        inBufLen = (len * 3)/4;      // sufficient size (see plbase64.h)
        inBuf = nsMemory::Alloc(inBufLen);
        if (!inBuf)
            return NS_ERROR_OUT_OF_MEMORY;

        // strip off any padding (see bug 230351)
        while (challenge[len - 1] == '=')
          len--;

        if (PL_Base64Decode(challenge, len, (char *) inBuf) == nsnull) {
            nsMemory::Free(inBuf);
            return NS_ERROR_UNEXPECTED; // improper base64 encoding
        }
    }

    rv = module->GetNextToken(inBuf, inBufLen, &outBuf, &outBufLen);
    if (NS_SUCCEEDED(rv)) {
        // base64 encode data in output buffer and prepend "NTLM "
        int credsLen = 5 + ((outBufLen + 2)/3)*4;
        *creds = (char *) nsMemory::Alloc(credsLen + 1);
        if (!*creds)
            rv = NS_ERROR_OUT_OF_MEMORY;
        else {
            memcpy(*creds, "NTLM ", 5);
            PL_Base64Encode((char *) outBuf, outBufLen, *creds + 5);
            (*creds)[credsLen] = '\0'; // null terminate
        }
        // OK, we are done with |outBuf|
        nsMemory::Free(outBuf);
    }

    if (inBuf)
        nsMemory::Free(inBuf);

    return rv;
}

NS_IMETHODIMP
nsHttpNTLMAuth::GetAuthFlags(PRUint32 *flags)
{
    *flags = CONNECTION_BASED | IDENTITY_INCLUDES_DOMAIN;
    return NS_OK;
}
