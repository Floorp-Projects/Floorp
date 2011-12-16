/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cindent: */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Jeff Muizelaar <jmuizelaar@mozilla.com>
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
            mCapacity = NS_MAX(capacity, mCapacity*2);
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
