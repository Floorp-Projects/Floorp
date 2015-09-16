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

#include <nr_api.h>

#include <assert.h>

#include "nr_proxy_tunnel.h"

#define MAX_HTTP_CONNECT_ADDR_SIZE 256
#define MAX_HTTP_CONNECT_BUFFER_SIZE 1024
#define MAX_ALPN_LENGTH 64
#ifndef CRLF
#define CRLF "\r\n"
#endif
#define END_HEADERS CRLF CRLF

typedef struct nr_socket_proxy_tunnel_ {
  nr_proxy_tunnel_config *config;
  nr_socket *inner;
  nr_transport_addr remote_addr;
  int connect_requested;
  int connect_answered;
  int connect_failed;
  char buffer[MAX_HTTP_CONNECT_BUFFER_SIZE];
  size_t buffered_bytes;
  void *resolver_handle;
} nr_socket_proxy_tunnel;

typedef struct nr_socket_wrapper_factory_proxy_tunnel_ {
  nr_proxy_tunnel_config *config;
} nr_socket_wrapper_factory_proxy_tunnel;

static int nr_socket_proxy_tunnel_destroy(void **objpp);
static int nr_socket_proxy_tunnel_getfd(void *obj, NR_SOCKET *fd);
static int nr_socket_proxy_tunnel_getaddr(void *obj, nr_transport_addr *addrp);
static int nr_socket_proxy_tunnel_connect(void *sock, nr_transport_addr *addr);
static int nr_socket_proxy_tunnel_write(void *obj, const void *msg, size_t len, size_t *written);
static int nr_socket_proxy_tunnel_read(void *obj, void * restrict buf, size_t maxlen, size_t *len);
static int nr_socket_proxy_tunnel_close(void *obj);

int nr_proxy_tunnel_config_copy(nr_proxy_tunnel_config *config, nr_proxy_tunnel_config **copypp);

int nr_socket_wrapper_factory_proxy_tunnel_wrap(void *obj,
                                                nr_socket *inner,
                                                nr_socket **socketpp);

int nr_socket_wrapper_factory_proxy_tunnel_destroy(void **objpp);

static nr_socket_vtbl nr_socket_proxy_tunnel_vtbl={
  1,
  nr_socket_proxy_tunnel_destroy,
  0,
  0,
  nr_socket_proxy_tunnel_getfd,
  nr_socket_proxy_tunnel_getaddr,
  nr_socket_proxy_tunnel_connect,
  nr_socket_proxy_tunnel_write,
  nr_socket_proxy_tunnel_read,
  nr_socket_proxy_tunnel_close
};

static int send_http_connect(nr_socket_proxy_tunnel *sock)
{
  int r, _status;
  int port;
  int printed;
  char addr[MAX_HTTP_CONNECT_ADDR_SIZE];
  char mesg[MAX_HTTP_CONNECT_ADDR_SIZE + MAX_ALPN_LENGTH + 128];
  size_t offset = 0;
  size_t bytes_sent;

  r_log(LOG_GENERIC,LOG_DEBUG,"send_http_connect");

  if ((r=nr_transport_addr_get_port(&sock->remote_addr, &port))) {
    ABORT(r);
  }

  if ((r=nr_transport_addr_get_addrstring(&sock->remote_addr, addr, sizeof(addr)))) {
    ABORT(r);
  }

  printed = snprintf(mesg + offset, sizeof(mesg) - offset,
                     "CONNECT %s:%d HTTP/1.0", addr, port);
  offset += printed;
  if (printed < 0 || (offset >= sizeof(mesg))) {
    ABORT(R_FAILED);
  }

  if (sock->config->alpn) {
    printed = snprintf(mesg + offset, sizeof(mesg) - offset,
                       CRLF "ALPN: %s", sock->config->alpn);
    offset += printed;
    if (printed < 0 || (offset >= sizeof(mesg))) {
      ABORT(R_FAILED);
    }
  }
  if (offset + sizeof(END_HEADERS) >= sizeof(mesg)) {
    ABORT(R_FAILED);
  }
  memcpy(mesg + offset, END_HEADERS, strlen(END_HEADERS));
  offset += strlen(END_HEADERS);

  if ((r=nr_socket_write(sock->inner, mesg, offset, &bytes_sent, 0))) {
    ABORT(r);
  }

  if (bytes_sent < offset) {
    /* TODO(bug 1116583): buffering and wait for */
    r_log(LOG_GENERIC,LOG_DEBUG,"send_http_connect should be buffering %lu", (unsigned long)bytes_sent);
    ABORT(R_IO_ERROR);
  }

  sock->connect_requested = 1;

  _status = 0;
abort:
  return(_status);
}

