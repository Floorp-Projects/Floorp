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


static char *RCSSTRING __UNUSED__="$Id: stun_codec.c,v 1.2 2008/04/28 18:21:30 ekr Exp $";

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
#include <stddef.h>

#include "nr_api.h"
#include "stun.h"
#include "byteorder.h"
#include "r_crc32.h"
#include "nr_crypto.h"
#include "mbslen.h"

#define NR_STUN_IPV4_FAMILY  0x01
#define NR_STUN_IPV6_FAMILY  0x02

#define SKIP_ATTRIBUTE_DECODE -1

static int nr_stun_find_attr_info(UINT2 type, nr_stun_attr_info **info);

static int nr_stun_fix_attribute_ordering(nr_stun_message *msg);

static int nr_stun_encode_htons(UINT2 data, int buflen, UCHAR *buf, int *offset);
static int nr_stun_encode_htonl(UINT4 data, int buflen, UCHAR *buf, int *offset);
static int nr_stun_encode_htonll(UINT8 data, int buflen, UCHAR *buf, int *offset);
static int nr_stun_encode(UCHAR *data, int length, int buflen, UCHAR *buf, int *offset);

static int nr_stun_decode_htons(UCHAR *buf, int buflen, int *offset, UINT2 *data);
static int nr_stun_decode_htonl(UCHAR *buf, int buflen, int *offset, UINT4 *data);
static int nr_stun_decode_htonll(UCHAR *buf, int buflen, int *offset, UINT8 *data);
static int nr_stun_decode(int length, UCHAR *buf, int buflen, int *offset, UCHAR *data);

static int nr_stun_attr_string_illegal(nr_stun_attr_info *attr_info, int len, void *data, int max_bytes, int max_chars);

static int nr_stun_attr_error_code_illegal(nr_stun_attr_info *attr_info, int attrlen, void *data);
static int nr_stun_attr_nonce_illegal(nr_stun_attr_info *attr_info, int attrlen, void *data);
static int nr_stun_attr_realm_illegal(nr_stun_attr_info *attr_info, int attrlen, void *data);
static int nr_stun_attr_server_illegal(nr_stun_attr_info *attr_info, int attrlen, void *data);
static int nr_stun_attr_username_illegal(nr_stun_attr_info *attr_info, int attrlen, void *data);
static int
nr_stun_attr_codec_fingerprint_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data);


int
nr_stun_encode_htons(UINT2 data, int buflen, UCHAR *buf, int *offset)
{
   UINT2 d = htons(data);

   if (*offset + sizeof(d) >= buflen) {
      r_log(NR_LOG_STUN, LOG_WARNING, "Attempted buffer overrun: %d + %zd >= %d", *offset, sizeof(d), buflen);
      return R_BAD_DATA;
   }

   memcpy(&buf[*offset], &d, sizeof(d));
   *offset += sizeof(d);

   return 0;
}

int
nr_stun_encode_htonl(UINT4 data, int buflen, UCHAR *buf, int *offset)
{
   UINT4 d = htonl(data);

   if (*offset + sizeof(d) > buflen) {
      r_log(NR_LOG_STUN, LOG_WARNING, "Attempted buffer overrun: %d + %zd > %d", *offset, sizeof(d), buflen);
      return R_BAD_DATA;
   }

   memcpy(&buf[*offset], &d, sizeof(d));
   *offset += sizeof(d);

   return 0;
}

int
nr_stun_encode_htonll(UINT8 data, int buflen, UCHAR *buf, int *offset)
{
   UINT8 d = nr_htonll(data);

   if (*offset + sizeof(d) > buflen) {
      r_log(NR_LOG_STUN, LOG_WARNING, "Attempted buffer overrun: %d + %zd > %d", *offset, sizeof(d), buflen);
      return R_BAD_DATA;
   }

   memcpy(&buf[*offset], &d, sizeof(d));
   *offset += sizeof(d);

   return 0;
}

int
nr_stun_encode(UCHAR *data, int length, int buflen, UCHAR *buf, int *offset)
{
   if (*offset + length > buflen) {
      r_log(NR_LOG_STUN, LOG_WARNING, "Attempted buffer overrun: %d + %d > %d", *offset, length, buflen);
      return R_BAD_DATA;
   }

   memcpy(&buf[*offset], data, length);
   *offset += length;

   return 0;
}


int
nr_stun_decode_htons(UCHAR *buf, int buflen, int *offset, UINT2 *data)
{
   UINT2 d;

   if (*offset + sizeof(d) > buflen) {
      r_log(NR_LOG_STUN, LOG_WARNING, "Attempted buffer overrun: %d + %zd > %d", *offset, sizeof(d), buflen);
      return R_BAD_DATA;
   }

   memcpy(&d, &buf[*offset], sizeof(d));
   *offset += sizeof(d);
   *data = htons(d);

   return 0;
}

int
nr_stun_decode_htonl(UCHAR *buf, int buflen, int *offset, UINT4 *data)
{
   UINT4 d;

   if (*offset + sizeof(d) > buflen) {
      r_log(NR_LOG_STUN, LOG_WARNING, "Attempted buffer overrun: %d + %zd > %d", *offset, sizeof(d), buflen);
      return R_BAD_DATA;
   }

   memcpy(&d, &buf[*offset], sizeof(d));
   *offset += sizeof(d);
   *data = htonl(d);

   return 0;
}

int
nr_stun_decode_htonll(UCHAR *buf, int buflen, int *offset, UINT8 *data)
{
   UINT8 d;

   if (*offset + sizeof(d) > buflen) {
      r_log(NR_LOG_STUN, LOG_WARNING, "Attempted buffer overrun: %d + %zd > %d", *offset, sizeof(d), buflen);
      return R_BAD_DATA;
   }

   memcpy(&d, &buf[*offset], sizeof(d));
   *offset += sizeof(d);
   *data = nr_htonll(d);

   return 0;
}

int
nr_stun_decode(int length, UCHAR *buf, int buflen, int *offset, UCHAR *data)
{
   if (*offset + length > buflen) {
      r_log(NR_LOG_STUN, LOG_WARNING, "Attempted buffer overrun: %d + %d > %d", *offset, length, buflen);
      return R_BAD_DATA;
   }

   memcpy(data, &buf[*offset], length);
   *offset += length;

   return 0;
}

int
nr_stun_attr_string_illegal(nr_stun_attr_info *attr_info, int len, void *data, int max_bytes, int max_chars)
{
    int _status;
    char *s = data;
    size_t nchars;

    if (len > max_bytes) {
        r_log(NR_LOG_STUN, LOG_WARNING, "%s is too large: %d bytes", attr_info->name, len);
        ABORT(R_FAILED);
    }

    if (max_chars >= 0) {
        if (mbslen(s, &nchars)) {
            /* who knows what to do, just assume everything is working ok */
        }
        else if (nchars > max_chars) {
            r_log(NR_LOG_STUN, LOG_WARNING, "%s is too large: %zd characters", attr_info->name, nchars);
            ABORT(R_FAILED);
        }
    }

    _status = 0;
  abort:
    return _status;
}

