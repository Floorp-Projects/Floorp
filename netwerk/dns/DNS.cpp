/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/DNS.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/mozalloc.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include <string.h>

#ifdef XP_WIN
#  include "ws2tcpip.h"
#endif

namespace mozilla {
namespace net {

const char* inet_ntop_internal(int af, const void* src, char* dst,
                               socklen_t size) {
#ifdef XP_WIN
  if (af == AF_INET) {
    struct sockaddr_in s;
    memset(&s, 0, sizeof(s));
    s.sin_family = AF_INET;
    memcpy(&s.sin_addr, src, sizeof(struct in_addr));
    int result = getnameinfo((struct sockaddr*)&s, sizeof(struct sockaddr_in),
                             dst, size, nullptr, 0, NI_NUMERICHOST);
    if (result == 0) {
      return dst;
    }
  } else if (af == AF_INET6) {
    struct sockaddr_in6 s;
    memset(&s, 0, sizeof(s));
    s.sin6_family = AF_INET6;
    memcpy(&s.sin6_addr, src, sizeof(struct in_addr6));
    int result = getnameinfo((struct sockaddr*)&s, sizeof(struct sockaddr_in6),
                             dst, size, nullptr, 0, NI_NUMERICHOST);
    if (result == 0) {
      return dst;
    }
  }
  return nullptr;
#else
  return inet_ntop(af, src, dst, size);
#endif
}

// Copies the contents of a PRNetAddr to a NetAddr.
// Does not do a ptr safety check!
void PRNetAddrToNetAddr(const PRNetAddr* prAddr, NetAddr* addr) {
  if (prAddr->raw.family == PR_AF_INET) {
    addr->inet.family = AF_INET;
    addr->inet.port = prAddr->inet.port;
    addr->inet.ip = prAddr->inet.ip;
  } else if (prAddr->raw.family == PR_AF_INET6) {
    addr->inet6.family = AF_INET6;
    addr->inet6.port = prAddr->ipv6.port;
    addr->inet6.flowinfo = prAddr->ipv6.flowinfo;
    memcpy(&addr->inet6.ip, &prAddr->ipv6.ip, sizeof(addr->inet6.ip.u8));
    addr->inet6.scope_id = prAddr->ipv6.scope_id;
  }
#if defined(XP_UNIX)
  else if (prAddr->raw.family == PR_AF_LOCAL) {
    addr->local.family = AF_LOCAL;
    memcpy(addr->local.path, prAddr->local.path, sizeof(addr->local.path));
  }
#endif
}

extern "C" {
// Rust bindings

uint16_t moz_netaddr_get_family(const NetAddr* addr) {
  return addr->raw.family;
}

uint32_t moz_netaddr_get_network_order_ip(const NetAddr* addr) {
  return addr->inet.ip;
}

uint8_t const* moz_netaddr_get_ipv6(const NetAddr* addr) {
  return addr->inet6.ip.u8;
}

uint16_t moz_netaddr_get_network_order_port(const NetAddr* addr) {
  if (addr->raw.family == PR_AF_INET) {
    return addr->inet.port;
  }
  if (addr->raw.family == PR_AF_INET6) {
    return addr->inet6.port;
  }
  return 0;
}

}  // extern "C"

// Copies the contents of a NetAddr to a PRNetAddr.
// Does not do a ptr safety check!
void NetAddrToPRNetAddr(const NetAddr* addr, PRNetAddr* prAddr) {
  if (addr->raw.family == AF_INET) {
    prAddr->inet.family = PR_AF_INET;
    prAddr->inet.port = addr->inet.port;
    prAddr->inet.ip = addr->inet.ip;
  } else if (addr->raw.family == AF_INET6) {
    prAddr->ipv6.family = PR_AF_INET6;
    prAddr->ipv6.port = addr->inet6.port;
    prAddr->ipv6.flowinfo = addr->inet6.flowinfo;
    memcpy(&prAddr->ipv6.ip, &addr->inet6.ip, sizeof(addr->inet6.ip.u8));
    prAddr->ipv6.scope_id = addr->inet6.scope_id;
  }
#if defined(XP_UNIX)
  else if (addr->raw.family == AF_LOCAL) {
    prAddr->local.family = PR_AF_LOCAL;
    memcpy(prAddr->local.path, addr->local.path, sizeof(addr->local.path));
  }
#elif defined(XP_WIN)
  else if (addr->raw.family == AF_LOCAL) {
    prAddr->local.family = PR_AF_LOCAL;
    memcpy(prAddr->local.path, addr->local.path, sizeof(addr->local.path));
  }
#endif
}

bool NetAddr::ToStringBuffer(char* buf, uint32_t bufSize) const {
  const NetAddr* addr = this;
  if (addr->raw.family == AF_INET) {
    if (bufSize < INET_ADDRSTRLEN) {
      return false;
    }
    struct in_addr nativeAddr = {};
    nativeAddr.s_addr = addr->inet.ip;
    return !!inet_ntop_internal(AF_INET, &nativeAddr, buf, bufSize);
  }
  if (addr->raw.family == AF_INET6) {
    if (bufSize < INET6_ADDRSTRLEN) {
      return false;
    }
    struct in6_addr nativeAddr = {};
    memcpy(&nativeAddr.s6_addr, &addr->inet6.ip, sizeof(addr->inet6.ip.u8));
    return !!inet_ntop_internal(AF_INET6, &nativeAddr, buf, bufSize);
  }
#if defined(XP_UNIX)
  if (addr->raw.family == AF_LOCAL) {
    if (bufSize < sizeof(addr->local.path)) {
      // Many callers don't bother checking our return value, so
      // null-terminate just in case.
      if (bufSize > 0) {
        buf[0] = '\0';
      }
      return false;
    }

    // Usually, the size passed to memcpy should be the size of the
    // destination. Here, we know that the source is no larger than the
    // destination, so using the source's size is always safe, whereas
    // using the destination's size may cause us to read off the end of the
    // source.
    memcpy(buf, addr->local.path, sizeof(addr->local.path));
    return true;
  }
#endif
  return false;
}

nsCString NetAddr::ToString() const {
  nsCString out;
  out.SetLength(kNetAddrMaxCStrBufSize);
  if (ToStringBuffer(out.BeginWriting(), kNetAddrMaxCStrBufSize)) {
    out.SetLength(strlen(out.BeginWriting()));
    return out;
  }
  return ""_ns;
}

bool NetAddr::IsLoopbackAddr() const {
  if (IsLoopBackAddressWithoutIPv6Mapping()) {
    return true;
  }
  const NetAddr* addr = this;
  if (addr->raw.family != AF_INET6) {
    return false;
  }

  return IPv6ADDR_IS_V4MAPPED(&addr->inet6.ip) &&
         IPv6ADDR_V4MAPPED_TO_IPADDR(&addr->inet6.ip) == htonl(INADDR_LOOPBACK);
}

bool NetAddr::IsLoopBackAddressWithoutIPv6Mapping() const {
  const NetAddr* addr = this;
  if (addr->raw.family == AF_INET) {
    // Consider 127.0.0.1/8 as loopback
    uint32_t ipv4Addr = ntohl(addr->inet.ip);
    return (ipv4Addr >> 24) == 127;
  }

  return addr->raw.family == AF_INET6 && IPv6ADDR_IS_LOOPBACK(&addr->inet6.ip);
}

bool IsLoopbackHostname(const nsACString& aAsciiHost) {
  // If the user has configured to proxy localhost addresses don't consider them
  // to be secure
  if (StaticPrefs::network_proxy_allow_hijacking_localhost() &&
      !StaticPrefs::network_proxy_testing_localhost_is_secure_when_hijacked()) {
    return false;
  }

  nsAutoCString host;
  nsContentUtils::ASCIIToLower(aAsciiHost, host);

  return host.EqualsLiteral("localhost") ||
         StringEndsWith(host, ".localhost"_ns);
}

bool HostIsIPLiteral(const nsACString& aAsciiHost) {
  NetAddr addr;
  return NS_SUCCEEDED(addr.InitFromString(aAsciiHost));
}

bool NetAddr::IsIPAddrAny() const {
  if (this->raw.family == AF_INET) {
    if (this->inet.ip == htonl(INADDR_ANY)) {
      return true;
    }
  } else if (this->raw.family == AF_INET6) {
    if (IPv6ADDR_IS_UNSPECIFIED(&this->inet6.ip)) {
      return true;
    }
    if (IPv6ADDR_IS_V4MAPPED(&this->inet6.ip) &&
        IPv6ADDR_V4MAPPED_TO_IPADDR(&this->inet6.ip) == htonl(INADDR_ANY)) {
      return true;
    }
  }
  return false;
}

NetAddr::NetAddr(const PRNetAddr* prAddr) { PRNetAddrToNetAddr(prAddr, this); }

nsresult NetAddr::InitFromString(const nsACString& aString, uint16_t aPort) {
  PRNetAddr prAddr{};
  memset(&prAddr, 0, sizeof(PRNetAddr));
  if (PR_StringToNetAddr(PromiseFlatCString(aString).get(), &prAddr) !=
      PR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  PRNetAddrToNetAddr(&prAddr, this);

  if (this->raw.family == PR_AF_INET) {
    this->inet.port = PR_htons(aPort);
  } else if (this->raw.family == PR_AF_INET6) {
    this->inet6.port = PR_htons(aPort);
  }
  return NS_OK;
}

bool NetAddr::IsIPAddrV4() const { return this->raw.family == AF_INET; }

bool NetAddr::IsIPAddrV4Mapped() const {
  if (this->raw.family == AF_INET6) {
    return IPv6ADDR_IS_V4MAPPED(&this->inet6.ip);
  }
  return false;
}

static bool isLocalIPv4(uint32_t networkEndianIP) {
  uint32_t addr32 = ntohl(networkEndianIP);
  return addr32 >> 24 == 0x0A ||    // 10/8 prefix (RFC 1918).
         addr32 >> 20 == 0xAC1 ||   // 172.16/12 prefix (RFC 1918).
         addr32 >> 16 == 0xC0A8 ||  // 192.168/16 prefix (RFC 1918).
         addr32 >> 16 == 0xA9FE;    // 169.254/16 prefix (Link Local).
}

bool NetAddr::IsIPAddrLocal() const {
  const NetAddr* addr = this;

  // IPv4 RFC1918 and Link Local Addresses.
  if (addr->raw.family == AF_INET) {
    return isLocalIPv4(addr->inet.ip);
  }
  // IPv6 Unique and Link Local Addresses.
  // or mapped IPv4 addresses
  if (addr->raw.family == AF_INET6) {
    uint16_t addr16 = ntohs(addr->inet6.ip.u16[0]);
    if (addr16 >> 9 == 0xfc >> 1 ||    // fc00::/7 Unique Local Address.
        addr16 >> 6 == 0xfe80 >> 6) {  // fe80::/10 Link Local Address.
      return true;
    }
    if (IPv6ADDR_IS_V4MAPPED(&addr->inet6.ip)) {
      return isLocalIPv4(IPv6ADDR_V4MAPPED_TO_IPADDR(&addr->inet6.ip));
    }
  }

  // Not an IPv4/6 local address.
  return false;
}

bool NetAddr::IsIPAddrShared() const {
  const NetAddr* addr = this;

  // IPv4 RFC6598.
  if (addr->raw.family == AF_INET) {
    uint32_t addr32 = ntohl(addr->inet.ip);
    if (addr32 >> 22 == 0x644 >> 2) {  // 100.64/10 prefix (RFC 6598).
      return true;
    }
  }

  // Not an IPv4 shared address.
  return false;
}

nsresult NetAddr::GetPort(uint16_t* aResult) const {
  uint16_t port;
  if (this->raw.family == PR_AF_INET) {
    port = this->inet.port;
  } else if (this->raw.family == PR_AF_INET6) {
    port = this->inet6.port;
  } else {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aResult = ntohs(port);
  return NS_OK;
}

bool NetAddr::operator==(const NetAddr& other) const {
  if (this->raw.family != other.raw.family) {
    return false;
  }
  if (this->raw.family == AF_INET) {
    return (this->inet.port == other.inet.port) &&
           (this->inet.ip == other.inet.ip);
  }
  if (this->raw.family == AF_INET6) {
    return (this->inet6.port == other.inet6.port) &&
           (this->inet6.flowinfo == other.inet6.flowinfo) &&
           (memcmp(&this->inet6.ip, &other.inet6.ip, sizeof(this->inet6.ip)) ==
            0) &&
           (this->inet6.scope_id == other.inet6.scope_id);
#if defined(XP_UNIX)
  }
  if (this->raw.family == AF_LOCAL) {
    return strncmp(this->local.path, other.local.path,
                   ArrayLength(this->local.path));
#endif
  }
  return false;
}

bool NetAddr::operator<(const NetAddr& other) const {
  if (this->raw.family != other.raw.family) {
    return this->raw.family < other.raw.family;
  }
  if (this->raw.family == AF_INET) {
    if (this->inet.ip == other.inet.ip) {
      return this->inet.port < other.inet.port;
    }
    return this->inet.ip < other.inet.ip;
  }
  if (this->raw.family == AF_INET6) {
    int cmpResult =
        memcmp(&this->inet6.ip, &other.inet6.ip, sizeof(this->inet6.ip));
    if (cmpResult) {
      return cmpResult < 0;
    }
    if (this->inet6.port != other.inet6.port) {
      return this->inet6.port < other.inet6.port;
    }
    return this->inet6.flowinfo < other.inet6.flowinfo;
  }
  return false;
}

AddrInfo::AddrInfo(const nsACString& host, const PRAddrInfo* prAddrInfo,
                   bool disableIPv4, bool filterNameCollision,
                   const nsACString& cname)
    : mHostName(host), mCanonicalName(cname) {
  MOZ_ASSERT(prAddrInfo,
             "Cannot construct AddrInfo with a null prAddrInfo pointer!");
  const uint32_t nameCollisionAddr = htonl(0x7f003535);  // 127.0.53.53

  PRNetAddr tmpAddr;
  void* iter = nullptr;
  do {
    iter = PR_EnumerateAddrInfo(iter, prAddrInfo, 0, &tmpAddr);
    bool addIt = iter && (!disableIPv4 || tmpAddr.raw.family != PR_AF_INET) &&
                 (!filterNameCollision || tmpAddr.raw.family != PR_AF_INET ||
                  (tmpAddr.inet.ip != nameCollisionAddr));
    if (addIt) {
      NetAddr elem(&tmpAddr);
      mAddresses.AppendElement(elem);
    }
  } while (iter);
}

AddrInfo::AddrInfo(const nsACString& host, const nsACString& cname,
                   DNSResolverType aResolverType, unsigned int aTRRType,
                   nsTArray<NetAddr>&& addresses)
    : mHostName(host),
      mCanonicalName(cname),
      mResolverType(aResolverType),
      mTRRType(aTRRType),
      mAddresses(std::move(addresses)) {}

AddrInfo::AddrInfo(const nsACString& host, DNSResolverType aResolverType,
                   unsigned int aTRRType, nsTArray<NetAddr>&& addresses,
                   uint32_t aTTL)
    : ttl(aTTL),
      mHostName(host),
      mCanonicalName(),
      mResolverType(aResolverType),
      mTRRType(aTRRType),
      mAddresses(std::move(addresses)) {}

// deep copy constructor
AddrInfo::AddrInfo(const AddrInfo* src) {
  mHostName = src->mHostName;
  mCanonicalName = src->mCanonicalName;
  ttl = src->ttl;
  mResolverType = src->mResolverType;
  mTRRType = src->mTRRType;
  mTrrFetchDuration = src->mTrrFetchDuration;
  mTrrFetchDurationNetworkOnly = src->mTrrFetchDurationNetworkOnly;

  mAddresses = src->mAddresses.Clone();
}

AddrInfo::~AddrInfo() = default;

size_t AddrInfo::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);
  n += mHostName.SizeOfExcludingThisIfUnshared(mallocSizeOf);
  n += mCanonicalName.SizeOfExcludingThisIfUnshared(mallocSizeOf);
  n += mAddresses.ShallowSizeOfExcludingThis(mallocSizeOf);
  return n;
}

}  // namespace net
}  // namespace mozilla
