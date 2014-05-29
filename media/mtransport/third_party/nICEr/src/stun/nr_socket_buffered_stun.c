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

#include <nr_api.h>

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <assert.h>

#include "p_buf.h"
#include "nr_socket.h"
#include "stun.h"
#include "nr_socket_buffered_stun.h"


typedef struct nr_socket_buffered_stun_ {
  nr_socket *inner;
  nr_transport_addr remote_addr;
  int connected;

  /* Read state */
  int read_state;
#define NR_ICE_SOCKET_READ_NONE   0
#define NR_ICE_SOCKET_READ_HDR    1
#define NR_ICE_SOCKET_READ_FAILED 2
  UCHAR *buffer;
  size_t buffer_size;
  size_t bytes_needed;
  size_t bytes_read;

  /* Write state */
  nr_p_buf_ctx *p_bufs;
  nr_p_buf_head pending_writes;
  size_t pending;
  size_t max_pending;
} nr_socket_buffered_stun;

static int nr_socket_buffered_stun_destroy(void **objp);
static int nr_socket_buffered_stun_sendto(void *obj,const void *msg, size_t len,
  int flags, nr_transport_addr *to);
static int nr_socket_buffered_stun_recvfrom(void *obj,void * restrict buf,
  size_t maxlen, size_t *len, int flags, nr_transport_addr *from);
static int nr_socket_buffered_stun_getfd(void *obj, NR_SOCKET *fd);
static int nr_socket_buffered_stun_getaddr(void *obj, nr_transport_addr *addrp);
static int nr_socket_buffered_stun_close(void *obj);
static int nr_socket_buffered_stun_connect(void *sock, nr_transport_addr *addr);
static int nr_socket_buffered_stun_write(void *obj,const void *msg, size_t len, size_t *written);
static void nr_socket_buffered_stun_writable_cb(NR_SOCKET s, int how, void *arg);

static nr_socket_vtbl nr_socket_buffered_stun_vtbl={
  1,
  nr_socket_buffered_stun_destroy,
  nr_socket_buffered_stun_sendto,
  nr_socket_buffered_stun_recvfrom,
  nr_socket_buffered_stun_getfd,
  nr_socket_buffered_stun_getaddr,
  nr_socket_buffered_stun_connect,
  0,
  0,
  nr_socket_buffered_stun_close
};

int nr_socket_buffered_set_connected_to(nr_socket *sock, nr_transport_addr *remote_addr)
{
  nr_socket_buffered_stun *buf_sock = (nr_socket_buffered_stun *)sock->obj;
  int r, _status;

  if ((r=nr_transport_addr_copy(&buf_sock->remote_addr, remote_addr)))
    ABORT(r);

  buf_sock->connected = 1;

  _status=0;
abort:
  return(_status);
}

int nr_socket_buffered_stun_create(nr_socket *inner, int max_pending, nr_socket **sockp)
{
  int r, _status;
  nr_socket_buffered_stun *sock = 0;

  if (!(sock = RCALLOC(sizeof(nr_socket_buffered_stun))))
    ABORT(R_NO_MEMORY);

  sock->inner = inner;

  if ((r=nr_ip4_port_to_transport_addr(INADDR_ANY, 0, IPPROTO_UDP, &sock->remote_addr)))
    ABORT(r);

  /* TODO(ekr@rtfm.com): Check this */
  if (!(sock->buffer = RMALLOC(NR_STUN_MAX_MESSAGE_SIZE)))
    ABORT(R_NO_MEMORY);

  sock->read_state = NR_ICE_SOCKET_READ_NONE;
  sock->buffer_size = NR_STUN_MAX_MESSAGE_SIZE;
  sock->bytes_needed = sizeof(nr_stun_message_header);
  sock->connected = 0;

  STAILQ_INIT(&sock->pending_writes);
  if ((r=nr_p_buf_ctx_create(NR_STUN_MAX_MESSAGE_SIZE, &sock->p_bufs)))
    ABORT(r);
  sock->max_pending=max_pending;


  if ((r=nr_socket_create_int(sock, &nr_socket_buffered_stun_vtbl, sockp)))
    ABORT(r);

  _status=0;
abort:
  if (_status) {
    void *sock_v = sock;
    sock->inner = 0;  /* Give up ownership so we don't destroy */
    nr_socket_buffered_stun_destroy(&sock_v);
  }
  return(_status);
}

