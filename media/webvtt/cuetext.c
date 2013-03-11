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

#include <stdlib.h>
#include <string.h>
#include "parser_internal.h"
#include "cuetext_internal.h"
#include "cue_internal.h"
#include "string_internal.h"

static void webvtt_skipwhite( webvtt_byte **position );

#ifdef min
# undef min
#endif
#define min(a,b) ( (a) < (b) ? (a) : (b) )

/**
 * ERROR macro used for webvtt_parse_cuetext
 */
#undef ERROR
#define ERROR(code) \
do \
{ \
  if( self->error ) \
    if( self->error( self->userdata, line, col, code ) < 0 ) \
      return WEBVTT_PARSE_ERROR; \
} while(0)

/**
 * Macros for return statuses based on memory operations.
 * This is to avoid many if statements checking for multiple memory operation
 * return statuses in functions.
 */
#define CHECK_MEMORY_OP(status) \
  if( status != WEBVTT_SUCCESS ) \
    return status; \

#define CHECK_MEMORY_OP_JUMP(status_var, returned_status) \
  if( returned_status != WEBVTT_SUCCESS) \
  { \
    status_var = returned_status; \
    goto dealloc; \
  } \

/**
 * This will only work on null-terminated strings, remember that!
 */
static void
webvtt_skipwhite( webvtt_byte **position )
{
  webvtt_byte *p = *position;
  while( *p && webvtt_iswhite(*p) ) {
    ++p;
  }
  *position = p;
}

WEBVTT_INTERN webvtt_status
webvtt_create_token( webvtt_cuetext_token **token, webvtt_token_type token_type )
{
  webvtt_cuetext_token *temp_token = (webvtt_cuetext_token *)webvtt_alloc0( sizeof(*temp_token) );

  if( !temp_token ) {
    return WEBVTT_OUT_OF_MEMORY;
  }

  temp_token->token_type = token_type;
  *token = temp_token;

  return WEBVTT_SUCCESS;
}

WEBVTT_INTERN webvtt_status
webvtt_create_start_token( webvtt_cuetext_token **token, webvtt_string *tag_name,
    webvtt_stringlist *css_classes, webvtt_string *annotation )
{
  webvtt_status status;
  webvtt_start_token_data sd;
  
  if( WEBVTT_FAILED( status = webvtt_create_token( token, START_TOKEN ) ) ) {
    return status;
  }
  
  webvtt_copy_string( &(*token)->tag_name, tag_name );
  webvtt_copy_stringlist( &sd.css_classes, css_classes );
  webvtt_copy_string( &sd.annotations, annotation );
  
  (*token)->start_token_data = sd;
    
  return WEBVTT_SUCCESS;
}

WEBVTT_INTERN webvtt_status
webvtt_create_end_token( webvtt_cuetext_token **token, webvtt_string *tag_name )
{
  webvtt_status status;

  if( WEBVTT_FAILED( status = webvtt_create_token( token, END_TOKEN ) ) ) {
    return status;
  }

  webvtt_copy_string( &(*token)->tag_name, tag_name );
  
  return WEBVTT_SUCCESS;
}

WEBVTT_INTERN webvtt_status
webvtt_create_text_token( webvtt_cuetext_token **token, webvtt_string *text )
{
  webvtt_status status;

  if( WEBVTT_FAILED( status = webvtt_create_token( token, TEXT_TOKEN ) ) ) {
    return status;
  }

  webvtt_copy_string( &(*token)->text, text);

  return WEBVTT_SUCCESS;
}

WEBVTT_INTERN webvtt_status
webvtt_create_timestamp_token( webvtt_cuetext_token **token, webvtt_timestamp time_stamp )
{
  webvtt_status status;

  if( WEBVTT_FAILED( status = webvtt_create_token( token, TIME_STAMP_TOKEN ) ) ) {
    return status;
  }

  (*token)->time_stamp = time_stamp;

  return WEBVTT_SUCCESS;
}

