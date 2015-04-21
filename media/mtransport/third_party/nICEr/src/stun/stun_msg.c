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


static char *RCSSTRING __UNUSED__="$Id: stun_msg.c,v 1.2 2008/04/28 18:21:30 ekr Exp $";

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


int
nr_stun_message_create(nr_stun_message **msg)
{
    int _status;
    nr_stun_message *m = 0;

    m = RCALLOC(sizeof(*m));
    if (!m)
        ABORT(R_NO_MEMORY);

    TAILQ_INIT(&m->attributes);

    *msg = m;

    _status=0;
  abort:
    return(_status);
}

int
nr_stun_message_create2(nr_stun_message **msg, UCHAR *buffer, int length)
{
    int r,_status;
    nr_stun_message *m = 0;

    if (length > sizeof(m->buffer)) {
        ABORT(R_BAD_DATA);
    }

    if ((r=nr_stun_message_create(&m)))
        ABORT(r);

    memcpy(m->buffer, buffer, length);
    m->length = length;

    *msg = m;

    _status=0;
  abort:
    return(_status);
}

int
nr_stun_message_destroy(nr_stun_message **msg)
{
    int _status;
    nr_stun_message_attribute_head *attrs;
    nr_stun_message_attribute *attr;

    if (msg && *msg) {
        attrs = &(*msg)->attributes;
        while (!TAILQ_EMPTY(attrs)) {
            attr = TAILQ_FIRST(attrs);
            nr_stun_message_attribute_destroy(*msg, &attr);
        }

        RFREE(*msg);

        *msg = 0;
    }

    _status=0;
/*  abort: */
    return(_status);
}

int
nr_stun_message_attribute_create(nr_stun_message *msg, nr_stun_message_attribute **attr)
{
    int _status;
    nr_stun_message_attribute *a = 0;

    a = RCALLOC(sizeof(*a));
    if (!a)
        ABORT(R_NO_MEMORY);

    TAILQ_INSERT_TAIL(&msg->attributes, a, entry);

    *attr = a;

    _status=0;
  abort:
    return(_status);
}

int
nr_stun_message_attribute_destroy(nr_stun_message *msg, nr_stun_message_attribute **attr)
{
    int _status;
    nr_stun_message_attribute *a = 0;

    if (attr && *attr) {
        a = *attr;
        TAILQ_REMOVE(&msg->attributes, a, entry);

        RFREE(a);

        *attr = 0;
    }

    _status=0;
/*  abort: */
    return(_status);
}

int
nr_stun_message_has_attribute(nr_stun_message *msg, UINT2 type, nr_stun_message_attribute **attribute)
{
    nr_stun_message_attribute *attr = 0;

    if (attribute)
        *attribute = 0;

    TAILQ_FOREACH(attr, &msg->attributes, entry) {
        if (attr->type == type)
            break;
    }

    if (!attr || attr->invalid)
        return 0;  /* does not have */

    if (attribute)
        *attribute = attr;

    return 1;  /* has */
}

#define NR_STUN_MESSAGE_ADD_ATTRIBUTE(__type, __code) \
  { \
    int r,_status; \
    nr_stun_message_attribute *attr = 0; \
    if ((r=nr_stun_message_attribute_create(msg, &attr))) \
        ABORT(r); \
    attr->type = (__type); \
    { __code } \
    _status=0; \
  abort: \
    if (_status){ \
      nr_stun_message_attribute_destroy(msg, &attr); \
    } \
    return(_status); \
  }


int
nr_stun_message_add_alternate_server_attribute(nr_stun_message *msg, nr_transport_addr *alternate_server)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_ALTERNATE_SERVER,
    {
        if ((r=nr_transport_addr_copy(&attr->u.alternate_server, alternate_server)))
            ABORT(r);
    }
)

int
nr_stun_message_add_error_code_attribute(nr_stun_message *msg, UINT2 number, char *reason)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_ERROR_CODE,
    {
        attr->u.error_code.number = number;
        strlcpy(attr->u.error_code.reason, reason, sizeof(attr->u.error_code.reason));
    }
)

int
nr_stun_message_add_fingerprint_attribute(nr_stun_message *msg)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_FINGERPRINT,
    {}
)

