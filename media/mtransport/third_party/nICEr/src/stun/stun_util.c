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


static char *RCSSTRING __UNUSED__="$Id: stun_util.c,v 1.2 2008/04/28 18:21:30 ekr Exp $";

#include <errno.h>
#include <csi_platform.h>

#ifdef WIN32
#include <winsock2.h>
#include <stdlib.h>
#include <io.h>
#include <time.h>
#else   /* UNIX */
#include <string.h>
#endif  /* end UNIX */
#include <assert.h>

#include "stun.h"
#include "stun_reg.h"
#include "registry.h"
#include "addrs.h"
#include "transport_addr_reg.h"
#include "nr_crypto.h"
#include "hex.h"


int NR_LOG_STUN = 0;

int
nr_stun_startup(void)
{
   int r,_status;

    if ((r=r_log_register("stun", &NR_LOG_STUN)))
      ABORT(r);

   _status=0;
 abort:
   return _status;
}

int
nr_stun_xor_mapped_address(UINT4 magicCookie, UINT12 transactionId, nr_transport_addr *from, nr_transport_addr *to)
{
    int _status;

    switch (from->ip_version) {
    case NR_IPV4:
        nr_ip4_port_to_transport_addr(
            (ntohl(from->u.addr4.sin_addr.s_addr) ^ magicCookie),
            (ntohs(from->u.addr4.sin_port) ^ (magicCookie>>16)),
            from->protocol, to);
        break;
    case NR_IPV6:
        {
          union {
            unsigned char addr[16];
            UINT4 addr32[4];
          } maskedAddr;

          maskedAddr.addr32[0] = htonl(magicCookie); /* Passed in host byte order */
          memcpy(&maskedAddr.addr32[1], transactionId.octet, sizeof(transactionId));

          /* We now have the mask in network byte order */
          /* Xor the address in network byte order */
          for (size_t i = 0; i < sizeof(maskedAddr); ++i) {
            maskedAddr.addr[i] ^= from->u.addr6.sin6_addr.s6_addr[i];
          }

          nr_ip6_port_to_transport_addr(
              (struct in6_addr*)&maskedAddr,
              (ntohs(from->u.addr6.sin6_port) ^ (magicCookie>>16)),
              from->protocol, to);
        }
        break;
    default:
        assert(0);
        ABORT(R_INTERNAL);
        break;
    }

    _status = 0;
  abort:
    return _status;
}

int
nr_stun_filter_local_addresses(nr_local_addr addrs[], int *count)
{
    int r,_status;
    char allow_loopback = 0;
    char allow_link_local = 0;

    if ((r=NR_reg_get_char(NR_STUN_REG_PREF_ALLOW_LOOPBACK_ADDRS,
                           &allow_loopback))) {
        if (r != R_NOT_FOUND) {
            ABORT(r);
        }
    }

    if ((r=NR_reg_get_char(NR_STUN_REG_PREF_ALLOW_LINK_LOCAL_ADDRS,
                           &allow_link_local))) {
        if (r != R_NOT_FOUND) {
            ABORT(r);
        }
    }

    if ((r=nr_stun_remove_duplicate_addrs(addrs,
                                          !allow_loopback,
                                          !allow_link_local,
                                          count))) {
        ABORT(r);
    }

    _status=0;
 abort:
    return _status;
}

int
nr_stun_find_local_addresses(nr_local_addr addrs[], int maxaddrs, int *count)
{
    int r,_status;
    //NR_registry *children = 0;

    *count = 0;

#if 0
    // this really goes with the code commented out below. (mjf)
    if ((r=NR_reg_get_child_count(NR_STUN_REG_PREF_ADDRESS_PRFX, (unsigned int*)count)))
        if (r != R_NOT_FOUND)
            ABORT(r);
#endif

    if (*count == 0) {
        if ((r=nr_stun_get_addrs(addrs, maxaddrs, count)))
            ABORT(r);

        goto done;
    }

    if (*count >= maxaddrs) {
        r_log(NR_LOG_STUN, LOG_INFO, "Address list truncated from %d to %d", *count, maxaddrs);
       *count = maxaddrs;
    }

#if 0
    if (*count > 0) {
      /* TODO(ekr@rtfm.com): Commented out 2012-07-26.

         This code is currently not used in Firefox and needs to be
         ported to 64-bit */
        children = RCALLOC((*count + 10) * sizeof(*children));
        if (!children)
            ABORT(R_NO_MEMORY);

        assert(sizeof(size_t) == sizeof(*count));

        if ((r=NR_reg_get_children(NR_STUN_REG_PREF_ADDRESS_PRFX, children, (size_t)(*count + 10), (size_t*)count)))
            ABORT(r);

        for (i = 0; i < *count; ++i) {
            if ((r=nr_reg_get_transport_addr(children[i], 0, &addrs[i].addr)))
                ABORT(r);
        }
    }
#endif

  done:

     _status=0;
 abort:
     //RFREE(children);
     return _status;
}

