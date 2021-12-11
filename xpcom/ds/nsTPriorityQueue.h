/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_TPRIORITY_QUEUE_H_
#define NS_TPRIORITY_QUEUE_H_

#include "mozilla/Assertions.h"
#include "nsTArray.h"

/**
 * A templatized priority queue data structure that uses an nsTArray to serve as
 * a binary heap. The default comparator causes this to act like a min-heap.
 * Only the LessThan method of the comparator is used.
 */
template <class T, class Compare = nsDefaultComparator<T, T>>
class nsTPriorityQueue {
 public:
  typedef typename nsTArray<T>::size_type size_type;

  /**
   * Default constructor also creates a comparator object using the default
   * constructor for type Compare.
   */
  nsTPriorityQueue() : mCompare(Compare()) {}

  /**
   * Constructor to allow a specific instance of a comparator object to be
   * used.
   */
  explicit nsTPriorityQueue(const Compare& aComp) : mCompare(aComp) {}

  nsTPriorityQueue(nsTPriorityQueue&&) = default;
  nsTPriorityQueue& operator=(nsTPriorityQueue&&) = default;

  /**
   * @return True if the queue is empty or false otherwise.
   */
  bool IsEmpty() const { return mElements.IsEmpty(); }

  /**
   * @return The number of elements in the queue.
   */
  size_type Length() const { return mElements.Length(); }

  /**
   * @return The topmost element in the queue without changing the queue. This
   * is the element 'a' such that there is no other element 'b' in the queue for
   * which Compare(b, a) returns true. (Since this container does not check
   * for duplicate entries there may exist 'b' for which Compare(a, b) returns
   * false.)
   */
  const T& Top() const {
    MOZ_ASSERT(!mElements.IsEmpty(), "Empty queue");
    return mElements[0];
  }

  /**
   * Adds an element to the queue
   * @param aElement The element to add
   * @return true on success, false on out of memory.
   */
  void Push(T&& aElement) {
    mElements.AppendElement(std::move(aElement));

    // Sift up
    size_type i = mElements.Length() - 1;
    while (i) {
      size_type parent = (size_type)((i - 1) / 2);
      if (mCompare.LessThan(mElements[parent], mElements[i])) {
        break;
      }
      std::swap(mElements[i], mElements[parent]);
      i = parent;
    }
  }

  /**
   * Removes and returns the top-most element from the queue.
   * @return The topmost element, that is, the element 'a' such that there is no
   * other element 'b' in the queue for which Compare(b, a) returns true.
   * @see Top()
   */
  T Pop() {
    MOZ_ASSERT(!mElements.IsEmpty(), "Empty queue");
    T pop = std::move(mElements[0]);

    const size_type newLength = mElements.Length() - 1;
    if (newLength == 0) {
      mElements.Clear();
      return pop;
    }

    // Move last to front
    mElements[0] = mElements.PopLastElement();

    // Sift down
    size_type i = 0;
    while (2 * i + 1 < newLength) {
      size_type swap = i;
      size_type l_child = 2 * i + 1;
      if (mCompare.LessThan(mElements[l_child], mElements[i])) {
        swap = l_child;
      }
      size_type r_child = l_child + 1;
      if (r_child < newLength &&
          mCompare.LessThan(mElements[r_child], mElements[swap])) {
        swap = r_child;
      }
      if (swap == i) {
        break;
      }
      std::swap(mElements[i], mElements[swap]);
      i = swap;
    }

    return pop;
  }

  /**
   * Removes all elements from the queue.
   */
  void Clear() { mElements.Clear(); }

  /**
   * Provides readonly access to the queue elements as an array. Generally this
   * should be avoided but may be needed in some situations such as when the
   * elements contained in the queue need to be enumerated for cycle-collection.
   * @return A pointer to the first element of the array.  If the array is
   * empty, then this pointer must not be dereferenced.
   */
  const T* Elements() const { return mElements.Elements(); }

  nsTPriorityQueue Clone() const {
    auto res = nsTPriorityQueue{mCompare};
    res.mElements = mElements.Clone();
    return res;
  }

 protected:
  nsTArray<T> mElements;
  Compare mCompare;  // Comparator object
};

#endif  // NS_TPRIORITY_QUEUE_H_
