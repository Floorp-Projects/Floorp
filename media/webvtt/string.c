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

#include "string_internal.h"
#include <stdlib.h>
#include <string.h>

/* TODO: Use libc implementation if we have one */

void *
memmem(const void *l, size_t l_len, const void *s, size_t s_len)
{
  register char *cur, *last;
  const char *cl = ( const char * )l;
  const char *cs = ( const char * )s;

  /* we need something to compare */
  if ( l_len == 0 || s_len == 0 ) {
    return NULL;
  }

  /* "s" must be smaller or equal to "l" */
  if ( l_len < s_len ) {
    return NULL;
  }

  /* special case where s_len == 1 */
  if ( s_len == 1 ) {
    return ( void * )memchr( l, ( int )*cs, l_len );
  }

  /* the last position where its possible to find "s" in "l" */
  last = (char *)cl + l_len - s_len;

  for ( cur = ( char * )cl; cur <= last; cur++ ) {
    if ( cur[0] == cs[0] && memcmp( cur, cs, s_len ) == 0 ) {
      return cur;
    }
  }
  return NULL;
}

static webvtt_string_data empty_string = {
  { 1 }, /* init refcount */
  0, /* length */
  0, /* capacity */
  empty_string.array, /* text */
  { '\0' } /* array */
};

WEBVTT_EXPORT void
webvtt_init_string( webvtt_string *result )
{
  if( result ) {
    result->d = &empty_string;
    webvtt_ref( &result->d->refs );
  }
}

WEBVTT_EXPORT webvtt_uint
webvtt_string_is_empty( const webvtt_string *str ) {
  return str->d == &empty_string || webvtt_string_length( str ) == 0 ? 1 : 0;
}

/**
 * Allocate new string.
 */
WEBVTT_EXPORT webvtt_status
webvtt_create_string( webvtt_uint32 alloc, webvtt_string *result )
{
  webvtt_string_data *d;

  if( !result ) {
    return WEBVTT_INVALID_PARAM;
  }

  d = ( webvtt_string_data * )webvtt_alloc( sizeof( webvtt_string_data ) +
                                            ( alloc * sizeof( char ) ) );

  if( !d ) {
    return WEBVTT_OUT_OF_MEMORY;
  }

  d->refs.value = 1;
  d->alloc = alloc;
  d->length = 0;
  d->text = d->array;
  d->text[0] = 0;

  result->d = d;

  return WEBVTT_SUCCESS;
}

WEBVTT_EXPORT webvtt_status
webvtt_create_string_with_text( webvtt_string *out, const char *init_text,
                                int len )
{
  if( !out ) {
    return WEBVTT_INVALID_PARAM;
  }

  if( !init_text ) {
    webvtt_init_string( out );
    return WEBVTT_SUCCESS;
  }

  if( len < 0 ) {
    len = strlen( init_text );
  }

  /**
   * initialize the string by referencing empty_string
   */
  webvtt_init_string( out );

  if( len == 0 ) {
    return WEBVTT_SUCCESS;
  }

  /**
   * append the appropriate data to the empty string
   */
  return webvtt_string_append( out, init_text, len );
}

/**
 * reference counting
 */
WEBVTT_EXPORT void
webvtt_ref_string( webvtt_string *str )
{
  if( str ) {
    webvtt_ref( &str->d->refs );
  }
}

WEBVTT_EXPORT void
webvtt_release_string( webvtt_string *str )
{
  /**
   * pulls the string data out of the string container, decreases the string
   */
  if( str ) {
    webvtt_string_data *d = str->d;
    str->d = 0;
    if( d && webvtt_deref( &d->refs ) == 0 ) {
      webvtt_free( d );
    }
  }
}

/**
 * "Detach" a shared string, so that it's safely mutable
 */