int
nr_stun_message_add_message_integrity_attribute(nr_stun_message *msg, Data *password)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_MESSAGE_INTEGRITY,
    {
        if (sizeof(attr->u.message_integrity.password) < password->len)
            ABORT(R_BAD_DATA);

        memcpy(attr->u.message_integrity.password, password->data, password->len);
        attr->u.message_integrity.passwordlen = password->len;
    }
)

int
nr_stun_message_add_nonce_attribute(nr_stun_message *msg, char *nonce)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_NONCE,
    { strlcpy(attr->u.nonce, nonce, sizeof(attr->u.nonce)); }
)

int
nr_stun_message_add_realm_attribute(nr_stun_message *msg, char *realm)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_REALM,
    { strlcpy(attr->u.realm, realm, sizeof(attr->u.realm)); }
)

int
nr_stun_message_add_server_attribute(nr_stun_message *msg, char *server_name)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_SERVER,
    { strlcpy(attr->u.server_name, server_name, sizeof(attr->u.server_name)); }
)

int
nr_stun_message_add_unknown_attributes_attribute(nr_stun_message *msg, nr_stun_attr_unknown_attributes *unknown_attributes)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_UNKNOWN_ATTRIBUTES,
    { memcpy(&attr->u.unknown_attributes, unknown_attributes, sizeof(attr->u.unknown_attributes)); }
)

int
nr_stun_message_add_username_attribute(nr_stun_message *msg, char *username)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_USERNAME,
    { strlcpy(attr->u.username, username, sizeof(attr->u.username)); }
)

int
nr_stun_message_add_requested_transport_attribute(nr_stun_message *msg, UCHAR protocol)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_REQUESTED_TRANSPORT,
    { attr->u.requested_transport = protocol; }
)

int
nr_stun_message_add_xor_mapped_address_attribute(nr_stun_message *msg, nr_transport_addr *mapped_address)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_XOR_MAPPED_ADDRESS,
    {
        if ((r=nr_transport_addr_copy(&attr->u.xor_mapped_address.unmasked, mapped_address)))
            ABORT(r);
    }
)

int
nr_stun_message_add_xor_peer_address_attribute(nr_stun_message *msg, nr_transport_addr *peer_address)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_XOR_PEER_ADDRESS,
    {
        if ((r=nr_transport_addr_copy(&attr->u.xor_mapped_address.unmasked, peer_address)))
            ABORT(r);
    }
)

#ifdef USE_ICE
int
nr_stun_message_add_ice_controlled_attribute(nr_stun_message *msg, UINT8 ice_controlled)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_ICE_CONTROLLED,
    { attr->u.ice_controlled = ice_controlled; }
)

int
nr_stun_message_add_ice_controlling_attribute(nr_stun_message *msg, UINT8 ice_controlling)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_ICE_CONTROLLING,
    { attr->u.ice_controlling = ice_controlling; }
)

int
nr_stun_message_add_priority_attribute(nr_stun_message *msg, UINT4 priority)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_PRIORITY,
    { attr->u.priority = priority; }
)

int
nr_stun_message_add_use_candidate_attribute(nr_stun_message *msg)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_USE_CANDIDATE,
    {}
)
#endif /* USE_ICE */

#ifdef USE_TURN
int
nr_stun_message_add_data_attribute(nr_stun_message *msg, UCHAR *data, int length)

NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_DATA,
    {
      if (length > NR_STUN_MAX_MESSAGE_SIZE)
        ABORT(R_BAD_ARGS);

      memcpy(attr->u.data.data, data, length);
      attr->u.data.length=length;
    }
)

int
nr_stun_message_add_lifetime_attribute(nr_stun_message *msg, UINT4 lifetime_secs)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_LIFETIME,
    { attr->u.lifetime_secs = lifetime_secs; }
)

#endif /* USE_TURN */

#ifdef USE_STUND_0_96
int
nr_stun_message_add_change_request_attribute(nr_stun_message *msg, UINT4 change_request)
NR_STUN_MESSAGE_ADD_ATTRIBUTE(
    NR_STUN_ATTR_OLD_CHANGE_REQUEST,
    { attr->u.change_request = change_request; }
)
#endif /* USE_STUND_0_96 */

