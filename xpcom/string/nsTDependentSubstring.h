/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsString.h"

#ifndef nsTDependentSubstring_h
#define nsTDependentSubstring_h

#include "nsTSubstring.h"
#include "nsTLiteralString.h"
#include "mozilla/Span.h"

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
template <typename T>
class nsTDependentSubstring : public nsTSubstring<T> {
 public:
  typedef nsTDependentSubstring<T> self_type;
  typedef nsTSubstring<T> substring_type;
  typedef typename substring_type::fallible_t fallible_t;

  typedef typename substring_type::char_type char_type;
  typedef typename substring_type::char_traits char_traits;
  typedef
      typename substring_type::incompatible_char_type incompatible_char_type;

  typedef typename substring_type::substring_tuple_type substring_tuple_type;

  typedef typename substring_type::const_iterator const_iterator;
  typedef typename substring_type::iterator iterator;

  typedef typename substring_type::comparator_type comparator_type;

  typedef typename substring_type::const_char_iterator const_char_iterator;

  typedef typename substring_type::string_view string_view;

  typedef typename substring_type::index_type index_type;
  typedef typename substring_type::size_type size_type;

  // These are only for internal use within the string classes:
  typedef typename substring_type::DataFlags DataFlags;
  typedef typename substring_type::ClassFlags ClassFlags;

 public:
  void Rebind(const substring_type&, size_type aStartPos,
              size_type aLength = size_type(-1));

  void Rebind(const char_type* aData, size_type aLength);

  void Rebind(const char_type* aStart, const char_type* aEnd);

  nsTDependentSubstring(const substring_type& aStr, size_type aStartPos,
                        size_type aLength = size_type(-1))
      : substring_type() {
    Rebind(aStr, aStartPos, aLength);
  }

  nsTDependentSubstring(const char_type* aData, size_type aLength)
      : substring_type(const_cast<char_type*>(aData), aLength, DataFlags(0),
                       ClassFlags(0)) {}

  explicit nsTDependentSubstring(mozilla::Span<const char_type> aData)
      : nsTDependentSubstring(aData.Elements(), aData.Length()) {}

  nsTDependentSubstring(const char_type* aStart, const char_type* aEnd);

#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  nsTDependentSubstring(char16ptr_t aData, size_type aLength)
      : nsTDependentSubstring(static_cast<const char16_t*>(aData), aLength) {}

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  nsTDependentSubstring(char16ptr_t aStart, char16ptr_t aEnd);
#endif

  nsTDependentSubstring(const const_iterator& aStart,
                        const const_iterator& aEnd);

  // Create a nsTDependentSubstring to be bound later
  nsTDependentSubstring() : substring_type() {}

  // auto-generated copy-constructor OK (XXX really?? what about base class
  // copy-ctor?)
  nsTDependentSubstring(const nsTDependentSubstring&) = default;

 private:
  // NOT USED
  void operator=(const self_type&) =
      delete;  // we're immutable, you can't assign into a substring
};

extern template class nsTDependentSubstring<char>;
extern template class nsTDependentSubstring<char16_t>;

template <typename T>
inline const nsTDependentSubstring<T> Substring(const nsTSubstring<T>& aStr,
                                                size_t aStartPos,
                                                size_t aLength = size_t(-1)) {
  return nsTDependentSubstring<T>(aStr, aStartPos, aLength);
}

template <typename T>
inline const nsTDependentSubstring<T> Substring(const nsTLiteralString<T>& aStr,
                                                size_t aStartPos,
                                                size_t aLength = size_t(-1)) {
  return nsTDependentSubstring<T>(aStr, aStartPos, aLength);
}

template <typename T>
inline const nsTDependentSubstring<T> Substring(
    const nsReadingIterator<T>& aStart, const nsReadingIterator<T>& aEnd) {
  return nsTDependentSubstring<T>(aStart.get(), aEnd.get());
}

template <typename T>
inline const nsTDependentSubstring<T> Substring(const T* aData,
                                                size_t aLength) {
  return nsTDependentSubstring<T>(aData, aLength);
}

template <typename T>
const nsTDependentSubstring<T> Substring(const T* aStart, const T* aEnd);

extern template const nsTDependentSubstring<char> Substring(const char* aStart,
                                                            const char* aEnd);

extern template const nsTDependentSubstring<char16_t> Substring(
    const char16_t* aStart, const char16_t* aEnd);

#if defined(MOZ_USE_CHAR16_WRAPPER)
inline const nsTDependentSubstring<char16_t> Substring(char16ptr_t aData,
                                                       size_t aLength);

const nsTDependentSubstring<char16_t> Substring(char16ptr_t aStart,
                                                char16ptr_t aEnd);
#endif

template <typename T>
inline const nsTDependentSubstring<T> StringHead(const nsTSubstring<T>& aStr,
                                                 size_t aCount) {
  return nsTDependentSubstring<T>(aStr, 0, aCount);
}

template <typename T>
inline const nsTDependentSubstring<T> StringTail(const nsTSubstring<T>& aStr,
                                                 size_t aCount) {
  return nsTDependentSubstring<T>(aStr, aStr.Length() - aCount, aCount);
}

#endif