WEBVTT_EXPORT webvtt_status
webvtt_string_detach( /* in, out */ webvtt_string *str )
{
  webvtt_string_data *d, *q;

  if( !str ) {
    return WEBVTT_INVALID_PARAM;
  }

  q = str->d;

  if( q->refs.value == 1 ) {
    return WEBVTT_SUCCESS;
  }

  d = ( webvtt_string_data * )webvtt_alloc( sizeof( webvtt_string_data ) +
                                           ( sizeof( char ) * str->d->alloc ) );

  d->refs.value = 1;
  d->text = d->array;
  d->alloc = q->alloc;
  d->length = q->length;
  memcpy( d->text, q->text, q->length );

  str->d = d;

  if( webvtt_deref( &q->refs ) == 0 ) {
    webvtt_free( q );
  }

  return WEBVTT_SUCCESS;
}

WEBVTT_EXPORT void
webvtt_copy_string( webvtt_string *left, const webvtt_string *right )
{
  if( left ) {
    if( right && right->d ) {
      left->d = right->d;
    } else {
      left->d = &empty_string;
    }
    webvtt_ref( &left->d->refs );
  }
}

WEBVTT_EXPORT const char *
webvtt_string_text(const webvtt_string *str)
{
  if( !str || !str->d )
  {
    return 0;
  }

  return str->d->text;
}

WEBVTT_EXPORT webvtt_uint32
webvtt_string_length(const webvtt_string *str)
{
  if( !str || !str->d )
  {
    return 0;
  }

  return str->d->length;
}

WEBVTT_EXPORT webvtt_uint32
webvtt_string_capacity(const webvtt_string *str)
{
  if( !str || !str->d )
  {
    return 0;
  }

  return str->d->alloc;
}

/**
 * Reallocate string.
 * Grow to at least 'need' characters. Power of 2 growth.
 */
static webvtt_status
grow( webvtt_string *str, webvtt_uint need )
{
  static const webvtt_uint page = 0x1000;
  webvtt_uint32 n;
  webvtt_string_data *p, *d;
  webvtt_uint32 grow;

  if( !str )
  {
    return WEBVTT_INVALID_PARAM;
  }

  if( ( str->d->length + need ) <= str->d->alloc )
  {
    return WEBVTT_SUCCESS;
  }

  p = d = str->d;
  grow = sizeof( *d ) + ( sizeof( char ) * ( d->length + need ) );

  if( grow < page ) {
    n = page;
    do {
      n = n / 2;
    } while( n > grow );
    if( n < 1 << 6 ) {
      n = 1 << 6;
    } else {
      n = n * 2;
    }
  } else {
    n = page;
    do {
      n = n * 2;
    } while ( n < grow );
  }

  p = ( webvtt_string_data * )webvtt_alloc( n );

  if( !p ) {
    return WEBVTT_OUT_OF_MEMORY;
  }

  p->refs.value = 1;
  p->alloc = ( n - sizeof( *p ) ) / sizeof( char );
  p->length = d->length;
  p->text = p->array;
  memcpy( p->text, d->text, sizeof( char ) * p->length );
  p->text[ p->length ] = 0;
  str->d = p;

  if( webvtt_deref( &d->refs ) == 0 ) {
    webvtt_free( d );
  }

  return WEBVTT_SUCCESS;
}

WEBVTT_EXPORT int
webvtt_string_getline( webvtt_string *src, const char *buffer,
                       webvtt_uint *pos, int len, int *truncate,
                       webvtt_bool finish )
{
  int ret = 0;
  webvtt_string *str = src;
  webvtt_string_data *d = 0;
  const char *s = buffer + *pos;
  const char *p = s;
  const char *n;

  /**
   *if this is public now, maybe we should return webvtt_status so we can
   * differentiate between WEBVTT_OUT_OF_MEMORY and WEBVTT_INVALID_PARAM
   */
  if( !str ) {
    return -1;
  }

  /* This had better be a valid string_data, or else NULL. */
  d = str->d;
  if( !str->d ) {
    if(WEBVTT_FAILED(webvtt_create_string( 0x100, str ))) {
      return -1;
    }
    d = str->d;
  }
  if( len < 0 ) {
    len = strlen( buffer );
  }
  n = buffer + len;

  while( p < n && *p != '\r' && *p != '\n' ) {
    ++p;
  }

  if( p < n || finish ) {
    ret = 1; /* indicate that we found EOL */
  }
  len = (webvtt_uint)( p - s );
  *pos += len;
  if( d->length + len + 1 >= d->alloc ) {
    if( truncate && d->alloc >= WEBVTT_MAX_LINE ) {
      /* truncate. */
      (*truncate)++;
    } else {
      if( grow( str, len + 1 ) == WEBVTT_OUT_OF_MEMORY ) {
        ret = -1;
      }
      d = str->d;
    }
  }

  /* Copy everything in */
  if( len && ret >= 0 && d->length + len < d->alloc ) {
    memcpy( d->text + d->length, s, len );
    d->length += len;
    d->text[ d->length ] = 0;
  }

  return ret;
}

