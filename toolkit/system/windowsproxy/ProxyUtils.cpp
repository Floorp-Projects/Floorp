/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyUtils.h"
#include "nsTArray.h"
#include "prnetdb.h"
#include "prtypes.h"

namespace mozilla {
namespace toolkit {
namespace system {

/**
 * Normalize the short IP form into the complete form.
 * For example, it converts "192.168" into "192.168.0.0"
 */
static void NormalizeAddr(const nsACString& aAddr, nsCString& aNormalized) {
  nsTArray<nsCString> addr;
  ParseString(aAddr, '.', addr);
  aNormalized = "";
  for (uint32_t i = 0; i < 4; ++i) {
    if (i != 0) {
      aNormalized.AppendLiteral(".");
    }
    if (i < addr.Length()) {
      aNormalized.Append(addr[i]);
    } else {
      aNormalized.AppendLiteral("0");
    }
  }
}

static PRUint32 MaskIPv4Addr(PRUint32 aAddr, uint16_t aMaskLen) {
  if (aMaskLen == 32) {
    return aAddr;
  }
  return PR_htonl(PR_ntohl(aAddr) & (~0L << (32 - aMaskLen)));
}

static void MaskIPv6Addr(PRIPv6Addr& aAddr, uint16_t aMaskLen) {
  if (aMaskLen == 128) {
    return;
  }

  if (aMaskLen > 96) {
    aAddr.pr_s6_addr32[3] =
        PR_htonl(PR_ntohl(aAddr.pr_s6_addr32[3]) & (~0L << (128 - aMaskLen)));
  } else if (aMaskLen > 64) {
    aAddr.pr_s6_addr32[3] = 0;
    aAddr.pr_s6_addr32[2] =
        PR_htonl(PR_ntohl(aAddr.pr_s6_addr32[2]) & (~0L << (96 - aMaskLen)));
  } else if (aMaskLen > 32) {
    aAddr.pr_s6_addr32[3] = 0;
    aAddr.pr_s6_addr32[2] = 0;
    aAddr.pr_s6_addr32[1] =
        PR_htonl(PR_ntohl(aAddr.pr_s6_addr32[1]) & (~0L << (64 - aMaskLen)));
  } else {
    aAddr.pr_s6_addr32[3] = 0;
    aAddr.pr_s6_addr32[2] = 0;
    aAddr.pr_s6_addr32[1] = 0;
    aAddr.pr_s6_addr32[0] =
        PR_htonl(PR_ntohl(aAddr.pr_s6_addr32[0]) & (~0L << (32 - aMaskLen)));
  }

  return;
}

static bool IsMatchMask(const nsACString& aHost, const nsACString& aOverride) {
  nsresult rv;

  auto tokenEnd = aOverride.FindChar('/');
  if (tokenEnd == -1) {
    return false;
  }

  nsAutoCString prefixStr(
      Substring(aOverride, tokenEnd + 1, aOverride.Length() - tokenEnd - 1));
  auto maskLen = prefixStr.ToInteger(&rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsAutoCString override(aOverride);
  NormalizeAddr(Substring(aOverride, 0, tokenEnd), override);

  PRNetAddr prAddrHost;
  PRNetAddr prAddrOverride;
  if (PR_SUCCESS !=
          PR_StringToNetAddr(PromiseFlatCString(aHost).get(), &prAddrHost) ||
      PR_SUCCESS != PR_StringToNetAddr(override.get(), &prAddrOverride)) {
    return false;
  }

  if (prAddrHost.raw.family == PR_AF_INET &&
      prAddrOverride.raw.family == PR_AF_INET) {
    return MaskIPv4Addr(prAddrHost.inet.ip, maskLen) ==
           MaskIPv4Addr(prAddrOverride.inet.ip, maskLen);
  } else if (prAddrHost.raw.family == PR_AF_INET6 &&
             prAddrOverride.raw.family == PR_AF_INET6) {
    MaskIPv6Addr(prAddrHost.ipv6.ip, maskLen);
    MaskIPv6Addr(prAddrOverride.ipv6.ip, maskLen);

    return memcmp(&prAddrHost.ipv6.ip, &prAddrOverride.ipv6.ip,
                  sizeof(PRIPv6Addr)) == 0;
  }

  return false;
}

static bool IsMatchWildcard(const nsACString& aHost,
                            const nsACString& aOverride) {
  nsAutoCString host(aHost);
  nsAutoCString override(aOverride);

  int32_t overrideLength = override.Length();
  int32_t tokenStart = 0;
  int32_t offset = 0;
  bool star = false;

  while (tokenStart < overrideLength) {
    int32_t tokenEnd = override.FindChar('*', tokenStart);
    if (tokenEnd == tokenStart) {
      // Star is the first character in the token.
      star = true;
      tokenStart++;
      // If the character following the '*' is a '.' character then skip
      // it so that "*.foo.com" allows "foo.com".
      if (override.FindChar('.', tokenStart) == tokenStart) {
        nsAutoCString token(Substring(override, tokenStart + 1,
                                      overrideLength - tokenStart - 1));
        if (host.Equals(token)) {
          return true;
        }
      }
    } else {
      if (tokenEnd == -1) {
        tokenEnd = overrideLength;  // no '*' char, match rest of string
      }
      nsAutoCString token(
          Substring(override, tokenStart, tokenEnd - tokenStart));
      offset = host.Find(token, /* aIgnoreCase = */ false, offset);
      if (offset == -1 || (!star && offset)) {
        return false;
      }
      star = false;
      tokenStart = tokenEnd;
      offset += token.Length();
    }
  }

  return (star || (offset == static_cast<int32_t>(host.Length())));
}

bool IsHostProxyEntry(const nsACString& aHost, const nsACString& aOverride) {
  return IsMatchMask(aHost, aOverride) || IsMatchWildcard(aHost, aOverride);
}

}  // namespace system
}  // namespace toolkit
}  // namespace mozilla
