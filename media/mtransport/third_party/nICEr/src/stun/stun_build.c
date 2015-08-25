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


static char *RCSSTRING __UNUSED__="$Id: stun_build.c,v 1.2 2008/04/28 18:21:30 ekr Exp $";

#include <csi_platform.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "nr_api.h"
#include "stun.h"
#include "registry.h"
#include "stun_reg.h"
#include "nr_crypto.h"


/* draft-ietf-behave-rfc3489bis-10.txt S 7.1 */
/* draft-ietf-behave-rfc3489bis-10.txt S 10.1.1 */
/* note that S 10.1.1 states the message MUST include MESSAGE-INTEGRITY
 * and USERNAME, but that's not correct -- for instance ICE keepalive
 * messages don't include these (See draft-ietf-mmusic-ice-18.txt S 10:
 * "If STUN is being used for keepalives, a STUN Binding Indication is
 * used.  The Indication MUST NOT utilize any authentication mechanism")
 */
int
nr_stun_form_request_or_indication(int mode, int msg_type, nr_stun_message **msg)
{
   int r,_status;
   nr_stun_message *req = 0;

   assert(NR_STUN_GET_TYPE_CLASS(msg_type) == NR_CLASS_REQUEST
       || NR_STUN_GET_TYPE_CLASS(msg_type) == NR_CLASS_INDICATION);

   *msg = 0;

   if ((r=nr_stun_message_create(&req)))
       ABORT(r);

   req->header.type = msg_type;

   nr_crypto_random_bytes((UCHAR*)&req->header.id,sizeof(req->header.id));

   switch (mode) {
   default:
       req->header.magic_cookie = NR_STUN_MAGIC_COOKIE;

       if ((r=nr_stun_message_add_fingerprint_attribute(req)))
           ABORT(r);

       break;

#ifdef USE_STUND_0_96
   case NR_STUN_MODE_STUND_0_96:
       req->header.magic_cookie = NR_STUN_MAGIC_COOKIE2;

       /* actually, stund 0.96 just ignores the fingerprint
        * attribute, but don't bother to send it */

       break;
#endif /* USE_STUND_0_96 */

   }

   *msg = req;

   _status=0;
 abort:
   if (_status) RFREE(req);
   return _status;
}

int
nr_stun_build_req_lt_auth(nr_stun_client_stun_binding_request_params *params, nr_stun_message **msg)
{
   int r,_status;
   nr_stun_message *req = 0;

   if ((r=nr_stun_form_request_or_indication(NR_STUN_MODE_STUN, NR_STUN_MSG_BINDING_REQUEST, &req)))
       ABORT(r);

   if ((r=nr_stun_message_add_username_attribute(req, params->username)))
       ABORT(r);

   if (params->realm && params->nonce) {
       if ((r=nr_stun_message_add_realm_attribute(req, params->realm)))
           ABORT(r);

       if ((r=nr_stun_message_add_nonce_attribute(req, params->nonce)))
           ABORT(r);

       if ((r=nr_stun_message_add_message_integrity_attribute(req, params->password)))
           ABORT(r);
   }

   *msg = req;

   _status=0;
 abort:
   if (_status) nr_stun_message_destroy(&req);
   return _status;
}

int
nr_stun_build_req_st_auth(nr_stun_client_stun_binding_request_params *params, nr_stun_message **msg)
{
   int r,_status;
   nr_stun_message *req = 0;

   if ((r=nr_stun_form_request_or_indication(NR_STUN_MODE_STUN, NR_STUN_MSG_BINDING_REQUEST, &req)))
       ABORT(r);

   if ((r=nr_stun_message_add_username_attribute(req, params->username)))
       ABORT(r);

   if (params->password) {
       if ((r=nr_stun_message_add_message_integrity_attribute(req, params->password)))
           ABORT(r);
   }

   *msg = req;

   _status=0;
 abort:
   if (_status) nr_stun_message_destroy(&req);
   return _status;
}

int
nr_stun_build_req_no_auth(nr_stun_client_stun_binding_request_params *params, nr_stun_message **msg)
{
   int r,_status;
   nr_stun_message *req = 0;

   if ((r=nr_stun_form_request_or_indication(NR_STUN_MODE_STUN, NR_STUN_MSG_BINDING_REQUEST, &req)))
       ABORT(r);

   *msg = req;

   _status=0;
 abort:
   if (_status) nr_stun_message_destroy(&req);
   return _status;
}

int
nr_stun_build_keepalive(nr_stun_client_stun_keepalive_params *params, nr_stun_message **msg)
{
   int r,_status;
   nr_stun_message *ind = 0;

   if ((r=nr_stun_form_request_or_indication(NR_STUN_MODE_STUN, NR_STUN_MSG_BINDING_INDICATION, &ind)))
       ABORT(r);

   *msg = ind;

   _status=0;
 abort:
   if (_status) nr_stun_message_destroy(&ind);
   return _status;
}

