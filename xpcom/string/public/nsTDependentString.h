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
         * constructors
         */

      nsTDependentString_CharT( const char_type* start, const char_type* end )
        : string_type(const_cast<char_type*>(start), uint32_t(end - start), F_TERMINATED)
        {
          AssertValidDepedentString();
        }

      nsTDependentString_CharT( const char_type* data, uint32_t length )
        : string_type(const_cast<char_type*>(data), length, F_TERMINATED)
        {
          AssertValidDepedentString();
        }

      explicit
      nsTDependentString_CharT( const char_type* data )
        : string_type(const_cast<char_type*>(data), uint32_t(char_traits::length(data)), F_TERMINATED)
        {
          AssertValidDepedentString();
        }

      nsTDependentString_CharT( const string_type& str, uint32_t startPos )
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

      using nsTString_CharT::Rebind;
      void Rebind( const char_type* data )
        {
          Rebind(data, uint32_t(char_traits::length(data)));
        }

      void Rebind( const char_type* start, const char_type* end )
        {
          Rebind(start, uint32_t(end - start));
        }

      void Rebind( const string_type&, uint32_t startPos );

    private:
      
      // NOT USED
      nsTDependentString_CharT( const substring_tuple_type& );
  };
