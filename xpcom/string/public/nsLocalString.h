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

#ifndef nsLocalString_h___
#define nsLocalString_h___

#ifndef nsAFlatString_h___
#include "nsAFlatString.h"
#endif

    /*
      ...this class wraps a constant literal string and lets it act like an |nsAReadable...|.
      
      Use it like this:

        SomeFunctionTakingACString( nsLiteralCString("Hello, World!") );

      With some tweaking, I think I can make this work as well...

        SomeStringFunc( nsLiteralString( L"Hello, World!" ) );

      This class just holds a pointer.  If you don't supply the length, it must calculate it.
      No copying or allocations are performed.

      |const nsLocalString&| appears frequently in interfaces because it
      allows the automatic conversion of a |PRUnichar*|.
    */

class NS_COM nsLocalString
      : public nsAFlatString
  {
    public:
    
      explicit
      nsLocalString( const PRUnichar* aLiteral )
          : mHandle(NS_CONST_CAST(PRUnichar*, aLiteral), aLiteral ? (NS_CONST_CAST(PRUnichar*, aLiteral)+nsCharTraits<PRUnichar>::length(aLiteral)) : NS_CONST_CAST(PRUnichar*, aLiteral))
        {
          // nothing else to do here
        }

      nsLocalString( const PRUnichar* aLiteral, PRUint32 aLength )
          : mHandle(NS_CONST_CAST(PRUnichar*, aLiteral), NS_CONST_CAST(PRUnichar*, aLiteral)+aLength)
        {
            // This is an annoying hack.  Callers should be fixed to use the other
            //  constructor if they don't really know the length.
          if ( aLength == PRUint32(-1) )
            {
//            NS_WARNING("Tell scc: Caller constructing a string doesn't know the real length.  Please use the other constructor.");
              mHandle.DataEnd(aLiteral ? (NS_CONST_CAST(PRUnichar*, aLiteral)+nsCharTraits<PRUnichar>::length(aLiteral)) : NS_CONST_CAST(PRUnichar*, aLiteral));
            }
        }

      // nsLocalString( const nsLocalString& );  // auto-generated copy-constructor OK
      // ~nsLocalString();                       // auto-generated destructor OK

      virtual const nsBufferHandle<PRUnichar>* GetFlatBufferHandle() const  { return &mHandle; }
      virtual const nsBufferHandle<PRUnichar>* GetBufferHandle() const      { return &mHandle; }

    private:
      nsBufferHandle<PRUnichar> mHandle;

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const nsLocalString& );    // we're immutable
  };



class NS_COM nsLocalCString
      : public nsAFlatCString
  {
    public:
    
      explicit
      nsLocalCString( const char* aLiteral )
          : mHandle(NS_CONST_CAST(char*, aLiteral), aLiteral ? (NS_CONST_CAST(char*, aLiteral)+nsCharTraits<char>::length(aLiteral)) : NS_CONST_CAST(char*, aLiteral))
        {
          // nothing else to do here
        }

      nsLocalCString( const char* aLiteral, PRUint32 aLength )
          : mHandle(NS_CONST_CAST(char*, aLiteral), NS_CONST_CAST(char*, aLiteral)+aLength)
        {
            // This is an annoying hack.  Callers should be fixed to use the other
            //  constructor if they don't really know the length.
          if ( aLength == PRUint32(-1) )
            {
//            NS_WARNING("Tell scc: Caller constructing a string doesn't know the real length.  Please use the other constructor.");
              mHandle.DataEnd(aLiteral ? (NS_CONST_CAST(char*, aLiteral)+nsCharTraits<char>::length(aLiteral)) : NS_CONST_CAST(char*, aLiteral));
            }
        }

      // nsLocalCString( const nsLocalCString& );   // auto-generated copy-constructor OK
      // ~nsLocalCString();                         // auto-generated destructor OK

      virtual const nsBufferHandle<char>* GetFlatBufferHandle() const   { return &mHandle; }
      virtual const nsBufferHandle<char>* GetBufferHandle() const       { return &mHandle; }

    private:
      nsBufferHandle<char> mHandle;

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const nsLocalCString& );    // we're immutable
  };

#endif /* !defined(nsLocalString_h___) */