WEBVTT_INTERN void
webvtt_delete_token( webvtt_cuetext_token **token )
{
  webvtt_start_token_data data;
  webvtt_cuetext_token *t;
  
  if( !token ) {
    return;
  }
  if( !*token ) {
    return;
  }  
  t = *token;
  
  /** 
   * Note that time stamp tokens do not need to free any internal data because 
   * they do not allocate anything. 
   */
  switch( t->token_type ) {
    case START_TOKEN:
      data = t->start_token_data;
      webvtt_release_stringlist( &data.css_classes );
      webvtt_release_string( &data.annotations );
      webvtt_release_string( &t->tag_name );
      break;
    case END_TOKEN:
      webvtt_release_string( &t->tag_name );
      break;
    case TEXT_TOKEN:
      webvtt_release_string( &t->text );
      break;
  }
  webvtt_free( t );
  *token = 0;
}

WEBVTT_INTERN int
tag_accepts_annotation( webvtt_string *tag_name )
{
  return webvtt_string_is_equal( tag_name, "v", 1 );
}

WEBVTT_INTERN webvtt_status
webvtt_node_kind_from_tag_name( webvtt_string *tag_name, webvtt_node_kind *kind )
{
  if( !tag_name || !kind ) {
    return WEBVTT_INVALID_PARAM;
  }

  if( webvtt_string_length(tag_name) == 1 ) {
    switch( webvtt_string_text(tag_name)[0] ) {
      case( UTF8_B ):
        *kind = WEBVTT_BOLD;
        break;
      case( UTF8_I ):
        *kind = WEBVTT_ITALIC;
        break;
      case( UTF8_U ):
        *kind = WEBVTT_UNDERLINE;
        break;
      case( UTF8_C ):
        *kind = WEBVTT_CLASS;
        break;
      case( UTF8_V ):
        *kind = WEBVTT_VOICE;
        break;
    }
  } else if( webvtt_string_is_equal( tag_name, "ruby", 4 ) ) {
    *kind = WEBVTT_RUBY;
  } else if( webvtt_string_is_equal( tag_name, "rt", 2 ) ) {
    *kind = WEBVTT_RUBY_TEXT;
  } else {
    return WEBVTT_INVALID_TAG_NAME;
  }

  return WEBVTT_SUCCESS;
}

WEBVTT_INTERN webvtt_status
webvtt_create_node_from_token( webvtt_cuetext_token *token, webvtt_node **node, webvtt_node *parent )
{
  webvtt_node_kind kind;

  if( !token || !node || !parent ) {
    return WEBVTT_INVALID_PARAM;
  }

  /** 
   * We've recieved a node that is not null.
   * In order to prevent memory leaks caused by overwriting a node which the 
   * caller has not released return unsuccessful.
   */
  if( *node ) {
    return WEBVTT_UNSUCCESSFUL;
  }
  
  switch ( token->token_type ) {
    case( TEXT_TOKEN ):
      return webvtt_create_text_node( node, parent, &token->text );
      break;
    case( START_TOKEN ):

      CHECK_MEMORY_OP( webvtt_node_kind_from_tag_name( &token->tag_name, &kind) );

      return webvtt_create_internal_node( node, parent, kind,
        token->start_token_data.css_classes, &token->start_token_data.annotations );

      break;
    case ( TIME_STAMP_TOKEN ):
      return webvtt_create_timestamp_node( node, parent, token->time_stamp );
      break;
    default:
      return WEBVTT_INVALID_TOKEN_TYPE;
  }
}

WEBVTT_INTERN webvtt_status
webvtt_data_state( webvtt_byte **position, webvtt_token_state *token_state, 
                   webvtt_string *result )
{
  for ( ; *token_state == DATA; (*position)++ ) {
    switch( **position ) {
      case UTF8_AMPERSAND:
        *token_state = ESCAPE;
        break;
      case UTF8_LESS_THAN:
        if( webvtt_string_length(result) == 0 ) {
          *token_state = TAG;
        } else {
          return WEBVTT_SUCCESS;
        }
        break;
      case UTF8_NULL_BYTE:
        return WEBVTT_SUCCESS;
        break;
      default:
        CHECK_MEMORY_OP( webvtt_string_putc( result, *position[0] ) );
        break;
    }
  }

  return WEBVTT_UNFINISHED;
}

/**
 * Definitions for escape sequence replacement strings.
 */
#define RLM_REPLACE_LENGTH    3
#define LRM_REPLACE_LENGTH    3
#define NBSP_REPLACE_LENGTH   2
 
webvtt_byte rlm_replace[RLM_REPLACE_LENGTH] = { UTF8_RIGHT_TO_LEFT_1, 
    UTF8_RIGHT_TO_LEFT_2, UTF8_RIGHT_TO_LEFT_3 };
