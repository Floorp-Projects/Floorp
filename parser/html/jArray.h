/*
 * Copyright (c) 2008-2015 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef jArray_h
#define jArray_h

#include "mozilla/Attributes.h"
#include "mozilla/BinarySearch.h"
#include "nsDebug.h"

template<class T, class L>
struct staticJArray {
  const T* arr;
  const L length;
  operator T*() { return arr; }
  T& operator[] (L const index) {
    MOZ_ASSERT(index >= 0, "Array access with negative index.");
    MOZ_ASSERT(index < length, "Array index out of bounds.");
    return ((T*)arr)[index];
  }
  L binarySearch(T const elem) {
    size_t idx;
    bool found = mozilla::BinarySearch(arr, 0, length, elem, &idx);
    return found ? idx : -1;
  }
};

template<class T, class L>
struct jArray {
  T* arr;
  L length;
  static jArray<T,L> newJArray(L const len) {
    MOZ_ASSERT(len >= 0, "Negative length.");
    jArray<T,L> newArray = { new T[len], len };
    return newArray;
  }
  static jArray<T,L> newFallibleJArray(L const len) {
    MOZ_ASSERT(len >= 0, "Negative length.");
    T* a = new (mozilla::fallible) T[len];
    jArray<T,L> newArray = { a, a ? len : 0 };
    return newArray;
  }
  operator T*() { return arr; }
  T& operator[] (L const index) {
    MOZ_ASSERT(index >= 0, "Array access with negative index.");
    MOZ_ASSERT(index < length, "Array index out of bounds.");
    return arr[index];
  }
  void operator=(staticJArray<T,L>& other) {
    arr = (T*)other.arr;
    length = other.length;
  }
};

template<class T, class L>
class autoJArray {
  private:
    T* arr;
  public:
    L length;
    autoJArray()
     : arr(0)
     , length(0)
    {
    }
    MOZ_IMPLICIT autoJArray(const jArray<T,L>& other)
     : arr(other.arr)
     , length(other.length)
    {
    }
    ~autoJArray()
    {
      delete[] arr;
    }
    operator T*() { return arr; }
    T& operator[] (L const index) {
      MOZ_ASSERT(index >= 0, "Array access with negative index.");
      MOZ_ASSERT(index < length, "Array index out of bounds.");
      return arr[index];
    }
    operator jArray<T,L>() {
      // WARNING! This makes it possible to goof with buffer ownership!
      // This is needed for the getStack and getListOfActiveFormattingElements
      // methods to work sensibly.
      jArray<T,L> newArray = { arr, length };
      return newArray;
    }
    void operator=(const jArray<T,L>& other) {
      delete[] arr;
      arr = other.arr;
      length = other.length;
    }
    void operator=(decltype(nullptr)) {
      // Make assigning null to an array in Java delete the buffer in C++
      delete[] arr;
      arr = nullptr;
      length = 0;
    }
};

#endif // jArray_h
