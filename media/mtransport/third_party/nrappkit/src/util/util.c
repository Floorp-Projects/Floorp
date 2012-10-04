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
      r_log(log_fac,LOG_ERR,"Invalid value for key '%s'=%lu, (min = %lu)",key,my_val,min);
      ABORT(R_BAD_DATA);
    }

    if(my_val>max){
      r_log(log_fac,LOG_ERR,"Invalid value for key '%s'=%lu, (max = %lu)",key,my_val,max);
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
/* Hack version of addr2ascii */
char *addr2ascii(int af, const void *addrp, int len,char *buf)
  {
    static char buf2[256];
    char *ret;
    struct in_addr *addr=(struct in_addr *)addrp;

    if (! buf)
      buf = buf2;

    if(af!=AF_INET)
      return("UNKNOWN");
    if(len!=sizeof(struct in_addr))
      return("UNKNOWN");

    ret=inet_ntoa(*addr);

    strcpy(buf,ret);

    return(buf);
  }

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

