/*
Copyright (c) 2007, Adobe Systems, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <csi_platform.h>
#include <assert.h>
#include <string.h>

#ifdef WIN32
#include "addrs-win32.h"
#elif defined(BSD) || defined(DARWIN)
#include "addrs-bsd.h"
#else
#include "addrs-netlink.h"
#endif

#include "stun.h"
#include "addrs.h"
#include "util.h"

static int
nr_stun_is_duplicate_addr(nr_local_addr addrs[], int count, nr_local_addr *addr)
{
    int i;
    int different;

    for (i = 0; i < count; ++i) {
        different = nr_transport_addr_cmp(&addrs[i].addr, &(addr->addr),
          NR_TRANSPORT_ADDR_CMP_MODE_ALL);
        if (!different)
            return 1;  /* duplicate */
    }

    return 0;
}

int
nr_stun_filter_addrs(nr_local_addr addrs[], int remove_loopback, int remove_link_local, int *count)
{
    int r, _status;
    nr_local_addr *tmp = 0;
    int i;
    int n;
    /* We prefer temp ipv6 for their privacy properties. If we cannot get
     * that, we prefer ipv6 that are not based on mac address. */
    int filter_mac_ipv6 = 0;
    int filter_teredo_ipv6 = 0;
    int filter_non_temp_ipv6 = 0;

    tmp = RMALLOC(*count * sizeof(*tmp));
    if (!tmp)
        ABORT(R_NO_MEMORY);

    for (i = 0; i < *count; ++i) {
      if (addrs[i].addr.ip_version == NR_IPV6) {
        if (nr_transport_addr_is_teredo(&addrs[i].addr)) {
            addrs[i].interface.type |= NR_INTERFACE_TYPE_TEREDO;
            /* Prefer teredo over mac-based address. Probably will never see
             * both. */
            filter_mac_ipv6 = 1;
        } else {
          filter_teredo_ipv6 = 1;
        }

        if (!nr_transport_addr_is_mac_based(&addrs[i].addr)) {
          filter_mac_ipv6 = 1;
        }

        if (addrs[i].flags & NR_ADDR_FLAG_TEMPORARY) {
          filter_non_temp_ipv6 = 1;
        }
      }
    }

    n = 0;
    for (i = 0; i < *count; ++i) {
        if (nr_stun_is_duplicate_addr(tmp, n, &addrs[i])) {
            /* skip addrs[i], it's a duplicate */
        }
        else if (remove_loopback && nr_transport_addr_is_loopback(&addrs[i].addr)) {
            /* skip addrs[i], it's a loopback */
        }
        else if (remove_link_local &&
                 nr_transport_addr_is_link_local(&addrs[i].addr)) {
            /* skip addrs[i], it's a link-local address */
        }
        else if (filter_mac_ipv6 &&
                 nr_transport_addr_is_mac_based(&addrs[i].addr)) {
            /* skip addrs[i], it's MAC based */
        }
        else if (filter_teredo_ipv6 &&
                 nr_transport_addr_is_teredo(&addrs[i].addr)) {
            /* skip addrs[i], it's a Teredo address */
        }
        else if (filter_non_temp_ipv6 &&
                 (addrs[i].addr.ip_version == NR_IPV6) &&
                 !(addrs[i].flags & NR_ADDR_FLAG_TEMPORARY)) {
            /* skip addrs[i], it's a non-temporary ipv6, and we have a temporary */
        }
        else {
            /* otherwise, copy it to the temporary array */
            if ((r=nr_local_addr_copy(&tmp[n], &addrs[i])))
                ABORT(r);
            ++n;
        }
    }

    *count = n;

    memset(addrs, 0, *count * sizeof(*addrs));
    /* copy temporary array into passed in/out array */
    for (i = 0; i < *count; ++i) {
        if ((r=nr_local_addr_copy(&addrs[i], &tmp[i])))
            ABORT(r);
    }

    _status = 0;
  abort:
    RFREE(tmp);
    return _status;
}

#ifndef USE_PLATFORM_NR_STUN_GET_ADDRS

int
nr_stun_get_addrs(nr_local_addr addrs[], int maxaddrs, int *count)
{
    int _status=0;
    int i;
    char typestr[100];

    // Ensure output records are always fully defined.  See bug 1589990.
    if (maxaddrs > 0) {
       memset(addrs, 0, maxaddrs * sizeof(nr_local_addr));
    }

    _status = stun_getaddrs_filtered(addrs, maxaddrs, count);

    for (i = 0; i < *count; ++i) {
      nr_local_addr_fmt_info_string(addrs+i,typestr,sizeof(typestr));
      r_log(NR_LOG_STUN, LOG_DEBUG, "Address %d: %s on %s, type: %s\n",
            i,addrs[i].addr.as_string,addrs[i].addr.ifname,typestr);
    }

    return _status;
}

#endif
