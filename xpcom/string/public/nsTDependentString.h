/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * nsTDependentString_CharT
 *
 * Stores a null-terminated, immutable sequence of characters.
 *
 * Subclass of nsTString that restricts string value to an immutable
 * character sequence.  This class does not own its data, so the creator
 * of objects of this type must take care to ensure that a
 * nsTDependentString continues to reference valid memory for the
 * duration of its use.
 */
class nsTDependentString_CharT : public nsTString_CharT
{
public:

  typedef nsTDependentString_CharT self_type;

public:

  /**
   * constructors
   */

  nsTDependentString_CharT(const char_type* aStart, const char_type* aEnd)
    : string_type(const_cast<char_type*>(aStart),
                  uint32_t(aEnd - aStart), F_TERMINATED)
  {
    AssertValidDepedentString();
  }

  nsTDependentString_CharT(const char_type* aData, uint32_t aLength)
    : string_type(const_cast<char_type*>(aData), aLength, F_TERMINATED)
  {
    AssertValidDepedentString();
  }

#if defined(CharT_is_PRUnichar) && defined(MOZ_USE_CHAR16_WRAPPER)
  nsTDependentString_CharT(char16ptr_t aData, uint32_t aLength)
    : nsTDependentString_CharT(static_cast<const char16_t*>(aData), aLength)
  {
  }
#endif

  explicit
  nsTDependentString_CharT(const char_type* aData)
    : string_type(const_cast<char_type*>(aData),
                  uint32_t(char_traits::length(aData)), F_TERMINATED)
  {
    AssertValidDepedentString();
  }

#if defined(CharT_is_PRUnichar) && defined(MOZ_USE_CHAR16_WRAPPER)
  explicit
  nsTDependentString_CharT(char16ptr_t aData)
    : nsTDependentString_CharT(static_cast<const char16_t*>(aData))
  {
  }
#endif

  nsTDependentString_CharT(const string_type& aStr, uint32_t aStartPos)
    : string_type()
  {
    Rebind(aStr, aStartPos);
  }

  // Create a nsTDependentSubstring to be bound later
  nsTDependentString_CharT()
    : string_type()
  {
  }

  // XXX are you sure??
  // auto-generated copy-constructor OK
  // auto-generated copy-assignment operator OK
  // auto-generated destructor OK


  /**
   * allow this class to be bound to a different string...
   */

  using nsTString_CharT::Rebind;
  void Rebind(const char_type* aData)
  {
    Rebind(aData, uint32_t(char_traits::length(aData)));
  }

  void Rebind(const char_type* aStart, const char_type* aEnd)
  {
    Rebind(aStart, uint32_t(aEnd - aStart));
  }

  void Rebind(const string_type&, uint32_t aStartPos);

private:

  // NOT USED
  nsTDependentString_CharT(const substring_tuple_type&) MOZ_DELETE;
};
