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


#ifndef _STUN_H
#define _STUN_H

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/param.h>
#include <sys/socket.h>
#ifndef LINUX
#include <net/if.h>
#if !defined(__OpenBSD__) && !defined(__NetBSD__)
#include <net/if_var.h>
#endif
#include <net/if_dl.h>
#include <net/if_types.h>
#else
#include <linux/if.h>
#endif
#ifndef BSD
#include <net/route.h>
#endif
#include <netinet/in.h>
#ifndef LINUX
#include <netinet/in_var.h>
#endif
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <time.h>

#include "nr_api.h"
#include "stun_msg.h"
#include "stun_build.h"
#include "stun_codec.h"
#include "stun_hint.h"
#include "stun_util.h"
#include "nr_socket.h"
#include "stun_client_ctx.h"
#include "stun_server_ctx.h"
#include "stun_proc.h"

#define NR_STUN_VERSION                 "rfc3489bis-11"
#define NR_STUN_PORT                    3478

/* STUN attributes */
#define NR_STUN_ATTR_MAPPED_ADDRESS          0x0001
#define NR_STUN_ATTR_USERNAME                0x0006
#define NR_STUN_ATTR_MESSAGE_INTEGRITY       0x0008
#define NR_STUN_ATTR_ERROR_CODE              0x0009
#define NR_STUN_ATTR_UNKNOWN_ATTRIBUTES      0x000A
#define NR_STUN_ATTR_REALM                   0x0014
#define NR_STUN_ATTR_NONCE                   0x0015
#define NR_STUN_ATTR_XOR_MAPPED_ADDRESS      0x0020
#define NR_STUN_ATTR_SERVER                  0x8022
#define NR_STUN_ATTR_ALTERNATE_SERVER        0x8023
#define NR_STUN_ATTR_FINGERPRINT             0x8028

/* for backwards compatibility with obsolete versions of the STUN spec */
#define NR_STUN_ATTR_OLD_XOR_MAPPED_ADDRESS  0x8020

#ifdef USE_STUND_0_96
#define NR_STUN_ATTR_OLD_CHANGE_REQUEST      0x0003
#endif /* USE_STUND_0_96 */

#ifdef USE_RFC_3489_BACKWARDS_COMPATIBLE
/* for backwards compatibility with obsolete versions of the STUN spec */
#define NR_STUN_ATTR_OLD_PASSWORD            0x0007
#define NR_STUN_ATTR_OLD_RESPONSE_ADDRESS    0x0002
#define NR_STUN_ATTR_OLD_SOURCE_ADDRESS      0x0004
#define NR_STUN_ATTR_OLD_CHANGED_ADDRESS     0x0005
#endif /* USE_RFC_3489_BACKWARDS_COMPATIBLE */

#ifdef USE_ICE
/* ICE attributes */
#define NR_STUN_ATTR_PRIORITY                0x0024
#define NR_STUN_ATTR_USE_CANDIDATE           0x0025
#define NR_STUN_ATTR_ICE_CONTROLLED          0x8029
#define NR_STUN_ATTR_ICE_CONTROLLING         0x802A
#endif /* USE_ICE */

#ifdef USE_TURN
/* TURN attributes */
#define NR_STUN_ATTR_LIFETIME                0x000d
#define NR_STUN_ATTR_XOR_PEER_ADDRESS        0x0012
#define NR_STUN_ATTR_DATA                    0x0013
#define NR_STUN_ATTR_XOR_RELAY_ADDRESS       0x0016
#define NR_STUN_ATTR_REQUESTED_TRANSPORT     0x0019

#define NR_STUN_ATTR_REQUESTED_TRANSPORT_UDP     17
#endif /* USE_TURN */

/*
 *                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                |M|M|M|M|M|C|M|M|M|C|M|M|M|M|
 *                |1|1|9|8|7|1|6|5|4|0|3|2|1|0|
 *                |1|0| | | | | | | | | | | | |
 *                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *      Figure 3: Format of STUN Message Type Field
 */
#define NR_STUN_METHOD_TYPE_BITS(m) \
        ((((m) & 0xf80) << 2) | (((m) & 0x070) << 1) | ((m) & 0x00f))

#define NR_STUN_CLASS_TYPE_BITS(c) \
        ((((c) & 0x002) << 7) | (((c) & 0x001) << 4))

#define NR_STUN_GET_TYPE_METHOD(t) \
        ((((t) >> 2) & 0xf80) | (((t) >> 1) & 0x070) | ((t) & 0x00f))

#define NR_STUN_GET_TYPE_CLASS(t) \
        ((((t) >> 7) & 0x002) | (((t) >> 4) & 0x001))

#define NR_STUN_TYPE(m,c)  (NR_STUN_METHOD_TYPE_BITS((m)) | NR_STUN_CLASS_TYPE_BITS((c)))

