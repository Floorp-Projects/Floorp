/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@netscape.com>
 */

#ifndef _nsCharTraits_h__
#define _nsCharTraits_h__

#include <eof.h>

#include <string.h>
  // for |memcpy|, et al

#ifndef nscore_h___
#include "nscore.h"
  // for |PRUnichar|
#endif


#ifdef HAVE_CPP_BOOL
  typedef bool nsCharTraits_bool;
#else
  typedef PRBool nsCharTraits_bool;
#endif

template <class CharT>
struct nsCharTraits
  {
    typedef CharT char_type;

    static
    void
    assign( char_type& lhs, const char_type& rhs )
      {
        lhs = rhs;
      }


      // integer representation of characters:

    typedef int int_type;

    static
    char_type
    to_char_type( const int_type& c )
      {
        return char_type(c);
      }

    static
    int_type
    to_int_type( const char_type& c )
      {
        return int_type(c);
      }

    static
    nsCharTraits_bool
    eq_int_type( const int_type& lhs, const int_type& rhs )
      {
        return lhs == rhs;
      }


      // |char_type| comparisons:

    static
    nsCharTraits_bool
    eq( const char_type& lhs, const char_type& rhs )
      {
        return lhs == rhs;
      }

    static
    nsCharTraits_bool
    lt( const char_type& lhs, const char_type& rhs )
      {
        return lhs < rhs;
      }


      // operations on s[n] arrays:

    static
    char_type*
    copy( char_type* s1, const char_type* s2, size_t n )
      {
        char_type* result = s1;
        while ( n-- )
          assign(*s1++, *s2++);
        return result;
      }

    static
    char_type*
    move( char_type* s1, const char_type* s2, size_t n )
      {
        char_type* result = s1;

        if ( n )
          {
            if ( s2 > s1 )
              copy(s1, s2, n);
            else
              {
                s1 += n;
                s2 += n;
                while ( n-- )
                  assign(*--s1, *--s2);
              }
          }
        
        return result;
      }

    static
    char_type*
    assign( char_type* s, size_t n, const char_type& c )
      {
        char_type* result = s;
        while ( n-- )
          assign(*s++, c);
        return result;
      }

    static
    int
    compare( const char_type* s1, const char_type* s2, size_t n )
      {
        for ( ; n--; ++s1, ++s2 )
          {
            if ( lt(*s1, *s2) )
              return -1;
            if ( lt(*s2, *s1) )
              return 1;
          }

        return 0;
      }

    static
    size_t
    length( const char_type* s )
      {
        size_t result = 0;
        while ( !eq(*s++, CharT()) )
          ++result;
        return result;
      }

    static
    const char_type*
    find( const char_type* s, size_t n, const char_type& c )
      {
        while ( n-- )
          {
            if ( eq(*s, c) )
              return s;
            ++s;
          }

        return 0;
      }

#if 0
      // I/O related:

    typedef streamoff off_type;
    typedef streampos pos_type;
    typedef mbstate_t state_type;

    static
    int_type
    eof()
      {
        return EOF;
      }

    static
    int_type
    not_eof( const int_type& c )
      {
        return eq_int_type(c, eof()) ? ~eof() : c;
      }

    // static state_type get_state( pos_type );
#endif
  };

