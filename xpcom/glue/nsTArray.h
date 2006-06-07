/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is C++ array template.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsTArray_h__
#define nsTArray_h__

#include "prtypes.h"
#include "nsQuickSort.h"
#include "nsDebug.h"
#include NEW_H

//
// This class serves as a base class for nsTArray.  It shouldn't be used
// directly.  It holds common implementation code that does not depend on the
// element type of the nsTArray.
//
class NS_COM_GLUE nsTArray_base {
  public:
    typedef PRUint32 size_type;
    typedef PRUint32 index_type;

    // A special value that is used to indicate an invalid or unknown index
    // into the array.
    enum {
      NoIndex = index_type(-1)
    };

    // @return The number of elements in the array.
    size_type Length() const {
      return mHdr->mLength;
    }

    // @return True if the array is empty or false otherwise.
    PRBool IsEmpty() const {
      return Length() == 0;
    }

    // @return The number of elements that can fit in the array without forcing
    // the array to be re-allocated.  The length of an array is always less
    // than or equal to its capacity.
    size_type Capacity() const {
      return mHdr->mCapacity;
    }

  protected:
    nsTArray_base()
      : mHdr(NS_CONST_CAST(Header *, &sEmptyHdr)) {
    }

    // Resize the storage if necessary to achieve the requested capacity.
    // @param capacity     The requested number of array elements.
    // @param elementSize  The size of an array element.
    // @return False if insufficient memory is available; true otherwise.
    PRBool EnsureCapacity(size_type capacity, size_type elementSize);

    // Resize the storage to the minimum required amount.
    // @param elementSize  The size of an array element.
    void ShrinkCapacity(size_type elementSize);
    
    // This method may be called to resize a "gap" in the array by shifting
    // elements around.  It updates mLength appropriately.  If the resulting
    // array has zero elements, then the array's memory is free'd.
    // @param start        The starting index of the gap.
    // @param oldLen       The current length of the gap.
    // @param newLen       The desired length of the gap.
    // @param elementSize  The size of an array element.
    void ShiftData(index_type start, size_type oldLen, size_type newLen,
                   size_type elementSize);

    // This method increments the length member of the array's header.
    void IncrementLength(PRUint32 n) {
      NS_ASSERTION(mHdr != &sEmptyHdr, "bad data pointer");
      mHdr->mLength += n;
    }

  protected:

    // We prefix mData with a structure of this type.  This is done to minimize
    // the size of the nsTArray object when it is empty.
    struct Header {
      PRUint32 mLength;
      PRUint32 mCapacity;
    };

    static const Header sEmptyHdr;

    // The array's elements (prefixed with a Header).  This pointer is never
    // null.  If the array is empty, then this will point to sEmptyHdr.
    Header *mHdr;
};

//
// This class defines convenience functions for element specific operations.
// Specialize this template if necessary.
//
template<class E>
class nsTArrayElementTraits {
  public:
    // Invoke the default constructor in place.
    static inline void Construct(E *e) {
      new (NS_STATIC_CAST(void *, e)) E();
    }
    // Invoke the copy-constructor in place.
    template<class A>
    static inline void Construct(E *e, const A &arg) {
      new (NS_STATIC_CAST(void *, e)) E(arg);
    }
    // Invoke the destructor in place.
    static inline void Destruct(E *e) {
      e->~E();
    }
};

// This class exists because VC6 cannot handle static template functions.
// Otherwise, the Compare method would be defined directly on nsTArray.
template <class E, class Comparator>
class nsQuickSortComparator {
  public:
    typedef E elem_type;
    // This function is meant to be used with the NS_QuickSort function.  It
    // maps the callback API expected by NS_QuickSort to the Comparator API
    // used by nsTArray.  See nsTArray::Sort.
    static int Compare(const void* e1, const void* e2, void *data) {
      const Comparator* c = NS_REINTERPRET_CAST(const Comparator*, data);
      const elem_type* a = NS_STATIC_CAST(const elem_type*, e1);
      const elem_type* b = NS_STATIC_CAST(const elem_type*, e2);
      return c->LessThan(*a, *b) ? -1 : (c->Equals(*a, *b) ? 0 : 1);
    }
};

// The default comparator used by nsTArray
template<class A, class B>
class nsDefaultComparator {
  public:
    PRBool Equals(const A& a, const B& b) const {
      return a == b;
    }
    PRBool LessThan(const A& a, const B& b) const {
      return a < b;
    }
};

