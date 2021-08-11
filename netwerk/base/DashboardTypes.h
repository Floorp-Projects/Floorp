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

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.host);
    WriteParam(aMsg, aParam.sent);
    WriteParam(aMsg, aParam.received);
    WriteParam(aMsg, aParam.port);
    WriteParam(aMsg, aParam.active);
    WriteParam(aMsg, aParam.type);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->host) &&
           ReadParam(aMsg, aIter, &aResult->sent) &&
           ReadParam(aMsg, aIter, &aResult->received) &&
           ReadParam(aMsg, aIter, &aResult->port) &&
           ReadParam(aMsg, aIter, &aResult->active) &&
           ReadParam(aMsg, aIter, &aResult->type);
  }
};

template <>
struct ParamTraits<mozilla::net::DNSCacheEntries> {
  typedef mozilla::net::DNSCacheEntries paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.hostname);
    WriteParam(aMsg, aParam.hostaddr);
    WriteParam(aMsg, aParam.family);
    WriteParam(aMsg, aParam.expiration);
    WriteParam(aMsg, aParam.netInterface);
    WriteParam(aMsg, aParam.TRR);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->hostname) &&
           ReadParam(aMsg, aIter, &aResult->hostaddr) &&
           ReadParam(aMsg, aIter, &aResult->family) &&
           ReadParam(aMsg, aIter, &aResult->expiration) &&
           ReadParam(aMsg, aIter, &aResult->netInterface) &&
           ReadParam(aMsg, aIter, &aResult->TRR);
  }
};

template <>
struct ParamTraits<mozilla::net::DnsAndConnectSockets> {
  typedef mozilla::net::DnsAndConnectSockets paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.speculative);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->speculative);
  }
};

template <>
struct ParamTraits<mozilla::net::HttpConnInfo> {
  typedef mozilla::net::HttpConnInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.ttl);
    WriteParam(aMsg, aParam.rtt);
    WriteParam(aMsg, aParam.protocolVersion);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->ttl) &&
           ReadParam(aMsg, aIter, &aResult->rtt) &&
           ReadParam(aMsg, aIter, &aResult->protocolVersion);
  }
};

template <>
struct ParamTraits<mozilla::net::HttpRetParams> {
  typedef mozilla::net::HttpRetParams paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.host);
    WriteParam(aMsg, aParam.active);
    WriteParam(aMsg, aParam.idle);
    WriteParam(aMsg, aParam.dnsAndSocks);
    WriteParam(aMsg, aParam.counter);
    WriteParam(aMsg, aParam.port);
    WriteParam(aMsg, aParam.httpVersion);
    WriteParam(aMsg, aParam.ssl);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->host) &&
           ReadParam(aMsg, aIter, &aResult->active) &&
           ReadParam(aMsg, aIter, &aResult->idle) &&
           ReadParam(aMsg, aIter, &aResult->dnsAndSocks) &&
           ReadParam(aMsg, aIter, &aResult->counter) &&
           ReadParam(aMsg, aIter, &aResult->port) &&
           ReadParam(aMsg, aIter, &aResult->httpVersion) &&
           ReadParam(aMsg, aIter, &aResult->ssl);
  }
};

}  // namespace IPC

#endif  // mozilla_net_DashboardTypes_h_
