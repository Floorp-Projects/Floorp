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

        SomeFunctionTakingACString( nsLiteralCString(myCharPtr) );

      This class just holds a pointer.  If you don't supply the length, it must calculate it.
      No copying or allocations are performed.

      See also |NS_LITERAL_STRING| and |NS_LITERAL_CSTRING| when you have a quoted string you want to
      use as a |const nsAString|.
    */

class NS_COM nsDependentString
      : public nsAFlatString
  {
    public:
    
      explicit
      nsDependentString( const PRUnichar* aPtr )
          : mHandle(NS_CONST_CAST(PRUnichar*, aPtr), aPtr ? (NS_CONST_CAST(PRUnichar*, aPtr)+nsCharTraits<PRUnichar>::length(aPtr)) : 0)
        {
          // nothing else to do here
        }

      nsDependentString( const PRUnichar* aPtr, PRUint32 aLength )
          : mHandle(NS_CONST_CAST(PRUnichar*, aPtr), NS_CONST_CAST(PRUnichar*, aPtr)+aLength)
        {
            // This is an annoying hack.  Callers should be fixed to use the other
            //  constructor if they don't really know the length.
          if ( aLength == PRUint32(-1) )
            {
//            NS_WARNING("Tell scc: Caller constructing a string doesn't know the real length.  Please use the other constructor.");
              mHandle.DataEnd(aPtr ? (NS_CONST_CAST(PRUnichar*, aPtr)+nsCharTraits<PRUnichar>::length(aPtr)) : 0);
            }
        }

      // nsDependentString( const nsDependentString& ); // auto-generated copy-constructor OK
      // ~nsDependentString();                          // auto-generated destructor OK

      virtual const nsBufferHandle<PRUnichar>* GetFlatBufferHandle() const  { return &mHandle; }
      virtual const nsBufferHandle<PRUnichar>* GetBufferHandle() const      { return &mHandle; }

    private:
      nsBufferHandle<PRUnichar> mHandle;

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const nsDependentString& );    // we're immutable
  };



class NS_COM nsDependentCString
      : public nsAFlatCString
  {
    public:
    
      explicit
      nsDependentCString( const char* aPtr )
          : mHandle(NS_CONST_CAST(char*, aPtr), aPtr ? (NS_CONST_CAST(char*, aPtr)+nsCharTraits<char>::length(aPtr)) : 0)
        {
          // nothing else to do here
        }

      nsDependentCString( const char* aPtr, PRUint32 aLength )
          : mHandle(NS_CONST_CAST(char*, aPtr), NS_CONST_CAST(char*, aPtr)+aLength)
        {
            // This is an annoying hack.  Callers should be fixed to use the other
            //  constructor if they don't really know the length.
          if ( aLength == PRUint32(-1) )
            {
//            NS_WARNING("Tell scc: Caller constructing a string doesn't know the real length.  Please use the other constructor.");
              mHandle.DataEnd(aPtr ? (NS_CONST_CAST(char*, aPtr)+nsCharTraits<char>::length(aPtr)) : 0);
            }
        }

      // nsDependentCString( const nsDependentCString& ); // auto-generated copy-constructor OK
      // ~nsDependentCString();                           // auto-generated destructor OK

      virtual const nsBufferHandle<char>* GetFlatBufferHandle() const   { return &mHandle; }
      virtual const nsBufferHandle<char>* GetBufferHandle() const       { return &mHandle; }

    private:
      nsBufferHandle<char> mHandle;

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const nsDependentCString& );    // we're immutable
  };


  // temporary |typedef|s till we fix commercial and soap
  //  see http://bugzilla.mozilla.org/show_bug.cgi?id=75220
typedef nsDependentString   nsLocalString;
typedef nsDependentCString  nsLocalCString;

#endif /* !defined(nsDependentString_h___) */
