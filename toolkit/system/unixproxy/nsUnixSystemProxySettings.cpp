/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Robert O'Callahan (robert@ocallahan.org)
 *    Michael Ventnor (m.ventnor@gmail.com)
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

class nsUnixSystemProxySettings : public nsISystemProxySettings {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISYSTEMPROXYSETTINGS

  nsUnixSystemProxySettings() {}
  nsresult Init();

private:
  ~nsUnixSystemProxySettings() {}
  
  nsCOMPtr<nsIGConfService> mGConf;
  bool IsProxyMode(const char* aMode);
  nsresult SetProxyResultFromGConf(const char* aKeyBase, const char* aType, nsACString& aResult);
  nsresult GetProxyFromGConf(const nsACString& aScheme, const nsACString& aHost, PRInt32 aPort, nsACString& aResult);
};

NS_IMPL_ISUPPORTS1(nsUnixSystemProxySettings, nsISystemProxySettings)

nsresult
nsUnixSystemProxySettings::Init()
{
  mGConf = do_GetService(NS_GCONFSERVICE_CONTRACTID);
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
  if (!mGConf || !IsProxyMode("auto")) {
    // Return an empty string in this case
    aResult.Truncate();
    return NS_OK;
  }

  return mGConf->GetString(NS_LITERAL_CSTRING("/system/proxy/autoconfig_url"),
                           aResult);
}

static bool
IsInNoProxyList(const nsACString& aHost, PRInt32 aPort, const char* noProxyVal)
{
  NS_ASSERTION(aPort >= 0, "Negative port?");
  
  nsCAutoString noProxy(noProxyVal);
  if (noProxy.EqualsLiteral("*"))
    return PR_TRUE;
    
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
        return PR_TRUE;
    }
    
    pos = nextPos;
  }
  
  return PR_FALSE;
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
  if (PR_StringToNetAddr(PromiseFlatCString(aName).get(), &addr) != PR_SUCCESS)
    return PR_FALSE;

  PRIPv6Addr ipv6;
  // convert parsed address to IPv6
  if (addr.raw.family == PR_AF_INET) {
    // convert to IPv4-mapped address
    PR_ConvertIPv4AddrToIPv6(addr.inet.ip, &ipv6);
  } else if (addr.raw.family == PR_AF_INET6) {
    // copy the address
    memcpy(&ipv6, &addr.ipv6.ip, sizeof(PRIPv6Addr));
  } else {
    return PR_FALSE;
  }
  
  return PR_TRUE;
}

static bool GConfIgnoreHost(const nsACString& aIgnore,
                              const nsACString& aHost)
{
  if (aIgnore.Equals(aHost, nsCaseInsensitiveCStringComparator()))
    return PR_TRUE;

  if (aIgnore.First() == '*' &&
      StringEndsWith(aHost, nsDependentCSubstring(aIgnore, 1),
                     nsCaseInsensitiveCStringComparator()))
    return PR_TRUE;

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

  PRIPv6Addr ignoreAddr, hostAddr;
  if (!ConvertToIPV6Addr(aIgnore, &ignoreAddr) ||
      !ConvertToIPV6Addr(aHost, &hostAddr))
    return PR_FALSE;

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
          if (GConfIgnoreHost(NS_ConvertUTF16toUTF8(s), aHost)) {
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

  if (!mGConf)
    return GetProxyFromEnvironment(scheme, host, port, aResult);

  return GetProxyFromGConf(scheme, host, port, aResult);
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
