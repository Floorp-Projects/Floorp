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

//-------1---------2---------3---------4---------5---------6---------7---------8

  /**
   * Not yet ready for non-|const| access
   */

class NS_COM nsSharableString
    : public nsAFlatString
  {
    public:
      typedef nsSharableString  self_type;

    public:
      nsSharableString() { }
      nsSharableString( const self_type& aOther ) : mBuffer(aOther.mBuffer) { }
      explicit nsSharableString( const abstract_string_type& aReadable ) { assign(aReadable); }
      explicit nsSharableString( const shared_buffer_handle_type* aHandle ) : mBuffer(aHandle) { }

      self_type&
      operator=( const abstract_string_type& aReadable )
        {
          assign(aReadable);
          return *this;
        }

    protected:
      void assign( const abstract_string_type& );
      virtual const shared_buffer_handle_type*  GetSharedBufferHandle() const;

    protected:
      nsAutoBufferHandle<char_type> mBuffer;
  };


class NS_COM nsSharableCString
    : public nsAFlatCString
  {
    public:
      typedef nsSharableCString self_type;

    public:
      nsSharableCString() { }
      nsSharableCString( const self_type& aOther ) : mBuffer(aOther.mBuffer) { }
      explicit nsSharableCString( const abstract_string_type& aReadable ) { assign(aReadable); }
      explicit nsSharableCString( const shared_buffer_handle_type* aHandle ) : mBuffer(aHandle) { }

      self_type&
      operator=( const abstract_string_type& aReadable )
        {
          assign(aReadable);
          return *this;
        }

    protected:
      void assign( const abstract_string_type& );
      virtual const shared_buffer_handle_type*  GetSharedBufferHandle() const;

    protected:
      nsAutoBufferHandle<char_type> mBuffer;
  };


#endif /* !defined(nsSharableString_h___) */
