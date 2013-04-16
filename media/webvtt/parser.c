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
#include "cuetext_internal.h"
#include "cue_internal.h"
#include <string.h>

#define _ERROR(X) do { if( skip_error == 0 ) { ERROR(X); } } while(0)

static const webvtt_byte separator[] = {
  '-', '-', '>'
};

#define MSECS_PER_HOUR (3600000)
#define MSECS_PER_MINUTE (60000)
#define MSECS_PER_SECOND (1000)
#define BUFFER (self->buffer + self->position)
#define MALFORMED_TIME ((webvtt_timestamp_t)-1.0)

static webvtt_status find_bytes( const webvtt_byte *buffer, webvtt_uint len, const webvtt_byte *sbytes, webvtt_uint slen );
static webvtt_int64 parse_int( const webvtt_byte **pb, int *pdigits );
static void skip_spacetab( const webvtt_byte *text, webvtt_uint *pos,
  webvtt_uint len, webvtt_uint *column );
static void skip_until_white( const webvtt_byte *text, webvtt_uint *pos,
  webvtt_uint len, webvtt_uint *column );

WEBVTT_EXPORT webvtt_status
webvtt_create_parser( webvtt_cue_fn on_read,
                      webvtt_error_fn on_error, void *
                      userdata,
                      webvtt_parser *ppout )
{
  webvtt_parser p;
  if( !on_read || !on_error || !ppout ) {
    return WEBVTT_INVALID_PARAM;
  }

  if( !( p = ( webvtt_parser )webvtt_alloc0( sizeof * p ) ) ) {
    return WEBVTT_OUT_OF_MEMORY;
  }

  memset( p->astack, 0, sizeof( p->astack ) );
  p->stack = p->astack;
  p->top = p->stack;
  p->top->state = T_INITIAL;
  p->stack_alloc = sizeof( p->astack ) / sizeof( p->astack[0] );

  p->read = on_read;
  p->error = on_error;
  p->column = p->line = 1;
  p->userdata = userdata;
  p->finished = 0;
  *ppout = p;

  return WEBVTT_SUCCESS;
}

/**
 * Helper to validate a cue and, if valid, notify the application that a cue has
 * been read.
 * If it fails to validate, silently delete the cue.
 *
 * ( This might not be the best way to go about this, and additionally,
 * webvtt_validate_cue has no means to report errors with the cue, and we do
 * nothing with its return value )
 */
static void
finish_cue( webvtt_parser self, webvtt_cue **pcue )
{
  if( pcue ) {
    webvtt_cue *cue = *pcue;
    if( cue ) {
      if( webvtt_validate_cue( cue ) ) {
        self->read( self->userdata, cue );
      } else {
        webvtt_release_cue( &cue );
      }
      *pcue = 0;
    }
  }
}

/**
 * This routine tries to clean up the stack
 * for us, to prevent leaks.
 *
 * It should also help find errors in stack management.
 */
WEBVTT_INTERN void
cleanup_stack( webvtt_parser self )
{
  webvtt_state *st = self->top;
  while( st >= self->stack ) {
    switch( st->type ) {
      case V_CUE:
        webvtt_release_cue( &st->v.cue );
        break;
      case V_TEXT:
        webvtt_release_string( &st->v.text );
        break;
        /**
         * TODO: Clean up cuetext nodes as well.
         * Eventually the cuetext parser will probably be making use
         * of this stack, and will need to manage it well also.
         */
      default:
        break;
    }
    st->type = V_NONE;
    st->line = st->column = st->token = 0;
    st->v.cue = NULL;
    if( st > self->stack ) {
      --self->top;
    }
    --st;
  }
  if( self->stack != self->astack ) {
    /**
     * If the stack is dynamically allocated (probably not),
     * then point it to the statically allocated one (and zeromem it),
     * then finally delete the old dynamically allocated stack
     */
    webvtt_state *pst = self->stack;
    memset( self->astack, 0, sizeof( self->astack ) );
    self->stack = self->astack;
    self->stack_alloc = sizeof( self->astack ) / sizeof( *( self->astack ) );
    webvtt_free( pst );
  }
}

/**
 *
 */
WEBVTT_EXPORT webvtt_status
webvtt_finish_parsing( webvtt_parser self )
{
  webvtt_status status = WEBVTT_SUCCESS;
  const webvtt_byte buffer[] = "\0";
  const webvtt_uint len = 0;
  webvtt_uint pos = 0;

  if( !self->finished ) {
    self->finished = 1;

retry:
    switch( self->mode ) {
      /**
       * We've left off parsing cue settings and are not in the empty state,
       * return WEBVTT_CUE_INCOMPLETE.
       */
      case M_WEBVTT:
        if( self->top->state == T_CUEREAD ) {
          SAFE_ASSERT( self->top != self->stack );
          --self->top;
          self->popped = 1;
        }

        if( self->top->state == T_CUE ) {
          webvtt_string text;
          webvtt_cue *cue;
          if( self->top->type == V_NONE ) {
            webvtt_create_cue( &self->top->v.cue );
            self->top->type = V_CUE;
          }
          cue = self->top->v.cue;
          SAFE_ASSERT( self->popped && (self->top+1)->state == T_CUEREAD );
          SAFE_ASSERT( cue != 0 );
          text.d = (self->top+1)->v.text.d;
          (self->top+1)->v.text.d = 0;
          (self->top+1)->type = V_NONE;
          (self->top+1)->state = 0;
          self->column = 1;
          status = webvtt_proc_cueline( self, cue, &text );
          if( cue_is_incomplete( cue ) ) {
            ERROR( WEBVTT_CUE_INCOMPLETE );
          }
          ++self->line;
          self->column = 1;
          if( self->mode == M_CUETEXT ) {
            goto retry;
          }
        }
        break;
      /**
       * We've left off on trying to read in a cue text.
       * Parse the partial cue text read and pass the cue back to the
       * application if possible.
       */
      case M_CUETEXT:
        status = webvtt_proc_cuetext( self, buffer, &pos, len, self->finished );
        break;
      case M_SKIP_CUE:
        /* Nothing to do here. */
        break;
      case M_READ_LINE:
        /* Nothing to do here. */
        break;
    }
    cleanup_stack( self );
  }

  return status;
}

WEBVTT_EXPORT void
webvtt_delete_parser( webvtt_parser self )
{
  if( self ) {
    cleanup_stack( self );

    webvtt_release_string( &self->line_buffer );
    webvtt_free( self );
  }
}

