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

#ifndef __INTERN_CUETEXT_H__
# define __INTERN_CUETEXT_H__
# include <webvtt/util.h>
# include <webvtt/string.h>
# include <webvtt/cue.h>

typedef enum webvtt_token_type_t webvtt_token_type;
typedef enum webvtt_token_state_t webvtt_token_state;

typedef struct webvtt_cuetext_token_t webvtt_cuetext_token;
typedef struct webvtt_start_token_data_t webvtt_start_token_data;

/**
 * Enumerates token types.
 */
enum
webvtt_token_type_t {
  START_TOKEN, /* Identifies a webvtt_cue_text_start_tag_token. */
  END_TOKEN, /* Identifies a webvtt_cue_text_end_tag_token. */
  TIME_STAMP_TOKEN, /* Identifies a webvtt_cue_text_time_stamp_token. */
  TEXT_TOKEN /* Identifies a webvtt_cue_text_text_token. */
};

/**
 * Enumerates possible states that the cue text tokenizer can be in.
 */
enum
webvtt_token_state_t {
  DATA, /* Initial state. */
  ESCAPE, /* Parsing an escape value. */
  TAG, /* Reached a '<' character, about to start parsing a tag. */
  START_TAG, /* Parsing the beginning of a tag i.e. the tag name. */
  START_TAG_CLASS, /* Parsing a tag class. Reached when the tokenizer in the
                      START_TAG
                      state reaches a '.' character. */
  START_TAG_ANNOTATION, /* Parsing a tag annotation. Reached when the tokenizer
                           in the START_TAG_CLASS state reaches a TAB, SPACE, or
                           FORM FEED character. */
  END_TAG, /* Parsing an end tag. Reached when a '<' character is follwed by a
              '/' character. */
  TIME_STAMP_TAG /* Parsing a time stamp tag. Reached when a '<' character is
                    follwed by an integer character. */
};

/**
 * Represents a start tag in the cue text.
 * These take the form of <[TAG_NAME].[CLASSES] [POSSIBLE_ANNOTATION]> in the
 * cue text.
 */
struct
webvtt_start_token_data_t {
  webvtt_stringlist *css_classes;
  webvtt_string annotations;
};

/**
 * Contains a void pointer to a concrete token as well as a token type enum that
 * identifies what kind of token it is.
 */
struct
webvtt_cuetext_token_t {
  webvtt_token_type token_type;
  webvtt_string tag_name; // Only used for start token and end token types.
  union {
    webvtt_string text;
    webvtt_timestamp time_stamp;
    webvtt_start_token_data start_token_data;
  };
};

/**
 * Routines for creating cue text tokens.
 * Sets the passed token to the new token.
 */
WEBVTT_INTERN webvtt_status webvtt_create_token( webvtt_cuetext_token **token, webvtt_token_type token_type );
WEBVTT_INTERN webvtt_status webvtt_create_start_token( webvtt_cuetext_token **token, webvtt_string *tag_name,
    webvtt_stringlist *css_classes, webvtt_string *annotation );
WEBVTT_INTERN webvtt_status webvtt_create_end_token( webvtt_cuetext_token **token, webvtt_string *tag_name );
WEBVTT_INTERN webvtt_status webvtt_create_text_token( webvtt_cuetext_token **token, webvtt_string *text );
WEBVTT_INTERN webvtt_status webvtt_create_timestamp_token( webvtt_cuetext_token **token,
    webvtt_timestamp time_stamp );

/**
 * Returns true if the passed tag matches a tag name that accepts an annotation.
 */
WEBVTT_INTERN int tag_accepts_annotation( webvtt_string *tag_name );

/**
 * Routines for deleting cue text tokens.
 */
WEBVTT_INTERN void webvtt_delete_token( webvtt_cuetext_token **token );

/**
 * Converts the textual representation of a node kind into a particular kind.
 * I.E. tag_name of 'ruby' would create a ruby kind, etc.
 * Returns a WEBVTT_NOT_SUPPORTED if it does not find a valid tag name.
 */
WEBVTT_INTERN webvtt_status webvtt_node_kind_from_tag_name( webvtt_string *tag_name, webvtt_node_kind *kind );

/**
 * Creates a node from a valid token.
 * Returns WEBVTT_NOT_SUPPORTED if it does not find a valid tag name.
 */
WEBVTT_INTERN webvtt_status webvtt_create_node_from_token( webvtt_cuetext_token *token, webvtt_node **node, webvtt_node *parent );

/**
 * Tokenizes the cue text into something that can be easily understood by the
 * cue text parser.
 * Referenced from - http://dev.w3.org/html5/webvtt/#webvtt-cue-text-tokenizer
 */
WEBVTT_INTERN webvtt_status webvtt_tokenizer( webvtt_byte **position, webvtt_cuetext_token **token );

/**
 * Routines that take care of certain states in the webvtt cue text tokenizer.
 */

/**
 * Referenced from http://dev.w3.org/html5/webvtt/#webvtt-data-state
 */
WEBVTT_INTERN webvtt_status webvtt_data_state( webvtt_byte **position,
  webvtt_token_state *token_state, webvtt_string *result );

/**
 * Referenced from http://dev.w3.org/html5/webvtt/#webvtt-escape-state
 */
WEBVTT_INTERN webvtt_status webvtt_escape_state( webvtt_byte **position,
  webvtt_token_state *token_state, webvtt_string *result );

/**
 * Referenced from http://dev.w3.org/html5/webvtt/#webvtt-tag-state
 */
WEBVTT_INTERN webvtt_status webvtt_tag_state( webvtt_byte **position,
  webvtt_token_state *token_state, webvtt_string *result );

/**
 * Referenced from http://dev.w3.org/html5/webvtt/#webvtt-start-tag-state
 */
WEBVTT_INTERN webvtt_status webvtt_start_tag_state( webvtt_byte **position,
  webvtt_token_state *token_state, webvtt_string *result );

/**
 * Referenced from http://dev.w3.org/html5/webvtt/#webvtt-start-tag-class-state
 */
WEBVTT_INTERN webvtt_status webvtt_class_state( webvtt_byte **position,
  webvtt_token_state *token_state, webvtt_stringlist *css_classes );

/**
 * Referenced from 
 * http://dev.w3.org/html5/webvtt/#webvtt-start-tag-annotation-state
 */
WEBVTT_INTERN webvtt_status webvtt_annotation_state( webvtt_byte **position,
  webvtt_token_state *token_state, webvtt_string *annotation );

/**
 * Referenced from http://dev.w3.org/html5/webvtt/#webvtt-end-tag-state
 */
WEBVTT_INTERN webvtt_status webvtt_end_tag_state( webvtt_byte **position,
  webvtt_token_state *token_state, webvtt_string *result );

/**
 * Referenced from http://dev.w3.org/html5/webvtt/#webvtt-timestamp-tag-state
 */
WEBVTT_INTERN webvtt_status webvtt_timestamp_state( webvtt_byte **position,
  webvtt_token_state *token_state, webvtt_string *result );

WEBVTT_INTERN webvtt_status webvtt_parse_cuetext( webvtt_parser self, webvtt_cue *cue, webvtt_string *payload, int finished );

#endif
