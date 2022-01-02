/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeque.h"
#include "nsISupportsImpl.h"
#include <string.h>

#include "mozilla/CheckedInt.h"

#define modulus(x, y) ((x) % (y))

namespace mozilla {
namespace detail {

/**
 * Standard constructor
 * @param deallocator, called by Erase and ~nsDequeBase
 */
nsDequeBase::nsDequeBase() {
  MOZ_COUNT_CTOR(nsDequeBase);
  mOrigin = mSize = 0;
  mData = mBuffer;  // don't allocate space until you must
  mCapacity = sizeof(mBuffer) / sizeof(mBuffer[0]);
  memset(mData, 0, mCapacity * sizeof(mBuffer[0]));
}

/**
 * Destructor
 */
nsDequeBase::~nsDequeBase() {
  MOZ_COUNT_DTOR(nsDequeBase);

  if (mData && mData != mBuffer) {
    free(mData);
  }
  mData = nullptr;
}

size_t nsDequeBase::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t size = 0;
  if (mData != mBuffer) {
    size += aMallocSizeOf(mData);
  }

  return size;
}

/**
 * Remove all items from container without destroying them.
 */
void nsDequeBase::Empty() {
  if (mSize && mData) {
    memset(mData, 0, mCapacity * sizeof(*mData));
  }
  mSize = 0;
  mOrigin = 0;
}

/**
 * This method quadruples the size of the deque
 * Elements in the deque are resequenced so that elements
 * in the deque are stored sequentially
 *
 * @return  whether growing succeeded
 */
bool nsDequeBase::GrowCapacity() {
  mozilla::CheckedInt<size_t> newCapacity = mCapacity;
  newCapacity *= 4;

  NS_ASSERTION(newCapacity.isValid(), "Overflow");
  if (!newCapacity.isValid()) {
    return false;
  }

  // Sanity check the new byte size.
  mozilla::CheckedInt<size_t> newByteSize = newCapacity;
  newByteSize *= sizeof(void*);

  NS_ASSERTION(newByteSize.isValid(), "Overflow");
  if (!newByteSize.isValid()) {
    return false;
  }

  void** temp = (void**)malloc(newByteSize.value());
  if (!temp) {
    return false;
  }

  // Here's the interesting part: You can't just move the elements
  // directly (in situ) from the old buffer to the new one.
  // Since capacity has changed, the old origin doesn't make
  // sense anymore. It's better to resequence the elements now.

  memcpy(temp, mData + mOrigin, sizeof(void*) * (mCapacity - mOrigin));
  memcpy(temp + (mCapacity - mOrigin), mData, sizeof(void*) * mOrigin);

  if (mData != mBuffer) {
    free(mData);
  }

  mCapacity = newCapacity.value();
  mOrigin = 0;  // now realign the origin...
  mData = temp;

  return true;
}

/**
 * This method adds an item to the end of the deque.
 * This operation has the potential to cause the
 * underlying buffer to resize.
 *
 * @param   aItem: new item to be added to deque
 */
bool nsDequeBase::Push(void* aItem, const fallible_t&) {
  if (mSize == mCapacity && !GrowCapacity()) {
    return false;
  }
  mData[modulus(mOrigin + mSize, mCapacity)] = aItem;
  mSize++;
  return true;
}

/**
 * This method adds an item to the front of the deque.
 * This operation has the potential to cause the
 * underlying buffer to resize.
 *
 * --Commments for GrowCapacity() case
 * We've grown and shifted which means that the old
 * final element in the deque is now the first element
 * in the deque.  This is temporary.
 * We haven't inserted the new element at the front.
 *
 * To continue with the idea of having the front at zero
 * after a grow, we move the old final item (which through
 * the voodoo of mOrigin-- is now the first) to its final
 * position which is conveniently the old length.
 *
 * Note that this case only happens when the deque is full.
 * [And that pieces of this magic only work if the deque is full.]
 * picture:
 *   [ABCDEFGH] @[mOrigin:3]:D.
 * Task: PushFront("Z")
 * shift mOrigin so, @[mOrigin:2]:C
 * stretch and rearrange: (mOrigin:0)
 *   [CDEFGHAB ________ ________ ________]
 * copy: (The second C is currently out of bounds)
 *   [CDEFGHAB C_______ ________ ________]
 * later we will insert Z:
 *   [ZDEFGHAB C_______ ________ ________]
 * and increment size: 9. (C is no longer out of bounds)
 * --
 * @param   aItem: new item to be added to deque
 */
bool nsDequeBase::PushFront(void* aItem, const fallible_t&) {
  if (mOrigin == 0) {
    mOrigin = mCapacity - 1;
  } else {
    mOrigin--;
  }

  if (mSize == mCapacity) {
    if (!GrowCapacity()) {
      return false;
    }
    /* Comments explaining this are above*/
    mData[mSize] = mData[mOrigin];
  }
  mData[mOrigin] = aItem;
  mSize++;
  return true;
}

/**
 * Remove and return the last item in the container.
 *
 * @return  ptr to last item in container
 */
void* nsDequeBase::Pop() {
  void* result = nullptr;
  if (mSize > 0) {
    --mSize;
    size_t offset = modulus(mSize + mOrigin, mCapacity);
    result = mData[offset];
    mData[offset] = nullptr;
    if (!mSize) {
      mOrigin = 0;
    }
  }
  return result;
}

/**
 * This method gets called you want to remove and return
 * the first member in the container.
 *
 * @return  last item in container
 */
void* nsDequeBase::PopFront() {
  void* result = nullptr;
  if (mSize > 0) {
    NS_ASSERTION(mOrigin < mCapacity, "Error: Bad origin");
    result = mData[mOrigin];
    mData[mOrigin++] = nullptr;  // zero it out for debugging purposes.
    mSize--;
    // Cycle around if we pop off the end
    // and reset origin if when we pop the last element
    if (mCapacity == mOrigin || !mSize) {
      mOrigin = 0;
    }
  }
  return result;
}

/**
 * This method gets called you want to peek at the bottom
 * member without removing it.
 *
 * @return  last item in container
 */
void* nsDequeBase::Peek() const {
  void* result = nullptr;
  if (mSize > 0) {
    result = mData[modulus(mSize - 1 + mOrigin, mCapacity)];
  }
  return result;
}

/**
 * This method gets called you want to peek at the topmost
 * member without removing it.
 *
 * @return  last item in container
 */
void* nsDequeBase::PeekFront() const {
  void* result = nullptr;
  if (mSize > 0) {
    result = mData[mOrigin];
  }
  return result;
}

/**
 * Call this to retrieve the ith element from this container.
 * Keep in mind that accessing the underlying elements is
 * done in a relative fashion. Object 0 is not necessarily
 * the first element (the first element is at mOrigin).
 *
 * @param   aIndex : 0 relative offset of item you want
 * @return  void* or null
 */
void* nsDequeBase::ObjectAt(size_t aIndex) const {
  void* result = nullptr;
  if (aIndex < mSize) {
    result = mData[modulus(mOrigin + aIndex, mCapacity)];
  }
  return result;
}
}  // namespace detail
}  // namespace mozilla
