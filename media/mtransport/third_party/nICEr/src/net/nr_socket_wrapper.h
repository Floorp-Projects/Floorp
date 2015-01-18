/*
Copyright (c) 2007, Adobe Systems, Incorporated
Copyright (c) 2013, Mozilla

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance, Mozilla nor
  the names of its contributors may be used to endorse or promote
  products derived from this software without specific prior written
  permission.

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

#ifndef _nr_socket_wrapper_h
#define _nr_socket_wrapper_h

#include "nr_socket.h"

typedef struct nr_socket_wrapper_factory_vtbl_ {
  int (*wrap)(void *obj,
                nr_socket *socket,
                nr_socket **socketp);
  int (*destroy)(void **obj);
} nr_socket_wrapper_factory_vtbl;

typedef struct nr_socket_wrapper_factory_ {
  void *obj;
  nr_socket_wrapper_factory_vtbl *vtbl;
} nr_socket_wrapper_factory;


int nr_socket_wrapper_factory_create_int(void *obj, nr_socket_wrapper_factory_vtbl *vtbl,
                                         nr_socket_wrapper_factory **wrapperp);


int nr_socket_wrapper_factory_wrap(nr_socket_wrapper_factory *wrapper, nr_socket *inner,
                                   nr_socket **socketp);

int nr_socket_wrapper_factory_destroy(nr_socket_wrapper_factory **wrapperp);

#endif
