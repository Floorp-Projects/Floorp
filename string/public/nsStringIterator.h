/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Collins <scc@mozilla.org> (original author)
 */

#ifndef nsStringIterator_h___
#define nsStringIterator_h___

#ifndef nsStringFragment_h___
#include "nsStringFragment.h"
#endif

#ifndef nsCharTraits_h___
#include "nsCharTraits.h"
#endif

#ifndef nsStringTraits_h___
#include "nsStringTraits.h"
#endif

#ifndef nsAlgorithm_h___
#include "nsAlgorithm.h"
  // for |NS_MIN|, |NS_MAX|, and |NS_COUNT|...
#endif






  /**
   *
   * @see nsReadableFragment
   * @see nsAString
   */

template <class CharT>
class nsReadingIterator
//    : public bidirectional_iterator_tag
  {
    public:
      typedef ptrdiff_t                   difference_type;
      typedef CharT                       value_type;
      typedef const CharT*                pointer;
      typedef const CharT&                reference;
//    typedef bidirectional_iterator_tag  iterator_category;

      typedef nsReadableFragment<CharT>   const_fragment_type;
      typedef nsWritableFragment<CharT>   fragment_type;

    private:
      friend class nsAString;
      friend class nsACString;
      typedef typename nsStringTraits<CharT>::abstract_string_type abstract_string_type;

      const_fragment_type         mFragment;
      const CharT*                mPosition;
      const abstract_string_type* mOwningString;

    public:
      nsReadingIterator() { }
      // nsReadingIterator( const nsReadingIterator<CharT>& );                    // auto-generated copy-constructor OK
      // nsReadingIterator<CharT>& operator=( const nsReadingIterator<CharT>& );  // auto-generated copy-assignment operator OK

      inline void normalize_forward();
      inline void normalize_backward();

      pointer
      get() const
        {
          return mPosition;
        }
      
      CharT
      operator*() const
        {
          return *get();
        }

#if 0
        // An iterator really deserves this, but some compilers (notably IBM VisualAge for OS/2)
        //  don't like this when |CharT| is a type without members.
      pointer
      operator->() const
        {
          return get();
        }
#endif

      nsReadingIterator<CharT>&
      operator++()
        {
          ++mPosition;
          normalize_forward();
          return *this;
        }

      nsReadingIterator<CharT>
      operator++( int )
        {
          nsReadingIterator<CharT> result(*this);
          ++mPosition;
          normalize_forward();
          return result;
        }

      nsReadingIterator<CharT>&
      operator--()
        {
          normalize_backward();
          --mPosition;
          return *this;
        }

      nsReadingIterator<CharT>
      operator--( int )
        {
          nsReadingIterator<CharT> result(*this);
          normalize_backward();
          --mPosition;
          return result;
        }

      const const_fragment_type&
      fragment() const
        {
          return mFragment;
        }

      const abstract_string_type&
      string() const
        {
          NS_ASSERTION(mOwningString, "iterator not attached to a string (|mOwningString| == 0)");
          return *mOwningString;
        }

      difference_type
      size_forward() const
        {
          return mFragment.mEnd - mPosition;
        }

      difference_type
      size_backward() const
        {
          return mPosition - mFragment.mStart;
        }

      nsReadingIterator<CharT>&
      advance( difference_type n )
        {
          while ( n > 0 )
            {
              difference_type one_hop = NS_MIN(n, size_forward());

              NS_ASSERTION(one_hop>0, "Infinite loop: can't advance a reading iterator beyond the end of a string");
                // perhaps I should |break| if |!one_hop|?

              mPosition += one_hop;
              normalize_forward();
              n -= one_hop;
            }

          while ( n < 0 )
            {
              normalize_backward();
              difference_type one_hop = NS_MAX(n, -size_backward());

              NS_ASSERTION(one_hop<0, "Infinite loop: can't advance (backward) a reading iterator beyond the end of a string");
                // perhaps I should |break| if |!one_hop|?

              mPosition += one_hop;
              n -= one_hop;
            }

          return *this;
        }
  };

