/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** A doubly-linked list with flexible next/prev naming. */

#ifndef mozilla_DoublyLinkedList_h
#define mozilla_DoublyLinkedList_h

#include <algorithm>
#include <iterator>

#include "mozilla/Assertions.h"

/**
 * Where mozilla::LinkedList strives for ease of use above all other
 * considerations, mozilla::DoublyLinkedList strives for flexibility. The
 * following are things that can be done with mozilla::DoublyLinkedList that
 * cannot be done with mozilla::LinkedList:
 *
 *   * Arbitrary next/prev placement and naming. With the tools provided here,
 *     the next and previous pointers can be at the end of the structure, in a
 *     sub-structure, stored with a tag, in a union, wherever, as long as you
 *     can look them up and set them on demand.
 *   * Can be used without deriving from a new base and, thus, does not require
 *     use of constructors.
 *
 * Example:
 *
 *   class Observer : public DoublyLinkedListElement<Observer>
 *   {
 *   public:
 *     void observe(char* aTopic) { ... }
 *   };
 *
 *   class ObserverContainer
 *   {
 *   private:
 *     DoublyLinkedList<Observer> mList;
 *
 *   public:
 *     void addObserver(Observer* aObserver)
 *     {
 *       // Will assert if |aObserver| is part of another list.
 *       mList.pushBack(aObserver);
 *     }
 *
 *     void removeObserver(Observer* aObserver)
 *     {
 *       // Will assert if |aObserver| is not part of |list|.
 *       mList.remove(aObserver);
 *     }
 *
 *     void notifyObservers(char* aTopic)
 *     {
 *       for (Observer* o : mList) {
 *         o->observe(aTopic);
 *       }
 *     }
 *   };
 */

namespace mozilla {

/**
 * Provides access to a next and previous element pointer named |mNext| and
 * |mPrev| respectively. This class is the default and will work if the list
 * element derives from DoublyLinkedListElement.
 *
 * Although designed to work with DoublyLinkedListElement this will als work
 * with any class that defines |mNext| and |mPrev| members with the correct
 * type.
 */
template <typename T>
struct DoublyLinkedSiblingAccess {
  static void SetNext(T* aElm, T* aNext) { aElm->mNext = aNext; }
  static T* GetNext(T* aElm) { return aElm->mNext; }
  static void SetPrev(T* aElm, T* aPrev) { aElm->mPrev = aPrev; }
  static T* GetPrev(T* aElm) { return aElm->mPrev; }
};

/**
 *  Deriving from this will allow T to be inserted into and removed from a
 *  DoublyLinkedList.
 */
template <typename T>
struct DoublyLinkedListElement
{
  friend struct DoublyLinkedSiblingAccess<T>;
  T* mNext;
  T* mPrev;

public:
  DoublyLinkedListElement() : mNext(nullptr), mPrev(nullptr) {}
};

/**
 * A doubly linked list. |T| is the type of element stored in this list. |T|
 * must contain or have access to unique next and previous element pointers.
 * The template argument |SiblingAccess| provides code to tell this list how to
 * get and set the next and previous pointers. The actual storage of next/prev
 * links may reside anywhere and be encoded in any way.
 */
template <typename T, typename SiblingAccess = DoublyLinkedSiblingAccess<T>>
class DoublyLinkedList final
{
  T* mHead;
  T* mTail;

  /**
   * Checks that either the list is empty and both mHead and mTail are nullptr
   * or the list has entries and both mHead and mTail are non-null.
   */
  bool isStateValid() const {
    return (mHead != nullptr) == (mTail != nullptr);
  }

  static bool ElementNotInList(T* aElm) {
    return !SiblingAccess::GetNext(aElm) && !SiblingAccess::GetPrev(aElm);
  }

public:
  DoublyLinkedList() : mHead(nullptr), mTail(nullptr) {}

  class Iterator final {
    T* mCurrent;

  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;

    Iterator() : mCurrent(nullptr) {}
    explicit Iterator(T* aCurrent) : mCurrent(aCurrent) {}

    T& operator *() const { return *mCurrent; }
    T* operator ->() const { return mCurrent; }

    Iterator& operator++() {
      mCurrent = SiblingAccess::GetNext(mCurrent);
      return *this;
    }

    Iterator operator++(int) {
      Iterator result = *this;
      ++(*this);
      return result;
    }

    Iterator& operator--() {
      mCurrent = SiblingAccess::GetPrev(mCurrent);
      return *this;
    }

    Iterator operator--(int) {
      Iterator result = *this;
      --(*this);
      return result;
    }

    bool operator!=(const Iterator& aOther) const {
      return mCurrent != aOther.mCurrent;
    }

    bool operator==(const Iterator& aOther) const {
      return mCurrent == aOther.mCurrent;
    }

    explicit operator bool() const {
      return mCurrent;
    }
  };

  Iterator begin() { return Iterator(mHead); }
  const Iterator begin() const { return Iterator(mHead); }
  const Iterator cbegin() const { return Iterator(mHead); }

  Iterator end() { return Iterator(); }
  const Iterator end() const { return Iterator(); }
  const Iterator cend() const { return Iterator(); }

