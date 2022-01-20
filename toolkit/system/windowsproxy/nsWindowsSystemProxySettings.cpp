/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <ras.h>
#include <wininet.h>

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "nsISystemProxySettings.h"
#include "mozilla/Components.h"
#include "mozilla/ProfilerLabels.h"
#include "nsPrintfCString.h"
#include "nsNetCID.h"
#include "nsThreadUtils.h"
#include "prnetdb.h"
#include "ProxyUtils.h"

class nsWindowsSystemProxySettings final : public nsISystemProxySettings {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISYSTEMPROXYSETTINGS

  nsWindowsSystemProxySettings(){};

 private:
  ~nsWindowsSystemProxySettings(){};

  bool MatchOverride(const nsACString& aHost);
  bool PatternMatch(const nsACString& aHost, const nsACString& aOverride);
};

NS_IMPL_ISUPPORTS(nsWindowsSystemProxySettings, nsISystemProxySettings)

NS_IMETHODIMP
nsWindowsSystemProxySettings::GetMainThreadOnly(bool* aMainThreadOnly) {
  // bug 1366133: if you change this to main thread only, please handle
  // nsProtocolProxyService::Resolve_Internal carefully to avoid hang on main
  // thread.
  *aMainThreadOnly = false;
  return NS_OK;
}

static void SetProxyResult(const char* aType, const nsACString& aHostPort,
                           nsACString& aResult) {
  aResult.AssignASCII(aType);
  aResult.Append(' ');
  aResult.Append(aHostPort);
}

static void SetProxyResultDirect(nsACString& aResult) {
  // For whatever reason, a proxy is not to be used.
  aResult.AssignLiteral("DIRECT");
}

static nsresult ReadInternetOption(uint32_t aOption, uint32_t& aFlags,
                                   nsAString& aValue) {
  // Bug 1366133: InternetGetConnectedStateExW() may cause hangs
  MOZ_ASSERT(!NS_IsMainThread());

  DWORD connFlags = 0;
  WCHAR connName[RAS_MaxEntryName + 1];
  MOZ_SEH_TRY {
    InternetGetConnectedStateExW(&connFlags, connName,
                                 mozilla::ArrayLength(connName), 0);
  }
  MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) { return NS_ERROR_FAILURE; }

  INTERNET_PER_CONN_OPTIONW options[2];
  options[0].dwOption = INTERNET_PER_CONN_FLAGS_UI;
  options[1].dwOption = aOption;

  INTERNET_PER_CONN_OPTION_LISTW list;
  list.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LISTW);
  list.pszConnection =
      connFlags & INTERNET_CONNECTION_MODEM ? connName : nullptr;
  list.dwOptionCount = mozilla::ArrayLength(options);
  list.dwOptionError = 0;
  list.pOptions = options;

  unsigned long size = sizeof(INTERNET_PER_CONN_OPTION_LISTW);
  if (!InternetQueryOptionW(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION,
                            &list, &size)) {
    return NS_ERROR_FAILURE;
  }

  aFlags = options[0].Value.dwValue;
  aValue.Assign(options[1].Value.pszValue);
  GlobalFree(options[1].Value.pszValue);

  return NS_OK;
}

bool nsWindowsSystemProxySettings::MatchOverride(const nsACString& aHost) {
  nsresult rv;
  uint32_t flags = 0;
  nsAutoString buf;

  rv = ReadInternetOption(INTERNET_PER_CONN_PROXY_BYPASS, flags, buf);
  if (NS_FAILED(rv)) return false;

  NS_ConvertUTF16toUTF8 cbuf(buf);

  nsAutoCString host(aHost);
  int32_t start = 0;
  int32_t end = cbuf.Length();

  // Windows formats its proxy override list in the form:
  // server;server;server where 'server' is a server name pattern or IP
  // address, or "<local>". "<local>" must be translated to
  // "localhost;127.0.0.1".
  // In a server name pattern, a '*' character matches any substring and
  // all other characters must match themselves; the whole pattern must match
  // the whole hostname.
  while (true) {
    int32_t delimiter = cbuf.FindCharInSet(" ;", start);
    if (delimiter == -1) delimiter = end;

    if (delimiter != start) {
      const nsAutoCString override(Substring(cbuf, start, delimiter - start));
      if (override.EqualsLiteral("<local>")) {
        PRNetAddr addr;
        bool isIpAddr = (PR_StringToNetAddr(host.get(), &addr) == PR_SUCCESS);

        // Don't use proxy for local hosts (plain hostname, no dots)
        if (!isIpAddr && !host.Contains('.')) {
          return true;
        }

        if (host.EqualsLiteral("127.0.0.1") || host.EqualsLiteral("::1")) {
          return true;
        }
      } else if (PatternMatch(host, override)) {
        return true;
      }
    }

    if (delimiter == end) break;
    start = ++delimiter;
  }

  return false;
}

