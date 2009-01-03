/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * oggplay_tcp_reader.c
 *
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 */

#include "oggplay_private.h"
#include "oggplay_tcp_reader.h"

#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <process.h>
#include <io.h>

#else
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <time.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include <assert.h>

#define PRINT_BUFFER(s,m) \
    printf("%s: in_mem: %d size: %d pos: %d stored: %d\n", \
            s, m->amount_in_memory, m->buffer_size, \
            m->current_position, m->stored_offset);

#ifndef WIN32
typedef int SOCKET;
#define INVALID_SOCKET  -1
#endif

#define START_TIMEOUT(ref) \
  (ref) = oggplay_sys_time_in_ms()

#ifdef WIN32
#define CHECK_ERROR(error) \
  (WSAGetLastError() == WSA##error)
#else
#define CHECK_ERROR(error) \
  (errno == error)
#endif

#define RETURN_ON_TIMEOUT_OR_CONTINUE(ref)        \
  if (oggplay_sys_time_in_ms() - (ref) > 500) {   \
    return E_OGGPLAY_TIMEOUT;                     \
  } else {                                        \
    oggplay_millisleep(10);                       \
    continue;                                     \
  }

#ifdef WIN32
int
oggplay_set_socket_blocking_state(SOCKET socket, int is_blocking) {
  u_long  io_mode = !is_blocking;
  if (ioctlsocket(socket, FIONBIO, &io_mode) == SOCKET_ERROR) {
     return 0;
  }
  return 1;
}
#else
int
oggplay_set_socket_blocking_state(SOCKET socket, int is_blocking) {
 if (fcntl(socket, F_SETFL, is_blocking ? 0 : O_NONBLOCK) == -1) {
    return 0;
  }
  return 1;
}
#endif

SOCKET
oggplay_create_socket() {
  SOCKET sock;

#ifdef WIN32
  WORD                  wVersionRequested;
  WSADATA               wsaData;
#ifdef HAVE_WINSOCK2
  wVersionRequested = MAKEWORD(2,2);
#else
  wVersionRequested = MAKEWORD(1,1);
#endif
  if (WSAStartup(wVersionRequested, &wsaData) == -1) {
    printf("Socket open error\n");
    return INVALID_SOCKET;
  }
  if (wsaData.wVersion != wVersionRequested) {
    printf("Incorrect winsock version [%d]\n", wVersionRequested);
    WSACleanup();
    return INVALID_SOCKET;
  }
#endif

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET) {
    printf("Could not create socket\n");
#ifdef WIN32
    WSACleanup();
#endif
    return INVALID_SOCKET;
  }

  return sock;
}

/*
 * this function guarantees it will return malloced versions of host and
 * path
 */
void
oggplay_hostname_and_path(char *location, char *proxy, int proxy_port,
                                  char **host, int *port, char **path) {


  char  * colon;
  char  * slash;
  char  * end_of_host;

  /* if we have a proxy installed this is all dead simple */
  if (proxy != NULL) {
    *host = strdup(proxy);
    *port = proxy_port;
    *path = strdup(location);
    return;
  }

  /* find start_pos */
  if (strncmp(location, "http://", 7) == 0) {
    location += 7;
  }

  colon = strchr(location, ':');
  slash = strchr(location, '/');

  /*
   * if both are null, then just set the simple defaults and return
   */
  if (colon == NULL && slash == NULL) {
    *host = strdup(location);
    *port = 80;
    *path = strdup("/");
    return;
  }

  /*
   * if there's a slash and it's before colon, there's no port.  Hence, after
   * this code, the only time that there's a port is when colon is non-NULL
   */
  if (slash != NULL && colon > slash) {
    colon = NULL;
  }

  /*
   * we might as well extract the port now.  We can also work out where
   * the end of the hostname is, as it's either the colon (if there's a port)
   * or the slash (if there's no port)
   */
  if (colon != NULL) {
    *port = (int)strtol(colon+1, NULL, 10);
    end_of_host = colon;
  } else {
    *port = 80;
    end_of_host = slash;
  }

  *host = strdup(location);
  (*host)[end_of_host - location] = '\0';

  if (slash == NULL) {
    *path = strdup("/");
    return;
  }

  *path = strdup(slash);

}

