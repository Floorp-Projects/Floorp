/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsCharTraits_h___
#define nsCharTraits_h___

#include <ctype.h>
  // for |EOF|, |WEOF|

#define FORCED_CPP_2BYTE_WCHAR_T
  // disable special optimizations for now through this hack

#if defined(HAVE_CPP_2BYTE_WCHAR_T) && !defined(FORCED_CPP_2BYTE_WCHAR_T)
#define USE_CPP_WCHAR_FUNCS
#endif

#ifdef USE_CPP_WCHAR_FUNCS
#include <wchar.h>
  // for |wmemset|, et al
#endif

#include <string.h>
  // for |memcpy|, et al

#ifndef nscore_h___
#include "nscore.h"
  // for |PRUnichar|
#endif

#ifndef nsDebug_h__
#include "nsDebug.h"
  // for NS_ASSERTION
#endif

#ifdef HAVE_CPP_BOOL
  typedef bool nsCharTraits_bool;
#else
  typedef PRBool nsCharTraits_bool;
#endif

template <class CharT> struct nsCharTraits {};

NS_SPECIALIZE_TEMPLATE
struct nsCharTraits<PRUnichar>
  {
    typedef PRUnichar char_type;
    typedef PRUint16  unsigned_char_type;
    typedef char      incompatible_char_type;

    NS_COM static const char_type *sEmptyBuffer;

    static
    void
    assign( char_type& lhs, char_type rhs )
      {
        lhs = rhs;
      }


      // integer representation of characters:

#ifdef USE_CPP_WCHAR_FUNCS
    typedef wint_t int_type;
#else
    typedef int int_type;
#endif

    static
    char_type
    to_char_type( int_type c )
      {
        return char_type(c);
      }

    static
    int_type
    to_int_type( char_type c )
      {
        return int_type( NS_STATIC_CAST(unsigned_char_type, c) );
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
    eq( char_type lhs, char_type rhs )
      {
        return lhs == rhs;
      }

    static
    nsCharTraits_bool
    lt( char_type lhs, char_type rhs )
      {
        return lhs < rhs;
      }


      // operations on s[n] arrays:

    static
    char_type*
    move( char_type* s1, const char_type* s2, size_t n )
      {
        return NS_STATIC_CAST(char_type*, memmove(s1, s2, n * sizeof(char_type)));
      }

    static
    char_type*
    copy( char_type* s1, const char_type* s2, size_t n )
      {
        return NS_STATIC_CAST(char_type*, memcpy(s1, s2, n * sizeof(char_type)));
      }

    static
    char_type*
    copyASCII( char_type* s1, const char* s2, size_t n )
      {
        for (char_type* s = s1; n--; ++s, ++s2) {
          NS_ASSERTION(!(*s2 & ~0x7F), "Unexpected non-ASCII character");
          *s = *s2;
        }
        return s1;
      }

    static
    char_type*
    assign( char_type* s, size_t n, char_type c )
      {
#ifdef USE_CPP_WCHAR_FUNCS
        return NS_STATIC_CAST(char_type*, wmemset(s, to_int_type(c), n));
#else
        char_type* result = s;
        while ( n-- )
          assign(*s++, c);
        return result;
#endif
      }

    static
    int
    compare( const char_type* s1, const char_type* s2, size_t n )
      {
#ifdef USE_CPP_WCHAR_FUNCS
        return wmemcmp(s1, s2, n);
#else
        for ( ; n--; ++s1, ++s2 )
          {
            if ( !eq(*s1, *s2) )
              return to_int_type(*s1) - to_int_type(*s2);
          }

        return 0;
#endif
      }

    static
    int
    compareASCII( const char_type* s1, const char* s2, size_t n )
      {
        for ( ; n--; ++s1, ++s2 )
          {
            NS_ASSERTION(!(*s2 & ~0x7F), "Unexpected non-ASCII character");
            if ( !eq_int_type(to_int_type(*s1), to_int_type(*s2)) )
              return to_int_type(*s1) - to_int_type(*s2);
          }

        return 0;
      }

    // this version assumes that s2 is null-terminated and s1 has length n.
    // if s1 is shorter than s2 then we return -1; if s1 is longer than s2,
    // we return 1.
    static
    int
    compareASCIINullTerminated( const char_type* s1, size_t n, const char* s2 )
      {
        for ( ; n--; ++s1, ++s2 )
          {
            if ( !*s2 )
              return 1;
            NS_ASSERTION(!(*s2 & ~0x7F), "Unexpected non-ASCII character");
            if ( !eq_int_type(to_int_type(*s1), to_int_type(*s2)) )
              return to_int_type(*s1) - to_int_type(*s2);
          }

        if ( *s2 )
          return -1;

        return 0;
      }

    /**
     * Convert c to its lower-case form, but only if c is ASCII.
     */
    static
    char_type
    ASCIIToLower( char_type c )
      {
        return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
      }

    static
    int
    compareLowerCaseToASCII( const char_type* s1, const char* s2, size_t n )
      {
        for ( ; n--; ++s1, ++s2 )
          {
            NS_ASSERTION(!(*s2 & ~0x7F), "Unexpected non-ASCII character");
            NS_ASSERTION(!(*s2 >= 'A' && *s2 <= 'Z'),
                         "Unexpected uppercase character");
            char_type lower_s1 = ASCIIToLower(*s1);
            if ( lower_s1 != to_char_type(*s2) )
              return to_int_type(lower_s1) - to_int_type(*s2);
          }

        return 0;
      }

    // this version assumes that s2 is null-terminated and s1 has length n.
    // if s1 is shorter than s2 then we return -1; if s1 is longer than s2,
    // we return 1.
    static
    int
    compareLowerCaseToASCIINullTerminated( const char_type* s1, size_t n, const char* s2 )
      {
        for ( ; n--; ++s1, ++s2 )
          {
            if ( !*s2 )
              return 1;
            NS_ASSERTION(!(*s2 & ~0x7F), "Unexpected non-ASCII character");
            NS_ASSERTION(!(*s2 >= 'A' && *s2 <= 'Z'),
                         "Unexpected uppercase character");
            char_type lower_s1 = ASCIIToLower(*s1);
            if ( lower_s1 != to_char_type(*s2) )
              return to_int_type(lower_s1) - to_int_type(*s2);
          }

        if ( *s2 )
          return -1;

        return 0;
      }

    static
    size_t
    length( const char_type* s )
      {
#ifdef USE_CPP_WCHAR_FUNCS
        return wcslen(s);
#else
        size_t result = 0;
        while ( !eq(*s++, char_type(0)) )
          ++result;
        return result;
#endif
      }

    static
    const char_type*
    find( const char_type* s, size_t n, char_type c )
      {
#ifdef USE_CPP_WCHAR_FUNCS
        return NS_REINTERPRET_CAST(const char_type*, wmemchr(s, to_int_type(c), n));
#else
        while ( n-- )
          {
            if ( eq(*s, c) )
              return s;
            ++s;
          }

        return 0;
#endif
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
#ifdef USE_CPP_WCHAR_FUNCS
        return WEOF;
#else
        return EOF;
#endif
      }

    static
    int_type
    not_eof( int_type c )
      {
        return eq_int_type(c, eof()) ? ~eof() : c;
      }

    // static state_type get_state( pos_type );
#endif
  };

NS_SPECIALIZE_TEMPLATE
struct nsCharTraits<char>
  {
    typedef char           char_type;
    typedef unsigned char  unsigned_char_type;
    typedef PRUnichar      incompatible_char_type;

    NS_COM static const char_type *sEmptyBuffer;

    static
    void
    assign( char_type& lhs, char_type rhs )
      {
        lhs = rhs;
      }


      // integer representation of characters:

    typedef int int_type;

    static
    char_type
    to_char_type( int_type c )
      {
        return char_type(c);
      }

    static
    int_type
    to_int_type( char_type c )
      {
        return int_type( NS_STATIC_CAST(unsigned_char_type, c) );
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
    eq( char_type lhs, char_type rhs )
      {
        return lhs == rhs;
      }

    static
    nsCharTraits_bool
    lt( char_type lhs, char_type rhs )
      {
        return lhs < rhs;
      }


      // operations on s[n] arrays:

    static
    char_type*
    move( char_type* s1, const char_type* s2, size_t n )
      {
        return NS_STATIC_CAST(char_type*, memmove(s1, s2, n * sizeof(char_type)));
      }

    static
    char_type*
    copy( char_type* s1, const char_type* s2, size_t n )
      {
        return NS_STATIC_CAST(char_type*, memcpy(s1, s2, n * sizeof(char_type)));
      }

    static
    char_type*
    copyASCII( char_type* s1, const char* s2, size_t n )
      {
        return copy(s1, s2, n);
      }

    static
    char_type*
    assign( char_type* s, size_t n, char_type c )
      {
        return NS_STATIC_CAST(char_type*, memset(s, to_int_type(c), n));
      }

    static
    int
    compare( const char_type* s1, const char_type* s2, size_t n )
      {
        return memcmp(s1, s2, n);
      }

    static
    int
    compareASCII( const char_type* s1, const char* s2, size_t n )
      {
#ifdef DEBUG
        for (size_t i = 0; i < n; ++i)
          {
            NS_ASSERTION(!(s2[i] & ~0x7F), "Unexpected non-ASCII character");
          }
#endif
        return compare(s1, s2, n);
      }

    // this version assumes that s2 is null-terminated and s1 has length n.
    // if s1 is shorter than s2 then we return -1; if s1 is longer than s2,
    // we return 1.
    static
    int
    compareASCIINullTerminated( const char_type* s1, size_t n, const char* s2 )
      {
        // can't use strcmp here because we don't want to stop when s1
        // contains a null
        for ( ; n--; ++s1, ++s2 )
          {
            if ( !*s2 )
              return 1;
            NS_ASSERTION(!(*s2 & ~0x7F), "Unexpected non-ASCII character");
            if ( *s1 != *s2 )
              return to_int_type(*s1) - to_int_type(*s2);
          }

        if ( *s2 )
          return -1;

        return 0;
      }

    /**
     * Convert c to its lower-case form, but only if c is ASCII.
     */
    static
    char_type
    ASCIIToLower( char_type c )
      {
        return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
      }

    static
    int
    compareLowerCaseToASCII( const char_type* s1, const char* s2, size_t n )
      {
        for ( ; n--; ++s1, ++s2 )
          {
            NS_ASSERTION(!(*s2 & ~0x7F), "Unexpected non-ASCII character");
            NS_ASSERTION(!(*s2 >= 'A' && *s2 <= 'Z'),
                         "Unexpected uppercase character");
            char_type lower_s1 = ASCIIToLower(*s1);
            if ( lower_s1 != *s2 )
              return to_int_type(lower_s1) - to_int_type(*s2);
          }
        return 0;
      }

    // this version assumes that s2 is null-terminated and s1 has length n.
    // if s1 is shorter than s2 then we return -1; if s1 is longer than s2,
    // we return 1.
    static
    int
    compareLowerCaseToASCIINullTerminated( const char_type* s1, size_t n, const char* s2 )
      {
        for ( ; n--; ++s1, ++s2 )
          {
            if ( !*s2 )
              return 1;
            NS_ASSERTION(!(*s2 & ~0x7F), "Unexpected non-ASCII character");
            NS_ASSERTION(!(*s2 >= 'A' && *s2 <= 'Z'),
                         "Unexpected uppercase character");
            char_type lower_s1 = ASCIIToLower(*s1);
            if ( lower_s1 != *s2 )
              return to_int_type(lower_s1) - to_int_type(*s2);
          }

        if ( *s2 )
          return -1;

        return 0;
      }

    static
    size_t
    length( const char_type* s )
      {
        return strlen(s);
      }

    static
    const char_type*
    find( const char_type* s, size_t n, char_type c )
      {
        return NS_REINTERPRET_CAST(const char_type*, memchr(s, to_int_type(c), n));
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
    not_eof( int_type c )
      {
        return eq_int_type(c, eof()) ? ~eof() : c;
      }

    // static state_type get_state( pos_type );
#endif
  };

template <class InputIterator>
struct nsCharSourceTraits
  {
    typedef typename InputIterator::difference_type difference_type;

    static
    PRUint32
    readable_distance( const InputIterator& first, const InputIterator& last )
      {
        // assumes single fragment
        return last.get() - first.get();
      }

    static
    const typename InputIterator::value_type*
    read( const InputIterator& iter )
      {
        return iter.get();
      }

    static
    void
    advance( InputIterator& s, difference_type n )
      {
        s.advance(n);
      }
  };

#ifdef HAVE_CPP_PARTIAL_SPECIALIZATION

template <class CharT>
struct nsCharSourceTraits<CharT*>
  {
    typedef ptrdiff_t difference_type;

    static
    PRUint32
    readable_distance( CharT* s )
      {
        return PRUint32(nsCharTraits<CharT>::length(s));
//      return numeric_limits<PRUint32>::max();
      }

    static
    PRUint32
    readable_distance( CharT* first, CharT* last )
      {
        return PRUint32(last-first);
      }

    static
    const CharT*
    read( CharT* s )
      {
        return s;
      }

    static
    void
    advance( CharT*& s, difference_type n )
      {
        s += n;
      }
  };

#else

NS_SPECIALIZE_TEMPLATE
struct nsCharSourceTraits<const char*>
  {
    typedef ptrdiff_t difference_type;

    static
    PRUint32
    readable_distance( const char* s )
      {
        return PRUint32(nsCharTraits<char>::length(s));
//      return numeric_limits<PRUint32>::max();
      }

    static
    PRUint32
    readable_distance( const char* first, const char* last )
      {
        return PRUint32(last-first);
      }

    static
    const char*
    read( const char* s )
      {
        return s;
      }

    static
    void
    advance( const char*& s, difference_type n )
      {
        s += n;
      }
 };


NS_SPECIALIZE_TEMPLATE
struct nsCharSourceTraits<const PRUnichar*>
  {
    typedef ptrdiff_t difference_type;

    static
    PRUint32
    readable_distance( const PRUnichar* s )
      {
        return PRUint32(nsCharTraits<PRUnichar>::length(s));
//      return numeric_limits<PRUint32>::max();
      }

    static
    PRUint32
    readable_distance( const PRUnichar* first, const PRUnichar* last )
      {
        return PRUint32(last-first);
      }

    static
    const PRUnichar*
    read( const PRUnichar* s )
      {
        return s;
      }

    static
    void
    advance( const PRUnichar*& s, difference_type n )
      {
        s += n;
      }
 };

#endif


template <class OutputIterator>
struct nsCharSinkTraits
  {
    static
    PRUint32
    write( OutputIterator& iter, const typename OutputIterator::value_type* s, PRUint32 n )
      {
        return iter.write(s, n);
      }
  };

#ifdef HAVE_CPP_PARTIAL_SPECIALIZATION

template <class CharT>
struct nsCharSinkTraits<CharT*>
  {
    static
    PRUint32
    write( CharT*& iter, const CharT* s, PRUint32 n )
      {
        nsCharTraits<CharT>::move(iter, s, n);
        iter += n;
        return n;
      }
  };

#else

NS_SPECIALIZE_TEMPLATE
struct nsCharSinkTraits<char*>
  {
    static
    PRUint32
    write( char*& iter, const char* s, PRUint32 n )
      {
        nsCharTraits<char>::move(iter, s, n);
        iter += n;
        return n;
      }
  };

NS_SPECIALIZE_TEMPLATE
struct nsCharSinkTraits<PRUnichar*>
  {
    static
    PRUint32
    write( PRUnichar*& iter, const PRUnichar* s, PRUint32 n )
      {
        nsCharTraits<PRUnichar>::move(iter, s, n);
        iter += n;
        return n;
      }
  };

#endif

#endif // !defined(nsCharTraits_h___)