/* building blocks for message types */
#define NR_METHOD_BINDING          0x001
#define NR_CLASS_REQUEST           0x0
#define NR_CLASS_INDICATION        0x1
#define NR_CLASS_RESPONSE          0x2
#define NR_CLASS_ERROR_RESPONSE    0x3

/* define types for STUN messages */
#define NR_STUN_MSG_BINDING_REQUEST                 NR_STUN_TYPE(NR_METHOD_BINDING, \
                                                                 NR_CLASS_REQUEST)
#define NR_STUN_MSG_BINDING_INDICATION              NR_STUN_TYPE(NR_METHOD_BINDING, \
                                                                 NR_CLASS_INDICATION)
#define NR_STUN_MSG_BINDING_RESPONSE                NR_STUN_TYPE(NR_METHOD_BINDING, \
                                                                 NR_CLASS_RESPONSE)
#define NR_STUN_MSG_BINDING_ERROR_RESPONSE          NR_STUN_TYPE(NR_METHOD_BINDING, \
                                                                 NR_CLASS_ERROR_RESPONSE)

#ifdef USE_TURN
/* building blocks for TURN message types */
#define NR_METHOD_ALLOCATE                 0x003
#define NR_METHOD_REFRESH                  0x004

#define NR_METHOD_SEND                     0x006
#define NR_METHOD_DATA                     0x007
#define NR_METHOD_CREATE_PERMISSION        0x008
#define NR_METHOD_CHANNEL_BIND             0x009

/* define types for a TURN message */
#define NR_STUN_MSG_ALLOCATE_REQUEST                NR_STUN_TYPE(NR_METHOD_ALLOCATE, \
                                                                 NR_CLASS_REQUEST)
#define NR_STUN_MSG_ALLOCATE_RESPONSE               NR_STUN_TYPE(NR_METHOD_ALLOCATE, \
                                                                 NR_CLASS_RESPONSE)
#define NR_STUN_MSG_ALLOCATE_ERROR_RESPONSE         NR_STUN_TYPE(NR_METHOD_ALLOCATE, \
                                                                 NR_CLASS_ERROR_RESPONSE)
#define NR_STUN_MSG_REFRESH_REQUEST                 NR_STUN_TYPE(NR_METHOD_REFRESH, \
                                                                 NR_CLASS_REQUEST)
#define NR_STUN_MSG_REFRESH_RESPONSE                NR_STUN_TYPE(NR_METHOD_REFRESH, \
                                                                 NR_CLASS_RESPONSE)
#define NR_STUN_MSG_REFRESH_ERROR_RESPONSE          NR_STUN_TYPE(NR_METHOD_REFRESH, \
                                                                 NR_CLASS_ERROR_RESPONSE)

#define NR_STUN_MSG_SEND_INDICATION                 NR_STUN_TYPE(NR_METHOD_SEND, \
                                                                 NR_CLASS_INDICATION)
#define NR_STUN_MSG_DATA_INDICATION                 NR_STUN_TYPE(NR_METHOD_DATA, \
                                                                 NR_CLASS_INDICATION)

#define NR_STUN_MSG_PERMISSION_REQUEST                 NR_STUN_TYPE(NR_METHOD_CREATE_PERMISSION, \
                                                                    NR_CLASS_REQUEST)
#define NR_STUN_MSG_PERMISSION_RESPONSE                NR_STUN_TYPE(NR_METHOD_CREATE_PERMISSION, \
                                                                    NR_CLASS_RESPONSE)
#define NR_STUN_MSG_PERMISSION_ERROR_RESPONSE          NR_STUN_TYPE(NR_METHOD_CREATE_PERMISSION, \
                                                                    NR_CLASS_ERROR_RESPONSE)

#define NR_STUN_MSG_CHANNEL_BIND_REQUEST                 NR_STUN_TYPE(NR_METHOD_CHANNEL_BIND, \
                                                                      NR_CLASS_REQUEST)
#define NR_STUN_MSG_CHANNEL_BIND_RESPONSE                NR_STUN_TYPE(NR_METHOD_CHANNEL_BIND, \
                                                                      NR_CLASS_RESPONSE)
#define NR_STUN_MSG_CHANNEL_BIND_ERROR_RESPONSE          NR_STUN_TYPE(NR_METHOD_CHANNEL_BIND, \
                                                                      NR_CLASS_ERROR_RESPONSE)


#endif /* USE_TURN */


#define NR_STUN_AUTH_RULE_OPTIONAL      (1<<0)
#define NR_STUN_AUTH_RULE_SHORT_TERM    (1<<8)
#define NR_STUN_AUTH_RULE_LONG_TERM     (1<<9)

#endif
