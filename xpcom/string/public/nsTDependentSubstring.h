/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Scott Collins <scc@mozilla.org> (original author)
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
   * nsTDependentSubstring_CharT
   */
class nsTDependentSubstring_CharT : public nsTSubstring_CharT
  {
    public:

      typedef nsTDependentSubstring_CharT    self_type;

    public:

      NS_COM void Rebind( const abstract_string_type&, PRUint32 startPos, PRUint32 length = size_type(-1) );
      NS_COM void Rebind( const substring_type&, PRUint32 startPos, PRUint32 length = size_type(-1) );

      void Rebind( const char_type* start, const char_type* end )
        {
          NS_ASSERTION(start && end, "nsTDependentSubstring must wrap a non-NULL buffer");
          mData = NS_CONST_CAST(char_type*, start);
          mLength = end - start;
          SetDataFlags(F_NONE);
        }

      nsTDependentSubstring_CharT( const abstract_string_type& str, PRUint32 startPos, PRUint32 length = size_type(-1) )
        : substring_type(F_NONE)
        {
          Rebind(str, startPos, length);
        }

      nsTDependentSubstring_CharT( const substring_type& str, PRUint32 startPos, PRUint32 length = size_type(-1) )
        : substring_type(F_NONE)
        {
          Rebind(str, startPos, length);
        }

      nsTDependentSubstring_CharT( const char_type* start, const char_type* end )
        : substring_type(NS_CONST_CAST(char_type*, start), end - start, F_NONE) {}

      nsTDependentSubstring_CharT( const const_iterator& start, const const_iterator& end )
        : substring_type(NS_CONST_CAST(char_type*, start.get()), end.get() - start.get(), F_NONE) {}

      // auto-generated copy-constructor OK (XXX really?? what about base class copy-ctor?)

    private:
        // NOT USED
      void operator=( const self_type& );        // we're immutable, you can't assign into a substring
  };

inline
const nsTDependentSubstring_CharT
Substring( const nsTAString_CharT& str, PRUint32 startPos, PRUint32 length = PRUint32(-1) )
  {
    return nsTDependentSubstring_CharT(str, startPos, length);
  }

inline
const nsTDependentSubstring_CharT
Substring( const nsTSubstring_CharT& str, PRUint32 startPos, PRUint32 length = PRUint32(-1) )
  {
    return nsTDependentSubstring_CharT(str, startPos, length);
  }

inline
const nsTDependentSubstring_CharT
Substring( const nsReadingIterator<CharT>& start, const nsReadingIterator<CharT>& end )
  {
    return nsTDependentSubstring_CharT(start.get(), end.get());
  }

inline
const nsTDependentSubstring_CharT
Substring( const CharT* start, const CharT* end )
  {
    return nsTDependentSubstring_CharT(start, end);
  }

inline
const nsTDependentSubstring_CharT
StringHead( const nsTAString_CharT& str, PRUint32 count )
  {
    return nsTDependentSubstring_CharT(str, 0, count);
  }

inline
const nsTDependentSubstring_CharT
StringHead( const nsTSubstring_CharT& str, PRUint32 count )
  {
    return nsTDependentSubstring_CharT(str, 0, count);
  }

inline
const nsTDependentSubstring_CharT
StringTail( const nsTAString_CharT& str, PRUint32 count )
  {
    return nsTDependentSubstring_CharT(str, str.Length() - count, count);
  }

inline
const nsTDependentSubstring_CharT
StringTail( const nsTSubstring_CharT& str, PRUint32 count )
  {
    return nsTDependentSubstring_CharT(str, str.Length() - count, count);
  }
