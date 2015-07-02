/**
   util.c


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


   ekr@rtfm.com  Wed Dec 26 17:19:36 2001
 */


static char *RCSSTRING __UNUSED__ ="$Id: util.c,v 1.5 2007/11/21 00:09:13 adamcain Exp $";

#ifndef WIN32
#include <sys/uio.h>
#include <pwd.h>
#include <dirent.h>
#endif
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#ifdef OPENSSL
#include <openssl/evp.h>
#endif
#include "nr_common.h"
#include "r_common.h"
#include "registry.h"
#include "util.h"
#include "r_log.h"

int nr_util_default_log_facility=LOG_COMMON;

int nr_get_filename(base,name,namep)
  char *base;
  char *name;
  char **namep;
  {
    int len=strlen(base)+strlen(name)+2;
    char *ret=0;
    int _status;

    if(!(ret=(char *)RMALLOC(len)))
      ABORT(R_NO_MEMORY);
    if(base[strlen(base)-1]!='/'){
      sprintf(ret,"%s/%s",base,name);
    }
    else{
      sprintf(ret,"%s%s",base,name);
    }
    *namep=ret;
    _status=0;
  abort:
    return(_status);
  }

#if 0
int read_RSA_private_key(base,name,keyp)
  char *base;
  char *name;
  RSA **keyp;
  {
    char *keyfile=0;
    BIO *bio=0;
    FILE *fp=0;
    RSA *rsa=0;
    int r,_status;

    /* Load the keyfile */
    if(r=get_filename(base,name,&keyfile))
      ABORT(r);
    if(!(fp=fopen(keyfile,"r")))
      ABORT(R_NOT_FOUND);
    if(!(bio=BIO_new(BIO_s_file())))
      ABORT(R_NO_MEMORY);
    BIO_set_fp(bio,fp,BIO_NOCLOSE);

    if(!(rsa=PEM_read_bio_RSAPrivateKey(bio,0,0,0)))
      ABORT(R_NOT_FOUND);

    *keyp=rsa;
    _status=0;
  abort:
    return(_status);
  }
#endif


void nr_errprintf_log(const char *format,...)
  {
    va_list ap;

    va_start(ap,format);

    r_vlog(nr_util_default_log_facility,LOG_ERR,format,ap);

    va_end(ap);
  }

void nr_errprintf_log2(void *ignore, const char *format,...)
  {
    va_list ap;

    va_start(ap,format);

    r_vlog(nr_util_default_log_facility,LOG_ERR,format,ap);

    va_end(ap);
  }


