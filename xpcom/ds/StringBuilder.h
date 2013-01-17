/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* We would use std::max but MS makes it painful
// windef.h defines min and max macros that we don't want
// http://support.microsoft.com/kb/143208
#ifdef _WIN32
#define NOMINMAX
#endif
*/

#include <stdlib.h>
#include <string.h>
#include "nsAlgorithm.h"

/* This is a standard string builder like ones in Java
   or C#. It uses a doubling allocation strategy
   to grow when out of capacity.

   This does not use nsTArray because nsTArray starts
   growing by multiples of page size after it is the
   size of one page. We want to keep doubling in size
   so that we can continue to append at high speed even
   for large strings.

   Eventually, this should be templated for wide characters.

 */

namespace mozilla {

class StringBuilder
{
public:
    StringBuilder() {
        mCapacity = 16;
        mLength = 0;
        mBuffer = static_cast<char*>(malloc(sizeof(char)*mCapacity));
        mBuffer[0] = '\0';
    }

    void Append(const char *s) {
        size_t newLength = strlen(s);

        EnsureCapacity(mLength + newLength+1);

        // copy the entire string including the null terminator
        memcpy(&mBuffer[mLength], s, newLength+1);
        mLength += newLength;
    }

    char *Buffer() {
        return mBuffer;
    }

    size_t Length() {
        return mLength;
    }

    size_t EnsureCapacity(size_t capacity) {
        if (capacity > mCapacity) {
            // make sure we at least double in size
            mCapacity = XPCOM_MAX(capacity, mCapacity*2);
            mBuffer = static_cast<char*>(realloc(mBuffer, mCapacity));
            mCapacity = moz_malloc_usable_size(mBuffer);
        }
        return mCapacity;
    }

    ~StringBuilder()
    {
        free(mBuffer);
    }

private:
    char *mBuffer;
    size_t mLength; // the length of the contained string not including the null terminator
    size_t mCapacity; // the total size of mBuffer
};

}