webvtt_byte lrm_replace[LRM_REPLACE_LENGTH] = { UTF8_LEFT_TO_RIGHT_1,
  UTF8_LEFT_TO_RIGHT_2, UTF8_LEFT_TO_RIGHT_3 };
webvtt_byte nbsp_replace[NBSP_REPLACE_LENGTH] = { UTF8_NO_BREAK_SPACE_1,
  UTF8_NO_BREAK_SPACE_2 };
  
WEBVTT_INTERN webvtt_status
webvtt_escape_state( webvtt_byte **position, webvtt_token_state *token_state, 
                     webvtt_string *result )
{
  webvtt_string buffer;
  webvtt_status status = WEBVTT_SUCCESS;

  CHECK_MEMORY_OP_JUMP( status, webvtt_create_string( 1, &buffer ) );

  /**
   * Append ampersand here because the algorithm is not able to add it to the
   * buffer when it reads it in the DATA state tokenizer.
   */
  CHECK_MEMORY_OP_JUMP( status, webvtt_string_putc( &buffer, UTF8_AMPERSAND ) );

  for( ; *token_state == ESCAPE; (*position)++ ) {
    /**
     * We have encountered a token termination point.
     * Append buffer to result and return success.
     */
    if( **position == UTF8_NULL_BYTE || **position == UTF8_LESS_THAN ) {
      CHECK_MEMORY_OP_JUMP( status, webvtt_string_append_string( result, &buffer ) );
      goto dealloc;
    }
    /**
     * This means we have enocuntered a malformed escape character sequence.
     * This means that we need to add that malformed text to the result and
     * recreate the buffer to prepare for a new escape sequence.
     */
    else if( **position == UTF8_AMPERSAND ) {
      CHECK_MEMORY_OP_JUMP( status, webvtt_string_append_string( result, &buffer ) );
      webvtt_release_string( &buffer );
      CHECK_MEMORY_OP_JUMP( status, webvtt_create_string( 1, &buffer ) );
      CHECK_MEMORY_OP_JUMP( status, webvtt_string_putc( &buffer, *position[0] ) );
    }
    /**
     * We've encountered the semicolon which is the end of an escape sequence.
     * Check if buffer contains a valid escape sequence and if it does append
     * the interpretation to result and change the state to DATA.
     */
    else if( **position == UTF8_SEMI_COLON ) {
      if( webvtt_string_is_equal( &buffer, "&amp", 4 ) ) {
        CHECK_MEMORY_OP_JUMP( status, webvtt_string_putc( result, '&' ) );
      } else if( webvtt_string_is_equal( &buffer, "&lt", 3 ) ) {
        CHECK_MEMORY_OP_JUMP( status, webvtt_string_putc( result, '<' ) );
      } else if( webvtt_string_is_equal( &buffer, "&gt", 3 ) ) {
        CHECK_MEMORY_OP_JUMP( status, webvtt_string_putc( result, '>' ) );
      } else if( webvtt_string_is_equal( &buffer, "&rlm", 4 ) ) {
        CHECK_MEMORY_OP_JUMP( status, webvtt_string_append( result, rlm_replace, RLM_REPLACE_LENGTH ) );
      } else if( webvtt_string_is_equal( &buffer, "&lrm", 4 ) ) {
        CHECK_MEMORY_OP_JUMP( status, webvtt_string_append( result, lrm_replace, LRM_REPLACE_LENGTH ) );
      } else if( webvtt_string_is_equal( &buffer, "&nbsp", 5 ) ) {
        CHECK_MEMORY_OP_JUMP( status, webvtt_string_append( result, nbsp_replace, NBSP_REPLACE_LENGTH ) );
      } else {
        CHECK_MEMORY_OP_JUMP( status, webvtt_string_append_string( result, &buffer ) );
        CHECK_MEMORY_OP_JUMP( status, webvtt_string_putc( result, **position ) );
      }

      *token_state = DATA;
      status = WEBVTT_UNFINISHED;
    }
    /**
     * Character is alphanumeric. This means we are in the body of the escape
     * sequence.
     */
    else if( webvtt_isalphanum( **position ) ) {
      CHECK_MEMORY_OP_JUMP( status, webvtt_string_putc( &buffer, **position ) );
    }
    /**
     * If we have not found an alphanumeric character then we have encountered
     * a malformed escape sequence. Add buffer to result and continue to parse
     * in DATA state.
     */
    else {
      CHECK_MEMORY_OP_JUMP( status, webvtt_string_append_string( result, &buffer ) );
      CHECK_MEMORY_OP_JUMP( status, webvtt_string_putc( result, **position ) );
      status = WEBVTT_UNFINISHED;
      *token_state = DATA;
    }
  }

dealloc:
  webvtt_release_string( &buffer );

  return status;
}

