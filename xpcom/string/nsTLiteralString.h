/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
class nsTLiteralString_CharT : public mozilla::detail::nsTStringRepr_CharT
{
public:

  typedef nsTLiteralString_CharT self_type;

public:

  /**
   * constructor
   */

  template<size_type N>
  explicit nsTLiteralString_CharT(const char_type (&aStr)[N])
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
  const nsTString_CharT& AsString() const
  {
    return *reinterpret_cast<const nsTString_CharT*>(this);
  }

  operator const nsTString_CharT&() const
  {
    return AsString();
  }

  /**
   * Prohibit get() on temporaries as in nsLiteralCString("x").get().
   * These should be written as just "x", using a string literal directly.
   */
#if defined(CharT_is_PRUnichar) && defined(MOZ_USE_CHAR16_WRAPPER)
  char16ptr_t get() const && = delete;
  char16ptr_t get() const &
#else
  const char_type* get() const && = delete;
  const char_type* get() const &
#endif
  {
    return mData;
  }

private:

  // NOT TO BE IMPLEMENTED
  template<size_type N>
  nsTLiteralString_CharT(char_type (&aStr)[N]) = delete;

  self_type& operator=(const self_type&) = delete;
};

static_assert(sizeof(nsTLiteralString_CharT) == sizeof(nsTString_CharT),
              "nsTLiteralString_CharT can masquerade as nsTString_CharT, "
              "so they must have identical layout");