/* Note: This destroys the inner socket */
int nr_socket_buffered_stun_destroy(void **objp)
{
  nr_socket_buffered_stun *sock;
  NR_SOCKET fd;

  if (!objp || !*objp)
    return 0;

  sock = (nr_socket_buffered_stun *)*objp;
  *objp = 0;

  /* Free the buffer if needed */
  RFREE(sock->buffer);

  /* Cancel waiting on the socket */
  if (sock->inner && !nr_socket_getfd(sock->inner, &fd)) {
    NR_ASYNC_CANCEL(fd, NR_ASYNC_WAIT_WRITE);
  }

  nr_p_buf_free_chain(sock->p_bufs, &sock->pending_writes);
  nr_p_buf_ctx_destroy(&sock->p_bufs);
  nr_socket_destroy(&sock->inner);
  RFREE(sock);

  return 0;
}

static int nr_socket_buffered_stun_sendto(void *obj,const void *msg, size_t len,
  int flags, nr_transport_addr *to)
{
  nr_socket_buffered_stun *sock = (nr_socket_buffered_stun *)obj;

  int r, _status;
  size_t written;

  /* Check that we are writing to the connected address if
     connected */
  if (!nr_transport_addr_is_wildcard(&sock->remote_addr)) {
    if (nr_transport_addr_cmp(&sock->remote_addr, to, NR_TRANSPORT_ADDR_CMP_MODE_ALL)) {
      r_log(LOG_GENERIC, LOG_ERR, "Sendto on connected socket doesn't match");
      ABORT(R_BAD_DATA);
    }
  }

  if ((r=nr_socket_buffered_stun_write(obj, msg, len, &written)))
    ABORT(r);

  if (len != written)
    ABORT(R_IO_ERROR);

  _status=0;
abort:
  return _status;
}

static void nr_socket_buffered_stun_failed(nr_socket_buffered_stun *sock)
  {
    sock->read_state = NR_ICE_SOCKET_READ_FAILED;
  }

static int nr_socket_buffered_stun_recvfrom(void *obj,void * restrict buf,
  size_t maxlen, size_t *len, int flags, nr_transport_addr *from)
{
  int r, _status;
  size_t bytes_read;
  nr_socket_buffered_stun *sock = (nr_socket_buffered_stun *)obj;

  if (sock->read_state == NR_ICE_SOCKET_READ_FAILED) {
    ABORT(R_FAILED);
  }

reread:
  /* Read all the expected bytes */
  assert(sock->bytes_needed <= sock->buffer_size - sock->bytes_read);

  if(r=nr_socket_read(sock->inner,
                      sock->buffer + sock->bytes_read,
                      sock->bytes_needed, &bytes_read, 0))
    ABORT(r);

  assert(bytes_read <= sock->bytes_needed);
  sock->bytes_needed -= bytes_read;
  sock->bytes_read += bytes_read;

  /* Unfinished */
  if (sock->bytes_needed)
    ABORT(R_WOULDBLOCK);

  /* No more bytes expeected */
  if (sock->read_state == NR_ICE_SOCKET_READ_NONE) {
    int tmp_length;
    size_t remaining_length;

    /* Parse the header */
    if (r = nr_stun_message_length(sock->buffer, sock->bytes_read, &tmp_length))
      ABORT(r);
    assert(tmp_length >= 0);
    if (tmp_length < 0)
      ABORT(R_BAD_DATA);
    remaining_length = tmp_length;

    /* Check to see if we have enough room */
    if ((sock->buffer_size - sock->bytes_read) < remaining_length)
      ABORT(R_BAD_DATA);

    /* Set ourselves up to read the rest of the data */
    sock->read_state = NR_ICE_SOCKET_READ_HDR;
    sock->bytes_needed = remaining_length;

    goto reread;
  }

  if (maxlen < sock->bytes_read)
    ABORT(R_BAD_ARGS);

  memcpy(buf, sock->buffer, sock->bytes_read);
  *len = sock->bytes_read;
  sock->read_state = NR_ICE_SOCKET_READ_NONE;
  sock->bytes_read = 0;
  sock->bytes_needed = sizeof(nr_stun_message_header);

  assert(!nr_transport_addr_is_wildcard(&sock->remote_addr));
  if (!nr_transport_addr_is_wildcard(&sock->remote_addr)) {
    if ((r=nr_transport_addr_copy(from, &sock->remote_addr)))
      ABORT(r);
  }

  _status=0;
abort:
  if (_status && (_status != R_WOULDBLOCK)) {
    nr_socket_buffered_stun_failed(sock);
  }

  return(_status);
}

static int nr_socket_buffered_stun_getfd(void *obj, NR_SOCKET *fd)
{
  nr_socket_buffered_stun *sock = (nr_socket_buffered_stun *)obj;

  return nr_socket_getfd(sock->inner, fd);
}

