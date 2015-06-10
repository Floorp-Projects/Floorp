/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_NeckoMessageUtils_h
#define mozilla_net_NeckoMessageUtils_h

#include "mozilla/DebugOnly.h"

#include "ipc/IPCMessageUtils.h"
#include "nsStringGlue.h"
#include "prio.h"
#include "mozilla/net/DNS.h"
#include "TimingStruct.h"

namespace IPC {

// nsIPermissionManager utilities

struct Permission
{
  nsCString origin, type;
  uint32_t capability, expireType;
  int64_t expireTime;

  Permission() { }
  Permission(const nsCString& aOrigin,
             const nsCString& aType,
             const uint32_t aCapability,
             const uint32_t aExpireType,
             const int64_t aExpireTime) : origin(aOrigin),
                                          type(aType),
                                          capability(aCapability),
                                          expireType(aExpireType),
                                          expireTime(aExpireTime)
  {}
};

template<>
struct ParamTraits<Permission>
{
  static void Write(Message* aMsg, const Permission& aParam)
  {
    WriteParam(aMsg, aParam.origin);
    WriteParam(aMsg, aParam.type);
    WriteParam(aMsg, aParam.capability);
    WriteParam(aMsg, aParam.expireType);
    WriteParam(aMsg, aParam.expireTime);
  }

  static bool Read(const Message* aMsg, void** aIter, Permission* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->origin) &&
           ReadParam(aMsg, aIter, &aResult->type) &&
           ReadParam(aMsg, aIter, &aResult->capability) &&
           ReadParam(aMsg, aIter, &aResult->expireType) &&
           ReadParam(aMsg, aIter, &aResult->expireTime);
  }

  static void Log(const Permission& p, std::wstring* l)
  {
    l->append(L"(");
    LogParam(p.origin, l);
    l->append(L", ");
    LogParam(p.capability, l);
    l->append(L", ");
    LogParam(p.expireTime, l);
    l->append(L", ");
    LogParam(p.expireType, l);
    l->append(L")");
  }
};

template<>
struct ParamTraits<mozilla::net::NetAddr>
{
  static void Write(Message* aMsg, const mozilla::net::NetAddr &aParam)
  {
    WriteParam(aMsg, aParam.raw.family);
    if (aParam.raw.family == AF_UNSPEC) {
      aMsg->WriteBytes(aParam.raw.data, sizeof(aParam.raw.data));
    } else if (aParam.raw.family == AF_INET) {
      WriteParam(aMsg, aParam.inet.port);
      WriteParam(aMsg, aParam.inet.ip);
    } else if (aParam.raw.family == AF_INET6) {
      WriteParam(aMsg, aParam.inet6.port);
      WriteParam(aMsg, aParam.inet6.flowinfo);
      WriteParam(aMsg, aParam.inet6.ip.u64[0]);
      WriteParam(aMsg, aParam.inet6.ip.u64[1]);
      WriteParam(aMsg, aParam.inet6.scope_id);
#if defined(XP_UNIX)
    } else if (aParam.raw.family == AF_LOCAL) {
      // Train's already off the rails:  let's get a stack trace at least...
      NS_RUNTIMEABORT("Error: please post stack trace to "
                      "https://bugzilla.mozilla.org/show_bug.cgi?id=661158");
      aMsg->WriteBytes(aParam.local.path, sizeof(aParam.local.path));
#endif
    }

    /* If we get here without hitting any of the cases above, there's not much
     * we can do but let the deserializer fail when it gets this message */
  }

  static bool Read(const Message* aMsg, void** aIter, mozilla::net::NetAddr* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->raw.family))
      return false;

    if (aResult->raw.family == AF_UNSPEC) {
      return aMsg->ReadBytes(aIter,
                             reinterpret_cast<const char**>(&aResult->raw.data),
                             sizeof(aResult->raw.data));
    } else if (aResult->raw.family == AF_INET) {
      return ReadParam(aMsg, aIter, &aResult->inet.port) &&
             ReadParam(aMsg, aIter, &aResult->inet.ip);
    } else if (aResult->raw.family == AF_INET6) {
      return ReadParam(aMsg, aIter, &aResult->inet6.port) &&
             ReadParam(aMsg, aIter, &aResult->inet6.flowinfo) &&
             ReadParam(aMsg, aIter, &aResult->inet6.ip.u64[0]) &&
             ReadParam(aMsg, aIter, &aResult->inet6.ip.u64[1]) &&
             ReadParam(aMsg, aIter, &aResult->inet6.scope_id);
#if defined(XP_UNIX)
    } else if (aResult->raw.family == AF_LOCAL) {
      return aMsg->ReadBytes(aIter,
                             reinterpret_cast<const char**>(&aResult->local.path),
                             sizeof(aResult->local.path));
#endif
    }

    /* We've been tricked by some socket family we don't know about! */
    return false;
  }
};

template<>
struct ParamTraits<mozilla::net::ResourceTimingStruct>
{
  static void Write(Message* aMsg, const mozilla::net::ResourceTimingStruct& aParam)
  {
    WriteParam(aMsg, aParam.domainLookupStart);
    WriteParam(aMsg, aParam.domainLookupEnd);
    WriteParam(aMsg, aParam.connectStart);
    WriteParam(aMsg, aParam.connectEnd);
    WriteParam(aMsg, aParam.requestStart);
    WriteParam(aMsg, aParam.responseStart);
    WriteParam(aMsg, aParam.responseEnd);

    WriteParam(aMsg, aParam.fetchStart);
    WriteParam(aMsg, aParam.redirectStart);
    WriteParam(aMsg, aParam.redirectEnd);
  }

  static bool Read(const Message* aMsg, void** aIter, mozilla::net::ResourceTimingStruct* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->domainLookupStart) &&
           ReadParam(aMsg, aIter, &aResult->domainLookupEnd) &&
           ReadParam(aMsg, aIter, &aResult->connectStart) &&
           ReadParam(aMsg, aIter, &aResult->connectEnd) &&
           ReadParam(aMsg, aIter, &aResult->requestStart) &&
           ReadParam(aMsg, aIter, &aResult->responseStart) &&
           ReadParam(aMsg, aIter, &aResult->responseEnd) &&
           ReadParam(aMsg, aIter, &aResult->fetchStart) &&
           ReadParam(aMsg, aIter, &aResult->redirectStart) &&
           ReadParam(aMsg, aIter, &aResult->redirectEnd);
  }
};

} // namespace IPC

#endif // mozilla_net_NeckoMessageUtils_h