WEBVTT_INTERN webvtt_status
webvtt_tag_state( webvtt_byte **position, webvtt_token_state *token_state, 
                  webvtt_string *result )
{
  for( ; *token_state == TAG; (*position)++ ) {
    if( **position == UTF8_TAB || **position == UTF8_LINE_FEED ||
        **position == UTF8_CARRIAGE_RETURN || **position == UTF8_FORM_FEED ||
        **position == UTF8_SPACE ) {
      *token_state = START_TAG_ANNOTATION;
    } else if( webvtt_isdigit( **position )  ) {
      CHECK_MEMORY_OP( webvtt_string_putc( result, **position ) );
      *token_state = TIME_STAMP_TAG;
    } else {
      switch( **position ) {
        case UTF8_FULL_STOP:
          *token_state = START_TAG_CLASS;
          break;
        case UTF8_SOLIDUS:
          *token_state = END_TAG;
          break;
        case UTF8_GREATER_THAN:
          return WEBVTT_SUCCESS;
          break;
        case UTF8_NULL_BYTE:
          return WEBVTT_SUCCESS;
          break;
        default:
          CHECK_MEMORY_OP( webvtt_string_putc( result, **position ) );
          *token_state = START_TAG;
      }
    }
  }

  return WEBVTT_UNFINISHED;
}

WEBVTT_INTERN webvtt_status
webvtt_start_tag_state( webvtt_byte **position, webvtt_token_state *token_state, 
                        webvtt_string *result )
{
  for( ; *token_state == START_TAG; (*position)++ ) {
    if( **position == UTF8_TAB || **position == UTF8_FORM_FEED ||
        **position == UTF8_SPACE || **position == UTF8_LINE_FEED ||
        **position == UTF8_CARRIAGE_RETURN ) {
      *token_state = START_TAG_ANNOTATION;
    } else {
      switch( **position ) {
        case UTF8_TAB:
          *token_state = START_TAG_ANNOTATION;
          break;
        case UTF8_FULL_STOP:
          *token_state = START_TAG_CLASS;
          break;
        case UTF8_GREATER_THAN:
          return WEBVTT_SUCCESS;
          break;
        default:
          CHECK_MEMORY_OP( webvtt_string_putc( result, **position ) );
          break;
      }
    }
  }

  return WEBVTT_UNFINISHED;
}

WEBVTT_INTERN webvtt_status
webvtt_class_state( webvtt_byte **position, webvtt_token_state *token_state, 
                    webvtt_stringlist *css_classes )
{
  webvtt_string buffer;
  webvtt_status status = WEBVTT_SUCCESS;

  CHECK_MEMORY_OP( webvtt_create_string( 1, &buffer ) );

  for( ; *token_state == START_TAG_CLASS; (*position)++ ) {
    if( **position == UTF8_TAB || **position == UTF8_FORM_FEED ||
        **position == UTF8_SPACE || **position == UTF8_LINE_FEED ||
        **position == UTF8_CARRIAGE_RETURN) {
      CHECK_MEMORY_OP_JUMP( status, webvtt_stringlist_push( css_classes, &buffer ) );
      *token_state = START_TAG_ANNOTATION;
      return WEBVTT_SUCCESS;
    } else if( **position == UTF8_GREATER_THAN || **position == UTF8_NULL_BYTE ) {
      CHECK_MEMORY_OP_JUMP( status, webvtt_stringlist_push( css_classes, &buffer ) );
      webvtt_release_string( &buffer );
      return WEBVTT_SUCCESS;
    } else if( **position == UTF8_FULL_STOP ) {
      CHECK_MEMORY_OP_JUMP( status, webvtt_stringlist_push( css_classes, &buffer ) );
      webvtt_release_string( &buffer );
      CHECK_MEMORY_OP( webvtt_create_string( 1, &buffer ) );
    } else {
      CHECK_MEMORY_OP_JUMP( status, webvtt_string_putc( &buffer, **position ) );
    }
  }

dealloc:
  webvtt_release_string( &buffer );

  return status;
}