int
nr_stun_attr_error_code_illegal(nr_stun_attr_info *attr_info, int attrlen, void *data)
{
    int r,_status;
    nr_stun_attr_error_code *ec = data;

    if (ec->number < 300 || ec->number > 699)
        ABORT(R_FAILED);

    if ((r=nr_stun_attr_string_illegal(attr_info, strlen(ec->reason), ec->reason, NR_STUN_MAX_ERROR_CODE_REASON_BYTES, NR_STUN_MAX_ERROR_CODE_REASON_CHARS)))
        ABORT(r);

    _status = 0;
  abort:
    return _status;
}

int
nr_stun_attr_nonce_illegal(nr_stun_attr_info *attr_info, int attrlen, void *data)
{
    return nr_stun_attr_string_illegal(attr_info, attrlen, data, NR_STUN_MAX_NONCE_BYTES, NR_STUN_MAX_NONCE_CHARS);
}

int
nr_stun_attr_realm_illegal(nr_stun_attr_info *attr_info, int attrlen, void *data)
{
    return nr_stun_attr_string_illegal(attr_info, attrlen, data, NR_STUN_MAX_REALM_BYTES, NR_STUN_MAX_REALM_CHARS);
}

int
nr_stun_attr_server_illegal(nr_stun_attr_info *attr_info, int attrlen, void *data)
{
    return nr_stun_attr_string_illegal(attr_info, attrlen, data, NR_STUN_MAX_SERVER_BYTES, NR_STUN_MAX_SERVER_CHARS);
}

int
nr_stun_attr_username_illegal(nr_stun_attr_info *attr_info, int attrlen, void *data)
{
    return nr_stun_attr_string_illegal(attr_info, attrlen, data, NR_STUN_MAX_USERNAME_BYTES, -1);
}

static int
nr_stun_attr_codec_UCHAR_print(nr_stun_attr_info *attr_info, char *msg, void *data)
{
    r_log(NR_LOG_STUN, LOG_DEBUG, "%s %s: %u", msg, attr_info->name, *(UCHAR*)data);
    return 0;
}

static int
nr_stun_attr_codec_UCHAR_encode(nr_stun_attr_info *attr_info, void *data, int offset, int buflen, UCHAR *buf, int *attrlen)
{
    int start = offset;
    UINT4 tmp = *((UCHAR *)data);
    tmp <<= 24;

    if (nr_stun_encode_htons(attr_info->type    , buflen, buf, &offset)
     || nr_stun_encode_htons(sizeof(UINT4)      , buflen, buf, &offset)
     || nr_stun_encode_htonl(tmp                , buflen, buf, &offset))
        return R_FAILED;

    *attrlen = offset - start;

    return 0;
}

static int
nr_stun_attr_codec_UCHAR_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
    UINT4 tmp;

    if (attrlen != sizeof(UINT4)) {
        r_log(NR_LOG_STUN, LOG_WARNING, "Integer is illegal size: %d", attrlen);
        return R_FAILED;
    }

    if (nr_stun_decode_htonl(buf, buflen, &offset, &tmp))
        return R_FAILED;

    *((UCHAR *)data) = (tmp >> 24) & 0xff;

    return 0;
}

nr_stun_attr_codec nr_stun_attr_codec_UCHAR = {
    "UCHAR",
    nr_stun_attr_codec_UCHAR_print,
    nr_stun_attr_codec_UCHAR_encode,
    nr_stun_attr_codec_UCHAR_decode
};

static int
nr_stun_attr_codec_UINT4_print(nr_stun_attr_info *attr_info, char *msg, void *data)
{
    r_log(NR_LOG_STUN, LOG_DEBUG, "%s %s: %u", msg, attr_info->name, *(UINT4*)data);
    return 0;
}

static int
nr_stun_attr_codec_UINT4_encode(nr_stun_attr_info *attr_info, void *data, int offset, int buflen, UCHAR *buf, int *attrlen)
{
    int start = offset;

    if (nr_stun_encode_htons(attr_info->type    , buflen, buf, &offset)
     || nr_stun_encode_htons(sizeof(UINT4)      , buflen, buf, &offset)
     || nr_stun_encode_htonl(*(UINT4*)data      , buflen, buf, &offset))
        return R_FAILED;

    *attrlen = offset - start;

    return 0;
}

static int
nr_stun_attr_codec_UINT4_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
    if (attrlen != sizeof(UINT4)) {
        r_log(NR_LOG_STUN, LOG_WARNING, "Integer is illegal size: %d", attrlen);
        return R_FAILED;
    }

    if (nr_stun_decode_htonl(buf, buflen, &offset, (UINT4*)data))
        return R_FAILED;

    return 0;
}

nr_stun_attr_codec nr_stun_attr_codec_UINT4 = {
    "UINT4",
    nr_stun_attr_codec_UINT4_print,
    nr_stun_attr_codec_UINT4_encode,
    nr_stun_attr_codec_UINT4_decode
};

static int
nr_stun_attr_codec_UINT8_print(nr_stun_attr_info *attr_info, char *msg, void *data)
{
    r_log(NR_LOG_STUN, LOG_DEBUG, "%s %s: %llu", msg, attr_info->name, *(UINT8*)data);
    return 0;
}

static int
nr_stun_attr_codec_UINT8_encode(nr_stun_attr_info *attr_info, void *data, int offset, int buflen, UCHAR *buf, int *attrlen)
{
    int start = offset;

    if (nr_stun_encode_htons(attr_info->type   , buflen, buf, &offset)
     || nr_stun_encode_htons(sizeof(UINT8)     , buflen, buf, &offset)
     || nr_stun_encode_htonll(*(UINT8*)data    , buflen, buf, &offset))
        return R_FAILED;

    *attrlen = offset - start;

    return 0;
}

static int
nr_stun_attr_codec_UINT8_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
    if (attrlen != sizeof(UINT8)) {
        r_log(NR_LOG_STUN, LOG_WARNING, "Integer is illegal size: %d", attrlen);
        return R_FAILED;
    }

    if (nr_stun_decode_htonll(buf, buflen, &offset, (UINT8*)data))
        return R_FAILED;

    return 0;
}

nr_stun_attr_codec nr_stun_attr_codec_UINT8 = {
    "UINT8",
    nr_stun_attr_codec_UINT8_print,
    nr_stun_attr_codec_UINT8_encode,
    nr_stun_attr_codec_UINT8_decode
};

static int
nr_stun_attr_codec_addr_print(nr_stun_attr_info *attr_info, char *msg, void *data)
{
    r_log(NR_LOG_STUN, LOG_DEBUG, "%s %s: %s", msg, attr_info->name, ((nr_transport_addr*)data)->as_string);
    return 0;
}

