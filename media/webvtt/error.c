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

#include <webvtt/error.h>

static const char *errstr[] = {
  /* WEBVTT_ALLOCATION_FAILED */ "error allocating object",
  /* WEBVTT_MALFORMED_TAG */ "malformed 'WEBVTT' tag",
  /* WEBVTT_EXPECTED_EOL */ "expected newline",
  /* WEBVTT_EXPECTED_WHITESPACE */ "expected whitespace",
  /* WEBVTT_UNEXPECTED_WHITESPACE */ "unexpected whitespace",
  /* WEBVTT_LONG_COMMENT */ "very long tag-comment",
  /* WEBVTT_ID_TRUNCATED */ "webvtt-cue-id truncated",
  /* WEBVTT_MALFORMED_TIMESTAMP */ "malformed webvtt-timestamp",
  /* WEBVTT_EXPECTED_TIMESTAMP */ "expected webvtt-timestamp",
  /* WEBVTT_MISSING_CUETIME_SEPARATOR */ "missing webvtt-cuetime-separator `-->'",
  /* WEBVTT_EXPECTED_CUETIME_SEPARATOR */ "expected webvtt-cuetime-separator `-->'",
  /* WEBVTT_MISSING_CUESETTING_DELIMITER */ "missing whitespace before webvtt-cuesetting",
  /* WEBVTT_INVALID_CUESETTING_DELIMITER */ "invalid webvtt-cuesetting key:value delimiter. expected `:'",
  /* WEBVTT_INVALID_ENDTIME */ "webvtt-cue end-time must have value greater than start-time",
  /* WEBVTT_INVALID_CUESETTING */ "unrecognized webvtt-cue-setting",
  /* WEBVTT_UNFINISHED_CUETIMES */ "unfinished webvtt cuetimes. expected 'start-timestamp --> end-timestamp'",
  /* WEBVTT_MISSING_CUESETTING_KEYWORD */ "missing setting keyword for value",
  /* WEBVTT_VERTICAL_ALREADY_SET */ "'vertical' cue-setting already used",
  /* WEBVTT_VERTICAL_BAD_VALUE */ "'vertical' setting must have a value of either 'lr' or 'rl'",
  /* WEBVTT_LINE_ALREADY_SET */ "'line' cue-setting already used",
  /* WEBVTT_LINE_BAD_VALUE */ "'line' cue-setting must have a value that is an integer (signed) line number, or percentage (%) from top of video display",
  /* WEBVTT_POSITION_ALREADY_SET */ "'position' cue-setting already used",
  /* WEBVTT_POSITION_BAD_VALUE */ "'position' cue-setting must be a percentage (%) value representing the position in the direction orthogonal to the 'line' setting",
  /* WEBVTT_SIZE_ALREADY_SET */ "'size' cue-setting already used",
  /* WEBVTT_SIZE_BAD_VALUE */ "'size' cue-setting must have percentage (%) value",
  /* WEBVTT_ALIGN_ALREADY_SET */ "'align' cue-setting already used",
  /* WEBVTT_ALIGN_BAD_VALUE */ "'align' cue-setting must have a value of either 'start', 'middle', or 'end'",
  /* WEBVTT_CUE_CONTAINS_SEPARATOR */ "cue-text line contains unescaped timestamp separator '-->'",
  /* WEBVTT_CUE_INCOMPLETE */ "cue contains cue-id, but is missing cuetimes or cue text",
};

/**
 * TODO:
 * Add i18n localized error strings with support for glibc and msvcrt locale
 * identifiers
 * (This might be too much work!)
 */
WEBVTT_EXPORT const char *
webvtt_strerror( webvtt_error err )
{
  if( err >= (sizeof(errstr) / sizeof(*errstr)) ) {
    return "";
  }
  return errstr[ err ];
}
