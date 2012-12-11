/*
 * Copyright (c) 2008-2010 Mozilla Foundation
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

#ifndef jArray_h_
#define jArray_h_

#include "mozilla/NullPtr.h"
#include "nsDebug.h"

template<class T, class L>
struct staticJArray {
  const T* arr;
  const L length;
  operator T*() { return arr; }
  T& operator[] (L const index) { return ((T*)arr)[index]; }
  L binarySearch(T const elem) {
    L lo = 0;
    L hi = length - 1;
    while (lo <= hi) {
      L mid = (lo + hi) / 2;
      if (arr[mid] > elem) {
        hi = mid - 1;
      } else if (arr[mid] < elem) {
        lo = mid + 1;
      } else {
        return mid;
      }
    }
    return -1;
  }
};

template<class T, class L>
struct jArray {
  T* arr;
  L length;
  static jArray<T,L> newJArray(L const len) {
    NS_ASSERTION(len >= 0, "Bad length.");
    jArray<T,L> newArray = { new T[len], len };
    return newArray;
  }
  operator T*() { return arr; }
  T& operator[] (L const index) { return arr[index]; }
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
    autoJArray(const jArray<T,L>& other)
     : arr(other.arr)
     , length(other.length)
    {
    }
    ~autoJArray()
    {
      delete[] arr;
    }
    operator T*() { return arr; }
    T& operator[] (L const index) { return arr[index]; }
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
#if defined(MOZ_HAVE_CXX11_NULLPTR)
#  if defined(__clang__) || defined(__ANDROID__)
    // clang on OS X 10.7 and gcc-4.6 on android does not have std::nullptr_t
    typedef decltype(nullptr) jArray_nullptr_t;
#  else
    // decltype(nullptr) does not evaluate to std::nullptr_t on GCC 4.6.3
    typedef std::nullptr_t jArray_nullptr_t;
#  endif
#elif defined(__GNUC__)
    typedef void* jArray_nullptr_t;
#elif defined(_WIN64)
    typedef uint64_t jArray_nullptr_t;
#else
    typedef uint32_t jArray_nullptr_t;
#endif
    void operator=(jArray_nullptr_t zero) {
      // Make assigning null to an array in Java delete the buffer in C++
      // MSVC10 does not allow asserting that zero is null.
      delete[] arr;
      arr = nullptr;
      length = 0;
    }
};

#endif // jArray_h_
