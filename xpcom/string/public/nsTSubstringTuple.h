//* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// IWYU pragma: private, include "nsSubstringTuple.h"

  /**
   * nsTSubstringTuple_CharT
   *
   * Represents a tuple of string fragments.  Built as a recursive binary tree.
   * It is used to implement the concatenation of two or more string objects.
   *
   * NOTE: This class is a private implementation detail and should never be 
   * referenced outside the string code.
   */
class nsTSubstringTuple_CharT
  {
    public:

      typedef CharT                      char_type;
      typedef nsCharTraits<char_type>    char_traits;

      typedef nsTSubstringTuple_CharT    self_type;
      typedef nsTSubstring_CharT         substring_type;
      typedef nsTSubstring_CharT         base_string_type;
      typedef PRUint32                   size_type;

    public:

      nsTSubstringTuple_CharT(const base_string_type* a, const base_string_type* b)
        : mHead(nullptr)
        , mFragA(a)
        , mFragB(b) {}

      nsTSubstringTuple_CharT(const self_type& head, const base_string_type* b)
        : mHead(&head)
        , mFragA(nullptr) // this fragment is ignored when head != nullptr
        , mFragB(b) {}

        /**
         * computes the aggregate string length
         */
      size_type Length() const;

        /**
         * writes the aggregate string to the given buffer.  bufLen is assumed
         * to be equal to or greater than the value returned by the Length()
         * method.  the string written to |buf| is not null-terminated.
         */
      void WriteTo(char_type *buf, PRUint32 bufLen) const;

        /**
         * returns true if this tuple is dependent on (i.e., overlapping with)
         * the given char sequence.
         */
      bool IsDependentOn(const char_type *start, const char_type *end) const;

    private:

      const self_type*        mHead;
      const base_string_type* mFragA;
      const base_string_type* mFragB;
  };

inline
const nsTSubstringTuple_CharT
operator+(const nsTSubstringTuple_CharT::base_string_type& a, const nsTSubstringTuple_CharT::base_string_type& b)
  {
    return nsTSubstringTuple_CharT(&a, &b);
  }

inline
const nsTSubstringTuple_CharT
operator+(const nsTSubstringTuple_CharT& head, const nsTSubstringTuple_CharT::base_string_type& b)
  {
    return nsTSubstringTuple_CharT(head, &b);
  }
