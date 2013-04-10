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



#ifndef _stun_msg_h
#define _stun_msg_h

#include "csi_platform.h"
#include "nr_api.h"
#include "transport_addr.h"

#define NR_STUN_MAX_USERNAME_BYTES                 513
#define NR_STUN_MAX_ERROR_CODE_REASON_BYTES        763
#define NR_STUN_MAX_ERROR_CODE_REASON_CHARS        128
#define NR_STUN_MAX_REALM_BYTES                    763
#define NR_STUN_MAX_REALM_CHARS                    128
#define NR_STUN_MAX_NONCE_BYTES                    763
#define NR_STUN_MAX_NONCE_CHARS                    128
#define NR_STUN_MAX_SERVER_BYTES                   763
#define NR_STUN_MAX_SERVER_CHARS                   128
#define NR_STUN_MAX_STRING_SIZE                    763  /* any possible string */
#define NR_STUN_MAX_UNKNOWN_ATTRIBUTES             16
#define NR_STUN_MAX_MESSAGE_SIZE                   2048

#define NR_STUN_MAGIC_COOKIE            0x2112A442
#define NR_STUN_MAGIC_COOKIE2           0xc5cb4e1d  /* used recognize old stun messages */

typedef struct { UCHAR octet[12]; }  UINT12;

typedef struct nr_stun_attr_error_code_ {
    UINT2     number;
    char      reason[NR_STUN_MAX_ERROR_CODE_REASON_BYTES+1];  /* +1 for \0 */
} nr_stun_attr_error_code;

typedef struct nr_stun_attr_fingerprint_ {
    UINT4     checksum;
    int       valid;
} nr_stun_attr_fingerprint;

typedef struct nr_stun_attr_message_integrity_ {
    UCHAR     hash[20];
    int       unknown_user;
    UCHAR     password[1024];
    int       passwordlen;
    int       valid;
} nr_stun_attr_message_integrity;

typedef struct nr_stun_attr_unknown_attributes_ {
    UINT2     attribute[NR_STUN_MAX_UNKNOWN_ATTRIBUTES];
    int       num_attributes;
} nr_stun_attr_unknown_attributes;

typedef struct nr_stun_attr_xor_mapped_address_ {
        nr_transport_addr               masked;
        nr_transport_addr               unmasked;
} nr_stun_attr_xor_mapped_address;

typedef struct nr_stun_attr_data_ {
    UCHAR  data[NR_STUN_MAX_MESSAGE_SIZE];
    int    length;
} nr_stun_attr_data;


typedef struct nr_stun_encoded_attribute_ {
    UINT2                               type;
    UINT2                               length;
    UCHAR                               value[NR_STUN_MAX_MESSAGE_SIZE];
} nr_stun_encoded_attribute;

typedef struct nr_stun_message_attribute_ {
    UINT2                               type;
    UINT2                               length;
    union {
        nr_transport_addr               address;
        nr_transport_addr               alternate_server;
        nr_stun_attr_error_code         error_code;
        nr_stun_attr_fingerprint        fingerprint;
        nr_transport_addr               mapped_address;
        nr_stun_attr_message_integrity  message_integrity;
        char                            nonce[NR_STUN_MAX_NONCE_BYTES+1];  /* +1 for \0 */
        char                            realm[NR_STUN_MAX_REALM_BYTES+1];  /* +1 for \0 */
        nr_stun_attr_xor_mapped_address relay_address;
        char                            server_name[NR_STUN_MAX_SERVER_BYTES+1];  /* +1 for \0 */
        nr_stun_attr_unknown_attributes unknown_attributes;
        char                            username[NR_STUN_MAX_USERNAME_BYTES+1];  /* +1 for \0 */
        nr_stun_attr_xor_mapped_address xor_mapped_address;

#ifdef USE_ICE
        UINT4                           priority;
        UINT8                           ice_controlled;
        UINT8                           ice_controlling;
#endif /* USE_ICE */

#ifdef USE_TURN
        UINT4                           lifetime_secs;
        nr_transport_addr               remote_address;
        UCHAR                           requested_transport;
        nr_stun_attr_data               data;
#endif /* USE_TURN */

#ifdef USE_STUND_0_96
        UINT4                           change_request;
#endif /* USE_STUND_0_96 */

        /* make sure there's enough room here to place any possible
         * attribute */
        UCHAR                           largest_possible_attribute[NR_STUN_MAX_MESSAGE_SIZE];
    } u;
    nr_stun_encoded_attribute          *encoding;
    int                                 encoding_length;
    char                               *name;
    char                               *type_name;
    int                                 invalid;
    TAILQ_ENTRY(nr_stun_message_attribute_)  entry;
} nr_stun_message_attribute;

