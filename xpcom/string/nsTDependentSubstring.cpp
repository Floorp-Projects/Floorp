/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void
nsTDependentSubstring_CharT::Rebind(const substring_type& str,
                                    uint32_t startPos, uint32_t length)
{
  // If we currently own a buffer, release it.
  Finalize();

  size_type strLength = str.Length();

  if (startPos > strLength) {
    startPos = strLength;
  }

  mData = const_cast<char_type*>(static_cast<const char_type*>(str.Data())) + startPos;
  mLength = XPCOM_MIN(length, strLength - startPos);
  mDataFlags = DataFlags(0);
}

void
nsTDependentSubstring_CharT::Rebind(const char_type* data, size_type length)
{
  NS_ASSERTION(data, "nsTDependentSubstring must wrap a non-NULL buffer");

  // If we currently own a buffer, release it.
  Finalize();

  mData = const_cast<char_type*>(static_cast<const char_type*>(data));
  mLength = length;
  mDataFlags = DataFlags(0);
}

void
nsTDependentSubstring_CharT::Rebind(const char_type* aStart, const char_type* aEnd)
{
  MOZ_RELEASE_ASSERT(aStart <= aEnd, "Overflow!");
  Rebind(aStart, size_type(aEnd - aStart));
}

nsTDependentSubstring_CharT::nsTDependentSubstring_CharT(const char_type* aStart,
                                                         const char_type* aEnd)
  : substring_type(const_cast<char_type*>(aStart), uint32_t(aEnd - aStart),
                   DataFlags(0), ClassFlags(0))
{
  MOZ_RELEASE_ASSERT(aStart <= aEnd, "Overflow!");
}

#if defined(CharT_is_PRUnichar) && defined(MOZ_USE_CHAR16_WRAPPER)
nsTDependentSubstring_CharT::nsTDependentSubstring_CharT(char16ptr_t aStart,
                                                         char16ptr_t aEnd)
  : nsTDependentSubstring_CharT(static_cast<const char16_t*>(aStart),
                                static_cast<const char16_t*>(aEnd))
{
  MOZ_RELEASE_ASSERT(static_cast<const char16_t*>(aStart) <=
                     static_cast<const char16_t*>(aEnd),
                     "Overflow!");
}
#endif

nsTDependentSubstring_CharT::nsTDependentSubstring_CharT(const const_iterator& aStart,
                                                         const const_iterator& aEnd)
  : substring_type(const_cast<char_type*>(aStart.get()),
                   uint32_t(aEnd.get() - aStart.get()),
                   DataFlags(0), ClassFlags(0))
{
  MOZ_RELEASE_ASSERT(aStart.get() <= aEnd.get(), "Overflow!");
}

const nsTDependentSubstring_CharT
Substring(const CharT* aStart, const CharT* aEnd)
{
  MOZ_RELEASE_ASSERT(aStart <= aEnd, "Overflow!");
  return nsTDependentSubstring_CharT(aStart, aEnd);
}
