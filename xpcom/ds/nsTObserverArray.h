/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTObserverArray_h___
#define nsTObserverArray_h___

#include "mozilla/MemoryReporting.h"
#include "nsTArray.h"
#include "nsCycleCollectionNoteChild.h"

#include <functional>

/**
 * An array of observers. Like a normal array, but supports iterators that are
 * stable even if the array is modified during iteration.
 * The template parameter T is the observer type the array will hold;
 * N is the number of built-in storage slots that come with the array.
 * NOTE: You probably want to use nsTObserverArray, unless you specifically
 * want built-in storage. See below.
 * @see nsTObserverArray, nsTArray
 */

class nsTObserverArray_base {
 public:
  typedef size_t index_type;
  typedef size_t size_type;
  typedef ptrdiff_t diff_type;

 protected:
  class Iterator_base {
   protected:
    friend class nsTObserverArray_base;

    Iterator_base(index_type aPosition, Iterator_base* aNext)
        : mPosition(aPosition), mNext(aNext) {}

    // The current position of the iterator. Its exact meaning differs
    // depending on iterator. See nsTObserverArray<T>::ForwardIterator.
    index_type mPosition;

    // The next iterator currently iterating the same array
    Iterator_base* mNext;
  };

  nsTObserverArray_base() : mIterators(nullptr) {}

  ~nsTObserverArray_base() {
    NS_ASSERTION(mIterators == nullptr, "iterators outlasting array");
  }

  /**
   * Adjusts iterators after an element has been inserted or removed
   * from the array.
   * @param aModPos     Position where elements were added or removed.
   * @param aAdjustment -1 if an element was removed, 1 if an element was
   *                    added.
   */
  void AdjustIterators(index_type aModPos, diff_type aAdjustment);

  /**
   * Clears iterators when the array is destroyed.
   */
  void ClearIterators();

  mutable Iterator_base* mIterators;
};

template <class T, size_t N>
class nsAutoTObserverArray : protected nsTObserverArray_base {
 public:
  typedef T elem_type;
  typedef nsTArray<T> array_type;

  nsAutoTObserverArray() = default;

  //
  // Accessor methods
  //

  // @return The number of elements in the array.
  size_type Length() const { return mArray.Length(); }

  // @return True if the array is empty or false otherwise.
  bool IsEmpty() const { return mArray.IsEmpty(); }

  // This method provides direct, readonly access to the array elements.
  // @return A pointer to the first element of the array.  If the array is
  // empty, then this pointer must not be dereferenced.
  const elem_type* Elements() const { return mArray.Elements(); }
  elem_type* Elements() { return mArray.Elements(); }

  // This method provides direct access to an element of the array. The given
  // index must be within the array bounds. If the underlying array may change
  // during iteration, use an iterator instead of this function.
  // @param aIndex The index of an element in the array.
  // @return A reference to the i'th element of the array.
  elem_type& ElementAt(index_type aIndex) { return mArray.ElementAt(aIndex); }

  // Same as above, but readonly.
  const elem_type& ElementAt(index_type aIndex) const {
    return mArray.ElementAt(aIndex);
  }

  // This method provides direct access to an element of the array in a bounds
  // safe manner. If the requested index is out of bounds the provided default
  // value is returned.
  // @param aIndex The index of an element in the array.
  // @param aDef   The value to return if the index is out of bounds.
  elem_type& SafeElementAt(index_type aIndex, elem_type& aDef) {
    return mArray.SafeElementAt(aIndex, aDef);
  }

  // Same as above, but readonly.
  const elem_type& SafeElementAt(index_type aIndex,
                                 const elem_type& aDef) const {
    return mArray.SafeElementAt(aIndex, aDef);
  }

  // No operator[] is provided because the point of this class is to support
  // allow modifying the array during iteration, and ElementAt() is not safe
  // in those conditions.

  //
  // Search methods
  //

  // This method searches, starting from the beginning of the array,
  // for the first element in this array that is equal to the given element.
  // 'operator==' must be defined for elem_type.
  // @param aItem The item to search for.
  // @return      true if the element was found.
  template <class Item>
  bool Contains(const Item& aItem) const {
    return IndexOf(aItem) != array_type::NoIndex;
  }

