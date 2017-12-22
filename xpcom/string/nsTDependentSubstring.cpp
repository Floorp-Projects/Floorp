/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTDependentSubstring.h"

template <typename T>
void
nsTDependentSubstring<T>::Rebind(const substring_type& str,
                                    uint32_t startPos, uint32_t length)
{
  // If we currently own a buffer, release it.
  this->Finalize();

  size_type strLength = str.Length();

  if (startPos > strLength) {
    startPos = strLength;
  }

  char_type* newData =
    const_cast<char_type*>(static_cast<const char_type*>(str.Data())) + startPos;
  size_type newLength = XPCOM_MIN(length, strLength - startPos);
  DataFlags newDataFlags = DataFlags(0);
  this->SetData(newData, newLength, newDataFlags);
}

template <typename T>
void
nsTDependentSubstring<T>::Rebind(const char_type* data, size_type length)
{
  NS_ASSERTION(data, "nsTDependentSubstring must wrap a non-NULL buffer");

  // If we currently own a buffer, release it.
  this->Finalize();

  char_type* newData =
    const_cast<char_type*>(static_cast<const char_type*>(data));
  size_type newLength = length;
  DataFlags newDataFlags = DataFlags(0);
  this->SetData(newData, newLength, newDataFlags);
}

template <typename T>
void
nsTDependentSubstring<T>::Rebind(const char_type* aStart, const char_type* aEnd)
{
  MOZ_RELEASE_ASSERT(aStart <= aEnd, "Overflow!");
  this->Rebind(aStart, size_type(aEnd - aStart));
}

template <typename T>
nsTDependentSubstring<T>::nsTDependentSubstring(const char_type* aStart,
                                                const char_type* aEnd)
  : substring_type(const_cast<char_type*>(aStart), uint32_t(aEnd - aStart),
                   DataFlags(0), ClassFlags(0))
{
  MOZ_RELEASE_ASSERT(aStart <= aEnd, "Overflow!");
}

#if defined(MOZ_USE_CHAR16_WRAPPER)
template <typename T>
template <typename Q, typename EnableIfChar16>
nsTDependentSubstring<T>::nsTDependentSubstring(char16ptr_t aStart,
                                                char16ptr_t aEnd)
  : substring_type(static_cast<const char16_t*>(aStart),
                   static_cast<const char16_t*>(aEnd))
{
  MOZ_RELEASE_ASSERT(static_cast<const char16_t*>(aStart) <=
                     static_cast<const char16_t*>(aEnd),
                     "Overflow!");
}
#endif

template <typename T>
nsTDependentSubstring<T>::nsTDependentSubstring(const const_iterator& aStart,
                                                const const_iterator& aEnd)
  : substring_type(const_cast<char_type*>(aStart.get()),
                   uint32_t(aEnd.get() - aStart.get()),
                   DataFlags(0), ClassFlags(0))
{
  MOZ_RELEASE_ASSERT(aStart.get() <= aEnd.get(), "Overflow!");
}

template <typename T>
const nsTDependentSubstring<T>
Substring(const T* aStart, const T* aEnd)
{
  MOZ_RELEASE_ASSERT(aStart <= aEnd, "Overflow!");
  return nsTDependentSubstring<T>(aStart, aEnd);
}

#if defined(MOZ_USE_CHAR16_WRAPPER)
const nsTDependentSubstring<char16_t>
Substring(char16ptr_t aData, uint32_t aLength)
{
  return nsTDependentSubstring<char16_t>(aData, aLength);
}

const nsTDependentSubstring<char16_t>
Substring(char16ptr_t aStart, char16ptr_t aEnd)
{
  return Substring(static_cast<const char16_t*>(aStart),
                   static_cast<const char16_t*>(aEnd));
}
#endif
