/**
   platform.h


   Copyright (C) 2005, Network Resonance, Inc.
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


   briank@network-resonance.com  Tue Mar 15 14:28:12 PST 2005
 */


#ifndef _csi_platform_h
#define _csi_platform_h

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400  // This prevents weird "'TryEnterCriticalSection': identifier not found"
                             // compiler errors when poco/win32_mutex.h is included
#endif

#define UINT8 UBLAH_IGNORE_ME_PLEASE
#define INT8 BLAH_IGNORE_ME_PLEASE
#include <winsock2.h>
#undef UINT8
#undef INT8
#include <r_types.h>
#include <errno.h>

#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#define strcasestr stristr

/* Hack version of strlcpy (in util/util.c) */
size_t strlcat(char *dst, const char *src, size_t siz);

/* Hack version of getopt() (in util/getopt.c) */
int getopt(int argc, char *argv[], char *opstring);
extern char *optarg;
extern int optind;
extern int opterr;

/* Hack version of gettimeofday() (in util/util.c) */
int gettimeofday(struct timeval *tv, void *tz);

#ifdef NR_SOCKET_IS_VOID_PTR
typedef void* NR_SOCKET;
#else
typedef SOCKET NR_SOCKET;
#define NR_SOCKET_READ(sock,buf,count)   recv((sock),(buf),(count),0)
#define NR_SOCKET_WRITE(sock,buf,count)  send((sock),(buf),(count),0)
#define NR_SOCKET_CLOSE(sock)            closesocket(sock)
#endif

#ifndef EHOSTUNREACH
#define EHOSTUNREACH    WSAEHOSTUNREACH
#endif

#define LOG_EMERG       0
#define LOG_ALERT       1
#define LOG_CRIT        2
#define LOG_ERR         3
#define LOG_WARNING     4
#define LOG_NOTICE      5
#define LOG_INFO        6
#define LOG_DEBUG       7

// Until we refine the Windows port....

#define IFNAMSIZ        256  /* big enough for FriendlyNames */
#define in_addr_t   UINT4

#ifndef strlcpy
#define strlcpy(a,b,c) \
        (strncpy((a),(b),(c)), \
         ((c)<= 0 ? 0 : ((a)[(c)-1]='\0')), \
         strlen((b)))
#endif

#endif  /* _csi_platform_h */

