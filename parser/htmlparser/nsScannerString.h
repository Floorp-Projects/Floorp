/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScannerString_h___
#define nsScannerString_h___

#include "nsString.h"
#include "nsUnicharUtils.h"  // for nsCaseInsensitiveStringComparator
#include "mozilla/LinkedList.h"
#include <algorithm>

/**
 * NOTE: nsScannerString (and the other classes defined in this file) are
 * not related to nsAString or any of the other xpcom/string classes.
 *
 * nsScannerString is based on the nsSlidingString implementation that used
 * to live in xpcom/string.  Now that nsAString is limited to representing
 * only single fragment strings, nsSlidingString can no longer be used.
 *
 * An advantage to this design is that it does not employ any virtual
 * functions.
 *
 * This file uses SCC-style indenting in deference to the nsSlidingString
 * code from which this code is derived ;-)
 */

class nsScannerIterator;
class nsScannerSubstring;
class nsScannerString;

/**
 * nsScannerBufferList
 *
 * This class maintains a list of heap-allocated Buffer objects.  The buffers
 * are maintained in a circular linked list.  Each buffer has a usage count
 * that is decremented by the owning nsScannerSubstring.
 *
 * The buffer list itself is reference counted.  This allows the buffer list
 * to be shared by multiple nsScannerSubstring objects.  The reference
 * counting is not threadsafe, which is not at all a requirement.
 *
 * When a nsScannerSubstring releases its reference to a buffer list, it
 * decrements the usage count of the first buffer in the buffer list that it
 * was referencing.  It informs the buffer list that it can discard buffers
 * starting at that prefix.  The buffer list will do so if the usage count of
 * that buffer is 0 and if it is the first buffer in the list.  It will
 * continue to prune buffers starting from the front of the buffer list until
 * it finds a buffer that has a usage count that is non-zero.
 */
class nsScannerBufferList {
 public:
  /**
   * Buffer objects are directly followed by a data segment.  The start
   * of the data segment is determined by increment the |this| pointer
   * by 1 unit.
   */
  class Buffer : public mozilla::LinkedListElement<Buffer> {
   public:
    void IncrementUsageCount() { ++mUsageCount; }
    void DecrementUsageCount() { --mUsageCount; }

    bool IsInUse() const { return mUsageCount != 0; }

    const char16_t* DataStart() const { return (const char16_t*)(this + 1); }
    char16_t* DataStart() { return (char16_t*)(this + 1); }

    const char16_t* DataEnd() const { return mDataEnd; }
    char16_t* DataEnd() { return mDataEnd; }

    const Buffer* Next() const { return getNext(); }
    Buffer* Next() { return getNext(); }

    const Buffer* Prev() const { return getPrevious(); }
    Buffer* Prev() { return getPrevious(); }

    uint32_t DataLength() const { return mDataEnd - DataStart(); }
    void SetDataLength(uint32_t len) { mDataEnd = DataStart() + len; }

   private:
    friend class nsScannerBufferList;

    int32_t mUsageCount;
    char16_t* mDataEnd;
  };

  /**
   * Position objects serve as lightweight pointers into a buffer list.
   * The mPosition member must be contained with mBuffer->DataStart()
   * and mBuffer->DataEnd().
   */
  class Position {
   public:
    Position() : mBuffer(nullptr), mPosition(nullptr) {}

    Position(Buffer* buffer, char16_t* position)
        : mBuffer(buffer), mPosition(position) {}

    inline explicit Position(const nsScannerIterator& aIter);

    inline Position& operator=(const nsScannerIterator& aIter);

    static size_t Distance(const Position& p1, const Position& p2);

    Buffer* mBuffer;
    char16_t* mPosition;
  };

  static Buffer* AllocBufferFromString(const nsAString&);
  static Buffer* AllocBuffer(uint32_t capacity);  // capacity = number of chars

  explicit nsScannerBufferList(Buffer* buf) : mRefCnt(0) {
    mBuffers.insertBack(buf);
  }

  void AddRef() { ++mRefCnt; }
  void Release() {
    if (--mRefCnt == 0) delete this;
  }