NS_SPECIALIZE_TEMPLATE
struct nsCharTraits<char>
  {
    typedef char char_type;

    static
    void
    assign( char& lhs, char rhs )
      {
        lhs = rhs;
      }


      // integer representation of characters:

    typedef int int_type;

    static
    char
    to_char_type( int c )
      {
        return char(c);
      }

    static
    int
    to_int_type( char c )
      {
        return int( NS_STATIC_CAST(unsigned char, c) );
      }

    static
    nsCharTraits_bool
    eq_int_type( int lhs, int rhs )
      {
        return lhs == rhs;
      }


      // |char_type| comparisons:

    static
    nsCharTraits_bool
    eq( char lhs, char rhs )
      {
        return lhs == rhs;
      }

    static
    nsCharTraits_bool
    lt( char lhs, char rhs )
      {
        return lhs < rhs;
      }


      // operations on s[n] arrays:

    static
    char*
    move( char* s1, const char* s2, size_t n )
      {
        return NS_STATIC_CAST(char*, memmove(s1, s2, n));
      }

    static
    char*
    copy( char* s1, const char* s2, size_t n )
      {
        return NS_STATIC_CAST(char*, memcpy(s1, s2, n));
      }

    static
    char*
    assign( char* s, size_t n, char c )
      {
        return NS_STATIC_CAST(char*, memset(s, to_int_type(c), n));
      }

    static
    int
    compare( const char* s1, const char* s2, size_t n )
      {
        return memcmp(s1, s2, n);
      }

    static
    size_t
    length( const char* s )
      {
        return strlen(s);
      }

    static
    const char*
    find( const char* s, size_t n, char c )
      {
        return NS_STATIC_CAST(char*, memchr(s, to_int_type(c), n));
      }

#if 0
      // I/O related:

    typedef streamoff off_type;
    typedef streampos pos_type;
    typedef mbstate_t state_type;

    static
    int_type
    eof()
      {
        return EOF;
      }

    static
    int
    not_eof( int c )
      {
        return c==eof() ? ~eof() : c;
      }

    // static state_type get_state( pos_type );
#endif
  };

NS_SPECIALIZE_TEMPLATE
struct nsCharTraits<wchar_t>
  {
    typedef wchar_t char_type;

    static
    void
    assign( wchar_t& lhs, wchar_t rhs )
      {
        lhs = rhs;
      }


      // integer representation of characters:

    typedef wint_t int_type;

    static
    wchar_t
    to_char_type( int_type c )
      {
        return wchar_t(c);
      }

    static
    int_type
    to_int_type( wchar_t c )
      {
        return int_type(c);
      }

    static
    nsCharTraits_bool
    eq_int_type( int_type lhs, int_type rhs )
      {
        return lhs == rhs;
      }


      // |char_type| comparisons:

    static
    nsCharTraits_bool
    eq( wchar_t lhs, wchar_t rhs )
      {
        return lhs == rhs;
      }

    static
    nsCharTraits_bool
    lt( wchar_t lhs, wchar_t rhs )
      {
        return lhs < rhs;
      }


      // operations on s[n] arrays:

    static
    wchar_t*
    move( wchar_t* s1, const wchar_t* s2, size_t n )
      {
        return NS_STATIC_CAST(wchar_t*, wmemmove(s1, s2, n));
      }

    static
    wchar_t*
    copy( wchar_t* s1, const wchar_t* s2, size_t n )
      {
        return NS_STATIC_CAST(wchar_t*, wmemcpy(s1, s2, n));
      }

    static
    wchar_t*
    assign( wchar_t* s, size_t n, wchar_t c )
      {
        return NS_STATIC_CAST(wchar_t*, wmemset(s, to_int_type(c), n));
      }

    static
    int
    compare( const wchar_t* s1, const wchar_t* s2, size_t n )
      {
        return wmemcmp(s1, s2, n);
      }

    static
    size_t
    length( const wchar_t* s )
      {
        return wcslen(s);
      }

    static
    const wchar_t*
    find( const wchar_t* s, size_t n, wchar_t c )
      {
        return NS_STATIC_CAST(wchar_t*, wmemchr(s, to_int_type(c), n));
      }

#if 0
      // I/O related:

    typedef streamoff off_type;
    typedef streampos pos_type;
    typedef mbstate_t state_type;

    static
    int_type
    eof()
      {
        return WEOF;
      }

    static
    int_type
    not_eof( int_type c )
      {
        return c==eof() ? ~eof() : c;
      }

    // static state_type get_state( pos_type );
#endif
  };

#endif // !defined(_nsCharTraits_h__)
