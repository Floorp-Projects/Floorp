/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Queue_h
#define mozilla_Queue_h

namespace mozilla {

// A queue implements a singly linked list of pages, each of which contains some
// number of elements. Since the queue needs to store a "next" pointer, the
// actual number of elements per page won't be quite as many as were requested.
//
// This class should only be used if it's valid to construct T elements from all
// zeroes. The class also fails to call the destructor on items. However, it
// will only destroy items after it has moved out their contents. The queue is
// required to be empty when it is destroyed.
template<class T, size_t RequestedItemsPerPage = 256>
class Queue
{
public:
  Queue() {}

  ~Queue()
  {
    MOZ_ASSERT(IsEmpty());

    if (mHead) {
      free(mHead);
    }
  }

  T& Push(T&& aElement)
  {
    if (!mHead) {
      mHead = NewPage();
      MOZ_ASSERT(mHead);

      mTail = mHead;
      mOffsetHead = 0;
      mOffsetTail = 0;
    } else if (mOffsetTail == ItemsPerPage) {
      Page* page = NewPage();
      MOZ_ASSERT(page);

      mTail->mNext = page;
      mTail = page;
      mOffsetTail = 0;
    }

    T& eltLocation = mTail->mEvents[mOffsetTail];
    eltLocation = std::move(aElement);
    ++mOffsetTail;

    return eltLocation;
  }

  bool IsEmpty() const
  {
    return !mHead || (mHead == mTail && mOffsetHead == mOffsetTail);
  }

  T Pop()
  {
    MOZ_ASSERT(!IsEmpty());

    MOZ_ASSERT(mOffsetHead < ItemsPerPage);
    MOZ_ASSERT_IF(mHead == mTail, mOffsetHead <= mOffsetTail);
    T result = std::move(mHead->mEvents[mOffsetHead++]);

    MOZ_ASSERT(mOffsetHead <= ItemsPerPage);

    // Check if mHead points to empty Page
    if (mOffsetHead == ItemsPerPage) {
      Page* dead = mHead;
      mHead = mHead->mNext;
      free(dead);
      mOffsetHead = 0;
    }

    return result;
  }

  void FirstElementAssertions() const
  {
    MOZ_ASSERT(!IsEmpty());
    MOZ_ASSERT(mOffsetHead < ItemsPerPage);
    MOZ_ASSERT_IF(mHead == mTail, mOffsetHead <= mOffsetTail);
  }

  T& FirstElement()
  {
    FirstElementAssertions();
    return mHead->mEvents[mOffsetHead];
  }

  const T& FirstElement() const
  {
    FirstElementAssertions();
    return mHead->mEvents[mOffsetHead];
  }

  void LastElementAssertions() const
  {
    MOZ_ASSERT(!IsEmpty());
    MOZ_ASSERT(mOffsetTail > 0);
    MOZ_ASSERT(mOffsetTail <= ItemsPerPage);
    MOZ_ASSERT_IF(mHead == mTail, mOffsetHead <= mOffsetTail);
  }

  T& LastElement()
  {
    LastElementAssertions();
    return mTail->mEvents[mOffsetTail - 1];
  }

  const T& LastElement() const
  {
    LastElementAssertions();
    return mTail->mEvents[mOffsetTail - 1];
  }

  size_t Count() const
  {
    // It is obvious count is 0 when the queue is empty.
    if (!mHead) {
      return 0;
    }

    /* How we count the number of events in the queue:
     * 1. Let pageCount(x, y) denote the number of pages excluding the tail page
     *    where x is the index of head page and y is the index of the tail page.
     * 2. Then we have pageCount(x, y) = y - x.
     *
     * Ex: pageCount(0, 0) = 0 where both head and tail pages point to page 0.
     *     pageCount(0, 1) = 1 where head points to page 0 and tail points page 1.
     *
     * 3. number of events = (ItemsPerPage * pageCount(x, y))
     *      - (empty slots in head page) + (non-empty slots in tail page)
     *      = (ItemsPerPage * pageCount(x, y)) - mOffsetHead + mOffsetTail
     */

    int count = -mOffsetHead;

    // Compute (ItemsPerPage * pageCount(x, y))
    for (Page* page = mHead; page != mTail; page = page->mNext) {
      count += ItemsPerPage;
    }

    count += mOffsetTail;
    MOZ_ASSERT(count >= 0);

    return count;
  }

private:
  static_assert((RequestedItemsPerPage & (RequestedItemsPerPage - 1)) == 0,
                "RequestedItemsPerPage should be a power of two to avoid heap slop.");

  // Since a Page must also contain a "next" pointer, we use one of the items to
  // store this pointer. If sizeof(T) > sizeof(Page*), then some space will be
  // wasted. So be it.
  static const size_t ItemsPerPage = RequestedItemsPerPage - 1;

  // Page objects are linked together to form a simple deque.
  struct Page
  {
    struct Page* mNext;
    T mEvents[ItemsPerPage];
  };

  static Page* NewPage()
  {
    return static_cast<Page*>(moz_xcalloc(1, sizeof(Page)));
  }

  Page* mHead = nullptr;
  Page* mTail = nullptr;

  uint16_t mOffsetHead = 0;  // offset into mHead where next item is removed
  uint16_t mOffsetTail = 0;  // offset into mTail where next item is added
};

} // namespace mozilla

#endif // mozilla_Queue_h
