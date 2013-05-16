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

#ifndef __WEBVTT_ERROR_H__
# define __WEBVTT_ERROR_H__
# include "util.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

  enum
  webvtt_error_t
  {
    /* There was a problem allocating something */
    WEBVTT_ALLOCATION_FAILED = 0,
    /**
     * 'WEBVTT' is not the first 6 characters in the file
     * (not counting UTF8 BOM)
     */
    WEBVTT_MALFORMED_TAG,
    /* An end-of-line sequence was expected, but not found. */
    WEBVTT_EXPECTED_EOL,
    /* A string of whitespace was expected, but not found. */
    WEBVTT_EXPECTED_WHITESPACE,
    /**
     * A string of whitespace was found, but was not expected 
     * (Recoverable error)
     */
    WEBVTT_UNEXPECTED_WHITESPACE,
    /* Long WEBVTT comment, decide whether to abort parsing or not */
    WEBVTT_LONG_COMMENT,
    /* A cue-id was too long to fit in the buffer. */
    WEBVTT_ID_TRUNCATED,
    /* A timestamp is malformed */
    WEBVTT_MALFORMED_TIMESTAMP,
    /* Expected a timestamp, but didn't find one */
    WEBVTT_EXPECTED_TIMESTAMP,
    /* Missing timestamp separator */
    WEBVTT_MISSING_CUETIME_SEPARATOR,
    /* Were expecting a separator, but got some garbage that we can't
       recover from instead. */
    WEBVTT_EXPECTED_CUETIME_SEPARATOR,
    /* Missing cuesetting delimiter */
    WEBVTT_MISSING_CUESETTING_DELIMITER,
    /* Invalid cuesetting delimiter */
    WEBVTT_INVALID_CUESETTING_DELIMITER,
    /* End-time is less than or equal to start time */
    WEBVTT_INVALID_ENDTIME,
    /* Invalid cue-setting */
    WEBVTT_INVALID_CUESETTING,
    /* unfinished cuetimes */
    WEBVTT_UNFINISHED_CUETIMES,
    /* valid-looking cuesetting with no keyword */
    WEBVTT_MISSING_CUESETTING_KEYWORD,
    /* 'vertical' setting already exists for this cue. */
    WEBVTT_VERTICAL_ALREADY_SET,
    /* Bad 'vertical' value */
    WEBVTT_VERTICAL_BAD_VALUE,
    /* 'line' setting already exists for this cue. */
    WEBVTT_LINE_ALREADY_SET,
    /* Bad 'line' value */
    WEBVTT_LINE_BAD_VALUE,
    /* 'position' setting already exists for this cue. */
    WEBVTT_POSITION_ALREADY_SET,
    /* Bad 'position' value */
    WEBVTT_POSITION_BAD_VALUE,
    /* 'size' setting already exists for this cue. */
    WEBVTT_SIZE_ALREADY_SET,
    /* Bad 'size' value */
    WEBVTT_SIZE_BAD_VALUE,
    /* 'align' setting already exists for this cue. */
    WEBVTT_ALIGN_ALREADY_SET,
    /* Bad 'align' value */
    WEBVTT_ALIGN_BAD_VALUE,
    /* A cue-text object contains the string "-->", which needs to be escaped */
    WEBVTT_CUE_CONTAINS_SEPARATOR,
    /* A webvtt cue contains only a cue-id, and no cuetimes or payload. */
    WEBVTT_CUE_INCOMPLETE,
  };
  typedef enum webvtt_error_t webvtt_error;

  WEBVTT_EXPORT const char *webvtt_strerror( webvtt_error );

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
