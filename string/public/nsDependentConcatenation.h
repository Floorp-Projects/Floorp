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

/* nsPromiseConcatenation.h --- string concatenation machinery lives here, but don't include this file
    directly, always get it by including either "nsAString.h" or one of the compatibility headers */

#ifndef nsPromiseConcatenation_h___
#define nsPromiseConcatenation_h___

  /**
    NOT FOR USE BY HUMANS

    Instances of this class only exist as anonymous temporary results from |operator+()|.
    This is the machinery that makes string concatenation efficient.  No allocations or
    character copies are required unless and until a final assignment is made.  It works
    its magic by overriding and forwarding calls to |GetReadableFragment()|.

    Note: |nsPromiseConcatenation| imposes some limits on string concatenation with |operator+()|.
      - no more than 33 strings, e.g., |s1 + s2 + s3 + ... s32 + s33|
      - left to right evaluation is required ... do not use parentheses to override this

    In practice, neither of these is onerous.  Parentheses do not change the semantics of the
    concatenation, only the order in which the result is assembled ... so there's no reason
    for a user to need to control it.  Too many strings summed together can easily be worked
    around with an intermediate assignment.  I wouldn't have the parentheses limitation if I
    assigned the identifier mask starting at the top, the first time anybody called
    |GetReadableFragment()|.
   */

class NS_COM nsPromiseConcatenation
    : public nsAPromiseString
  {
    public:
      typedef nsPromiseConcatenation        self_type;
      typedef PRUnichar                     char_type;
      typedef nsAString                     string_type;
      typedef string_type::const_iterator   const_iterator;

    protected:
      virtual const char_type* GetReadableFragment( nsReadableFragment<char_type>&, nsFragmentRequest, PRUint32 ) const;
      virtual       char_type* GetWritableFragment( nsWritableFragment<char_type>&, nsFragmentRequest, PRUint32 ) { return 0; }

      enum { kLeftString, kRightString };

      int
      GetCurrentStringFromFragment( const nsReadableFragment<char_type>& aFragment ) const
        {
          return (NS_REINTERPRET_CAST(PRUint32, aFragment.mFragmentIdentifier) & mFragmentIdentifierMask) ? kRightString : kLeftString;
        }

      int
      SetLeftStringInFragment( nsReadableFragment<char_type>& aFragment ) const
        {
          aFragment.mFragmentIdentifier = NS_REINTERPRET_CAST(void*, NS_REINTERPRET_CAST(PRUint32, aFragment.mFragmentIdentifier) & ~mFragmentIdentifierMask);
          return kLeftString;
        }

      int
      SetRightStringInFragment( nsReadableFragment<char_type>& aFragment ) const
        {
          aFragment.mFragmentIdentifier = NS_REINTERPRET_CAST(void*, NS_REINTERPRET_CAST(PRUint32, aFragment.mFragmentIdentifier) | mFragmentIdentifierMask);
          return kRightString;
        }

    public:
      nsPromiseConcatenation( const string_type& aLeftString, const string_type& aRightString, PRUint32 aMask = 1 )
          : mFragmentIdentifierMask(aMask)
        {
          mStrings[kLeftString] = &aLeftString;
          mStrings[kRightString] = &aRightString;
        }

      nsPromiseConcatenation( const self_type& aLeftString, const string_type& aRightString )
          : mFragmentIdentifierMask(aLeftString.mFragmentIdentifierMask<<1)
        {
          mStrings[kLeftString] = &aLeftString;
          mStrings[kRightString] = &aRightString;
        }

      // nsPromiseConcatenation( const self_type& ); // auto-generated copy-constructor should be OK
      // ~nsPromiseConcatenation();                  // auto-generated destructor OK

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const self_type& );            // we're immutable, you can't assign into a concatenation

    public:

      virtual PRUint32 Length() const;
      virtual PRBool Promises( const string_type& ) const;
//    virtual PRBool PromisesExactly( const string_type& ) const;

//    const self_type operator+( const string_type& rhs ) const;

      PRUint32 GetFragmentIdentifierMask() const { return mFragmentIdentifierMask; }

    private:
      void operator+( const self_type& ); // NOT TO BE IMPLEMENTED
        // making this |private| stops you from over parenthesizing concatenation expressions, e.g., |(A+B) + (C+D)|
        //  which would break the algorithm for distributing bits in the fragment identifier

    private:
      const string_type*  mStrings[2];
      PRUint32            mFragmentIdentifierMask;
  };