OggPlayErrorCode
oggplay_connect_to_host(SOCKET socket, struct sockaddr *addr) {

  ogg_int64_t           time_ref;

  START_TIMEOUT(time_ref);

  while (connect(socket, addr, sizeof(struct sockaddr_in)) < 0) {
    if (
        CHECK_ERROR(EINPROGRESS) || CHECK_ERROR(EALREADY)
#ifdef WIN32
          /* see http://msdn2.microsoft.com/en-us/library/ms737625.aspx */
        || CHECK_ERROR(EWOULDBLOCK) || CHECK_ERROR(EINVAL)
#endif
    ) {
      RETURN_ON_TIMEOUT_OR_CONTINUE(time_ref);
    }
    else if CHECK_ERROR(EISCONN)
    {
      break;
    }
    printf("Could not connect to host; error code is %d\n", errno);
    return CHECK_ERROR(ETIMEDOUT) ? E_OGGPLAY_TIMEOUT :
                                              E_OGGPLAY_SOCKET_ERROR;
  }

  return E_OGGPLAY_OK;

}


OggPlayErrorCode
oggplay_tcp_reader_initialise(OggPlayReader * opr, int block) {

  OggPlayTCPReader    * me = (OggPlayTCPReader *)opr;
  struct hostent      * he;
  struct sockaddr_in    addr;
  char                * host;
  char                * path;
  int                   port;
  int                   nbytes;
  int                   remaining;
  char                  http_request_header[1024];
  ogg_int64_t           time_ref;
  int                   r;

  char                * pos;
  int                   len;

  if (me == NULL) {
    return E_OGGPLAY_BAD_READER;
  }
  if (me->state == OTRS_INIT_COMPLETE) {
    return E_OGGPLAY_OK;
  }

  /*
   * Create the socket.
   */
  if (me->state == OTRS_UNINITIALISED) {
    assert(me->socket == INVALID_SOCKET);

    me->socket = oggplay_create_socket();
    if (me->socket == INVALID_SOCKET) {
      return E_OGGPLAY_SOCKET_ERROR;
    }

    me->state = OTRS_SOCKET_CREATED;
  }

  /*
   * If required, set the socket to non-blocking mode so we can
   * timeout and return control to the caller.
   */
  if (!block) {
    if (!oggplay_set_socket_blocking_state(me->socket, 0)) {
      return E_OGGPLAY_SOCKET_ERROR;
    }
  }

  /*
   * Extract the host name and the path from the location.
   */
  oggplay_hostname_and_path(me->location, me->proxy, me->proxy_port,
                              &host, &port, &path);


  /*
   * Prepare the HTTP request header now, so we can free all our
   * allocated memory before returning on any errors.
   */
  snprintf(http_request_header, 1024,
    "GET %s HTTP/1.0\n"
    "Host: %s\n"
    "User-Agent: AnnodexFirefoxPlugin/0.1\n"
    "Accept: */*\n"
    "Connection: Keep-Alive\n\n", path, host);

  he = gethostbyname(host);

  free(host);
  free(path);

  if (he == NULL) {
    printf("Host not found\n");
    return E_OGGPLAY_BAD_INPUT;
  }
  memcpy(&addr.sin_addr.s_addr, he->h_addr, he->h_length);
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  /*
   * Connect to the host.
   */
  if (me->state == OTRS_SOCKET_CREATED) {
    r = oggplay_connect_to_host(me->socket, (struct sockaddr *)&addr);
    if (r != E_OGGPLAY_OK) {
      return r;
    }

    me->state = OTRS_CONNECTED;
  }

  /*
   * Send the HTTP request header.
   *
   * If this times out after sending some, but not all, of the request header,
   * we'll end up sending the entire header string again. This is probably not
   * the best idea, so we may want to rework it at some time...
   */
  if (me->state == OTRS_CONNECTED) {
    oggplay_set_socket_blocking_state(me->socket, 1);
    pos = http_request_header;
    len = strlen(http_request_header);
#ifdef WIN32
    nbytes = send(me->socket, pos, len, 0);
#else
    nbytes = write(me->socket, pos, len);
#endif
    assert(nbytes == len);
    if (nbytes < 0) {
      return E_OGGPLAY_SOCKET_ERROR;
    }
    me->state = OTRS_SENT_HEADER;
  }

  /*
   * Strip out the HTTP response by finding the first Ogg packet.
   */
  if (me->state == OTRS_SENT_HEADER) {
    int offset;
    int found_http_response = 0;

    if (me->buffer == NULL) {
      me->buffer = (unsigned char*)malloc(TCP_READER_MAX_IN_MEMORY);
      me->buffer_size = TCP_READER_MAX_IN_MEMORY;
      me->amount_in_memory = 0;
    }

    while (1) {
      char *dpos;

      remaining = TCP_READER_MAX_IN_MEMORY - me->amount_in_memory - 1;
#ifdef WIN32
      nbytes = recv(me->socket, (char*)(me->buffer + me->amount_in_memory),
              remaining, 0);
#else
      nbytes = read(me->socket, (char*)(me->buffer + me->amount_in_memory),
              remaining);
#endif
      if (nbytes < 0) {
        return E_OGGPLAY_SOCKET_ERROR;
      } else if (nbytes == 0) {
        /*
         * End-of-file is an error here, because we should at least be able
         * to read a complete HTTP response header.
         */
        return E_OGGPLAY_END_OF_FILE;
      }

      me->amount_in_memory += nbytes;
      me->buffer[me->amount_in_memory] = '\0';

      if (me->amount_in_memory < 15) {
        continue;
      }

      if
      (
        (!found_http_response)
        &&
        strncmp((char *)me->buffer, "HTTP/1.1 200 OK", 15) != 0
        &&
        strncmp((char *)me->buffer, "HTTP/1.0 200 OK", 15) != 0
      )
      {
        return E_OGGPLAY_BAD_INPUT;
      } else {
        found_http_response = 1;
      }

      dpos = strstr((char *)me->buffer, "X-Content-Duration:");
      if (dpos != NULL) {
        me->duration = (int)(strtod(dpos + 20, NULL) * 1000);
      }

      pos = strstr((char *)(me->buffer), "OggS");
      if (pos != NULL) {
        break;
      }
    }

    offset = pos - (char *)(me->buffer);
    memmove(me->buffer, pos, me->amount_in_memory - offset);
    me->amount_in_memory -= offset;
    me->backing_store = tmpfile();
    fwrite(me->buffer, 1, me->amount_in_memory, me->backing_store);
    me->current_position = 0;
    me->stored_offset = me->amount_in_memory;
    me->amount_in_memory = 0;
    me->state = OTRS_HTTP_RESPONDED;
  }



  /*
   * Read in enough data to fill the buffer.
   */
  if (!block) {
    oggplay_set_socket_blocking_state(me->socket, 0);
  }

  if (me->state == OTRS_HTTP_RESPONDED) {
    remaining = TCP_READER_MAX_IN_MEMORY - me->amount_in_memory;
    START_TIMEOUT(time_ref);
    while (remaining > 0) {
#ifdef WIN32
      nbytes = recv(me->socket, (char*)(me->buffer + me->amount_in_memory),
              remaining, 0);
#else
      nbytes = read(me->socket, me->buffer + me->amount_in_memory, remaining);
#endif
      if (nbytes < 0) {
#ifdef WIN32
        if CHECK_ERROR(EWOULDBLOCK) {
#else
        if CHECK_ERROR(EAGAIN) {
#endif
          RETURN_ON_TIMEOUT_OR_CONTINUE(time_ref);
        }
        return E_OGGPLAY_SOCKET_ERROR;
      } else if (nbytes == 0) {
        /*
         * End-of-file is *not* an error here, it's just a really small file.
         */
        break;
      }
      me->amount_in_memory += nbytes;
      remaining -= nbytes;
    }
    fwrite(me->buffer, 1, me->amount_in_memory, me->backing_store);
    me->stored_offset += me->amount_in_memory;
    me->amount_in_memory = 0;
    me->state = OTRS_INIT_COMPLETE;
  }

  /*
   * Set the socket back to blocking mode.
   */
  if (!oggplay_set_socket_blocking_state(me->socket, 1)) {
    return E_OGGPLAY_SOCKET_ERROR;
  }

  return E_OGGPLAY_OK;
}


OggPlayErrorCode
oggplay_tcp_reader_destroy(OggPlayReader * opr) {

  OggPlayTCPReader * me = (OggPlayTCPReader *)opr;

  if (me->socket != INVALID_SOCKET) {
#ifdef WIN32
#ifdef HAVE_WINSOCK2
    shutdown(me->socket, SD_BOTH);
#endif
    closesocket(me->socket);
    WSACleanup();
#else
    close(me->socket);
#endif
  }

  free(me->buffer);
  free(me->location);
  if (me->backing_store != NULL) {
    fclose(me->backing_store);
  }
  free(me);
  return E_OGGPLAY_OK;
}

OggPlayErrorCode
grab_some_data(OggPlayTCPReader *me, int block) {

  int remaining;
  int r;

  if (me->socket == INVALID_SOCKET) return E_OGGPLAY_OK;

  /*
   * see if we can grab some more data
   * if we're not blocking, make sure there's some available data
   */
  if (!block) {
    struct timeval tv;
    fd_set reads;
    fd_set empty;

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&reads);
    FD_ZERO(&empty);
    FD_SET(me->socket, &reads);
    if (select(me->socket + 1, &reads, &empty, &empty, &tv) == 0) {
      return E_OGGPLAY_OK;
    }
  }

  remaining = me->buffer_size;
#ifdef WIN32
  r = recv(me->socket, (char*)(me->buffer + me->amount_in_memory),
                  remaining, 0);
#else
  r = read(me->socket, me->buffer + me->amount_in_memory, remaining);
#endif

  if (!block && r <= 0) {
#ifdef WIN32
#ifdef HAVE_WINSOCK2
    shutdown(me->socket, SD_BOTH);
#endif
    closesocket(me->socket);
    WSACleanup();
#else
    close(me->socket);
#endif
    me->socket = INVALID_SOCKET;
  }

  fwrite(me->buffer, 1, r, me->backing_store);
  me->stored_offset += r;

  return E_OGGPLAY_OK;
}

