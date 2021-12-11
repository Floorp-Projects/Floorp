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

#ifndef nsHtml5ArrayCopy_h
#define nsHtml5ArrayCopy_h

class nsHtml5StackNode;

// Unfortunately, these don't work as template functions because the arguments
// would need coercion from a template class, which complicates things.
class nsHtml5ArrayCopy {
 public:
  static inline void arraycopy(char16_t* source, int32_t sourceOffset,
                               char16_t* target, int32_t targetOffset,
                               int32_t length) {
    memcpy(&(target[targetOffset]), &(source[sourceOffset]),
           size_t(length) * sizeof(char16_t));
  }

  static inline void arraycopy(char16_t* source, char16_t* target,
                               int32_t length) {
    memcpy(target, source, size_t(length) * sizeof(char16_t));
  }

  static inline void arraycopy(int32_t* source, int32_t* target,
                               int32_t length) {
    memcpy(target, source, size_t(length) * sizeof(int32_t));
  }

  static inline void arraycopy(nsHtml5StackNode** source,
                               nsHtml5StackNode** target, int32_t length) {
    memcpy(target, source, size_t(length) * sizeof(nsHtml5StackNode*));
  }

  static inline void arraycopy(nsHtml5StackNode** arr, int32_t sourceOffset,
                               int32_t targetOffset, int32_t length) {
    memmove(&(arr[targetOffset]), &(arr[sourceOffset]),
            size_t(length) * sizeof(nsHtml5StackNode*));
  }
};
#endif  // nsHtml5ArrayCopy_h