WEBVTT_INTERN webvtt_status
webvtt_annotation_state( webvtt_byte **position, webvtt_token_state *token_state, 
                         webvtt_string *annotation )
{
  for( ; *token_state == START_TAG_ANNOTATION; (*position)++ ) {
    if( **position == UTF8_NULL_BYTE || **position == UTF8_GREATER_THAN ) {
      return WEBVTT_SUCCESS;
    }
    CHECK_MEMORY_OP( webvtt_string_putc( annotation, **position ) );
  }

  return WEBVTT_UNFINISHED;
}

WEBVTT_INTERN webvtt_status
webvtt_end_tag_state( webvtt_byte **position, webvtt_token_state *token_state, 
                      webvtt_string *result )
{
  for( ; *token_state == END_TAG; (*position)++ ) {
    if( **position == UTF8_GREATER_THAN || **position == UTF8_NULL_BYTE ) {
      return WEBVTT_SUCCESS;
    }
    CHECK_MEMORY_OP( webvtt_string_putc( result, **position ) );
  }

  return WEBVTT_UNFINISHED;
}

WEBVTT_INTERN webvtt_status
webvtt_timestamp_state( webvtt_byte **position, webvtt_token_state *token_state, 
                        webvtt_string *result )
{
  for( ; *token_state == TIME_STAMP_TAG; (*position)++ ) {
    if( **position == UTF8_GREATER_THAN || **position == UTF8_NULL_BYTE ) {
      return WEBVTT_SUCCESS;
    }
    CHECK_MEMORY_OP( webvtt_string_putc( result, **position ) );
  }

  return WEBVTT_UNFINISHED;
}

/**
 * Need to set up differently.
 * Get a status in order to return at end and release memeory.
 */
WEBVTT_INTERN webvtt_status
webvtt_cuetext_tokenizer( webvtt_byte **position, webvtt_cuetext_token **token )
{
  webvtt_token_state token_state = DATA;
  webvtt_string result, annotation;
  webvtt_stringlist *css_classes;
  webvtt_timestamp time_stamp = 0;
  webvtt_status status = WEBVTT_UNFINISHED;

  if( !position ) {
    return WEBVTT_INVALID_PARAM;
  }

  webvtt_create_string( 10, &result );
  webvtt_create_string( 10, &annotation );
  webvtt_create_stringlist( &css_classes );
  
  /**
   * Loop while the tokenizer is not finished.
   * Based on the state of the tokenizer enter a function to handle that
   * particular tokenizer state. Those functions will loop until they either
   * change the state of the tokenizer or reach a valid token end point.
   */
  while( status == WEBVTT_UNFINISHED ) {
    switch( token_state ) {
      case DATA :
        status = webvtt_data_state( position, &token_state, &result );
        break;
      case ESCAPE:
        status = webvtt_escape_state( position, &token_state, &result );
        break;
      case TAG:
        status = webvtt_tag_state( position, &token_state, &result );
        break;
      case START_TAG:
        status = webvtt_start_tag_state( position, &token_state, &result );
        break;
      case START_TAG_CLASS:
        status = webvtt_class_state( position, &token_state, css_classes );
        break;
      case START_TAG_ANNOTATION:
        status = webvtt_annotation_state( position, &token_state, &annotation );
        break;
      case END_TAG:
        status = webvtt_end_tag_state( position, &token_state, &result );
        break;
      case TIME_STAMP_TAG:
        status = webvtt_timestamp_state( position, &token_state, &result );
        break;
    }

    if( token_state == START_TAG_ANNOTATION ) {
      webvtt_skipwhite( position );
    }
  }

  if( **position == UTF8_GREATER_THAN )
  { (*position)++; }
  
  if( status == WEBVTT_SUCCESS ) {
    /**
     * The state that the tokenizer left off on will tell us what kind of token
     * needs to be made.
     */
    if( token_state == DATA || token_state == ESCAPE ) {
      status = webvtt_create_text_token( token, &result );
    } else if(token_state == TAG || token_state == START_TAG || token_state == START_TAG_CLASS ||
              token_state == START_TAG_ANNOTATION) {
      /**
      * If the tag does not accept an annotation then release the current 
      * annotation and intialize annotation to a safe empty state
      */
      if( !tag_accepts_annotation( &result ) ) {
        webvtt_release_string( &annotation );
        webvtt_init_string( &annotation );
      }
      status = webvtt_create_start_token( token, &result, css_classes, &annotation );
    } else if( token_state == END_TAG ) {
      status = webvtt_create_end_token( token, &result );
    } else if( token_state == TIME_STAMP_TAG ) {
      parse_timestamp( webvtt_string_text( &result ), &time_stamp );
      status = webvtt_create_timestamp_token( token, time_stamp );
    } else {
      status = WEBVTT_INVALID_TOKEN_STATE;
    }
  }
  
  webvtt_release_stringlist( &css_classes );
  webvtt_release_string( &result );
  webvtt_release_string( &annotation );
  
  return status;
}

