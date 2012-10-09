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



static char *RCSSTRING __UNUSED__="$Id: nr_crypto.c,v 1.2 2008/04/28 17:59:01 ekr Exp $";

#include <nr_api.h>
#include "nr_crypto.h"

static int nr_ice_crypto_dummy_random_bytes(UCHAR *buf, int len)
  {
    fprintf(stderr,"Need to define crypto API implementation\n");

    exit(1);
  }

static int nr_ice_crypto_dummy_hmac_sha1(UCHAR *key, int key_l, UCHAR *buf, int buf_l, UCHAR digest[20])
  {
    fprintf(stderr,"Need to define crypto API implementation\n");

    exit(1);
  }

static nr_ice_crypto_vtbl nr_ice_crypto_dummy_vtbl= {
  nr_ice_crypto_dummy_random_bytes,
  nr_ice_crypto_dummy_hmac_sha1
};



nr_ice_crypto_vtbl *nr_crypto_vtbl=&nr_ice_crypto_dummy_vtbl;


