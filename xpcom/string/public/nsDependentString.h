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
      void
      Rebind( const PRUnichar* aPtr )
        {
          mHandle.DataStart(aPtr);
          mHandle.DataEnd(aPtr ? (aPtr+nsCharTraits<PRUnichar>::length(aPtr)) : 0);
        }

      void
      Rebind( const PRUnichar* aStartPtr, const PRUnichar* aEndPtr )
        {
          mHandle.DataStart(aStartPtr);
          mHandle.DataEnd(aEndPtr);
        }

      void
      Rebind( const PRUnichar* aPtr, PRUint32 aLength )
        {
          if ( aLength == PRUint32(-1) )
            {
//            NS_WARNING("Tell scc: Caller binding a dependent string doesn't know the real length.  Please pick the appropriate call.");
              Rebind(aPtr);
            }
          else
            Rebind(aPtr, aPtr+aLength);
        }

      nsDependentString( const PRUnichar* aStartPtr, const PRUnichar* aEndPtr ) { Rebind(aStartPtr, aEndPtr); }
      nsDependentString( const PRUnichar* aPtr, PRUint32 aLength )              { Rebind(aPtr, aLength); }
      explicit nsDependentString( const PRUnichar* aPtr )                       { Rebind(aPtr); }

      // nsDependentString( const nsDependentString& );                         // auto-generated copy-constructor OK
      // ~nsDependentString();                                                  // auto-generated destructor OK

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const nsDependentString& );                               // we're immutable, so no copy-assignment operator

    public:
      virtual const nsBufferHandle<PRUnichar>* GetFlatBufferHandle() const      { return NS_REINTERPRET_CAST(const nsBufferHandle<PRUnichar>*, &mHandle); }
      virtual const nsBufferHandle<PRUnichar>* GetBufferHandle() const          { return NS_REINTERPRET_CAST(const nsBufferHandle<PRUnichar>*, &mHandle); }

    private:
      nsConstBufferHandle<PRUnichar> mHandle;
  };



class NS_COM nsDependentCString
      : public nsAFlatCString
  {
    public:
      void
      Rebind( const char* aPtr )
        {
          mHandle.DataStart(aPtr);
          mHandle.DataEnd(aPtr ? (aPtr+nsCharTraits<char>::length(aPtr)) : 0);
        }

      void
      Rebind( const char* aStartPtr, const char* aEndPtr )
        {
          mHandle.DataStart(aStartPtr);
          mHandle.DataEnd(aEndPtr);
        }

      void
      Rebind( const char* aPtr, PRUint32 aLength )
        {
          if ( aLength == PRUint32(-1) )
            {
//            NS_WARNING("Tell scc: Caller binding a dependent string doesn't know the real length.  Please pick the appropriate call.");
              Rebind(aPtr);
            }
          else
            Rebind(aPtr, aPtr+aLength);
        }

      nsDependentCString( const char* aStartPtr, const char* aEndPtr )          { Rebind(aStartPtr, aEndPtr); }
      nsDependentCString( const char* aPtr, PRUint32 aLength )                  { Rebind(aPtr, aLength); }
      explicit nsDependentCString( const char* aPtr )                           { Rebind(aPtr); }

      // nsDependentCString( const nsDependentCString& );                       // auto-generated copy-constructor OK
      // ~nsDependentCString();                                                 // auto-generated destructor OK

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const nsDependentCString& );                              // we're immutable, so no copy-assignment operator

    public:
      virtual const nsBufferHandle<char>* GetFlatBufferHandle() const           { return NS_REINTERPRET_CAST(const nsBufferHandle<char>*, &mHandle); }
      virtual const nsBufferHandle<char>* GetBufferHandle() const               { return NS_REINTERPRET_CAST(const nsBufferHandle<char>*, &mHandle); }

    private:
      nsConstBufferHandle<char> mHandle;
  };

#endif /* !defined(nsDependentString_h___) */
