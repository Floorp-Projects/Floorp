/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTLiteralString_h
#define nsTLiteralString_h

#include "nsTStringRepr.h"

/**
 * nsTLiteralString_CharT
 *
 * Stores a null-terminated, immutable sequence of characters.
 *
 * nsTString-lookalike that restricts its string value to a literal character
 * sequence. Can be implicitly cast to const nsTString& (the const is
 * essential, since this class's data are not writable). The data are assumed
 * to be static (permanent) and therefore, as an optimization, this class
 * does not have a destructor.
 */
template<typename T>
class nsTLiteralString : public mozilla::detail::nsTStringRepr<T>
{
public:

  typedef nsTLiteralString<T> self_type;

#ifdef __clang__
  // bindgen w/ clang 3.9 at least chokes on a typedef, but using is okay.
  using typename mozilla::detail::nsTStringRepr<T>::base_string_type;
#else
  // On the other hand msvc chokes on the using statement. It seems others
  // don't care either way so we lump them in here.
  typedef typename mozilla::detail::nsTStringRepr<T>::base_string_type base_string_type;
#endif

  typedef typename base_string_type::char_type char_type;
  typedef typename base_string_type::size_type size_type;
  typedef typename base_string_type::DataFlags DataFlags;
  typedef typename base_string_type::ClassFlags ClassFlags;

public:

  /**
   * constructor
   */

  template<size_type N>
  explicit constexpr nsTLiteralString(const char_type (&aStr)[N])
    : base_string_type(const_cast<char_type*>(aStr), N - 1,
                       DataFlags::TERMINATED | DataFlags::LITERAL,
                       ClassFlags::NULL_TERMINATED)
  {
  }

  /**
   * For compatibility with existing code that requires const ns[C]String*.
   * Use sparingly. If possible, rewrite code to use const ns[C]String&
   * and the implicit cast will just work.
   */
  const nsTString<T>& AsString() const
  {
    return *reinterpret_cast<const nsTString<T>*>(this);
  }

  operator const nsTString<T>&() const
  {
    return AsString();
  }

  template<typename N, typename Dummy> struct raw_type { typedef N* type; };

#ifdef MOZ_USE_CHAR16_WRAPPER
  template<typename Dummy> struct raw_type<char16_t, Dummy> { typedef char16ptr_t type; };
#endif

  /**
   * Prohibit get() on temporaries as in nsLiteralCString("x").get().
   * These should be written as just "x", using a string literal directly.
   */
  const typename raw_type<T, int>::type get() const && = delete;
  const typename raw_type<T, int>::type get() const &
  {
    return this->mData;
  }

private:

  // NOT TO BE IMPLEMENTED
  template<size_type N>
  nsTLiteralString(char_type (&aStr)[N]) = delete;

  self_type& operator=(const self_type&) = delete;
};

extern template class nsTLiteralString<char>;
extern template class nsTLiteralString<char16_t>;

#endif