WEBVTT_EXPORT webvtt_status
webvtt_string_putc( webvtt_string *str, char to_append )
{
  webvtt_status result;

  if( !str ) {
    return WEBVTT_INVALID_PARAM;
  }

  if( WEBVTT_FAILED( result = webvtt_string_detach( str ) ) ) {
    return result;
  }

  if( !WEBVTT_FAILED( result = grow( str, 1 ) ) )
  {
    str->d->text[ str->d->length++ ] = to_append;
    str->d->text[ str->d->length ] = 0;
  }

  return result;
}

WEBVTT_EXPORT webvtt_bool
webvtt_string_is_equal( const webvtt_string *str, const char *to_compare,
                        int len )
{
  if( !str || !to_compare ) {
    return 0;
  }

  if( len < 0 ) {
    len = strlen( to_compare );
  }

  if( str->d->length != (unsigned)len ) {
    return 0;
  }

  return memcmp( webvtt_string_text( str ), to_compare, len ) == 0;
}

WEBVTT_EXPORT webvtt_status
webvtt_string_append( webvtt_string *str, const char *buffer, int len )
{
  webvtt_status result;

  if( !str || !buffer ) {
    return WEBVTT_INVALID_PARAM;
  }
  if( !str->d ) {
    webvtt_init_string( str );
  }

  if( len < 0 ) {
    len = strlen( buffer );
  }

  if( len == 0 ) {
    return WEBVTT_SUCCESS;
  }

  if( !WEBVTT_FAILED( result = grow( str, str->d->length + len ) ) ) {
    memcpy( str->d->text + str->d->length, buffer, len );
    str->d->length += len;
    /* null-terminate string */
    str->d->text[ str->d->length ] = 0;
  }

  return result;
}

WEBVTT_EXPORT webvtt_status
 webvtt_string_append_string( webvtt_string *str, const webvtt_string *other )
{
  if( !str || !other ) {
    return WEBVTT_INVALID_PARAM;
  }

  return webvtt_string_append( str, other->d->text, other->d->length );
}

WEBVTT_EXPORT webvtt_status
webvtt_string_replace( webvtt_string *str, const char *search, int search_len,
                       const char *replace, int replace_len )
{
  webvtt_status status = WEBVTT_SUCCESS;
  char *p;
  if( !str || !search || !replace ) {
    return WEBVTT_INVALID_PARAM;
  }

  if( search_len < 0 ) {
    search_len = ( int )strlen( search );
  }

  if( replace_len < 0 ) {
    replace_len = ( int )strlen( replace );
  }

  if( ( p = (char *)memmem( str->d->text, str->d->length, search,
                            search_len ) ) ) {
    const char *end;
    size_t pos = p - str->d->text;
    if( WEBVTT_FAILED( status = grow( str, replace_len ) ) ) {
      return status;
    }
    p = str->d->text + pos;
    end = str->d->text + str->d->length - 1; /* Don't worry about the NULL
                                              * byte. */
    if( search_len != replace_len ) {
      memmove( p + replace_len, p + search_len, end - p );
    }
    memcpy( p, replace, replace_len );
    str->d->length = ( str->d->length - search_len ) + replace_len;
    str->d->text[ str->d->length ] = 0;
    status = ( webvtt_status )1;
  }
  return status;
}

/**
 * webvtt_string_replace_all
 *
 * replace all instances of substring with replacement string
 */
