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

namespace IPC {

// nsIPermissionManager utilities

struct Permission
{
  nsCString host, type;
  uint32_t capability, expireType;
  int64_t expireTime;
  uint32_t appId;
  bool isInBrowserElement;

  Permission() { }
  Permission(const nsCString& aHost,
             const uint32_t aAppId,
             const bool aIsInBrowserElement,
             const nsCString& aType,
             const uint32_t aCapability,
             const uint32_t aExpireType,
             const int64_t aExpireTime) : host(aHost),
                                          type(aType),
                                          capability(aCapability),
                                          expireType(aExpireType),
                                          expireTime(aExpireTime),
                                          appId(aAppId),
                                          isInBrowserElement(aIsInBrowserElement)
  {}
};

template<>
struct ParamTraits<Permission>
{
  static void Write(Message* aMsg, const Permission& aParam)
  {
    WriteParam(aMsg, aParam.host);
    WriteParam(aMsg, aParam.type);
    WriteParam(aMsg, aParam.capability);
    WriteParam(aMsg, aParam.expireType);
    WriteParam(aMsg, aParam.expireTime);
    WriteParam(aMsg, aParam.appId);
    WriteParam(aMsg, aParam.isInBrowserElement);
  }

  static bool Read(const Message* aMsg, void** aIter, Permission* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->host) &&
           ReadParam(aMsg, aIter, &aResult->type) &&
           ReadParam(aMsg, aIter, &aResult->capability) &&
           ReadParam(aMsg, aIter, &aResult->expireType) &&
           ReadParam(aMsg, aIter, &aResult->expireTime) &&
           ReadParam(aMsg, aIter, &aResult->appId) &&
           ReadParam(aMsg, aIter, &aResult->isInBrowserElement);
  }

  static void Log(const Permission& p, std::wstring* l)
  {
    l->append(L"(");
    LogParam(p.host, l);
    l->append(L", ");
    LogParam(p.appId, l);
    l->append(L", ");
    LogParam(p.isInBrowserElement, l);
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
struct ParamTraits<PRNetAddr>
{
  static void Write(Message* aMsg, const PRNetAddr &aParam)
  {
    WriteParam(aMsg, aParam.raw.family);
    if (aParam.raw.family == PR_AF_UNSPEC) {
      aMsg->WriteBytes(aParam.raw.data, sizeof(aParam.raw.data));
    } else if (aParam.raw.family == PR_AF_INET) {
      WriteParam(aMsg, aParam.inet.port);
      WriteParam(aMsg, aParam.inet.ip);
    } else if (aParam.raw.family == PR_AF_INET6) {
      WriteParam(aMsg, aParam.ipv6.port);
      WriteParam(aMsg, aParam.ipv6.flowinfo);
      WriteParam(aMsg, aParam.ipv6.ip.pr_s6_addr64[0]);
      WriteParam(aMsg, aParam.ipv6.ip.pr_s6_addr64[1]);
      WriteParam(aMsg, aParam.ipv6.scope_id);
#if defined(XP_UNIX) || defined(XP_OS2)
    } else if (aParam.raw.family == PR_AF_LOCAL) {
      // Train's already off the rails:  let's get a stack trace at least...
      NS_RUNTIMEABORT("Error: please post stack trace to "
                      "https://bugzilla.mozilla.org/show_bug.cgi?id=661158");
      aMsg->WriteBytes(aParam.local.path, sizeof(aParam.local.path));
#endif
    }

    /* If we get here without hitting any of the cases above, there's not much
     * we can do but let the deserializer fail when it gets this message */
  }

  static bool Read(const Message* aMsg, void** aIter, PRNetAddr* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->raw.family))
      return false;

    if (aResult->raw.family == PR_AF_UNSPEC) {
      return aMsg->ReadBytes(aIter,
                             reinterpret_cast<const char**>(&aResult->raw.data),
                             sizeof(aResult->raw.data));
    } else if (aResult->raw.family == PR_AF_INET) {
      return ReadParam(aMsg, aIter, &aResult->inet.port) &&
             ReadParam(aMsg, aIter, &aResult->inet.ip);
    } else if (aResult->raw.family == PR_AF_INET6) {
      return ReadParam(aMsg, aIter, &aResult->ipv6.port) &&
             ReadParam(aMsg, aIter, &aResult->ipv6.flowinfo) &&
             ReadParam(aMsg, aIter, &aResult->ipv6.ip.pr_s6_addr64[0]) &&
             ReadParam(aMsg, aIter, &aResult->ipv6.ip.pr_s6_addr64[1]) &&
             ReadParam(aMsg, aIter, &aResult->ipv6.scope_id);
#if defined(XP_UNIX) || defined(XP_OS2)
    } else if (aResult->raw.family == PR_AF_LOCAL) {
      return aMsg->ReadBytes(aIter,
                             reinterpret_cast<const char**>(&aResult->local.path),
                             sizeof(aResult->local.path));
#endif
    }

    /* We've been tricked by some socket family we don't know about! */
    return false;
  }
};

}

#endif // mozilla_net_NeckoMessageUtils_h