//
// The templatized array class that dynamically resizes its storage as elements
// are added.  This class is designed to behave a bit like std::vector.
//
// The template parameter specifies the type of the elements (elem_type), and
// has the following requirements:
//
//   elem_type MUST define a copy-constructor.
//   elem_type MAY define operator< for sorting.
//   elem_type MAY define operator== for searching.
//
// For methods taking a Comparator instance, the Comparator must be a class
// defining the following methods:
//
//   class Comparator {
//     public:
//       /** @return True if the elements are equals; false otherwise. */
//       PRBool Equals(const elem_type& a, const elem_type& b) const;
//
//       /** @return True if (a < b); false otherwise. */
//       PRBool LessThan(const elem_type& a, const elem_type& b) const;
//   };
//
// The Equals method is used for searching, and the LessThan method is used
// for sorting.
//
template<class E>
class nsTArray : public nsTArray_base {
  public:
    typedef E                        elem_type;
    typedef nsTArray<E>              self_type;
    typedef nsTArrayElementTraits<E> elem_traits;

    //
    // Finalization method
    //

    ~nsTArray() { Clear(); }

    //
    // Initialization methods
    //

    nsTArray() {}

    // Initialize this array and pre-allocate some number of elements.
    explicit nsTArray(size_type capacity) {
      SetCapacity(capacity);
    }
    
    // The array's copy-constructor performs a 'deep' copy of the given array.
    // @param other  The array object to copy.
    nsTArray(const self_type& other) {
      AppendElements(other);
    }

    // The array's assignment operator performs a 'deep' copy of the given
    // array.  It is optimized to reuse existing storage if possible.
    // @param other  The array object to copy.
    nsTArray& operator=(const self_type& other) {
      ReplaceElementsAt(0, Length(), other.Elements(), other.Length());
      return *this;
    }

    //
    // Accessor methods
    //

    // This method provides direct access to the array elements.
    // @return A pointer to the first element of the array.  If the array is
    // empty, then this pointer must not be dereferenced.
    elem_type* Elements() {
      return NS_REINTERPRET_CAST(elem_type *, mHdr + 1);
    }

    // This method provides direct, readonly access to the array elements.
    // @return A pointer to the first element of the array.  If the array is
    // empty, then this pointer must not be dereferenced.
    const elem_type* Elements() const {
      return NS_REINTERPRET_CAST(const elem_type *, mHdr + 1);
    }
    
    // This method provides direct access to the i'th element of the array.
    // The given index must be within the array bounds.
    // @param i  The index of an element in the array.
    // @return   A reference to the i'th element of the array.
    elem_type& ElementAt(index_type i) {
      NS_ASSERTION(i < Length(), "invalid array index");
      return Elements()[i];
    }

    // This method provides direct, readonly access to the i'th element of the
    // array.  The given index must be within the array bounds.
    // @param i  The index of an element in the array.
    // @return   A const reference to the i'th element of the array.
    const elem_type& ElementAt(index_type i) const {
      NS_ASSERTION(i < Length(), "invalid array index");
      return Elements()[i];
    }

    // Shorthand for ElementAt(i)
    elem_type& operator[](index_type i) {
      return ElementAt(i);
    }

    // Shorthand for ElementAt(i)
    const elem_type& operator[](index_type i) const {
      return ElementAt(i);
    }

    //
    // Search methods
    //

    // This method searches for the offset of the first element in this
    // array that is equal to the given element.
    // @param item   The item to search for.
    // @param start  The index to start from.
    // @param comp   The Comparator used to determine element equality.
    // @return       The index of the found element or NoIndex if not found.
    template<class Item, class Comparator>
    index_type IndexOf(const Item& item, index_type start,
                       const Comparator& comp) const {
      const elem_type* iter = Elements() + start, *end = iter + Length();
      for (; iter != end; ++iter) {
        if (comp.Equals(*iter, item))
          return iter - Elements();
      }
      return NoIndex;
    }

    // This method searches for the offset of the first element in this
    // array that is equal to the given element.  This method assumes
    // that 'operator==' is defined for elem_type.
    // @param item   The item to search for.
    // @param start  The index to start from.
    // @return       The index of the found element or NoIndex if not found.
    template<class Item>
    index_type IndexOf(const Item& item, index_type start = 0) const {
      return IndexOf(item, start, nsDefaultComparator<elem_type, Item>());
    }

    // This method searches for the offset of the last element in this
    // array that is equal to the given element.
    // @param item   The item to search for.
    // @param start  The index to start from.  If greater than or equal to the
    //               length of the array, then the entire array is searched.
    // @param comp   The Comparator used to determine element equality.
    // @return       The index of the found element or NoIndex if not found.
    template<class Item, class Comparator>
    index_type LastIndexOf(const Item& item, index_type start,
                           const Comparator& comp) const {
      if (start >= Length())
        start = Length() - 1;
      const elem_type* end = Elements() - 1, *iter = end + start + 1;
      for (; iter != end; --iter) {
        if (comp.Equals(*iter, item))
          return iter - Elements();
      }
      return NoIndex;
    }