  void Append(Buffer* buf) { mBuffers.insertBack(buf); }
  void InsertAfter(Buffer* buf, Buffer* prev) { prev->setNext(buf); }
  void SplitBuffer(const Position&);
  void DiscardUnreferencedPrefix(Buffer*);

  Buffer* Head() { return mBuffers.getFirst(); }
  const Buffer* Head() const { return mBuffers.getFirst(); }

  Buffer* Tail() { return mBuffers.getLast(); }
  const Buffer* Tail() const { return mBuffers.getLast(); }

 private:
  friend class nsScannerSubstring;

  ~nsScannerBufferList() { ReleaseAll(); }
  void ReleaseAll();

  int32_t mRefCnt;
  mozilla::LinkedList<Buffer> mBuffers;
};

/**
 * nsScannerFragment represents a "slice" of a Buffer object.
 */
struct nsScannerFragment {
  typedef nsScannerBufferList::Buffer Buffer;

  const Buffer* mBuffer;
  const char16_t* mFragmentStart;
  const char16_t* mFragmentEnd;
};

/**
 * nsScannerSubstring is the base class for nsScannerString.  It provides
 * access to iterators and methods to bind the substring to another
 * substring or nsAString instance.
 *
 * This class owns the buffer list.
 */
class nsScannerSubstring {
 public:
  typedef nsScannerBufferList::Buffer Buffer;
  typedef nsScannerBufferList::Position Position;
  typedef uint32_t size_type;

  nsScannerSubstring();
  explicit nsScannerSubstring(const nsAString& s);

  ~nsScannerSubstring();

  nsScannerIterator& BeginReading(nsScannerIterator& iter) const;
  nsScannerIterator& EndReading(nsScannerIterator& iter) const;

  size_type Length() const { return mLength; }

  int32_t CountChar(char16_t) const;

  void Rebind(const nsScannerSubstring&, const nsScannerIterator&,
              const nsScannerIterator&);
  void Rebind(const nsAString&);

  const nsAString& AsString() const;

  bool GetNextFragment(nsScannerFragment&) const;
  bool GetPrevFragment(nsScannerFragment&) const;

  static inline Buffer* AllocBufferFromString(const nsAString& aStr) {
    return nsScannerBufferList::AllocBufferFromString(aStr);
  }
  static inline Buffer* AllocBuffer(size_type aCapacity) {
    return nsScannerBufferList::AllocBuffer(aCapacity);
  }

 protected:
  void acquire_ownership_of_buffer_list() const {
    mBufferList->AddRef();
    mStart.mBuffer->IncrementUsageCount();
  }

  void release_ownership_of_buffer_list() {
    if (mBufferList) {
      mStart.mBuffer->DecrementUsageCount();
      mBufferList->DiscardUnreferencedPrefix(mStart.mBuffer);
      mBufferList->Release();
    }
  }

  void init_range_from_buffer_list() {
    mStart.mBuffer = mBufferList->Head();
    mStart.mPosition = mStart.mBuffer->DataStart();

    mEnd.mBuffer = mBufferList->Tail();
    mEnd.mPosition = mEnd.mBuffer->DataEnd();

    mLength = Position::Distance(mStart, mEnd);
  }

  Position mStart;
  Position mEnd;
  nsScannerBufferList* mBufferList;
  size_type mLength;

  // these fields are used to implement AsString
  nsDependentSubstring mFlattenedRep;
  bool mIsDirty;

  friend class nsScannerSharedSubstring;
};

/**
 * nsScannerString provides methods to grow and modify a buffer list.
 */
class nsScannerString : public nsScannerSubstring {
 public:
  explicit nsScannerString(Buffer*);

  // you are giving ownership to the string, it takes and keeps your
  // buffer, deleting it when done.
  // Use AllocBuffer or AllocBufferFromString to create a Buffer object
  // for use with this function.
  void AppendBuffer(Buffer*);

  void DiscardPrefix(const nsScannerIterator&);
  // any other way you want to do this?

  void UngetReadable(const nsAString& aReadable,
                     const nsScannerIterator& aCurrentPosition);
};