template <class CharT>
class nsWritingIterator
//  : public nsReadingIterator<CharT>
  {
    public:
      typedef ptrdiff_t                   difference_type;
      typedef CharT                       value_type;
      typedef CharT*                      pointer;
      typedef CharT&                      reference;
//    typedef bidirectional_iterator_tag  iterator_category;

      typedef nsReadableFragment<CharT>   const_fragment_type;
      typedef nsWritableFragment<CharT>   fragment_type;

    private:
      friend class nsAString;
      friend class nsACString;
      typedef typename nsStringTraits<CharT>::abstract_string_type abstract_string_type;

      fragment_type               mFragment;
      CharT*                      mPosition;
      abstract_string_type*       mOwningString;

    public:
      nsWritingIterator() { }
      // nsWritingIterator( const nsWritingIterator<CharT>& );                    // auto-generated copy-constructor OK
      // nsWritingIterator<CharT>& operator=( const nsWritingIterator<CharT>& );  // auto-generated copy-assignment operator OK

      inline void normalize_forward();
      inline void normalize_backward();

      pointer
      get() const
        {
          return mPosition;
        }
      
      reference
      operator*() const
        {
          return *get();
        }

#if 0
        // An iterator really deserves this, but some compilers (notably IBM VisualAge for OS/2)
        //  don't like this when |CharT| is a type without members.
      pointer
      operator->() const
        {
          return get();
        }
#endif

      nsWritingIterator<CharT>&
      operator++()
        {
          ++mPosition;
          normalize_forward();
          return *this;
        }

      nsWritingIterator<CharT>
      operator++( int )
        {
          nsWritingIterator<CharT> result(*this);
          ++mPosition;
          normalize_forward();
          return result;
        }

      nsWritingIterator<CharT>&
      operator--()
        {
          normalize_backward();
          --mPosition;
          return *this;
        }

      nsWritingIterator<CharT>
      operator--( int )
        {
          nsWritingIterator<CharT> result(*this);
          normalize_backward();
          --mPosition;
          return result;
        }

      const fragment_type&
      fragment() const
        {
          return mFragment;
        }

      fragment_type&
      fragment()
        {
          return mFragment;
        }

      const abstract_string_type&
      string() const
        {
          NS_ASSERTION(mOwningString, "iterator not attached to a string (|mOwningString| == 0)");
          return *mOwningString;
        }

      abstract_string_type&
      string()
        {
          NS_ASSERTION(mOwningString, "iterator not attached to a string (|mOwningString| == 0)");
          return *mOwningString;
        }

      difference_type
      size_forward() const
        {
          return mFragment.mEnd - mPosition;
        }

      difference_type
      size_backward() const
        {
          return mPosition - mFragment.mStart;
        }

      nsWritingIterator<CharT>&
      advance( difference_type n )
        {
          while ( n > 0 )
            {
              difference_type one_hop = NS_MIN(n, size_forward());

              NS_ASSERTION(one_hop>0, "Infinite loop: can't advance a writing iterator beyond the end of a string");
                // perhaps I should |break| if |!one_hop|?

              mPosition += one_hop;
              normalize_forward();
              n -= one_hop;
            }

          while ( n < 0 )
            {
              normalize_backward();
              difference_type one_hop = NS_MAX(n, -size_backward());

              NS_ASSERTION(one_hop<0, "Infinite loop: can't advance (backward) a writing iterator beyond the end of a string");
                // perhaps I should |break| if |!one_hop|?

              mPosition += one_hop;
              n -= one_hop;
            }

          return *this;
        }

      PRUint32
      write( const value_type* s, PRUint32 n )
        {
          NS_ASSERTION(size_forward() > 0, "You can't |write| into an |nsWritingIterator| with no space!");

          n = NS_MIN(n, PRUint32(size_forward()));
          nsCharTraits<value_type>::move(mPosition, s, n);
          advance( difference_type(n) );
          return n;
        }
  };

template <class CharT>
inline
void
nsReadingIterator<CharT>::normalize_forward()
  {
    while ( mPosition == mFragment.mEnd
         && mOwningString->GetReadableFragment(mFragment, kNextFragment) )
        mPosition = mFragment.mStart;
  }

template <class CharT>
inline
void
nsReadingIterator<CharT>::normalize_backward()
  {
    while ( mPosition == mFragment.mStart
         && mOwningString->GetReadableFragment(mFragment, kPrevFragment) )
        mPosition = mFragment.mEnd;
  }

template <class CharT>
inline
PRBool
operator==( const nsReadingIterator<CharT>& lhs, const nsReadingIterator<CharT>& rhs )
  {
    return lhs.get() == rhs.get();
  }

template <class CharT>
inline
PRBool
operator!=( const nsReadingIterator<CharT>& lhs, const nsReadingIterator<CharT>& rhs )
  {
    return lhs.get() != rhs.get();
  }


  //
  // |nsWritingIterator|s
  //

template <class CharT>
inline
void
nsWritingIterator<CharT>::normalize_forward()
  {
    while ( mPosition == mFragment.mEnd
         && mOwningString->GetWritableFragment(mFragment, kNextFragment) )
      mPosition = mFragment.mStart;
  }

template <class CharT>
inline
void
nsWritingIterator<CharT>::normalize_backward()
  {
    while ( mPosition == mFragment.mStart
         && mOwningString->GetWritableFragment(mFragment, kPrevFragment) )
      mPosition = mFragment.mEnd;
  }

template <class CharT>
inline
PRBool
operator==( const nsWritingIterator<CharT>& lhs, const nsWritingIterator<CharT>& rhs )
  {
    return lhs.get() == rhs.get();
  }

template <class CharT>
inline
PRBool
operator!=( const nsWritingIterator<CharT>& lhs, const nsWritingIterator<CharT>& rhs )
  {
    return lhs.get() != rhs.get();
  }

#endif /* !defined(nsStringIterator_h___) */
