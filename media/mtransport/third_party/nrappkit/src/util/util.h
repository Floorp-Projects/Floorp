/**
   util.h


   Copyright (C) 2001-2003, Network Resonance, Inc.
   Copyright (C) 2006, Network Resonance, Inc.
   All Rights Reserved

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of Network Resonance, Inc. nor the name of any
      contributors to this software may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.


   ekr@rtfm.com  Wed Dec 26 17:20:23 2001
 */


#ifndef _util_h
#define _util_h

#include "registry.h"

int nr_get_filename(char *base,char *name, char **namep);
#if 0
#include <openssl/ssl.h>

int read_RSA_private_key(char *base, char *name,RSA **keyp);
#endif
void nr_errprintf_log(const char *fmt,...);
void nr_errprintf_log2(void *ignore, const char *fmt,...);
extern int nr_util_default_log_facility;

int nr_read_data(int fd,char *buf,int len);
int nr_drop_privileges(char *username);
int nr_hex_ascii_dump(Data *data);
int nr_fwrite_all(FILE *fp,UCHAR *buf,int len);
int nr_sha1_file(char *filename,UCHAR *out);
int nr_bin2hex(UCHAR *in,int len,UCHAR *out);
int nr_rm_tree(char *path);
int nr_write_pid_file(char *pid_filename);

int nr_reg_uint4_fetch_and_check(NR_registry key, UINT4 min, UINT4 max, int log_fac, int die, UINT4 *val);
int nr_reg_uint8_fetch_and_check(NR_registry key, UINT8 min, UINT8 max, int log_fac, int die, UINT8 *val);

#ifdef WIN32
int snprintf(char *buffer, size_t n, const char *format, ...);
const char *inet_ntop(int af, const void *src, char *dst, size_t size);
#endif

#endif

