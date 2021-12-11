/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Queue_h
#define mozilla_Queue_h

#include <utility>
#include <stdint.h>
#include "mozilla/MemoryReporting.h"
#include "mozilla/Assertions.h"
#include "mozalloc.h"

namespace mozilla {

// define to turn on additional (DEBUG) asserts
// #define EXTRA_ASSERTS 1

// A queue implements a singly linked list of pages, each of which contains some
// number of elements. Since the queue needs to store a "next" pointer, the
// actual number of elements per page won't be quite as many as were requested.
//
// Each page consists of N entries.  We use the head buffer as a circular buffer
// if it's the only buffer; if we have more than one buffer when the head is
// empty we release it.  This avoids occasional freeing and reallocating buffers
// every N entries.  We'll still allocate and free every N if the normal queue
// depth is greated than N.  A fancier solution would be to move an empty Head
// buffer to be an empty tail buffer, freeing if we have multiple empty tails,
// but that probably isn't worth it.
//
// Cases:
//   a) single buffer, circular
//      Push: if not full:
//              Add to tail, bump tail and reset to 0 if at end
//            full:
//              Add new page, insert there and set tail to 1
//      Pop:
//            take entry and bump head, reset to 0 if at end
//   b) multiple buffers:
//      Push: if not full:
//              Add to tail, bump tail
//            full:
//              Add new page, insert there and set tail to 1
//      Pop:
//            take entry and bump head, reset to 0 if at end
//            if buffer is empty, free head buffer and promote next to head
//
template <class T, size_t RequestedItemsPerPage = 256>
class Queue {
 public:
  Queue() = default;

  Queue(Queue&& aOther) noexcept
      : mHead(std::exchange(aOther.mHead, nullptr)),
        mTail(std::exchange(aOther.mTail, nullptr)),
        mOffsetHead(std::exchange(aOther.mOffsetHead, 0)),
        mHeadLength(std::exchange(aOther.mHeadLength, 0)),
        mTailLength(std::exchange(aOther.mTailLength, 0)) {}

  Queue& operator=(Queue&& aOther) noexcept {
    Clear();

    mHead = std::exchange(aOther.mHead, nullptr);
    mTail = std::exchange(aOther.mTail, nullptr);
    mOffsetHead = std::exchange(aOther.mOffsetHead, 0);
    mHeadLength = std::exchange(aOther.mHeadLength, 0);
    mTailLength = std::exchange(aOther.mTailLength, 0);
    return *this;
  }

  ~Queue() { Clear(); }

  // Discard all elements form the queue, clearing it to be empty.
  void Clear() {
    while (!IsEmpty()) {
      Pop();
    }
    if (mHead) {
      free(mHead);
      mHead = nullptr;
    }
  }

  T& Push(T&& aElement) {
#if defined(EXTRA_ASSERTS) && DEBUG
    size_t original_length = Count();
#endif
    if (!mHead) {
      mHead = NewPage();
      MOZ_ASSERT(mHead);

      mTail = mHead;
      T* eltPtr = &mTail->mEvents[0];
      new (eltPtr) T(std::move(aElement));
      mOffsetHead = 0;
      mHeadLength = 1;
#ifdef EXTRA_ASSERTS
      MOZ_ASSERT(Count() == original_length + 1);
#endif
      return *eltPtr;
    }
    if ((mHead == mTail && mHeadLength == ItemsPerPage) ||
        (mHead != mTail && mTailLength == ItemsPerPage)) {
      // either we have one (circular) buffer and it's full, or
      // we have multiple buffers and the last buffer is full
      Page* page = NewPage();
      MOZ_ASSERT(page);

      mTail->mNext = page;
      mTail = page;
      T* eltPtr = &page->mEvents[0];
      new (eltPtr) T(std::move(aElement));
      mTailLength = 1;
#ifdef EXTRA_ASSERTS
      MOZ_ASSERT(Count() == original_length + 1);
#endif
      return *eltPtr;
    }
    if (mHead == mTail) {
      // we have space in the (single) head buffer
      uint16_t offset = (mOffsetHead + mHeadLength++) % ItemsPerPage;
      T* eltPtr = &mTail->mEvents[offset];
      new (eltPtr) T(std::move(aElement));
#ifdef EXTRA_ASSERTS
      MOZ_ASSERT(Count() == original_length + 1);
#endif
      return *eltPtr;
    }
    // else we have space to insert into last buffer
    T* eltPtr = &mTail->mEvents[mTailLength++];
    new (eltPtr) T(std::move(aElement));
#ifdef EXTRA_ASSERTS
    MOZ_ASSERT(Count() == original_length + 1);
#endif
    return *eltPtr;
  }