#define BEGIN_STATE(State) case State: {
#define END_STATE } break;
#define IF_TOKEN(Token,Actions) case Token: { Actions } break;
#define BEGIN_DFA switch(top->state) {
#define END_DFA }
#define BEGIN_TOKEN switch(token) {
#define END_TOKEN }
#define IF_TRANSITION(Token,State) if( token == Token ) { self->state = State;
#define ELIF_TRANSITION(Token,State) } else IF_TRANSITION(Token,State)
#define ENDIF }
#define ELSE } else {

static int
find_newline( const webvtt_byte *buffer, webvtt_uint *pos, webvtt_uint len )
{
  while( *pos < len ) {
    if( buffer[ *pos ] == '\r' || buffer[ *pos ] == '\n' ) {
      return 1;
    } else {
      ( *pos )++;
    }
  }
  return -1;
}

static void
skip_spacetab( const webvtt_byte *text, webvtt_uint *pos, webvtt_uint len,
  webvtt_uint *column )
{
  webvtt_uint c = 0;
  if( !column ) {
    column = &c;
  }
  while( *pos < len ) {
    webvtt_byte ch = text[ *pos ];
    if( ch == ' ' || ch == '\t' ) {
      ++( *pos );
      ++( *column );
    } else {
      break;
    }
  }
}

static void
skip_until_white( const webvtt_byte *text, webvtt_uint *pos, webvtt_uint len,
  webvtt_uint *column )
{
  webvtt_uint c = 0;
  if( !column ) {
    column = &c;
  }
  while( *pos < len ) {
    webvtt_byte ch = text[ *pos ];
    if( ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' ) {
      break;
    } else {
      int length = webvtt_utf8_length( text + *pos );
      *pos += length;
      ++( *column );
    }
  }
}

/**
 * basic strnstr-ish routine
 */
static webvtt_status
find_bytes( const webvtt_byte *buffer, webvtt_uint len,
    const webvtt_byte *sbytes, webvtt_uint slen )
{
  webvtt_uint slen2;
  // check params for integrity
  if( !buffer || len < 1 || !sbytes || slen < 1 ) {
    return WEBVTT_INVALID_PARAM;
  }

  slen2 = slen - 1;
  while( len-- >= slen && *buffer ){
    if( *buffer == *sbytes && memcmp( buffer + 1, sbytes + 1, slen2 ) == 0 ) {
      return WEBVTT_SUCCESS;
    }
    buffer++;
  }

  return WEBVTT_NO_MATCH_FOUND;
}

/**
 * Helpers to figure out what state we're on
 */
#define SP (self->top)
#define AT_BOTTOM (self->top == self->stack)
#define ON_HEAP (self->stack_alloc == sizeof(p->astack) / sizeof(p->astack[0]))
#define STACK_SIZE ((webvtt_uint)(self->top - self->stack))
#define FRAME(i) (self->top - (i))
#define FRAMEUP(i) (self->top + (i))
#define RECHECK goto _recheck;
#define BACK (SP->back)
/**
 * More state stack helpers
 */
WEBVTT_INTERN webvtt_status
do_push( webvtt_parser self, webvtt_uint token, webvtt_uint back, webvtt_uint state, void *data, webvtt_state_value_type type, webvtt_uint line, webvtt_uint column )
{
  if( STACK_SIZE + 1 >= self->stack_alloc ) {
    webvtt_state *stack = ( webvtt_state * )webvtt_alloc0( sizeof( webvtt_state ) * ( self->stack_alloc << 1 ) ), *tmp;
    if( !stack ) {
      ERROR( WEBVTT_ALLOCATION_FAILED );
      return WEBVTT_OUT_OF_MEMORY;
    }
    memcpy( stack, self->stack, sizeof( webvtt_state ) * self->stack_alloc );
    tmp = self->stack;
    self->stack = stack;
    self->top = stack + ( self->top - tmp );
    if( tmp != self->astack ) {
      webvtt_free( tmp );
    }
  }
  ++self->top;
  self->top->state = state;
  self->top->flags = 0;
  self->top->type = type;
  self->top->token = ( webvtt_token )token;
  self->top->line = line;
  self->top->back = back;
  self->top->column = column;
  self->top->v.cue = ( webvtt_cue * )data;
  return WEBVTT_SUCCESS;
}
static int
do_pop( webvtt_parser self )
{
  int count = self->top->back;
  self->top -= count;
  self->top->back = 0;
  self->popped = 1;
  return count;
}

#define PUSH0(S,V,T) \
do { \
    self->popped = 0; \
    if( do_push(self,token,BACK+1,(S),(void*)(V),T,last_line, last_column) \
      == WEBVTT_OUT_OF_MEMORY ) \
      return WEBVTT_OUT_OF_MEMORY; \
  } while(0)

#define PUSH(S,B,V,T) \
do { \
  self->popped = 0; \
  if( do_push(self,token,(B),(S),(void*)(V),T,last_line, last_column) \
    == WEBVTT_OUT_OF_MEMORY ) \
    return WEBVTT_OUT_OF_MEMORY; \
  } while(0)

#define POP() \
do \
{ \
  --(self->top); \
  self->popped = 1; \
} while(0)
#define POPBACK() do_pop(self)

