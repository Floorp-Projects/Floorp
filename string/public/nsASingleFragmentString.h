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

/* nsASingleFragmentString.h --- */

#ifndef nsASingleFragmentString_h___
#define nsASingleFragmentString_h___

#ifndef nsAString_h___
#include "nsAString.h"
#endif

class NS_COM nsASingleFragmentString
    : public nsAString
  {
      // protected:
      // Foundation for more efficient iterators on single fragment strings
      //   typedef abstract_string_type::const_iterator  general_const_iterator;
      //   typedef abstract_string_type::iterator        general_iterator;

    public:
      typedef const char_type*  const_char_iterator;
      typedef char_type*        char_iterator;

#ifdef HAVE_CPP_AMBIGUITY_RESOLVING_USING
      using abstract_string_type::BeginReading;
      using abstract_string_type::EndReading;
      using abstract_string_type::BeginWriting;
      using abstract_string_type::EndWriting;
#else
      inline const_iterator& BeginReading( const_iterator& I ) const  { return abstract_string_type::BeginReading(I); }
      inline const_iterator& EndReading(   const_iterator& I ) const  { return abstract_string_type::EndReading(I);   }
      inline       iterator& BeginWriting(       iterator& I )        { return abstract_string_type::BeginWriting(I); }
      inline       iterator& EndWriting(         iterator& I )        { return abstract_string_type::EndWriting(I);   }
#endif

      inline
      const_char_iterator&
      BeginReading( const_char_iterator& aResult ) const
        {
          const_fragment_type f;
          GetReadableFragment(f, kFirstFragment, 0);
          return aResult = f.mStart;
        }

      inline
      const_char_iterator&
      EndReading( const_char_iterator& aResult ) const
        {
          const_fragment_type f;
          GetReadableFragment(f, kLastFragment, 0);
          return aResult = f.mEnd;
        }

      inline
      char_iterator&
      BeginWriting( char_iterator& aResult )
        {
          fragment_type f;
          GetWritableFragment(f, kFirstFragment, 0);
          return aResult = NS_CONST_CAST(char_iterator, f.mStart);
        }

      inline
      char_iterator&
      EndWriting( char_iterator& aResult )
        {
          fragment_type f;
          GetWritableFragment(f, kLastFragment, 0);
          return aResult = NS_CONST_CAST(char_iterator, f.mEnd);
        }

      char_type operator[]( PRUint32 i ) const { const_char_iterator temp; return BeginReading(temp)[ i ]; }
      char_type CharAt( PRUint32 ) const;

      virtual PRUint32 Length() const;

//  protected:  // can't hide these (yet), since I call them from forwarding routines in |nsPromiseFlatString|
    public:
      virtual const char_type* GetReadableFragment( const_fragment_type&, nsFragmentRequest, PRUint32 ) const;
      virtual       char_type* GetWritableFragment(       fragment_type&, nsFragmentRequest, PRUint32 );
  };

class NS_COM nsASingleFragmentCString
    : public nsACString
  {
      // protected:
      // Foundation for more efficient iterators on single fragment strings
      //   typedef abstract_string_type::const_iterator  general_const_iterator;
      //   typedef abstract_string_type::iterator        general_iterator;

    public:
      typedef const char_type*  const_char_iterator;
      typedef char_type*        char_iterator;

#ifdef HAVE_CPP_AMBIGUITY_RESOLVING_USING
      using abstract_string_type::BeginReading;
      using abstract_string_type::EndReading;
      using abstract_string_type::BeginWriting;
      using abstract_string_type::EndWriting;
#else
      inline const_iterator& BeginReading( const_iterator& I ) const  { return abstract_string_type::BeginReading(I); }
      inline const_iterator& EndReading(   const_iterator& I ) const  { return abstract_string_type::EndReading(I);   }
      inline       iterator& BeginWriting(       iterator& I )        { return abstract_string_type::BeginWriting(I); }
      inline       iterator& EndWriting(         iterator& I )        { return abstract_string_type::EndWriting(I);   }
#endif

      inline
      const_char_iterator&
      BeginReading( const_char_iterator& aResult ) const
        {
          const_fragment_type f;
          GetReadableFragment(f, kFirstFragment, 0);
          return aResult = f.mStart;
        }

      inline
      const_char_iterator&
      EndReading( const_char_iterator& aResult ) const
        {
          const_fragment_type f;
          GetReadableFragment(f, kLastFragment, 0);
          return aResult = f.mEnd;
        }

      inline
      char_iterator&
      BeginWriting( char_iterator& aResult )
        {
          fragment_type f;
          GetWritableFragment(f, kFirstFragment, 0);
          return aResult = NS_CONST_CAST(char_iterator, f.mStart);
        }

      inline
      char_iterator&
      EndWriting( char_iterator& aResult )
        {
          fragment_type f;
          GetWritableFragment(f, kLastFragment, 0);
          return aResult = NS_CONST_CAST(char_iterator, f.mEnd);
        }

      char_type operator[]( PRUint32 i ) const { const_char_iterator temp; return BeginReading(temp)[ i ]; }
      char_type CharAt( PRUint32 ) const;

      virtual PRUint32 Length() const;

//  protected:  // can't hide these (yet), since I call them from forwarding routines in |nsPromiseFlatString|
    public:
      virtual const char_type* GetReadableFragment( const_fragment_type&, nsFragmentRequest, PRUint32 ) const;
      virtual       char_type* GetWritableFragment(       fragment_type&, nsFragmentRequest, PRUint32 );
  };

inline
nsASingleFragmentString::char_type
nsASingleFragmentString::CharAt( PRUint32 i ) const
  {
    NS_ASSERTION(i<Length(), "|CharAt| out-of-range");
    return operator[](i);
  }

inline
nsASingleFragmentCString::char_type
nsASingleFragmentCString::CharAt( PRUint32 i ) const
  {
    NS_ASSERTION(i<Length(), "|CharAt| out-of-range");
    return operator[](i);
  }



#endif /* !defined(nsASingleFragmentString_h___) */