static char *find_http_terminator(char *response, size_t len)
{
  char *term = response;
  char *end = response + len;
  int N = strlen(END_HEADERS);

  for (; term = memchr(term, '\r', end - term); ++term) {
    if (end - term >= N && memcmp(term, END_HEADERS, N) == 0) {
      return term;
    }
  }

  return NULL;
}

static int parse_http_response(char *begin, char *end, unsigned int *status)
{
  size_t len = end - begin;
  char response[MAX_HTTP_CONNECT_BUFFER_SIZE + 1];

  // len should *never* be greater than nr_socket_proxy_tunnel::buffered_bytes.
  // Which in turn should never be greater nr_socket_proxy_tunnel::buffer size.
  assert(len <= MAX_HTTP_CONNECT_BUFFER_SIZE);

  memcpy(response, begin, len);
  response[len] = '\0';

  // http://www.rfc-editor.org/rfc/rfc7230.txt
  // status-line    = HTTP-version SP status-code SP reason-phrase CRLF
  // HTTP-version   = HTTP-name "/" DIGIT "." DIGIT
  // HTTP-name      = "HTTP" ; "HTTP", case-sensitive
  // status-code    = 3DIGIT

  if (sscanf(response, "HTTP/%*u.%*u %u", status) != 1) {
    r_log(LOG_GENERIC,LOG_WARNING,"parse_http_response failed to find status (%s)", response);
    return R_BAD_DATA;
  }

  return 0;
}

static int nr_socket_proxy_tunnel_destroy(void **objpp)
{
  nr_socket_proxy_tunnel *sock;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_proxy_tunnel_destroy");

  if (!objpp || !*objpp)
    return 0;

  sock = (nr_socket_proxy_tunnel *)*objpp;
  *objpp = 0;

  if (sock->resolver_handle) {
    nr_resolver_cancel(sock->config->resolver, sock->resolver_handle);
  }

  nr_proxy_tunnel_config_destroy(&sock->config);
  nr_socket_destroy(&sock->inner);
  RFREE(sock);

  return 0;
}

static int nr_socket_proxy_tunnel_getfd(void *obj, NR_SOCKET *fd)
{
  nr_socket_proxy_tunnel *sock = (nr_socket_proxy_tunnel *)obj;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_proxy_tunnel_getfd");

  return nr_socket_getfd(sock->inner, fd);
}

static int nr_socket_proxy_tunnel_getaddr(void *obj, nr_transport_addr *addrp)
{
  nr_socket_proxy_tunnel *sock = (nr_socket_proxy_tunnel *)obj;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_proxy_tunnel_getaddr");

  return nr_socket_getaddr(sock->inner, addrp);
}

static int nr_socket_proxy_tunnel_resolved_cb(void *obj, nr_transport_addr *proxy_addr)
{
  int r, _status;
  nr_socket_proxy_tunnel *sock = (nr_socket_proxy_tunnel*)obj;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_proxy_tunnel_resolved_cb");

  /* Mark the socket resolver as completed */
  sock->resolver_handle = 0;

  if (proxy_addr) {
    r_log(LOG_GENERIC,LOG_DEBUG,"Resolved proxy address %s -> %s",
        sock->config->proxy_host, proxy_addr->as_string);
  }
  else {
    r_log(LOG_GENERIC,LOG_WARNING,"Failed to resolve proxy %s",
        sock->config->proxy_host);
    ABORT(R_NOT_FOUND);
  }

  if ((r=nr_socket_connect(sock->inner, proxy_addr))) {
    ABORT(r);
  }

  _status = 0;
abort:
  return(_status);
}

