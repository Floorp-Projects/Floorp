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

      typedef nsTDependentString_CharT    self_type;

    public:

        /**
         * verify restrictions
         */
      void AssertValid()
        {
          NS_ASSERTION(mData, "nsTDependentString must wrap a non-NULL buffer");
          NS_ASSERTION(mLength != size_type(-1), "nsTDependentString has bogus length");
          NS_ASSERTION(mData[mLength] == 0, "nsTDependentString must wrap only null-terminated strings");
        }


        /**
         * constructors
         */

      nsTDependentString_CharT( const char_type* start, const char_type* end )
        : string_type(const_cast<char_type*>(start), PRUint32(end - start), F_TERMINATED)
        {
          AssertValid();
        }

      nsTDependentString_CharT( const char_type* data, PRUint32 length )
        : string_type(const_cast<char_type*>(data), length, F_TERMINATED)
        {
          AssertValid();
        }

      explicit
      nsTDependentString_CharT( const char_type* data )
        : string_type(const_cast<char_type*>(data), PRUint32(char_traits::length(data)), F_TERMINATED)
        {
          AssertValid();
        }

      explicit
      nsTDependentString_CharT( const substring_type& str )
        : string_type(const_cast<char_type*>(str.Data()), str.Length(), F_TERMINATED)
        {
          AssertValid();
        }

      // Create a nsTDependentSubstring to be bound later
      nsTDependentString_CharT()
        : string_type() {}

      // XXX are you sure??
      // auto-generated copy-constructor OK
      // auto-generated copy-assignment operator OK
      // auto-generated destructor OK


        /**
         * allow this class to be bound to a different string...
         */

      void Rebind( const char_type* data )
        {
          Rebind(data, PRUint32(char_traits::length(data)));
        }

      NS_COM void Rebind( const char_type* data, size_type length );

      void Rebind( const char_type* start, const char_type* end )
        {
          Rebind(start, PRUint32(end - start));
        }

    private:
      
      // NOT USED
      nsTDependentString_CharT( const substring_tuple_type& );
  };
