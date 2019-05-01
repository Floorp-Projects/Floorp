/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DNS_h_
#define DNS_h_

#include "nscore.h"
#include "nsString.h"
#include "prio.h"
#include "prnetdb.h"
#include "plstr.h"
#include "nsISupportsImpl.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"

#if !defined(XP_WIN)
#  include <arpa/inet.h>
#endif

#ifdef XP_WIN
#  include "winsock2.h"
#endif

#ifndef AF_LOCAL
#  define AF_LOCAL 1  // used for named pipe
#endif

#define IPv6ADDR_IS_LOOPBACK(a)                                      \
  (((a)->u32[0] == 0) && ((a)->u32[1] == 0) && ((a)->u32[2] == 0) && \
   ((a)->u8[12] == 0) && ((a)->u8[13] == 0) && ((a)->u8[14] == 0) && \
   ((a)->u8[15] == 0x1U))

#define IPv6ADDR_IS_V4MAPPED(a)                                     \
  (((a)->u32[0] == 0) && ((a)->u32[1] == 0) && ((a)->u8[8] == 0) && \
   ((a)->u8[9] == 0) && ((a)->u8[10] == 0xff) && ((a)->u8[11] == 0xff))

#define IPv6ADDR_V4MAPPED_TO_IPADDR(a) ((a)->u32[3])

#define IPv6ADDR_IS_UNSPECIFIED(a)                                   \
  (((a)->u32[0] == 0) && ((a)->u32[1] == 0) && ((a)->u32[2] == 0) && \
   ((a)->u32[3] == 0))

namespace mozilla {
namespace net {

// Required buffer size for text form of an IP address.
// Includes space for null termination. We make our own contants
// because we don't want higher-level code depending on things
// like INET6_ADDRSTRLEN and having to include the associated
// platform-specific headers.
#ifdef XP_WIN
// Windows requires longer buffers for some reason.
const int kIPv4CStrBufSize = 22;
const int kIPv6CStrBufSize = 65;
const int kNetAddrMaxCStrBufSize = kIPv6CStrBufSize;
#else
const int kIPv4CStrBufSize = 16;
const int kIPv6CStrBufSize = 46;
const int kLocalCStrBufSize = 108;
const int kNetAddrMaxCStrBufSize = kLocalCStrBufSize;
#endif

// This was all created at a time in which we were using NSPR for host
// resolution and we were propagating NSPR types like "PRAddrInfo" and
// "PRNetAddr" all over Gecko. This made it hard to use another host
// resolver -- we were locked into NSPR. The goal here is to get away
// from that. We'll translate what we get from NSPR or any other host
// resolution library into the types below and use them in Gecko.

union IPv6Addr {
  uint8_t u8[16];
  uint16_t u16[8];
  uint32_t u32[4];
  uint64_t u64[2];
};

// This struct is similar to operating system structs like "sockaddr", used for
// things like "connect" and "getsockname". When tempted to cast or do dumb
// copies of this struct to another struct, bear compiler-computed padding
// in mind. The size of this struct, and the layout of the data in it, may
// not be what you expect.
union NetAddr {
  struct {
    uint16_t family; /* address family (0x00ff maskable) */
    char data[14];   /* raw address data */
  } raw;
  struct {
    uint16_t family; /* address family (AF_INET) */
    uint16_t port;   /* port number */
    uint32_t ip;     /* The actual 32 bits of address */
  } inet;
  struct {
    uint16_t family;   /* address family (AF_INET6) */
    uint16_t port;     /* port number */
    uint32_t flowinfo; /* routing information */
    IPv6Addr ip;       /* the actual 128 bits of address */
    uint32_t scope_id; /* set of interfaces for a scope */
  } inet6;
#if defined(XP_UNIX) || defined(XP_WIN)
  struct {           /* Unix domain socket or
                        Windows Named Pipes address */
    uint16_t family; /* address family (AF_UNIX) */
    char path[104];  /* null-terminated pathname */
  } local;
#endif
  // introduced to support nsTArray<NetAddr> comparisons and sorting
  bool operator==(const NetAddr& other) const;
  bool operator<(const NetAddr& other) const;
};

// This class wraps a NetAddr union to provide C++ linked list
// capabilities and other methods. It is created from a PRNetAddr,
// which is converted to a mozilla::dns::NetAddr.
class NetAddrElement : public LinkedListElement<NetAddrElement> {
 public:
  explicit NetAddrElement(const PRNetAddr* prNetAddr);
  NetAddrElement(const NetAddrElement& netAddr);
  ~NetAddrElement();

  NetAddr mAddress;
};

class AddrInfo {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AddrInfo)

 public:
  // Creates an AddrInfo object.
  explicit AddrInfo(const nsACString& host, const PRAddrInfo* prAddrInfo,
                    bool disableIPv4, bool filterNameCollision,
                    const nsACString& cname);

  // Creates a basic AddrInfo object (initialize only the host, cname and TRR
  // type).
  explicit AddrInfo(const nsACString& host, const nsACString& cname,
                    unsigned int TRRType);

  // Creates a basic AddrInfo object (initialize only the host and TRR status).
  explicit AddrInfo(const nsACString& host, unsigned int TRRType);

  explicit AddrInfo(const AddrInfo* src);  // copy

  void AddAddress(NetAddrElement* address);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  nsCString mHostName;
  nsCString mCanonicalName;
  uint32_t ttl;
  static const uint32_t NO_TTL_DATA = (uint32_t)-1;

  AutoCleanLinkedList<NetAddrElement> mAddresses;
  unsigned int IsTRR() { return mFromTRR; }

 private:
  ~AddrInfo();
  unsigned int mFromTRR;
};

// Copies the contents of a PRNetAddr to a NetAddr.
// Does not do a ptr safety check!
void PRNetAddrToNetAddr(const PRNetAddr* prAddr, NetAddr* addr);

// Copies the contents of a NetAddr to a PRNetAddr.
// Does not do a ptr safety check!
void NetAddrToPRNetAddr(const NetAddr* addr, PRNetAddr* prAddr);

bool NetAddrToString(const NetAddr* addr, char* buf, uint32_t bufSize);

bool IsLoopBackAddress(const NetAddr* addr);

bool IsIPAddrAny(const NetAddr* addr);

bool IsIPAddrV4Mapped(const NetAddr* addr);

bool IsIPAddrLocal(const NetAddr* addr);

nsresult GetPort(const NetAddr* aAddr, uint16_t* aResult);

}  // namespace net
}  // namespace mozilla

#endif  // DNS_h_