/**
 * nsScannerSharedSubstring implements copy-on-write semantics for
 * nsScannerSubstring.  When you call .writable(), it will copy the data
 * and return a mutable string object.  This class also manages releasing
 * the reference to the scanner buffer when it is no longer needed.
 */

class nsScannerSharedSubstring {
 public:
  nsScannerSharedSubstring() : mBuffer(nullptr), mBufferList(nullptr) {}

  ~nsScannerSharedSubstring() {
    if (mBufferList) ReleaseBuffer();
  }

  // Acquire a copy-on-write reference to the given substring.
  void Rebind(const nsScannerIterator& aStart, const nsScannerIterator& aEnd);

  // Get a mutable reference to this string
  nsAString& writable() {
    if (mBufferList) MakeMutable();

    return mString;
  }

  // Get a const reference to this string
  const nsAString& str() const { return mString; }

 private:
  typedef nsScannerBufferList::Buffer Buffer;

  void ReleaseBuffer();
  void MakeMutable();

  nsDependentSubstring mString;
  Buffer* mBuffer;
  nsScannerBufferList* mBufferList;
};

/**
 * nsScannerIterator works just like nsReadingIterator<CharT> except that
 * it knows how to iterate over a list of scanner buffers.
 */
class nsScannerIterator {
 public:
  typedef nsScannerIterator self_type;
  typedef ptrdiff_t difference_type;
  typedef char16_t value_type;
  typedef const char16_t* pointer;
  typedef const char16_t& reference;
  typedef nsScannerSubstring::Buffer Buffer;

 protected:
  nsScannerFragment mFragment;
  const char16_t* mPosition;
  const nsScannerSubstring* mOwner;

  friend class nsScannerSubstring;
  friend class nsScannerSharedSubstring;

 public:
  // nsScannerIterator();                                       // auto-generate
  // default constructor is OK nsScannerIterator( const nsScannerIterator& ); //
  // auto-generated copy-constructor OK nsScannerIterator& operator=( const
  // nsScannerIterator& );  // auto-generated copy-assignment operator OK

  inline void normalize_forward();
  inline void normalize_backward();

  pointer get() const { return mPosition; }

  char16_t operator*() const { return *get(); }

  const nsScannerFragment& fragment() const { return mFragment; }

  const Buffer* buffer() const { return mFragment.mBuffer; }

  self_type& operator++() {
    ++mPosition;
    normalize_forward();
    return *this;
  }

  self_type operator++(int) {
    self_type result(*this);
    ++mPosition;
    normalize_forward();
    return result;
  }

  self_type& operator--() {
    normalize_backward();
    --mPosition;
    return *this;
  }

  self_type operator--(int) {
    self_type result(*this);
    normalize_backward();
    --mPosition;
    return result;
  }

  difference_type size_forward() const {
    return mFragment.mFragmentEnd - mPosition;
  }

  difference_type size_backward() const {
    return mPosition - mFragment.mFragmentStart;
  }

  self_type& advance(difference_type n) {
    while (n > 0) {
      difference_type one_hop = std::min(n, size_forward());

      NS_ASSERTION(one_hop > 0,
                   "Infinite loop: can't advance a reading iterator beyond the "
                   "end of a string");
      // perhaps I should |break| if |!one_hop|?

      mPosition += one_hop;
      normalize_forward();
      n -= one_hop;
    }

    while (n < 0) {
      normalize_backward();
      difference_type one_hop = std::max(n, -size_backward());

      NS_ASSERTION(one_hop < 0,
                   "Infinite loop: can't advance (backward) a reading iterator "
                   "beyond the end of a string");
      // perhaps I should |break| if |!one_hop|?

      mPosition += one_hop;
      n -= one_hop;
    }

    return *this;
  }
};

inline bool SameFragment(const nsScannerIterator& a,
                         const nsScannerIterator& b) {
  return a.fragment().mFragmentStart == b.fragment().mFragmentStart;
}

/**
 * this class is needed in order to make use of the methods in nsAlgorithm.h
 */
