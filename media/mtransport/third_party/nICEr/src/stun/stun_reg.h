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



#ifndef _stun_reg_h
#define _stun_reg_h
#ifdef __cplusplus
using namespace std;
extern "C" {
#endif /* __cplusplus */

#define NR_STUN_REG_PREF_CLNT_RETRANSMIT_TIMEOUT    "stun.client.retransmission_timeout"
#define NR_STUN_REG_PREF_CLNT_RETRANSMIT_BACKOFF    "stun.client.retransmission_backoff_factor"
#define NR_STUN_REG_PREF_CLNT_MAXIMUM_TRANSMITS     "stun.client.maximum_transmits"
#define NR_STUN_REG_PREF_CLNT_FINAL_RETRANSMIT_BACKOFF   "stun.client.final_retransmit_backoff"

#define NR_STUN_REG_PREF_ADDRESS_PRFX               "stun.address"
#define NR_STUN_REG_PREF_SERVER_NAME                "stun.server.name"
#define NR_STUN_REG_PREF_SERVER_NONCE_SIZE          "stun.server.nonce_size"
#define NR_STUN_REG_PREF_SERVER_REALM               "stun.server.realm"

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif

