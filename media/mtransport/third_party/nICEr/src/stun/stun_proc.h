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



#ifndef _stun_proc_h
#define _stun_proc_h

#include "stun.h"

int nr_stun_receive_message(nr_stun_message *req, nr_stun_message *msg);
int nr_stun_process_request(nr_stun_message *req, nr_stun_message *res);
int nr_stun_process_indication(nr_stun_message *ind);
int nr_stun_process_success_response(nr_stun_message *res);
int nr_stun_process_error_response(nr_stun_message *res);

int nr_stun_receive_request_or_indication_short_term_auth(nr_stun_message *msg, nr_stun_message *res);
int nr_stun_receive_response_short_term_auth(nr_stun_message *res);

int nr_stun_receive_request_long_term_auth(nr_stun_message *req, nr_stun_server_ctx *ctx, nr_stun_message *res);
int nr_stun_receive_response_long_term_auth(nr_stun_message *res, nr_stun_client_ctx *ctx);

#endif