int nr_fwrite_all(FILE *fp,UCHAR *buf,int len)
  {
    int r,_status;

    while(len){
      r=fwrite(buf,1,len,fp);
      if(r==0)
        ABORT(R_IO_ERROR);

      len-=r;
      buf+=r;
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_read_data(fd,buf,len)
  int fd;
  char *buf;
  int len;
  {
    int r,_status;

    while(len){
      r=NR_SOCKET_READ(fd,buf,len);
      if(r<=0)
        ABORT(R_EOD);

      buf+=r;
      len-=r;
    }


    _status=0;
  abort:
    return(_status);
  }

#ifdef WIN32
  // TODO
#else
int nr_drop_privileges(char *username)
  {
    int _status;

    /* Drop privileges */
    if ((getuid() == 0) || geteuid()==0) {
      struct passwd *passwd;

      if ((passwd = getpwnam(CAPTURE_USER)) == 0){
        r_log(LOG_GENERIC,LOG_EMERG,"Couldn't get user %s",CAPTURE_USER);
        ABORT(R_INTERNAL);
      }

      if(setuid(passwd->pw_uid)!=0){
        r_log(LOG_GENERIC,LOG_EMERG,"Couldn't drop privileges");
        ABORT(R_INTERNAL);
      }
    }

    _status=0;
  abort:
    return(_status);
  }
#endif

int nr_bin2hex(UCHAR *in,int len,UCHAR *out)
  {
    while(len){
      sprintf((char*)out,"%.2x",in[0] & 0xff);

      in+=1;
      out+=2;

      len--;
    }

    return(0);
  }

int nr_hex_ascii_dump(Data *data)
  {
    UCHAR *ptr=data->data;
    int len=data->len;

    while(len){
      int i;
      int bytes=MIN(len,16);

      for(i=0;i<bytes;i++)
        printf("%.2x ",ptr[i]&255);
      /* Fill */
      for(i=0;i<(16-bytes);i++)
        printf("   ");
      printf("   ");

      for(i=0;i<bytes;i++){
        if(isprint(ptr[i]))
          printf("%c",ptr[i]);
        else
          printf(".");
      }
      printf("\n");

      len-=bytes;
      ptr+=bytes;
    }
    return(0);
  }

#ifdef OPENSSL
int nr_sha1_file(char *filename,UCHAR *out)
  {
    EVP_MD_CTX md_ctx;
    FILE *fp=0;
    int r,_status;
    UCHAR buf[1024];
    int out_len;

    EVP_MD_CTX_init(&md_ctx);

    if(!(fp=fopen(filename,"r"))){
      r_log(LOG_COMMON,LOG_ERR,"Couldn't open file %s",filename);
      ABORT(R_NOT_FOUND);
    }

    EVP_DigestInit_ex(&md_ctx,EVP_sha1(),0);

    while(1){
      r=fread(buf,1,sizeof(buf),fp);

      if(r<0){
        r_log(LOG_COMMON,LOG_ERR,"Error reading from %s",filename);
        ABORT(R_INTERNAL);
      }

      if(!r)
        break;

      EVP_DigestUpdate(&md_ctx,buf,r);
    }

    EVP_DigestFinal(&md_ctx,out,(unsigned int*)&out_len);
    if(out_len!=20)
      ABORT(R_INTERNAL);

    _status=0;
  abort:
    EVP_MD_CTX_cleanup(&md_ctx);
    if(fp) fclose(fp);

    return(_status);
  }

#endif

#ifdef WIN32
  // TODO
#else

#if 0

#include <fts.h>

int nr_rm_tree(char *path)
  {
    FTS *fts=0;
    FTSENT *p;
    int failed=0;
    int _status;
    char *argv[2];

    argv[0]=path;
    argv[1]=0;

    if(!(fts=fts_open(argv,0,NULL))){
      r_log_e(LOG_COMMON,LOG_ERR,"Couldn't open directory %s",path);
      ABORT(R_FAILED);
    }

    while(p=fts_read(fts)){
      switch(p->fts_info){
        case FTS_D:
          break;
        case FTS_DOT:
          break;
        case FTS_ERR:
          r_log_e(LOG_COMMON,LOG_ERR,"Problem reading %s",p->fts_path);
          break;
        default:
          r_log(LOG_COMMON,LOG_DEBUG,"Removing %s",p->fts_path);
          errno=0;
          if(remove(p->fts_path)){
            r_log_e(LOG_COMMON,LOG_ERR,"Problem removing %s",p->fts_path);
            failed=1;
          }
      }
    }

    if(failed)
      ABORT(R_FAILED);

    _status=0;
  abort:
    if(fts) fts_close(fts);
    return(_status);
  }
#endif

int nr_write_pid_file(char *pid_filename)
  {
    FILE *fp;
    int _status;

    if(!pid_filename)
      ABORT(R_BAD_ARGS);

    unlink(pid_filename);

    if(!(fp=fopen(pid_filename,"w"))){
      r_log(LOG_GENERIC,LOG_CRIT,"Couldn't open PID file: %s",strerror(errno));
      ABORT(R_NOT_FOUND);
    }

    fprintf(fp,"%d\n",getpid());

    fclose(fp);

    chmod(pid_filename,S_IRUSR | S_IRGRP | S_IROTH);

    _status=0;
  abort:
    return(_status);
  }
#endif

int nr_reg_uint4_fetch_and_check(NR_registry key, UINT4 min, UINT4 max, int log_fac, int die, UINT4 *val)
  {
    int r,_status;
    UINT4 my_val;

    if(r=NR_reg_get_uint4(key,&my_val)){
      r_log(log_fac,LOG_ERR,"Couldn't get key '%s', error %d",key,r);
      ABORT(r);
    }

    if((min>0) && (my_val<min)){
      r_log(log_fac,LOG_ERR,"Invalid value for key '%s'=%lu, (min = %lu)",key,(unsigned long)my_val,(unsigned long)min);
      ABORT(R_BAD_DATA);
    }

    if(my_val>max){
      r_log(log_fac,LOG_ERR,"Invalid value for key '%s'=%lu, (max = %lu)",key,(unsigned long)my_val,(unsigned long)max);
      ABORT(R_BAD_DATA);
    }

    *val=my_val;
    _status=0;

  abort:
    if(die && _status){
      r_log(log_fac,LOG_CRIT,"Exiting due to invalid configuration (key '%s')",key);
      exit(1);
    }
    return(_status);
  }

int nr_reg_uint8_fetch_and_check(NR_registry key, UINT8 min, UINT8 max, int log_fac, int die, UINT8 *val)
  {
    int r,_status;
    UINT8 my_val;

    if(r=NR_reg_get_uint8(key,&my_val)){
      r_log(log_fac,LOG_ERR,"Couldn't get key '%s', error %d",key,r);
      ABORT(r);
    }

    if(my_val<min){
      r_log(log_fac,LOG_ERR,"Invalid value for key '%s'=%llu, (min = %llu)",key,my_val,min);
      ABORT(R_BAD_DATA);
    }

    if(my_val>max){
      r_log(log_fac,LOG_ERR,"Invalid value for key '%s'=%llu, (max = %llu)",key,my_val,max);
      ABORT(R_BAD_DATA);
    }

    *val=my_val;
    _status=0;

  abort:
    if(die && _status){
      r_log(log_fac,LOG_CRIT,"Exiting due to invalid configuration (key '%s')",key);
      exit(1);
    }
    return(_status);
  }

#if defined(LINUX) || defined(WIN32)
/*-
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t
strlcat(dst, src, siz)
        char *dst;
        const char *src;
        size_t siz;
{
        char *d = dst;
        const char *s = src;
        size_t n = siz;
        size_t dlen;

        /* Find the end of dst and adjust bytes left but don't go past end */
        while (n-- != 0 && *d != '\0')
                d++;
        dlen = d - dst;
        n = siz - dlen;

        if (n == 0)
                return(dlen + strlen(s));
        while (*s != '\0') {
                if (n != 1) {
                        *d++ = *s;
                        n--;
                }
                s++;
        }
        *d = '\0';

        return(dlen + (s - src));       /* count does not include NUL */
}

#endif  /* LINUX or WIN32 */

#if defined(USE_OWN_INET_NTOP) || defined(WIN32)
#include <errno.h>
#ifdef WIN32
#include <Ws2ipdef.h>
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#endif
#else
#include <sys/socket.h>
#endif
#define INET6

/* inet_ntop implementation from NetBSD */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if !defined(NS_INADDRSZ)
# define NS_INADDRSZ  4
#endif
#if !defined(NS_IN6ADDRSZ)
# define NS_IN6ADDRSZ  16
#endif
#if !defined(NS_INT16SZ)
# define NS_INT16SZ  2
#endif

/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

static const char *inet_ntop4(const unsigned char *src, char *dst, size_t size);
#ifdef INET6
static const char *inet_ntop6(const unsigned char *src, char *dst, size_t size);
#endif /* INET6 */

/* char *
 * inet_ntop(af, src, dst, size)
 *  convert a network format address to presentation format.
 * return:
 *  pointer to presentation format address (`dst'), or NULL (see errno).
 * author:
 *  Paul Vixie, 1996.
 */
const char *
inet_ntop(int af, const void *src, char *dst, size_t size)
{

  switch (af) {
  case AF_INET:
    return (inet_ntop4(src, dst, size));
#ifdef INET6
  case AF_INET6:
    return (inet_ntop6(src, dst, size));
#endif /* INET6 */
  default:
    errno = EAFNOSUPPORT;
    return (NULL);
  }
  /* NOTREACHED */
}

/* const char *
 * inet_ntop4(src, dst, size)
 *  format an IPv4 address, more or less like inet_ntoa()
 * return:
 *  `dst' (as a const)
 * notes:
 *  (1) uses no statics
 *  (2) takes a unsigned char* not an in_addr as input
 * author:
 *  Paul Vixie, 1996.
 */
static const char *
inet_ntop4(const unsigned char *src, char *dst, size_t size)
{
  char tmp[sizeof "255.255.255.255"];
  int l;

  l = snprintf(tmp, sizeof(tmp), "%u.%u.%u.%u",
      src[0], src[1], src[2], src[3]);
  if (l <= 0 || (size_t) l >= size) {
    errno = ENOSPC;
    return (NULL);
  }
  strlcpy(dst, tmp, size);
  return (dst);
}

#ifdef INET6
/* const char *
 * inet_ntop6(src, dst, size)
 *  convert IPv6 binary address into presentation (printable) format
 * author:
 *  Paul Vixie, 1996.
 */
static const char *
inet_ntop6(const unsigned char *src, char *dst, size_t size)
{
  /*
   * Note that int32_t and int16_t need only be "at least" large enough
   * to contain a value of the specified size.  On some systems, like
   * Crays, there is no such thing as an integer variable with 16 bits.
   * Keep this in mind if you think this function should have been coded
   * to use pointer overlays.  All the world's not a VAX.
   */
  char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
  char *tp, *ep;
  struct { int base, len; } best, cur;
  unsigned int words[NS_IN6ADDRSZ / NS_INT16SZ];
  int i;
  int advance;

  /*
   * Preprocess:
   *  Copy the input (bytewise) array into a wordwise array.
   *  Find the longest run of 0x00's in src[] for :: shorthanding.
   */
  memset(words, '\0', sizeof words);
  for (i = 0; i < NS_IN6ADDRSZ; i++)
    words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
  best.base = -1;
  cur.base = -1;
  best.len = -1;  /* XXX gcc */
  cur.len = -1;  /* XXX gcc */
  for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
    if (words[i] == 0) {
      if (cur.base == -1)
        cur.base = i, cur.len = 1;
      else
        cur.len++;
    } else {
      if (cur.base != -1) {
        if (best.base == -1 || cur.len > best.len)
          best = cur;
        cur.base = -1;
      }
    }
  }
  if (cur.base != -1) {
    if (best.base == -1 || cur.len > best.len)
      best = cur;
  }
  if (best.base != -1 && best.len < 2)
    best.base = -1;

  /*
   * Format the result.
   */
  tp = tmp;
  ep = tmp + sizeof(tmp);
  for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
    /* Are we inside the best run of 0x00's? */
    if (best.base != -1 && i >= best.base &&
        i < (best.base + best.len)) {
      if (i == best.base)
        *tp++ = ':';
      continue;
    }
    /* Are we following an initial run of 0x00s or any real hex? */
    if (i != 0) {
      if (tp + 1 >= ep)
        return (NULL);
      *tp++ = ':';
    }
    /* Is this address an encapsulated IPv4? */
    if (i == 6 && best.base == 0 &&
        (best.len == 6 ||
        (best.len == 7 && words[7] != 0x0001) ||
        (best.len == 5 && words[5] == 0xffff))) {
      if (!inet_ntop4(src+12, tp, (size_t)(ep - tp)))
        return (NULL);
      tp += strlen(tp);
      break;
    }
    advance = snprintf(tp, (size_t)(ep - tp), "%x", words[i]);
    if (advance <= 0 || advance >= ep - tp)
      return (NULL);
    tp += advance;
  }
  /* Was it a trailing run of 0x00's? */
  if (best.base != -1 && (best.base + best.len) ==
      (NS_IN6ADDRSZ / NS_INT16SZ)) {
    if (tp + 1 >= ep)
      return (NULL);
    *tp++ = ':';
  }
  if (tp + 1 >= ep)
    return (NULL);
  *tp++ = '\0';

  /*
   * Check for overflow, copy, and we're done.
   */
  if ((size_t)(tp - tmp) > size) {
    errno = ENOSPC;
    return (NULL);
  }
  strlcpy(dst, tmp, size);
  return (dst);
}
#endif /* INET6 */