template <>
struct nsCharSourceTraits<nsScannerIterator> {
  typedef nsScannerIterator::difference_type difference_type;

  static uint32_t readable_distance(const nsScannerIterator& first,
                                    const nsScannerIterator& last) {
    return uint32_t(SameFragment(first, last) ? last.get() - first.get()
                                              : first.size_forward());
  }

  static const nsScannerIterator::value_type* read(
      const nsScannerIterator& iter) {
    return iter.get();
  }

  static void advance(nsScannerIterator& s, difference_type n) { s.advance(n); }
};

/**
 * inline methods follow
 */

inline void nsScannerIterator::normalize_forward() {
  while (mPosition == mFragment.mFragmentEnd &&
         mOwner->GetNextFragment(mFragment))
    mPosition = mFragment.mFragmentStart;
}

inline void nsScannerIterator::normalize_backward() {
  while (mPosition == mFragment.mFragmentStart &&
         mOwner->GetPrevFragment(mFragment))
    mPosition = mFragment.mFragmentEnd;
}

inline bool operator==(const nsScannerIterator& lhs,
                       const nsScannerIterator& rhs) {
  return lhs.get() == rhs.get();
}

inline bool operator!=(const nsScannerIterator& lhs,
                       const nsScannerIterator& rhs) {
  return lhs.get() != rhs.get();
}

inline nsScannerBufferList::Position::Position(const nsScannerIterator& aIter)
    : mBuffer(const_cast<Buffer*>(aIter.buffer())),
      mPosition(const_cast<char16_t*>(aIter.get())) {}

inline nsScannerBufferList::Position& nsScannerBufferList::Position::operator=(
    const nsScannerIterator& aIter) {
  mBuffer = const_cast<Buffer*>(aIter.buffer());
  mPosition = const_cast<char16_t*>(aIter.get());
  return *this;
}

/**
 * scanner string utils
 *
 * These methods mimic the API provided by nsReadableUtils in xpcom/string.
 * Here we provide only the methods that the htmlparser module needs.
 */

inline size_t Distance(const nsScannerIterator& aStart,
                       const nsScannerIterator& aEnd) {
  typedef nsScannerBufferList::Position Position;
  return Position::Distance(Position(aStart), Position(aEnd));
}

bool CopyUnicodeTo(const nsScannerIterator& aSrcStart,
                   const nsScannerIterator& aSrcEnd, nsAString& aDest);

inline bool CopyUnicodeTo(const nsScannerSubstring& aSrc, nsAString& aDest) {
  nsScannerIterator begin, end;
  return CopyUnicodeTo(aSrc.BeginReading(begin), aSrc.EndReading(end), aDest);
}

bool AppendUnicodeTo(const nsScannerIterator& aSrcStart,
                     const nsScannerIterator& aSrcEnd, nsAString& aDest);

inline bool AppendUnicodeTo(const nsScannerSubstring& aSrc, nsAString& aDest) {
  nsScannerIterator begin, end;
  return AppendUnicodeTo(aSrc.BeginReading(begin), aSrc.EndReading(end), aDest);
}

bool AppendUnicodeTo(const nsScannerIterator& aSrcStart,
                     const nsScannerIterator& aSrcEnd,
                     nsScannerSharedSubstring& aDest);

bool FindCharInReadable(char16_t aChar, nsScannerIterator& aStart,
                        const nsScannerIterator& aEnd);

bool FindInReadable(const nsAString& aPattern, nsScannerIterator& aStart,
                    nsScannerIterator& aEnd,
                    nsStringComparator = nsTDefaultStringComparator);

bool RFindInReadable(const nsAString& aPattern, nsScannerIterator& aStart,
                     nsScannerIterator& aEnd,
                     nsStringComparator = nsTDefaultStringComparator);

inline bool CaseInsensitiveFindInReadable(const nsAString& aPattern,
                                          nsScannerIterator& aStart,
                                          nsScannerIterator& aEnd) {
  return FindInReadable(aPattern, aStart, aEnd,
                        nsCaseInsensitiveStringComparator);
}

#endif  // !defined(nsScannerString_h___)
