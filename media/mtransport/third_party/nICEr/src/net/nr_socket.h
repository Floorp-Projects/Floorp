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



#ifndef _nr_socket_h
#define _nr_socket_h

#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

#include "transport_addr.h"

#ifdef __cplusplus
#define restrict
#elif defined(WIN32)
#define restrict __restrict
#endif

typedef struct nr_socket_vtbl_ {
  int (*destroy)(void **obj);
  int (*ssendto)(void *obj,const void *msg, size_t len, int flags,
    nr_transport_addr *addr);
  int (*srecvfrom)(void *obj,void * restrict buf, size_t maxlen, size_t *len, int flags,
    nr_transport_addr *addr);
  int (*getfd)(void *obj, NR_SOCKET *fd);
  int (*getaddr)(void *obj, nr_transport_addr *addrp);
  int (*close)(void *obj);
} nr_socket_vtbl;

typedef struct nr_socket_ {
  void *obj;
  nr_socket_vtbl *vtbl;
} nr_socket;


/* To be called by constructors */
int nr_socket_create_int(void *obj, nr_socket_vtbl *vtbl, nr_socket **sockp);
int nr_socket_destroy(nr_socket **sockp);
int nr_socket_sendto(nr_socket *sock,const void *msg, size_t len,
  int flags,nr_transport_addr *addr);
int nr_socket_recvfrom(nr_socket *sock,void * restrict buf, size_t maxlen,
  size_t *len, int flags, nr_transport_addr *addr);
int nr_socket_getfd(nr_socket *sock, NR_SOCKET *fd);
int nr_socket_getaddr(nr_socket *sock, nr_transport_addr *addrp);
int nr_socket_close(nr_socket *sock);

#endif

