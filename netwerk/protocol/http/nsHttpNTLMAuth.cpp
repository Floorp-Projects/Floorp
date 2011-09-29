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
 *   Jim Mathies <jmathies@mozilla.com>
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

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"
#include "nsIHttpAuthenticableChannel.h"
#include "nsIURI.h"

static const char kAllowProxies[] = "network.automatic-ntlm-auth.allow-proxies";
static const char kTrustedURIs[]  = "network.automatic-ntlm-auth.trusted-uris";
static const char kForceGeneric[] = "network.auth.force-generic-ntlm";

// XXX MatchesBaseURI and TestPref are duplicated in nsHttpNegotiateAuth.cpp,
// but since that file lives in a separate library we cannot directly share it.
// bug 236865 addresses this problem.

static bool
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
    if (hostEnd && hostEnd < baseEnd) {
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

static bool
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

// Check to see if we should use our generic (internal) NTLM auth module.
static bool
ForceGenericNTLM()
{
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return PR_FALSE;
    bool flag = false;

    if (NS_FAILED(prefs->GetBoolPref(kForceGeneric, &flag)))
        flag = PR_FALSE;

    LOG(("Force use of generic ntlm auth module: %d\n", flag));
    return flag;
}

// Check to see if we should use default credentials for this host or proxy.
static bool
CanUseDefaultCredentials(nsIHttpAuthenticableChannel *channel,
                         bool isProxyAuth)
{
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return PR_FALSE;

    if (isProxyAuth) {
        bool val;
        if (NS_FAILED(prefs->GetBoolPref(kAllowProxies, &val)))
            val = PR_FALSE;
        LOG(("Default credentials allowed for proxy: %d\n", val));
        return val;
    }

    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    bool isTrustedHost = (uri && TestPref(uri, kTrustedURIs));
    LOG(("Default credentials allowed for host: %d\n", isTrustedHost));
    return isTrustedHost;
}

// Dummy class for session state object.  This class doesn't hold any data.
// Instead we use its existence as a flag.  See ChallengeReceived.
class nsNTLMSessionState : public nsISupports
{
public:
    NS_DECL_ISUPPORTS
};
NS_IMPL_ISUPPORTS0(nsNTLMSessionState)

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(nsHttpNTLMAuth, nsIHttpAuthenticator)

NS_IMETHODIMP
nsHttpNTLMAuth::ChallengeReceived(nsIHttpAuthenticableChannel *channel,
                                  const char     *challenge,
                                  bool            isProxyAuth,
                                  nsISupports   **sessionState,
                                  nsISupports   **continuationState,
                                  bool           *identityInvalid)
{
    LOG(("nsHttpNTLMAuth::ChallengeReceived [ss=%p cs=%p]\n",
         *sessionState, *continuationState));

    // NOTE: we don't define any session state, but we do use the pointer.

    *identityInvalid = PR_FALSE;

    // Start a new auth sequence if the challenge is exactly "NTLM".
    // If native NTLM auth apis are available and enabled through prefs,
    // try to use them.
    if (PL_strcasecmp(challenge, "NTLM") == 0) {
        nsCOMPtr<nsISupports> module;

        // Check to see if we should default to our generic NTLM auth module
        // through UseGenericNTLM. (We use native auth by default if the
        // system provides it.) If *sessionState is non-null, we failed to
        // instantiate a native NTLM module the last time, so skip trying again.
        bool forceGeneric = ForceGenericNTLM();
        if (!forceGeneric && !*sessionState) {
            // Check for approved default credentials hosts and proxies. If 
            // *continuationState is non-null, the last authentication attempt
            // failed so skip default credential use.
            if (!*continuationState && CanUseDefaultCredentials(channel, isProxyAuth)) {
                // Try logging in with the user's default credentials. If
                // successful, |identityInvalid| is false, which will trigger
                // a default credentials attempt once we return.
                module = do_CreateInstance(NS_AUTH_MODULE_CONTRACTID_PREFIX "sys-ntlm");
            }
#ifdef XP_WIN
            else {
                // Try to use native NTLM and prompt the user for their domain,
                // username, and password. (only supported by windows nsAuthSSPI module.)
                // Note, for servers that use LMv1 a weak hash of the user's password
                // will be sent. We rely on windows internal apis to decide whether
                // we should support this older, less secure version of the protocol.
                module = do_CreateInstance(NS_AUTH_MODULE_CONTRACTID_PREFIX "sys-ntlm");
                *identityInvalid = PR_TRUE;
            }
#endif // XP_WIN
#ifdef PR_LOGGING
            if (!module)
                LOG(("Native sys-ntlm auth module not found.\n"));
#endif
        }

#ifdef XP_WIN
        // On windows, never fall back unless the user has specifically requested so.
        if (!forceGeneric && !module)
            return NS_ERROR_UNEXPECTED;
#endif

        // If no native support was available. Fall back on our internal NTLM implementation.
        if (!module) {
            if (!*sessionState) {
                // Remember the fact that we cannot use the "sys-ntlm" module,
                // so we don't ever bother trying again for this auth domain.
                *sessionState = new nsNTLMSessionState();
                if (!*sessionState)
                    return NS_ERROR_OUT_OF_MEMORY;
                NS_ADDREF(*sessionState);
            }

            // Use our internal NTLM implementation. Note, this is less secure,
            // see bug 520607 for details.
            LOG(("Trying to fall back on internal ntlm auth.\n"));
            module = do_CreateInstance(NS_AUTH_MODULE_CONTRACTID_PREFIX "ntlm");

            // Prompt user for domain, username, and password.
            *identityInvalid = PR_TRUE;
        }

        // If this fails, then it means that we cannot do NTLM auth.
        if (!module) {
            LOG(("No ntlm auth modules available.\n"));
            return NS_ERROR_UNEXPECTED;
        }

        // A non-null continuation state implies that we failed to authenticate.
        // Blow away the old authentication state, and use the new one.
        module.swap(*continuationState);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHttpNTLMAuth::GenerateCredentials(nsIHttpAuthenticableChannel *authChannel,
                                    const char      *challenge,
                                    bool             isProxyAuth,
                                    const PRUnichar *domain,
                                    const PRUnichar *user,
                                    const PRUnichar *pass,
                                    nsISupports    **sessionState,
                                    nsISupports    **continuationState,
                                    PRUint32       *aFlags,
                                    char           **creds)

{
    LOG(("nsHttpNTLMAuth::GenerateCredentials\n"));

    *creds = nsnull;
    *aFlags = 0;

    // if user or password is empty, ChallengeReceived returned
    // identityInvalid = PR_FALSE, that means we are using default user
    // credentials; see  nsAuthSSPI::Init method for explanation of this 
    // condition
    if (!user || !pass)
        *aFlags = USING_INTERNAL_IDENTITY;

    nsresult rv;
    nsCOMPtr<nsIAuthModule> module = do_QueryInterface(*continuationState, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    void *inBuf, *outBuf;
    PRUint32 inBufLen, outBufLen;

    // initial challenge
    if (PL_strcasecmp(challenge, "NTLM") == 0) {
        // NTLM service name format is 'HTTP@host' for both http and https
        nsCOMPtr<nsIURI> uri;
        rv = authChannel->GetURI(getter_AddRefs(uri));
        if (NS_FAILED(rv))
            return rv;
        nsCAutoString serviceName, host;
        rv = uri->GetAsciiHost(host);
        if (NS_FAILED(rv))
            return rv;
        serviceName.AppendLiteral("HTTP@");
        serviceName.Append(host);
        // initialize auth module
        rv = module->Init(serviceName.get(), nsIAuthModule::REQ_DEFAULT, domain, user, pass);
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

        // strip off any padding (see bug 230351)
        while (challenge[len - 1] == '=')
          len--;

        // decode into the input secbuffer
        inBufLen = (len * 3)/4;      // sufficient size (see plbase64.h)
        inBuf = nsMemory::Alloc(inBufLen);
        if (!inBuf)
            return NS_ERROR_OUT_OF_MEMORY;

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
    *flags = CONNECTION_BASED | IDENTITY_INCLUDES_DOMAIN | IDENTITY_ENCRYPTED;
    return NS_OK;
}
