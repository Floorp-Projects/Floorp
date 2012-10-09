/**
   r_log.h

   
   Copyright (C) 2001, RTFM, Inc.
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
   

   ekr@rtfm.com  Mon Dec  3 15:14:45 2001
 */


#ifndef _r_log_h
#define _r_log_h

#ifndef WIN32
#include <syslog.h>
#endif
#include <stdarg.h>
#include <r_common.h>

int r_log(int facility,int level,const char *fmt,...);
int r_vlog(int facility,int level,const char *fmt,va_list ap);
int r_dump(int facility,int level,char *name,char *data,int len);

int r_log_e(int facility,int level,const char *fmt,...);
int r_vlog_e(int facility,int level,const char *fmt,va_list ap);
int r_log_nr(int facility,int level,int r,const char *fmt,...);
int r_vlog_nr(int facility,int level,int r,const char *fmt,va_list ap);

int r_log_register(char *tipename,int *facility);
int r_log_facility(int facility,char **tipename);
int r_logging(int facility, int level);
int r_log_init(void);

#define LOG_GENERIC 0
#define LOG_COMMON 0

typedef int r_dest_vlog(int facility,int level,const char *format,va_list ap);
int r_log_set_extra_destination(int default_level, r_dest_vlog *dest_vlog);

#endif