int nr_socket_proxy_tunnel_connect(void *obj, nr_transport_addr *addr)
{
  int r, _status;
  int has_addr;
  nr_socket_proxy_tunnel *sock = (nr_socket_proxy_tunnel*)obj;
  nr_proxy_tunnel_config *config = sock->config;
  nr_transport_addr proxy_addr;
  nr_resolver_resource resource;

  if ((r=nr_transport_addr_copy(&sock->remote_addr, addr))) {
    ABORT(r);
  }

  assert(config->proxy_host);

  /* Check if the proxy_host is already an IP address */
  has_addr = !nr_str_port_to_transport_addr(config->proxy_host,
      config->proxy_port, IPPROTO_TCP, &proxy_addr);

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_proxy_tunnel_connect: %s", config->proxy_host);

  if (!has_addr && !config->resolver) {
    r_log(LOG_GENERIC,LOG_ERR,"nr_socket_proxy_tunnel_connect name resolver not configured");
    ABORT(R_NOT_FOUND);
  }

  if (!has_addr) {
    resource.domain_name=config->proxy_host;
    resource.port=config->proxy_port;
    resource.stun_turn=NR_RESOLVE_PROTOCOL_TURN;
    resource.transport_protocol=IPPROTO_TCP;

    r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_proxy_tunnel_connect: nr_resolver_resolve");
    if ((r=nr_resolver_resolve(config->resolver, &resource,
            nr_socket_proxy_tunnel_resolved_cb, (void *)sock, &sock->resolver_handle))) {
      r_log(LOG_GENERIC,LOG_ERR,"Could not invoke DNS resolver");
      ABORT(r);
    }

    ABORT(R_WOULDBLOCK);
  }

  if ((r=nr_socket_connect(sock->inner, &proxy_addr))) {
    ABORT(r);
  }

  _status=0;
abort:
  return(_status);
}

int nr_socket_proxy_tunnel_write(void *obj, const void *msg, size_t len,
                                 size_t *written)
{
  int r, _status;
  nr_socket_proxy_tunnel *sock = (nr_socket_proxy_tunnel*)obj;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_proxy_tunnel_write");

  if (!sock->connect_requested) {
    if ((r=send_http_connect(sock))) {
      ABORT(r);
    }
  }

  /* TODO (bug 1117984): we cannot assume it's safe to write until we receive a response. */
  if ((r=nr_socket_write(sock->inner, msg, len, written, 0))) {
    ABORT(r);
  }

  _status=0;
abort:
  return(_status);
}

int nr_socket_proxy_tunnel_read(void *obj, void * restrict buf, size_t maxlen,
                                size_t *len)
{
  int r, _status;
  char *ptr, *http_term;
  size_t bytes_read, available_buffer_len, maxlen_int;
  size_t pending;
  nr_socket_proxy_tunnel *sock = (nr_socket_proxy_tunnel*)obj;
  unsigned int http_status;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_proxy_tunnel_read");

  *len = 0;

  if (sock->connect_failed) {
    return R_FAILED;
  }

  if (sock->connect_answered) {
    return nr_socket_read(sock->inner, buf, maxlen, len, 0);
  }

  if (sock->buffered_bytes >= sizeof(sock->buffer)) {
    r_log(LOG_GENERIC,LOG_ERR,"buffer filled waiting for CONNECT response");
    assert(sock->buffered_bytes == sizeof(sock->buffer));
    ABORT(R_INTERNAL);
  }

  /* Do not read more than maxlen bytes */
  available_buffer_len = sizeof(sock->buffer) - sock->buffered_bytes;
  maxlen_int = maxlen < available_buffer_len ? maxlen : available_buffer_len;
  if ((r=nr_socket_read(sock->inner, sock->buffer + sock->buffered_bytes,
          maxlen_int, &bytes_read, 0))) {
    ABORT(r);
  }

  sock->buffered_bytes += bytes_read;

  if (http_term = find_http_terminator(sock->buffer, sock->buffered_bytes)) {
    sock->connect_answered = 1;

    if ((r = parse_http_response(sock->buffer, http_term, &http_status))) {
      ABORT(r);
    }

    /* TODO (bug 1115934): Handle authentication challenges. */
    if (http_status < 200 || http_status >= 300) {
      r_log(LOG_GENERIC,LOG_ERR,"nr_socket_proxy_tunnel_read unable to connect %u",
            http_status);
      ABORT(R_FAILED);
    }

    ptr = http_term + strlen(END_HEADERS);
    pending = sock->buffered_bytes - (ptr - sock->buffer);

    if (pending == 0) {
      ABORT(R_WOULDBLOCK);
    }

    assert(pending <= maxlen);
    *len = pending;

    memcpy(buf, ptr, *len);
  }

  _status=0;
abort:
  if (_status && _status != R_WOULDBLOCK) {
      sock->connect_failed = 1;
  }
  return(_status);
}

