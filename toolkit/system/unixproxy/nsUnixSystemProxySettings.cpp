/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISystemProxySettings.h"
#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsIGConfService.h"
#include "nsIURI.h"
#include "nsReadableUtils.h"
#include "nsArrayUtils.h"
#include "prnetdb.h"
#include "prenv.h"
#include "nsPrintfCString.h"
#include "nsNetUtil.h"
#include "nsISupportsPrimitives.h"
#include "nsIGSettingsService.h"
#include "nsInterfaceHashtable.h"

class nsUnixSystemProxySettings : public nsISystemProxySettings {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISYSTEMPROXYSETTINGS

  nsUnixSystemProxySettings() {}
  nsresult Init();

private:
  ~nsUnixSystemProxySettings() {}
  
  nsCOMPtr<nsIGConfService> mGConf;
  nsCOMPtr<nsIGSettingsService> mGSettings;
  nsCOMPtr<nsIGSettingsCollection> mProxySettings;
  nsInterfaceHashtable<nsCStringHashKey, nsIGSettingsCollection> mSchemeProxySettings;
  bool IsProxyMode(const char* aMode);
  nsresult SetProxyResultFromGConf(const char* aKeyBase, const char* aType, nsACString& aResult);
  nsresult GetProxyFromGConf(const nsACString& aScheme, const nsACString& aHost, PRInt32 aPort, nsACString& aResult);
  nsresult GetProxyFromGSettings(const nsACString& aScheme, const nsACString& aHost, PRInt32 aPort, nsACString& aResult);
  nsresult SetProxyResultFromGSettings(const char* aKeyBase, const char* aType, nsACString& aResult);
};

NS_IMPL_ISUPPORTS1(nsUnixSystemProxySettings, nsISystemProxySettings)

nsresult
nsUnixSystemProxySettings::Init()
{
  mSchemeProxySettings.Init(5);
  mGConf = do_GetService(NS_GCONFSERVICE_CONTRACTID);
  mGSettings = do_GetService(NS_GSETTINGSSERVICE_CONTRACTID);
  if (mGSettings) {
    mGSettings->GetCollectionForSchema(NS_LITERAL_CSTRING("org.gnome.system.proxy"),
                                       getter_AddRefs(mProxySettings));
  }

  return NS_OK;
}

bool
nsUnixSystemProxySettings::IsProxyMode(const char* aMode)
{
  nsCAutoString mode;
  return NS_SUCCEEDED(mGConf->GetString(NS_LITERAL_CSTRING("/system/proxy/mode"), mode)) &&
         mode.EqualsASCII(aMode);
}

nsresult
nsUnixSystemProxySettings::GetPACURI(nsACString& aResult)
{
  if (mProxySettings) {
    nsCString proxyMode;
    // Check if mode is auto
    nsresult rv = mProxySettings->GetString(NS_LITERAL_CSTRING("mode"), proxyMode);
    if (rv == NS_OK && proxyMode.Equals("auto")) {
      return mProxySettings->GetString(NS_LITERAL_CSTRING("autoconfig-url"), aResult);
    }
    /* The org.gnome.system.proxy schema has been found, but auto mode is not set.
     * Don't try the GConf and return empty string. */
    aResult.Truncate();
    return NS_OK;
  }

  if (mGConf && IsProxyMode("auto")) {
    return mGConf->GetString(NS_LITERAL_CSTRING("/system/proxy/autoconfig_url"),
                             aResult);
  }
  // Return an empty string when auto mode is not set.
  aResult.Truncate();
  return NS_OK;
}

static bool
IsInNoProxyList(const nsACString& aHost, PRInt32 aPort, const char* noProxyVal)
{
  NS_ASSERTION(aPort >= 0, "Negative port?");
  
  nsCAutoString noProxy(noProxyVal);
  if (noProxy.EqualsLiteral("*"))
    return true;
    
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
    PRInt32 port = -1;
    if (FindCharInReadable(':', colon, last)) {
      ++colon;
      nsDependentCSubstring portStr(colon, last);
      nsCAutoString portStr2(portStr); // We need this for ToInteger. String API's suck.
      PRInt32 err;
      port = portStr2.ToInteger(&err);
      if (NS_FAILED(err)) {
        port = -2; // don't match any port, so we ignore this pattern
      }
      --colon;
    } else {
      colon = last;
    }
    
    if (port == -1 || port == aPort) {
      nsDependentCSubstring hostStr(pos, colon);
      // By using StringEndsWith instead of an equality comparator, we can include sub-domains
      if (StringEndsWith(aHost, hostStr, nsCaseInsensitiveCStringComparator()))
        return true;
    }
    
    pos = nextPos;
  }
  
  return false;
}