class NS_COM nsPromiseCConcatenation
    : public nsAPromiseCString
  {
    public:
      typedef nsPromiseCConcatenation       self_type;
      typedef char                          char_type;
      typedef nsACString                    string_type;
      typedef string_type::const_iterator   const_iterator;

    protected:
      virtual const char_type* GetReadableFragment( nsReadableFragment<char_type>&, nsFragmentRequest, PRUint32 ) const;
      virtual       char_type* GetWritableFragment( nsWritableFragment<char_type>&, nsFragmentRequest, PRUint32 ) { return 0; }

      enum { kLeftString, kRightString };

      int
      GetCurrentStringFromFragment( const nsReadableFragment<char_type>& aFragment ) const
        {
          return (NS_REINTERPRET_CAST(PRUint32, aFragment.mFragmentIdentifier) & mFragmentIdentifierMask) ? kRightString : kLeftString;
        }

      int
      SetLeftStringInFragment( nsReadableFragment<char_type>& aFragment ) const
        {
          aFragment.mFragmentIdentifier = NS_REINTERPRET_CAST(void*, NS_REINTERPRET_CAST(PRUint32, aFragment.mFragmentIdentifier) & ~mFragmentIdentifierMask);
          return kLeftString;
        }

      int
      SetRightStringInFragment( nsReadableFragment<char_type>& aFragment ) const
        {
          aFragment.mFragmentIdentifier = NS_REINTERPRET_CAST(void*, NS_REINTERPRET_CAST(PRUint32, aFragment.mFragmentIdentifier) | mFragmentIdentifierMask);
          return kRightString;
        }

    public:
      nsPromiseCConcatenation( const string_type& aLeftString, const string_type& aRightString, PRUint32 aMask = 1 )
          : mFragmentIdentifierMask(aMask)
        {
          mStrings[kLeftString] = &aLeftString;
          mStrings[kRightString] = &aRightString;
        }

      nsPromiseCConcatenation( const self_type& aLeftString, const string_type& aRightString )
          : mFragmentIdentifierMask(aLeftString.mFragmentIdentifierMask<<1)
        {
          mStrings[kLeftString] = &aLeftString;
          mStrings[kRightString] = &aRightString;
        }

      // nsPromiseCConcatenation( const self_type& ); // auto-generated copy-constructor should be OK
      // ~nsPromiseCConcatenation();                  // auto-generated destructor OK

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const self_type& );            // we're immutable, you can't assign into a concatenation

    public:

      virtual PRUint32 Length() const;
      virtual PRBool Promises( const string_type& ) const;
//    virtual PRBool PromisesExactly( const string_type& ) const;

//    const self_type operator+( const string_type& rhs ) const;

      PRUint32 GetFragmentIdentifierMask() const { return mFragmentIdentifierMask; }

    private:
      void operator+( const self_type& ); // NOT TO BE IMPLEMENTED
        // making this |private| stops you from over parenthesizing concatenation expressions, e.g., |(A+B) + (C+D)|
        //  which would break the algorithm for distributing bits in the fragment identifier

    private:
      const string_type*  mStrings[2];
      PRUint32            mFragmentIdentifierMask;
  };

  /*
    How shall we provide |operator+()|?

    What would it return?  It has to return a stack based object, because the client will
    not be given an opportunity to handle memory management in an expression like

      myWritableString = stringA + stringB + stringC;

    ...so the `obvious' answer of returning a new |nsSharedString| is no good.  We could
    return an |nsString|, if that name were in scope here, though there's no telling what the client
    will really want to do with the result.  What might be better, though,
    is to return a `promise' to concatenate some strings...

    By making |nsPromiseConcatenation| inherit from readable strings, we automatically handle
    assignment and other interesting uses within writable strings, plus we drastically reduce
    the number of cases we have to write |operator+()| for.  The cost is extra temporary concat strings
    in the evaluation of strings of '+'s, e.g., |A + B + C + D|, and that we have to do some work
    to implement the virtual functions of readables.
  */

inline
const nsPromiseConcatenation
operator+( const nsPromiseConcatenation& lhs, const nsAString& rhs )
  {
    return nsPromiseConcatenation(lhs, rhs, lhs.GetFragmentIdentifierMask()<<1);
  }

inline
const nsPromiseCConcatenation
operator+( const nsPromiseCConcatenation& lhs, const nsACString& rhs )
  {
    return nsPromiseCConcatenation(lhs, rhs, lhs.GetFragmentIdentifierMask()<<1);
  }

inline
const nsPromiseConcatenation
operator+( const nsAString& lhs, const nsAString& rhs )
  {
    return nsPromiseConcatenation(lhs, rhs);
  }

inline
const nsPromiseCConcatenation
operator+( const nsACString& lhs, const nsACString& rhs )
  {
    return nsPromiseCConcatenation(lhs, rhs);
  }

#if 0
inline
const nsPromiseConcatenation
nsPromiseConcatenation::operator+( const string_type& rhs ) const
  {
    return nsPromiseConcatenation(*this, rhs, mFragmentIdentifierMask<<1);
  }

inline
const nsPromiseCConcatenation
nsPromiseCConcatenation::operator+( const string_type& rhs ) const
  {
    return nsPromiseCConcatenation(*this, rhs, mFragmentIdentifierMask<<1);
  }
#endif


#endif /* !defined(nsPromiseConcatenation_h___) */
