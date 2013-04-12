/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5DependentUTF16Buffer_h_
#define nsHtml5DependentUTF16Buffer_h_

#include "nscore.h"
#include "nsHtml5OwningUTF16Buffer.h"

class MOZ_STACK_CLASS nsHtml5DependentUTF16Buffer : public nsHtml5UTF16Buffer
{
  public:
    /**
     * Wraps a string without taking ownership of the buffer. aToWrap MUST NOT
     * go away or be shortened while nsHtml5DependentUTF16Buffer is in use.
     */
    nsHtml5DependentUTF16Buffer(const nsAString& aToWrap);

    ~nsHtml5DependentUTF16Buffer();

    /**
     * Copies the currently unconsumed part of this buffer into a new
     * heap-allocated nsHtml5OwningUTF16Buffer. The new object is allocated
     * with a fallible allocator. If the allocation fails, nullptr is returned.
     * @return heap-allocated copy or nullptr if memory allocation failed
     */
    already_AddRefed<nsHtml5OwningUTF16Buffer> FalliblyCopyAsOwningBuffer();
};

#endif // nsHtml5DependentUTF16Buffer_h_
