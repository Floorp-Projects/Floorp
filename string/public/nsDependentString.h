/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Collins <scc@mozilla.org> (original author)
 */

#ifndef nsDependentString_h___
#define nsDependentString_h___

#ifndef nsAFlatString_h___
#include "nsAFlatString.h"
#endif

    /*
      ...this class wraps a constant literal string and lets it act like an |const nsAString|.
      
      Use it like this:

        SomeFunctionTakingACString( nsDependentCString(myCharPtr) );

      This class just holds a pointer.  If you don't supply the length, it must calculate it.
      No copying or allocations are performed.

      See also |NS_LITERAL_STRING| and |NS_LITERAL_CSTRING| when you have a quoted string you want to
      use as a |const nsAString|.
    */

class NS_COM nsDependentString
      : public nsAFlatString
  {
    public:
      typedef nsDependentString self_type;

      void
      Rebind( const char_type* aPtr )
        {
          NS_ASSERTION(aPtr, "nsDependentString must wrap a non-NULL buffer");
          mHandle.DataStart(aPtr);
          // XXX This should not be NULL-safe, but we should flip the switch
          // early in a milestone.
          //mHandle.DataEnd(aPtr+nsCharTraits<char_type>::length(aPtr));
          mHandle.DataEnd(aPtr ? (aPtr+nsCharTraits<char_type>::length(aPtr)) : 0);
        }

      void
      Rebind( const char_type* aStartPtr, const char_type* aEndPtr )
        {
          NS_ASSERTION(aStartPtr && aEndPtr, "nsDependentString must wrap a non-NULL buffer");

          /*
           * If you see the following assertion, it means that
           * |nsDependentString| is being used in a way that violates
           * the meaning of |nsAFlatString| (that the string object
           * wraps a buffer that is a null-terminated single fragment).
           * The way to fix the problem is to use the global |Substring|
           * function to return a string that is an
           * |nsASingleFragmentString| rather than an |nsAFlatString|.
           *
           * In other words, replace:
           *   nsDependentString(startPtr, endPtr)
           * with 
           *   Substring(startPtr, endPtr)
           *
           * or replace
           *   nsDependentString(startPtr, length)
           * with
           *   Substring(startPtr, startPtr+length)
           */
          NS_ASSERTION(!*aEndPtr, "nsDependentString must wrap only null-terminated strings");
          mHandle.DataStart(aStartPtr);
          mHandle.DataEnd(aEndPtr);
        }

      void
      Rebind( const char_type* aPtr, PRUint32 aLength )
        {
          NS_ASSERTION(aLength != PRUint32(-1), "caller passing bogus length");
          Rebind(aPtr, aPtr+aLength);
        }

      nsDependentString( const char_type* aStartPtr, const char_type* aEndPtr ) { Rebind(aStartPtr, aEndPtr); }
      nsDependentString( const char_type* aPtr, PRUint32 aLength )              { Rebind(aPtr, aLength); }
      explicit nsDependentString( const char_type* aPtr )                       { Rebind(aPtr); }

      // nsDependentString( const self_type& );                                 // auto-generated copy-constructor OK
      // ~nsDependentString();                                                  // auto-generated destructor OK

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const self_type& );                                       // we're immutable, so no copy-assignment operator

    public:
      virtual const buffer_handle_type* GetFlatBufferHandle() const             { return NS_REINTERPRET_CAST(const buffer_handle_type*, &mHandle); }
      virtual const buffer_handle_type* GetBufferHandle() const                 { return NS_REINTERPRET_CAST(const buffer_handle_type*, &mHandle); }

    private:
      const_buffer_handle_type mHandle;
  };



class NS_COM nsDependentCString
      : public nsAFlatCString
  {
    public:
      typedef nsDependentCString self_type;

      void
      Rebind( const char_type* aPtr )
        {
          NS_ASSERTION(aPtr, "nsDependentCString must wrap a non-NULL buffer");
          mHandle.DataStart(aPtr);
          // XXX This should not be NULL-safe, but we should flip the switch
          // early in a milestone.
          //mHandle.DataEnd(aPtr+nsCharTraits<char_type>::length(aPtr));
          mHandle.DataEnd(aPtr ? (aPtr+nsCharTraits<char_type>::length(aPtr)) : 0);
        }

      void
      Rebind( const char_type* aStartPtr, const char_type* aEndPtr )
        {
          NS_ASSERTION(aStartPtr && aEndPtr, "nsDependentCString must wrap a non-NULL buffer");

          /*
           * If you see the following assertion, it means that
           * |nsDependentCString| is being used in a way that violates
           * the meaning of |nsAFlatCString| (that the string object
           * wraps a buffer that is a null-terminated single fragment).
           * The way to fix the problem is to use the global |Substring|
           * function to return a string that is an
           * |nsASingleFragmentCString| rather than an |nsAFlatCString|.
           *
           * In other words, replace:
           *   nsDependentCString(startPtr, endPtr)
           * with 
           *   Substring(startPtr, endPtr)
           *
           * or replace
           *   nsDependentString(startPtr, length)
           * with
           *   Substring(startPtr, startPtr+length)
           */
          NS_ASSERTION(!*aEndPtr, "nsDependentCString must wrap only null-terminated strings");
          mHandle.DataStart(aStartPtr);
          mHandle.DataEnd(aEndPtr);
        }

      void
      Rebind( const char_type* aPtr, PRUint32 aLength )
        {
          NS_ASSERTION(aLength != PRUint32(-1), "caller passing bogus length");
          Rebind(aPtr, aPtr+aLength);
        }

      nsDependentCString( const char_type* aStartPtr, const char_type* aEndPtr ) { Rebind(aStartPtr, aEndPtr); }
      nsDependentCString( const char_type* aPtr, PRUint32 aLength )              { Rebind(aPtr, aLength); }
      explicit nsDependentCString( const char_type* aPtr )                       { Rebind(aPtr); }

      // nsDependentCString( const self_type& );                                 // auto-generated copy-constructor OK
      // ~nsDependentCString();                                                  // auto-generated destructor OK

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const self_type& );                                       // we're immutable, so no copy-assignment operator

    public:
      virtual const buffer_handle_type* GetFlatBufferHandle() const             { return NS_REINTERPRET_CAST(const buffer_handle_type*, &mHandle); }
      virtual const buffer_handle_type* GetBufferHandle() const                 { return NS_REINTERPRET_CAST(const buffer_handle_type*, &mHandle); }

    private:
      const_buffer_handle_type mHandle;
  };

#endif /* !defined(nsDependentString_h___) */