  // This method searches for the offset of the first element in this
  // array that is equal to the given element.
  // 'operator==' must be defined for elem_type.
  // @param aItem  The item to search for.
  // @param aStart The index to start from.
  // @return The index of the found element or NoIndex if not found.
  template <class Item>
  index_type IndexOf(const Item& aItem, index_type aStart = 0) const {
    return mArray.IndexOf(aItem, aStart);
  }

  //
  // Mutation methods
  //

  // Insert a given element at the given index.
  // @param aIndex The index at which to insert item.
  // @param aItem  The item to insert,
  template <class Item>
  void InsertElementAt(index_type aIndex, const Item& aItem) {
    mArray.InsertElementAt(aIndex, aItem);
    AdjustIterators(aIndex, 1);
  }

  // Same as above but without copy constructing.
  // This is useful to avoid temporaries.
  elem_type* InsertElementAt(index_type aIndex) {
    elem_type* item = mArray.InsertElementAt(aIndex);
    AdjustIterators(aIndex, 1);
    return item;
  }

  // Prepend an element to the array unless it already exists in the array.
  // 'operator==' must be defined for elem_type.
  // @param aItem The item to prepend.
  template <class Item>
  void PrependElementUnlessExists(const Item& aItem) {
    if (!Contains(aItem)) {
      mArray.InsertElementAt(0, aItem);
      AdjustIterators(0, 1);
    }
  }

  // Append an element to the array.
  // @param aItem The item to append.
  template <class Item>
  void AppendElement(Item&& aItem) {
    mArray.AppendElement(std::forward<Item>(aItem));
  }

  // Same as above, but without copy-constructing. This is useful to avoid
  // temporaries.
  elem_type* AppendElement() { return mArray.AppendElement(); }

  // Append an element to the array unless it already exists in the array.
  // 'operator==' must be defined for elem_type.
  // @param aItem The item to append.
  template <class Item>
  void AppendElementUnlessExists(const Item& aItem) {
    if (!Contains(aItem)) {
      mArray.AppendElement(aItem);
    }
  }

  // Remove an element from the array.
  // @param aIndex The index of the item to remove.
  void RemoveElementAt(index_type aIndex) {
    NS_ASSERTION(aIndex < mArray.Length(), "invalid index");
    mArray.RemoveElementAt(aIndex);
    AdjustIterators(aIndex, -1);
  }

  // This helper function combines IndexOf with RemoveElementAt to "search
  // and destroy" the first element that is equal to the given element.
  // 'operator==' must be defined for elem_type.
  // @param aItem The item to search for.
  // @return true if the element was found and removed.
  template <class Item>
  bool RemoveElement(const Item& aItem) {
    index_type index = mArray.IndexOf(aItem, 0);
    if (index == array_type::NoIndex) {
      return false;
    }

    mArray.RemoveElementAt(index);
    AdjustIterators(index, -1);
    return true;
  }

  // See nsTArray::RemoveElementsBy.
  template <typename Predicate>
  void RemoveElementsBy(Predicate aPredicate) {
    index_type i = 0;
    mArray.RemoveElementsBy([&](const elem_type& aItem) {
      if (aPredicate(aItem)) {
        // This element is going to be removed.
        AdjustIterators(i, -1);
        return true;
      }
      ++i;
      return false;
    });
  }

  // Removes all observers and collapses all iterators to the beginning of
  // the array. The result is that forward iterators will see all elements
  // in the array.
  void Clear() {
    mArray.Clear();
    ClearIterators();
  }

  // Compact the array to minimize the memory it uses
  void Compact() { mArray.Compact(); }

  // Returns the number of bytes on the heap taken up by this object, not
  // including sizeof(*this). If you want to measure anything hanging off the
  // array, you must iterate over the elements and measure them individually;
  // hence the "Shallow" prefix.
  size_t ShallowSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return mArray.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  //
  // Iterators
  //

  // Base class for iterators. Do not use this directly.
  class Iterator : public Iterator_base {
   protected:
    friend class nsAutoTObserverArray;
    typedef nsAutoTObserverArray<T, N> array_type;

