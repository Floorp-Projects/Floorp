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

/* nsDependentConcatenation.h --- string concatenation machinery lives here, but don't include this file
    directly, always get it by including either "nsAString.h" or one of the compatibility headers */

#ifndef nsDependentConcatenation_h___
#define nsDependentConcatenation_h___

  /**
    NOT FOR USE BY HUMANS

    Instances of this class only exist as anonymous temporary results from |operator+()|.
    This is the machinery that makes string concatenation efficient.  No allocations or
    character copies are required unless and until a final assignment is made.  It works
    its magic by overriding and forwarding calls to |GetReadableFragment()|.

    Note: |nsDependentConcatenation| imposes some limits on string concatenation with |operator+()|.
      - no more than 33 strings, e.g., |s1 + s2 + s3 + ... s32 + s33|
      - left to right evaluation is required ... do not use parentheses to override this

    In practice, neither of these is onerous.  Parentheses do not change the semantics of the
    concatenation, only the order in which the result is assembled ... so there's no reason
    for a user to need to control it.  Too many strings summed together can easily be worked
    around with an intermediate assignment.  I wouldn't have the parentheses limitation if I
    assigned the identifier mask starting at the top, the first time anybody called
    |GetReadableFragment()|.
   */

class NS_COM nsDependentConcatenation
    : public nsAPromiseString
  {
    public:
      typedef nsDependentConcatenation      self_type;

    protected:
      virtual const char_type* GetReadableFragment( const_fragment_type&, nsFragmentRequest, PRUint32 ) const;
      virtual       char_type* GetWritableFragment(       fragment_type&, nsFragmentRequest, PRUint32 ) { return 0; }

      enum { kFirstString, kLastString };

      int
      GetCurrentStringFromFragment( const const_fragment_type& aFragment ) const
        {
          return (aFragment.GetIDAsInt() & mFragmentIdentifierMask) ? kLastString : kFirstString;
        }

      int
      SetFirstStringInFragment( const_fragment_type& aFragment ) const
        {
          aFragment.SetID(aFragment.GetIDAsInt() & ~mFragmentIdentifierMask);
          return kFirstString;
        }

      int
      SetLastStringInFragment( const_fragment_type& aFragment ) const
        {
          aFragment.SetID(aFragment.GetIDAsInt() | mFragmentIdentifierMask);
          return kLastString;
        }

    public:
      nsDependentConcatenation( const abstract_string_type& aFirstString, const abstract_string_type& aLastString, PRUint32 aMask = 1 )
          : mFragmentIdentifierMask(aMask)
        {
          mStrings[kFirstString] = &aFirstString;
          mStrings[kLastString] = &aLastString;
        }

      nsDependentConcatenation( const self_type& aFirstString, const abstract_string_type& aLastString )
          : mFragmentIdentifierMask(aFirstString.mFragmentIdentifierMask<<1)
        {
          mStrings[kFirstString] = &aFirstString;
          mStrings[kLastString] = &aLastString;
        }

      // nsDependentConcatenation( const self_type& ); // auto-generated copy-constructor should be OK
      // ~nsDependentConcatenation();                  // auto-generated destructor OK

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const self_type& );            // we're immutable, you can't assign into a concatenation

    public:

      virtual PRUint32 Length() const;
      virtual PRBool IsDependentOn( const abstract_string_type& ) const;
//    virtual PRBool PromisesExactly( const abstract_string_type& ) const;

//    const self_type operator+( const abstract_string_type& rhs ) const;

      PRUint32 GetFragmentIdentifierMask() const { return mFragmentIdentifierMask; }

    private:
      void operator+( const self_type& ); // NOT TO BE IMPLEMENTED
        // making this |private| stops you from over parenthesizing concatenation expressions, e.g., |(A+B) + (C+D)|
        //  which would break the algorithm for distributing bits in the fragment identifier

    private:
      const abstract_string_type*  mStrings[2];
      PRUint32                     mFragmentIdentifierMask;
  };

