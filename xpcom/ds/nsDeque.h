/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * MODULE NOTES:
 *
 * The Deque is a very small, very efficient container object
 * than can hold elements of type void*, offering the following features:
 *    Its interface supports pushing and popping of elements.
 *    It can iterate (via an interator class) its elements.
 *    When full, it can efficiently resize dynamically.
 *
 *
 * NOTE: The only bit of trickery here is that this deque is
 * built upon a ring-buffer. Like all ring buffers, the first
 * element may not be at index[0]. The mOrigin member determines
 * where the first child is. This point is quietly hidden from
 * customers of this class.
 *
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
 * Use these objects in a call to ForEach();
 *
 */

class nsDequeFunctor
{
public:
  virtual void* operator()(void* aObject) = 0;
  virtual ~nsDequeFunctor() {}
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

class nsDequeIterator;

class nsDeque
{
  typedef mozilla::fallible_t fallible_t;
public:
  explicit nsDeque(nsDequeFunctor* aDeallocator = nullptr);
  ~nsDeque();

  /**
   * Returns the number of elements currently stored in
   * this deque.
   *
   * @return  number of elements currently in the deque
   */
  inline size_t GetSize() const { return mSize; }

  /**
   * Appends new member at the end of the deque.
   *
   * @param   item to store in deque
   */
  void Push(void* aItem)
  {
    if (!Push(aItem, mozilla::fallible)) {
      NS_ABORT_OOM(mSize * sizeof(void*));
    }
  }

  MOZ_MUST_USE bool Push(void* aItem, const fallible_t&);

  /**
   * Inserts new member at the front of the deque.
   *
   * @param   item to store in deque
   */
  void PushFront(void* aItem)
  {
    if (!PushFront(aItem, mozilla::fallible)) {
      NS_ABORT_OOM(mSize * sizeof(void*));
    }
  }

  MOZ_MUST_USE bool PushFront(void* aItem, const fallible_t&);

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
   * Retrieve the bottom item without removing it.
   *
   * @return  the first item in container
   */

  void* Peek() const;
  /**
   * Return topmost item without removing it.
   *
   * @return  the first item in container
   */
  void* PeekFront() const;

  /**
   * Retrieve a member from the deque without removing it.
   *
   * @param   index of desired item
   * @return  element in list
   */
  void* ObjectAt(size_t aIndex) const;

  /**
   * Remove and delete all items from container.
   * Deletes are handled by the deallocator nsDequeFunctor
   * which is specified at deque construction.
   */
  void Erase();

  /**
   * Call this method when you want to iterate all the
   * members of the container, passing a functor along
   * to call your code.
   *
   * @param   aFunctor object to call for each member
   */
  void ForEach(nsDequeFunctor& aFunctor) const;

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

protected:
  size_t         mSize;
  size_t         mCapacity;
  size_t         mOrigin;
  nsDequeFunctor* mDeallocator;
  void*           mBuffer[8];
  void**          mData;

private:

  /**
   * Copy constructor (PRIVATE)
   *
   * @param aOther another deque
   */
  nsDeque(const nsDeque& aOther);

  /**
   * Deque assignment operator (PRIVATE)
   *
   * @param aOther another deque
   * @return *this
   */
  nsDeque& operator=(const nsDeque& aOther);

  bool GrowCapacity();
  void SetDeallocator(nsDequeFunctor* aDeallocator);

  /**
   * Remove all items from container without destroying them.
   */
  void Empty();
};
#endif