    Iterator(index_type aPosition, const array_type& aArray)
        : Iterator_base(aPosition, aArray.mIterators),
          mArray(const_cast<array_type&>(aArray)) {
      aArray.mIterators = this;
    }

    ~Iterator() {
      NS_ASSERTION(mArray.mIterators == this,
                   "Iterators must currently be destroyed in opposite order "
                   "from the construction order. It is suggested that you "
                   "simply put them on the stack");
      mArray.mIterators = mNext;
    }

    // The array we're iterating
    array_type& mArray;
  };

  // Iterates the array forward from beginning to end. mPosition points
  // to the element that will be returned on next call to GetNext.
  // Elements:
  // - prepended to the array during iteration *will not* be traversed
  // - appended during iteration *will* be traversed
  // - removed during iteration *will not* be traversed.
  // @see EndLimitedIterator
  class ForwardIterator : protected Iterator {
   public:
    typedef nsAutoTObserverArray<T, N> array_type;
    typedef Iterator base_type;

    explicit ForwardIterator(const array_type& aArray) : Iterator(0, aArray) {}

    ForwardIterator(const array_type& aArray, index_type aPos)
        : Iterator(aPos, aArray) {}

    bool operator<(const ForwardIterator& aOther) const {
      NS_ASSERTION(&this->mArray == &aOther.mArray,
                   "not iterating the same array");
      return base_type::mPosition < aOther.mPosition;
    }

    // Returns true if there are more elements to iterate.
    // This must precede a call to GetNext(). If false is
    // returned, GetNext() must not be called.
    bool HasMore() const {
      return base_type::mPosition < base_type::mArray.Length();
    }

    // Returns the next element and steps one step. This must
    // be preceded by a call to HasMore().
    // @return The next observer.
    elem_type& GetNext() {
      NS_ASSERTION(HasMore(), "iterating beyond end of array");
      return base_type::mArray.Elements()[base_type::mPosition++];
    }
  };

  // EndLimitedIterator works like ForwardIterator, but will not iterate new
  // observers appended to the array after the iterator was created.
  class EndLimitedIterator : protected ForwardIterator {
   public:
    typedef nsAutoTObserverArray<T, N> array_type;
    typedef Iterator base_type;

    explicit EndLimitedIterator(const array_type& aArray)
        : ForwardIterator(aArray), mEnd(aArray, aArray.Length()) {}

    // Returns true if there are more elements to iterate.
    // This must precede a call to GetNext(). If false is
    // returned, GetNext() must not be called.
    bool HasMore() const { return *this < mEnd; }

    // Returns the next element and steps one step. This must
    // be preceded by a call to HasMore().
    // @return The next observer.
    elem_type& GetNext() {
      NS_ASSERTION(HasMore(), "iterating beyond end of array");
      return base_type::mArray.Elements()[base_type::mPosition++];
    }

   private:
    ForwardIterator mEnd;
  };

  // Iterates the array backward from end to start. mPosition points
  // to the element that was returned last.
  // Elements:
  // - prepended to the array during iteration *will* be traversed,
  //   unless the iteration already arrived at the first element
  // - appended during iteration *will not* be traversed
  // - removed during iteration *will not* be traversed.
  class BackwardIterator : protected Iterator {
   public:
    typedef nsAutoTObserverArray<T, N> array_type;
    typedef Iterator base_type;

    explicit BackwardIterator(const array_type& aArray)
        : Iterator(aArray.Length(), aArray) {}

    // Returns true if there are more elements to iterate.
    // This must precede a call to GetNext(). If false is
    // returned, GetNext() must not be called.
    bool HasMore() const { return base_type::mPosition > 0; }

    // Returns the next element and steps one step. This must
    // be preceded by a call to HasMore().
    // @return The next observer.
    elem_type& GetNext() {
      NS_ASSERTION(HasMore(), "iterating beyond start of array");
      return base_type::mArray.Elements()[--base_type::mPosition];
    }

    // Removes the element at the current iterator position.
    // (the last element returned from |GetNext()|)
    // This will not affect the next call to |GetNext()|
    void Remove() {
      return base_type::mArray.RemoveElementAt(base_type::mPosition);
    }
  };

