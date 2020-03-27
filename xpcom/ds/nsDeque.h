/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * MODULE NOTES:
 *
 * The Deque is a very small, very efficient container object
 * than can hold items of type void*, offering the following features:
 * - Its interface supports pushing, popping, and peeking of items at the back
 *   or front, and retrieval from any position.
 * - It can iterate over items via a ForEach method, range-for, or an iterator
 *   class.
 * - When full, it can efficiently resize dynamically.
 *
 * NOTE: The only bit of trickery here is that this deque is
 * built upon a ring-buffer. Like all ring buffers, the first
 * item may not be at index[0]. The mOrigin member determines
 * where the first child is. This point is quietly hidden from
 * customers of this class.
 */

#ifndef _NSDEQUE
#define _NSDEQUE

#include "nscore.h"
#include "nsDebug.h"
#include "mozilla/Attributes.h"
#include "mozilla/fallible.h"
#include "mozilla/MemoryReporting.h"

/**
 * The nsDequeFunctor class is used when you want to create
 * callbacks between the deque and your generic code.
 * Use these objects in a call to ForEach(), and as custom deallocators.
 */
class nsDequeFunctor {
 public:
  virtual void operator()(void* aObject) = 0;
  virtual ~nsDequeFunctor() = default;
};

/******************************************************
 * Here comes the nsDeque class itself...
 ******************************************************/

/**
 * The deque (double-ended queue) class is a common container type,
 * whose behavior mimics a line in your favorite checkout stand.
 * Classic CS describes the common behavior of a queue as FIFO.
 * A deque allows insertion and removal at both ends of
 * the container.
 *
 * The deque stores pointers to items.
 */
class nsDeque {
  typedef mozilla::fallible_t fallible_t;

 public:
  /**
   * Constructs an empty deque.
   *
   * @param   aDeallocator Optional deallocator functor that will be called from
   *                       Erase() and the destructor on any remaining item.
   *                       The deallocator is owned by the deque and will be
   *                       deleted at destruction time.
   */
  explicit nsDeque(nsDequeFunctor* aDeallocator = nullptr);

  /**
   * Deque destructor. Erases all items, deletes the deallocator.
   */
  ~nsDeque();

  /**
   * Returns the number of items currently stored in
   * this deque.
   *
   * @return  number of items currently in the deque
   */
  inline size_t GetSize() const { return mSize; }

  /**
   * Appends new member at the end of the deque.
   *
   * @param   aItem item to store in deque
   */
  void Push(void* aItem) {
    if (!Push(aItem, mozilla::fallible)) {
      NS_ABORT_OOM(mSize * sizeof(void*));
    }
  }

  /**
   * Appends new member at the end of the deque.
   *
   * @param   aItem item to store in deque
   * @return  true if succeeded, false if failed to resize deque as needed
   */
  [[nodiscard]] bool Push(void* aItem, const fallible_t&);

  /**
   * Inserts new member at the front of the deque.
   *
   * @param   aItem item to store in deque
   */
  void PushFront(void* aItem) {
    if (!PushFront(aItem, mozilla::fallible)) {
      NS_ABORT_OOM(mSize * sizeof(void*));
    }
  }

  /**
   * Inserts new member at the front of the deque.
   *
   * @param   aItem item to store in deque
   * @return  true if succeeded, false if failed to resize deque as needed
   */
  [[nodiscard]] bool PushFront(void* aItem, const fallible_t&);

  /**
   * Remove and return the last item in the container.
   *
   * @return  the item that was the last item in container
   */
  void* Pop();

  /**
   * Remove and return the first item in the container.
   *
   * @return  the item that was first item in container
   */
  void* PopFront();

  /**
   * Retrieve the last item without removing it.
   *
   * @return  the last item in container
   */
  void* Peek() const;

  /**
   * Retrieve the first item without removing it.
   *
   * @return  the first item in container
   */
  void* PeekFront() const;

  /**
   * Retrieve a member from the deque without removing it.
   *
   * @param   index of desired item
   * @return  item in list, or nullptr if index is outside the deque
   */
  void* ObjectAt(size_t aIndex) const;

  /**
   * Remove and delete all items from container.
   * Deletes are handled by the deallocator nsDequeFunctor
   * which is specified at deque construction.
   */
  void Erase();