  bool IsEmpty() const {
    return !mHead || (mHead == mTail && mHeadLength == 0);
  }

  T Pop() {
#if defined(EXTRA_ASSERTS) && DEBUG
    size_t original_length = Count();
#endif
    MOZ_ASSERT(!IsEmpty());

    T result = std::move(mHead->mEvents[mOffsetHead]);
    mHead->mEvents[mOffsetHead].~T();
    mOffsetHead = (mOffsetHead + 1) % ItemsPerPage;
    mHeadLength -= 1;

    // Check if mHead points to empty (circular) Page and we have more
    // pages
    if (mHead != mTail && mHeadLength == 0) {
      Page* dead = mHead;
      mHead = mHead->mNext;
      free(dead);
      mOffsetHead = 0;
      // if there are still >1 pages, the new head is full.
      if (mHead != mTail) {
        mHeadLength = ItemsPerPage;
      } else {
        mHeadLength = mTailLength;
        mTailLength = 0;
      }
    }

#ifdef EXTRA_ASSERTS
    MOZ_ASSERT(Count() == original_length - 1);
#endif
    return result;
  }

  T& FirstElement() {
    MOZ_ASSERT(!IsEmpty());
    return mHead->mEvents[mOffsetHead];
  }

  const T& FirstElement() const {
    MOZ_ASSERT(!IsEmpty());
    return mHead->mEvents[mOffsetHead];
  }

  T& LastElement() {
    MOZ_ASSERT(!IsEmpty());
    uint16_t offset =
        mHead == mTail ? mOffsetHead + mHeadLength - 1 : mTailLength - 1;
    return mTail->mEvents[offset];
  }

  const T& LastElement() const {
    MOZ_ASSERT(!IsEmpty());
    uint16_t offset =
        mHead == mTail ? mOffsetHead + mHeadLength - 1 : mTailLength - 1;
    return mTail->mEvents[offset];
  }

  size_t Count() const {
    // It is obvious count is 0 when the queue is empty.
    if (!mHead) {
      return 0;
    }

    // Compute full (intermediate) pages; Doesn't count first or last page
    int count = 0;
    // 1 buffer will have mHead == mTail; 2 will have mHead->mNext == mTail
    for (Page* page = mHead; page != mTail && page->mNext != mTail;
         page = page->mNext) {
      count += ItemsPerPage;
    }
    // add first and last page
    count += mHeadLength + mTailLength;
    MOZ_ASSERT(count >= 0);

    return count;
  }

  size_t ShallowSizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t n = 0;
    if (mHead) {
      for (Page* page = mHead; page != mTail; page = page->mNext) {
        n += aMallocSizeOf(page);
      }
    }
    return n;
  }

  size_t ShallowSizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  static_assert(
      (RequestedItemsPerPage & (RequestedItemsPerPage - 1)) == 0,
      "RequestedItemsPerPage should be a power of two to avoid heap slop.");

  // Since a Page must also contain a "next" pointer, we use one of the items to
  // store this pointer. If sizeof(T) > sizeof(Page*), then some space will be
  // wasted. So be it.
  static const size_t ItemsPerPage = RequestedItemsPerPage - 1;

  // Page objects are linked together to form a simple deque.
  struct Page {
    struct Page* mNext;
    T mEvents[ItemsPerPage];
  };

  static Page* NewPage() {
    return static_cast<Page*>(moz_xcalloc(1, sizeof(Page)));
  }

  Page* mHead = nullptr;
  Page* mTail = nullptr;

  uint16_t mOffsetHead = 0;  // Read position in head page
  uint16_t mHeadLength = 0;  // Number of items in the head page
  uint16_t mTailLength = 0;  // Number of items in the tail page
};

}  // namespace mozilla

#endif  // mozilla_Queue_h
