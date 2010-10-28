/*
 * Copyright (c) 2008 Mozilla Foundation
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

#ifndef jArray_h__
#define jArray_h__

#define J_ARRAY_STATIC(T, L, arr) \
  jArray<T,L>( ((T*)arr), (sizeof(arr)/sizeof(arr[0])) )

template<class T, class L>
class jArray {
  private:
    T* arr;
  public:
    L length;
    jArray(T* const a, L const len);
    jArray(L const len);
    jArray(const jArray<T,L>& other);
    jArray();
    operator T*() { return arr; }
    T& operator[] (L const index) { return arr[index]; }
    void release() { delete[] arr; arr = 0; length = 0; }
    L binarySearch(T const elem);
};

template<class T, class L>
jArray<T,L>::jArray(T* const a, L const len)
       : arr(a), length(len)
{
}

template<class T, class L>
jArray<T,L>::jArray(L const len)
       : arr(len ? new T[len] : 0), length(len)
{
}

template<class T, class L>
jArray<T,L>::jArray(const jArray<T,L>& other)
       : arr(other.arr), length(other.length)
{
}

template<class T, class L>
jArray<T,L>::jArray()
       : arr(0), length(0)
{
}

template<class T, class L>
L
jArray<T,L>::binarySearch(T const elem)
{
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

#endif // jArray_h__
