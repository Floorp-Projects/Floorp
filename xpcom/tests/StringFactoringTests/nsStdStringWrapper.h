/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsStdStringWrapper_h___
#define nsStdStringWrapper_h___

#include <string>

#ifndef nsAWritableString_h___
#include "nsAWritableString.h"
#endif


template <class T>
class nsStringAllocator
     : public std::allocator<T> // temporarily
  {
    // ...
  };


template < class CharT, class TraitsT = nsCharTraits<CharT>, class AllocatorT = nsStringAllocator<CharT> >
class basic_nsStdStringWrapper
      : public basic_nsAWritableString<CharT>
    /*
      ...
    */
  {
    protected:
      std::basic_string<CharT, TraitsT, AllocatorT> mRawString;

    typedef std::basic_string<CharT, TraitsT, AllocatorT> basic_string_t;

    using typename basic_string_t::traits_type;
    using typename basic_string_t::value_type;
    using typename basic_string_t::allocator_type;
    using typename basic_string_t::size_type;
    using typename basic_string_t::difference_type;
    using typename basic_string_t::reference;
    using typename basic_string_t::const_reference;
    using typename basic_string_t::pointer;
    using typename basic_string_t::const_pointer;
    using typename basic_string_t::iterator;
    using typename basic_string_t::const_iterator;
    using typename basic_string_t::reverse_iterator;
    using typename basic_string_t::const_reverse_iterator;

  	static const size_type npos = size_type(-1);

    protected:
      virtual const void* Implementation() const;

      virtual const CharT* GetReadableFragment( nsReadableFragment<CharT>&, nsFragmentRequest, PRUint32 ) const;
      virtual CharT* GetWritableFragment( nsWritableFragment<CharT>&, nsFragmentRequest, PRUint32 );

    public:
      basic_nsStdStringWrapper() { }

#if 0
    	explicit
    	basic_nsStdStringWrapper( const AllocatorT& a = AllocatorT() )
    	    : mRawString(a)
    	  {
    	  }
#endif

      explicit
      basic_nsStdStringWrapper( const basic_nsAReadableString<CharT>& str )
        {
          Assign(str);
        }

#if 0
      explicit
    	basic_nsStdStringWrapper( const basic_string_t& str, size_type pos = 0, size_type n = npos )
    	    : mRawString(str, pos, n)
    	  {
    	  }

    	basic_nsStdStringWrapper( const basic_string_t& str, size_type pos, size_type n, const AllocatorT& a )
    	    : mRawString(str, pos, n, a)
    	  {
    	  }
#endif

    	basic_nsStdStringWrapper( const CharT* s, size_type n, const AllocatorT& a = AllocatorT() )
    	    : mRawString(s, n, a)
    	  {
    	  }

      explicit
    	basic_nsStdStringWrapper( const CharT* s, const AllocatorT& a = AllocatorT() )
    	    : mRawString(s, a)
    	  {
    	  }

#if 0
    	basic_nsStdStringWrapper( size_type n, CharT c, const AllocatorT& a = AllocatorT() )
    	    : mRawString(n, c, a)
    	  {
    	  }
#endif

      virtual
      PRUint32
      Length() const
        {
          return mRawString.length();
        }

      virtual
      void
      SetCapacity( PRUint32 aNewCapacity )
        {
          mRawString.reserve(aNewCapacity);
        }

      virtual
      void
      SetLength( PRUint32 aNewLength )
        {
          mRawString.resize(aNewLength);
        }

    protected:
      virtual void do_AssignFromReadable( const basic_nsAReadableString<CharT>& );

    // ...
  };

NS_DEF_TEMPLATE_STRING_COMPARISON_OPERATORS(basic_nsStdStringWrapper<CharT>, CharT)



template <class CharT, class TraitsT, class AllocatorT>
const void*
basic_nsStdStringWrapper<CharT, TraitsT, AllocatorT>::Implementation() const
  {
    static const char* implementation = "nsStdStringWrapper";
    return implementation;
  }


template <class CharT, class TraitsT, class AllocatorT>
const CharT*
basic_nsStdStringWrapper<CharT, TraitsT, AllocatorT>::GetReadableFragment( nsReadableFragment<CharT>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          aFragment.mEnd = (aFragment.mStart = mRawString.data()) + mRawString.length();
          return aFragment.mStart + aOffset;
        
        case kPrevFragment:
        case kNextFragment:
        default:
          return 0;
      }
  }

template <class CharT, class TraitsT, class AllocatorT>
CharT*
basic_nsStdStringWrapper<CharT, TraitsT, AllocatorT>::GetWritableFragment( nsWritableFragment<CharT>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset )
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          aFragment.mEnd = (aFragment.mStart = NS_CONST_CAST(CharT*, mRawString.data())) + mRawString.length();
          return aFragment.mStart + aOffset;
        
        case kPrevFragment:
        case kNextFragment:
        default:
          return 0;
      }
  }

template <class CharT, class TraitsT, class AllocatorT>
void
basic_nsStdStringWrapper<CharT, TraitsT, AllocatorT>::do_AssignFromReadable( const basic_nsAReadableString<CharT>& rhs )
  {
    typedef basic_nsStdStringWrapper<CharT, TraitsT, AllocatorT> this_t;

    if ( SameImplementation(*this, rhs) )
      mRawString = NS_STATIC_CAST(this_t, rhs).mRawString;
    else
      basic_nsAWritableString<CharT>::do_AssignFromReadable(rhs);
  }


typedef basic_nsStdStringWrapper<PRUnichar> nsStdString;
typedef basic_nsStdStringWrapper<char>      nsStdCString;


#endif // !defined(nsStdStringWrapper_h___)