  /**
   * Call this method when you want to iterate through all
   * items in the container, passing a functor along
   * to call your code.
   * If the deque is modified during ForEach, iteration will continue based on
   * item indices; meaning that front operations may effectively skip over
   * items or visit some items multiple times.
   *
   * @param   aFunctor object to call for each member
   */
  void ForEach(nsDequeFunctor& aFunctor) const;

  // This iterator assumes that the deque itself is const, i.e., it cannot be
  // modified while the iterator is used.
  // Also it is a 'const' iterator in that it provides copies of the deque's
  // elements, and therefore it is not possible to modify the deque's contents
  // by assigning to a dereference of this iterator.
  class ConstDequeIterator {
   public:
    ConstDequeIterator(const nsDeque& aDeque, size_t aIndex)
        : mDeque(aDeque), mIndex(aIndex) {}
    ConstDequeIterator& operator++() {
      ++mIndex;
      return *this;
    }
    bool operator==(const ConstDequeIterator& aOther) const {
      return mIndex == aOther.mIndex;
    }
    bool operator!=(const ConstDequeIterator& aOther) const {
      return mIndex != aOther.mIndex;
    }
    void* operator*() const {
      // Don't allow out-of-deque dereferences.
      MOZ_RELEASE_ASSERT(mIndex < mDeque.GetSize());
      return mDeque.ObjectAt(mIndex);
    }

   private:
    const nsDeque& mDeque;
    size_t mIndex;
  };
  // If this deque is const, we can provide ConstDequeIterator's.
  ConstDequeIterator begin() const { return ConstDequeIterator(*this, 0); }
  ConstDequeIterator end() const { return ConstDequeIterator(*this, mSize); }

  // It is a 'const' iterator in that it provides copies of the deque's
  // elements, and therefore it is not possible to modify the deque's contents
  // by assigning to a dereference of this iterator.
  // If the deque is modified in other ways, this iterator will stay at the same
  // index, and will handle past-the-end comparisons, but not dereferencing.
  class ConstIterator {
   public:
    // Special index for the end iterator, to track the possibly-shifting
    // deque size.
    static const size_t EndIteratorIndex = size_t(-1);

    ConstIterator(const nsDeque& aDeque, size_t aIndex)
        : mDeque(aDeque), mIndex(aIndex) {}
    ConstIterator& operator++() {
      // End-iterator shouldn't be modified.
      MOZ_ASSERT(mIndex != EndIteratorIndex);
      ++mIndex;
      return *this;
    }
    bool operator==(const ConstIterator& aOther) const {
      return EffectiveIndex() == aOther.EffectiveIndex();
    }
    bool operator!=(const ConstIterator& aOther) const {
      return EffectiveIndex() != aOther.EffectiveIndex();
    }
    void* operator*() const {
      // Don't allow out-of-deque dereferences.
      MOZ_RELEASE_ASSERT(mIndex < mDeque.GetSize());
      return mDeque.ObjectAt(mIndex);
    }

   private:
    // 0 <= index < deque.GetSize() inside the deque, deque.GetSize() otherwise.
    // Only used when comparing indices, not to actually access items.
    size_t EffectiveIndex() const {
      return (mIndex < mDeque.GetSize()) ? mIndex : mDeque.GetSize();
    }

    const nsDeque& mDeque;
    size_t mIndex;  // May point outside the deque!
  };
  // If this deque is *not* const, we provide ConstIterator's that can handle
  // deque size changes.
  ConstIterator begin() { return ConstIterator(*this, 0); }
  ConstIterator end() {
    return ConstIterator(*this, ConstIterator::EndIteratorIndex);
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 protected:
  size_t mSize;
  size_t mCapacity;
  size_t mOrigin;
  nsDequeFunctor* mDeallocator;
  void* mBuffer[8];
  void** mData;

 private:
  /**
   * Copy constructor (deleted)
   *
   * @param aOther another deque
   */
  nsDeque(const nsDeque& aOther) = delete;

  /**
   * Deque assignment operator (deleted)
   *
   * @param aOther another deque
   * @return *this
   */
  nsDeque& operator=(const nsDeque& aOther) = delete;

  bool GrowCapacity();
  void SetDeallocator(nsDequeFunctor* aDeallocator);

  /**
   * Remove all items from container without destroying them.
   */
  void Empty();
};
#endif
