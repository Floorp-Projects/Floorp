/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
  typedef size_t                      size_type;
  typedef CharT                       value_type;
  typedef const CharT*                pointer;
  typedef const CharT&                reference;

private:
  friend class mozilla::detail::nsTStringRepr<CharT>;

  // unfortunately, the API for nsReadingIterator requires that the
  // iterator know its start and end positions.  this was needed when
  // we supported multi-fragment strings, but now it is really just
  // extra baggage.  we should remove mStart and mEnd at some point.

  const CharT* mStart;
  const CharT* mEnd;
  const CharT* mPosition;

public:
  nsReadingIterator()
    : mStart(nullptr)
    , mEnd(nullptr)
    , mPosition(nullptr)
  {
  }
  // nsReadingIterator( const nsReadingIterator<CharT>& );                    // auto-generated copy-constructor OK
  // nsReadingIterator<CharT>& operator=( const nsReadingIterator<CharT>& );  // auto-generated copy-assignment operator OK

  pointer get() const
  {
    return mPosition;
  }

  CharT operator*() const
  {
    return *get();
  }

  self_type& operator++()
  {
    ++mPosition;
    return *this;
  }

  self_type operator++(int)
  {
    self_type result(*this);
    ++mPosition;
    return result;
  }

  self_type& operator--()
  {
    --mPosition;
    return *this;
  }

  self_type operator--(int)
  {
    self_type result(*this);
    --mPosition;
    return result;
  }

  self_type& advance(difference_type aN)
  {
    if (aN > 0) {
      difference_type step = XPCOM_MIN(aN, mEnd - mPosition);

      NS_ASSERTION(step > 0,
                   "can't advance a reading iterator beyond the end of a string");

      mPosition += step;
    } else if (aN < 0) {
      difference_type step = XPCOM_MAX(aN, -(mPosition - mStart));

      NS_ASSERTION(step < 0,
                   "can't advance (backward) a reading iterator beyond the end of a string");

      mPosition += step;
    }
    return *this;
  }

  // We return an unsigned type here (with corresponding assert) rather than
  // the more usual difference_type because we want to make this class go
  // away in favor of mozilla::RangedPtr.  Since RangedPtr has the same
  // requirement we are enforcing here, the transition ought to be much
  // smoother.
  size_type operator-(const self_type& aOther) const
  {
    MOZ_ASSERT(mPosition >= aOther.mPosition);
    return mPosition - aOther.mPosition;
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
  typedef size_t                     size_type;
  typedef CharT                      value_type;
  typedef CharT*                     pointer;
  typedef CharT&                     reference;

private:
  friend class nsTSubstring<CharT>;

  // unfortunately, the API for nsWritingIterator requires that the
  // iterator know its start and end positions.  this was needed when
  // we supported multi-fragment strings, but now it is really just
  // extra baggage.  we should remove mStart and mEnd at some point.

  CharT* mStart;
  CharT* mEnd;
  CharT* mPosition;

public:
  nsWritingIterator()
    : mStart(nullptr)
    , mEnd(nullptr)
    , mPosition(nullptr)
  {
  }
  // nsWritingIterator( const nsWritingIterator<CharT>& );                    // auto-generated copy-constructor OK
  // nsWritingIterator<CharT>& operator=( const nsWritingIterator<CharT>& );  // auto-generated copy-assignment operator OK

  pointer get() const
  {
    return mPosition;
  }

  reference operator*() const
  {
    return *get();
  }

  self_type& operator++()
  {
    ++mPosition;
    return *this;
  }

  self_type operator++(int)
  {
    self_type result(*this);
    ++mPosition;
    return result;
  }

  self_type& operator--()
  {
    --mPosition;
    return *this;
  }

  self_type operator--(int)
  {
    self_type result(*this);
    --mPosition;
    return result;
  }

  self_type& advance(difference_type aN)
  {
    if (aN > 0) {
      difference_type step = XPCOM_MIN(aN, mEnd - mPosition);

      NS_ASSERTION(step > 0,
                   "can't advance a writing iterator beyond the end of a string");

      mPosition += step;
    } else if (aN < 0) {
      difference_type step = XPCOM_MAX(aN, -(mPosition - mStart));

      NS_ASSERTION(step < 0,
                   "can't advance (backward) a writing iterator beyond the end of a string");

      mPosition += step;
    }
    return *this;
  }

  // We return an unsigned type here (with corresponding assert) rather than
  // the more usual difference_type because we want to make this class go
  // away in favor of mozilla::RangedPtr.  Since RangedPtr has the same
  // requirement we are enforcing here, the transition ought to be much
  // smoother.
  size_type operator-(const self_type& aOther) const
  {
    MOZ_ASSERT(mPosition >= aOther.mPosition);
    return mPosition - aOther.mPosition;
  }
};

template <class CharT>
struct nsCharSinkTraits<nsWritingIterator<CharT>>
{
  static void
  write(nsWritingIterator<CharT>& aIter, const CharT* aStr, uint32_t aN)
  {
    nsCharTraits<CharT>::move(aIter.get(), aStr, aN);
    aIter.advance(aN);
  }
};

template <class CharT>
inline bool
operator==(const nsReadingIterator<CharT>& aLhs,
           const nsReadingIterator<CharT>& aRhs)
{
  return aLhs.get() == aRhs.get();
}

template <class CharT>
inline bool
operator!=(const nsReadingIterator<CharT>& aLhs,
           const nsReadingIterator<CharT>& aRhs)
{
  return aLhs.get() != aRhs.get();
}


//
// |nsWritingIterator|s
//

template <class CharT>
inline bool
operator==(const nsWritingIterator<CharT>& aLhs,
           const nsWritingIterator<CharT>& aRhs)
{
  return aLhs.get() == aRhs.get();
}

template <class CharT>
inline bool
operator!=(const nsWritingIterator<CharT>& aLhs,
           const nsWritingIterator<CharT>& aRhs)
{
  return aLhs.get() != aRhs.get();
}

#endif /* !defined(nsStringIterator_h___) */