static int nr_socket_buffered_stun_getaddr(void *obj, nr_transport_addr *addrp)
{
  nr_socket_buffered_stun *sock = (nr_socket_buffered_stun *)obj;

  return nr_socket_getaddr(sock->inner, addrp);
}

static int nr_socket_buffered_stun_close(void *obj)
{
  nr_socket_buffered_stun *sock = (nr_socket_buffered_stun *)obj;
  NR_SOCKET fd;

  /* Cancel waiting on the socket */
  if (sock->inner && !nr_socket_getfd(sock->inner, &fd)) {
    NR_ASYNC_CANCEL(fd, NR_ASYNC_WAIT_WRITE);
  }

  return nr_socket_close(sock->inner);
}

static void nr_socket_buffered_stun_connected_cb(NR_SOCKET s, int how, void *arg)
{
  nr_socket_buffered_stun *sock = (nr_socket_buffered_stun *)arg;

  assert(!sock->connected);

  sock->connected = 1;
  if (sock->pending)
    nr_socket_buffered_stun_writable_cb(s, how, arg);
}

static int nr_socket_buffered_stun_connect(void *obj, nr_transport_addr *addr)
{
  nr_socket_buffered_stun *sock = (nr_socket_buffered_stun *)obj;
  int r, _status;

  if ((r=nr_transport_addr_copy(&sock->remote_addr, addr)))
    ABORT(r);

  if ((r=nr_socket_connect(sock->inner, addr))) {
    if (r == R_WOULDBLOCK) {
      NR_SOCKET fd;

      if ((r=nr_socket_getfd(sock->inner, &fd)))
        ABORT(r);

      NR_ASYNC_WAIT(fd, NR_ASYNC_WAIT_WRITE, nr_socket_buffered_stun_connected_cb, sock);
      ABORT(R_WOULDBLOCK);
    }
    ABORT(r);
  } else {
    sock->connected = 1;
  }

  _status=0;
abort:
  return(_status);
}

static int nr_socket_buffered_stun_write(void *obj,const void *msg, size_t len, size_t *written)
{
  nr_socket_buffered_stun *sock = (nr_socket_buffered_stun *)obj;
  int already_armed = 0;
  int r,_status;
  size_t written2 = 0;
  size_t original_len = len;

  /* Buffers are close to full, report error. Do this now so we never
     get partial writes */
  if ((sock->pending + len) > sock->max_pending) {
    r_log(LOG_GENERIC, LOG_INFO, "Write buffer for %s full", sock->remote_addr.as_string);
    ABORT(R_WOULDBLOCK);
  }


  if (sock->connected && !sock->pending) {
    r = nr_socket_write(sock->inner, msg, len, &written2, 0);
    if (r) {
      if (r != R_WOULDBLOCK)
        ABORT(r);

      written2=0;
    }
  } else {
    already_armed = 1;
  }

  /* Buffer what's left */
  len -= written2;

  if (len) {
    if ((r=nr_p_buf_write_to_chain(sock->p_bufs, &sock->pending_writes,
                                     ((UCHAR *)msg) + written2, len)))
      ABORT(r);

    sock->pending += len;
  }

  if (sock->pending && !already_armed) {
    NR_SOCKET fd;

    if ((r=nr_socket_getfd(sock->inner, &fd)))
      ABORT(r);

    NR_ASYNC_WAIT(fd, NR_ASYNC_WAIT_WRITE, nr_socket_buffered_stun_writable_cb, sock);
  }

  *written = original_len;

  _status=0;
abort:
  return _status;
}

static void nr_socket_buffered_stun_writable_cb(NR_SOCKET s, int how, void *arg)
{
  nr_socket_buffered_stun *sock = (nr_socket_buffered_stun *)arg;
  int r,_status;
  nr_p_buf *n1, *n2;

  /* Try to flush */
  STAILQ_FOREACH_SAFE(n1, &sock->pending_writes, entry, n2) {
    size_t written = 0;

    if ((r=nr_socket_write(sock->inner, n1->data + n1->r_offset,
                           n1->length - n1->r_offset,
                           &written, 0))) {

      ABORT(r);
    }

    n1->r_offset += written;
    assert(sock->pending >= written);
    sock->pending -= written;

    if (n1->r_offset < n1->length) {
      /* We wrote something, but not everything */
      ABORT(R_WOULDBLOCK);
    }

    /* We are done with this p_buf */
    STAILQ_REMOVE_HEAD(&sock->pending_writes, entry);
    nr_p_buf_free(sock->p_bufs, n1);
  }

  assert(!sock->pending);
  _status=0;
abort:
  if (_status && _status != R_WOULDBLOCK) {
    /* TODO(ekr@rtfm.com): Mark the socket as failed */
  }
}
