/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DashboardTypes_h_
#define mozilla_net_DashboardTypes_h_

#include "mozilla/StandardInteger.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

namespace mozilla {
namespace net {

struct SocketInfo
{
    nsCString host;
    uint64_t  sent;
    uint64_t  received;
    uint16_t  port;
    bool      active;
    bool      tcp;
};

struct DNSCacheEntries
{
    nsCString hostname;
    nsTArray<nsCString> hostaddr;
    uint16_t family;
    int64_t expiration;
};

struct HttpConnInfo
{
    uint32_t ttl;
    uint32_t rtt;
};

struct HttpRetParams
{
    nsCString host;
    nsTArray<HttpConnInfo>   active;
    nsTArray<HttpConnInfo>   idle;
    uint32_t  counter;
    uint16_t  port;
    bool      spdy;
    bool      ssl;
};

} }

#endif // mozilla_net_DashboardTypes_h_
