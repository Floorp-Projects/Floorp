/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <netdb.h>

#include "mozilla/DebugOnly.h"
#include "mozilla/ScopeExit.h"

#include "NetworkInfoServiceImpl.h"

namespace mozilla {
namespace net {

static nsresult
ListInterfaceAddresses(int aFd, const char* aIface, AddrMapType& aAddrMap);

nsresult
DoListAddresses(AddrMapType& aAddrMap)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return NS_ERROR_FAILURE;
    }

    auto autoCloseSocket = MakeScopeExit([&] {
      close(fd);
    });

    struct ifconf ifconf;
    /* 16k of space should be enough to list all interfaces.  Worst case, if it's
     * not then we will error out and fail to list addresses.  This should only
     * happen on pathological machines with way too many interfaces.
     */
    char buf[16384];

    ifconf.ifc_len = sizeof(buf);
    ifconf.ifc_buf = buf;
    if (ioctl(fd, SIOCGIFCONF, &ifconf) != 0) {
        return NS_ERROR_FAILURE;
    }

    struct ifreq* ifreq = ifconf.ifc_req;
    int i = 0;
    while (i < ifconf.ifc_len) {
        size_t len = IFNAMSIZ + ifreq->ifr_addr.sa_len;

        DebugOnly<nsresult> rv =
          ListInterfaceAddresses(fd, ifreq->ifr_name, aAddrMap);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "ListInterfaceAddresses failed");

        ifreq = (struct ifreq*) ((char*)ifreq + len);
        i += len;
    }

    autoCloseSocket.release();
    return NS_OK;
}

static nsresult
ListInterfaceAddresses(int aFd, const char* aInterface, AddrMapType& aAddrMap)
{
    struct ifreq ifreq;
    memset(&ifreq, 0, sizeof(struct ifreq));
    strncpy(ifreq.ifr_name, aInterface, IFNAMSIZ - 1);
    if (ioctl(aFd, SIOCGIFADDR, &ifreq) != 0) {
        return NS_ERROR_FAILURE;
    }

    char host[128];
    int family;
    switch(family=ifreq.ifr_addr.sa_family) {
      case AF_INET:
      case AF_INET6:
        getnameinfo(&ifreq.ifr_addr, sizeof(ifreq.ifr_addr), host, sizeof(host), nullptr, 0, NI_NUMERICHOST);
        break;
      case AF_UNSPEC:
        return NS_OK;
      default:
        // Unknown family.
        return NS_OK;
    }

    nsCString ifaceStr;
    ifaceStr.AssignASCII(aInterface);

    nsCString addrStr;
    addrStr.AssignASCII(host);

    aAddrMap.Put(ifaceStr, addrStr);

    return NS_OK;
}

} // namespace net
} // namespace mozilla
