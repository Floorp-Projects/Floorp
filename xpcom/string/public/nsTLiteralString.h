/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


  /**
   * nsTLiteralString_CharT
   *
   * Stores a null-terminated, immutable sequence of characters.
   *
   * Subclass of nsTString that restricts string value to a literal
   * character sequence.  This class does not own its data. The data is
   * assumed to be permanent. In practice this is true because this code
   * is only usable by and for libxul.
   */
class nsTLiteralString_CharT : public nsTString_CharT
  {
    public:

      typedef nsTLiteralString_CharT    self_type;

    public:

        /**
         * constructor
         */

      template<size_type N>
      nsTLiteralString_CharT( const char_type (&str)[N] )
        : string_type(const_cast<char_type*>(str), N - 1, F_TERMINATED | F_LITERAL)
        {
        }

    private:

        // NOT TO BE IMPLEMENTED
      template<size_type N>
      nsTLiteralString_CharT( char_type (&str)[N] ) MOZ_DELETE;
  };
