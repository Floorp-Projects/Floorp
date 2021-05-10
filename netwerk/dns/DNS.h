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
#include "mozilla/MemoryReporting.h"
#include "nsTArray.h"

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

// IMPORTANT: when adding new values, always add them to the end, otherwise
// it will mess up telemetry.
// Stage_0: Receive the record before the http transaction is created.
// Stage_1: Receive the record after the http transaction is created and the
//          transaction is not dispatched.
// Stage_2: Receive the record after the http transaction is dispatched.
enum HTTPSSVC_RECEIVED_STAGE : uint32_t {
  HTTPSSVC_NOT_PRESENT = 0,
  HTTPSSVC_WITH_IPHINT_RECEIVED_STAGE_0 = 1,
  HTTPSSVC_WITHOUT_IPHINT_RECEIVED_STAGE_0 = 2,
  HTTPSSVC_WITH_IPHINT_RECEIVED_STAGE_1 = 3,
  HTTPSSVC_WITHOUT_IPHINT_RECEIVED_STAGE_1 = 4,
  HTTPSSVC_WITH_IPHINT_RECEIVED_STAGE_2 = 5,
  HTTPSSVC_WITHOUT_IPHINT_RECEIVED_STAGE_2 = 6,
  HTTPSSVC_NOT_USED = 7,
  HTTPSSVC_NO_USABLE_RECORD = 8,
};

#define HTTPS_RR_IS_USED(s) \
  (s > HTTPSSVC_NOT_PRESENT && s < HTTPSSVC_WITH_IPHINT_RECEIVED_STAGE_2)

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
  } raw{};
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

  inline NetAddr& operator=(const NetAddr& other) {
    memcpy(this, &other, sizeof(NetAddr));
    return *this;
  }

  NetAddr() { memset(this, 0, sizeof(NetAddr)); }
  explicit NetAddr(const PRNetAddr* prAddr);

  // Will parse aString into a NetAddr using PR_StringToNetAddr.
  // Returns an error code if parsing fails.
  // If aPort is non-0 will set the NetAddr's port to (the network endian
  // value of) that.
  nsresult InitFromString(const nsACString& aString, uint16_t aPort = 0);

  bool IsIPAddrAny() const;
  bool IsLoopbackAddr() const;
  bool IsLoopBackAddressWithoutIPv6Mapping() const;
  bool IsIPAddrV4() const;
  bool IsIPAddrV4Mapped() const;
  bool IsIPAddrLocal() const;
  bool IsIPAddrShared() const;
  nsresult GetPort(uint16_t* aResult) const;
  bool ToStringBuffer(char* buf, uint32_t bufSize) const;
};

#define ODOH_VERSION 0xff04
static const char kODoHQuery[] = "odoh query";
static const char hODoHConfigID[] = "odoh key id";
static const char kODoHSecret[] = "odoh secret";
static const char kODoHKey[] = "odoh key";
static const char kODoHNonce[] = "odoh nonce";

struct ObliviousDoHConfigContents {
  uint16_t mKemId{};
  uint16_t mKdfId{};
  uint16_t mAeadId{};
  nsTArray<uint8_t> mPublicKey;
};

struct ObliviousDoHConfig {
  uint16_t mVersion{};
  uint16_t mLength{};
  ObliviousDoHConfigContents mContents;
  nsTArray<uint8_t> mConfigId;
};

enum ObliviousDoHMessageType : uint8_t {
  ODOH_QUERY = 1,
  ODOH_RESPONSE = 2,
};

struct ObliviousDoHMessage {
  ObliviousDoHMessageType mType{ODOH_QUERY};
  nsTArray<uint8_t> mKeyId;
  nsTArray<uint8_t> mEncryptedMessage;
};

enum class DNSResolverType : uint32_t { Native = 0, TRR, ODoH };

class AddrInfo {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AddrInfo)

 public:
  static const uint32_t NO_TTL_DATA = (uint32_t)-1;

  // Creates an AddrInfo object.
  explicit AddrInfo(const nsACString& host, const PRAddrInfo* prAddrInfo,
                    bool disableIPv4, bool filterNameCollision,
                    const nsACString& cname);

  // Creates a basic AddrInfo object (initialize only the host, cname and TRR
  // type).
  explicit AddrInfo(const nsACString& host, const nsACString& cname,
                    DNSResolverType aResolverType, unsigned int aTRRType,
                    nsTArray<NetAddr>&& addresses);

  // Creates a basic AddrInfo object (initialize only the host and TRR status).
  explicit AddrInfo(const nsACString& host, DNSResolverType aResolverType,
                    unsigned int aTRRType, nsTArray<NetAddr>&& addresses,
                    uint32_t aTTL = NO_TTL_DATA);

  explicit AddrInfo(const AddrInfo* src);  // copy

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  bool IsTRROrODoH() const {
    return mResolverType == DNSResolverType::TRR ||
           mResolverType == DNSResolverType::ODoH;
  }
  DNSResolverType ResolverType() const { return mResolverType; }
  unsigned int TRRType() { return mTRRType; }

  double GetTrrFetchDuration() { return mTrrFetchDuration; }
  double GetTrrFetchDurationNetworkOnly() {
    return mTrrFetchDurationNetworkOnly;
  }

  const nsTArray<NetAddr>& Addresses() { return mAddresses; }
  const nsCString& Hostname() { return mHostName; }
  const nsCString& CanonicalHostname() { return mCanonicalName; }
  uint32_t TTL() { return ttl; }

  class MOZ_STACK_CLASS AddrInfoBuilder {
   public:
    explicit AddrInfoBuilder(AddrInfo* aInfo) {
      mInfo = new AddrInfo(aInfo);  // Clone it
    }

    void SetTrrFetchDurationNetworkOnly(double aTime) {
      mInfo->mTrrFetchDurationNetworkOnly = aTime;
    }

    void SetTrrFetchDuration(double aTime) { mInfo->mTrrFetchDuration = aTime; }

    void SetTTL(uint32_t aTTL) { mInfo->ttl = aTTL; }

    void SetAddresses(nsTArray<NetAddr>&& addresses) {
      mInfo->mAddresses = std::move(addresses);
    }

    void SetCanonicalHostname(const nsACString& aCname) {
      mInfo->mCanonicalName = aCname;
    }

    already_AddRefed<AddrInfo> Finish() { return mInfo.forget(); }

   private:
    RefPtr<AddrInfo> mInfo;
  };

  AddrInfoBuilder Build() { return AddrInfoBuilder(this); }

 private:
  ~AddrInfo();
  uint32_t ttl = NO_TTL_DATA;

  nsCString mHostName;
  nsCString mCanonicalName;
  DNSResolverType mResolverType = DNSResolverType::Native;
  unsigned int mTRRType = 0;
  double mTrrFetchDuration = 0;
  double mTrrFetchDurationNetworkOnly = 0;

  nsTArray<NetAddr> mAddresses;
};

// Copies the contents of a PRNetAddr to a NetAddr.
// Does not do a ptr safety check!
void PRNetAddrToNetAddr(const PRNetAddr* prAddr, NetAddr* addr);

// Copies the contents of a NetAddr to a PRNetAddr.
// Does not do a ptr safety check!
void NetAddrToPRNetAddr(const NetAddr* addr, PRNetAddr* prAddr);

bool IsLoopbackHostname(const nsACString& aAsciiHost);

bool HostIsIPLiteral(const nsACString& aAsciiHost);

}  // namespace net
}  // namespace mozilla

#endif  // DNS_h_