typedef TAILQ_HEAD(nr_stun_message_attribute_head_,nr_stun_message_attribute_) nr_stun_message_attribute_head;

typedef struct nr_stun_message_header_ {
    UINT2                               type;
    UINT2                               length;
    UINT4                               magic_cookie;
    UINT12                              id;
} nr_stun_message_header;

typedef struct nr_stun_message_ {
    char                               *name;
    UCHAR                               buffer[NR_STUN_MAX_MESSAGE_SIZE];
    int                                 length;
    nr_stun_message_header              header;
    int                                 comprehension_required_unknown_attributes;
    int                                 comprehension_optional_unknown_attributes;
    nr_stun_message_attribute_head      attributes;
} nr_stun_message;

int nr_stun_message_create(nr_stun_message **msg);
int nr_stun_message_create2(nr_stun_message **msg, UCHAR *buffer, int length);
int nr_stun_message_destroy(nr_stun_message **msg);

int nr_stun_message_attribute_create(nr_stun_message *msg, nr_stun_message_attribute **attr);
int nr_stun_message_attribute_destroy(nr_stun_message *msg, nr_stun_message_attribute **attr);

int nr_stun_message_has_attribute(nr_stun_message *msg, UINT2 type, nr_stun_message_attribute **attribute);

int nr_stun_message_add_alternate_server_attribute(nr_stun_message *msg, nr_transport_addr *alternate_server);
int nr_stun_message_add_error_code_attribute(nr_stun_message *msg, UINT2 number, char *reason);
int nr_stun_message_add_fingerprint_attribute(nr_stun_message *msg);
int nr_stun_message_add_message_integrity_attribute(nr_stun_message *msg, Data *password);
int nr_stun_message_add_nonce_attribute(nr_stun_message *msg, char *nonce);
int nr_stun_message_add_realm_attribute(nr_stun_message *msg, char *realm);
int nr_stun_message_add_server_attribute(nr_stun_message *msg, char *server_name);
int nr_stun_message_add_unknown_attributes_attribute(nr_stun_message *msg, nr_stun_attr_unknown_attributes *unknown_attributes);
int nr_stun_message_add_username_attribute(nr_stun_message *msg, char *username);
int nr_stun_message_add_xor_mapped_address_attribute(nr_stun_message *msg, nr_transport_addr *mapped_address);

#ifdef USE_ICE
int nr_stun_message_add_ice_controlled_attribute(nr_stun_message *msg, UINT8 ice_controlled);
int nr_stun_message_add_ice_controlling_attribute(nr_stun_message *msg, UINT8 ice_controlling);
int nr_stun_message_add_priority_attribute(nr_stun_message *msg, UINT4 priority);
int nr_stun_message_add_use_candidate_attribute(nr_stun_message *msg);
#endif /* USE_ICE */

#ifdef USE_TURN
int nr_stun_message_add_data_attribute(nr_stun_message *msg, UCHAR *data, int length);
int nr_stun_message_add_lifetime_attribute(nr_stun_message *msg, UINT4 lifetime_secs);
int nr_stun_message_add_requested_transport_attribute(nr_stun_message *msg, UCHAR transport);
int
nr_stun_message_add_xor_peer_address_attribute(nr_stun_message *msg, nr_transport_addr *peer_address);
#endif /* USE_TURN */

#ifdef USE_STUND_0_96
int nr_stun_message_add_change_request_attribute(nr_stun_message *msg, UINT4 change_request);
#endif /* USE_STUND_0_96 */

#endif