bool nsWindowsSystemProxySettings::PatternMatch(const nsACString& aHost,
                                                const nsACString& aOverride) {
  return mozilla::toolkit::system::IsHostProxyEntry(aHost, aOverride);
}

nsresult nsWindowsSystemProxySettings::GetPACURI(nsACString& aResult) {
  AUTO_PROFILER_LABEL("nsWindowsSystemProxySettings::GetPACURI", OTHER);
  nsresult rv;
  uint32_t flags = 0;
  nsAutoString buf;

  rv = ReadInternetOption(INTERNET_PER_CONN_AUTOCONFIG_URL, flags, buf);
  if (!(flags & PROXY_TYPE_AUTO_PROXY_URL)) {
    aResult.Truncate();
    return rv;
  }

  if (NS_SUCCEEDED(rv)) aResult = NS_ConvertUTF16toUTF8(buf);
  return rv;
}

nsresult nsWindowsSystemProxySettings::GetProxyForURI(const nsACString& aSpec,
                                                      const nsACString& aScheme,
                                                      const nsACString& aHost,
                                                      const int32_t aPort,
                                                      nsACString& aResult) {
  nsresult rv;
  uint32_t flags = 0;
  nsAutoString buf;

  rv = ReadInternetOption(INTERNET_PER_CONN_PROXY_SERVER, flags, buf);
  if (NS_FAILED(rv) || !(flags & PROXY_TYPE_PROXY)) {
    SetProxyResultDirect(aResult);
    return NS_OK;
  }

  if (MatchOverride(aHost)) {
    SetProxyResultDirect(aResult);
    return NS_OK;
  }

  NS_ConvertUTF16toUTF8 cbuf(buf);

  constexpr auto kSocksPrefix = "socks="_ns;
  nsAutoCString prefix;
  ToLowerCase(aScheme, prefix);

  prefix.Append('=');

  nsAutoCString specificProxy;
  nsAutoCString defaultProxy;
  nsAutoCString socksProxy;
  int32_t start = 0;
  int32_t end = cbuf.Length();

  while (true) {
    int32_t delimiter = cbuf.FindCharInSet(" ;", start);
    if (delimiter == -1) delimiter = end;

    if (delimiter != start) {
      const nsAutoCString proxy(Substring(cbuf, start, delimiter - start));
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
      } else if (proxy.Find(kSocksPrefix) == 0) {
        // SOCKS proxy.
        socksProxy =
            Substring(proxy, kSocksPrefix.Length());  // "socks=" length.
      }
    }

    if (delimiter == end) break;
    start = ++delimiter;
  }

  if (!specificProxy.IsEmpty())
    SetProxyResult("PROXY", specificProxy,
                   aResult);  // Protocol-specific proxy.
  else if (!defaultProxy.IsEmpty())
    SetProxyResult("PROXY", defaultProxy, aResult);  // Default proxy.
  else if (!socksProxy.IsEmpty())
    SetProxyResult("SOCKS", socksProxy, aResult);  // SOCKS proxy.
  else
    SetProxyResultDirect(aResult);  // Direct connection.

  return NS_OK;
}

NS_IMPL_COMPONENT_FACTORY(nsWindowsSystemProxySettings) {
  return mozilla::MakeAndAddRef<nsWindowsSystemProxySettings>()
      .downcast<nsISupports>();
}