#ifdef USE_STUND_0_96
int
nr_stun_build_req_stund_0_96(nr_stun_client_stun_binding_request_stund_0_96_params *params, nr_stun_message **msg)
{
    int r,_status;
    nr_stun_message *req = 0;

    if ((r=nr_stun_form_request_or_indication(NR_STUN_MODE_STUND_0_96, NR_STUN_MSG_BINDING_REQUEST, &req)))
        ABORT(r);

    if ((r=nr_stun_message_add_change_request_attribute(req, 0)))
        ABORT(r);

    assert(! nr_stun_message_has_attribute(req, NR_STUN_ATTR_USERNAME, 0));
    assert(! nr_stun_message_has_attribute(req, NR_STUN_ATTR_MESSAGE_INTEGRITY, 0));

    *msg = req;

   _status=0;
 abort:
   if (_status) nr_stun_message_destroy(&req);
   return _status;
}
#endif /* USE_STUND_0_96 */

#ifdef USE_ICE
int
nr_stun_build_use_candidate(nr_stun_client_ice_binding_request_params *params, nr_stun_message **msg)
{
   int r,_status;
   nr_stun_message *req = 0;

   if ((r=nr_stun_form_request_or_indication(NR_STUN_MODE_STUN, NR_STUN_MSG_BINDING_REQUEST, &req)))
       ABORT(r);

   if ((r=nr_stun_message_add_username_attribute(req, params->username)))
       ABORT(r);

   if ((r=nr_stun_message_add_message_integrity_attribute(req, &params->password)))
       ABORT(r);

   if ((r=nr_stun_message_add_use_candidate_attribute(req)))
       ABORT(r);

   if ((r=nr_stun_message_add_priority_attribute(req, params->priority)))
       ABORT(r);

   if ((r=nr_stun_message_add_ice_controlling_attribute(req, params->tiebreaker)))
     ABORT(r);

   *msg = req;

   _status=0;
 abort:
   if (_status) nr_stun_message_destroy(&req);
   return _status;
}

int
nr_stun_build_req_ice(nr_stun_client_ice_binding_request_params *params, nr_stun_message **msg)
{
   int r,_status;
   nr_stun_message *req = 0;

   if ((r=nr_stun_form_request_or_indication(NR_STUN_MODE_STUN, NR_STUN_MSG_BINDING_REQUEST, &req)))
       ABORT(r);

   if ((r=nr_stun_message_add_username_attribute(req, params->username)))
       ABORT(r);

   if ((r=nr_stun_message_add_message_integrity_attribute(req, &params->password)))
       ABORT(r);

   if ((r=nr_stun_message_add_priority_attribute(req, params->priority)))
       ABORT(r);

   switch (params->control) {
   case NR_ICE_CONTROLLING:
       if ((r=nr_stun_message_add_ice_controlling_attribute(req, params->tiebreaker)))
           ABORT(r);
       break;
   case NR_ICE_CONTROLLED:
       if ((r=nr_stun_message_add_ice_controlled_attribute(req, params->tiebreaker)))
           ABORT(r);
       break;
   default:
       assert(0);
       ABORT(R_INTERNAL);
   }

   *msg = req;

   _status=0;
 abort:
   if (_status) nr_stun_message_destroy(&req);
   return _status;
}
#endif /* USE_ICE */

#ifdef USE_TURN

#ifndef __isascii
#define __isascii(c) (((c) & ~0x7F) == 0)
#endif

/* Long-term passwords are computed over the key:

            key = MD5(username ":" realm ":" SASLprep(password))

   Per RFC 5389 S 15.4
*/
int
nr_stun_compute_lt_message_integrity_password(const char *username, const char *realm,
                                              Data *password, Data *hmac_key)
{
  char digest_input[1000];
  int i;
  int r, _status;
  size_t len;

  /* First check that the password is ASCII. We are supposed to
     SASLprep but we don't support this yet
     TODO(ekr@rtfm.com): Add SASLprep for password.
 */
  for (i=0; i<password->len; i++) {
    if (!__isascii(password->data[i]))
      ABORT(R_BAD_DATA);
  }

  if (hmac_key->len < 16)
    ABORT(R_BAD_ARGS);

  snprintf(digest_input, sizeof(digest_input), "%s:%s:", username, realm);
  if ((sizeof(digest_input) - strlen(digest_input)) < password->len)
    ABORT(R_BAD_DATA);

  len = strlen(digest_input);
  memcpy(digest_input + len, password->data, password->len);


  if (r=nr_crypto_md5((UCHAR *)digest_input, len + password->len, hmac_key->data))
    ABORT(r);
  hmac_key->len=16;

  _status=0;
abort:
  return(_status);
}