/**
 * Currently line and len are not being kept track of.
 * Don't think pnode_length is needed as nodes track there list count
 * internally.
 */
WEBVTT_INTERN webvtt_status
webvtt_parse_cuetext( webvtt_parser self, webvtt_cue *cue, webvtt_string *payload, int finished )
{

  const webvtt_byte *cue_text;
  webvtt_status status;
  webvtt_byte *position;
  webvtt_node *node_head;
  webvtt_node *current_node;
  webvtt_node *temp_node;
  webvtt_cuetext_token *token;
  webvtt_node_kind kind;

  if( !cue ) {
    return WEBVTT_INVALID_PARAM;
  }

  cue_text = webvtt_string_text( payload );

  if( !cue_text ) {
    return WEBVTT_INVALID_PARAM;
  }

  if ( WEBVTT_FAILED(status = webvtt_create_head_node( &cue->node_head ) ) ) {
    return status;
  }

  position = (webvtt_byte *)cue_text;
  node_head = cue->node_head;
  current_node = node_head;
  temp_node = NULL;
  token = NULL;

  /**
   * Routine taken from the W3C specification
   * http://dev.w3.org/html5/webvtt/#webvtt-cue-text-parsing-rules
   */
  while( *position != UTF8_NULL_BYTE ) {
    
    webvtt_delete_token( &token );

    /* Step 7. */
    switch( webvtt_cuetext_tokenizer( &position, &token ) ) {
      case( WEBVTT_UNFINISHED ):
        /* Error here. */
        break;
        /* Step 8. */
      case( WEBVTT_SUCCESS ):

        /**
         * If we've found an end token which has a valid end token tag name and
         * a tag name that is equal to the current node then set current to the
         * parent of current.
         */
        if( token->token_type == END_TOKEN ) {
          /**
           * We have encountered an end token but we are at the top of the list
           * and thus have not encountered any start tokens yet, throw away the
           * token.
           */
          if( current_node->kind == WEBVTT_HEAD_NODE ) {
            continue;
          }

          /**
           * We have encountered an end token but it is not in a format that is
           * supported, throw away the token.
           */
          if( webvtt_node_kind_from_tag_name( &token->tag_name, &kind ) == WEBVTT_INVALID_TAG_NAME ) {
            continue;
          }

          /**
           * We have encountered an end token and it matches the start token of
           * the node that we are currently on. Move back up the list of nodes
           * and continue parsing.
           */
          if( current_node->kind == kind ) {
            current_node = current_node->parent;
          }
        } else {

          /**
           * Attempt to create a valid node from the token.
           * If successful then attach the node to the current nodes list and
           * also set current to the newly created node if it is an internal
           * node type.
           */
          if( webvtt_create_node_from_token( token, &temp_node, current_node ) != WEBVTT_SUCCESS ) { 
            /* Do something here? */ 
          }
          else {
            webvtt_attach_node( current_node, temp_node );
            
            if( WEBVTT_IS_VALID_INTERNAL_NODE( temp_node->kind ) ) { 
              current_node = temp_node; 
            }
            
            /* Release the node as attach internal node increases the count. */
            webvtt_release_node( &temp_node );
          }
        }
        break;
    }
    webvtt_skipwhite( &position );
  }
  
  webvtt_delete_token( &token );
  
  return WEBVTT_SUCCESS;
}
