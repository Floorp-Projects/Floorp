/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStringIterator_h___
#define nsStringIterator_h___

#include "nsCharTraits.h"
#include "nsAlgorithm.h"
#include "nsDebug.h"

  /**
   * @see nsTAString
   */

template <class CharT>
class nsReadingIterator
  {
    public:
      typedef nsReadingIterator<CharT>    self_type;
      typedef ptrdiff_t                   difference_type;
      typedef CharT                       value_type;
      typedef const CharT*                pointer;
      typedef const CharT&                reference;

    private:
      friend class nsAString;
      friend class nsACString;

        // unfortunately, the API for nsReadingIterator requires that the
        // iterator know its start and end positions.  this was needed when
        // we supported multi-fragment strings, but now it is really just
        // extra baggage.  we should remove mStart and mEnd at some point.

      const CharT* mStart;
      const CharT* mEnd;
      const CharT* mPosition;

    public:
      nsReadingIterator() { }
      // nsReadingIterator( const nsReadingIterator<CharT>& );                    // auto-generated copy-constructor OK
      // nsReadingIterator<CharT>& operator=( const nsReadingIterator<CharT>& );  // auto-generated copy-assignment operator OK

      inline void normalize_forward() {}
      inline void normalize_backward() {}

      pointer
      start() const
        {
          return mStart;
        }

      pointer
      end() const
        {
          return mEnd;
        }

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

      self_type&
      operator++()
        {
          ++mPosition;
          return *this;
        }

      self_type
      operator++( int )
        {
          self_type result(*this);
          ++mPosition;
          return result;
        }

      self_type&
      operator--()
        {
          --mPosition;
          return *this;
        }

      self_type
      operator--( int )
        {
          self_type result(*this);
          --mPosition;
          return result;
        }

      difference_type
      size_forward() const
        {
          return mEnd - mPosition;
        }

      difference_type
      size_backward() const
        {
          return mPosition - mStart;
        }

      self_type&
      advance( difference_type n )
        {
          if (n > 0)
            {
              difference_type step = XPCOM_MIN(n, size_forward());

              NS_ASSERTION(step>0, "can't advance a reading iterator beyond the end of a string");

              mPosition += step;
            }
          else if (n < 0)
            {
              difference_type step = XPCOM_MAX(n, -size_backward());

              NS_ASSERTION(step<0, "can't advance (backward) a reading iterator beyond the end of a string");

              mPosition += step;
            }
          return *this;
        }
  };

  /**
   * @see nsTAString
   */

template <class CharT>
class nsWritingIterator
  {
    public:
      typedef nsWritingIterator<CharT>   self_type;
      typedef ptrdiff_t                  difference_type;
      typedef CharT                      value_type;
      typedef CharT*                     pointer;
      typedef CharT&                     reference;

    private:
      friend class nsAString;
      friend class nsACString;

        // unfortunately, the API for nsWritingIterator requires that the
        // iterator know its start and end positions.  this was needed when
        // we supported multi-fragment strings, but now it is really just
        // extra baggage.  we should remove mStart and mEnd at some point.

      CharT* mStart;
      CharT* mEnd;
      CharT* mPosition;

    public:
      nsWritingIterator() { }
      // nsWritingIterator( const nsWritingIterator<CharT>& );                    // auto-generated copy-constructor OK
      // nsWritingIterator<CharT>& operator=( const nsWritingIterator<CharT>& );  // auto-generated copy-assignment operator OK

      inline void normalize_forward() {}
      inline void normalize_backward() {}

      pointer
      start() const
        {
          return mStart;
        }

      pointer
      end() const
        {
          return mEnd;
        }

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

      self_type&
      operator++()
        {
          ++mPosition;
          return *this;
        }

      self_type
      operator++( int )
        {
          self_type result(*this);
          ++mPosition;
          return result;
        }

      self_type&
      operator--()
        {
          --mPosition;
          return *this;
        }

      self_type
      operator--( int )
        {
          self_type result(*this);
          --mPosition;
          return result;
        }

      difference_type
      size_forward() const
        {
          return mEnd - mPosition;
        }

      difference_type
      size_backward() const
        {
          return mPosition - mStart;
        }

      self_type&
      advance( difference_type n )
        {
          if (n > 0)
            {
              difference_type step = XPCOM_MIN(n, size_forward());

              NS_ASSERTION(step>0, "can't advance a writing iterator beyond the end of a string");

              mPosition += step;
            }
          else if (n < 0)
            {
              difference_type step = XPCOM_MAX(n, -size_backward());

              NS_ASSERTION(step<0, "can't advance (backward) a writing iterator beyond the end of a string");

              mPosition += step;
            }
          return *this;
        }

      void
      write( const value_type* s, uint32_t n )
        {
          NS_ASSERTION(size_forward() > 0, "You can't |write| into an |nsWritingIterator| with no space!");

          nsCharTraits<value_type>::move(mPosition, s, n);
          advance( difference_type(n) );
        }
  };

template <class CharT>
inline
bool
operator==( const nsReadingIterator<CharT>& lhs, const nsReadingIterator<CharT>& rhs )
  {
    return lhs.get() == rhs.get();
  }

template <class CharT>
inline
bool
operator!=( const nsReadingIterator<CharT>& lhs, const nsReadingIterator<CharT>& rhs )
  {
    return lhs.get() != rhs.get();
  }


  //
  // |nsWritingIterator|s
  //

template <class CharT>
inline
bool
operator==( const nsWritingIterator<CharT>& lhs, const nsWritingIterator<CharT>& rhs )
  {
    return lhs.get() == rhs.get();
  }

template <class CharT>
inline
bool
operator!=( const nsWritingIterator<CharT>& lhs, const nsWritingIterator<CharT>& rhs )
  {
    return lhs.get() != rhs.get();
  }

#endif /* !defined(nsStringIterator_h___) */