int nr_socket_proxy_tunnel_close(void *obj)
{
  nr_socket_proxy_tunnel *sock = (nr_socket_proxy_tunnel*)obj;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_proxy_tunnel_close");

  if (sock->resolver_handle) {
    nr_resolver_cancel(sock->config->resolver, sock->resolver_handle);
    sock->resolver_handle = 0;
  }

  return nr_socket_close(sock->inner);
}

int nr_proxy_tunnel_config_create(nr_proxy_tunnel_config **configpp)
{
  int _status;
  nr_proxy_tunnel_config *configp=0;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_proxy_tunnel_config_create");

  if (!(configp=RCALLOC(sizeof(nr_proxy_tunnel_config))))
    ABORT(R_NO_MEMORY);

  *configpp=configp;
  _status=0;
abort:
  return(_status);
}

int nr_proxy_tunnel_config_destroy(nr_proxy_tunnel_config **configpp)
{
  nr_proxy_tunnel_config *configp;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_proxy_tunnel_config_destroy");

  if (!configpp || !*configpp)
    return 0;

  configp = *configpp;
  *configpp = 0;

  RFREE(configp->proxy_host);
  RFREE(configp->alpn);
  RFREE(configp);

  return 0;
}

int nr_proxy_tunnel_config_set_proxy(nr_proxy_tunnel_config *config,
                                     const char *host, UINT2 port)
{
  char *hostdup;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_proxy_tunnel_config_set_proxy %s %d", host, port);

  if (!host) {
    return R_BAD_ARGS;
  }

  if (!(hostdup = r_strdup(host))) {
    return R_NO_MEMORY;
  }

  if (config->proxy_host) {
    RFREE(config->proxy_host);
  }

  config->proxy_host = hostdup;
  config->proxy_port = port;

  return 0;
}

int nr_proxy_tunnel_config_set_resolver(nr_proxy_tunnel_config *config,
                                        nr_resolver *resolver)
{
  r_log(LOG_GENERIC,LOG_DEBUG,"nr_proxy_tunnel_config_set_resolver");

  config->resolver = resolver;

  return 0;
}

int nr_proxy_tunnel_config_set_alpn(nr_proxy_tunnel_config *config,
                                    const char *alpn)
{
  r_log(LOG_GENERIC,LOG_DEBUG,"nr_proxy_tunnel_config_set_alpn");

  if (alpn && (strlen(alpn) > MAX_ALPN_LENGTH)) {
    return R_BAD_ARGS;
  }

  if (config->alpn) {
    RFREE(config->alpn);
  }

  config->alpn = NULL;

  if (alpn) {
    char *alpndup = r_strdup(alpn);

    if (!alpndup) {
      return R_NO_MEMORY;
    }

    config->alpn = alpndup;
  }

  return 0;
}