static int
nr_stun_build_auth_params(nr_stun_client_auth_params *auth, nr_stun_message *req)
{
  int r, _status;
  UCHAR hmac_key_d[16];
  Data hmac_key;

  ATTACH_DATA(hmac_key, hmac_key_d);

  if (!auth->authenticate)
    goto done;

  assert(auth->username);
  assert(auth->password.len);
  assert(auth->realm);
  assert(auth->nonce);

  if (r=nr_stun_compute_lt_message_integrity_password(auth->username,
                                                      auth->realm,
                                                      &auth->password,
                                                      &hmac_key))
    ABORT(r);

  if (!auth->username) {
    r_log(NR_LOG_STUN, LOG_WARNING, "STUN authentication requested but no username provided");
    ABORT(R_INTERNAL);
  }

  if (!auth->password.len) {
    r_log(NR_LOG_STUN, LOG_WARNING, "STUN authentication requested but no password provided");
    ABORT(R_INTERNAL);
  }

  if (!auth->realm) {
    r_log(NR_LOG_STUN, LOG_WARNING, "STUN authentication requested but no realm provided");
    ABORT(R_INTERNAL);
  }

  if (!auth->nonce) {
    r_log(NR_LOG_STUN, LOG_WARNING, "STUN authentication requested but no nonce provided");
    ABORT(R_INTERNAL);
  }

  if ((r=nr_stun_message_add_username_attribute(req, auth->username)))
    ABORT(r);

  if ((r=nr_stun_message_add_realm_attribute(req, auth->realm)))
    ABORT(r);

  if ((r=nr_stun_message_add_nonce_attribute(req, auth->nonce)))
    ABORT(r);

  if ((r=nr_stun_message_add_message_integrity_attribute(req, &hmac_key)))
    ABORT(r);

done:
  _status=0;
abort:
  return(_status);
}

int
nr_stun_build_allocate_request(nr_stun_client_auth_params *auth, nr_stun_client_allocate_request_params *params, nr_stun_message **msg)
{
   int r,_status;
   nr_stun_message *req = 0;

   if ((r=nr_stun_form_request_or_indication(NR_STUN_MODE_STUN, NR_STUN_MSG_ALLOCATE_REQUEST, &req)))
       ABORT(r);

   if ((r=nr_stun_message_add_requested_transport_attribute(req, NR_STUN_ATTR_REQUESTED_TRANSPORT_UDP)))
       ABORT(r);

  if ((r=nr_stun_message_add_lifetime_attribute(req, params->lifetime_secs)))
       ABORT(r);

  /* TODO(ekr@rtfm.com): Add the SOFTWARE attribute (Firefox bug 857666) */

  if ((r=nr_stun_build_auth_params(auth, req)))
       ABORT(r);

   *msg = req;

   _status=0;
 abort:
   if (_status) nr_stun_message_destroy(&req);
   return _status;
}


int nr_stun_build_refresh_request(nr_stun_client_auth_params *auth, nr_stun_client_refresh_request_params *params, nr_stun_message **msg)
{
   int r,_status;
   nr_stun_message *req = 0;

   if ((r=nr_stun_form_request_or_indication(NR_STUN_MODE_STUN, NR_STUN_MSG_REFRESH_REQUEST, &req)))
       ABORT(r);

  if ((r=nr_stun_message_add_lifetime_attribute(req, params->lifetime_secs)))
       ABORT(r);


  /* TODO(ekr@rtfm.com): Add the SOFTWARE attribute (Firefox bug 857666) */

  if ((r=nr_stun_build_auth_params(auth, req)))
       ABORT(r);

  *msg = req;

   _status=0;
 abort:
   if (_status) nr_stun_message_destroy(&req);
   return _status;
}


int nr_stun_build_permission_request(nr_stun_client_auth_params *auth, nr_stun_client_permission_request_params *params, nr_stun_message **msg)
{
   int r,_status;
   nr_stun_message *req = 0;

   if ((r=nr_stun_form_request_or_indication(NR_STUN_MODE_STUN, NR_STUN_MSG_PERMISSION_REQUEST, &req)))
       ABORT(r);

   if ((r=nr_stun_message_add_xor_peer_address_attribute(req, &params->remote_addr)))
       ABORT(r);

   if ((r=nr_stun_build_auth_params(auth, req)))
       ABORT(r);

   *msg = req;

   _status=0;
 abort:
   if (_status) nr_stun_message_destroy(&req);
   return _status;
}

