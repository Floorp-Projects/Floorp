/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISystemProxySettings.h"
#include "mozilla/Components.h"
#include "nsIURI.h"
#include "nsReadableUtils.h"
#include "nsArrayUtils.h"
#include "prnetdb.h"
#include "prenv.h"
#include "nsPrintfCString.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsISupportsPrimitives.h"
#include "nsIGSettingsService.h"
#include "nsInterfaceHashtable.h"
#include "mozilla/Attributes.h"
#include "nsIURI.h"

using namespace mozilla;

class nsUnixSystemProxySettings final : public nsISystemProxySettings {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISYSTEMPROXYSETTINGS

  nsUnixSystemProxySettings() : mSchemeProxySettings(4) {}
  void Init();

 private:
  ~nsUnixSystemProxySettings() = default;

  nsCOMPtr<nsIGSettingsService> mGSettings;
  nsCOMPtr<nsIGSettingsCollection> mProxySettings;
  nsInterfaceHashtable<nsCStringHashKey, nsIGSettingsCollection>
      mSchemeProxySettings;
  nsresult GetProxyFromGSettings(const nsACString& aScheme,
                                 const nsACString& aHost, int32_t aPort,
                                 nsACString& aResult);
  nsresult SetProxyResultFromGSettings(const char* aKeyBase, const char* aType,
                                       nsACString& aResult);
};

NS_IMPL_ISUPPORTS(nsUnixSystemProxySettings, nsISystemProxySettings)

NS_IMETHODIMP
nsUnixSystemProxySettings::GetMainThreadOnly(bool* aMainThreadOnly) {
  // dbus prevents us from being threadsafe, but this routine should not block
  // anyhow
  *aMainThreadOnly = true;
  return NS_OK;
}

void nsUnixSystemProxySettings::Init() {
  mGSettings = do_GetService(NS_GSETTINGSSERVICE_CONTRACTID);
  if (mGSettings) {
    mGSettings->GetCollectionForSchema("org.gnome.system.proxy"_ns,
                                       getter_AddRefs(mProxySettings));
  }
}

nsresult nsUnixSystemProxySettings::GetPACURI(nsACString& aResult) {
  if (mProxySettings) {
    nsCString proxyMode;
    // Check if mode is auto
    nsresult rv = mProxySettings->GetString("mode"_ns, proxyMode);
    if (rv == NS_OK && proxyMode.EqualsLiteral("auto")) {
      return mProxySettings->GetString("autoconfig-url"_ns, aResult);
    }
  }

  // Return an empty string when auto mode is not set.
  aResult.Truncate();
  return NS_OK;
}

static bool IsInNoProxyList(const nsACString& aHost, int32_t aPort,
                            const char* noProxyVal) {
  NS_ASSERTION(aPort >= 0, "Negative port?");

  nsAutoCString noProxy(noProxyVal);
  if (noProxy.EqualsLiteral("*")) return true;

  noProxy.StripWhitespace();

  nsReadingIterator<char> pos;
  nsReadingIterator<char> end;
  noProxy.BeginReading(pos);
  noProxy.EndReading(end);
  while (pos != end) {
    nsReadingIterator<char> last = pos;
    nsReadingIterator<char> nextPos;
    if (FindCharInReadable(',', last, end)) {
      nextPos = last;
      ++nextPos;
    } else {
      last = end;
      nextPos = end;
    }

    nsReadingIterator<char> colon = pos;
    int32_t port = -1;
    if (FindCharInReadable(':', colon, last)) {
      ++colon;
      nsDependentCSubstring portStr(colon, last);
      nsAutoCString portStr2(
          portStr);  // We need this for ToInteger. String API's suck.
      nsresult err;
      port = portStr2.ToInteger(&err);
      if (NS_FAILED(err)) {
        port = -2;  // don't match any port, so we ignore this pattern
      }
      --colon;
    } else {
      colon = last;
    }

    if (port == -1 || port == aPort) {
      nsDependentCSubstring hostStr(pos, colon);
      // By using StringEndsWith instead of an equality comparator, we can
      // include sub-domains
      if (StringEndsWith(aHost, hostStr, nsCaseInsensitiveCStringComparator))
        return true;
    }

    pos = nextPos;
  }

  return false;
}

static void SetProxyResult(const char* aType, const nsACString& aHost,
                           int32_t aPort, nsACString& aResult) {
  aResult.AssignASCII(aType);
  aResult.Append(' ');
  aResult.Append(aHost);
  if (aPort > 0) {
    aResult.Append(':');
    aResult.AppendInt(aPort);
  }
}

static void SetProxyResultDirect(nsACString& aResult) {
  aResult.AssignLiteral("DIRECT");
}