    // This method searches for the offset of the last element in this
    // array that is equal to the given element.  This method assumes
    // that 'operator==' is defined for elem_type.
    // @param item   The item to search for.
    // @param start  The index to start from.  If greater than or equal to the
    //               length of the array, then the entire array is searched.
    // @return       The index of the found element or NoIndex if not found.
    template<class Item>
    index_type LastIndexOf(const Item& item,
                           index_type start = NoIndex) const {
      return LastIndexOf(item, start, nsDefaultComparator<elem_type, Item>());
    }

    //
    // Mutation methods
    //

    // This method replaces a range of elements in this array.
    // @param start     The starting index of the elements to replace.
    // @param count     The number of elements to replace.  This may be zero to
    //                  insert elements without removing any existing elements.
    // @param array     The values to copy into this array.  Must be non-null,
    //                  and these elements must not already exist in the array
    //                  being modified.
    // @param arrayLen  The number of values to copy into this array.
    // @return          A pointer to the new elements in the array, or null if
    //                  the operation failed due to insufficient memory.
    template<class Item>
    elem_type *ReplaceElementsAt(index_type start, size_type count,
                                 const Item* array, size_type arrayLen) {
      // Adjust memory allocation up-front to catch errors.
      if (!EnsureCapacity(Length() + arrayLen - count, sizeof(elem_type)))
        return nsnull;
      DestructRange(start, count);
      ShiftData(start, count, arrayLen, sizeof(elem_type));
      AssignRange(start, arrayLen, array);
      return Elements() + start;
    }

    // A variation on the ReplaceElementsAt method defined above.
    template<class Item>
    elem_type *ReplaceElementsAt(index_type start, size_type count,
                                 const nsTArray<Item>& array) {
      return ReplaceElementsAt(start, count, array.Elements(), array.Length());
    }

    // A variation on the ReplaceElementsAt method defined above.
    template<class Item>
    elem_type *ReplaceElementsAt(index_type start, size_type count,
                                 const Item& item) {
      return ReplaceElementsAt(start, count, &item, 1);
    }
    
    // A variation on the ReplaceElementsAt method defined above.
    template<class Item>
    elem_type *InsertElementsAt(index_type index, const Item* array,
                                size_type arrayLen) {
      return ReplaceElementsAt(index, 0, array, arrayLen);
    }

    // A variation on the ReplaceElementsAt method defined above.
    template<class Item>
    elem_type *InsertElementsAt(index_type index, const nsTArray<Item>& array) {
      return ReplaceElementsAt(index, 0, array.Elements(), array.Length());
    }

    // A variation on the ReplaceElementsAt method defined above.
    template<class Item>
    elem_type *InsertElementAt(index_type index, const Item& item) {
      return ReplaceElementsAt(index, 0, &item, 1);
    }

    // Insert a new element without copy-constructing. This is useful to avoid
    // temporaries.
    // @return A pointer to the newly inserted element, or null on OOM.
    elem_type* InsertElementAt(index_type index) {
      if (!EnsureCapacity(Length() + 1, sizeof(elem_type)))
         return nsnull;
      ShiftData(index, 0, 1, sizeof(elem_type));
      elem_type *elem = Elements() + index;
      elem_traits::Construct(elem);
      return elem;
    }

    // This method appends elements to the end of this array.
    // @param array     The elements to append to this array.
    // @param arrayLen  The number of elements to append to this array.
    // @return          A pointer to the new elements in the array, or null if
    //                  the operation failed due to insufficient memory.
    template<class Item>
    elem_type *AppendElements(const Item* array, size_type arrayLen) {
      if (!EnsureCapacity(Length() + arrayLen, sizeof(elem_type)))
        return nsnull;
      index_type len = Length();
      AssignRange(len, arrayLen, array);
      IncrementLength(arrayLen);
      return Elements() + len;
    }

    // A variation on the AppendElements method defined above.
    template<class Item>
    elem_type *AppendElements(const nsTArray<Item>& array) {
      return AppendElements(array.Elements(), array.Length());
    }

    // A variation on the AppendElements method defined above.
    template<class Item>
    elem_type *AppendElement(const Item& item) {
      return AppendElements(&item, 1);
    }

