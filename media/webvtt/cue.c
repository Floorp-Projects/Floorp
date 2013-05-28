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

#include "parser_internal.h"
#include "cue_internal.h"

WEBVTT_EXPORT webvtt_status
webvtt_create_cue( webvtt_cue **pcue )
{
  webvtt_cue *cue;
  if( !pcue ) {
    return WEBVTT_INVALID_PARAM;
  }
  cue = (webvtt_cue *)webvtt_alloc0( sizeof(*cue) );
  if( !cue ) {
    return WEBVTT_OUT_OF_MEMORY;
  }
  /**
   * From http://dev.w3.org/html5/webvtt/#parsing (10/25/2012)
   *
   * Let cue's text track cue snap-to-lines flag be true.
   *
   * Let cue's text track cue line position be auto.
   *
   * Let cue's text track cue text position be 50.
   *
   * Let cue's text track cue size be 100.
   *
   * Let cue's text track cue alignment be middle alignment.
   */
  webvtt_ref( &cue->refs );
  webvtt_init_string( &cue->id );
  webvtt_init_string( &cue->body );
  cue->from = 0xFFFFFFFFFFFFFFFF;
  cue->until = 0xFFFFFFFFFFFFFFFF;
  cue->snap_to_lines = 1;
  cue->settings.position = 50;
  cue->settings.size = 100;
  cue->settings.align = WEBVTT_ALIGN_MIDDLE;
  cue->settings.line = WEBVTT_AUTO;
  cue->settings.vertical = WEBVTT_HORIZONTAL;

  *pcue = cue;
  return WEBVTT_SUCCESS;
}

WEBVTT_EXPORT void
webvtt_ref_cue( webvtt_cue *cue )
{
  if( cue ) {
    webvtt_ref( &cue->refs );
  }
}

WEBVTT_EXPORT void
webvtt_release_cue( webvtt_cue **pcue )
{
  if( pcue && *pcue ) {
    webvtt_cue *cue = *pcue;
    *pcue = 0;
    if( webvtt_deref( &cue->refs ) == 0 ) {
      webvtt_release_string( &cue->id );
      webvtt_release_string( &cue->body );
      webvtt_release_node( &cue->node_head );
      webvtt_free( cue );
    }
  }
}

WEBVTT_EXPORT int
webvtt_validate_cue( webvtt_cue *cue )
{
  if( cue ) {
    /**
     * validate cue-times (Can't do checks against previously parsed cuetimes.
     * That's the applications responsibility
     */
    if( BAD_TIMESTAMP(cue->from) || BAD_TIMESTAMP(cue->until) ) {
      goto error;
    }

    if( cue->until <= cue->from ) {
      goto error;
    }

    /**
     * Don't do any payload validation, because this would involve parsing the
     * payload, which is optional.
     */
    return 1;
  }

error:
  return 0;
}

WEBVTT_INTERN webvtt_bool
cue_is_incomplete( const webvtt_cue *cue ) {
  return !cue || ( cue->flags & CUE_HEADER_MASK ) == CUE_HAVE_ID;
}