static nsresult GetProxyFromEnvironment(const nsACString& aScheme,
                                        const nsACString& aHost, int32_t aPort,
                                        nsACString& aResult) {
  nsAutoCString envVar;
  envVar.Append(aScheme);
  envVar.AppendLiteral("_proxy");
  const char* proxyVal = PR_GetEnv(envVar.get());
  if (!proxyVal) {
    proxyVal = PR_GetEnv("all_proxy");
    if (!proxyVal) {
      // Return failure so that the caller can detect the failure and
      // fall back to other proxy detection (e.g., WPAD)
      return NS_ERROR_FAILURE;
    }
  }

  const char* noProxyVal = PR_GetEnv("no_proxy");
  if (noProxyVal && IsInNoProxyList(aHost, aPort, noProxyVal)) {
    SetProxyResultDirect(aResult);
    return NS_OK;
  }

  // Use our URI parser to crack the proxy URI
  nsCOMPtr<nsIURI> proxyURI;
  nsresult rv = NS_NewURI(getter_AddRefs(proxyURI), proxyVal);
  NS_ENSURE_SUCCESS(rv, rv);

  // Is there a way to specify "socks://" or something in these environment
  // variables? I can't find any documentation.
  if (!proxyURI->SchemeIs("http")) {
    return NS_ERROR_UNKNOWN_PROTOCOL;
  }

  nsAutoCString proxyHost;
  rv = proxyURI->GetHost(proxyHost);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t proxyPort;
  rv = proxyURI->GetPort(&proxyPort);
  NS_ENSURE_SUCCESS(rv, rv);

  SetProxyResult("PROXY", proxyHost, proxyPort, aResult);
  return NS_OK;
}

nsresult nsUnixSystemProxySettings::SetProxyResultFromGSettings(
    const char* aKeyBase, const char* aType, nsACString& aResult) {
  nsDependentCString key(aKeyBase);

  nsCOMPtr<nsIGSettingsCollection> proxy_settings =
      mSchemeProxySettings.Get(key);
  nsresult rv;
  if (!proxy_settings) {
    rv =
        mGSettings->GetCollectionForSchema(key, getter_AddRefs(proxy_settings));
    NS_ENSURE_SUCCESS(rv, rv);

    mSchemeProxySettings.Put(key, proxy_settings);
  }

  nsAutoCString host;
  rv = proxy_settings->GetString("host"_ns, host);
  NS_ENSURE_SUCCESS(rv, rv);
  if (host.IsEmpty()) return NS_ERROR_FAILURE;

  int32_t port;
  rv = proxy_settings->GetInt("port"_ns, &port);
  NS_ENSURE_SUCCESS(rv, rv);

  /* When port is 0, proxy is not considered as enabled even if host is set. */
  if (port == 0) return NS_ERROR_FAILURE;

  SetProxyResult(aType, host, port, aResult);
  return NS_OK;
}

/* copied from nsProtocolProxyService.cpp --- we should share this! */
static void proxy_MaskIPv6Addr(PRIPv6Addr& addr, uint16_t mask_len) {
  if (mask_len == 128) return;

  if (mask_len > 96) {
    addr.pr_s6_addr32[3] =
        PR_htonl(PR_ntohl(addr.pr_s6_addr32[3]) & (~0L << (128 - mask_len)));
  } else if (mask_len > 64) {
    addr.pr_s6_addr32[3] = 0;
    addr.pr_s6_addr32[2] =
        PR_htonl(PR_ntohl(addr.pr_s6_addr32[2]) & (~0L << (96 - mask_len)));
  } else if (mask_len > 32) {
    addr.pr_s6_addr32[3] = 0;
    addr.pr_s6_addr32[2] = 0;
    addr.pr_s6_addr32[1] =
        PR_htonl(PR_ntohl(addr.pr_s6_addr32[1]) & (~0L << (64 - mask_len)));
  } else {
    addr.pr_s6_addr32[3] = 0;
    addr.pr_s6_addr32[2] = 0;
    addr.pr_s6_addr32[1] = 0;
    addr.pr_s6_addr32[0] =
        PR_htonl(PR_ntohl(addr.pr_s6_addr32[0]) & (~0L << (32 - mask_len)));
  }
}

static bool ConvertToIPV6Addr(const nsACString& aName, PRIPv6Addr* aAddr,
                              int32_t* aMask) {
  PRNetAddr addr;
  // try to convert hostname to IP
  if (PR_StringToNetAddr(PromiseFlatCString(aName).get(), &addr) != PR_SUCCESS)
    return false;

  // convert parsed address to IPv6
  if (addr.raw.family == PR_AF_INET) {
    // convert to IPv4-mapped address
    PR_ConvertIPv4AddrToIPv6(addr.inet.ip, aAddr);
    if (aMask) {
      if (*aMask <= 32)
        *aMask += 96;
      else
        return false;
    }
  } else if (addr.raw.family == PR_AF_INET6) {
    // copy the address
    memcpy(aAddr, &addr.ipv6.ip, sizeof(PRIPv6Addr));
  } else {
    return false;
  }

  return true;
}