static webvtt_status
webvtt_parse_cuesetting( webvtt_parser self, const webvtt_byte *text,
  webvtt_uint *pos, webvtt_uint len, webvtt_error bv, webvtt_token
  keyword, webvtt_token values[], webvtt_uint *value_column ) {
  enum webvtt_param_mode
  {
    P_KEYWORD,
    P_COLON,
    P_VALUE
  };
  int i;
  webvtt_bool precws = 0;
  webvtt_bool prevws = 0;
  static const webvtt_token value_tokens[] = {
    INTEGER, RL, LR, START, MIDDLE, END, LEFT, RIGHT, PERCENTAGE, 0
  };
  static const webvtt_token keyword_tokens[] = {
    ALIGN, SIZE, LINE, POSITION, VERTICAL, 0
  };
  enum webvtt_param_mode mode = P_KEYWORD;
  webvtt_uint keyword_column = 0;
  while( *pos < len ) {
    webvtt_uint last_line = self->line;
    webvtt_uint last_column = self->column;
    webvtt_uint last_pos = *pos;
    webvtt_token tk = webvtt_lex( self, text, pos, len, 1 );
    webvtt_uint tp = self->token_pos;
    self->token_pos = 0;

    switch( mode ) {
      case P_KEYWORD:
        switch( tk ) {
          case ALIGN:
          case SIZE:
          case POSITION:
          case VERTICAL:
          case LINE:
            if( tk != keyword ) {
              *pos -= tp;
              self->column -= tp;
              return WEBVTT_NEXT_CUESETTING;
            }
            if( *pos < len ) {
              webvtt_uint column = last_column;
              webvtt_byte ch = text[ *pos ];
              if( ch != ':' ) {
                webvtt_error e = WEBVTT_INVALID_CUESETTING;
                if( ch == ' ' || ch == '\t' ) {
                  column = self->column;
                  e = WEBVTT_UNEXPECTED_WHITESPACE;
                  skip_spacetab( text, pos, len, &self->column );
                  if( text[ *pos ] == ':' ) {
                    skip_until_white( text, pos, len, &self->column );
                  }
                } else {
                  skip_until_white( text, pos, len, &self->column );
                }
                ERROR_AT_COLUMN( e, column );
              } else {
                mode = P_COLON;
                keyword_column = last_column;
              }
            } else {
              ERROR_AT_COLUMN( WEBVTT_INVALID_CUESETTING, last_column );
            }
            break;
          case WHITESPACE:
            break;
          case NEWLINE:
            return WEBVTT_SUCCESS;
            break;
          default:
            ERROR_AT( WEBVTT_INVALID_CUESETTING, last_line,
              last_column );
            *pos = *pos + tp + 1;
            while( *pos < len && text[ *pos ] != ' ' && text[ *pos ] != '\t' ) {
              if( text[ *pos ] == '\n' || text[ *pos ] == '\r' ) {
                return WEBVTT_SUCCESS;
              }
              ++( *pos );
              ++self->column;
            }
            break;
        }
        break;
      case P_COLON:
        if( tk == WHITESPACE && !precws ) {
          ERROR_AT( WEBVTT_UNEXPECTED_WHITESPACE, last_line,
            last_column
          );
          precws = 1;
        } else if( tk == COLON ) {
          mode = P_VALUE;
        } else if( token_in_list( tk, value_tokens ) ) {
          ERROR_AT( WEBVTT_MISSING_CUESETTING_DELIMITER, last_line,
            last_column );
          mode = P_VALUE;
          goto get_value;
        } else if( token_in_list( tk, keyword_tokens ) ) {
          ERROR_AT( WEBVTT_INVALID_CUESETTING, last_line,
            keyword_column );
        } else {
          ERROR_AT( WEBVTT_INVALID_CUESETTING_DELIMITER, last_line,
            last_column );
          *pos = last_pos + tp + 1;
        }
        break;
      case P_VALUE:
get_value:
        if( tk == WHITESPACE && !prevws ) {
          ERROR_AT( WEBVTT_UNEXPECTED_WHITESPACE, last_line,
            last_column );
        } else if( ( i = find_token( tk, values ) ) >= 0 ) {
          webvtt_token t = values[ i ] & TF_TOKEN_MASK;
          int flags = values[ i ] & TF_FLAGS_MASK;
          *value_column = last_column;
          if( *pos < len ) {
            webvtt_byte ch = text[ *pos ];
            if( ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' ) {
              goto bad_value;
            }
          }
          switch( t ) {
            case INTEGER:
            case PERCENTAGE:
              if( ( flags & TF_SIGN_MASK ) != TF_SIGN_MASK ) {
                const webvtt_byte p = self->token[ 0 ];
                if( ( ( flags & TF_NEGATIVE ) && p != '-' )
                  || ( ( flags & TF_POSITIVE ) && p == '-' ) ) {
                  goto bad_value;
                }
              }
              break;

            default: /* Currently, other types don't need these checks */
              break;
          }
          return i + 1;
        } else {
bad_value:
          ERROR_AT( bv, last_line, last_column );
bad_value_eol:
          while( *pos < len && text[ *pos ] != ' ' && text[ *pos ] != '\t' ) {
            if( text[ *pos ] == '\n' || text[ *pos ] == '\r' ) {
              return WEBVTT_SUCCESS;
            }
            ++( *pos );
            ++self->column;
          }
          if( *pos >= len ) {
            return WEBVTT_SUCCESS;
          }
        }
        break;
    }
  }
  if( mode == P_VALUE && *pos >= len ) {
    ERROR( bv );
    goto bad_value_eol;
  }
  return WEBVTT_NEXT_CUESETTING;
}

WEBVTT_INTERN webvtt_status
webvtt_parse_align( webvtt_parser self, webvtt_cue *cue,
                    const webvtt_byte *text, webvtt_uint *pos, webvtt_uint len )
{
  webvtt_uint last_line = self->line;
  webvtt_uint last_column = self->column;
  webvtt_status v;
  webvtt_uint vc;
  webvtt_token tokens[] = { START, MIDDLE, END, LEFT, RIGHT, 0 };
  webvtt_align_type values[] = {
    WEBVTT_ALIGN_START, WEBVTT_ALIGN_MIDDLE, WEBVTT_ALIGN_END,
    WEBVTT_ALIGN_LEFT, WEBVTT_ALIGN_RIGHT
  };
  if( ( v = webvtt_parse_cuesetting( self, text, pos, len,
    WEBVTT_ALIGN_BAD_VALUE, ALIGN, tokens, &vc ) ) > 0 ) {
    if( cue->flags & CUE_HAVE_ALIGN ) {
      ERROR_AT( WEBVTT_ALIGN_ALREADY_SET, last_line, last_column );
    }
    cue->flags |= CUE_HAVE_ALIGN;
    cue->settings.align = values[ v - 1 ];
  }
  return v >= 0 ? WEBVTT_SUCCESS : v;
}

WEBVTT_INTERN webvtt_status
webvtt_parse_line( webvtt_parser self, webvtt_cue *cue, const webvtt_byte *text,
                   webvtt_uint *pos, webvtt_uint len )
{
  webvtt_uint last_line = self->line;
  webvtt_uint last_column = self->column;
  webvtt_status v;
  webvtt_uint vc;
  webvtt_bool first_flag = 0;
  webvtt_token values[] = { INTEGER, PERCENTAGE|TF_POSITIVE, 0 };
  if( ( v = webvtt_parse_cuesetting( self, text, pos, len,
    WEBVTT_LINE_BAD_VALUE, LINE, values, &vc ) ) > 0 ) {
    int digits;
    webvtt_int64 value;
    const webvtt_byte *t = self->token;
    if( cue->flags & CUE_HAVE_LINE ) {
      ERROR_AT( WEBVTT_LINE_ALREADY_SET, last_line, last_column );
    } else {
      first_flag = 1;
    }
    cue->flags |= CUE_HAVE_LINE;
    value = parse_int( &t, &digits );
    switch( values[ v - 1 ] & TF_TOKEN_MASK ) {
      case INTEGER: {
        cue->snap_to_lines = 1;
        cue->settings.line = ( int )value;
      }
      break;

      case PERCENTAGE: {
        if( value < 0 || value > 100 ) {
          if( first_flag ) {
            cue->flags &= ~CUE_HAVE_LINE;
          }
          ERROR_AT_COLUMN( WEBVTT_LINE_BAD_VALUE, vc );
          return WEBVTT_SUCCESS;
        }
        cue->snap_to_lines = 0;
        cue->settings.line = ( int )value;
      } break;
    }
  }
  return v >= 0 ? WEBVTT_SUCCESS : v;
}

