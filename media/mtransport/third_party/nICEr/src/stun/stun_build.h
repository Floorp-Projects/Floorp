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


#ifndef _stun_build_h
#define _stun_build_h

#include "stun.h"

#define NR_STUN_MODE_STUN               1
#ifdef USE_STUND_0_96
#define NR_STUN_MODE_STUND_0_96         2    /* backwards compatibility mode */
#endif /* USE_STUND_0_96 */
int nr_stun_form_request_or_indication(int mode, int msg_type, nr_stun_message **msg);

typedef struct nr_stun_client_stun_binding_request_params_ {
    char *username;
    Data *password;
    char *nonce;
    char *realm;
} nr_stun_client_stun_binding_request_params;

int nr_stun_build_req_lt_auth(nr_stun_client_stun_binding_request_params *params, nr_stun_message **msg);
int nr_stun_build_req_st_auth(nr_stun_client_stun_binding_request_params *params, nr_stun_message **msg);
int nr_stun_build_req_no_auth(nr_stun_client_stun_binding_request_params *params, nr_stun_message **msg);


typedef struct nr_stun_client_stun_keepalive_params_ {
#ifdef WIN32  // silly VC++ gives error if no members
    int dummy;
#endif
} nr_stun_client_stun_keepalive_params;

int nr_stun_build_keepalive(nr_stun_client_stun_keepalive_params *params, nr_stun_message **msg);


#ifdef USE_STUND_0_96
typedef struct nr_stun_client_stun_binding_request_stund_0_96_params_ {
#ifdef WIN32  // silly VC++ gives error if no members
    int dummy;
#endif
} nr_stun_client_stun_binding_request_stund_0_96_params;

int nr_stun_build_req_stund_0_96(nr_stun_client_stun_binding_request_stund_0_96_params *params, nr_stun_message **msg);
#endif /* USE_STUND_0_96 */


#ifdef USE_ICE
typedef struct nr_stun_client_ice_use_candidate_params_ {
    char *username;
    Data password;
    UINT4 priority;
    UINT8 tiebreaker;
} nr_stun_client_ice_use_candidate_params;

int nr_stun_build_use_candidate(nr_stun_client_ice_use_candidate_params *params, nr_stun_message **msg);


typedef struct nr_stun_client_ice_binding_request_params_ {
    char *username;
    Data password;
    UINT4 priority;
    int control;
#define NR_ICE_CONTROLLING  1
#define NR_ICE_CONTROLLED   2
    UINT8 tiebreaker;
} nr_stun_client_ice_binding_request_params;

int nr_stun_build_req_ice(nr_stun_client_ice_binding_request_params *params, nr_stun_message **msg);
#endif /* USE_ICE */


typedef struct nr_stun_client_auth_params_ {
    char authenticate;
    char *username;
    char *realm;
    char *nonce;
    Data password;
} nr_stun_client_auth_params;

#ifdef USE_TURN
typedef struct nr_stun_client_allocate_request_params_ {
    UINT4 lifetime_secs;
} nr_stun_client_allocate_request_params;

int nr_stun_build_allocate_request(nr_stun_client_auth_params *auth, nr_stun_client_allocate_request_params *params, nr_stun_message **msg);


typedef struct nr_stun_client_refresh_request_params_ {
    UINT4 lifetime_secs;
} nr_stun_client_refresh_request_params;

int nr_stun_build_refresh_request(nr_stun_client_auth_params *auth, nr_stun_client_refresh_request_params *params, nr_stun_message **msg);



typedef struct nr_stun_client_permission_request_params_ {
    nr_transport_addr remote_addr;
} nr_stun_client_permission_request_params;

int nr_stun_build_permission_request(nr_stun_client_auth_params *auth, nr_stun_client_permission_request_params *params, nr_stun_message **msg);


typedef struct nr_stun_client_send_indication_params_ {
    nr_transport_addr remote_addr;
    Data data;
} nr_stun_client_send_indication_params;

int nr_stun_build_send_indication(nr_stun_client_send_indication_params *params, nr_stun_message **msg);

typedef struct nr_stun_client_data_indication_params_ {
    nr_transport_addr remote_addr;
    Data data;
} nr_stun_client_data_indication_params;

int nr_stun_build_data_indication(nr_stun_client_data_indication_params *params, nr_stun_message **msg);
#endif /* USE_TURN */

int nr_stun_form_success_response(nr_stun_message *req, nr_transport_addr *from, Data *password, nr_stun_message *res);
void nr_stun_form_error_response(nr_stun_message *request, nr_stun_message* response, int number, char* msg);
int nr_stun_compute_lt_message_integrity_password(const char *username, const char *realm,
                                                  Data *password, Data *hmac_key);

#endif
