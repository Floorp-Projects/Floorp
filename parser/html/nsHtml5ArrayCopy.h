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

#ifndef nsHtml5ArrayCopy_h__
#define nsHtml5ArrayCopy_h__

#include "prtypes.h"

class nsString;
class nsHtml5StackNode;
class nsHtml5AttributeName;

// Unfortunately, these don't work as template functions because the arguments
// would need coercion from a template class, which complicates things.
class nsHtml5ArrayCopy {
  public:

    static inline void
    arraycopy(PRUnichar* source, int32_t sourceOffset, PRUnichar* target, int32_t targetOffset, int32_t length)
    {
      memcpy(&(target[targetOffset]), &(source[sourceOffset]), length * sizeof(PRUnichar));
    }

    static inline void
    arraycopy(PRUnichar* source, PRUnichar* target, int32_t length)
    {
      memcpy(target, source, length * sizeof(PRUnichar));
    }

    static inline void
    arraycopy(int32_t* source, int32_t* target, int32_t length)
    {
      memcpy(target, source, length * sizeof(int32_t));
    }

    static inline void
    arraycopy(nsString** source, nsString** target, int32_t length)
    {
      memcpy(target, source, length * sizeof(nsString*));
    }

    static inline void
    arraycopy(nsHtml5AttributeName** source, nsHtml5AttributeName** target, int32_t length)
    {
      memcpy(target, source, length * sizeof(nsHtml5AttributeName*));
    }

    static inline void
    arraycopy(nsHtml5StackNode** source, nsHtml5StackNode** target, int32_t length)
    {
      memcpy(target, source, length * sizeof(nsHtml5StackNode*));
    }

    static inline void
    arraycopy(nsHtml5StackNode** arr, int32_t sourceOffset, int32_t targetOffset, int32_t length)
    {
      memmove(&(arr[targetOffset]), &(arr[sourceOffset]), length * sizeof(nsHtml5StackNode*));
    }
};
#endif // nsHtml5ArrayCopy_h__
