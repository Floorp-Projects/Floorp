/**
 * Copyright (c) 2013 Mozilla Foundation and Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __WEBVTT_PARSER_H__
# define __WEBVTT_PARSER_H__
# include "string.h"
# include "cue.h"
# include "error.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct webvtt_parser_t *webvtt_parser;

/**
 * Allows application to request error reporting
 */
typedef int ( WEBVTT_CALLBACK *webvtt_error_fn )( void *userdata,
                                                  webvtt_uint line,
                                                  webvtt_uint col,
                                                  webvtt_error error );

typedef void ( WEBVTT_CALLBACK *webvtt_cue_fn )( void *userdata,
                                                 webvtt_cue *cue );


WEBVTT_EXPORT webvtt_status
webvtt_create_parser( webvtt_cue_fn on_read, webvtt_error_fn on_error,
                      void * userdata, webvtt_parser *ppout );

WEBVTT_EXPORT void
webvtt_delete_parser( webvtt_parser parser );

WEBVTT_EXPORT webvtt_status
webvtt_parse_chunk( webvtt_parser self, const void *buffer, webvtt_uint len );

WEBVTT_EXPORT webvtt_status
webvtt_finish_parsing( webvtt_parser self );

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