int
nr_stun_different_transaction(UCHAR *msg, size_t len, nr_stun_message *req)
{
    int _status;
    nr_stun_message_header header;
    char reqid[44];
    char msgid[44];
    size_t unused;

    if (sizeof(header) > len)
        ABORT(R_FAILED);

    assert(sizeof(header.id) == sizeof(UINT12));

    memcpy(&header, msg, sizeof(header));

    if (memcmp(&req->header.id, &header.id, sizeof(header.id))) {
        nr_nbin2hex((UCHAR*)&req->header.id, sizeof(req->header.id), reqid, sizeof(reqid), &unused);
        nr_nbin2hex((UCHAR*)&header.id, sizeof(header.id), msgid, sizeof(msgid), &unused);
        r_log(NR_LOG_STUN, LOG_DEBUG, "Mismatched message IDs %s/%s", reqid, msgid);
        ABORT(R_NOT_FOUND);
    }

   _status=0;
 abort:
   return _status;
}

char*
nr_stun_msg_type(int type)
{
    char *ret = 0;

    switch (type) {
    case NR_STUN_MSG_BINDING_REQUEST:
         ret = "BINDING-REQUEST";
         break;
    case NR_STUN_MSG_BINDING_INDICATION:
         ret = "BINDING-INDICATION";
         break;
    case NR_STUN_MSG_BINDING_RESPONSE:
         ret = "BINDING-RESPONSE";
         break;
    case NR_STUN_MSG_BINDING_ERROR_RESPONSE:
         ret = "BINDING-ERROR-RESPONSE";
         break;

#ifdef USE_TURN
    case NR_STUN_MSG_ALLOCATE_REQUEST:
         ret = "ALLOCATE-REQUEST";
         break;
    case NR_STUN_MSG_ALLOCATE_RESPONSE:
         ret = "ALLOCATE-RESPONSE";
         break;
    case NR_STUN_MSG_ALLOCATE_ERROR_RESPONSE:
         ret = "ALLOCATE-ERROR-RESPONSE";
         break;
    case NR_STUN_MSG_REFRESH_REQUEST:
         ret = "REFRESH-REQUEST";
         break;
    case NR_STUN_MSG_REFRESH_RESPONSE:
         ret = "REFRESH-RESPONSE";
         break;
    case NR_STUN_MSG_REFRESH_ERROR_RESPONSE:
         ret = "REFRESH-ERROR-RESPONSE";
         break;
    case NR_STUN_MSG_SEND_INDICATION:
         ret = "SEND-INDICATION";
         break;
    case NR_STUN_MSG_DATA_INDICATION:
         ret = "DATA-INDICATION";
         break;
    case NR_STUN_MSG_PERMISSION_REQUEST:
         ret = "PERMISSION-REQUEST";
         break;
    case NR_STUN_MSG_PERMISSION_RESPONSE:
         ret = "PERMISSION-RESPONSE";
         break;
    case NR_STUN_MSG_PERMISSION_ERROR_RESPONSE:
         ret = "PERMISSION-ERROR-RESPONSE";
         break;
#endif /* USE_TURN */

    default:
         /* ret remains 0 */
         break;
    }

    return ret;
}

int
nr_random_alphanum(char *alphanum, int size)
{
    static char alphanums[256] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
        'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
        'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
        'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
        'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
        'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
        'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
        'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
        'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
        'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
        'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
        'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
        'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
        'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
        'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
        'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
        'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H' };
    int i;

    nr_crypto_random_bytes((UCHAR*)alphanum, size);

    /* now convert from binary to alphanumeric */
    for (i = 0; i < size; ++i)
        alphanum[i] = alphanums[(UCHAR)alphanum[i]];

    return 0;
}

#ifndef UINT2_MAX
#define UINT2_MAX ((UINT2)(65535U))
#endif

void nr_accumulate_count(UINT2* orig_count, UINT2 add_count)
  {
    if (UINT2_MAX - add_count < *orig_count) {
      // don't rollover, just stop accumulating at MAX value
      *orig_count = UINT2_MAX;
    } else {
      *orig_count += add_count;
    }
  }