#define MIN(a,b) ((a)<(b)?(a):(b))

int
oggplay_tcp_reader_available(OggPlayReader * opr, ogg_int64_t current_bytes,
    ogg_int64_t current_time) {

  OggPlayTCPReader  * me;
  ogg_int64_t         tpb     = (current_time << 16) / current_bytes;

  me = (OggPlayTCPReader *)opr;

  if (me->socket == INVALID_SOCKET) {
    return me->duration;
  }

  grab_some_data(me, 0);
  if (me->duration > -1 && ((tpb * me->stored_offset) >> 16) > me->duration)
  {
    return me->duration;
  }
  return (int)((tpb * me->stored_offset) >> 16);

}

ogg_int64_t
oggplay_tcp_reader_duration(OggPlayReader * opr) {
  OggPlayTCPReader    *me = (OggPlayTCPReader *)opr;
  return me->duration;
}

static size_t
oggplay_tcp_reader_io_read(void * user_handle, void * buf, size_t n) {

  OggPlayTCPReader  * me = (OggPlayTCPReader *)user_handle;
  int                 len;

  grab_some_data(me, 0);

  fseek(me->backing_store, me->current_position, SEEK_SET);
  len = fread(buf, 1, n, me->backing_store);
  if (len == 0) {
    fseek(me->backing_store, 0, SEEK_END);
    grab_some_data(me, 1);
    fseek(me->backing_store, me->current_position, SEEK_SET);
    len = fread(buf, 1, n, me->backing_store);
  }
  me->current_position += len;
  fseek(me->backing_store, 0, SEEK_END);
  return len;
}