#ifdef WIN32
/* Not exactly, will forgive stuff like <addr>:<port> */
int inet_pton(int af, const char *src, void *dst)
{
  struct sockaddr_storage ss;
  int addrlen = sizeof(ss);

  if (af != AF_INET && af != AF_INET6) {
    return -1;
  }

  if (!WSAStringToAddressA(src, af, NULL, (struct sockaddr*)&ss, &addrlen)) {
    if (af == AF_INET) {
      struct sockaddr_in *in = (struct sockaddr_in*)&ss;
      memcpy(dst, &in->sin_addr, sizeof(struct in_addr));
    } else {
      struct sockaddr_in6 *in6 = (struct sockaddr_in6*)&ss;
      memcpy(dst, &in6->sin6_addr, sizeof(struct in6_addr));
    }
    return 1;
  }
  return 0;
}
#endif /* WIN32 */

#endif

#ifdef WIN32
#include <time.h>
/* this is only millisecond-accurate, but that should be OK */

int gettimeofday(struct timeval *tv, void *tz)
  {
    SYSTEMTIME st;
    FILETIME ft;
    ULARGE_INTEGER u;

    GetLocalTime (&st);

    /* strangely, the FILETIME is the number of 100 nanosecond (0.1 us) intervals
     * since the Epoch */
    SystemTimeToFileTime(&st, &ft);
    u.HighPart = ft.dwHighDateTime;
    u.LowPart = ft.dwLowDateTime;

    tv->tv_sec = (long) (u.QuadPart / 10000000L);
    tv->tv_usec = (long) (st.wMilliseconds * 1000);;

    return 0;
  }

#if _MSC_VER < 1900
int snprintf(char *buffer, size_t n, const char *format, ...)
{
  va_list argp;
  int ret;
  va_start(argp, format);
  ret = _vscprintf(format, argp);
  vsnprintf_s(buffer, n, _TRUNCATE, format, argp);
  va_end(argp);
  return ret;
}
#endif

#endif

