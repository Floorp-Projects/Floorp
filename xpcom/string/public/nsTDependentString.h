/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


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

      nsTDependentString_CharT( const string_type& str, PRUint32 startPos )
        : string_type()
        {
          Rebind(str, startPos);
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

      void Rebind( const char_type* data, size_type length );

      void Rebind( const char_type* start, const char_type* end )
        {
          Rebind(start, PRUint32(end - start));
        }

      void Rebind( const string_type&, PRUint32 startPos );

    private:
      
      // NOT USED
      nsTDependentString_CharT( const substring_tuple_type& );
  };
