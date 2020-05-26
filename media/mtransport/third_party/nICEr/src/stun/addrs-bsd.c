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

int stun_getaddrs_filtered(nr_local_addr addrs[], int maxaddrs, int *count)
{
  int r,_status;
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
          if (r=nr_sockaddr_to_transport_addr(if_addr->ifa_addr, IPPROTO_UDP, 0, &(addrs[*count].addr))) {
            r_log(NR_LOG_STUN, LOG_ERR, "nr_sockaddr_to_transport_addr error r = %d", r);
          } else {
            (void)strlcpy(addrs[*count].addr.ifname, if_addr->ifa_name, sizeof(addrs[*count].addr.ifname));
            ++(*count);
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
