/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5DependentUTF16Buffer.h"

nsHtml5DependentUTF16Buffer::nsHtml5DependentUTF16Buffer(const nsAString& aToWrap)
  : nsHtml5UTF16Buffer(const_cast<PRUnichar*> (aToWrap.BeginReading()),
                       aToWrap.Length())
{
  MOZ_COUNT_CTOR(nsHtml5DependentUTF16Buffer);
}

nsHtml5DependentUTF16Buffer::~nsHtml5DependentUTF16Buffer()
{
  MOZ_COUNT_DTOR(nsHtml5DependentUTF16Buffer);
}

already_AddRefed<nsHtml5OwningUTF16Buffer>
nsHtml5DependentUTF16Buffer::FalliblyCopyAsOwningBuffer()
{
  PRInt32 newLength = getEnd() - getStart();
  nsRefPtr<nsHtml5OwningUTF16Buffer> newObj =
    nsHtml5OwningUTF16Buffer::FalliblyCreate(newLength);
  if (!newObj) {
    return nullptr;
  }
  newObj->setEnd(newLength);
  memcpy(newObj->getBuffer(),
         getBuffer() + getStart(),
         newLength * sizeof(PRUnichar));
  return newObj.forget();
}