int
nr_stun_build_send_indication(nr_stun_client_send_indication_params *params, nr_stun_message **msg)
{
   int r,_status;
   nr_stun_message *ind = 0;

   if ((r=nr_stun_form_request_or_indication(NR_STUN_MODE_STUN, NR_STUN_MSG_SEND_INDICATION, &ind)))
       ABORT(r);

   if ((r=nr_stun_message_add_xor_peer_address_attribute(ind, &params->remote_addr)))
       ABORT(r);

   if ((r=nr_stun_message_add_data_attribute(ind, params->data.data, params->data.len)))
       ABORT(r);

   *msg = ind;

   _status=0;
 abort:
   if (_status) nr_stun_message_destroy(&ind);
   return _status;
}

int
nr_stun_build_data_indication(nr_stun_client_data_indication_params *params, nr_stun_message **msg)
{
   int r,_status;
   nr_stun_message *ind = 0;

   if ((r=nr_stun_form_request_or_indication(NR_STUN_MODE_STUN, NR_STUN_MSG_DATA_INDICATION, &ind)))
       ABORT(r);

   if ((r=nr_stun_message_add_xor_peer_address_attribute(ind, &params->remote_addr)))
       ABORT(r);

   if ((r=nr_stun_message_add_data_attribute(ind, params->data.data, params->data.len)))
       ABORT(r);

   *msg = ind;

   _status=0;
 abort:
   if (_status) nr_stun_message_destroy(&ind);
   return _status;
}

#endif /* USE_TURN */

/* draft-ietf-behave-rfc3489bis-10.txt S 7.3.1.1 */
int
nr_stun_form_success_response(nr_stun_message *req, nr_transport_addr *from, Data *password, nr_stun_message *res)
{
    int r,_status;
    int request_method;
    char server_name[NR_STUN_MAX_SERVER_BYTES+1]; /* +1 for \0 */

    /* set up information for default response */

    request_method = NR_STUN_GET_TYPE_METHOD(req->header.type);
    res->header.type = NR_STUN_TYPE(request_method, NR_CLASS_RESPONSE);
    res->header.magic_cookie = req->header.magic_cookie;
    memcpy(&res->header.id, &req->header.id, sizeof(res->header.id));

    r_log(NR_LOG_STUN, LOG_DEBUG, "Mapped Address = %s", from->as_string);

    if ((r=nr_stun_message_add_xor_mapped_address_attribute(res, from)))
        ABORT(r);

    if (!NR_reg_get_string(NR_STUN_REG_PREF_SERVER_NAME, server_name, sizeof(server_name))) {
        if ((r=nr_stun_message_add_server_attribute(res, server_name)))
            ABORT(r);
    }

    if (res->header.magic_cookie == NR_STUN_MAGIC_COOKIE) {
        if (password != 0) {
            if ((r=nr_stun_message_add_message_integrity_attribute(res, password)))
                ABORT(r);
        }

        if ((r=nr_stun_message_add_fingerprint_attribute(res)))
            ABORT(r);
    }

    _status=0;
 abort:
    return _status;
}

/* draft-ietf-behave-rfc3489bis-10.txt S 7.3.1.1 */
void
nr_stun_form_error_response(nr_stun_message *req, nr_stun_message* res, int number, char* msg)
{
    char *str;
    int request_method;
    char server_name[NR_STUN_MAX_SERVER_BYTES+1]; /* +1 for \0 */

    if (number < 300 || number > 699)
        number = 500;

    r_log(NR_LOG_STUN, LOG_INFO, "Responding with error %d: %s", number, msg);

    request_method = NR_STUN_GET_TYPE_METHOD(req->header.type);
    res->header.type = NR_STUN_TYPE(request_method, NR_CLASS_ERROR_RESPONSE);
    res->header.magic_cookie = req->header.magic_cookie;
    memcpy(&res->header.id, &req->header.id, sizeof(res->header.id));

    /* during development we should never see 500s (hopefully not in deployment either) */

    str = 0;
    switch (number) {
    case 300:  str = "Try Alternate";       break;
    case 400:  str = "Bad Request";         break;
    case 401:  str = "Unauthorized";        break;
    case 420:  str = "Unknown Attribute";   break;
    case 438:  str = "Stale Nonce";         break;
#ifdef USE_ICE
    case 487:  str = "Role Conflict";       break;
#endif
    case 500:  str = "Server Error";        break;
    }
    if (str == 0) {
        str = "Unknown";
    }

    if (nr_stun_message_add_error_code_attribute(res, number, str)) {
         assert(0);  /* should never happen */
    }

    if (!NR_reg_get_string(NR_STUN_REG_PREF_SERVER_NAME, server_name, sizeof(server_name))) {
        nr_stun_message_add_server_attribute(res, server_name);
    }
}

