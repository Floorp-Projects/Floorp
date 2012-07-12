/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <ras.h>
#include <wininet.h>

#include "mozilla/Util.h"
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

    bool MatchOverride(const nsACString& aHost);
    bool PatternMatch(const nsACString& aHost, const nsACString& aOverride);
};

NS_IMPL_ISUPPORTS1(nsWindowsSystemProxySettings, nsISystemProxySettings)

nsresult
nsWindowsSystemProxySettings::Init()
{
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

static nsresult ReadInternetOption(PRUint32 aOption, PRUint32& aFlags,
                                   nsAString& aValue)
{
    DWORD connFlags = 0;
    WCHAR connName[RAS_MaxEntryName + 1];
    InternetGetConnectedStateExW(&connFlags, connName,
                                 mozilla::ArrayLength(connName), 0);

    INTERNET_PER_CONN_OPTIONW options[2];
    options[0].dwOption = INTERNET_PER_CONN_FLAGS;
    options[1].dwOption = aOption;

    INTERNET_PER_CONN_OPTION_LISTW list;
    list.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LISTW);
    list.pszConnection = connFlags & INTERNET_CONNECTION_MODEM ?
                         connName : NULL;
    list.dwOptionCount = mozilla::ArrayLength(options);
    list.dwOptionError = 0;
    list.pOptions = options;

    unsigned long size = sizeof(INTERNET_PER_CONN_OPTION_LISTW);
    if (!InternetQueryOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
                              &list, &size)) {
        return NS_ERROR_FAILURE;
    }

    aFlags = options[0].Value.dwValue;
    aValue.Assign(options[1].Value.pszValue);
    GlobalFree(options[1].Value.pszValue);

    return NS_OK;
}

bool
nsWindowsSystemProxySettings::MatchOverride(const nsACString& aHost)
{
    nsresult rv;
    PRUint32 flags = 0;
    nsAutoString buf;

    rv = ReadInternetOption(INTERNET_PER_CONN_PROXY_BYPASS, flags, buf);
    if (NS_FAILED(rv))
        return false;

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
                    return true;
            } else if (PatternMatch(host, override)) {
                return true;
            }
        }

        if (delimiter == end)
            break;
        start = ++delimiter;
    }

    return false;
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
            star = true;
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
                return false;
            star = false;
            tokenStart = tokenEnd;
            offset += token.Length();
        }
    }

    return (star || (offset == host.Length()));
}

nsresult
nsWindowsSystemProxySettings::GetPACURI(nsACString& aResult)
{
    nsresult rv;
    PRUint32 flags = 0;
    nsAutoString buf;

    rv = ReadInternetOption(INTERNET_PER_CONN_AUTOCONFIG_URL, flags, buf);
    if (!(flags & PROXY_TYPE_AUTO_PROXY_URL)) {
        aResult.Truncate();
        return rv;
    }

    if (NS_SUCCEEDED(rv))
        aResult = NS_ConvertUTF16toUTF8(buf);
    return rv;
}

nsresult
nsWindowsSystemProxySettings::GetProxyForURI(nsIURI* aURI, nsACString& aResult)
{
    nsresult rv;
    PRUint32 flags = 0;
    nsAutoString buf;

    rv = ReadInternetOption(INTERNET_PER_CONN_PROXY_SERVER, flags, buf);
    if (NS_FAILED(rv) || !(flags & PROXY_TYPE_PROXY)) {
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
