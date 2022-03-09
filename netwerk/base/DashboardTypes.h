/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DashboardTypes_h_
#define mozilla_net_DashboardTypes_h_

#include "ipc/IPCMessageUtils.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "nsHttp.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace net {

struct SocketInfo {
  nsCString host;
  uint64_t sent;
  uint64_t received;
  uint16_t port;
  bool active;
  nsCString type;
};

inline bool operator==(const SocketInfo& a, const SocketInfo& b) {
  return a.host == b.host && a.sent == b.sent && a.received == b.received &&
         a.port == b.port && a.active == b.active && a.type == b.type;
}

struct DnsAndConnectSockets {
  bool speculative;
};

struct DNSCacheEntries {
  nsCString hostname;
  nsTArray<nsCString> hostaddr;
  uint16_t family;
  int64_t expiration;
  nsCString netInterface;
  bool TRR;
  nsCString originAttributesSuffix;
  nsCString flags;
};

struct HttpConnInfo {
  uint32_t ttl;
  uint32_t rtt;
  nsString protocolVersion;

  void SetHTTPProtocolVersion(HttpVersion pv);
};

struct HttpRetParams {
  nsCString host;
  CopyableTArray<HttpConnInfo> active;
  CopyableTArray<HttpConnInfo> idle;
  CopyableTArray<DnsAndConnectSockets> dnsAndSocks;
  uint32_t counter;
  uint16_t port;
  nsCString httpVersion;
  bool ssl;
};

}  // namespace net
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::net::SocketInfo> {
  typedef mozilla::net::SocketInfo paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.host);
    WriteParam(aWriter, aParam.sent);
    WriteParam(aWriter, aParam.received);
    WriteParam(aWriter, aParam.port);
    WriteParam(aWriter, aParam.active);
    WriteParam(aWriter, aParam.type);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->host) &&
           ReadParam(aReader, &aResult->sent) &&
           ReadParam(aReader, &aResult->received) &&
           ReadParam(aReader, &aResult->port) &&
           ReadParam(aReader, &aResult->active) &&
           ReadParam(aReader, &aResult->type);
  }
};

template <>
struct ParamTraits<mozilla::net::DNSCacheEntries> {
  typedef mozilla::net::DNSCacheEntries paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.hostname);
    WriteParam(aWriter, aParam.hostaddr);
    WriteParam(aWriter, aParam.family);
    WriteParam(aWriter, aParam.expiration);
    WriteParam(aWriter, aParam.netInterface);
    WriteParam(aWriter, aParam.TRR);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->hostname) &&
           ReadParam(aReader, &aResult->hostaddr) &&
           ReadParam(aReader, &aResult->family) &&
           ReadParam(aReader, &aResult->expiration) &&
           ReadParam(aReader, &aResult->netInterface) &&
           ReadParam(aReader, &aResult->TRR);
  }
};

template <>
struct ParamTraits<mozilla::net::DnsAndConnectSockets> {
  typedef mozilla::net::DnsAndConnectSockets paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.speculative);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->speculative);
  }
};

template <>
struct ParamTraits<mozilla::net::HttpConnInfo> {
  typedef mozilla::net::HttpConnInfo paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.ttl);
    WriteParam(aWriter, aParam.rtt);
    WriteParam(aWriter, aParam.protocolVersion);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->ttl) &&
           ReadParam(aReader, &aResult->rtt) &&
           ReadParam(aReader, &aResult->protocolVersion);
  }
};

template <>
struct ParamTraits<mozilla::net::HttpRetParams> {
  typedef mozilla::net::HttpRetParams paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.host);
    WriteParam(aWriter, aParam.active);
    WriteParam(aWriter, aParam.idle);
    WriteParam(aWriter, aParam.dnsAndSocks);
    WriteParam(aWriter, aParam.counter);
    WriteParam(aWriter, aParam.port);
    WriteParam(aWriter, aParam.httpVersion);
    WriteParam(aWriter, aParam.ssl);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->host) &&
           ReadParam(aReader, &aResult->active) &&
           ReadParam(aReader, &aResult->idle) &&
           ReadParam(aReader, &aResult->dnsAndSocks) &&
           ReadParam(aReader, &aResult->counter) &&
           ReadParam(aReader, &aResult->port) &&
           ReadParam(aReader, &aResult->httpVersion) &&
           ReadParam(aReader, &aResult->ssl);
  }
};

}  // namespace IPC

#endif  // mozilla_net_DashboardTypes_h_
