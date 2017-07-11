/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsString.h"

/**
 * nsTDependentSubstring_CharT
 *
 * A string class which wraps an external array of string characters. It
 * is the client code's responsibility to ensure that the external buffer
 * remains valid for a long as the string is alive.
 *
 * NAMES:
 *   nsDependentSubstring for wide characters
 *   nsDependentCSubstring for narrow characters
 */
class nsTDependentSubstring_CharT : public nsTSubstring_CharT
{
public:

  typedef nsTDependentSubstring_CharT self_type;

public:

  void Rebind(const substring_type&, uint32_t aStartPos,
              uint32_t aLength = size_type(-1));

  void Rebind(const char_type* aData, size_type aLength);

  void Rebind(const char_type* aStart, const char_type* aEnd);

  nsTDependentSubstring_CharT(const substring_type& aStr, uint32_t aStartPos,
                              uint32_t aLength = size_type(-1))
    : substring_type()
  {
    Rebind(aStr, aStartPos, aLength);
  }

  nsTDependentSubstring_CharT(const char_type* aData, size_type aLength)
    : substring_type(const_cast<char_type*>(aData), aLength,
                     DataFlags(0), ClassFlags(0))
  {
  }

  nsTDependentSubstring_CharT(const char_type* aStart, const char_type* aEnd);

#if defined(CharT_is_PRUnichar) && defined(MOZ_USE_CHAR16_WRAPPER)
  nsTDependentSubstring_CharT(char16ptr_t aData, size_type aLength)
    : nsTDependentSubstring_CharT(static_cast<const char16_t*>(aData), aLength)
  {
  }

  nsTDependentSubstring_CharT(char16ptr_t aStart, char16ptr_t aEnd);
#endif

  nsTDependentSubstring_CharT(const const_iterator& aStart,
                              const const_iterator& aEnd);

  // Create a nsTDependentSubstring to be bound later
  nsTDependentSubstring_CharT()
    : substring_type()
  {
  }

  // auto-generated copy-constructor OK (XXX really?? what about base class copy-ctor?)

private:
  // NOT USED
  void operator=(const self_type&);  // we're immutable, you can't assign into a substring
};

inline const nsTDependentSubstring_CharT
Substring(const nsTSubstring_CharT& aStr, uint32_t aStartPos,
          uint32_t aLength = uint32_t(-1))
{
  return nsTDependentSubstring_CharT(aStr, aStartPos, aLength);
}

inline const nsTDependentSubstring_CharT
Substring(const nsReadingIterator<CharT>& aStart,
          const nsReadingIterator<CharT>& aEnd)
{
  return nsTDependentSubstring_CharT(aStart.get(), aEnd.get());
}

inline const nsTDependentSubstring_CharT
Substring(const CharT* aData, uint32_t aLength)
{
  return nsTDependentSubstring_CharT(aData, aLength);
}

const nsTDependentSubstring_CharT
Substring(const CharT* aStart, const CharT* aEnd);

inline const nsTDependentSubstring_CharT
StringHead(const nsTSubstring_CharT& aStr, uint32_t aCount)
{
  return nsTDependentSubstring_CharT(aStr, 0, aCount);
}

inline const nsTDependentSubstring_CharT
StringTail(const nsTSubstring_CharT& aStr, uint32_t aCount)
{
  return nsTDependentSubstring_CharT(aStr, aStr.Length() - aCount, aCount);
}
