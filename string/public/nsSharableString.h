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

/* nsSharableString.h --- a string implementation that shares its underlying storage */


#ifndef nsSharableString_h___
#define nsSharableString_h___

#ifndef nsAFlatString_h___
#include "nsAFlatString.h"
#endif

#ifndef nsBufferHandleUtils_h___
#include "nsBufferHandleUtils.h"
#endif

  /**
   * |nsSharableC?String| is the basic copy-on-write string class.  It
   * implements |nsAFlatC?String|, so it always has a single,
   * null-terminated, buffer.   It can accept assignment in two ways:
   *   1. |Assign| (which is equivalent to |operator=|), which shares
   *      the buffer if possible and copies the buffer otherwise, and
   *   2. |Adopt|, which takes over ownership of a raw buffer.
   */

class NS_COM nsSharableString
    : public nsAFlatString
  {
    public:
      typedef nsSharableString  self_type;

    public:
      nsSharableString() : mBuffer(GetSharedEmptyBufferHandle()) { }
      nsSharableString( const self_type& aOther ) : mBuffer(aOther.mBuffer) { }
      explicit nsSharableString( const abstract_string_type& aReadable )
        {
          // can call |do_AssignFromReadable| directly since we know
          // |&aReadable != this|.
          do_AssignFromReadable(aReadable);
        }
      explicit nsSharableString( const shared_buffer_handle_type* aHandle )
          : mBuffer(aHandle)
        {
          NS_ASSERTION(aHandle, "null handle");
        }

      // |operator=| does not inherit, so we must provide it again
      self_type& operator=( const abstract_string_type& aReadable )
        {
          Assign(aReadable);
          return *this;
        }

      // copy-assignment operator.  I must define my own if I don't want the compiler to make me one
      self_type& operator=( const self_type& aReadable )
        {
          Assign(aReadable);
          return *this;
        }

      /**
       * The |Adopt| method assigns a raw, null-terminated, character
       * buffer to this string object by transferring ownership of that
       * buffer to the string.  No copying occurs.
       *
       * XXX Add a second |Adopt| method that takes a length for clients
       * that already know the length.
       */
      void Adopt( char_type* aNewValue );

      /**
       * These overrides of |SetCapacity|, |SetLength|,
       * |do_AssignFromReadable|, and |GetSharedBufferHandle|, all
       * virtual methods on |nsAString|, allow |nsSharableString| to do
       * copy-on-write buffer sharing.
       */
    public:
      virtual void SetCapacity( size_type aNewCapacity );
      virtual void SetLength( size_type aNewLength );
    protected:
      virtual void do_AssignFromReadable( const abstract_string_type& aReadable );
      virtual const shared_buffer_handle_type*  GetSharedBufferHandle() const;
//  protected:  // can't hide these (yet), since I call them from forwarding routines in |nsPromiseFlatString|
    public:
      virtual char_type* GetWritableFragment( fragment_type&, nsFragmentRequest, PRUint32 );

    protected:
      static shared_buffer_handle_type* GetSharedEmptyBufferHandle();

    protected:
      nsAutoBufferHandle<char_type> mBuffer;
  };


class NS_COM nsSharableCString
    : public nsAFlatCString
  {
    public:
      typedef nsSharableCString  self_type;

    public:
      nsSharableCString() : mBuffer(GetSharedEmptyBufferHandle()) { }
      nsSharableCString( const self_type& aOther ) : mBuffer(aOther.mBuffer) { }
      explicit nsSharableCString( const abstract_string_type& aReadable )
        {
          // can call |do_AssignFromReadable| directly since we know
          // |&aReadable != this|.
          do_AssignFromReadable(aReadable);
        }
      explicit nsSharableCString( const shared_buffer_handle_type* aHandle )
          : mBuffer(aHandle)
        {
          NS_ASSERTION(aHandle, "null handle");
        }

      // |operator=| does not inherit, so we must provide it again
      self_type& operator=( const abstract_string_type& aReadable )
        {
          Assign(aReadable);
          return *this;
        }

      // copy-assignment operator.  I must define my own if I don't want the compiler to make me one
      self_type& operator=( const self_type& aReadable )
        {
          Assign(aReadable);
          return *this;
        }

      /**
       * The |Adopt| method assigns a raw, null-terminated, character
       * buffer to this string object by transferring ownership of that
       * buffer to the string.  No copying occurs.
       *
       * XXX Add a second |Adopt| method that takes a length for clients
       * that already know the length.
       */
      void Adopt( char_type* aNewValue );

      /**
       * These overrides of |SetCapacity|, |SetLength|,
       * |do_AssignFromReadable|, and |GetSharedBufferHandle|, all
       * virtual methods on |nsAString|, allow |nsSharableCString| to do
       * copy-on-write buffer sharing.
       */
    public:
      virtual void SetCapacity( size_type aNewCapacity );
      virtual void SetLength( size_type aNewLength );
    protected:
      virtual void do_AssignFromReadable( const abstract_string_type& aReadable );
      virtual const shared_buffer_handle_type*  GetSharedBufferHandle() const;
//  protected:  // can't hide these (yet), since I call them from forwarding routines in |nsPromiseFlatString|
    public:
      virtual char_type* GetWritableFragment( fragment_type&, nsFragmentRequest, PRUint32 );

    protected:
      static shared_buffer_handle_type* GetSharedEmptyBufferHandle();

    protected:
      nsAutoBufferHandle<char_type> mBuffer;
  };


#endif /* !defined(nsSharableString_h___) */
