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

// This file may be used (through nsUTF8Utils.h) from non-XPCOM code, in
// particular the standalone software updater. In that case stub out
// the macros provided by nsDebug.h which are only usable when linking XPCOM

#ifdef NS_NO_XPCOM
#define NS_WARNING(msg)
#define NS_ASSERTION(cond, msg)
#define NS_ERROR(msg)
#else
#ifndef nsDebug_h__
#include "nsDebug.h"
  // for NS_ASSERTION
#endif
#endif

/*
 * Some macros for converting PRUnichar (UTF-16) to and from Unicode scalar
 * values.
 *
 * Note that UTF-16 represents all Unicode scalar values up to U+10FFFF by
 * using "surrogate pairs". These consist of a high surrogate, i.e. a code
 * point in the range U+D800 - U+DBFF, and a low surrogate, i.e. a code point
 * in the range U+DC00 - U+DFFF, like this:
 *
 *  U+D800 U+DC00 =  U+10000
 *  U+D800 U+DC01 =  U+10001
 *  ...
 *  U+DBFF U+DFFE = U+10FFFE
 *  U+DBFF U+DFFF = U+10FFFF
 *
 * These surrogate code points U+D800 - U+DFFF are not themselves valid Unicode
 * scalar values and are not well-formed UTF-16 except as high-surrogate /
 * low-surrogate pairs.
 */

#define PLANE1_BASE          PRUint32(0x00010000)
// High surrogates are in the range 0xD800 -- OxDBFF
#define NS_IS_HIGH_SURROGATE(u) ((PRUint32(u) & 0xFFFFFC00) == 0xD800)
// Low surrogates are in the range 0xDC00 -- 0xDFFF
#define NS_IS_LOW_SURROGATE(u)  ((PRUint32(u) & 0xFFFFFC00) == 0xDC00)
// Faster than testing NS_IS_HIGH_SURROGATE || NS_IS_LOW_SURROGATE
#define IS_SURROGATE(u)      ((PRUint32(u) & 0xFFFFF800) == 0xD800)

// Everything else is not a surrogate: 0x000 -- 0xD7FF, 0xE000 -- 0xFFFF

// N = (H - 0xD800) * 0x400 + 0x10000 + (L - 0xDC00)
// I wonder whether we could somehow assert that H is a high surrogate
// and L is a low surrogate
#define SURROGATE_TO_UCS4(h, l) (((PRUint32(h) & 0x03FF) << 10) + \
                                 (PRUint32(l) & 0x03FF) + PLANE1_BASE)

// Extract surrogates from a UCS4 char
// Reference: the Unicode standard 4.0, section 3.9
// Since (c - 0x10000) >> 10 == (c >> 10) - 0x0080 and 
// 0xD7C0 == 0xD800 - 0x0080,
// ((c - 0x10000) >> 10) + 0xD800 can be simplified to
#define H_SURROGATE(c) PRUnichar(PRUnichar(PRUint32(c) >> 10) + \
                                 PRUnichar(0xD7C0)) 
// where it's to be noted that 0xD7C0 is not bitwise-OR'd
// but added.

// Since 0x10000 & 0x03FF == 0, 
// (c - 0x10000) & 0x03FF == c & 0x03FF so that
// ((c - 0x10000) & 0x03FF) | 0xDC00 is equivalent to
#define L_SURROGATE(c) PRUnichar(PRUnichar(PRUint32(c) & PRUint32(0x03FF)) | \
                                 PRUnichar(0xDC00))

#define IS_IN_BMP(ucs) (PRUint32(ucs) < PLANE1_BASE)
#define UCS2_REPLACEMENT_CHAR PRUnichar(0xFFFD)

#define UCS_END PRUint32(0x00110000)
#define IS_VALID_CHAR(c) ((PRUint32(c) < UCS_END) && !IS_SURROGATE(c))
#define ENSURE_VALID_CHAR(c) (IS_VALID_CHAR(c) ? (c) : UCS2_REPLACEMENT_CHAR)

template <class CharT> struct nsCharTraits {};