static int
nr_stun_attr_codec_addr_encode(nr_stun_attr_info *attr_info, void *data, int offset, int buflen, UCHAR *buf, int *attrlen)
{
    int r,_status;
    int start = offset;
    nr_transport_addr *addr = data;
    UCHAR pad = '\0';
    UCHAR family;

    if ((r=nr_stun_encode_htons(attr_info->type, buflen, buf, &offset)))
        ABORT(r);

    switch (addr->ip_version) {
    case NR_IPV4:
        family = NR_STUN_IPV4_FAMILY;
        if (nr_stun_encode_htons(8               , buflen, buf, &offset)
         || nr_stun_encode(&pad, 1               , buflen, buf, &offset)
         || nr_stun_encode(&family, 1            , buflen, buf, &offset)
         || nr_stun_encode_htons(ntohs(addr->u.addr4.sin_port), buflen, buf, &offset)
         || nr_stun_encode_htonl(ntohl(addr->u.addr4.sin_addr.s_addr), buflen, buf, &offset))
            ABORT(R_FAILED);
        break;

    case NR_IPV6:
        assert(0);
        ABORT(R_INTERNAL);
        break;
    default:
        assert(0);
        ABORT(R_INTERNAL);
        break;
    }

    *attrlen = offset - start;

    _status = 0;
  abort:
    return _status;
}

static int
nr_stun_attr_codec_addr_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
    int _status;
    UCHAR pad;
    UCHAR family;
    UINT2 port;
    UINT4 addr4;
    nr_transport_addr *result = data;

    if (nr_stun_decode(1, buf, buflen, &offset, &pad)
     || nr_stun_decode(1, buf, buflen, &offset, &family))
        ABORT(R_FAILED);

    switch (family) {
    case NR_STUN_IPV4_FAMILY:
        if (attrlen != 8) {
            r_log(NR_LOG_STUN, LOG_WARNING, "Illegal attribute length: %d", attrlen);
            ABORT(R_FAILED);
        }

        if (nr_stun_decode_htons(buf, buflen, &offset, &port)
         || nr_stun_decode_htonl(buf, buflen, &offset, &addr4))
            ABORT(R_FAILED);

        if (nr_ip4_port_to_transport_addr(addr4, port, IPPROTO_UDP, result))
            ABORT(R_FAILED);
        break;

    case NR_STUN_IPV6_FAMILY:
        if (attrlen != 16) {
            r_log(NR_LOG_STUN, LOG_WARNING, "Illegal attribute length: %d", attrlen);
            ABORT(R_FAILED);
        }

        r_log(NR_LOG_STUN, LOG_WARNING, "IPv6 not supported");
#ifdef NDEBUG
        ABORT(SKIP_ATTRIBUTE_DECODE);
#else
        UNIMPLEMENTED;
#endif /* NDEBUG */
        break;

    default:
        r_log(NR_LOG_STUN, LOG_WARNING, "Illegal address family: %d", family);
        ABORT(R_FAILED);
        break;
    }

    _status = 0;
  abort:
    return _status;
}

nr_stun_attr_codec nr_stun_attr_codec_addr = {
    "addr",
    nr_stun_attr_codec_addr_print,
    nr_stun_attr_codec_addr_encode,
    nr_stun_attr_codec_addr_decode
};

static int
nr_stun_attr_codec_data_print(nr_stun_attr_info *attr_info, char *msg, void *data)
{
    nr_stun_attr_data *d = data;
    r_dump(NR_LOG_STUN, LOG_DEBUG, attr_info->name, (char*)d->data, d->length);
    return 0;
}

static int
nr_stun_attr_codec_data_encode(nr_stun_attr_info *attr_info, void *data, int offset, int buflen, UCHAR *buf, int *attrlen)
{
    nr_stun_attr_data *d = data;
    int start = offset;

    if (nr_stun_encode_htons(attr_info->type     , buflen, buf, &offset)
     || nr_stun_encode_htons(d->length           , buflen, buf, &offset)
     || nr_stun_encode(d->data, d->length        , buflen, buf, &offset))
        return R_FAILED;

    *attrlen = offset - start;

    return 0;
}

static int
nr_stun_attr_codec_data_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
    int _status;
    nr_stun_attr_data *result = data;

    /* -1 because it is going to be null terminated just to be safe */
    if (attrlen >= (sizeof(result->data) - 1)) {
        r_log(NR_LOG_STUN, LOG_WARNING, "Too much data: %d bytes", attrlen);
        ABORT(R_FAILED);
    }

    if (nr_stun_decode(attrlen, buf, buflen, &offset, result->data))
        ABORT(R_FAILED);

    result->length = attrlen;
    result->data[attrlen] = '\0'; /* just to be nice */

    _status=0;
  abort:
    return _status;
}

nr_stun_attr_codec nr_stun_attr_codec_data = {
    "data",
    nr_stun_attr_codec_data_print,
    nr_stun_attr_codec_data_encode,
    nr_stun_attr_codec_data_decode
};

static int
nr_stun_attr_codec_error_code_print(nr_stun_attr_info *attr_info, char *msg, void *data)
{
    nr_stun_attr_error_code *error_code = data;
    r_log(NR_LOG_STUN, LOG_DEBUG, "%s %s: %d %s",
                          msg, attr_info->name, error_code->number,
                          error_code->reason);
    return 0;
}

static int
nr_stun_attr_codec_error_code_encode(nr_stun_attr_info *attr_info, void *data, int offset, int buflen, UCHAR *buf, int *attrlen)
{
    nr_stun_attr_error_code *error_code = data;
    int start = offset;
    int length = strlen(error_code->reason);
    UCHAR pad[2] = { 0 };
    UCHAR class = error_code->number / 100;
    UCHAR number = error_code->number % 100;

    if (nr_stun_encode_htons(attr_info->type   , buflen, buf, &offset)
     || nr_stun_encode_htons(4 + length        , buflen, buf, &offset)
     || nr_stun_encode(pad, 2                  , buflen, buf, &offset)
     || nr_stun_encode(&class, 1               , buflen, buf, &offset)
     || nr_stun_encode(&number, 1              , buflen, buf, &offset)
     || nr_stun_encode((UCHAR*)error_code->reason, length, buflen, buf, &offset))
        return R_FAILED;

    *attrlen = offset - start;

    return 0;
}

static int
nr_stun_attr_codec_error_code_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
    int _status;
    nr_stun_attr_error_code *result = data;
    UCHAR pad[2];
    UCHAR class;
    UCHAR number;
    int size_reason;

    if (nr_stun_decode(2, buf, buflen, &offset, pad)
     || nr_stun_decode(1, buf, buflen, &offset, &class)
     || nr_stun_decode(1, buf, buflen, &offset, &number))
        ABORT(R_FAILED);

    result->number = (class * 100) + number;

    size_reason = attrlen - 4;

    /* -1 because the string will be null terminated */
    if (size_reason > (sizeof(result->reason) - 1)) {
        r_log(NR_LOG_STUN, LOG_WARNING, "Reason is too large, truncating");
        /* don't fail, but instead truncate the reason */
        size_reason = sizeof(result->reason) - 1;
    }

    if (nr_stun_decode(size_reason, buf, buflen, &offset, (UCHAR*)result->reason))
        ABORT(R_FAILED);
    result->reason[size_reason] = '\0';

    _status=0;
  abort:
    return _status;
}