WEBVTT_EXPORT webvtt_status
webvtt_string_replace_all( webvtt_string *str, const char *search,
                           int search_len, const char *replace,
                           int replace_len )
{
  webvtt_status status = WEBVTT_SUCCESS;
  if( !str || !search || !replace ) {
    return WEBVTT_INVALID_PARAM;
  }

  if( search_len < 0 ) {
    search_len = ( int )strlen( search );
  }

  if( replace_len < 0 ) {
    replace_len = ( int )strlen( replace );
  }

  while( ( status = webvtt_string_replace( str, search, search_len, replace,
                                           replace_len ) ) == 1 );
  return status;
}

/**
 * String lists
 */
WEBVTT_EXPORT webvtt_status
webvtt_create_stringlist( webvtt_stringlist **result )
{
  webvtt_stringlist *list;

  if( !result ) {
    return WEBVTT_INVALID_PARAM;
  }

  list = ( webvtt_stringlist * )webvtt_alloc0( sizeof( *list ) );

  if( !list ) {
    return WEBVTT_OUT_OF_MEMORY;
  }
  list->alloc = 0;
  list->length = 0;
  webvtt_ref_stringlist( list );

  *result = list;

  return WEBVTT_SUCCESS;
}

WEBVTT_EXPORT void
webvtt_ref_stringlist( webvtt_stringlist *list )
{
  if( list ) {
    webvtt_ref( &list->refs );
  }
}

WEBVTT_EXPORT void
webvtt_copy_stringlist( webvtt_stringlist **left, webvtt_stringlist *right )
{
  if( !left || !right ) {
    return;
  }
  *left = right;
  webvtt_ref_stringlist( *left );
}

WEBVTT_EXPORT void
webvtt_release_stringlist( webvtt_stringlist **list )
{
  webvtt_stringlist *l;
  webvtt_uint i;

  if( !list || !*list ) {
    return;
  }
  l = *list;

  if( webvtt_deref( &l->refs ) == 0 ) {
    if( l->items ) {
      for( i = 0; i < l->length; i++ ) {
        webvtt_release_string( &l->items[ i ] );
      }
      webvtt_free( l->items );
    }
    webvtt_free( l );
  }
  *list = 0;
}

WEBVTT_EXPORT webvtt_status
webvtt_stringlist_push( webvtt_stringlist *list, webvtt_string *str )
{
  if( !list || !str ) {
    return WEBVTT_INVALID_PARAM;
  }

  if( list->length + 1 >= ( ( list->alloc / 3 ) * 2 ) ) {
    webvtt_string *arr, *old;

    list->alloc = list->alloc == 0 ? 8 : list->alloc * 2;
    arr = ( webvtt_string * )webvtt_alloc0( sizeof( webvtt_string ) *
                                            list->alloc );

    if( !arr ) {
      return WEBVTT_OUT_OF_MEMORY;
    }

    memcpy( arr, list->items, sizeof( webvtt_string ) * list->length );
    old = list->items;
    list->items = arr;

    webvtt_free( old );
  }

  list->items[list->length].d = str->d;
  webvtt_ref_string( list->items + list->length++ );

  return WEBVTT_SUCCESS;
}

WEBVTT_EXPORT webvtt_bool
webvtt_stringlist_pop( webvtt_stringlist *list, webvtt_string *out )
{
  if( !list || !out || list->length < 1 ) {
    return 0;
  }

  list->length--;
  webvtt_copy_string( out, list->items + list->length );
  webvtt_release_string( list->items + list->length );

  return 1;
}

WEBVTT_EXPORT webvtt_bool
webvtt_next_utf8( const char **begin, const char *end )
{
  int c;
  const char *p;
  if( !begin || !*begin || !**begin || ( end && ( end <= *begin ) ) ) {
    /* Either begin is null, or end is null, or end <= begin */
    return 0;
  }

  p = *begin;

  if( !end ) {
    end = p + strlen( p );
  }

  c = webvtt_utf8_length( p );
  if( c > 0 ) {
    p += c;
  } else if( ( *p & 0xC0 ) == 0x80 ) {
    const char *pc = p + 1;
    while( pc < end && ( ( *pc & 0xC0 ) == 0x80 ) ) {
      ++pc;
    }
    if( pc <= end ) {
      p = pc;
    }
  }

  if( *begin != p && p <= end ) {
    *begin = p;
    return 1;
  }
  return 0;
}