int
oggplay_tcp_reader_finished_retrieving(OggPlayReader *opr) {
  OggPlayTCPReader *me = (OggPlayTCPReader *)opr;
  return (me->socket == INVALID_SOCKET);
}


static int
oggplay_tcp_reader_io_seek(void * user_handle, long offset, int whence) {

  OggPlayTCPReader  * me = (OggPlayTCPReader *)user_handle;
  int                 r;

  fseek(me->backing_store, me->current_position, SEEK_SET);
  r = fseek(me->backing_store, offset, whence);
  me->current_position = ftell(me->backing_store);
  fseek(me->backing_store, 0, SEEK_END);

  return r;
}

static long
oggplay_tcp_reader_io_tell(void * user_handle) {

  OggPlayTCPReader  * me = (OggPlayTCPReader *)user_handle;

  return me->current_position;

}

OggPlayReader *
oggplay_tcp_reader_new(char *location, char *proxy, int proxy_port) {

  OggPlayTCPReader * me = (OggPlayTCPReader *)malloc (sizeof (OggPlayTCPReader));

  me->state = OTRS_UNINITIALISED;
  me->socket = INVALID_SOCKET;
  me->buffer = NULL;
  me->buffer_size = 0;
  me->current_position = 0;
  me->location = strdup(location);
  me->amount_in_memory = 0;
  me->backing_store = NULL;
  me->stored_offset = 0;

  me->proxy = proxy;
  me->proxy_port = proxy_port;

  me->functions.initialise = &oggplay_tcp_reader_initialise;
  me->functions.destroy = &oggplay_tcp_reader_destroy;
  me->functions.seek = NULL;
  me->functions.available = &oggplay_tcp_reader_available;
  me->functions.duration = &oggplay_tcp_reader_duration;
  me->functions.finished_retrieving = &oggplay_tcp_reader_finished_retrieving;
  me->functions.io_read = &oggplay_tcp_reader_io_read;
  me->functions.io_seek = &oggplay_tcp_reader_io_seek;
  me->functions.io_tell = &oggplay_tcp_reader_io_tell;

  return (OggPlayReader *)me;
}


