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
 * oggplay_tcp_reader.h
 * 
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 */

#ifndef __OGGPLAY_FILE_READER_H__
#define __OGGPLAY_FILE_READER_H__

#include <stdio.h>

/* we'll fill to the MAX_IN_MEMORY line, then write out to the
 * WRITE_THRESHOLD line
 */
#define TCP_READER_MAX_IN_MEMORY            128*1024
#define TCP_READER_WRITE_THRESHOLD           64*1024

typedef enum {
  TCP_READER_FROM_MEMORY,
  TCP_READER_FROM_FILE
} dataLocation;

typedef enum {
  OTRS_UNINITIALISED,
  OTRS_SOCKET_CREATED,
  OTRS_CONNECTED,
  OTRS_SENT_HEADER,
  OTRS_HTTP_RESPONDED,
  OTRS_INIT_COMPLETE
} OPTCPReaderState;

typedef struct {
  OggPlayReader     functions;
  OPTCPReaderState  state;
#ifdef _WIN32
  SOCKET            socket;
#else
  int               socket;
#endif
  unsigned char   * buffer;
  int               buffer_size;
  int               current_position;
  char            * location;
  char            * proxy;
  int               proxy_port;
  int               amount_in_memory;
  FILE            * backing_store;
  int               stored_offset;
  dataLocation      mode;
  int               duration;
} OggPlayTCPReader;

#endif
