/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mitchell Field
 *
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Mitchell Field <mitch_1_2@live.com.au>
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

#include <windows.h>
#include "nsIWindowsRegKey.h"

#include "nsISystemProxySettings.h"
#include "nsIServiceManager.h"
#include "mozilla/ModuleUtils.h"
#include "nsPrintfCString.h"
#include "nsNetUtil.h"
#include "nsISupportsPrimitives.h"
#include "nsIURI.h"

class nsWindowsSystemProxySettings : public nsISystemProxySettings
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISYSTEMPROXYSETTINGS

    nsWindowsSystemProxySettings() {};
    nsresult Init();

private:
    ~nsWindowsSystemProxySettings() {};

    nsCOMPtr<nsIWindowsRegKey> mKey;
    bool MatchOverride(const nsACString& aHost);
    bool PatternMatch(const nsACString& aHost, const nsACString& aOverride);
};

NS_IMPL_ISUPPORTS1(nsWindowsSystemProxySettings, nsISystemProxySettings)

nsresult
nsWindowsSystemProxySettings::Init()
{
    nsresult rv;
    mKey = do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_NAMED_LITERAL_STRING(key,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
    rv = mKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER, key,
                    nsIWindowsRegKey::ACCESS_READ);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
}

static void SetProxyResult(const char* aType, const nsACString& aHost,
                           PRInt32 aPort, nsACString& aResult)
{
    aResult.AssignASCII(aType);
    aResult.Append(' ');
    aResult.Append(aHost);
    aResult.Append(':');
    aResult.Append(nsPrintfCString("%d", aPort));
}

static void SetProxyResult(const char* aType, const nsACString& aHostPort,
                           nsACString& aResult)
{
    nsCOMPtr<nsIURI> uri;
    nsCAutoString host;
    PRInt32 port;

    // Try parsing it as a URI.
    if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(uri), aHostPort)) &&
        NS_SUCCEEDED(uri->GetHost(host)) && !host.IsEmpty() &&
        NS_SUCCEEDED(uri->GetPort(&port))) {
        SetProxyResult(aType, host, port, aResult);
    } else {
        aResult.AssignASCII(aType);
        aResult.Append(' ');
        aResult.Append(aHostPort);
    }
}

static void SetProxyResultDirect(nsACString& aResult)
{
    // For whatever reason, a proxy is not to be used.
    aResult.AssignASCII("DIRECT");
}

bool
nsWindowsSystemProxySettings::MatchOverride(const nsACString& aHost)
{
    nsresult rv;
    nsAutoString buf;

    rv = mKey->ReadStringValue(NS_LITERAL_STRING("ProxyOverride"), buf);
    if (NS_FAILED(rv))
        return PR_FALSE;

    NS_ConvertUTF16toUTF8 cbuf(buf);

    nsCAutoString host(aHost);
    PRInt32 start = 0;
    PRInt32 end = cbuf.Length();

    // Windows formats its proxy override list in the form:
    // server;server;server where 'server' is a server name pattern or IP
    // address, or "<local>". "<local>" must be translated to
    // "localhost;127.0.0.1".
    // In a server name pattern, a '*' character matches any substring and
    // all other characters must match themselves; the whole pattern must match
    // the whole hostname.
    while (true) {
        PRInt32 delimiter = cbuf.FindCharInSet(" ;", start);
        if (delimiter == -1)
            delimiter = end;

        if (delimiter != start) {
            const nsCAutoString override(Substring(cbuf, start,
                                                   delimiter - start));
            if (override.EqualsLiteral("<local>")) {
                // This override matches local addresses.
                if (host.EqualsLiteral("localhost") ||
                    host.EqualsLiteral("127.0.0.1"))
                    return PR_TRUE;
            } else if (PatternMatch(host, override)) {
                return PR_TRUE;
            }
        }

        if (delimiter == end)
            break;
        start = ++delimiter;
    }

    return PR_FALSE;
}

bool
nsWindowsSystemProxySettings::PatternMatch(const nsACString& aHost,
                                           const nsACString& aOverride)
{
    nsCAutoString host(aHost);
    nsCAutoString override(aOverride);
    PRInt32 overrideLength = override.Length();
    PRInt32 tokenStart = 0;
    PRInt32 offset = 0;
    bool star = false;

    while (tokenStart < overrideLength) {
        PRInt32 tokenEnd = override.FindChar('*', tokenStart);
        if (tokenEnd == tokenStart) {
            star = PR_TRUE;
            tokenStart++;
            // If the character following the '*' is a '.' character then skip
            // it so that "*.foo.com" allows "foo.com".
            if (override.FindChar('.', tokenStart) == tokenStart)
                tokenStart++;
        } else {
            if (tokenEnd == -1)
                tokenEnd = overrideLength;
            nsCAutoString token(Substring(override, tokenStart,
                                          tokenEnd - tokenStart));
            offset = host.Find(token, offset);
            if (offset == -1 || (!star && offset))
                return PR_FALSE;
            star = PR_FALSE;
            tokenStart = tokenEnd;
            offset += token.Length();
        }
    }

    return (star || (offset == host.Length()));
}