WEBVTT_INTERN webvtt_status
webvtt_parse_position( webvtt_parser self, webvtt_cue *cue,
                       const webvtt_byte *text, webvtt_uint *pos,
                       webvtt_uint len )
{
  webvtt_uint last_line = self->line;
  webvtt_uint last_column = self->column;
  webvtt_status v;
  webvtt_uint vc;
  webvtt_bool first_flag = 0;
  webvtt_token values[] = { PERCENTAGE|TF_POSITIVE, 0 };
  if( ( v = webvtt_parse_cuesetting( self, text, pos, len,
    WEBVTT_POSITION_BAD_VALUE, POSITION, values, &vc ) ) > 0 ) {
    int digits;
    webvtt_int64 value;
    const webvtt_byte *t = self->token;
    if( cue->flags & CUE_HAVE_LINE ) {
      ERROR_AT( WEBVTT_POSITION_ALREADY_SET, last_line, last_column );
    } else {
      first_flag = 1;
    }
    cue->flags |= CUE_HAVE_POSITION;
    value = parse_int( &t, &digits );
    if( value < 0 || value > 100 ) {
      if( first_flag ) {
        cue->flags &= ~CUE_HAVE_POSITION;
      }
      ERROR_AT_COLUMN( WEBVTT_POSITION_BAD_VALUE, vc );
      return WEBVTT_SUCCESS;
    }
    cue->settings.position = ( int )value;
  }
  return v >= 0 ? WEBVTT_SUCCESS : v;
}

WEBVTT_INTERN webvtt_status
webvtt_parse_size( webvtt_parser self, webvtt_cue *cue, const webvtt_byte *text,
                   webvtt_uint *pos, webvtt_uint len )
{
  webvtt_uint last_line = self->line;
  webvtt_uint last_column = self->column;
  webvtt_status v;
  webvtt_uint vc;
  webvtt_token tokens[] = { PERCENTAGE|TF_POSITIVE, 0 };
  if( ( v = webvtt_parse_cuesetting( self, text, pos, len,
    WEBVTT_SIZE_BAD_VALUE, SIZE, tokens, &vc ) ) > 0 ) {
    if( cue->flags & CUE_HAVE_SIZE ) {
      ERROR_AT( WEBVTT_SIZE_ALREADY_SET, last_line, last_column );
    }
    cue->flags |= CUE_HAVE_SIZE;
    if( tokens[ v - 1 ] ) {
      int digits;
      const webvtt_byte *t = self->token;
      webvtt_int64 value;
      self->token_pos = 0;
      value = parse_int( &t, &digits );
      if( value < 0 || value > 100 ) {
        ERROR_AT_COLUMN( WEBVTT_SIZE_BAD_VALUE, vc );
      } else {
        cue->settings.size = ( int )value;
      }
    }
  }
  return v >= 0 ? WEBVTT_SUCCESS : v;
}

WEBVTT_INTERN webvtt_status
webvtt_parse_vertical( webvtt_parser self, webvtt_cue *cue,
                       const webvtt_byte *text, webvtt_uint *pos,
                       webvtt_uint len )
{
  webvtt_uint last_line = self->line;
  webvtt_uint last_column = self->column;
  webvtt_status v;
  webvtt_uint vc;
  webvtt_token tokens[] = { RL, LR, 0 };
  webvtt_vertical_type values[] = { WEBVTT_VERTICAL_RL, WEBVTT_VERTICAL_LR };
  if( ( v = webvtt_parse_cuesetting( self, text, pos, len,
        WEBVTT_VERTICAL_BAD_VALUE, VERTICAL, tokens, &vc ) ) > 0 ) {
    if( cue->flags & CUE_HAVE_VERTICAL ) {
      ERROR_AT( WEBVTT_VERTICAL_ALREADY_SET, last_line, last_column );
    }
    cue->flags |= CUE_HAVE_VERTICAL;
    cue->settings.vertical = values[ v - 1 ];
  }
  return v >= 0 ? WEBVTT_SUCCESS : v;
}
/**
 * Read a timestamp into 'result' field, following the rules of the cue-times
 * section of the draft:
 * http://dev.w3.org/html5/webvtt/#collect-webvtt-cue-timings-and-settings
 *
 * - Ignore whitespace
 * - Return immediately after having parsed a timestamp
 * - Return error if timestamp invalid.
 */
WEBVTT_INTERN webvtt_status
webvtt_get_timestamp( webvtt_parser self, webvtt_timestamp *result,
                      const webvtt_byte *text, webvtt_uint *pos,
                      webvtt_uint len, const char *accepted )
{
  webvtt_uint last_column = self->column;
  webvtt_uint last_line = self->line;
  webvtt_token token;
  while( *pos < len ) {
    last_column = self->column;
    last_line = self->line;
    token = webvtt_lex( self, text, pos, len, 1 );
    self->token_pos = 0;

    if( token == TIMESTAMP ) {
      if( *pos < len && text[ *pos ] != '\r' && text[ *pos ] != '\n' &&
          text[ *pos ] != ' ' &&  text[ *pos ] != '\t' ) {
        if( accepted == 0 || !strchr( accepted, text[ *pos ] ) ) {
          ERROR_AT( WEBVTT_EXPECTED_TIMESTAMP, last_line, last_column );
          return WEBVTT_PARSE_ERROR;
        }
      }

      if( !parse_timestamp( self->token, result ) ) {
        /* Read a bad timestamp, throw away and abort cue */
        ERROR_AT( WEBVTT_MALFORMED_TIMESTAMP, last_line, last_column );
        if( BAD_TIMESTAMP( *result ) ) {
          return WEBVTT_PARSE_ERROR;
        }
      }
      return WEBVTT_SUCCESS;
    }
    else if( token == WHITESPACE ) {
      /* Ignore whitespace */
    } else {
      /* Not a timestamp, this is an error. */
      ERROR_AT( WEBVTT_EXPECTED_TIMESTAMP, last_line, last_column );
      return WEBVTT_PARSE_ERROR;
    }
  }

  /* If we finish entire line without reading a timestamp,
     this is not a proper cue header and should be discarded. */
  ERROR_AT( WEBVTT_EXPECTED_TIMESTAMP, last_line, last_column );
  return WEBVTT_PARSE_ERROR;
}

