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

/* nsCommonString.h --- a string implementation that shares its underlying storage */


#ifndef nsCommonString_h___
#define nsCommonString_h___

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

class NS_COM nsCommonString
    : public nsAFlatString
  {
    public:
      typedef nsCommonString    self_type;
      typedef PRUnichar         char_type;
      typedef nsAString         string_type;

    public:
      nsCommonString() { }
      nsCommonString( const self_type& aOther ) : mBuffer(aOther.mBuffer) { }
      nsCommonString( const string_type& aReadable ) { assign(aReadable); }

      self_type&
      operator=( const string_type& aReadable )
        {
          assign(aReadable);
          return *this;
        }

    protected:
      void assign( const string_type& );
      virtual const nsSharedBufferHandle<char_type>*  GetSharedBufferHandle() const;

    private:
      nsAutoBufferHandle<char_type> mBuffer;
  };


class NS_COM nsCommonCString
    : public nsAFlatCString
  {
    public:
      typedef nsCommonCString   self_type;
      typedef char              char_type;
      typedef nsACString        string_type;

    public:
      nsCommonCString() { }
      nsCommonCString( const self_type& aOther ) : mBuffer(aOther.mBuffer) { }
      nsCommonCString( const string_type& aReadable ) { assign(aReadable); }

      self_type&
      operator=( const string_type& aReadable )
        {
          assign(aReadable);
          return *this;
        }

    protected:
      void assign( const string_type& );
      virtual const nsSharedBufferHandle<char_type>*  GetSharedBufferHandle() const;

    private:
      nsAutoBufferHandle<char_type> mBuffer;
  };


#endif /* !defined(nsCommonString_h___) */