int nr_proxy_tunnel_config_copy(nr_proxy_tunnel_config *config, nr_proxy_tunnel_config **copypp)
{
  int r,_status;
  nr_proxy_tunnel_config *copy = 0;

  if ((r=nr_proxy_tunnel_config_create(&copy)))
    ABORT(r);

  if ((r=nr_proxy_tunnel_config_set_proxy(copy, config->proxy_host, config->proxy_port)))
    ABORT(r);

  if ((r=nr_proxy_tunnel_config_set_resolver(copy, config->resolver)))
    ABORT(r);

  if ((r=nr_proxy_tunnel_config_set_alpn(copy, config->alpn)))
    ABORT(r);

  *copypp = copy;

  _status=0;
abort:
  if (_status) {
    nr_proxy_tunnel_config_destroy(&copy);
  }
  return(_status);
}


int nr_socket_proxy_tunnel_create(nr_proxy_tunnel_config *config,
                                  nr_socket *inner,
                                  nr_socket **socketpp)
{
  int r, _status;
  nr_socket_proxy_tunnel *sock=0;
  void *sockv;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_proxy_tunnel_create");

  if (!config) {
    ABORT(R_BAD_ARGS);
  }

  if (!(sock=RCALLOC(sizeof(nr_socket_proxy_tunnel)))) {
    ABORT(R_NO_MEMORY);
  }

  sock->inner = inner;

  if ((r=nr_proxy_tunnel_config_copy(config, &sock->config)))
    ABORT(r);

  if ((r=nr_socket_create_int(sock, &nr_socket_proxy_tunnel_vtbl, socketpp))) {
    ABORT(r);
  }

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_proxy_tunnel_created");

  _status=0;
abort:
  if (_status) {
    sockv = sock;
    nr_socket_proxy_tunnel_destroy(&sockv);
  }
  return(_status);
}

int nr_socket_wrapper_factory_proxy_tunnel_wrap(void *obj,
                                                nr_socket *inner,
                                                nr_socket **socketpp)
{
  nr_socket_wrapper_factory_proxy_tunnel *wrapper = (nr_socket_wrapper_factory_proxy_tunnel *)obj;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_wrapper_factory_proxy_tunnel_wrap");

  return nr_socket_proxy_tunnel_create(wrapper->config, inner, socketpp);
}


int nr_socket_wrapper_factory_proxy_tunnel_destroy(void **objpp) {
  nr_socket_wrapper_factory_proxy_tunnel *wrapper;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_wrapper_factory_proxy_tunnel_destroy");

  if (!objpp || !*objpp)
    return 0;

  wrapper = (nr_socket_wrapper_factory_proxy_tunnel *)*objpp;
  *objpp = 0;

  nr_proxy_tunnel_config_destroy(&wrapper->config);
  RFREE(wrapper);

  return 0;
}

static nr_socket_wrapper_factory_vtbl proxy_tunnel_wrapper_vtbl = {
  nr_socket_wrapper_factory_proxy_tunnel_wrap,
  nr_socket_wrapper_factory_proxy_tunnel_destroy
};

int nr_socket_wrapper_factory_proxy_tunnel_create(nr_proxy_tunnel_config *config,
                                                  nr_socket_wrapper_factory **factory) {
  int r,_status;
  nr_socket_wrapper_factory_proxy_tunnel *wrapper=0;
  void *wrapperv;

  r_log(LOG_GENERIC,LOG_DEBUG,"nr_socket_wrapper_factory_proxy_tunnel_create");

  if (!(wrapper=RCALLOC(sizeof(nr_socket_wrapper_factory_proxy_tunnel))))
    ABORT(R_NO_MEMORY);

  if ((r=nr_proxy_tunnel_config_copy(config, &wrapper->config)))
    ABORT(r);

  if ((r=nr_socket_wrapper_factory_create_int(wrapper, &proxy_tunnel_wrapper_vtbl, factory)))
    ABORT(r);

  _status=0;
abort:
  if (_status) {
    wrapperv = wrapper;
    nr_socket_wrapper_factory_proxy_tunnel_destroy(&wrapperv);
  }
  return(_status);
}
