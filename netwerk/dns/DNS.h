/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DNS_h_
#define DNS_h_

#include "nscore.h"
#include "prio.h"
#include "prnetdb.h"
#include "mozilla/LinkedList.h"

#if !defined(XP_WIN) && !defined(XP_OS2)
#include <arpa/inet.h>
#endif

#ifdef XP_WIN
#include "Winsock2.h"
#endif

#define IPv6ADDR_IS_LOOPBACK(a) \
  (((a)->u32[0] == 0)     &&    \
   ((a)->u32[1] == 0)     &&    \
   ((a)->u32[2] == 0)     &&    \
   ((a)->u8[12] == 0)     &&    \
   ((a)->u8[13] == 0)     &&    \
   ((a)->u8[14] == 0)     &&    \
   ((a)->u8[15] == 0x1U))

#define IPv6ADDR_IS_V4MAPPED(a) \
  (((a)->u32[0] == 0)     &&    \
   ((a)->u32[1] == 0)     &&    \
   ((a)->u8[8] == 0)      &&    \
   ((a)->u8[9] == 0)      &&    \
   ((a)->u8[10] == 0xff)  &&    \
   ((a)->u8[11] == 0xff))

#define IPv6ADDR_V4MAPPED_TO_IPADDR(a) ((a)->u32[3])

#define IPv6ADDR_IS_UNSPECIFIED(a) \
  (((a)->u32[0] == 0)  &&          \
   ((a)->u32[1] == 0)  &&          \
   ((a)->u32[2] == 0)  &&          \
   ((a)->u32[3] == 0))

namespace mozilla {
namespace net {

// Required buffer size for text form of an IP address.
// Includes space for null termination.
const int kIPv4CStrBufSize = 16;
const int kIPv6CStrBufSize = 46;

// This was all created at a time in which we were using NSPR for host
// resolution and we were propagating NSPR types like "PRAddrInfo" and
// "PRNetAddr" all over Gecko. This made it hard to use another host
// resolver -- we were locked into NSPR. The goal here is to get away
// from that. We'll translate what we get from NSPR or any other host
// resolution library into the types below and use them in Gecko.

union IPv6Addr {
  uint8_t  u8[16];
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
    uint16_t family;                /* address family (0x00ff maskable) */
    char data[14];                  /* raw address data */
  } raw;
  struct {
    uint16_t family;                /* address family (AF_INET) */
    uint16_t port;                  /* port number */
    uint32_t ip;                    /* The actual 32 bits of address */
  } inet;
  struct {
    uint16_t family;                /* address family (AF_INET6) */
    uint16_t port;                  /* port number */
    uint32_t flowinfo;              /* routing information */
    IPv6Addr ip;                    /* the actual 128 bits of address */
    uint32_t scope_id;              /* set of interfaces for a scope */
  } inet6;
#if defined(XP_UNIX) || defined(XP_OS2)
  struct {                          /* Unix domain socket address */
    uint16_t family;                /* address family (AF_UNIX) */
#ifdef XP_OS2
    char path[108];                 /* null-terminated pathname */
#else
    char path[104];                 /* null-terminated pathname */
#endif
  } local;
#endif
};

// This class wraps a NetAddr union to provide C++ linked list
// capabilities and other methods. It is created from a PRNetAddr,
// which is converted to a mozilla::dns::NetAddr.
class NetAddrElement : public LinkedListElement<NetAddrElement> {
public:
  NetAddrElement(const PRNetAddr *prNetAddr);
  ~NetAddrElement();

  NetAddr mAddress;
};
    
class AddrInfo {
public:
  AddrInfo(const char *host, const PRAddrInfo *prAddrInfo);
  ~AddrInfo();

  char *mHostName;
  LinkedList<NetAddrElement> mAddresses;
};

// Copies the contents of a PRNetAddr to a NetAddr.
// Does not do a ptr safety check!
void PRNetAddrToNetAddr(const PRNetAddr *prAddr, NetAddr *addr);

// Copies the contents of a NetAddr to a PRNetAddr.
// Does not do a ptr safety check!
void NetAddrToPRNetAddr(const NetAddr *addr, PRNetAddr *prAddr);

bool NetAddrToString(const NetAddr *addr, char *buf, uint32_t bufSize);

bool IsLoopBackAddress(const NetAddr *addr);

bool IsIPAddrAny(const NetAddr *addr);

bool IsIPAddrV4Mapped(const NetAddr *addr);

} // namespace net
} // namespace mozilla

#endif // DNS_h_