nr_stun_attr_codec nr_stun_attr_codec_error_code = {
    "error_code",
    nr_stun_attr_codec_error_code_print,
    nr_stun_attr_codec_error_code_encode,
    nr_stun_attr_codec_error_code_decode
};

static int
nr_stun_attr_codec_fingerprint_print(nr_stun_attr_info *attr_info, char *msg, void *data)
{
    nr_stun_attr_fingerprint *fingerprint = data;
    r_log(NR_LOG_STUN, LOG_DEBUG, "%s %s: %08x", msg, attr_info->name, fingerprint->checksum);
    return 0;
}

static int
nr_stun_attr_codec_fingerprint_encode(nr_stun_attr_info *attr_info, void *data, int offset, int buflen, UCHAR *buf, int *attrlen)
{
    UINT4 checksum;
    nr_stun_attr_fingerprint *fingerprint = data;
    nr_stun_message_header *header = (nr_stun_message_header*)buf;

    /* the length must include the FINGERPRINT attribute when computing
     * the fingerprint */
    header->length = ntohs(header->length);
    header->length += 8;  /* Fingerprint */
    header->length = htons(header->length);

    if (r_crc32((char*)buf, offset, &checksum)) {
        r_log(NR_LOG_STUN, LOG_WARNING, "Unable to compute fingerprint");
        return R_FAILED;
    }

    fingerprint->checksum = checksum ^ 0x5354554e;

    r_log(NR_LOG_STUN, LOG_DEBUG, "Computed FINGERPRINT %08x", fingerprint->checksum);

    fingerprint->valid = 1;
    return nr_stun_attr_codec_UINT4.encode(attr_info, &fingerprint->checksum, offset, buflen, buf, attrlen);
}

static int
nr_stun_attr_codec_fingerprint_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
    int r,_status;
    nr_stun_attr_fingerprint *fingerprint = data;
    nr_stun_message_header *header = (nr_stun_message_header*)buf;
    int length;
    UINT4 checksum;

    if ((r=nr_stun_attr_codec_UINT4.decode(attr_info, attrlen, buf, offset, buflen, &fingerprint->checksum)))
        ABORT(r);

    offset -= 4; /* rewind to before the length and type fields */

    /* the length must include the FINGERPRINT attribute when computing
     * the fingerprint */
    length = offset;  /* right before FINGERPRINT */
    length -= sizeof(*header); /* remove header length */
    length += 8;  /* add length of Fingerprint */
    header->length = htons(length);

    /* make sure FINGERPRINT is final attribute in message */
    assert(length + sizeof(*header) == buflen);

    if (r_crc32((char*)buf, offset, &checksum)) {
        r_log(NR_LOG_STUN, LOG_WARNING, "Unable to compute fingerprint");
        ABORT(R_FAILED);
    }

    fingerprint->valid = (fingerprint->checksum == (checksum ^ 0x5354554e));

    r_log(NR_LOG_STUN, LOG_DEBUG, "Computed FINGERPRINT %08x", (checksum ^ 0x5354554e));
    if (! fingerprint->valid)
        r_log(NR_LOG_STUN, LOG_WARNING, "Invalid FINGERPRINT %08x", fingerprint->checksum);

    _status=0;
  abort:
    return _status;
}

nr_stun_attr_codec nr_stun_attr_codec_fingerprint = {
    "fingerprint",
    nr_stun_attr_codec_fingerprint_print,
    nr_stun_attr_codec_fingerprint_encode,
    nr_stun_attr_codec_fingerprint_decode
};

static int
nr_stun_attr_codec_flag_print(nr_stun_attr_info *attr_info, char *msg, void *data)
{
    r_log(NR_LOG_STUN, LOG_DEBUG, "%s %s: on", msg, attr_info->name);
    return 0;
}

static int
nr_stun_attr_codec_flag_encode(nr_stun_attr_info *attr_info, void *data, int offset, int buflen, UCHAR *buf, int *attrlen)
{
    int start = offset;

    if (nr_stun_encode_htons(attr_info->type  , buflen, buf, &offset)
     || nr_stun_encode_htons(0                , buflen, buf, &offset))
        return R_FAILED;

    *attrlen = offset - start;

    return 0;
}

static int
nr_stun_attr_codec_flag_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
    if (attrlen != 0) {
        r_log(NR_LOG_STUN, LOG_WARNING, "Illegal flag length: %d", attrlen);
        return R_FAILED;
    }

    return 0;
}

nr_stun_attr_codec nr_stun_attr_codec_flag = {
    "flag",
    nr_stun_attr_codec_flag_print,
    nr_stun_attr_codec_flag_encode,
    nr_stun_attr_codec_flag_decode
};

static int
nr_stun_attr_codec_message_integrity_print(nr_stun_attr_info *attr_info, char *msg, void *data)
{
    nr_stun_attr_message_integrity *integrity = data;
    r_dump(NR_LOG_STUN, LOG_DEBUG, attr_info->name, (char*)integrity->hash, sizeof(integrity->hash));
    return 0;
}

static int
nr_stun_compute_message_integrity(UCHAR *buf, int offset, UCHAR *password, int passwordlen, UCHAR *computedHMAC)
{
    int r,_status;
    UINT2 hold;
    UINT2 length;
    nr_stun_message_header *header;

    r_log(NR_LOG_STUN, LOG_DEBUG, "Computing MESSAGE-INTEGRITY");

    header = (nr_stun_message_header*)buf;
    hold = header->length;

    /* adjust the length of the message */
    length = offset;
    length -= sizeof(*header);
    length += 24; /* for MESSAGE-INTEGRITY attribute */
    header->length = htons(length);

    if ((r=nr_crypto_hmac_sha1((UCHAR*)password, passwordlen,
                               buf, offset, computedHMAC)))
        ABORT(r);

    r_dump(NR_LOG_STUN, LOG_DEBUG, "Computed MESSAGE-INTEGRITY ", (char*)computedHMAC, 20);

    _status=0;
 abort:
    header->length = hold;
    return _status;
}

static int
nr_stun_attr_codec_message_integrity_encode(nr_stun_attr_info *attr_info, void *data, int offset, int buflen, UCHAR *buf, int *attrlen)
{
    int start = offset;
    nr_stun_attr_message_integrity *integrity = data;

    if (nr_stun_compute_message_integrity(buf, offset, integrity->password, integrity->passwordlen, integrity->hash))
        return R_FAILED;

    if (nr_stun_encode_htons(attr_info->type                         , buflen, buf, &offset)
     || nr_stun_encode_htons(sizeof(integrity->hash)                 , buflen, buf, &offset)
     || nr_stun_encode(integrity->hash, sizeof(integrity->hash)      , buflen, buf, &offset))
        return R_FAILED;

    *attrlen = offset - start;

    return 0;
}