WEBVTT_INTERN webvtt_status
webvtt_proc_cueline( webvtt_parser self, webvtt_cue *cue,
                     webvtt_string *line )
{
  const webvtt_byte *text;
  webvtt_uint length;
  DIE_IF( line == NULL );
  length = webvtt_string_length( line );
  text = webvtt_string_text( line );
  /* backup the column */
  self->column = 1;
  if( find_bytes( text, length, separator, sizeof( separator ) )
      == WEBVTT_SUCCESS) {
    /* It's not a cue id, we found '-->'. It can't be a second
       cueparams line, because if we had it, we would be in
       a different state. */
    int v;
    self->cuetext_line = self->line + 1;
    if( ( v = parse_cueparams( self, text, length, cue ) ) < 0 ) {
        if( v == WEBVTT_PARSE_ERROR ) {
          return WEBVTT_PARSE_ERROR;
        }
        self->mode = M_SKIP_CUE;
      } else {
        cue->flags |= CUE_HAVE_CUEPARAMS;
        self->mode = M_CUETEXT;
      }
  } else {
    /* It is a cue-id */
    if( cue && cue->flags & CUE_HAVE_ID ) {
      /**
       * This isn't actually a cue-id, because we already
       * have one. It seems to be cuetext, which is occurring
       * before cue-params
       */
      webvtt_release_string( line );
      ERROR( WEBVTT_CUE_INCOMPLETE );
      self->mode = M_SKIP_CUE;
      return WEBVTT_SUCCESS;
    } else {
      webvtt_uint last_column = self->column;
      webvtt_uint last_line = self->line;
      webvtt_token token = UNFINISHED;
      self->column += length;
      self->cuetext_line = self->line;
      if( WEBVTT_FAILED( webvtt_string_append( &cue->id, text,
                                               length ) ) ) {
        webvtt_release_string( line );
        ERROR( WEBVTT_ALLOCATION_FAILED );
        return WEBVTT_OUT_OF_MEMORY;
      }
      cue->flags |= CUE_HAVE_ID;

      /* Read cue-params line */
      PUSH0( T_CUEREAD, 0, V_NONE );
      webvtt_init_string( &SP->v.text );
      SP->type = V_TEXT;
    }
  }

  webvtt_release_string( line );
  return WEBVTT_SUCCESS;
}

WEBVTT_INTERN int
parse_cueparams( webvtt_parser self, const webvtt_byte *buffer,
                 webvtt_uint len, webvtt_cue *cue )
{
  webvtt_uint pos = 0;

  enum cp_state {
    CP_STARTTIME = 0, /* Start timestamp */
    CP_SEPARATOR, /* --> */
    CP_ENDTIME, /* End timestamp */

    CP_SETTING, /* Cuesetting */
  };

  enum cp_state state = CP_STARTTIME;

#define SETST(X) do { baddelim = 0; state = (X); } while( 0 )

  self->token_pos = 0;
  while( pos < len ) {
    webvtt_uint last_column;
    webvtt_uint last_line;
    webvtt_token token;
    webvtt_uint token_len;

    switch( state ) {
      /* start timestamp */
      case CP_STARTTIME:
        if( WEBVTT_FAILED( webvtt_get_timestamp( self, &cue->from, buffer, &pos,
                                                 len, "-" ) ) ) {
          /* abort cue */
          return -1;
        }

        state = CP_SEPARATOR;
        break;

      /* '-->' separator */
      case CP_SEPARATOR:
        last_column = self->column;
        last_line = self->line;
        token = webvtt_lex( self, buffer, &pos, len, 1 );
        self->token_pos = 0;
        if( token == SEPARATOR ) {
          /* Expect end timestamp */
          state = CP_ENDTIME;
        } else if( token == WHITESPACE ) {
          /* ignore whitespace */
        } else {
          /* Unexpected token. Abort cue. */
          ERROR_AT( WEBVTT_EXPECTED_CUETIME_SEPARATOR, last_line, last_column );
          return -1;
        }
        break;

      /* end timestamp */
      case CP_ENDTIME:
	if( WEBVTT_FAILED( webvtt_get_timestamp( self, &cue->until, buffer,
                                                 &pos, len, 0 ) ) ) {
          /* abort cue */
          return -1;
        }

        /* Expect cuesetting */
        state = CP_SETTING;
        break;


      default:
        /**
         * Most of these branches need to read a token for the time
         * being, so they're grouped together here.
         *
         * TODO: Remove need to call lexer at all here.
         */
        last_column = self->column;
        last_line = self->line;
        token = webvtt_lex( self, buffer, &pos, len, 1 );
        token_len = self->token_pos;
        self->token_pos = 0;

        switch( token ) {
          case NEWLINE:
            return 0;
          case WHITESPACE:
            continue;

          case VERTICAL:
          {
            webvtt_status status;
            pos -= token_len; /* Required for parse_vertical() */
            self->column = last_column; /* Reset for parse_vertical() */
            status = webvtt_parse_vertical( self, cue, buffer, &pos, len );
            if( status == WEBVTT_PARSE_ERROR ) {
              return WEBVTT_PARSE_ERROR;
            }
          }
          break;


          case POSITION:
          {
            webvtt_status status;
            pos -= token_len; /* Required for parse_position() */
            self->column = last_column; /* Reset for parse_position() */
            status = webvtt_parse_position( self, cue, buffer, &pos, len );
            if( status == WEBVTT_PARSE_ERROR ) {
              return WEBVTT_PARSE_ERROR;
            }
          }
          break;

          case ALIGN:
          {
            webvtt_status status;
            pos -= token_len; /* Required for parse_align() */
            self->column = last_column; /* Reset for parse_align() */
            status = webvtt_parse_align( self, cue, buffer, &pos, len );
            if( status == WEBVTT_PARSE_ERROR ) {
              return WEBVTT_PARSE_ERROR;
            }
          }
          break;

          case SIZE:
          {
            webvtt_status status;
            pos -= token_len; /* Required for parse_size() */
            self->column = last_column; /* Reset for parse_size() */
            status = webvtt_parse_size( self, cue, buffer, &pos, len );
            if( status == WEBVTT_PARSE_ERROR ) {
              return WEBVTT_PARSE_ERROR;
            }
          }
          break;

          case LINE:
          {
            webvtt_status status;
            pos -= token_len; /* Required for parse_line() */
            self->column = last_column; /* Reset for parse_line() */
            status = webvtt_parse_line( self, cue, buffer, &pos, len );
            if( status == WEBVTT_PARSE_ERROR ) {
              return WEBVTT_PARSE_ERROR;
            }
          }
          break;
          default:
            ERROR_AT_COLUMN( WEBVTT_INVALID_CUESETTING, last_column );
            while( pos < len && buffer[pos] != '\t' && buffer[pos] != ' ' ) {
              ++pos;
            }
        }
        break;
    }
    self->token_pos = 0;
  }
  /**
   * If we didn't finish in a good state...
   */
  if( state != CP_SETTING ) {
    /* if we never made it to the cuesettings, we didn't finish the cuetimes */
    if( state < CP_SETTING ) {
      ERROR( WEBVTT_UNFINISHED_CUETIMES );
      return -1;
    } else {
      /* if we did, we should report an error but continue parsing. */
      webvtt_error e = WEBVTT_INVALID_CUESETTING;
      ERROR( e );
    }
  }
#undef SETST
  return 0;
}