static bool HostIgnoredByProxy(const nsACString& aIgnore,
                               const nsACString& aHost) {
  if (aIgnore.Equals(aHost, nsCaseInsensitiveCStringComparator)) return true;

  if (aIgnore.First() == '*' &&
      StringEndsWith(aHost, nsDependentCSubstring(aIgnore, 1),
                     nsCaseInsensitiveCStringComparator))
    return true;

  int32_t mask = 128;
  nsReadingIterator<char> start;
  nsReadingIterator<char> slash;
  nsReadingIterator<char> end;
  aIgnore.BeginReading(start);
  aIgnore.BeginReading(slash);
  aIgnore.EndReading(end);
  if (FindCharInReadable('/', slash, end)) {
    ++slash;
    nsDependentCSubstring maskStr(slash, end);
    nsAutoCString maskStr2(maskStr);
    nsresult err;
    mask = maskStr2.ToInteger(&err);
    if (NS_FAILED(err)) {
      mask = 128;
    }
    --slash;
  } else {
    slash = end;
  }

  nsDependentCSubstring ignoreStripped(start, slash);
  PRIPv6Addr ignoreAddr, hostAddr;
  if (!ConvertToIPV6Addr(ignoreStripped, &ignoreAddr, &mask) ||
      !ConvertToIPV6Addr(aHost, &hostAddr, nullptr))
    return false;

  proxy_MaskIPv6Addr(ignoreAddr, mask);
  proxy_MaskIPv6Addr(hostAddr, mask);

  return memcmp(&ignoreAddr, &hostAddr, sizeof(PRIPv6Addr)) == 0;
}

nsresult nsUnixSystemProxySettings::GetProxyFromGSettings(
    const nsACString& aScheme, const nsACString& aHost, int32_t aPort,
    nsACString& aResult) {
  nsCString proxyMode;
  nsresult rv = mProxySettings->GetString("mode"_ns, proxyMode);
  NS_ENSURE_SUCCESS(rv, rv);

  // return NS_ERROR_FAILURE when no proxy is set
  if (!proxyMode.EqualsLiteral("manual")) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIArray> ignoreList;
  if (NS_SUCCEEDED(mProxySettings->GetStringList("ignore-hosts"_ns,
                                                 getter_AddRefs(ignoreList))) &&
      ignoreList) {
    uint32_t len = 0;
    ignoreList->GetLength(&len);
    for (uint32_t i = 0; i < len; ++i) {
      nsCOMPtr<nsISupportsCString> str = do_QueryElementAt(ignoreList, i);
      if (str) {
        nsCString s;
        if (NS_SUCCEEDED(str->GetData(s)) && !s.IsEmpty()) {
          if (HostIgnoredByProxy(s, aHost)) {
            SetProxyResultDirect(aResult);
            return NS_OK;
          }
        }
      }
    }
  }

  if (aScheme.LowerCaseEqualsLiteral("http")) {
    rv = SetProxyResultFromGSettings("org.gnome.system.proxy.http", "PROXY",
                                     aResult);
  } else if (aScheme.LowerCaseEqualsLiteral("https")) {
    rv = SetProxyResultFromGSettings("org.gnome.system.proxy.https", "PROXY",
                                     aResult);
    /* Try to use HTTP proxy when HTTPS proxy is not explicitly defined */
    if (rv != NS_OK)
      rv = SetProxyResultFromGSettings("org.gnome.system.proxy.http", "PROXY",
                                       aResult);
  } else if (aScheme.LowerCaseEqualsLiteral("ftp")) {
    rv = SetProxyResultFromGSettings("org.gnome.system.proxy.ftp", "PROXY",
                                     aResult);
  } else {
    rv = NS_ERROR_FAILURE;
  }
  if (rv != NS_OK) {
    /* If proxy for scheme is not specified, use SOCKS proxy for all schemes */
    rv = SetProxyResultFromGSettings("org.gnome.system.proxy.socks", "SOCKS",
                                     aResult);
  }

  if (NS_FAILED(rv)) {
    SetProxyResultDirect(aResult);
  }

  return NS_OK;
}

nsresult nsUnixSystemProxySettings::GetProxyForURI(const nsACString& aSpec,
                                                   const nsACString& aScheme,
                                                   const nsACString& aHost,
                                                   const int32_t aPort,
                                                   nsACString& aResult) {
  if (mProxySettings) {
    nsresult rv = GetProxyFromGSettings(aScheme, aHost, aPort, aResult);
    if (NS_SUCCEEDED(rv)) return rv;
  }

  return GetProxyFromEnvironment(aScheme, aHost, aPort, aResult);
}

NS_IMPL_COMPONENT_FACTORY(nsUnixSystemProxySettings) {
  auto result = MakeRefPtr<nsUnixSystemProxySettings>();
  result->Init();
  return result.forget().downcast<nsISupports>();
}