nsresult
nsWindowsSystemProxySettings::GetPACURI(nsACString& aResult)
{
    NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);
    nsresult rv;
    nsAutoString buf;
    rv = mKey->ReadStringValue(NS_LITERAL_STRING("AutoConfigURL"), buf);
    if (NS_SUCCEEDED(rv))
        aResult = NS_ConvertUTF16toUTF8(buf);
    return rv;
}

nsresult
nsWindowsSystemProxySettings::GetProxyForURI(nsIURI* aURI, nsACString& aResult)
{
    NS_ENSURE_TRUE(mKey, NS_ERROR_NOT_INITIALIZED);
    nsresult rv;
    PRUint32 enabled = 0;

    rv = mKey->ReadIntValue(NS_LITERAL_STRING("ProxyEnable"), &enabled);
    if (!enabled) {
        SetProxyResultDirect(aResult);
        return NS_OK;
    }

    nsCAutoString scheme;
    rv = aURI->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString host;
    rv = aURI->GetHost(host);
    NS_ENSURE_SUCCESS(rv, rv);

    if (MatchOverride(host)) {
        SetProxyResultDirect(aResult);
        return NS_OK;
    }

    nsAutoString buf;

    rv = mKey->ReadStringValue(NS_LITERAL_STRING("ProxyServer"), buf);
    if (NS_FAILED(rv)) {
        SetProxyResultDirect(aResult);
        return NS_OK;
    }

    NS_ConvertUTF16toUTF8 cbuf(buf);

    nsCAutoString prefix;
    ToLowerCase(scheme, prefix);

    prefix.Append('=');

    nsCAutoString specificProxy;
    nsCAutoString defaultProxy;
    nsCAutoString socksProxy;
    PRInt32 start = 0;
    PRInt32 end = cbuf.Length();

    while (true) {
        PRInt32 delimiter = cbuf.FindCharInSet(" ;", start);
        if (delimiter == -1)
            delimiter = end;

        if (delimiter != start) {
            const nsCAutoString proxy(Substring(cbuf, start,
                                                delimiter - start));
            if (proxy.FindChar('=') == -1) {
                // If a proxy name is listed by itself, it is used as the
                // default proxy for any protocols that do not have a specific
                // proxy specified.
                // (http://msdn.microsoft.com/en-us/library/aa383996%28VS.85%29.aspx)
                defaultProxy = proxy;
            } else if (proxy.Find(prefix) == 0) {
                // To list a proxy for a specific protocol, the string must
                // follow the format "<protocol>=<protocol>://<proxy_name>".
                // (http://msdn.microsoft.com/en-us/library/aa383996%28VS.85%29.aspx)
                specificProxy = Substring(proxy, prefix.Length());
                break;
            } else if (proxy.Find("socks=") == 0) {
                // SOCKS proxy.
                socksProxy = Substring(proxy, 5); // "socks=" length.
            }
        }

        if (delimiter == end)
            break;
        start = ++delimiter;
    }

    if (!specificProxy.IsEmpty())
        SetProxyResult("PROXY", specificProxy, aResult); // Protocol-specific proxy.
    else if (!defaultProxy.IsEmpty())
        SetProxyResult("PROXY", defaultProxy, aResult); // Default proxy.
    else if (!socksProxy.IsEmpty())
        SetProxyResult("SOCKS", socksProxy, aResult); // SOCKS proxy.
    else
        SetProxyResultDirect(aResult); // Direct connection.

    return NS_OK;
}

#define NS_WINDOWSSYSTEMPROXYSERVICE_CID  /* 4e22d3ea-aaa2-436e-ada4-9247de57d367 */\
    { 0x4e22d3ea, 0xaaa2, 0x436e, \
        {0xad, 0xa4, 0x92, 0x47, 0xde, 0x57, 0xd3, 0x67 } }

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsWindowsSystemProxySettings, Init)
NS_DEFINE_NAMED_CID(NS_WINDOWSSYSTEMPROXYSERVICE_CID);

static const mozilla::Module::CIDEntry kSysProxyCIDs[] = {
    { &kNS_WINDOWSSYSTEMPROXYSERVICE_CID, false, NULL, nsWindowsSystemProxySettingsConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kSysProxyContracts[] = {
    { NS_SYSTEMPROXYSETTINGS_CONTRACTID, &kNS_WINDOWSSYSTEMPROXYSERVICE_CID },
    { NULL }
};

static const mozilla::Module kSysProxyModule = {
    mozilla::Module::kVersion,
    kSysProxyCIDs,
    kSysProxyContracts
};

NSMODULE_DEFN(nsWindowsProxyModule) = &kSysProxyModule;