static void SetProxyResult(const char* aType, const nsACString& aHost,
                           PRInt32 aPort, nsACString& aResult)
{
  aResult.AppendASCII(aType);
  aResult.Append(' ');
  aResult.Append(aHost);
  aResult.Append(':');
  aResult.Append(nsPrintfCString("%d", aPort));
}

static nsresult
GetProxyFromEnvironment(const nsACString& aScheme,
                        const nsACString& aHost,
                        PRInt32 aPort,
                        nsACString& aResult)
{
  nsCAutoString envVar;
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
    aResult.AppendLiteral("DIRECT");
    return NS_OK;
  }
  
  // Use our URI parser to crack the proxy URI
  nsCOMPtr<nsIURI> proxyURI;
  nsresult rv = NS_NewURI(getter_AddRefs(proxyURI), proxyVal);
  NS_ENSURE_SUCCESS(rv, rv);

  // Is there a way to specify "socks://" or something in these environment
  // variables? I can't find any documentation.
  bool isHTTP;
  rv = proxyURI->SchemeIs("http", &isHTTP);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isHTTP)
    return NS_ERROR_UNKNOWN_PROTOCOL;

  nsCAutoString proxyHost;
  rv = proxyURI->GetHost(proxyHost);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 proxyPort;
  rv = proxyURI->GetPort(&proxyPort);
  NS_ENSURE_SUCCESS(rv, rv);

  SetProxyResult("PROXY", proxyHost, proxyPort, aResult);
  return NS_OK;
}

nsresult
nsUnixSystemProxySettings::SetProxyResultFromGConf(const char* aKeyBase, const char* aType,
                                                   nsACString& aResult)
{
  nsCAutoString hostKey;
  hostKey.AppendASCII(aKeyBase);
  hostKey.AppendLiteral("host");
  nsCAutoString host;
  nsresult rv = mGConf->GetString(hostKey, host);
  NS_ENSURE_SUCCESS(rv, rv);
  if (host.IsEmpty())
    return NS_ERROR_FAILURE;
  
  nsCAutoString portKey;
  portKey.AppendASCII(aKeyBase);
  portKey.AppendLiteral("port");
  PRInt32 port;
  rv = mGConf->GetInt(portKey, &port);
  NS_ENSURE_SUCCESS(rv, rv);

  /* When port is 0, proxy is not considered as enabled even if host is set. */
  if (port == 0)
    return NS_ERROR_FAILURE;

  SetProxyResult(aType, host, port, aResult);
  return NS_OK;
}

nsresult
nsUnixSystemProxySettings::SetProxyResultFromGSettings(const char* aKeyBase, const char* aType,
                                                       nsACString& aResult)
{
  nsDependentCString key(aKeyBase);

  nsCOMPtr<nsIGSettingsCollection> proxy_settings = mSchemeProxySettings.Get(key);
  nsresult rv;
  if (!proxy_settings) {
    rv = mGSettings->GetCollectionForSchema(key, getter_AddRefs(proxy_settings));
    NS_ENSURE_SUCCESS(rv, rv);

    mSchemeProxySettings.Put(key, proxy_settings);
  }

  nsCAutoString host;
  rv = proxy_settings->GetString(NS_LITERAL_CSTRING("host"), host);
  NS_ENSURE_SUCCESS(rv, rv);
  if (host.IsEmpty())
    return NS_ERROR_FAILURE;
  
  PRInt32 port;
  rv = proxy_settings->GetInt(NS_LITERAL_CSTRING("port"), &port);
  NS_ENSURE_SUCCESS(rv, rv);
    
  /* When port is 0, proxy is not considered as enabled even if host is set. */
  if (port == 0)
    return NS_ERROR_FAILURE;

  SetProxyResult(aType, host, port, aResult);
  return NS_OK;
}

/* copied from nsProtocolProxyService.cpp --- we should share this! */
static void
proxy_MaskIPv6Addr(PRIPv6Addr &addr, PRUint16 mask_len)
{
  if (mask_len == 128)
    return;

  if (mask_len > 96) {
    addr.pr_s6_addr32[3] = PR_htonl(
            PR_ntohl(addr.pr_s6_addr32[3]) & (~0L << (128 - mask_len)));
  }
  else if (mask_len > 64) {
    addr.pr_s6_addr32[3] = 0;
    addr.pr_s6_addr32[2] = PR_htonl(
            PR_ntohl(addr.pr_s6_addr32[2]) & (~0L << (96 - mask_len)));
  }
  else if (mask_len > 32) {
    addr.pr_s6_addr32[3] = 0;
    addr.pr_s6_addr32[2] = 0;
    addr.pr_s6_addr32[1] = PR_htonl(
            PR_ntohl(addr.pr_s6_addr32[1]) & (~0L << (64 - mask_len)));
  }
  else {
    addr.pr_s6_addr32[3] = 0;
    addr.pr_s6_addr32[2] = 0;
    addr.pr_s6_addr32[1] = 0;
    addr.pr_s6_addr32[0] = PR_htonl(
            PR_ntohl(addr.pr_s6_addr32[0]) & (~0L << (32 - mask_len)));
  }
}