class NS_COM nsDependentCConcatenation
    : public nsAPromiseCString
  {
    public:
      typedef nsDependentCConcatenation     self_type;

    protected:
      virtual const char_type* GetReadableFragment( const_fragment_type&, nsFragmentRequest, PRUint32 ) const;
      virtual       char_type* GetWritableFragment(       fragment_type&, nsFragmentRequest, PRUint32 ) { return 0; }

      enum { kFirstString, kLastString };

      int
      GetCurrentStringFromFragment( const const_fragment_type& aFragment ) const
        {
          return (aFragment.GetIDAsInt() & mFragmentIdentifierMask) ? kLastString : kFirstString;
        }

      int
      SetFirstStringInFragment( const_fragment_type& aFragment ) const
        {
          aFragment.SetID(aFragment.GetIDAsInt() & ~mFragmentIdentifierMask);
          return kFirstString;
        }

      int
      SetLastStringInFragment( const_fragment_type& aFragment ) const
        {
          aFragment.SetID(aFragment.GetIDAsInt() | mFragmentIdentifierMask);
          return kLastString;
        }

    public:
      nsDependentCConcatenation( const abstract_string_type& aFirstString, const abstract_string_type& aLastString, PRUint32 aMask = 1 )
          : mFragmentIdentifierMask(aMask)
        {
          mStrings[kFirstString] = &aFirstString;
          mStrings[kLastString] = &aLastString;
        }

      nsDependentCConcatenation( const self_type& aFirstString, const abstract_string_type& aLastString )
          : mFragmentIdentifierMask(aFirstString.mFragmentIdentifierMask<<1)
        {
          mStrings[kFirstString] = &aFirstString;
          mStrings[kLastString] = &aLastString;
        }

      // nsDependentCConcatenation( const self_type& ); // auto-generated copy-constructor should be OK
      // ~nsDependentCConcatenation();                  // auto-generated destructor OK

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const self_type& );            // we're immutable, you can't assign into a concatenation

    public:

      virtual PRUint32 Length() const;
      virtual PRBool IsDependentOn( const abstract_string_type& ) const;
//    virtual PRBool PromisesExactly( const abstract_string_type& ) const;

//    const self_type operator+( const abstract_string_type& rhs ) const;

      PRUint32 GetFragmentIdentifierMask() const { return mFragmentIdentifierMask; }

    private:
      void operator+( const self_type& ); // NOT TO BE IMPLEMENTED
        // making this |private| stops you from over parenthesizing concatenation expressions, e.g., |(A+B) + (C+D)|
        //  which would break the algorithm for distributing bits in the fragment identifier

    private:
      const abstract_string_type*  mStrings[2];
      PRUint32                     mFragmentIdentifierMask;
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

    By making |nsDependentConcatenation| inherit from readable strings, we automatically handle
    assignment and other interesting uses within writable strings, plus we drastically reduce
    the number of cases we have to write |operator+()| for.  The cost is extra temporary concat strings
    in the evaluation of strings of '+'s, e.g., |A + B + C + D|, and that we have to do some work
    to implement the virtual functions of readables.
  */

inline
const nsDependentConcatenation
operator+( const nsDependentConcatenation& lhs, const nsAString& rhs )
  {
    return nsDependentConcatenation(lhs, rhs, lhs.GetFragmentIdentifierMask()<<1);
  }

inline
const nsDependentCConcatenation
operator+( const nsDependentCConcatenation& lhs, const nsACString& rhs )
  {
    return nsDependentCConcatenation(lhs, rhs, lhs.GetFragmentIdentifierMask()<<1);
  }

inline
const nsDependentConcatenation
operator+( const nsAString& lhs, const nsAString& rhs )
  {
    return nsDependentConcatenation(lhs, rhs);
  }

inline
const nsDependentCConcatenation
operator+( const nsACString& lhs, const nsACString& rhs )
  {
    return nsDependentCConcatenation(lhs, rhs);
  }

#if 0
inline
const nsDependentConcatenation
nsDependentConcatenation::operator+( const abstract_string_type& rhs ) const
  {
    return nsDependentConcatenation(*this, rhs, mFragmentIdentifierMask<<1);
  }

inline
const nsDependentCConcatenation
nsDependentCConcatenation::operator+( const abstract_string_type& rhs ) const
  {
    return nsDependentCConcatenation(*this, rhs, mFragmentIdentifierMask<<1);
  }
#endif


#endif /* !defined(nsDependentConcatenation_h___) */
