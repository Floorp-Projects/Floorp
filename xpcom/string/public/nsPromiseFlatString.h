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

#ifndef nsPromiseFlatString_h___
#define nsPromiseFlatString_h___

#ifndef nsSharableString_h___
#include "nsSharableString.h"
#endif

  /**
   * WARNING:
   *
   * Try to avoid flat strings.  |PromiseFlat[C]String| will help you as a last resort,
   * and this may be necessary when dealing with legacy or OS calls, but in general,
   * requiring a zero-terminated contiguous hunk of characters kills many of the performance
   * wins the string classes offer.  Write your own code to use |nsA[C]String&|s for parameters.
   * Write your string proccessing algorithms to exploit iterators.  If you do this, you
   * will benefit from being able to chain operations without copying or allocating and your
   * code will be significantly more efficient.  Remember, a function that takes an
   * |const nsA[C]String&| can always be passed a raw character pointer by wrapping it (for free)
   * in a |nsLocal[C]String|.  But a function that takes a character pointer always has the
   * potential to force allocation and copying.
   *
   *
   * How to use it:
   *
   * Like all `promises', a |nsPromiseFlat[C]String| doesn't own the characters it promises.
   * You must never use it to promise characters out of a string with a shorter lifespan.
   * The typical use will be something like this
   *
   *   SomeOSFunction( PromiseFlatCString(aCString).get() ); // GOOD
   *
   * Here's a BAD use:
   *
   *  const char* buffer = PromiseFlatCString(aCString).get();
   *  SomeOSFunction(buffer); // BAD!! |buffer| is a dangling pointer
   *
   * A |nsPromiseFlat[C]String| doesn't support non-|const| access (you can't use it to make
   * changes back into the original string).  To help you avoid that, the only way to make
   * one is with the function |PromiseFlat[C]String|, which produce a |const| instance.
   * ``What if I need to keep a promise around for a little while?'' you might ask.
   * In that case, you can keep a reference, like so
   *
   *   const nsPromiseFlatString& flat = PromiseFlatString(aString);
   *     // this reference holds the anonymous temporary alive, but remember, it must _still_
   *     //   have a lifetime shorter than that of |aString|
   *
   *  SomeOSFunction(flat.get());
   *  SomeOtherOSFunction(flat.get());
   *
   *
   * How does it work?
   *
   * A |nsPromiseFlat[C]String| is just a wrapper for another string.  If you apply it to
   * a string that happens to be flat, your promise is just a reference to that other string
   * and all calls are forwarded through to it.  If you apply it to a non-flat string,
   * then a temporary flat string is created for you, by allocating and copying.  In the unlikely
   * event that you end up assigning the result into a sharing string (e.g., |nsSharable[C]String|),
   * the right thing happens.
   */

class NS_COM nsPromiseFlatString
    : public nsAFlatString /* , public nsAPromiseString */
  {
    friend const nsPromiseFlatString PromiseFlatString( const abstract_string_type& );

    public:
      typedef nsPromiseFlatString self_type;

    public:
      nsPromiseFlatString( const self_type& );

    protected:
      nsPromiseFlatString() : mPromisedString(&mFlattenedString) { }
      explicit nsPromiseFlatString( const abstract_string_type& aString );

        // things we want to forward to the string we are promising
      virtual const buffer_handle_type*         GetFlatBufferHandle() const;
      virtual const buffer_handle_type*         GetBufferHandle() const;
      virtual const shared_buffer_handle_type*  GetSharedBufferHandle() const;
      virtual PRBool IsDependentOn( const abstract_string_type& ) const;


        // things we are forwarding now, but won't when we finally fix obsolete/nsString et al
    public:
      virtual const char_type* get() const;
      virtual PRUint32 Length() const;
    protected:
      virtual const char_type* GetReadableFragment( const_fragment_type&, nsFragmentRequest, PRUint32 ) const;


    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const nsPromiseFlatString& );

    private:
      nsSharableString      mFlattenedString;
      const nsAFlatString*  mPromisedString;
  };

class NS_COM nsPromiseFlatCString
    : public nsAFlatCString /* , public nsAPromiseCString */
  {
    friend const nsPromiseFlatCString PromiseFlatCString( const abstract_string_type& );

    public:
      typedef nsPromiseFlatCString self_type;

    public:
      nsPromiseFlatCString( const self_type& );

    protected:
      nsPromiseFlatCString() : mPromisedString(&mFlattenedString) { }
      explicit nsPromiseFlatCString( const abstract_string_type& aString );

        // things we want to forward to the string we are promising
      virtual const buffer_handle_type*         GetFlatBufferHandle() const;
      virtual const buffer_handle_type*         GetBufferHandle() const;
      virtual const shared_buffer_handle_type*  GetSharedBufferHandle() const;
      virtual PRBool IsDependentOn( const abstract_string_type& ) const;


        // things we are forwarding now, but won't when we finally fix obsolete/nsString et al
    public:
      virtual const char_type* get() const;
      virtual PRUint32 Length() const;
    protected:
      virtual const char_type* GetReadableFragment( const_fragment_type&, nsFragmentRequest, PRUint32 ) const;


    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const self_type& );

    private:
      nsSharableCString     mFlattenedString;
      const nsAFlatCString* mPromisedString;
  };


inline
const nsPromiseFlatString
PromiseFlatString( const nsAString& aString )
  {
    return nsPromiseFlatString(aString);
  }

inline
const nsPromiseFlatCString
PromiseFlatCString( const nsACString& aString )
  {
    return nsPromiseFlatCString(aString);
  }

#endif /* !defined(nsPromiseFlatString_h___) */