static webvtt_status
parse_webvtt( webvtt_parser self, const webvtt_byte *buffer, webvtt_uint *ppos,
              webvtt_uint len, int finish )
{
  webvtt_status status = WEBVTT_SUCCESS;
  webvtt_token token = 0;
  webvtt_uint pos = *ppos;
  int skip_error = 0;

  while( pos < len ) {
    webvtt_uint last_column, last_line;
    skip_error = 0;
    last_column = self->column;
    last_line = self->line;

    /**
     * If we're in certain states, we don't want to get a token and just
     * want to read text instead.
     */
    if( SP->state == T_CUEREAD ) {
      DIE_IF( SP->type != V_TEXT );
      if( SP->flags == 0 ) {
        int v;
        if( ( v = webvtt_string_getline( &SP->v.text, buffer, &pos, len, 0,
                                         finish ) ) ) {
          if( v < 0 ) {
            webvtt_release_string( &SP->v.text );
            SP->type = V_NONE;
            POP();
            ERROR( WEBVTT_ALLOCATION_FAILED );
            status = WEBVTT_OUT_OF_MEMORY;
            goto _finish;
          }
          SP->flags = 1;
        }
      }
      if( SP->flags ) {
        webvtt_token token = webvtt_lex_newline( self, buffer, &pos, len,
                                                 self->finished );
        if( token == NEWLINE ) {
          POP();
          continue;
        }
      }
    }

    /**
     * Get the next token from the stream
     *
     * If the token is 'UNFINISHED', but we are at the end of our input
     * data, change it to BADTOKEN because it will never be finished.
     *
     * Otherwise, if we are expecting further data at some point, and have
     * an unfinished token, return and let the next chunk deal with it.
     */
    if( SP->state != T_CUE || !( self->popped && FRAMEUP( 1 )->state == T_CUEREAD ) ) {
      /**
       * We don't tokenize in certain states
       */
      token = webvtt_lex( self, buffer, &pos, len, finish );
      if( token == UNFINISHED ) {
        if( finish ) {
          token = BADTOKEN;
        } else if( pos == len ) {
          goto _finish;
        }
      }
    }
_recheck:
    switch( SP->state ) {
      default:
        /* Should never happen */
        break;

      case T_INITIAL:
        /**
         * In the initial state:
         * We should have WEBVTT as the first token returned,
         * otherwise this isn't really a valid file.
         *
         * If we get 'WEBVTT', push us into the TAG state, where we
         * check for a tag comment (arbitrary text following a whitespace
         * after the WEBVTT token) until a newline
         *
         * If WEBVTT is not the first token, then report error and
         * abort parsing.
         */
        if( token == WEBVTT ) {
          PUSH0( T_TAG, 0, V_NONE );
          break;
        } else if( token != UNFINISHED ) {
          ERROR_AT( WEBVTT_MALFORMED_TAG, 1, 1 );
          status = WEBVTT_PARSE_ERROR;
          goto _finish;
        }
        break;

      case T_TAG:
        /**
         * If we have a WHITESPACE following the WEBVTT token,
         * switch to T_TAGCOMMENT state and skip the comment.
         * Otherwise, if it's a NEWLINE, we can just skip to
         * the T_BODY state.
         *
         * Otherwise, we didn't actually have a WEBVTT token,
         * and should feel ashamed.
         */
        if( token == WHITESPACE ) {
          /* switch to comment skipper */
          PUSH0( T_TAGCOMMENT, 0, V_NONE );
        } else if( token == NEWLINE ) {
          /* switch to NEWLINE counter */
          POPBACK();
          self->popped = 0;
          SP->state = T_BODY;
          PUSH0( T_EOL, 1, V_INTEGER );
          break;
        } else {
          /**
           * This wasn't preceded by an actual WEBVTT token, it's more
           * like WEBVTTasdasd, which is not valid. Report an error,
           * which should be considered fatal.
           */
          if( !skip_error ) {
            ERROR_AT_COLUMN( WEBVTT_MALFORMED_TAG, FRAME( 1 )->column );
            skip_error = 1;
            status = WEBVTT_PARSE_ERROR;
            goto _finish;
          }
        }
        break;

        /**
         * COMMENT -- Read until EOL, ignore everything else
         */
      case T_TAGCOMMENT:
        switch( token ) {
          case NEWLINE:
            /**
             * If we encounter a newline, switch to NEWLINE mode,
             * and set up so that when we POPBACK() we are in the
             * T_BODY state.
             */
            POPBACK();
            PUSH0( T_EOL, 1, V_INTEGER );
            break;
          default:
            find_newline( buffer, &pos, len );
            continue;
        }
        break;

      case T_CUEID:
        switch( token ) {
            /**
             * We're only really expecting a newline here --
             * The cue id should have been read already
             */
          case NEWLINE:
            SP->state = T_FROM;
            break;

          default:
            break;
        }

        /**
         * Count EOLs, POP when finished
         */
      case T_EOL:
        switch( token ) {
          case NEWLINE:
            SP->v.value++;
            break;
          default:
            POPBACK();
            RECHECK
        }
        break;

      case T_BODY:
        if( self->popped && FRAMEUP( 1 )->state == T_EOL ) {
          if( FRAMEUP( 1 )->v.value < 2 ) {
            ERROR_AT_COLUMN( WEBVTT_EXPECTED_EOL, 1 );
          }
          FRAMEUP( 1 )->state = 0;
          FRAMEUP( 1 )->v.cue = NULL;
        }
        if( token == NOTE ) {
          PUSH0( T_COMMENT, 0, V_NONE );
        } else if( token != NEWLINE ) {
          webvtt_cue *cue = 0;
          webvtt_string tk = { 0 };
          if( WEBVTT_FAILED( status = webvtt_create_cue( &cue ) ) ) {
            if( status == WEBVTT_OUT_OF_MEMORY ) {
              ERROR( WEBVTT_ALLOCATION_FAILED );
            }
            goto _finish;
          }
          if( WEBVTT_FAILED( status = webvtt_create_string_with_text( &tk,
            self->token, self->token_pos ) ) ) {
            if( status == WEBVTT_OUT_OF_MEMORY ) {
              ERROR( WEBVTT_ALLOCATION_FAILED );
            }
            webvtt_release_cue( &cue );
            goto _finish;
          }
          PUSH0( T_CUE, cue, V_CUE );
          PUSH0( T_CUEREAD, 0, V_TEXT );
          SP->v.text.d = tk.d;
        }
        break;


      case T_CUE:
      {
        webvtt_cue *cue;
        webvtt_state *st;
        webvtt_string text;
        SAFE_ASSERT( self->popped && FRAMEUP( 1 )->state == T_CUEREAD );
        /**
         * We're expecting either cue-id (contains '-->') or cue
         * params
         */
        cue = SP->v.cue;
        st = FRAMEUP( 1 );
        text.d = st->v.text.d;

        st->type = V_NONE;
        st->v.cue = NULL;

        /* FIXME: guard inconsistent state */
        if (!cue) {
          ERROR( WEBVTT_PARSE_ERROR );
          status = WEBVTT_PARSE_ERROR;
          goto _finish;
        }

        status = webvtt_proc_cueline( self, cue, &text );
        ++self->line;
        if( self->mode != M_WEBVTT ) {
          goto _finish;
        }
        self->popped = 0;
      }
      break;
    }

    /**
     * reset token pos
     */
    self->token_pos = 0;
  }


_finish:
  if( status == WEBVTT_OUT_OF_MEMORY ) {
    cleanup_stack( self );
  }
  *ppos = pos;
  return status;
}

