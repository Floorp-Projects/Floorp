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



#ifndef _stun_codec_h
#define _stun_codec_h

#include "stun_msg.h"

typedef struct nr_stun_attr_info_  nr_stun_attr_info;

typedef struct nr_stun_attr_codec_ {
    char     *name;
    int     (*print)(nr_stun_attr_info *attr_info, char *msg, void *data);
    int     (*encode)(nr_stun_attr_info *attr_info, void *data, size_t offset, size_t buflen, UCHAR *buf, size_t *attrlen);
    int     (*decode)(nr_stun_attr_info *attr_info, size_t attrlen, UCHAR *buf, size_t offset, size_t buflen, void *data);
} nr_stun_attr_codec;

struct nr_stun_attr_info_ {
     UINT2                 type;
     char                 *name;
     nr_stun_attr_codec   *codec;
     int                 (*illegal)(nr_stun_attr_info *attr_info, size_t attrlen, void *data);
};

extern nr_stun_attr_codec nr_stun_attr_codec_UINT4;
extern nr_stun_attr_codec nr_stun_attr_codec_UINT8;
extern nr_stun_attr_codec nr_stun_attr_codec_addr;
extern nr_stun_attr_codec nr_stun_attr_codec_bytes;
extern nr_stun_attr_codec nr_stun_attr_codec_data;
extern nr_stun_attr_codec nr_stun_attr_codec_error_code;
extern nr_stun_attr_codec nr_stun_attr_codec_fingerprint;
extern nr_stun_attr_codec nr_stun_attr_codec_flag;
extern nr_stun_attr_codec nr_stun_attr_codec_message_integrity;
extern nr_stun_attr_codec nr_stun_attr_codec_noop;
extern nr_stun_attr_codec nr_stun_attr_codec_quoted_string;
extern nr_stun_attr_codec nr_stun_attr_codec_string;
extern nr_stun_attr_codec nr_stun_attr_codec_unknown_attributes;
extern nr_stun_attr_codec nr_stun_attr_codec_xor_mapped_address;
extern nr_stun_attr_codec nr_stun_attr_codec_xor_peer_address;
extern nr_stun_attr_codec nr_stun_attr_codec_old_xor_mapped_address;

size_t nr_count_utf8_code_points_without_validation(const char *s);
int nr_stun_encode_message(nr_stun_message *msg);
int nr_stun_decode_message(nr_stun_message *msg, int (*get_password)(void *arg, nr_stun_message *msg, Data **password), void *arg);

#endif