NS_SPECIALIZE_TEMPLATE
struct nsCharTraits<PRUnichar>
  {
    typedef PRUnichar char_type;
    typedef PRUint16  unsigned_char_type;
    typedef char      incompatible_char_type;

    static char_type *sEmptyBuffer;

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
        return int_type( static_cast<unsigned_char_type>(c) );
      }

    static
    bool
    eq_int_type( int_type lhs, int_type rhs )
      {
        return lhs == rhs;
      }


      // |char_type| comparisons:

    static
    bool
    eq( char_type lhs, char_type rhs )
      {
        return lhs == rhs;
      }

    static
    bool
    lt( char_type lhs, char_type rhs )
      {
        return lhs < rhs;
      }


      // operations on s[n] arrays:

    static
    char_type*
    move( char_type* s1, const char_type* s2, size_t n )
      {
        return static_cast<char_type*>(memmove(s1, s2, n * sizeof(char_type)));
      }

    static
    char_type*
    copy( char_type* s1, const char_type* s2, size_t n )
      {
        return static_cast<char_type*>(memcpy(s1, s2, n * sizeof(char_type)));
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
        return static_cast<char_type*>(wmemset(s, to_int_type(c), n));
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
     * Convert c to its lower-case form, but only if the lower-case form is
     * ASCII. Otherwise leave it alone.
     *
     * There are only two non-ASCII Unicode characters whose lowercase
     * equivalents are ASCII: KELVIN SIGN and LATIN CAPITAL LETTER I WITH
     * DOT ABOVE. So it's a simple matter to handle those explicitly.
     */
    static
    char_type
    ASCIIToLower( char_type c )
      {
        if (c < 0x100)
          {
            if (c >= 'A' && c <= 'Z')
              return char_type(c + ('a' - 'A'));
          
            return c;
          }
        else
          {
            if (c == 0x212A) // KELVIN SIGN
              return 'k';
            if (c == 0x0130) // LATIN CAPITAL LETTER I WITH DOT ABOVE
              return 'i';
            return c;
          }
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
        return reinterpret_cast<const char_type*>(wmemchr(s, to_int_type(c), n));
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

    static char_type *sEmptyBuffer;

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
        return int_type( static_cast<unsigned_char_type>(c) );
      }

    static
    bool
    eq_int_type( int_type lhs, int_type rhs )
      {
        return lhs == rhs;
      }


      // |char_type| comparisons:

    static
    bool
    eq( char_type lhs, char_type rhs )
      {
        return lhs == rhs;
      }

    static
    bool
    lt( char_type lhs, char_type rhs )
      {
        return lhs < rhs;
      }


      // operations on s[n] arrays:

    static
    char_type*
    move( char_type* s1, const char_type* s2, size_t n )
      {
        return static_cast<char_type*>(memmove(s1, s2, n * sizeof(char_type)));
      }

    static
    char_type*
    copy( char_type* s1, const char_type* s2, size_t n )
      {
        return static_cast<char_type*>(memcpy(s1, s2, n * sizeof(char_type)));
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
        return static_cast<char_type*>(memset(s, to_int_type(c), n));
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
        if (c >= 'A' && c <= 'Z')
          return char_type(c + ('a' - 'A'));

        return c;
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
        return reinterpret_cast<const char_type*>(memchr(s, to_int_type(c), n));
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
        return PRUint32(last.get() - first.get());
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
    void
    write( OutputIterator& iter, const typename OutputIterator::value_type* s, PRUint32 n )
      {
        iter.write(s, n);
      }
  };

#ifdef HAVE_CPP_PARTIAL_SPECIALIZATION

template <class CharT>
struct nsCharSinkTraits<CharT*>
  {
    static
    void
    write( CharT*& iter, const CharT* s, PRUint32 n )
      {
        nsCharTraits<CharT>::move(iter, s, n);
        iter += n;
      }
  };

#else

NS_SPECIALIZE_TEMPLATE
struct nsCharSinkTraits<char*>
  {
    static
    void
    write( char*& iter, const char* s, PRUint32 n )
      {
        nsCharTraits<char>::move(iter, s, n);
        iter += n;
      }
  };

NS_SPECIALIZE_TEMPLATE
struct nsCharSinkTraits<PRUnichar*>
  {
    static
    void
    write( PRUnichar*& iter, const PRUnichar* s, PRUint32 n )
      {
        nsCharTraits<PRUnichar>::move(iter, s, n);
        iter += n;
      }
  };

#endif

#endif // !defined(nsCharTraits_h___)