static int
nr_stun_attr_codec_message_integrity_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
    int _status;
    int start;
    nr_stun_attr_message_integrity *result = data;
    UCHAR computedHMAC[20];

    result->valid = 0;

    if (attrlen != 20) {
        r_log(NR_LOG_STUN, LOG_WARNING, "%s must be 20 bytes, not %d", attr_info->name, attrlen);
        ABORT(R_FAILED);
    }

    start = offset - 4; /* rewind to before the length and type fields */
    if (start < 0)
        ABORT(R_INTERNAL);

    if (nr_stun_decode(attrlen, buf, buflen, &offset, result->hash))
        ABORT(R_FAILED);

    if (result->unknown_user) {
        result->valid = 0;
    }
    else {
        if (nr_stun_compute_message_integrity(buf, start, result->password, result->passwordlen, computedHMAC))
            ABORT(R_FAILED);

        assert(sizeof(computedHMAC) == sizeof(result->hash));

        result->valid = (memcmp(computedHMAC, result->hash, 20) == 0);
    }

   _status=0;
 abort:
   return _status;
}

nr_stun_attr_codec nr_stun_attr_codec_message_integrity = {
    "message_integrity",
    nr_stun_attr_codec_message_integrity_print,
    nr_stun_attr_codec_message_integrity_encode,
    nr_stun_attr_codec_message_integrity_decode
};

static int
nr_stun_attr_codec_noop_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
    return SKIP_ATTRIBUTE_DECODE;
}

nr_stun_attr_codec nr_stun_attr_codec_noop = {
    "NOOP",
    0,  /* ignore, never print these attributes */
    0,  /* ignore, never encode these attributes */
    nr_stun_attr_codec_noop_decode
};

static int
nr_stun_attr_codec_quoted_string_print(nr_stun_attr_info *attr_info, char *msg, void *data)
{
    r_log(NR_LOG_STUN, LOG_DEBUG, "%s %s: %s",
                          msg, attr_info->name, (char*)data);
    return 0;
}

static int
nr_stun_attr_codec_quoted_string_encode(nr_stun_attr_info *attr_info, void *data, int offset, int buflen, UCHAR *buf, int *attrlen)
{
//TODO: !nn! syntax check, conversion if not quoted already?
//We'll just restrict this in the API -- EKR
    return nr_stun_attr_codec_string.encode(attr_info, data, offset, buflen, buf, attrlen);
}

static int
nr_stun_attr_codec_quoted_string_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
//TODO: !nn! I don't see any need to unquote this but we may
//find one later -- EKR
    return nr_stun_attr_codec_string.decode(attr_info, attrlen, buf, offset, buflen, data);
}

nr_stun_attr_codec nr_stun_attr_codec_quoted_string = {
    "quoted_string",
    nr_stun_attr_codec_quoted_string_print,
    nr_stun_attr_codec_quoted_string_encode,
    nr_stun_attr_codec_quoted_string_decode
};

static int
nr_stun_attr_codec_string_print(nr_stun_attr_info *attr_info, char *msg, void *data)
{
    r_log(NR_LOG_STUN, LOG_DEBUG, "%s %s: %s",
                          msg, attr_info->name, (char*)data);
    return 0;
}

static int
nr_stun_attr_codec_string_encode(nr_stun_attr_info *attr_info, void *data, int offset, int buflen, UCHAR *buf, int *attrlen)
{
    int start = offset;
    char *str = data;
    int length = strlen(str);

    if (nr_stun_encode_htons(attr_info->type  , buflen, buf, &offset)
     || nr_stun_encode_htons(length           , buflen, buf, &offset)
     || nr_stun_encode((UCHAR*)str, length    , buflen, buf, &offset))
        return R_FAILED;

    *attrlen = offset - start;

    return 0;
}

static int
nr_stun_attr_codec_string_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
    int _status;
    char *result = data;

    /* actual enforcement of the specific string size happens elsewhere */
    if (attrlen >= NR_STUN_MAX_STRING_SIZE) {
        r_log(NR_LOG_STUN, LOG_WARNING, "String is too large: %d bytes", attrlen);
        ABORT(R_FAILED);
    }

    if (nr_stun_decode(attrlen, buf, buflen, &offset, (UCHAR*)result))
        ABORT(R_FAILED);
    result[attrlen] = '\0'; /* just to be nice */

    if (strlen(result) != attrlen) {
        /* stund 0.96 sends a final null in the Server attribute, so
         * only error if the null appears anywhere else in a string */
        if (strlen(result) != attrlen-1) {
            r_log(NR_LOG_STUN, LOG_WARNING, "Error in string: %zd/%d", strlen(result), attrlen);
            ABORT(R_FAILED);
        }
    }

    _status = 0;
  abort:
    return _status;
}

nr_stun_attr_codec nr_stun_attr_codec_string = {
    "string",
    nr_stun_attr_codec_string_print,
    nr_stun_attr_codec_string_encode,
    nr_stun_attr_codec_string_decode
};

static int
nr_stun_attr_codec_unknown_attributes_print(nr_stun_attr_info *attr_info, char *msg, void *data)
{
    nr_stun_attr_unknown_attributes *unknown_attributes = data;
    char type[9];
    char str[64 + (NR_STUN_MAX_UNKNOWN_ATTRIBUTES * sizeof(type))];
    int i;

    snprintf(str, sizeof(str), "%s %s:", msg, attr_info->name);
    for (i = 0; i < unknown_attributes->num_attributes; ++i) {
        snprintf(type, sizeof(type), "%s 0x%04x", ((i>0)?",":""), unknown_attributes->attribute[i]);
        strlcat(str, type, sizeof(str));
    }

    r_log(NR_LOG_STUN, LOG_DEBUG, "%s", str);
    return 0;
}

static int
nr_stun_attr_codec_unknown_attributes_encode(nr_stun_attr_info *attr_info, void *data, int offset, int buflen, UCHAR *buf, int *attrlen)
{
    int _status;
    int start = offset;
    nr_stun_attr_unknown_attributes *unknown_attributes = data;
    int length = (2 * unknown_attributes->num_attributes);
    int i;

    if (unknown_attributes->num_attributes > NR_STUN_MAX_UNKNOWN_ATTRIBUTES) {
        r_log(NR_LOG_STUN, LOG_WARNING, "Too many UNKNOWN-ATTRIBUTES: %d", unknown_attributes->num_attributes);
        ABORT(R_FAILED);
    }

    if (nr_stun_encode_htons(attr_info->type  , buflen, buf, &offset)
     || nr_stun_encode_htons(length           , buflen, buf, &offset))
        ABORT(R_FAILED);

    for (i = 0; i < unknown_attributes->num_attributes; ++i) {
        if (nr_stun_encode_htons(unknown_attributes->attribute[i], buflen, buf, &offset))
            ABORT(R_FAILED);
    }

    *attrlen = offset - start;

    _status = 0;
  abort:
    return _status;
}