WEBVTT_INTERN webvtt_status
webvtt_read_cuetext( webvtt_parser self, const webvtt_byte *b,
                     webvtt_uint *ppos, webvtt_uint len, webvtt_bool finish )
{
  webvtt_status status = WEBVTT_SUCCESS;
  webvtt_uint pos = *ppos;
  int finished = 0;
  int flags = 0;
  webvtt_cue *cue;

  /* Ensure that we have a cue to work with */
  SAFE_ASSERT( self->top->type = V_CUE );
  cue = self->top->v.cue;

  /**
   * Hack to support lines spanning multiple buffers.
   *
   * TODO: Do this some better way. This is not good!
   */
  if( self->line_buffer.d != 0 && self->line_buffer.d->text[
      self->line_buffer.d->length - 1 ] == '\n' ) {
    flags = 1;
  }

  do {
    if( !flags ) {
      int v;
      if( ( v = webvtt_string_getline( &self->line_buffer, b, &pos, len,
                                       &self->truncate, finish ) ) ) {
        if( v < 0 || WEBVTT_FAILED( webvtt_string_putc( &self->line_buffer,
                                                        '\n' ) ) ) {
          status = WEBVTT_OUT_OF_MEMORY;
          goto _finish;
        }
        flags = 1;
      }
    }
    if( flags ) {
      webvtt_token token = webvtt_lex_newline( self, b, &pos, len, finish );
      if( token == NEWLINE ) {
        self->token_pos = 0;
        self->line++;

        /* Remove the '\n' that we appended to determine that we're in state 1
         */
        self->line_buffer.d->text[ --self->line_buffer.d->length ] = 0;
        /**
         * We've encountered a line without any cuetext on it, i.e. there is no
         * newline character and len is 0 or there is and len is 1, therefore,
         * the cue text is finished.
         */
        if( self->line_buffer.d->length == 0 ) {
          webvtt_release_string( &self->line_buffer );
          finished = 1;
        } else if( find_bytes( webvtt_string_text( &self->line_buffer ),
                   webvtt_string_length( &self->line_buffer ), separator,
                   sizeof( separator ) ) == WEBVTT_SUCCESS ) {
          /**
           * Line contains cue-times separator, and thus we treat it as a
           * separate cue. Trick program into thinking that T_CUEREAD had read
           * this line.
           */
          do_push( self, 0, 0, T_CUEREAD, 0, V_NONE, self->line, self->column );
          webvtt_copy_string( &SP->v.text, &self->line_buffer );
          webvtt_release_string( &self->line_buffer );
          SP->type = V_TEXT;
          POP();
          finished = 1;
        } else {
          /**
           * If it's not the end of a cue, simply append it to the cue's payload
           * text.
           */
          if( webvtt_string_length( &cue->body ) &&
              WEBVTT_FAILED( webvtt_string_putc( &cue->body, '\n' ) ) ) {
            status = WEBVTT_OUT_OF_MEMORY;
            goto _finish;
          }
          webvtt_string_append_string( &cue->body, &self->line_buffer );
          webvtt_release_string( &self->line_buffer );
          flags = 0;
        }
      }
    }
  } while( pos < len && !finished );
_finish:
  *ppos = pos;
  if( finish ) {
    finished = 1;
  }

  /**
   * If we didn't encounter 2 successive EOLs, and it's not the final buffer in
   * the file, notify the caller.
   */
  if( !finish && pos >= len && !WEBVTT_FAILED( status ) && !finished ) {
    status = WEBVTT_UNFINISHED;
  }
  return status;
}

WEBVTT_INTERN webvtt_status
webvtt_proc_cuetext( webvtt_parser self, const webvtt_byte *b,
                     webvtt_uint *ppos, webvtt_uint len, webvtt_bool finish )
{
  webvtt_status status;
  webvtt_cue *cue;
  SAFE_ASSERT( ( self->mode == M_CUETEXT || self->mode == M_SKIP_CUE )
               && self->top->type == V_CUE );
  cue = self->top->v.cue;
  SAFE_ASSERT( cue != 0 );
  status  = webvtt_read_cuetext( self, b, ppos, len, finish );

  if( status == WEBVTT_SUCCESS ) {
    if( self->mode != M_SKIP_CUE ) {
      /**
       * Once we've successfully read the cuetext into line_buffer, call the
       * cuetext parser from cuetext.c
       */
      status = webvtt_parse_cuetext( self, cue, &cue->body,
                                     self->finished );

      /**
       * return the cue to the user, if possible.
       */
      finish_cue( self, &cue );
    } else {
      webvtt_release_cue( &cue );
    }

    self->top->type = V_NONE;
    self->top->state = 0;
    self->top->v.cue = 0;

    if( (self->top+1)->type == V_NONE ) {
      (self->top+1)->state = 0;
      /* Pop from T_CUE state */
      POP();
    } else {
      /**
       * If we found '-->', we need to create another cue and remain
       * in T_CUE state
       */
      webvtt_create_cue( &self->top->v.cue );
      self->top->type = V_CUE;
      self->top->state = T_CUE;
    }
    self->mode = M_WEBVTT;
  }
  return status;
}