  /**
   * Returns true if the list contains no elements.
   */
  bool isEmpty() const {
    MOZ_ASSERT(isStateValid());
    return mHead == nullptr;
  }

  /**
   * Inserts aElm into the list at the head position. |aElm| must not already
   * be in a list.
   */
  void pushFront(T* aElm) {
    MOZ_ASSERT(aElm);
    MOZ_ASSERT(ElementNotInList(aElm));
    MOZ_ASSERT(isStateValid());

    SiblingAccess::SetNext(aElm, mHead);
    if (mHead) {
      MOZ_ASSERT(!SiblingAccess::GetPrev(mHead));
      SiblingAccess::SetPrev(mHead, aElm);
    }

    mHead = aElm;
    if (!mTail) {
      mTail = aElm;
    }
  }

  /**
   * Remove the head of the list and return it. Calling this on an empty list
   * will assert.
   */
  T* popFront() {
    MOZ_ASSERT(!isEmpty());
    MOZ_ASSERT(isStateValid());

    T* result = mHead;
    mHead = result ? SiblingAccess::GetNext(result) : nullptr;
    if (mHead) {
      SiblingAccess::SetPrev(mHead, nullptr);
    }

    if (mTail == result) {
      mTail = nullptr;
    }

    if (result) {
      SiblingAccess::SetNext(result, nullptr);
      SiblingAccess::SetPrev(result, nullptr);
    }

    return result;
  }

  /**
   * Inserts aElm into the list at the tail position. |aElm| must not already
   * be in a list.
   */
  void pushBack(T* aElm) {
    MOZ_ASSERT(aElm);
    MOZ_ASSERT(ElementNotInList(aElm));
    MOZ_ASSERT(isStateValid());

    SiblingAccess::SetNext(aElm, nullptr);
    SiblingAccess::SetPrev(aElm, mTail);
    if (mTail) {
      MOZ_ASSERT(!SiblingAccess::GetNext(mTail));
      SiblingAccess::SetNext(mTail, aElm);
    }

    mTail = aElm;
    if (!mHead) {
      mHead = aElm;
    }
  }

  /**
   * Remove the tail of the list and return it. Calling this on an empty list
   * will assert.
   */
  T* popBack() {
    MOZ_ASSERT(!isEmpty());
    MOZ_ASSERT(isStateValid());

    T* result = mTail;
    mTail = result ? SiblingAccess::GetPrev(result) : nullptr;
    if (mTail) {
      SiblingAccess::SetNext(mTail, nullptr);
    }

    if (mHead == result) {
      mHead = nullptr;
    }

    if (result) {
      SiblingAccess::SetNext(result, nullptr);
      SiblingAccess::SetPrev(result, nullptr);
    }

    return result;
  }

  /**
   * Insert the given |aElm| *before* |aIter|.
   */
  void insertBefore(const Iterator& aIter, T* aElm) {
    MOZ_ASSERT(aElm);
    MOZ_ASSERT(ElementNotInList(aElm));
    MOZ_ASSERT(isStateValid());

    if (!aIter) {
      return pushBack(aElm);
    } else if (aIter == begin()) {
      return pushFront(aElm);
    }

    T* after = &(*aIter);
    T* before = SiblingAccess::GetPrev(after);
    MOZ_ASSERT(before);

    SiblingAccess::SetNext(before, aElm);
    SiblingAccess::SetPrev(aElm, before);
    SiblingAccess::SetNext(aElm, after);
    SiblingAccess::SetPrev(after, aElm);
  }

  /**
   * Removes the given element from the list. The element must be in this list.
   */
  void remove(T* aElm) {
    MOZ_ASSERT(aElm);
    MOZ_ASSERT(SiblingAccess::GetNext(aElm) || SiblingAccess::GetPrev(aElm) ||
               (aElm == mHead && aElm == mTail),
               "Attempted to remove element not in this list");

    if (T* prev = SiblingAccess::GetPrev(aElm)) {
      SiblingAccess::SetNext(prev, SiblingAccess::GetNext(aElm));
    } else {
      MOZ_ASSERT(mHead == aElm);
      mHead = SiblingAccess::GetNext(aElm);
    }

    if (T* next = SiblingAccess::GetNext(aElm)) {
      SiblingAccess::SetPrev(next, SiblingAccess::GetPrev(aElm));
    } else {
      MOZ_ASSERT(mTail == aElm);
      mTail = SiblingAccess::GetPrev(aElm);
    }

    SiblingAccess::SetNext(aElm, nullptr);
    SiblingAccess::SetPrev(aElm, nullptr);
  }

  /**
   * Returns an iterator referencing the first found element whose value matches
   * the given element according to operator==.
   */
  Iterator find(const T& aElm) {
    return std::find(begin(), end(), aElm);
  }

  /**
   * Returns whether the given element is in the list. Note that this uses
   * T::operator==, not pointer comparison.
   */
  bool contains(const T& aElm) {
    return find(aElm) != Iterator();
  }
};

} // namespace mozilla

#endif // mozilla_DoublyLinkedList_h
