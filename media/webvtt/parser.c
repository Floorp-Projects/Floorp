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
  UTF8_HYPHEN_MINUS, UTF8_HYPHEN_MINUS, UTF8_GREATER_THAN
};

#define MSECS_PER_HOUR (3600000)
#define MSECS_PER_MINUTE (60000)
#define MSECS_PER_SECOND (1000)
#define BUFFER (self->buffer + self->position)
#define MALFORMED_TIME ((webvtt_timestamp_t)-1.0)

static webvtt_status find_bytes( const webvtt_byte *buffer, webvtt_uint len, const webvtt_byte *sbytes, webvtt_uint slen );
static webvtt_status webvtt_skipwhite( const webvtt_byte *buffer, webvtt_uint *pos, webvtt_uint len );
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
  
  if( !self->finished ) {
    self->finished = 1;
    
    switch( self->mode ) {
      /**
       * We've left off parsing cue settings and are not in the empty state,
       * return WEBVTT_CUE_INCOMPLETE.
       */
      case M_WEBVTT:
        if( self->top->type != V_NONE ) {
          ERROR( WEBVTT_CUE_INCOMPLETE );
        }
        break;
      /** 
       * We've left off on trying to read in a cue text.
       * Parse the partial cue text read and pass the cue back to the 
       * application if possible.
       */
      case M_CUETEXT:
        status = webvtt_parse_cuetext( self, self->top->v.cue, 
                                       &self->line_buffer, self->finished );
        webvtt_release_string( &self->line_buffer );
        finish_cue( self, &self->top->v.cue );
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
    if( buffer[ *pos ] == UTF8_CARRIAGE_RETURN || buffer[ *pos ] == UTF8_LINE_FEED ) {
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
    if( ch == 0x20 || ch == 0x09 ) {
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
    if( ch == 0x20 || ch == 0x09 || ch == 0x0A || ch == 0x0D ) {
      break;
    } else {
      int length = webvtt_utf8_length( text + *pos );
      *pos += length;
      ++( *column );
    }
  }
}

static webvtt_status
webvtt_skipwhite( const webvtt_byte *buffer, webvtt_uint *pos, webvtt_uint len )
{
  if( !buffer || !pos ) {
    return WEBVTT_INVALID_PARAM;
  }

  for( ; *pos < len && webvtt_iswhite( buffer[ *pos ] ); (*pos)++ );

  return WEBVTT_SUCCESS;
}

static void
find_next_whitespace( const webvtt_byte *buffer, webvtt_uint *ppos, webvtt_uint len )
{
  webvtt_uint pos = *ppos;
  while( pos < len ) {
    webvtt_byte c = buffer[pos];
    if( c == UTF8_CARRIAGE_RETURN || c == UTF8_LINE_FEED || c == UTF8_SPACE || c == UTF8_TAB ) {
      break;
    }

    ++pos;
  }
  *ppos = pos;
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
static webvtt_status
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
              if( ch != 0x3A ) {
                webvtt_error e = WEBVTT_INVALID_CUESETTING;
                if( ch == 0x20 || ch == 0x09 ) {
                  column = self->column;
                  e = WEBVTT_UNEXPECTED_WHITESPACE;
                  skip_spacetab( text, pos, len, &self->column );
                  if( text[ *pos ] == 0x3A ) {
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
 skip_param:
            while( *pos < len && text[ *pos ] != 0x20
              && text[ *pos ] != 0x09 ) {
              if( text[ *pos ] == 0x0A || text[ *pos ] == 0x0D ) {
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
            if( ch != 0x20 && ch != 0x09
              && ch != 0x0D && ch != 0x0A ) {
              goto bad_value;
            }
          }
          switch( t ) {
            case INTEGER:
            case PERCENTAGE:
              if( ( flags & TF_SIGN_MASK ) != TF_SIGN_MASK ) {
                const webvtt_byte p = self->token[ 0 ];
                if( ( ( flags & TF_NEGATIVE ) && p != UTF8_HYPHEN_MINUS )
                  || ( ( flags & TF_POSITIVE ) && p == UTF8_HYPHEN_MINUS
) ) {
                  goto bad_value;
                }
              }
          }
          return i + 1;
        } else {
bad_value:
          ERROR_AT( bv, last_line, last_column );
bad_value_eol:
          while( *pos < len && text[ *pos ] != 0x20
            && text[ *pos ] != 0x09 ) {
            if( text[ *pos ] == 0x0A || text[ *pos ] == 0x0D ) {
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
webvtt_parse_align( webvtt_parser self, webvtt_cue *cue, const
webvtt_byte *text,
  webvtt_uint *pos, webvtt_uint len )
{
  webvtt_uint last_line = self->line;
  webvtt_uint last_column = self->column;
  webvtt_status v;
  webvtt_uint vc;
  webvtt_token values[] = { START, MIDDLE, END, LEFT, RIGHT, 0 };
  if( ( v = webvtt_parse_cuesetting( self, text, pos, len,
    WEBVTT_ALIGN_BAD_VALUE, ALIGN, values, &vc ) ) > 0 ) {
    if( cue->flags & CUE_HAVE_ALIGN ) {
      ERROR_AT( WEBVTT_ALIGN_ALREADY_SET, last_line, last_column );
    }
    cue->flags |= CUE_HAVE_ALIGN;
    switch( values[ v - 1 ] ) {
      case START: cue->settings.align = WEBVTT_ALIGN_START; break;
      case MIDDLE: cue->settings.align = WEBVTT_ALIGN_MIDDLE; break;
      case END: cue->settings.align = WEBVTT_ALIGN_END; break;
      case LEFT: cue->settings.align = WEBVTT_ALIGN_LEFT; break;
      case RIGHT: cue->settings.align = WEBVTT_ALIGN_RIGHT; break;
    }
  }
  return v >= 0 ? WEBVTT_SUCCESS : v;
}

WEBVTT_INTERN webvtt_status
webvtt_parse_line( webvtt_parser self, webvtt_cue *cue, const
webvtt_byte *text,
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
    webvtt_uint digits;
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

WEBVTT_INTERN int
parse_cueparams( webvtt_parser self, const webvtt_byte *buffer,
                 webvtt_uint len, webvtt_cue *cue )
{
  int digits;
  int have_ws = 0;
  int unexpected_whitespace = 0;
  webvtt_uint baddelim = 0;
  webvtt_uint pos = 0;
  webvtt_token last_token = 0;
  webvtt_uint last_line = self->line;

  enum cp_state {
    CP_T1, CP_T2, CP_T3, CP_T4, CP_T5, /* 'start' cuetime, whitespace1,
                   'separator', whitespace2, 'end' cuetime */
    CP_CS0, /* pre-cuesetting */

    CP_SD, /* cuesettings delimiter here */

    CP_V1, /* 'vertical' cuesetting */
    CP_P1, /* 'position' cuesetting */
    CP_A1, /* 'align' cuesetting */
    CP_S1, /* 'size' cuesetting */
    CP_L1, /* 'line' cuesetting */

    CP_SV, /* cuesettings value here */

    CP_V2,
    CP_P2,
    CP_A2,
    CP_S2,
    CP_L2,
  };

  enum cp_state last_state = CP_T1;
  enum cp_state state = CP_T1;

#define SETST(X) do { baddelim = 0; last_state = state; state = (X); } while( 0 )

  self->token_pos = 0;
  while( pos < len ) {
    webvtt_uint last_column = self->column;
    webvtt_token token = webvtt_lex( self, buffer, &pos, len, 1 );
    webvtt_uint tlen = self->token_pos;
    self->token_pos = 0;
_recheck:
    switch( state ) {
        /* start timestamp */
      case CP_T1:
        if( token == WHITESPACE && !unexpected_whitespace ) {
          ERROR_AT_COLUMN( WEBVTT_UNEXPECTED_WHITESPACE, self->column );
          unexpected_whitespace = 1;
        } else if( token == TIMESTAMP )
          if( !parse_timestamp( self->token, &cue->from ) ) {
            ERROR_AT_COLUMN(
              ( BAD_TIMESTAMP( cue->from )
                ? WEBVTT_EXPECTED_TIMESTAMP
                : WEBVTT_MALFORMED_TIMESTAMP ), last_column  );
            if( self->token_pos && !webvtt_isdigit( self->token[self->token_pos - 1] ) ) {
              while( pos < len && buffer[pos] != 0x09 && buffer[pos] != 0x20 ) { ++pos; }
            }
            if( BAD_TIMESTAMP( cue->from ) )
            { return -1; }
            SETST( CP_T2 );
          } else {
            SETST( CP_T2 );
          }
        else {
          ERROR_AT_COLUMN( WEBVTT_EXPECTED_TIMESTAMP, last_column );
          return -1;
        }
        break;
        /* end timestamp */
      case CP_T5:
        if( token == WHITESPACE ) {
          /* no problem, just ignore it and continue */
        } else if( token == TIMESTAMP )
          if( !parse_timestamp( self->token, &cue->until ) ) {
            ERROR_AT_COLUMN(
              ( BAD_TIMESTAMP( cue->until )
                ? WEBVTT_EXPECTED_TIMESTAMP
                : WEBVTT_MALFORMED_TIMESTAMP ), last_column  );
            if( !webvtt_isdigit( self->token[self->token_pos - 1] ) ) {
              while( pos < len && buffer[pos] != 0x09 && buffer[pos] != 0x20 ) { ++pos; }
            }
            if( BAD_TIMESTAMP( cue->until ) )
            { return -1; }
            SETST( CP_CS0 );
          } else {
            SETST( CP_CS0 );
          }
        else {
          ERROR_AT_COLUMN( WEBVTT_EXPECTED_TIMESTAMP, last_column );
          return -1;
        }
        break;

        /* whitespace 1 */
      case CP_T2:
        switch( token ) {
          case SEPARATOR:
            ERROR_AT_COLUMN( WEBVTT_EXPECTED_WHITESPACE, last_column );
            SETST( CP_T4 );
            break;
          case WHITESPACE:
            SETST( CP_T3 );
            break;
        }
        break;
      case CP_T3:
        switch( token ) {
          case WHITESPACE: /* ignore this whitespace */
            break;

          case SEPARATOR:
            SETST( CP_T4 );
            break;

          case TIMESTAMP:
            ERROR( WEBVTT_MISSING_CUETIME_SEPARATOR );
            SETST( CP_T5 );
            goto _recheck;

          default: /* some garbage */
            ERROR_AT_COLUMN( WEBVTT_EXPECTED_CUETIME_SEPARATOR, last_column );
            return -1;
        }
        break;
      case CP_T4:
        switch( token ) {
          case WHITESPACE:
            SETST( CP_T5 );
            break;
          case TIMESTAMP:
            ERROR_AT_COLUMN( WEBVTT_EXPECTED_WHITESPACE, last_column );
            goto _recheck;
          default:
            ERROR_AT_COLUMN( WEBVTT_EXPECTED_WHITESPACE, last_column );
            goto _recheck;
        }
        break;
#define CHKDELIM \
if( baddelim ) \
  ERROR_AT_COLUMN(WEBVTT_INVALID_CUESETTING_DELIMITER,baddelim); \
else if( !have_ws ) \
  ERROR_AT_COLUMN(WEBVTT_EXPECTED_WHITESPACE,last_column);

        /**
         * This section is "pre-cuesetting". We are expecting whitespace,
         * followed by a cuesetting keyword
         *
         * If we don't see a keyword, but have our whitespace, it is considered
         * a bad keyword (invalid cuesetting)
         *
         * Otherwise, if we don't have whitespace and have a bad token, it's an
         * invalid delimiter
         */
      case CP_CS0:
        switch( token ) {
          case WHITESPACE:
            have_ws = last_column;
            break;
          case VERTICAL:
            CHKDELIM have_ws = 0;
            SETST( CP_V1 );
            break;
          case POSITION:
            CHKDELIM have_ws = 0;
            SETST( CP_P1 );
            break;
          case ALIGN:
          {
            webvtt_status status;
            pos -= tlen; /* Required for parse_align() */
            self->column = last_column; /* Reset for parse_align() */
            status = webvtt_parse_align( self, cue, buffer, &pos, len );
            if( status == WEBVTT_PARSE_ERROR ) {
              return WEBVTT_PARSE_ERROR;
            }
          }
          break;

          case SIZE:
            CHKDELIM have_ws = 0;
            SETST( CP_S1 );
            break;
          case LINE:
          {
            webvtt_status status;
            pos -= tlen; /* Required for parse_align() */
            self->column = last_column; /* Reset for parse_align() */
            status = webvtt_parse_line( self, cue, buffer, &pos, len );
            if( status == WEBVTT_PARSE_ERROR ) {
              return WEBVTT_PARSE_ERROR;
            }
          }
          break;
          default:
            if( have_ws ) {
              ERROR_AT_COLUMN( WEBVTT_INVALID_CUESETTING, last_column );
            } else if( token == BADTOKEN ) {
              /* it was a bad delimiter... */
              if( !baddelim ) {
                baddelim = last_column;
              }
              ++pos;
            }
            while( pos < len && buffer[pos] != 0x09 && buffer[pos] != 0x20 ) {
              ++pos;
            }
        }
        break;
#define CS1(S) \
  if( token == COLON ) \
  { if(have_ws) { ERROR_AT_COLUMN(WEBVTT_UNEXPECTED_WHITESPACE,have_ws); } SETST((S)); have_ws = 0; } \
  else if( token == WHITESPACE && !have_ws ) \
  { \
    have_ws = last_column; \
  } \
  else \
  { \
    switch(token) \
    { \
    case LR: case RL: case INTEGER: case PERCENTAGE: case START: case MIDDLE: case END: case LEFT: case RIGHT: \
       ERROR_AT_COLUMN(WEBVTT_MISSING_CUESETTING_DELIMITER,have_ws ? have_ws : last_column); break; \
    default: \
      ERROR_AT_COLUMN(WEBVTT_INVALID_CUESETTING_DELIMITER,last_column); \
      while( pos < len && buffer[pos] != 0x20 && buffer[pos] != 0x09 ) ++pos; \
      break; \
    } \
    have_ws = 0; \
  }

        /**
         * If we get a COLON, we advance to the next state.
         * If we encounter whitespace first, fire an "unexpected whitespace"
         * error and continue. If we encounter a cue-setting value, fire a
         * "missing cuesetting delimiter" error otherwise (eg vertical;rl), fire
         * "invalid cuesetting delimiter" error
         *
         * this logic is performed by the CS1 macro, defined above
         */
      case CP_V1:
        CS1( CP_V2 );
        break;
      case CP_P1:
        CS1( CP_P2 );
        break;
      case CP_A1:
        CS1( CP_A2 );
        break;
      case CP_S1:
        CS1( CP_S2 );
        break;
      case CP_L1:
        CS1( CP_L2 );
        break;
#undef CS1

/* BV: emit the BAD_VALUE error for the appropriate setting, when required */
#define BV(T) \
ERROR_AT_COLUMN(WEBVTT_##T##_BAD_VALUE,last_column); \
while( pos < len && buffer[pos] != 0x20 && buffer[pos] != 0x09 ) ++pos; \
SETST(CP_CS0);

/* HV: emit the ALREADY_SET (have value) error for the appropriate setting, when required */
#define HV(T) \
if( cue->flags & CUE_HAVE_##T ) \
{ \
  ERROR_AT_COLUMN(WEBVTT_##T##_ALREADY_SET,last_column); \
}
/* WS: emit the WEBVTT_UNEXPECTED_WHITESPACE error when required. */
#define WS \
case WHITESPACE: \
  if( !have_ws ) \
  { \
    ERROR_AT_COLUMN(WEBVTT_UNEXPECTED_WHITESPACE,last_column); \
    have_ws = last_column; \
  } \
break

/* set that the cue already has a value for this */
#define SV(T) cue->flags |= CUE_HAVE_##T
      case CP_V2:
        HV( VERTICAL );
        switch( token ) {
            WS;
          case LR:
            cue->settings.vertical = WEBVTT_VERTICAL_LR;
            have_ws = 0;
            SETST( CP_CS0 );
            SV( VERTICAL );
            break;
          case RL:
            cue->settings.vertical = WEBVTT_VERTICAL_RL;
            have_ws = 0;
            SETST( CP_CS0 );
            SV( VERTICAL );
            break;
          default:
            BV( VERTICAL );
        }
        break;

      case CP_P2:
        HV( POSITION );
        switch( token ) {
            WS;
          case PERCENTAGE: {
            int digits;
            const webvtt_byte *t = self->token;
            webvtt_int64 v = parse_int( &t, &digits );
            if( v < 0 ) {
              BV( POSITION );
            }
            cue->settings.position = ( webvtt_uint )v;
            SETST( CP_CS0 );
            SV( POSITION );
          }
          break;
          default:
            BV( POSITION );
            break;
        }
        break;

      case CP_A2:
        HV( ALIGN );
        switch( token ) {
            WS;
          case START:
            cue->settings.align = WEBVTT_ALIGN_START;
            have_ws = 0;
            SETST( CP_CS0 );
            SV( ALIGN );
            break;
          case MIDDLE:
            cue->settings.align = WEBVTT_ALIGN_MIDDLE;
            have_ws = 0;
            SETST( CP_CS0 );
            SV( ALIGN );
            break;
          case END:
            cue->settings.align = WEBVTT_ALIGN_END;
            have_ws = 0;
            SETST( CP_CS0 );
            SV( ALIGN );
            break;
          case LEFT:
            cue->settings.align = WEBVTT_ALIGN_LEFT;
            have_ws = 0;
            SETST( CP_CS0 );
            SV( ALIGN );
            break;
          case RIGHT:
            cue->settings.align = WEBVTT_ALIGN_RIGHT;
            have_ws = 0;
            SETST( CP_CS0 );
            SV( ALIGN );
            break;
          default:
            BV( ALIGN );
            break;
        }
        break;

      case CP_S2:
        HV( SIZE );
        switch( token ) {
            WS;
          case PERCENTAGE: {
            int digits;
            const webvtt_byte *t = self->token;
            webvtt_int64 v = parse_int( &t, &digits );
            if( v < 0 ) {
              BV( SIZE );
            }
            cue->settings.size = ( webvtt_uint )v;
            SETST( CP_CS0 );
            SV( SIZE );
          }
          break;
          default:
            BV( SIZE );
            break;
        }
        break;

      case CP_L2:
        HV( LINE );
        switch( token ) {
            WS;
          case INTEGER: {
            const webvtt_byte *t = self->token;
            webvtt_int64 v = parse_int( &t, &digits );
            cue->snap_to_lines = 1;
            cue->settings.line = ( int )v;
            SETST( CP_CS0 );
            SV( LINE );
          }
          break;
          case PERCENTAGE: {
            const webvtt_byte *t = self->token;
            webvtt_int64 v = parse_int( &t, &digits );
            if( v < 0 ) {
              BV( POSITION );
            }
            cue->snap_to_lines = 0;
            cue->settings.line = ( int )v;
            SETST( CP_CS0 );
            SV( LINE );
          }
          break;
          default:
            BV( LINE );
            break;
        }
#undef BV
#undef HV
#undef SV
#undef WS
    }
    self->token_pos = 0;
    last_token = token;
  }
  /**
   * If we didn't finish in a good state...
   */
  if( state != CP_CS0 ) {
    /* if we never made it to the cuesettings, we didn't finish the cuetimes */
    if( state < CP_CS0 ) {
      ERROR( WEBVTT_UNFINISHED_CUETIMES );
      return -1;
    } else {
      /* if we did, we should report an error but continue parsing. */
      webvtt_error e = WEBVTT_INVALID_CUESETTING;
      switch( state ) {
        case CP_V2:
          e = WEBVTT_VERTICAL_BAD_VALUE;
          break;
        case CP_P2:
          e = WEBVTT_POSITION_BAD_VALUE;
          break;
        case CP_A2:
          e = WEBVTT_ALIGN_BAD_VALUE;
          break;
        case CP_S2:
          e = WEBVTT_SIZE_BAD_VALUE;
          break;
        case CP_L2:
          e = WEBVTT_LINE_BAD_VALUE;
          break;
      }
      ERROR( e );
    }
  } else {
    if( baddelim ) {
      ERROR_AT_COLUMN( WEBVTT_INVALID_CUESETTING_DELIMITER, baddelim );
    }
  }
#undef SETST
  return 0;
}

static webvtt_status
parse_webvtt( webvtt_parser self, const webvtt_byte *buffer, webvtt_uint *ppos,
              webvtt_uint len, webvtt_parse_mode *mode, int finish )
{
  webvtt_status status = WEBVTT_SUCCESS;
  webvtt_token token;
  webvtt_uint pos = *ppos;
  int settings_delimiter = 0;
  int skip_error = 0;
  int settings_whitespace = 0;

  while( pos < len ) {
    webvtt_uint last_column, last_line, last_pos;
    skip_error = 0;
_next:
    last_column = self->column;
    last_line = self->line;
    last_pos = pos;

    /**
     * If we're in certain states, we don't want to get a token and just
     * want to read text instead.
     */
    if( SP->state == T_CUEREAD ) {
      int v;
      webvtt_uint old_pos = pos;
      if( v = webvtt_string_getline( &SP->v.text, buffer, &pos,
                                        len, 0, finish, 0 ) ) {
        if( v < 0 ) {
          webvtt_release_string( &SP->v.text );
          SP->type = V_NONE;
          POP();
          ERROR( WEBVTT_ALLOCATION_FAILED );
          status = WEBVTT_OUT_OF_MEMORY;
          goto _finish;
        }
        /* POP the stack and let the previous frame deal with it */
        POP();
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
        } else {
          if( pos != len ) {
            if( !skip_error ) {
              ERROR_AT_COLUMN( WEBVTT_MALFORMED_TAG, last_column );
              skip_error = 1;
            }
            status = WEBVTT_PARSE_ERROR;
            goto _finish;
          }
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
        if( self->popped && FRAMEUP( 1 )->state == T_CUEREAD ) {
          /**
           * We're expecting either cue-id (contains '-->') or cue
           * params
           */
          webvtt_cue *cue = SP->v.cue;
          webvtt_state *st = FRAMEUP( 1 );
          webvtt_string text = st->v.text;

          /* FIXME: guard inconsistent state */
          if (!cue) {
            ERROR( WEBVTT_PARSE_ERROR );
            status = WEBVTT_PARSE_ERROR;
            goto _finish;
          }

          st->type = V_NONE;
          st->v.cue = NULL;

          /**
           * The type should be V_TEXT. If it's not, somethings wrong.
           *
           * TODO: Add debug assertion
           */
          if( find_bytes( webvtt_string_text( &text ), webvtt_string_length( &text ), separator,
                          sizeof( separator ) ) == WEBVTT_SUCCESS) {
            /* It's not a cue id, we found '-->'. It can't be a second
               cueparams line, because if we had it, we would be in
               a different state. */
            int v;
            /* backup the column */
            self->column = 1;
            if( ( v = parse_cueparams( self, webvtt_string_text( &text ),
                                       webvtt_string_length( &text ), cue ) ) < 0 ) {
              if( v == WEBVTT_PARSE_ERROR ) {
                status = WEBVTT_PARSE_ERROR;
                goto _finish;
              }
              webvtt_release_string( &text );
              *mode = M_SKIP_CUE;
              goto _finish;
            } else {
              webvtt_release_string( &text );
              cue->flags |= CUE_HAVE_CUEPARAMS;
              *mode = M_CUETEXT;
              goto _finish;
            }
          } else {
            /* It is a cue-id */
            if( cue && cue->flags & CUE_HAVE_ID ) {
              /**
               * This isn't actually a cue-id, because we already
               * have one. It seems to be cuetext, which is occurring
               * before cue-params
               */
              webvtt_release_string( &text );
              ERROR( WEBVTT_CUE_INCOMPLETE );
              *mode = M_SKIP_CUE;
              goto _finish;
            } else {
              self->column += webvtt_string_length( &text );
              if( WEBVTT_FAILED( status = webvtt_string_append(
                                            &cue->id, webvtt_string_text( &text ), webvtt_string_length( &text ) ) ) ) {
                webvtt_release_string( &text );
                ERROR( WEBVTT_ALLOCATION_FAILED );
              }

              cue->flags |= CUE_HAVE_ID;
            }
          }
          webvtt_release_string( &text );
          self->popped = 0;
        } else {
          webvtt_cue *cue = SP->v.cue;
          /* If we have a newline, it might be the end of the cue. */
          if( token == NEWLINE ) {
            if( cue->flags & CUE_HAVE_CUEPARAMS ) {
              *mode = M_CUETEXT;
            } else if( cue->flags & CUE_HAVE_ID ) {
              PUSH0( T_CUEREAD, 0, V_NONE );
            } else {
              /* I don't think this should ever happen? */
              POPBACK();
            }
          }
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

static webvtt_status
read_cuetext( webvtt_parser self, const webvtt_byte *b, webvtt_uint
*ppos, webvtt_uint len, webvtt_parse_mode *mode, webvtt_bool finish )
{
  webvtt_status status = WEBVTT_SUCCESS;
  webvtt_uint pos = *ppos;
  int finished = 0;
  do {
    int v;
    if( ( v = webvtt_string_getline( &self->line_buffer, b, &pos, len, &self->truncate, finish, 1 ) ) ) {
      if( v < 0 ) {
        status = WEBVTT_OUT_OF_MEMORY;
        goto _finish;
      }

      /**
       * We've encountered a line without any cuetext on it, i.e. there is no
       * newline character and len is 0 or there is and len is 1, therefore,
       * the cue text is finished.
       */
      if( self->line_buffer.d->length <= 1 ) {
        /**
         * finished
         */
        finished = 1;
      }
    }
  } while( pos < len && !finished );
_finish:
  *ppos = pos;
  /**
   * If we didn't encounter 2 successive EOLs, and it's not the final buffer in
   * the file, notify the caller.
   */
  if( pos >= len && !WEBVTT_FAILED( status ) && !finished ) {
    status = WEBVTT_UNFINISHED;
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
        if( WEBVTT_FAILED( status = parse_webvtt( self, b, &pos, len, &self->mode, self->finished ) ) ) {
          return status;
        }
        break;

      case M_CUETEXT:
        /**
         * read in cuetext
         */
        if( WEBVTT_FAILED( status = read_cuetext( self, b, &pos, len, &self->mode, self->finished ) ) ) {
          if( status == WEBVTT_UNFINISHED ) {
            /* Make an exception here, because this isn't really a failure. */
            return WEBVTT_SUCCESS;
          }
          return status;
        }
        /**
         * Once we've successfully read the cuetext into line_buffer, call the
         * cuetext parser from cuetext.c
         */
        status = webvtt_parse_cuetext( self, SP->v.cue, &self->line_buffer, self->finished );

        /**
         * return the cue to the user, if possible.
         */
        finish_cue( self, &SP->v.cue );

        /**
         * return to our typical parsing mode now.
         */
        SP->type = V_NONE;
        webvtt_release_string( &self->line_buffer );
        self->mode = M_WEBVTT;

        /* If we failed to parse cuetext, return the error */
        if( WEBVTT_FAILED( status ) ) {
          return status;
        }
        break;

      case M_SKIP_CUE:
        if( WEBVTT_FAILED( status = read_cuetext( self, b, &pos, len, &self->mode, self->finished ) ) ) {
          return status;
        }
        webvtt_release_string( &self->line_buffer );
        self->mode = M_WEBVTT;
        break;

      case M_READ_LINE: {
        /**
         * Read in a line of text into the line-buffer,
         * we will and depending on our state, do something with it.
         */
        int ret;
        if( ( ret = webvtt_string_getline( &self->line_buffer, b, &pos, len, &self->truncate, self->finished, 0 ) ) ) {
          if( ret < 0 ) {
            ERROR( WEBVTT_ALLOCATION_FAILED );
            return WEBVTT_OUT_OF_MEMORY;
          }
          self->mode = M_WEBVTT;
        }
        break;
      }
    }
    if( WEBVTT_FAILED( status = webvtt_skipwhite( b, &pos, len ) ) ) {
      return status;
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
      result = result * 10 + ( ch - UTF8_DIGIT_ZERO );
      ++digits;
    } else if( mul == 1 && digits == 0 && ch == UTF8_HYPHEN_MINUS ) {
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
  if ( !*b || *b++ != UTF8_COLON ) {
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
  if ( have_hours || ( *b == UTF8_COLON ) ) {
    if( *b++ != UTF8_COLON ) {
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
  if( *b++ != UTF8_FULL_STOP || !webvtt_isdigit( *b ) ) {
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