static int
nr_stun_attr_codec_unknown_attributes_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
    int _status;
    nr_stun_attr_unknown_attributes *unknown_attributes = data;
    int i;
    UINT2 *a;

    if ((attrlen % 4) != 0) {
        r_log(NR_LOG_STUN, LOG_WARNING, "Attribute is illegal size: %d", attrlen);
        ABORT(R_REJECTED);
    }

    unknown_attributes->num_attributes = attrlen / 2;

    if (unknown_attributes->num_attributes > NR_STUN_MAX_UNKNOWN_ATTRIBUTES) {
        r_log(NR_LOG_STUN, LOG_WARNING, "Too many UNKNOWN-ATTRIBUTES: %d", unknown_attributes->num_attributes);
        ABORT(R_REJECTED);
    }

    for (i = 0; i < unknown_attributes->num_attributes; ++i) {
        a = &(unknown_attributes->attribute[i]);
        if (nr_stun_decode_htons(buf, buflen, &offset, a))
            return R_FAILED;
    }

    _status = 0;
  abort:
    return _status;
}

nr_stun_attr_codec nr_stun_attr_codec_unknown_attributes = {
    "unknown_attributes",
    nr_stun_attr_codec_unknown_attributes_print,
    nr_stun_attr_codec_unknown_attributes_encode,
    nr_stun_attr_codec_unknown_attributes_decode
};

static int
nr_stun_attr_codec_xor_mapped_address_print(nr_stun_attr_info *attr_info, char *msg, void *data)
{
    nr_stun_attr_xor_mapped_address *xor_mapped_address = data;
    r_log(NR_LOG_STUN, LOG_DEBUG, "%s %s: %s (unmasked) %s (masked)",
                          msg, attr_info->name,
                          xor_mapped_address->unmasked.as_string,
                          xor_mapped_address->masked.as_string);
    return 0;
}

static int
nr_stun_attr_codec_xor_mapped_address_encode(nr_stun_attr_info *attr_info, void *data, int offset, int buflen, UCHAR *buf, int *attrlen)
{
    nr_stun_attr_xor_mapped_address *xor_mapped_address = data;
    nr_stun_message_header *header = (nr_stun_message_header*)buf;
    UINT4 magic_cookie;

    r_log(NR_LOG_STUN, LOG_DEBUG, "Unmasked XOR-MAPPED-ADDRESS = %s", xor_mapped_address->unmasked.as_string);

    /* this needs to be the magic cookie in the header and not
     * the MAGIC_COOKIE constant because if we're talking to
     * older servers (that don't have a magic cookie) they use
     * message ID for this */
    magic_cookie = ntohl(header->magic_cookie);

    nr_stun_xor_mapped_address(magic_cookie, &xor_mapped_address->unmasked, &xor_mapped_address->masked);

    r_log(NR_LOG_STUN, LOG_DEBUG, "Masked XOR-MAPPED-ADDRESS = %s", xor_mapped_address->masked.as_string);

    if (nr_stun_attr_codec_addr.encode(attr_info, &xor_mapped_address->masked, offset, buflen, buf, attrlen))
        return R_FAILED;

    return 0;
}

static int
nr_stun_attr_codec_xor_mapped_address_decode(nr_stun_attr_info *attr_info, int attrlen, UCHAR *buf, int offset, int buflen, void *data)
{
    int r,_status;
    nr_stun_attr_xor_mapped_address *xor_mapped_address = data;
    nr_stun_message_header *header = (nr_stun_message_header*)buf;
    UINT4 magic_cookie;

    if ((r=nr_stun_attr_codec_addr.decode(attr_info, attrlen, buf, offset, buflen, &xor_mapped_address->masked)))
        ABORT(r);

    r_log(NR_LOG_STUN, LOG_DEBUG, "Masked XOR-MAPPED-ADDRESS = %s", xor_mapped_address->masked.as_string);

    /* this needs to be the magic cookie in the header and not
     * the MAGIC_COOKIE constant because if we're talking to
     * older servers (that don't have a magic cookie) they use
     * message ID for this */
    magic_cookie = ntohl(header->magic_cookie);

    nr_stun_xor_mapped_address(magic_cookie, &xor_mapped_address->masked, &xor_mapped_address->unmasked);

    r_log(NR_LOG_STUN, LOG_DEBUG, "Unmasked XOR-MAPPED-ADDRESS = %s", xor_mapped_address->unmasked.as_string);

    _status = 0;
  abort:
    return _status;
}

nr_stun_attr_codec nr_stun_attr_codec_xor_mapped_address = {
    "xor_mapped_address",
    nr_stun_attr_codec_xor_mapped_address_print,
    nr_stun_attr_codec_xor_mapped_address_encode,
    nr_stun_attr_codec_xor_mapped_address_decode
};

nr_stun_attr_codec nr_stun_attr_codec_old_xor_mapped_address = {
    "xor_mapped_address",
    nr_stun_attr_codec_xor_mapped_address_print,
    0, /* never encode this type */
    nr_stun_attr_codec_xor_mapped_address_decode
};

nr_stun_attr_codec nr_stun_attr_codec_xor_peer_address = {
    "xor_peer_address",
    nr_stun_attr_codec_xor_mapped_address_print,
    nr_stun_attr_codec_xor_mapped_address_encode,
    nr_stun_attr_codec_xor_mapped_address_decode
};

#define NR_ADD_STUN_ATTRIBUTE(type, name, codec, illegal) \
 { (type), (name), &(codec), illegal },

#define NR_ADD_STUN_ATTRIBUTE_IGNORE(type, name) \
 { (type), (name), &nr_stun_attr_codec_noop, 0 },


static nr_stun_attr_info attrs[] = {
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_ALTERNATE_SERVER, "ALTERNATE-SERVER", nr_stun_attr_codec_addr, 0)
#ifdef USE_STUND_0_96
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_OLD_CHANGE_REQUEST, "CHANGE-REQUEST", nr_stun_attr_codec_UINT4, 0)
#endif
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_ERROR_CODE, "ERROR-CODE", nr_stun_attr_codec_error_code, nr_stun_attr_error_code_illegal)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_FINGERPRINT, "FINGERPRINT", nr_stun_attr_codec_fingerprint, 0)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_MAPPED_ADDRESS, "MAPPED-ADDRESS", nr_stun_attr_codec_addr, 0)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_MESSAGE_INTEGRITY, "MESSAGE-INTEGRITY", nr_stun_attr_codec_message_integrity, 0)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_NONCE, "NONCE", nr_stun_attr_codec_quoted_string, nr_stun_attr_nonce_illegal)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_REALM, "REALM", nr_stun_attr_codec_quoted_string, nr_stun_attr_realm_illegal)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_SERVER, "SERVER", nr_stun_attr_codec_string, nr_stun_attr_server_illegal)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_UNKNOWN_ATTRIBUTES, "UNKNOWN-ATTRIBUTES", nr_stun_attr_codec_unknown_attributes, 0)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_USERNAME, "USERNAME", nr_stun_attr_codec_string, nr_stun_attr_username_illegal)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_XOR_MAPPED_ADDRESS, "XOR-MAPPED-ADDRESS", nr_stun_attr_codec_xor_mapped_address, 0)

#ifdef USE_ICE
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_ICE_CONTROLLED, "ICE-CONTROLLED", nr_stun_attr_codec_UINT8, 0)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_ICE_CONTROLLING, "ICE-CONTROLLING", nr_stun_attr_codec_UINT8, 0)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_PRIORITY, "PRIORITY", nr_stun_attr_codec_UINT4, 0)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_USE_CANDIDATE, "USE-CANDIDATE", nr_stun_attr_codec_flag, 0)
#endif