WEBVTT_EXPORT webvtt_bool
webvtt_skip_utf8( const char **begin, const char *end, int n_chars )
{
  const char *first;
  if( !begin || !*begin ) {
    return 0;
  }

  if( n_chars < 0 ) {
    return 0;
  }

  first = *begin;
  if( !end ) {
    end = first + strlen( first );
  }

  if( end > first ) {
    /* forwards */
    while( n_chars && end > *begin ) {
      if( webvtt_next_utf8( begin, end ) ) {
        --n_chars;
      }
    }
  }

  return n_chars == 0;
}

WEBVTT_EXPORT webvtt_uint16
webvtt_utf8_to_utf16( const char *utf8, const char *end,
  webvtt_uint16 *high_surrogate )
{
  int need = 0;
  webvtt_uint32 uc = 0, min = 0;

  /* We're missing our pointers */
  if( !utf8 ) {
    return 0;
  }
  if( !end ) {
    end = utf8 + strlen( utf8 );
  }
  if( utf8 >= end ) {
    return 0;
  }

  /* If we are returning a surrogate pair, initialize it to 0 */
  if( high_surrogate ) {
    *high_surrogate = 0;
  }

  /* We're not at the start of a character */
  if( ( *utf8 & 0xC0 ) == 0x80 ) {
    return 0;
  }

  if( (unsigned char)*utf8 < 0x80 ) {
    return ( webvtt_uint32 )( *utf8 );
  }
  while( utf8 < end ) {
    char ch = *utf8;
    utf8++;
    if( need ) {
      if( ( ch & 0xC0 ) == 0x80 ) {
        uc = ( uc << 6 ) | ( ch & 0x3F );
        if (!--need) {
          int nc;
          if ( !( nc = UTF_IS_NONCHAR( uc ) ) && uc > 0xFFFF && uc < 0x110000) {
            /* Surrogate pair */
            if( high_surrogate ) {
              *high_surrogate = UTF_HIGH_SURROGATE( uc );
            }
            return UTF_LOW_SURROGATE( uc );
          } else if ( ( uc < min ) || ( uc >= 0xD800 && uc <= 0xDFFF ) || nc
                      || uc >= 0x110000) {
            /* Non-character, overlong sequence, or utf16 surrogate */
            return 0xFFFD;
          } else {
            /* Non-surrogate */
            return uc;
          }
        }
      }
    } else {
      if ( ( ch & 0xE0 ) == 0xC0 ) {
        uc = ch & 0x1f;
        need = 1;
        min = 0x80;
      } else if ( ( ch & 0xF0 ) == 0xE0 ) {
        uc = ch & 0x0f;
        need = 2;
        min = 0x800;
      } else if ( ( ch & 0xF8 ) == 0xF0 ) {
        uc = ch & 0x07;
        need = 3;
        min = 0x10000;
      } else {
        /* TODO This should deal with 5-7 byte sequences */
        /* return the replacement character in other cases */
        return 0xFFFD;
      }
    }
  }
  return 0;
}

WEBVTT_EXPORT int
webvtt_utf8_chcount( const char *utf8, const char *end )
{
  int n = 0;
  const char *p;
  if( !utf8 || !*utf8 || ( end != 0 && end < utf8 ) ) {
    return 0;
  }
  if( !end ) {
    end = utf8 + strlen( utf8 );
  }

  for( p = utf8; p < end; ++n ) {
    int c = webvtt_utf8_length( p );
    if( c < 1 ) {
      break;
    }
    p += c;
  }

  return n;
}

WEBVTT_EXPORT int
webvtt_utf8_length( const char *utf8 )
{
  char ch;
  if( !utf8 ) {
    return 0;
  }
  ch = *utf8;
  if( (unsigned char)ch < 0x80 ) {
    return 1;
  } else if( ( ch & 0xE0 ) == 0xC0 ) {
    return 2;
  } else if( ( ch & 0xF0 ) == 0xE0 ) {
    return 3;
  } else if( ( ch & 0xF8 ) == 0xF0 ) {
    return 4;
  } else if( ( ch & 0xFE ) == 0xFC ) {
    return 5;
  }
  return -1;
}
