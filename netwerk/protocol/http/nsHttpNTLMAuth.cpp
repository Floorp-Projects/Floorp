/* vim:set ts=4 sw=4 sts=4 et ci: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "nsHttp.h"
#include "nsHttpNTLMAuth.h"
#include "nsIComponentManager.h"
#include "nsIAuthModule.h"
#include "nsCOMPtr.h"
#include "plbase64.h"
#include "prnetdb.h"

//-----------------------------------------------------------------------------

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"
#include "nsIHttpAuthenticableChannel.h"
#include "nsIURI.h"
#include "nsIX509Cert.h"
#include "nsISSLStatus.h"
#include "nsISSLStatusProvider.h"
#include "mozilla/Attributes.h"

static const char kAllowProxies[] = "network.automatic-ntlm-auth.allow-proxies";
static const char kAllowNonFqdn[] = "network.automatic-ntlm-auth.allow-non-fqdn";
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
            return false;
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
            return false;
    }
    else
        hostEnd = baseEnd;


    // if we didn't parse out a host, then assume we got a match.
    if (hostStart == hostEnd)
        return true;

    PRUint32 hostLen = hostEnd - hostStart;

    // matchHost must either equal host or be a subdomain of host
    if (matchHost.Length() < hostLen)
        return false;

    const char *end = matchHost.EndReading();
    if (PL_strncasecmp(end - hostLen, hostStart, hostLen) == 0) {
        // if matchHost ends with host from the base URI, then make sure it is
        // either an exact match, or prefixed with a dot.  we don't want
        // "foobar.com" to match "bar.com"
        if (matchHost.Length() == hostLen ||
            *(end - hostLen) == '.' ||
            *(end - hostLen - 1) == '.')
            return true;
    }

    return false;
}

static bool
IsNonFqdn(nsIURI *uri)
{
    nsCAutoString host;
    PRNetAddr addr;

    if (NS_FAILED(uri->GetAsciiHost(host)))
        return false;

    // return true if host does not contain a dot and is not an ip address
    return !host.IsEmpty() && host.FindChar('.') == kNotFound &&
           PR_StringToNetAddr(host.BeginReading(), &addr) != PR_SUCCESS;
}

static bool
TestPref(nsIURI *uri, const char *pref)
{
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return false;

    nsCAutoString scheme, host;
    PRInt32 port;

    if (NS_FAILED(uri->GetScheme(scheme)))
        return false;
    if (NS_FAILED(uri->GetAsciiHost(host)))
        return false;
    if (NS_FAILED(uri->GetPort(&port)))
        return false;

    char *hostList;
    if (NS_FAILED(prefs->GetCharPref(pref, &hostList)) || !hostList)
        return false;

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
            return true;
        if (*end == '\0')
            break;
        start = end + 1;
    }
    
    nsMemory::Free(hostList);
    return false;
}

// Check to see if we should use our generic (internal) NTLM auth module.
static bool
ForceGenericNTLM()
{
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return false;
    bool flag = false;

    if (NS_FAILED(prefs->GetBoolPref(kForceGeneric, &flag)))
        flag = false;

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
        return false;

    if (isProxyAuth) {
        bool val;
        if (NS_FAILED(prefs->GetBoolPref(kAllowProxies, &val)))
            val = false;
        LOG(("Default credentials allowed for proxy: %d\n", val));
        return val;
    }

    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));

    bool allowNonFqdn;
    if (NS_FAILED(prefs->GetBoolPref(kAllowNonFqdn, &allowNonFqdn)))
        allowNonFqdn = false;
    if (allowNonFqdn && uri && IsNonFqdn(uri)) {
        LOG(("Host is non-fqdn, default credentials are allowed\n"));
        return true;
    }

    bool isTrustedHost = (uri && TestPref(uri, kTrustedURIs));
    LOG(("Default credentials allowed for host: %d\n", isTrustedHost));
    return isTrustedHost;
}

// Dummy class for session state object.  This class doesn't hold any data.
// Instead we use its existence as a flag.  See ChallengeReceived.
class nsNTLMSessionState MOZ_FINAL : public nsISupports
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

    // Use the native NTLM if available
    mUseNative = true;

    // NOTE: we don't define any session state, but we do use the pointer.

    *identityInvalid = false;

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
                *identityInvalid = true;
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
	    
            mUseNative = false;

            // Prompt user for domain, username, and password.
            *identityInvalid = true;
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

    *creds = nullptr;
    *aFlags = 0;

    // if user or password is empty, ChallengeReceived returned
    // identityInvalid = false, that means we are using default user
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
        nsCAutoString serviceName, host;
        rv = authChannel->GetAsciiHostForAuth(host);
        if (NS_FAILED(rv))
            return rv;
        serviceName.AppendLiteral("HTTP@");
        serviceName.Append(host);
        // initialize auth module
        rv = module->Init(serviceName.get(), nsIAuthModule::REQ_DEFAULT, domain, user, pass);
        if (NS_FAILED(rv))
            return rv;

// This update enables updated Windows machines (Win7 or patched previous
// versions) and Linux machines running Samba (updated for Channel 
// Binding), to perform Channel Binding when authenticating using NTLMv2 
// and an outer secure channel.
// 
// Currently only implemented for Windows, linux support will be landing in 
// a separate patch, update this #ifdef accordingly then.
#if defined (XP_WIN) /* || defined (LINUX) */
        // We should retrieve the server certificate and compute the CBT, 
        // but only when we are using the native NTLM implementation and 
        // not the internal one.
        // It is a valid case not having the security info object.  This
        // occures when we connect an https site through an ntlm proxy.
        // After the ssl tunnel has been created, we get here the second
        // time and now generate the CBT from now valid security info.
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(authChannel, &rv);
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsISupports> security;
        rv = channel->GetSecurityInfo(getter_AddRefs(security));
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsISSLStatusProvider> statusProvider =
            do_QueryInterface(security);

        if (mUseNative && statusProvider) {
            nsCOMPtr<nsISSLStatus> status;
            rv = statusProvider->GetSSLStatus(getter_AddRefs(status));
            if (NS_FAILED(rv))
                return rv;

            nsCOMPtr<nsIX509Cert> cert;
            rv = status->GetServerCert(getter_AddRefs(cert));
            if (NS_FAILED(rv))
                return rv;

            PRUint32 length;
            PRUint8* certArray;
            cert->GetRawDER(&length, &certArray);						  
			
            // If there is a server certificate, we pass it along the
            // first time we call GetNextToken().
            inBufLen = length;
            inBuf = certArray;
        } else { 
            // If there is no server certificate, we don't pass anything.
            inBufLen = 0;
            inBuf = nullptr;
        }
#else // Extended protection update is just for Linux and Windows machines.
        inBufLen = 0;
        inBuf = nullptr;
#endif
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

        if (PL_Base64Decode(challenge, len, (char *) inBuf) == nullptr) {
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
