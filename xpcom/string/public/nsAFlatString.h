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
      virtual const char_type* get() const;
      char_type operator[]( PRUint32 i ) const { return get()[ i ]; }
      char_type CharAt( PRUint32 ) const;

      virtual PRUint32 Length() const;

//  protected:  // can't hide these (yet), since I call them from forwarding routines in |nsPromiseFlatString|
    public:
      virtual const char_type* GetReadableFragment( const_fragment_type&, nsFragmentRequest, PRUint32 ) const;
      virtual       char_type* GetWritableFragment(       fragment_type&, nsFragmentRequest, PRUint32 );
  };

class NS_COM nsAFlatCString
    : public nsACString
  {
    public:
        // don't really want this to be virtual, and won't after |obsolete_nsCString| is really dead
      virtual const char_type* get() const;
      char_type operator[]( PRUint32 i ) const      { return get()[ i ]; }
      char_type CharAt( PRUint32 ) const;

      virtual PRUint32 Length() const;

//  protected:  // can't hide these (yet), since I call them from forwarding routines in |nsPromiseFlatCString|
    public:
      virtual const char_type* GetReadableFragment( const_fragment_type&, nsFragmentRequest, PRUint32 ) const;
      virtual       char_type* GetWritableFragment(       fragment_type&, nsFragmentRequest, PRUint32 );
  };

inline
nsAFlatString::char_type
nsAFlatString::CharAt( PRUint32 i ) const
  {
    NS_ASSERTION(i<Length(), "|CharAt| out-of-range");
    return operator[](i);
  }

inline
nsAFlatCString::char_type
nsAFlatCString::CharAt( PRUint32 i ) const
  {
    NS_ASSERTION(i<Length(), "|CharAt| out-of-range");
    return operator[](i);
  }



#endif /* !defined(nsAFlatString_h___) */
