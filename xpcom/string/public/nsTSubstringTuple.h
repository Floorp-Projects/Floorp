//* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
        : mHead(nsnull)
        , mFragA(a)
        , mFragB(b) {}

      nsTSubstringTuple_CharT(const self_type& head, const base_string_type* b)
        : mHead(&head)
        , mFragA(nsnull) // this fragment is ignored when head != nsnull
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