static bool ConvertToIPV6Addr(const nsACString& aName,
                                PRIPv6Addr* aAddr)
{
  PRNetAddr addr;
  // try to convert hostname to IP
  if (PR_StringToNetAddr(PromiseFlatCString(aName).get(), &addr) != PR_SUCCESS)
    return false;

  // convert parsed address to IPv6
  if (addr.raw.family == PR_AF_INET) {
    // convert to IPv4-mapped address
    PR_ConvertIPv4AddrToIPv6(addr.inet.ip, aAddr);
  } else if (addr.raw.family == PR_AF_INET6) {
    // copy the address
    memcpy(aAddr, &addr.ipv6.ip, sizeof(PRIPv6Addr));
  } else {
    return false;
  }
  
  return true;
}

static bool HostIgnoredByProxy(const nsACString& aIgnore,
                               const nsACString& aHost)
{
  if (aIgnore.Equals(aHost, nsCaseInsensitiveCStringComparator()))
    return true;

  if (aIgnore.First() == '*' &&
      StringEndsWith(aHost, nsDependentCSubstring(aIgnore, 1),
                     nsCaseInsensitiveCStringComparator()))
    return true;

  PRInt32 mask = 128;
  nsReadingIterator<char> start;
  nsReadingIterator<char> slash;
  nsReadingIterator<char> end;
  aIgnore.BeginReading(start);
  aIgnore.BeginReading(slash);
  aIgnore.EndReading(end);
  if (FindCharInReadable('/', slash, end)) {
    ++slash;
    nsDependentCSubstring maskStr(slash, end);
    nsCAutoString maskStr2(maskStr);
    PRInt32 err;
    mask = maskStr2.ToInteger(&err);
    if (err != 0) {
      mask = 128;
    }
    --slash;
  } else {
    slash = end;
  }

  nsDependentCSubstring ignoreStripped(start, slash);
  PRIPv6Addr ignoreAddr, hostAddr;
  if (!ConvertToIPV6Addr(ignoreStripped, &ignoreAddr) ||
      !ConvertToIPV6Addr(aHost, &hostAddr))
    return false;

  proxy_MaskIPv6Addr(ignoreAddr, mask);
  proxy_MaskIPv6Addr(hostAddr, mask);
  
  return memcmp(&ignoreAddr, &hostAddr, sizeof(PRIPv6Addr)) == 0;
}

nsresult
nsUnixSystemProxySettings::GetProxyFromGConf(const nsACString& aScheme,
                                             const nsACString& aHost,
                                             PRInt32 aPort,
                                             nsACString& aResult)
{
  bool masterProxySwitch = false;
  mGConf->GetBool(NS_LITERAL_CSTRING("/system/http_proxy/use_http_proxy"), &masterProxySwitch);
  if (!IsProxyMode("manual") || !masterProxySwitch) {
    aResult.AppendLiteral("DIRECT");
    return NS_OK;
  }
  
  nsCOMPtr<nsIArray> ignoreList;
  if (NS_SUCCEEDED(mGConf->GetStringList(NS_LITERAL_CSTRING("/system/http_proxy/ignore_hosts"),
                                         getter_AddRefs(ignoreList))) && ignoreList) {
    PRUint32 len = 0;
    ignoreList->GetLength(&len);
    for (PRUint32 i = 0; i < len; ++i) {
      nsCOMPtr<nsISupportsString> str = do_QueryElementAt(ignoreList, i);
      if (str) {
        nsAutoString s;
        if (NS_SUCCEEDED(str->GetData(s)) && !s.IsEmpty()) {
          if (HostIgnoredByProxy(NS_ConvertUTF16toUTF8(s), aHost)) {
            aResult.AppendLiteral("DIRECT");
            return NS_OK;
          }
        }
      }
    }
  }

  bool useHttpProxyForAll = false;
  // This setting sometimes doesn't exist, don't bail on failure
  mGConf->GetBool(NS_LITERAL_CSTRING("/system/http_proxy/use_same_proxy"), &useHttpProxyForAll);

  nsresult rv;
  if (!useHttpProxyForAll) {
    rv = SetProxyResultFromGConf("/system/proxy/socks_", "SOCKS", aResult);
    if (NS_SUCCEEDED(rv))
      return rv;
  }
  
  if (aScheme.LowerCaseEqualsLiteral("http") || useHttpProxyForAll) {
    rv = SetProxyResultFromGConf("/system/http_proxy/", "PROXY", aResult);
  } else if (aScheme.LowerCaseEqualsLiteral("https")) {
    rv = SetProxyResultFromGConf("/system/proxy/secure_", "PROXY", aResult);
  } else if (aScheme.LowerCaseEqualsLiteral("ftp")) {
    rv = SetProxyResultFromGConf("/system/proxy/ftp_", "PROXY", aResult);
  } else {
    rv = NS_ERROR_FAILURE;
  }
  
  if (NS_FAILED(rv)) {
    aResult.AppendLiteral("DIRECT");
  }
  return NS_OK;
}

