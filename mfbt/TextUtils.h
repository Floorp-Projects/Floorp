/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Character/text operations. */

#ifndef mozilla_TextUtils_h
#define mozilla_TextUtils_h

#include "mozilla/Assertions.h"
#include "mozilla/TypeTraits.h"

namespace mozilla {

namespace detail {

template<typename Char>
class MakeUnsignedChar
  : public MakeUnsigned<Char>
{};

template<>
class MakeUnsignedChar<char16_t>
{
public:
  using Type = char16_t;
};

template<>
class MakeUnsignedChar<char32_t>
{
public:
  using Type = char32_t;
};

} // namespace detail

/**
 * Returns true iff |aChar| matches [a-z].
 *
 * This function is basically what you thought islower was, except its behavior
 * doesn't depend on the user's current locale.
 */
template<typename Char>
constexpr bool
IsAsciiLowercaseAlpha(Char aChar)
{
  using UnsignedChar = typename detail::MakeUnsignedChar<Char>::Type;
  auto uc = static_cast<UnsignedChar>(aChar);
  return 'a' <= uc && uc <= 'z';
}

/**
 * Returns true iff |aChar| matches [A-Z].
 *
 * This function is basically what you thought isupper was, except its behavior
 * doesn't depend on the user's current locale.
 */
template<typename Char>
constexpr bool
IsAsciiUppercaseAlpha(Char aChar)
{
  using UnsignedChar = typename detail::MakeUnsignedChar<Char>::Type;
  auto uc = static_cast<UnsignedChar>(aChar);
  return 'A' <= uc && uc <= 'Z';
}

/**
 * Returns true iff |aChar| matches [a-zA-Z].
 *
 * This function is basically what you thought isalpha was, except its behavior
 * doesn't depend on the user's current locale.
 */
template<typename Char>
constexpr bool
IsAsciiAlpha(Char aChar)
{
  return IsAsciiLowercaseAlpha(aChar) || IsAsciiUppercaseAlpha(aChar);
}

/**
 * Returns true iff |aChar| matches [0-9].
 *
 * This function is basically what you thought isdigit was, except its behavior
 * doesn't depend on the user's current locale.
 */
template<typename Char>
constexpr bool
IsAsciiDigit(Char aChar)
{
  using UnsignedChar = typename detail::MakeUnsignedChar<Char>::Type;
  auto uc = static_cast<UnsignedChar>(aChar);
  return '0' <= uc && uc <= '9';
}

/**
 * Returns true iff |aChar| matches [a-zA-Z0-9].
 *
 * This function is basically what you thought isalnum was, except its behavior
 * doesn't depend on the user's current locale.
 */
template<typename Char>
constexpr bool
IsAsciiAlphanumeric(Char aChar)
{
  return IsAsciiDigit(aChar) || IsAsciiAlpha(aChar);
}

/**
 * Converts an ASCII alphanumeric digit [0-9a-zA-Z] to number as if in base-36.
 * (This function therefore works for decimal, hexadecimal, etc.).
 */
template<typename Char>
uint8_t
AsciiAlphanumericToNumber(Char aChar)
{
  using UnsignedChar = typename detail::MakeUnsignedChar<Char>::Type;
  auto uc = static_cast<UnsignedChar>(aChar);

  if ('0' <= uc && uc <= '9') {
    return uc - '0';
  }

  if ('A' <= uc && uc <= 'Z') {
    return uc - 'A' + 10;
  }

  // Ideally this function would be constexpr, but unfortunately gcc at least as
  // of 6.4 forbids non-constexpr function calls in unevaluated constexpr
  // function calls.  See bug 1453456.  So for now, just assert and leave the
  // entire function non-constexpr.
  MOZ_ASSERT('a' <= uc && uc <= 'z',
             "non-ASCII alphanumeric character can't be converted to number");
  return uc - 'a' + 10;
}

} // namespace mozilla

#endif /* mozilla_TextUtils_h */
