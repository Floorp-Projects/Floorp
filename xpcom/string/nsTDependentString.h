/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTDependentString_h
#define nsTDependentString_h

#include "nsTString.h"

/**
 * nsTDependentString
 *
 * Stores a null-terminated, immutable sequence of characters.
 *
 * Subclass of nsTString that restricts string value to an immutable
 * character sequence.  This class does not own its data, so the creator
 * of objects of this type must take care to ensure that a
 * nsTDependentString continues to reference valid memory for the
 * duration of its use.
 */
template <typename T>
class nsTDependentString : public nsTString<T> {
 public:
  typedef nsTDependentString<T> self_type;
  typedef nsTString<T> base_string_type;
  typedef typename base_string_type::string_type string_type;

  typedef typename base_string_type::fallible_t fallible_t;

  typedef typename base_string_type::char_type char_type;
  typedef typename base_string_type::char_traits char_traits;
  typedef
      typename base_string_type::incompatible_char_type incompatible_char_type;

  typedef typename base_string_type::substring_tuple_type substring_tuple_type;

  typedef typename base_string_type::const_iterator const_iterator;
  typedef typename base_string_type::iterator iterator;

  typedef typename base_string_type::comparator_type comparator_type;

  typedef typename base_string_type::const_char_iterator const_char_iterator;

  typedef typename base_string_type::index_type index_type;
  typedef typename base_string_type::size_type size_type;

  // These are only for internal use within the string classes:
  typedef typename base_string_type::DataFlags DataFlags;
  typedef typename base_string_type::ClassFlags ClassFlags;

 public:
  /**
   * constructors
   */

  nsTDependentString(const char_type* aStart, const char_type* aEnd);

  nsTDependentString(const char_type* aData, size_type aLength)
      : string_type(const_cast<char_type*>(aData), aLength,
                    DataFlags::TERMINATED, ClassFlags(0)) {
    this->AssertValidDependentString();
  }

#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  nsTDependentString(char16ptr_t aData, size_type aLength)
      : nsTDependentString(static_cast<const char16_t*>(aData), aLength) {}
#endif

  explicit nsTDependentString(const char_type* aData)
      : string_type(const_cast<char_type*>(aData), char_traits::length(aData),
                    DataFlags::TERMINATED, ClassFlags(0)) {
    string_type::AssertValidDependentString();
  }

#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  explicit nsTDependentString(char16ptr_t aData)
      : nsTDependentString(static_cast<const char16_t*>(aData)) {}
#endif

  nsTDependentString(const string_type& aStr, index_type aStartPos)
      : string_type() {
    Rebind(aStr, aStartPos);
  }

  // Create a nsTDependentSubstring to be bound later
  nsTDependentString() : string_type() {}

  // auto-generated destructor OK

  nsTDependentString(self_type&& aStr) : string_type() {
    Rebind(aStr, /* aStartPos = */ 0);
    aStr.SetToEmptyBuffer();
  }

  explicit nsTDependentString(const self_type& aStr) : string_type() {
    Rebind(aStr, /* aStartPos = */ 0);
  }

  /**
   * allow this class to be bound to a different string...
   */

  using nsTString<T>::Rebind;
  void Rebind(const char_type* aData) {
    Rebind(aData, char_traits::length(aData));
  }

  void Rebind(const char_type* aStart, const char_type* aEnd);
  void Rebind(const string_type&, index_type aStartPos);

 private:
  // NOT USED
  nsTDependentString(const substring_tuple_type&) = delete;
  self_type& operator=(const self_type& aStr) = delete;
};

extern template class nsTDependentString<char>;
extern template class nsTDependentString<char16_t>;

#endif
