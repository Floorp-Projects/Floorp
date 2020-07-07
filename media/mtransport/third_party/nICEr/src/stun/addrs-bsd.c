/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(BSD) || defined(DARWIN)
#include "addrs-bsd.h"
#include <csi_platform.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "stun_util.h"
#include "util.h"
#include <r_macros.h>

#include <sys/types.h> /* getifaddrs */
#include <ifaddrs.h> /* getifaddrs */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>

static int
stun_ifaddr_get_v6_flags(struct ifaddrs *ifaddr)
{
  if (ifaddr->ifa_addr->sa_family != AF_INET6) {
    return 0;
  }

  int flags = 0;
  int s = socket(AF_INET6, SOCK_DGRAM, 0);
  if (!s) {
    r_log(NR_LOG_STUN, LOG_ERR, "socket(AF_INET6, SOCK_DGRAM, 0) failed, errno=%d", errno);
    assert(0);
    return 0;
  }
  struct in6_ifreq ifr6;
  memset(&ifr6, 0, sizeof(ifr6));
  strncpy(ifr6.ifr_name, ifaddr->ifa_name, sizeof(ifr6.ifr_name));
  /* ifr_addr is a sockaddr_in6, ifa_addr is a sockaddr* */
  struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)ifaddr->ifa_addr;
  ifr6.ifr_addr = *sin6;
  if (ioctl(s, SIOCGIFAFLAG_IN6, &ifr6) != -1) {
    flags = ifr6.ifr_ifru.ifru_flags6;
  } else {
    r_log(NR_LOG_STUN, LOG_ERR, "ioctl(SIOCGIFAFLAG_IN6) failed, errno=%d", errno);
    assert(0);
  }
  close(s);
  return flags;
}

static int
stun_ifaddr_is_disallowed_v6(int flags) {
  return flags & (IN6_IFF_ANYCAST | IN6_IFF_TENTATIVE | IN6_IFF_DUPLICATED | IN6_IFF_DETACHED | IN6_IFF_DEPRECATED);
}

int stun_getaddrs_filtered(nr_local_addr addrs[], int maxaddrs, int *count)
{
  int r,_status,flags;
  struct ifaddrs* if_addrs_head=NULL;
  struct ifaddrs* if_addr;

  *count = 0;

  if (maxaddrs <= 0)
    ABORT(R_BAD_ARGS);

  if (getifaddrs(&if_addrs_head) == -1) {
    r_log(NR_LOG_STUN, LOG_ERR, "getifaddrs error e = %d", errno);
    ABORT(R_INTERNAL);
  }

  if_addr = if_addrs_head;

  while (if_addr && *count < maxaddrs) {
    /* This can be null */
    if (if_addr->ifa_addr) {
      switch (if_addr->ifa_addr->sa_family) {
        case AF_INET:
        case AF_INET6:
          flags = stun_ifaddr_get_v6_flags(if_addr);
          if (!stun_ifaddr_is_disallowed_v6(flags)) {
            if (r=nr_sockaddr_to_transport_addr(if_addr->ifa_addr, IPPROTO_UDP, 0, &(addrs[*count].addr))) {
              r_log(NR_LOG_STUN, LOG_ERR, "nr_sockaddr_to_transport_addr error r = %d", r);
            } else {
              if (flags & IN6_IFF_TEMPORARY) {
                addrs[*count].flags |= NR_ADDR_FLAG_TEMPORARY;
              }
              (void)strlcpy(addrs[*count].addr.ifname, if_addr->ifa_name, sizeof(addrs[*count].addr.ifname));
              ++(*count);
            }
          }
          break;
        default:
          ;
      }
    }

    if_addr = if_addr->ifa_next;
  }

  _status=0;
abort:
  if (if_addrs_head) {
    freeifaddrs(if_addrs_head);
  }
  return(_status);
}
#endif
