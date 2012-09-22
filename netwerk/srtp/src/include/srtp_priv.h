/*
 * srtp_priv.h
 *
 * private internal data structures and functions for libSRTP
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *	
 * Copyright (c) 2001-2006 Cisco Systems, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef SRTP_PRIV_H
#define SRTP_PRIV_H

#include "srtp.h"
#include "rdbx.h"
#include "rdb.h"
#include "integers.h"

/*
 * an srtp_hdr_t represents the srtp header
 *
 * in this implementation, an srtp_hdr_t is assumed to be 32-bit aligned
 * 
 * (note that this definition follows that of RFC 1889 Appendix A, but
 * is not identical)
 */
 
#ifndef WORDS_BIGENDIAN

/*
 * srtp_hdr_t represents an RTP or SRTP header.  The bit-fields in
 * this structure should be declared "unsigned int" instead of 
 * "unsigned char", but doing so causes the MS compiler to not
 * fully pack the bit fields.
 */

typedef struct {
  unsigned char cc:4;	/* CSRC count             */
  unsigned char x:1;	/* header extension flag  */
  unsigned char p:1;	/* padding flag           */
  unsigned char version:2; /* protocol version    */
  unsigned char pt:7;	/* payload type           */
  unsigned char m:1;	/* marker bit             */
  uint16_t seq;		/* sequence number        */
  uint32_t ts;		/* timestamp              */
  uint32_t ssrc;	/* synchronization source */
} srtp_hdr_t;

#else /*  BIG_ENDIAN */

typedef struct {
  unsigned char version:2; /* protocol version    */
  unsigned char p:1;	/* padding flag           */
  unsigned char x:1;	/* header extension flag  */
  unsigned char cc:4;	/* CSRC count             */
  unsigned char m:1;	/* marker bit             */
  unsigned pt:7;	/* payload type           */
  uint16_t seq;		/* sequence number        */
  uint32_t ts;		/* timestamp              */
  uint32_t ssrc;	/* synchronization source */
} srtp_hdr_t;

#endif

typedef struct {
  uint16_t profile_specific;    /* profile-specific info               */
  uint16_t length;              /* number of 32-bit words in extension */
} srtp_hdr_xtnd_t;


/*
 * srtcp_hdr_t represents a secure rtcp header 
 *
 * in this implementation, an srtcp header is assumed to be 32-bit
 * alinged
 */

#ifndef WORDS_BIGENDIAN

typedef struct {
  unsigned char rc:5;		/* reception report count */
  unsigned char p:1;		/* padding flag           */
  unsigned char version:2;	/* protocol version       */
  unsigned char pt:8;		/* payload type           */
  uint16_t len;			/* length                 */
  uint32_t ssrc;	       	/* synchronization source */
} srtcp_hdr_t;

typedef struct {
  unsigned int index:31;    /* srtcp packet index in network order! */
  unsigned int e:1;         /* encrypted? 1=yes */
  /* optional mikey/etc go here */
  /* and then the variable-length auth tag */
} srtcp_trailer_t;


#else /*  BIG_ENDIAN */

typedef struct {
  unsigned char version:2;	/* protocol version       */
  unsigned char p:1;		/* padding flag           */
  unsigned char rc:5;		/* reception report count */
  unsigned char pt:8;		/* payload type           */
  uint16_t len;			/* length                 */
  uint32_t ssrc;	       	/* synchronization source */
} srtcp_hdr_t;

typedef struct {
  unsigned int version:2;  /* protocol version                     */
  unsigned int p:1;        /* padding flag                         */
  unsigned int count:5;    /* varies by packet type                */
  unsigned int pt:8;       /* payload type                         */
  uint16_t length;         /* len of uint32s of packet less header */
} rtcp_common_t;

typedef struct {
  unsigned int e:1;         /* encrypted? 1=yes */
  unsigned int index:31;    /* srtcp packet index */
  /* optional mikey/etc go here */
  /* and then the variable-length auth tag */
} srtcp_trailer_t;

#endif


/*
 * the following declarations are libSRTP internal functions 
 */

/*
 * srtp_get_stream(ssrc) returns a pointer to the stream corresponding
 * to ssrc, or NULL if no stream exists for that ssrc
 */

srtp_stream_t 
srtp_get_stream(srtp_t srtp, uint32_t ssrc);


/*
 * srtp_stream_init_keys(s, k) (re)initializes the srtp_stream_t s by
 * deriving all of the needed keys using the KDF and the key k.
 */


err_status_t
srtp_stream_init_keys(srtp_stream_t srtp, const void *key);

/*
 * srtp_stream_init(s, p) initializes the srtp_stream_t s to 
 * use the policy at the location p
 */
err_status_t
srtp_stream_init(srtp_stream_t srtp, 
		 const srtp_policy_t *p);


/*
 * libsrtp internal datatypes 
 */

typedef enum direction_t { 
  dir_unknown       = 0,
  dir_srtp_sender   = 1, 
  dir_srtp_receiver = 2
} direction_t;

/* 
 * an srtp_stream_t has its own SSRC, encryption key, authentication
 * key, sequence number, and replay database
 * 
 * note that the keys might not actually be unique, in which case the
 * cipher_t and auth_t pointers will point to the same structures
 */

typedef struct srtp_stream_ctx_t {
  uint32_t   ssrc;
  cipher_t  *rtp_cipher;
  auth_t    *rtp_auth;
  rdbx_t     rtp_rdbx;
  sec_serv_t rtp_services;
  cipher_t  *rtcp_cipher;
  auth_t    *rtcp_auth;
  rdb_t      rtcp_rdb;
  sec_serv_t rtcp_services;
  key_limit_ctx_t *limit;
  direction_t direction;
  int        allow_repeat_tx;
  ekt_stream_t ekt; 
  struct srtp_stream_ctx_t *next;   /* linked list of streams */
} srtp_stream_ctx_t;


/*
 * an srtp_ctx_t holds a stream list and a service description
 */

typedef struct srtp_ctx_t {
  srtp_stream_ctx_t *stream_list;     /* linked list of streams            */
  srtp_stream_ctx_t *stream_template; /* act as template for other streams */
} srtp_ctx_t;



/*
 * srtp_handle_event(srtp, srtm, evnt) calls the event handling
 * function, if there is one.
 *
 * This macro is not included in the documentation as it is 
 * an internal-only function.
 */

#define srtp_handle_event(srtp, strm, evnt)         \
   if(srtp_event_handler) {                         \
      srtp_event_data_t data;                       \
      data.session = srtp;                          \
      data.stream  = strm;                          \
      data.event   = evnt;                          \
      srtp_event_handler(&data);                    \
}   


#endif /* SRTP_PRIV_H */