nsresult
nsUnixSystemProxySettings::GetProxyFromGSettings(const nsACString& aScheme,
                                                 const nsACString& aHost,
                                                 PRInt32 aPort,
                                                 nsACString& aResult)
{
  nsCString proxyMode; 
  nsresult rv = mProxySettings->GetString(NS_LITERAL_CSTRING("mode"), proxyMode);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!proxyMode.Equals("manual")) {
    aResult.AppendLiteral("DIRECT");
    return NS_OK;
  }

  nsCOMPtr<nsIArray> ignoreList;
  if (NS_SUCCEEDED(mProxySettings->GetStringList(NS_LITERAL_CSTRING("ignore-hosts"),
                                                 getter_AddRefs(ignoreList))) && ignoreList) {
    PRUint32 len = 0;
    ignoreList->GetLength(&len);
    for (PRUint32 i = 0; i < len; ++i) {
      nsCOMPtr<nsISupportsCString> str = do_QueryElementAt(ignoreList, i);
      if (str) {
        nsCString s;
        if (NS_SUCCEEDED(str->GetData(s)) && !s.IsEmpty()) {
          if (HostIgnoredByProxy(s, aHost)) {
            aResult.AppendLiteral("DIRECT");
            return NS_OK;
          }
        }
      }
    }
  }

  if (aScheme.LowerCaseEqualsLiteral("http")) {
    rv = SetProxyResultFromGSettings("org.gnome.system.proxy.http", "PROXY", aResult);
  } else if (aScheme.LowerCaseEqualsLiteral("https")) {
    rv = SetProxyResultFromGSettings("org.gnome.system.proxy.https", "PROXY", aResult);
    /* Try to use HTTP proxy when HTTPS proxy is not explicitly defined */
    if (rv != NS_OK) 
      rv = SetProxyResultFromGSettings("org.gnome.system.proxy.http", "PROXY", aResult);
  } else if (aScheme.LowerCaseEqualsLiteral("ftp")) {
    rv = SetProxyResultFromGSettings("org.gnome.system.proxy.ftp", "PROXY", aResult);
  } else {
    rv = NS_ERROR_FAILURE;
  }
  if (rv != NS_OK) {
     /* If proxy for scheme is not specified, use SOCKS proxy for all schemes */
     rv = SetProxyResultFromGSettings("org.gnome.system.proxy.socks", "SOCKS", aResult);
  }
  
  if (NS_FAILED(rv)) {
    aResult.AppendLiteral("DIRECT");
  }
  
  return NS_OK;
}

nsresult
nsUnixSystemProxySettings::GetProxyForURI(nsIURI* aURI, nsACString& aResult)
{
  nsCAutoString scheme;
  nsresult rv = aURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString host;
  rv = aURI->GetHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 port;
  rv = aURI->GetPort(&port);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mProxySettings) {
    rv = GetProxyFromGSettings(scheme, host, port, aResult);
    if (rv == NS_OK)
      return rv;
  }
  if (mGConf)
    return GetProxyFromGConf(scheme, host, port, aResult);

  return GetProxyFromEnvironment(scheme, host, port, aResult);
}

#define NS_UNIXSYSTEMPROXYSERVICE_CID  /* 0fa3158c-d5a7-43de-9181-a285e74cf1d4 */\
     { 0x0fa3158c, 0xd5a7, 0x43de, \
       {0x91, 0x81, 0xa2, 0x85, 0xe7, 0x4c, 0xf1, 0xd4 } }

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsUnixSystemProxySettings, Init)
NS_DEFINE_NAMED_CID(NS_UNIXSYSTEMPROXYSERVICE_CID);

static const mozilla::Module::CIDEntry kUnixProxyCIDs[] = {
  { &kNS_UNIXSYSTEMPROXYSERVICE_CID, false, NULL, nsUnixSystemProxySettingsConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kUnixProxyContracts[] = {
  { NS_SYSTEMPROXYSETTINGS_CONTRACTID, &kNS_UNIXSYSTEMPROXYSERVICE_CID },
  { NULL }
};

static const mozilla::Module kUnixProxyModule = {
  mozilla::Module::kVersion,
  kUnixProxyCIDs,
  kUnixProxyContracts
};

NSMODULE_DEFN(nsUnixProxyModule) = &kUnixProxyModule;