WEBVTT_EXPORT webvtt_status
webvtt_parse_chunk( webvtt_parser self, const void *buffer, webvtt_uint len )
{
  webvtt_status status;
  webvtt_uint pos = 0;
  const webvtt_byte *b = ( const webvtt_byte * )buffer;

  while( pos < len ) {
    switch( self->mode ) {
      case M_WEBVTT:
        if( WEBVTT_FAILED( status = parse_webvtt( self, b, &pos, len, self->finished ) ) ) {
          return status;
        }
        break;

      case M_CUETEXT:
        /**
         * read in cuetext
         */
        if( WEBVTT_FAILED( status = webvtt_proc_cuetext( self, b, &pos, len,
                                                  self->finished ) ) ) {
          if( status == WEBVTT_UNFINISHED ) {
            /* Make an exception here, because this isn't really a failure. */
            return WEBVTT_SUCCESS;
          }
          return status;
        }

        /* If we failed to parse cuetext, return the error */
        if( WEBVTT_FAILED( status ) ) {
          return status;
        }
        break;

      case M_SKIP_CUE:
        if( WEBVTT_FAILED( status = webvtt_proc_cuetext( self, b, &pos, len,
                                                  self->finished ) ) ) {
          return status;
        }
        break;

      case M_READ_LINE: {
        /**
         * Read in a line of text into the line-buffer,
         * we will and depending on our state, do something with it.
         */
        int ret;
        if( ( ret = webvtt_string_getline( &self->line_buffer, b, &pos, len,
                                           &self->truncate,
                                           self->finished ) ) ) {
          if( ret < 0 ) {
            ERROR( WEBVTT_ALLOCATION_FAILED );
            return WEBVTT_OUT_OF_MEMORY;
          }
          self->mode = M_WEBVTT;
        }
        break;
      }
    }
  }

  return WEBVTT_SUCCESS;
}

#undef SP
#undef AT_BOTTOM
#undef ON_HEAP
#undef STACK_SIZE
#undef FRAME
#undef PUSH
#undef POP

/**
 * Get an integer value from a series of digits.
 */
static webvtt_int64
parse_int( const webvtt_byte **pb, int *pdigits )
{
  int digits = 0;
  webvtt_int64 result = 0;
  webvtt_int64 mul = 1;
  const webvtt_byte *b = *pb;
  while( *b ) {
    webvtt_byte ch = *b;
    if( webvtt_isdigit( ch ) ) {
      /**
       * Digit character, carry on
       */
      result = result * 10 + ( ch - '0' );
      ++digits;
    } else if( mul == 1 && digits == 0 && ch == '-' ) {
      mul = -1;
    } else {
      break;
    }
    ++b;
  }
  *pb = b;
  if( pdigits ) {
    *pdigits = digits;
  }
  return result * mul;
}

/**
 * Turn the token of a TIMESTAMP tag into something useful, and returns non-zero
 * returns 0 if it fails
 */
WEBVTT_INTERN int
parse_timestamp( const webvtt_byte *b, webvtt_timestamp *result )
{
  webvtt_int64 tmp;
  int have_hours = 0;
  int digits;
  int malformed = 0;
  webvtt_int64 v[4];
  if ( !webvtt_isdigit( *b ) ) {
    goto _malformed;
  }

  /* get sequence of digits */
  v[0] = parse_int( &b, &digits );

  /**
   * assume v[0] contains hours if more or less than 2 digits, or value is
   * greater than 59
   */
  if ( digits != 2 || v[0] > 59 ) {
    have_hours = 1;
  }

  /* fail if missing colon ':' character */
  if ( !*b || *b++ != ':' ) {
    malformed = 1;
  }

  /* fail if end of data reached, or byte is not an ASCII digit */
  if ( !*b || !webvtt_isdigit( *b ) ) {
    malformed = 1;
  }

  /* get another integer value, fail if digits is not equal to 2 */
  v[1] = parse_int( &b, &digits );
  if( digits != 2 ) {
    malformed = 1;
  }

  /* if we already know there's an hour component, or if the next byte is a
     colon ':', read the next value */
  if ( have_hours || ( *b == ':' ) ) {
    if( *b++ != ':' ) {
      goto _malformed;
    }
    if( !*b || !webvtt_isdigit( *b ) ) {
      malformed = 1;
    }
    v[2] = parse_int( &b, &digits );
    if( digits != 2 ) {
      malformed = 1;
    }
  } else {
    /* Otherwise, if there is no hour component, shift everything over */
    v[2] = v[1];
    v[1] = v[0];
    v[0] = 0;
  }

  /* collect the manditory seconds-frac component. fail if there is no FULL_STOP
     '.' or if there is no ascii digit following it */
  if( *b++ != '.' || !webvtt_isdigit( *b ) ) {
    goto _malformed;
  }
  v[3] = parse_int( &b, &digits );
  if( digits != 3 ) {
    malformed = 1;
  }

  /* Ensure that minutes and seconds are acceptable values */
  if( v[3] > 999 ) {
#define MILLIS_PER_SEC (1000)
    tmp = v[3];
    v[2] += tmp / MILLIS_PER_SEC;
    v[3] = tmp % MILLIS_PER_SEC;
    malformed = 1;
  }
  if( v[2] > 59 ) {
#define SEC_PER_MIN (60)
    tmp = v[2];
    v[1] += tmp / SEC_PER_MIN;
    v[2] = tmp % SEC_PER_MIN;
    malformed = 1;
  }
  if( v[1] > 59 ) {
#define MIN_PER_HOUR (60)
    tmp = v[1];
    v[0] += tmp / MIN_PER_HOUR;
    v[1] = tmp % MIN_PER_HOUR;
    malformed = 1;
  }

  *result = ( webvtt_timestamp )( v[0] * MSECS_PER_HOUR )
            + ( v[1] * MSECS_PER_MINUTE )
            + ( v[2] * MSECS_PER_SECOND )
            + ( v[3] );

  if( malformed ) {
    return 0;
  }
  return 1;
_malformed:
  *result = 0xFFFFFFFFFFFFFFFF;
  return 0;
}

WEBVTT_INTERN webvtt_bool
token_in_list( webvtt_token token, const webvtt_token list[] )
{
  int i = 0;
  webvtt_token t;
  while( ( t = list[ i++ ] ) != 0 ) {
    if( token == t ) {
      return 1;
    }
  }
  return 0;
}

WEBVTT_INTERN int
find_token( webvtt_token token, const webvtt_token list[] )
{
  int i = 0;
  webvtt_token t;
  while( ( t = list[ i ] ) != 0 ) {
    webvtt_token masked = t & TF_TOKEN_MASK;
    if( token == masked ) {
      return i;
    }
    ++i;
  }
  return -1;
}