 protected:
  AutoTArray<T, N> mArray;
};

template <class T>
class nsTObserverArray : public nsAutoTObserverArray<T, 0> {
 public:
  typedef nsAutoTObserverArray<T, 0> base_type;
  typedef nsTObserverArray_base::size_type size_type;

  //
  // Initialization methods
  //

  nsTObserverArray() = default;

  // Initialize this array and pre-allocate some number of elements.
  explicit nsTObserverArray(size_type aCapacity) {
    base_type::mArray.SetCapacity(aCapacity);
  }
};

template <typename T, size_t N>
inline void ImplCycleCollectionUnlink(nsAutoTObserverArray<T, N>& aField) {
  aField.Clear();
}

template <typename T, size_t N>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsAutoTObserverArray<T, N>& aField, const char* aName,
    uint32_t aFlags = 0) {
  aFlags |= CycleCollectionEdgeNameArrayFlag;
  size_t length = aField.Length();
  for (size_t i = 0; i < length; ++i) {
    ImplCycleCollectionTraverse(aCallback, aField.ElementAt(i), aName, aFlags);
  }
}

// XXXbz I wish I didn't have to pass in the observer type, but I
// don't see a way to get it out of array_.
// Note that this macro only works if the array holds pointers to XPCOM objects.
#define NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(array_, obstype_, func_, \
                                                 params_)                 \
  do {                                                                    \
    nsTObserverArray<obstype_*>::ForwardIterator iter_(array_);           \
    RefPtr<obstype_> obs_;                                                \
    while (iter_.HasMore()) {                                             \
      obs_ = iter_.GetNext();                                             \
      obs_->func_ params_;                                                \
    }                                                                     \
  } while (0)

// Note that this macro only works if the array holds pointers to XPCOM objects.
#define NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(array_, obstype_, func_, params_) \
  do {                                                                       \
    nsTObserverArray<obstype_*>::ForwardIterator iter_(array_);              \
    obstype_* obs_;                                                          \
    while (iter_.HasMore()) {                                                \
      obs_ = iter_.GetNext();                                                \
      obs_->func_ params_;                                                   \
    }                                                                        \
  } while (0)

// Note that this macro only works if the array holds pointers to XPCOM objects.
#define NS_OBSERVER_AUTO_ARRAY_NOTIFY_OBSERVERS(array_, obstype_, num_, func_, \
                                                params_)                       \
  do {                                                                         \
    nsAutoTObserverArray<obstype_*, num_>::ForwardIterator iter_(array_);      \
    obstype_* obs_;                                                            \
    while (iter_.HasMore()) {                                                  \
      obs_ = iter_.GetNext();                                                  \
      obs_->func_ params_;                                                     \
    }                                                                          \
  } while (0)

#define NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS_WITH_QI(array_, basetype_,        \
                                                   obstype_, func_, params_) \
  do {                                                                       \
    nsTObserverArray<basetype_*>::ForwardIterator iter_(array_);             \
    basetype_* obsbase_;                                                     \
    while (iter_.HasMore()) {                                                \
      obsbase_ = iter_.GetNext();                                            \
      nsCOMPtr<obstype_> obs_ = do_QueryInterface(obsbase_);                 \
      if (obs_) {                                                            \
        obs_->func_ params_;                                                 \
      }                                                                      \
    }                                                                        \
  } while (0)

#define NS_OBSERVER_AUTO_ARRAY_NOTIFY_OBSERVERS_WITH_QI(                   \
    array_, basetype_, num_, obstype_, func_, params_)                     \
  do {                                                                     \
    nsAutoTObserverArray<basetype_*, num_>::ForwardIterator iter_(array_); \
    basetype_* obsbase_;                                                   \
    while (iter_.HasMore()) {                                              \
      obsbase_ = iter_.GetNext();                                          \
      nsCOMPtr<obstype_> obs_ = do_QueryInterface(obsbase_);               \
      if (obs_) {                                                          \
        obs_->func_ params_;                                               \
      }                                                                    \
    }                                                                      \
  } while (0)
#endif  // nsTObserverArray_h___
