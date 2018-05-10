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


static char *RCSSTRING __UNUSED__="$Id: stun_hint.c,v 1.2 2008/04/28 18:21:30 ekr Exp $";


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


/* returns 0 if it's not a STUN message
 *         1 if it's likely to be a STUN message
 *         2 if it's super likely to be a STUN message
 *         3 if it really is a STUN message */
int
nr_is_stun_message(UCHAR *buf, size_t len)
{
   const UINT4 cookie = htonl(NR_STUN_MAGIC_COOKIE);
   const UINT4 cookie2 = htonl(NR_STUN_MAGIC_COOKIE2);
#if 0
   nr_stun_message msg;
#endif
   UINT2 type;
   nr_stun_encoded_attribute* attr;
   unsigned int attrLen;
   int atrType;

   if (sizeof(nr_stun_message_header) > len)
       return 0;

   if ((buf[0] & (0x80|0x40)) != 0)
       return 0;

   memcpy(&type, buf, 2);
   type = ntohs(type);

   switch (type) {
   case NR_STUN_MSG_BINDING_REQUEST:
   case NR_STUN_MSG_BINDING_INDICATION:
   case NR_STUN_MSG_BINDING_RESPONSE:
   case NR_STUN_MSG_BINDING_ERROR_RESPONSE:

#ifdef USE_TURN
    case NR_STUN_MSG_ALLOCATE_REQUEST:
    case NR_STUN_MSG_ALLOCATE_RESPONSE:
    case NR_STUN_MSG_ALLOCATE_ERROR_RESPONSE:
    case NR_STUN_MSG_REFRESH_REQUEST:
    case NR_STUN_MSG_REFRESH_RESPONSE:
    case NR_STUN_MSG_REFRESH_ERROR_RESPONSE:
    case NR_STUN_MSG_PERMISSION_REQUEST:
    case NR_STUN_MSG_PERMISSION_RESPONSE:
    case NR_STUN_MSG_PERMISSION_ERROR_RESPONSE:
    case NR_STUN_MSG_CHANNEL_BIND_REQUEST:
    case NR_STUN_MSG_CHANNEL_BIND_RESPONSE:
    case NR_STUN_MSG_CHANNEL_BIND_ERROR_RESPONSE:
    case NR_STUN_MSG_SEND_INDICATION:
    case NR_STUN_MSG_DATA_INDICATION:
#ifdef NR_STUN_MSG_CONNECT_REQUEST
    case NR_STUN_MSG_CONNECT_REQUEST:
#endif
#ifdef NR_STUN_MSG_CONNECT_RESPONSE
    case NR_STUN_MSG_CONNECT_RESPONSE:
#endif
#ifdef NR_STUN_MSG_CONNECT_ERROR_RESPONSE
    case NR_STUN_MSG_CONNECT_ERROR_RESPONSE:
#endif
#ifdef NR_STUN_MSG_CONNECT_STATUS_INDICATION
    case NR_STUN_MSG_CONNECT_STATUS_INDICATION:
#endif
#endif /* USE_TURN */

        /* ok so far, continue */
        break;
   default:
        return 0;
        break;
   }

   if (!memcmp(&cookie2, &buf[4], sizeof(UINT4))) {
       /* return here because if it's an old-style message then there will
        * not be a fingerprint in the message */
       return 1;
   }

   if (memcmp(&cookie, &buf[4], sizeof(UINT4)))
       return 0;

   /* the magic cookie was right, so it's pretty darn likely that what we've
    * got here is a STUN message */

   attr = (nr_stun_encoded_attribute*)(buf + (len - 8));
   attrLen = ntohs(attr->length);
   atrType = ntohs(attr->type);

   if (atrType != NR_STUN_ATTR_FINGERPRINT || attrLen != 4)
       return 1;

   /* the fingerprint is in the right place and looks sane, so we can be quite
    * sure we've got a STUN message */

#if 0
/* nevermind this check ... there's a reasonable chance that a NAT has modified
 * the message (and thus the fingerprint check will fail), but it's still an
 * otherwise-perfectly-good STUN message, so skip the check since we're going
 * to return "true" whether the check succeeds or fails */

   if (nr_stun_parse_attr_UINT4(buf + (len - 4), attrLen, &msg.fingerprint))
       return 2;


   if (nr_stun_compute_fingerprint(buf, len - 8, &computedFingerprint))
       return 2;

   if (msg.fingerprint.number != computedFingerprint)
       return 2;

   /* and the fingerprint is good, so it's gotta be a STUN message */
#endif

   return 3;
}

int
nr_is_stun_request_message(UCHAR *buf, size_t len)
{
   UINT2 type;

   if (sizeof(nr_stun_message_header) > len)
       return 0;

   if (!nr_is_stun_message(buf, len))
       return 0;

   memcpy(&type, buf, 2);
   type = ntohs(type);

   return NR_STUN_GET_TYPE_CLASS(type) == NR_CLASS_REQUEST;
}

int
nr_is_stun_indication_message(UCHAR *buf, size_t len)
{
   UINT2 type;

   if (sizeof(nr_stun_message_header) > len)
       return 0;

   if (!nr_is_stun_message(buf, len))
       return 0;

   memcpy(&type, buf, 2);
   type = ntohs(type);

   return NR_STUN_GET_TYPE_CLASS(type) == NR_CLASS_INDICATION;
}

int
nr_is_stun_response_message(UCHAR *buf, size_t len)
{
   UINT2 type;

   if (sizeof(nr_stun_message_header) > len)
       return 0;

   if (!nr_is_stun_message(buf, len))
       return 0;

   memcpy(&type, buf, 2);
   type = ntohs(type);

   return NR_STUN_GET_TYPE_CLASS(type) == NR_CLASS_RESPONSE
       || NR_STUN_GET_TYPE_CLASS(type) == NR_CLASS_ERROR_RESPONSE;
}

int
nr_has_stun_cookie(UCHAR *buf, size_t len)
{
   static UINT4 cookie;

   cookie = htonl(NR_STUN_MAGIC_COOKIE);

   if (sizeof(nr_stun_message_header) > len)
       return 0;

   if (memcmp(&cookie, &buf[4], sizeof(UINT4)))
       return 0;

   return 1;
}

int
nr_stun_message_length(UCHAR *buf, int buf_len, int *msg_len)
{
  nr_stun_message_header *hdr;

  if (!nr_is_stun_message(buf, buf_len))
    return(R_BAD_DATA);

  hdr = (nr_stun_message_header *)buf;

  *msg_len = ntohs(hdr->length);

  return(0);
}