#ifdef USE_TURN
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_DATA, "DATA", nr_stun_attr_codec_data, 0)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_LIFETIME, "LIFETIME", nr_stun_attr_codec_UINT4, 0)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_XOR_RELAY_ADDRESS, "XOR-RELAY-ADDRESS", nr_stun_attr_codec_xor_mapped_address, 0)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_XOR_PEER_ADDRESS, "XOR-PEER-ADDRESS", nr_stun_attr_codec_xor_peer_address, 0)
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_REQUESTED_TRANSPORT, "REQUESTED-TRANSPORT", nr_stun_attr_codec_UCHAR, 0)
#endif /* USE_TURN */

   /* for backwards compatibilty */
   NR_ADD_STUN_ATTRIBUTE(NR_STUN_ATTR_OLD_XOR_MAPPED_ADDRESS, "Old XOR-MAPPED-ADDRESS", nr_stun_attr_codec_old_xor_mapped_address, 0)
#ifdef USE_RFC_3489_BACKWARDS_COMPATIBLE
   NR_ADD_STUN_ATTRIBUTE_IGNORE(NR_STUN_ATTR_OLD_RESPONSE_ADDRESS, "RESPONSE-ADDRESS")
   NR_ADD_STUN_ATTRIBUTE_IGNORE(NR_STUN_ATTR_OLD_SOURCE_ADDRESS, "SOURCE-ADDRESS")
   NR_ADD_STUN_ATTRIBUTE_IGNORE(NR_STUN_ATTR_OLD_CHANGED_ADDRESS, "CHANGED-ADDRESS")
   NR_ADD_STUN_ATTRIBUTE_IGNORE(NR_STUN_ATTR_OLD_PASSWORD, "PASSWORD")
#endif /* USE_RFC_3489_BACKWARDS_COMPATIBLE */
};


int
nr_stun_find_attr_info(UINT2 type, nr_stun_attr_info **info)
{
    int _status;
    int i;

    *info = 0;
    for (i = 0; i < sizeof(attrs)/sizeof(*attrs); ++i) {
        if (type == attrs[i].type) {
            *info = &attrs[i];
            break;
        }
    }

    if (*info == 0)
        ABORT(R_NOT_FOUND);

    _status=0;
  abort:
    return(_status);
}

int
nr_stun_fix_attribute_ordering(nr_stun_message *msg)
{
    nr_stun_message_attribute *message_integrity;
    nr_stun_message_attribute *fingerprint;

    /* 2nd to the last */
    if (nr_stun_message_has_attribute(msg, NR_STUN_ATTR_MESSAGE_INTEGRITY, &message_integrity)) {
        TAILQ_REMOVE(&msg->attributes, message_integrity, entry);
        TAILQ_INSERT_TAIL(&msg->attributes, message_integrity, entry);
    }

    /* last */
    if (nr_stun_message_has_attribute(msg, NR_STUN_ATTR_FINGERPRINT, &fingerprint)) {
        TAILQ_REMOVE(&msg->attributes, fingerprint, entry);
        TAILQ_INSERT_TAIL(&msg->attributes, fingerprint, entry);
    }

    return 0;
}

#ifdef SANITY_CHECKS
static void sanity_check_encoding_stuff(nr_stun_message *msg)
{
    nr_stun_message_attribute *attr = 0;
    int padding_bytes;
    int l;

    r_log(NR_LOG_STUN, LOG_DEBUG, "Starting to sanity check encoding");

    l = 0;
    TAILQ_FOREACH(attr, &msg->attributes, entry) {
        padding_bytes = 0;
        if ((attr->length % 4) != 0) {
            padding_bytes = 4 - (attr->length % 4);
        }
        assert(attr->length == (attr->encoding_length - (4 + padding_bytes)));
        assert(((void*)attr->encoding) == (msg->buffer + 20 + l));
        l += attr->encoding_length;
        assert((l % 4) == 0);
    }
    assert(l == msg->header.length);
}
#endif /* SANITY_CHECKS */


int
nr_stun_encode_message(nr_stun_message *msg)
{
    int r,_status;
    int length_offset;
    int length_offset_hold;
    nr_stun_attr_info *attr_info;
    nr_stun_message_attribute *attr;
    int padding_bytes;

    r_log(NR_LOG_STUN, LOG_DEBUG, "Encoding STUN message");

    nr_stun_fix_attribute_ordering(msg);

    msg->name = nr_stun_msg_type(msg->header.type);
    msg->length = 0;
    msg->header.length = 0;

    if ((r=nr_stun_encode_htons(msg->header.type, sizeof(msg->buffer), msg->buffer, &msg->length)))
        ABORT(r);
    if (msg->name)
        r_log(NR_LOG_STUN, LOG_DEBUG, "Encoded MsgType: %s", msg->name);
    else
        r_log(NR_LOG_STUN, LOG_DEBUG, "Encoded MsgType: 0x%03x", msg->header.type);

    /* grab the offset to be used later to re-write the header length field */
    length_offset_hold = msg->length;

    if ((r=nr_stun_encode_htons(msg->header.length, sizeof(msg->buffer), msg->buffer, &msg->length)))
        ABORT(r);

    if ((r=nr_stun_encode_htonl(msg->header.magic_cookie, sizeof(msg->buffer), msg->buffer, &msg->length)))
        ABORT(r);
    r_log(NR_LOG_STUN, LOG_DEBUG, "Encoded Cookie: %08x", msg->header.magic_cookie);

    if ((r=nr_stun_encode((UCHAR*)(&msg->header.id), sizeof(msg->header.id), sizeof(msg->buffer), msg->buffer, &msg->length)))
        ABORT(r);
    r_dump(NR_LOG_STUN, LOG_DEBUG, "Encoded ID", (void*)&msg->header.id, sizeof(msg->header.id));

    TAILQ_FOREACH(attr, &msg->attributes, entry) {
        if ((r=nr_stun_find_attr_info(attr->type, &attr_info))) {
            r_log(NR_LOG_STUN, LOG_WARNING, "Unrecognized attribute: 0x%04x", attr->type);
            ABORT(R_INTERNAL);
        }

        attr_info->name = attr_info->name;
        attr->type_name = attr_info->codec->name;
        attr->encoding = (nr_stun_encoded_attribute*)&msg->buffer[msg->length];

        if (attr_info->codec->encode != 0) {
            if ((r=attr_info->codec->encode(attr_info, &attr->u, msg->length, sizeof(msg->buffer), msg->buffer, &attr->encoding_length))) {
                r_log(NR_LOG_STUN, LOG_WARNING, "Unable to encode %s", attr_info->name);
                ABORT(r);
            }

            msg->length += attr->encoding_length;
            attr->length = attr->encoding_length - 4;  /* -4 for type and length fields */

            if (attr_info->illegal) {
                if ((r=attr_info->illegal(attr_info, attr->length, &attr->u)))
                    ABORT(r);
            }

            attr_info->codec->print(attr_info, "Encoded", &attr->u);

            if ((attr->length % 4) == 0) {
                padding_bytes = 0;
            }
            else {
                padding_bytes = 4 - (attr->length % 4);
                nr_stun_encode((UCHAR*)"\0\0\0\0", padding_bytes, sizeof(msg->buffer), msg->buffer, &msg->length);
                attr->encoding_length += padding_bytes;
            }

            msg->header.length += attr->encoding_length;
            length_offset = length_offset_hold;
            (void)nr_stun_encode_htons(msg->header.length, sizeof(msg->buffer), msg->buffer, &length_offset);
        }
    }

    r_log(NR_LOG_STUN, LOG_DEBUG, "Encoded Length: %d", msg->header.length);

    assert(msg->length < NR_STUN_MAX_MESSAGE_SIZE);

#ifdef SANITY_CHECKS
    sanity_check_encoding_stuff(msg);
#endif /* SANITY_CHECKS */

    _status=0;
abort:
    return _status;
}