    // Append a new element without copy-constructing. This is useful to avoid
    // temporaries.
    // @return A pointer to the newly appended element, or null on OOM.
    elem_type *AppendElement() {
      if (!EnsureCapacity(Length() + 1, sizeof(elem_type)))
         return nsnull;
      elem_type *elem = Elements() + Length();
      elem_traits::Construct(elem);
      IncrementLength(1);
      return elem;
    }

    // This method removes a range of elements from this array.
    // @param start  The starting index of the elements to remove.
    // @param count  The number of elements to remove.
    void RemoveElementsAt(index_type start, size_type count) {
      DestructRange(start, count);
      ShiftData(start, count, 0, sizeof(elem_type));
    }

    // A variation on the RemoveElementsAt method defined above.
    void RemoveElementAt(index_type index) {
      RemoveElementsAt(index, 1);
    }

    // A variation on the RemoveElementsAt method defined above.
    void Clear() {
      RemoveElementsAt(0, Length());
    }

    // This helper function combines IndexOf with RemoveElementAt to "search
    // and destroy" the first element that is equal to the given element.
    // @param item  The item to search for.
    // @param comp  The Comparator used to determine element equality.
    // @return PR_TRUE if the element was found
    template<class Item, class Comparator>
    PRBool RemoveElement(const Item& item, const Comparator& comp) {
      index_type i = IndexOf(item, 0, comp);
      if (i == NoIndex)
        return PR_FALSE;

      RemoveElementAt(i);
      return PR_TRUE;
    }

    // A variation on the RemoveElement method defined above that assumes
    // that 'operator==' is defined for elem_type.
    template<class Item>
    PRBool RemoveElement(const Item& item) {
      return RemoveElement(item, nsDefaultComparator<elem_type, Item>());
    }

    //
    // Allocation
    //

    // This method may increase the capacity of this array object by the
    // specified amount.  This method may be called in advance of several
    // AppendElement operations to minimize heap re-allocations.  This method
    // will not reduce the number of elements in this array.
    // @param capacity  The desired capacity of this array.
    void SetCapacity(size_type capacity) {
      EnsureCapacity(capacity, sizeof(elem_type));
    }

    // This method modifies the length of the array.  If the new length is
    // larger than the existing length of the array, then new elements will be
    // constructed using elem_type's default constructor.  Otherwise, this call
    // removes elements from the array (see also RemoveElementsAt).
    // @param newLen  The desired length of this array.
    // @return        True if the operation succeeded; false otherwise.
    PRBool SetLength(size_type newLen) {
      size_type oldLen = Length();
      if (newLen > oldLen) {
        SetCapacity(newLen);
        // Check for out of memory conditions
        if (Capacity() < newLen)
          return PR_FALSE;
        // Initialize the extra array elements
        elem_type *iter = Elements() + oldLen, *end = Elements() + newLen;
        for (; iter != end; ++iter) {
          elem_traits::Construct(iter);
        }
        IncrementLength(newLen - oldLen);
      } else {
        RemoveElementsAt(newLen, oldLen - newLen);
      }
      return PR_TRUE;
    }

    // This method may be called to minimize the memory used by this array.
    void Compact() {
      ShrinkCapacity(sizeof(elem_type));
    }

    //
    // Sorting
    //

    // This method sorts the elements of the array.  It uses the LessThan
    // method defined on the given Comparator object to collate elements.
    // @param c  The Comparator to used to collate elements.
    template<class Comparator>
    void Sort(const Comparator& comp) {
      NS_QuickSort(Elements(), Length(), sizeof(elem_type),
                   nsQuickSortComparator<elem_type, Comparator>::Compare,
                   NS_CONST_CAST(Comparator*, &comp));
    }

    // A variation on the Sort method defined above that assumes that
    // 'operator<' is defined for elem_type.
    void Sort() {
      Sort(nsDefaultComparator<elem_type, elem_type>());
    }

  protected:

    // This method invokes elem_type's destructor on a range of elements.
    // @param start  The index of the first element to destroy.
    // @param count  The number of elements to destroy.
    void DestructRange(index_type start, size_type count) {
      elem_type *iter = Elements() + start, *end = iter + count;
      for (; iter != end; ++iter) {
        elem_traits::Destruct(iter);
      }
    }

    // This method invokes elem_type's copy-constructor on a range of elements.
    // @param start   The index of the first element to construct.
    // @param count   The number of elements to construct. 
    // @param values  The array of elements to copy. 
    template<class Item>
    void AssignRange(index_type start, size_type count,
                     const Item *values) {
      elem_type *iter = Elements() + start, *end = iter + count;
      for (; iter != end; ++iter, ++values) {
        elem_traits::Construct(iter, *values);
      }
    }
};

#endif  // nsTArray_h__
