/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5OwningUTF16Buffer_h
#define nsHtml5OwningUTF16Buffer_h

#include "nsHtml5UTF16Buffer.h"

class nsHtml5OwningUTF16Buffer : public nsHtml5UTF16Buffer
{
  private:

    /**
     * Passes a buffer and its length to the superclass constructor.
     */
    nsHtml5OwningUTF16Buffer(char16_t* aBuffer);

  public:

    /**
     * Constructor for a parser key placeholder. (No actual buffer.)
     * @param aKey a parser key
     */
    nsHtml5OwningUTF16Buffer(void* aKey);

    /**
     * Takes care of releasing the owned buffer.
     */
    ~nsHtml5OwningUTF16Buffer();

    /**
     * The next buffer in a queue.
     */
    nsRefPtr<nsHtml5OwningUTF16Buffer> next;

    /**
     * A parser key.
     */
    void* key;

    static already_AddRefed<nsHtml5OwningUTF16Buffer>
    FalliblyCreate(int32_t aLength);

    /**
     * Swap start, end and buffer fields with another object.
     */
    void Swap(nsHtml5OwningUTF16Buffer* aOther);

    nsrefcnt AddRef();
    nsrefcnt Release();
  private:
    nsAutoRefCnt mRefCnt;
};

#endif // nsHtml5OwningUTF16Buffer_h