int
nr_stun_decode_message(nr_stun_message *msg, int (*get_password)(void *arg, nr_stun_message *msg, Data **password), void *arg)
{
    int r,_status;
    int offset;
    int size;
    int padding_bytes;
    nr_stun_message_attribute *attr;
    nr_stun_attr_info *attr_info;
    Data *password;

    r_log(NR_LOG_STUN, LOG_DEBUG, "Parsing STUN message of %d bytes", msg->length);

    if (!TAILQ_EMPTY(&msg->attributes))
        ABORT(R_BAD_ARGS);

    if (sizeof(nr_stun_message_header) > msg->length) {
       r_log(NR_LOG_STUN, LOG_WARNING, "Message too small");
       ABORT(R_FAILED);
    }

    memcpy(&msg->header, msg->buffer, sizeof(msg->header));
    msg->header.type = ntohs(msg->header.type);
    msg->header.length = ntohs(msg->header.length);
    msg->header.magic_cookie = ntohl(msg->header.magic_cookie);

    msg->name = nr_stun_msg_type(msg->header.type);

    if (msg->name)
        r_log(NR_LOG_STUN, LOG_DEBUG, "Parsed MsgType: %s", msg->name);
    else
        r_log(NR_LOG_STUN, LOG_DEBUG, "Parsed MsgType: 0x%03x", msg->header.type);
    r_log(NR_LOG_STUN, LOG_DEBUG, "Parsed Length: %d", msg->header.length);
    r_log(NR_LOG_STUN, LOG_DEBUG, "Parsed Cookie: %08x", msg->header.magic_cookie);
    r_dump(NR_LOG_STUN, LOG_DEBUG, "Parsed ID", (void*)&msg->header.id, sizeof(msg->header.id));

    if (msg->header.length + sizeof(msg->header) != msg->length) {
       r_log(NR_LOG_STUN, LOG_WARNING, "Inconsistent message header length: %d/%d",
                                        msg->header.length, msg->length);
       ABORT(R_FAILED);
    }

    size = msg->header.length;

    if ((size % 4) != 0) {
       r_log(NR_LOG_STUN, LOG_WARNING, "Illegal message size: %d", msg->header.length);
       ABORT(R_FAILED);
    }

    offset = sizeof(msg->header);

    while (size > 0) {
        r_log(NR_LOG_STUN, LOG_DEBUG, "size = %d", size);

        if (size < 4) {
           r_log(NR_LOG_STUN, LOG_WARNING, "Illegal message length: %d", size);
           ABORT(R_FAILED);
        }

        if ((r=nr_stun_message_attribute_create(msg, &attr)))
            ABORT(R_NO_MEMORY);

        attr->encoding          = (nr_stun_encoded_attribute*)&msg->buffer[offset];
        attr->type              = ntohs(attr->encoding->type);
        attr->length            = ntohs(attr->encoding->length);
        attr->encoding_length   = attr->length + 4;

        if ((attr->length % 4) != 0) {
            padding_bytes = 4 - (attr->length % 4);
            attr->encoding_length += padding_bytes;
        }

        if ((attr->encoding_length) > size) {
           r_log(NR_LOG_STUN, LOG_WARNING, "Attribute length larger than remaining message size: %d/%d", attr->encoding_length, size);
           ABORT(R_FAILED);
        }

        if ((r=nr_stun_find_attr_info(attr->type, &attr_info))) {
            if (attr->type <= 0x7FFF)
                ++msg->comprehension_required_unknown_attributes;
            else
                ++msg->comprehension_optional_unknown_attributes;
            r_log(NR_LOG_STUN, LOG_INFO, "Unrecognized attribute: 0x%04x", attr->type);
        }
        else {
            attr_info->name = attr_info->name;
            attr->type_name = attr_info->codec->name;

            if (attr->type == NR_STUN_ATTR_MESSAGE_INTEGRITY) {
                if (get_password && get_password(arg, msg, &password) == 0) {
                    if (password->len > sizeof(attr->u.message_integrity.password)) {
                        r_log(NR_LOG_STUN, LOG_WARNING, "Password too long: %d bytes", password->len);
                        ABORT(R_FAILED);
                    }

                    memcpy(attr->u.message_integrity.password, password->data, password->len);
                    attr->u.message_integrity.passwordlen = password->len;
                }
                else {
                    /* set to user "not found" */
                    attr->u.message_integrity.unknown_user = 1;
                }
            }
            else if (attr->type == NR_STUN_ATTR_OLD_XOR_MAPPED_ADDRESS) {
                attr->type = NR_STUN_ATTR_XOR_MAPPED_ADDRESS;
                r_log(NR_LOG_STUN, LOG_INFO, "Translating obsolete XOR-MAPPED-ADDRESS type");
            }

            if ((r=attr_info->codec->decode(attr_info, attr->length, msg->buffer, offset+4, msg->length, &attr->u))) {
                if (r == SKIP_ATTRIBUTE_DECODE) {
                    r_log(NR_LOG_STUN, LOG_INFO, "Skipping %s", attr_info->name);
                }
                else {
                    r_log(NR_LOG_STUN, LOG_WARNING, "Unable to parse %s", attr_info->name);
                }

                attr->invalid = 1;
            }
            else {
                attr_info->codec->print(attr_info, "Parsed", &attr->u);

#ifdef USE_STUN_PEDANTIC
                r_log(NR_LOG_STUN, LOG_DEBUG, "Before pedantic attr_info checks");
                if (attr_info->illegal) {
                    if ((r=attr_info->illegal(attr_info, attr->length, &attr->u))) {
                        r_log(NR_LOG_STUN, LOG_WARNING, "Failed pedantic attr_info checks");
                        ABORT(r);
                    }
                }
                r_log(NR_LOG_STUN, LOG_DEBUG, "After pedantic attr_info checks");
#endif /* USE_STUN_PEDANTIC */
            }
        }

        offset += attr->encoding_length;
        size -= attr->encoding_length;
    }

#ifdef SANITY_CHECKS
    sanity_check_encoding_stuff(msg);
#endif /* SANITY_CHECKS */

    _status=0;
  abort:
    return _status;
}

