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

/* nsAFlatString.h --- */

#ifndef nsAFlatString_h___
#define nsAFlatString_h___

#ifndef nsAString_h___
#include "nsAString.h"
#endif

class NS_COM nsAFlatString
    : public nsAString
  {
    public:
        // don't really want this to be virtual, and won't after |obsolete_nsString| is really dead
      virtual const PRUnichar* get() const      { return GetBufferHandle()->DataStart(); }
      PRUnichar  operator[]( PRUint32 i ) const { return get()[ i ]; }
      PRUnichar  CharAt( PRUint32 ) const;

      virtual PRUint32 Length() const           { return PRUint32(GetBufferHandle()->DataLength()); }

    protected:
      virtual const PRUnichar* GetReadableFragment( nsReadableFragment<PRUnichar>&, nsFragmentRequest, PRUint32 ) const;
      virtual       PRUnichar* GetWritableFragment( nsWritableFragment<PRUnichar>&, nsFragmentRequest, PRUint32 );
  };

class NS_COM nsAFlatCString
    : public nsACString
  {
    public:
        // don't really want this to be virtual, and won't after |obsolete_nsCString| is really dead
      virtual const char* get() const           { return GetBufferHandle()->DataStart(); }
      char  operator[]( PRUint32 i ) const      { return get()[ i ]; }
      char  CharAt( PRUint32 ) const;

      virtual PRUint32 Length() const           { return PRUint32(GetBufferHandle()->DataLength()); }

    protected:
      virtual const char* GetReadableFragment( nsReadableFragment<char>&, nsFragmentRequest, PRUint32 ) const;
      virtual       char* GetWritableFragment( nsWritableFragment<char>&, nsFragmentRequest, PRUint32 );
  };

inline
PRUnichar
nsAFlatString::CharAt( PRUint32 i ) const
  {
    NS_ASSERTION(i<Length(), "|CharAt| out-of-range");
    return operator[](i);
  }

inline
char
nsAFlatCString::CharAt( PRUint32 i ) const
  {
    NS_ASSERTION(i<Length(), "|CharAt| out-of-range");
    return operator[](i);
  }



#endif /* !defined(nsAFlatString_h___) */
