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

//-------1---------2---------3---------4---------5---------6---------7---------8

#include "nsAString.h"
  // remember, no one should include "nsDependentConcatenation.h" themselves
  //  one always gets it through "nsAString.h"

#ifndef nsDependentConcatenation_h___
#include "nsDependentConcatenation.h"
#endif


PRUint32
nsDependentConcatenation::Length() const
  {
    return mStrings[kFirstString]->Length() + mStrings[kLastString]->Length();
  }

PRBool
nsDependentConcatenation::IsDependentOn( const abstract_string_type& aString ) const
  {
    return mStrings[kFirstString]->IsDependentOn(aString) || mStrings[kLastString]->IsDependentOn(aString);
  }

#if 0
PRBool
nsDependentConcatenation::PromisesExactly( const abstract_string_type& aString ) const
  {
      // Not really like this, test for the empty string, etc
    return mStrings[kFirstString] == &aString && !mStrings[kLastString] || !mStrings[kFirstString] && mStrings[kLastString] == &aString;
  }
#endif

const nsDependentConcatenation::char_type*
nsDependentConcatenation::GetReadableFragment( const_fragment_type& aFragment, nsFragmentRequest aRequest, PRUint32 aPosition ) const
  {
    int whichString;

      // based on the request, pick which string we will forward the |GetReadableFragment()| call into

    switch ( aRequest )
      {
        case kPrevFragment:
        case kNextFragment:
          whichString = GetCurrentStringFromFragment(aFragment);
          break;

        case kFirstFragment:
          whichString = SetFirstStringInFragment(aFragment);
          break;

        case kLastFragment:
          whichString = SetLastStringInFragment(aFragment);
          break;

        case kFragmentAt:
          PRUint32 leftLength = mStrings[kFirstString]->Length();
          if ( aPosition < leftLength )
            whichString = SetFirstStringInFragment(aFragment);
          else
            {
              whichString = SetLastStringInFragment(aFragment);
              aPosition -= leftLength;
            }
          break;
            
      }

    const char_type* result;
    PRBool done;
    do
      {
        done = PR_TRUE;
        result = mStrings[whichString]->GetReadableFragment(aFragment, aRequest, aPosition);

        if ( !result )
          {
            done = PR_FALSE;
            if ( aRequest == kNextFragment && whichString == kFirstString )
              {
                aRequest = kFirstFragment;
                whichString = SetLastStringInFragment(aFragment);
              }
            else if ( aRequest == kPrevFragment && whichString == kLastString )
              {
                aRequest = kLastFragment;
                whichString = SetFirstStringInFragment(aFragment);
              }
            else
              done = PR_TRUE;
          }
      }
    while ( !done );
    return result;
  }


PRUint32
nsDependentCConcatenation::Length() const
  {
    return mStrings[kFirstString]->Length() + mStrings[kLastString]->Length();
  }

PRBool
nsDependentCConcatenation::IsDependentOn( const abstract_string_type& aString ) const
  {
    return mStrings[kFirstString]->IsDependentOn(aString) || mStrings[kLastString]->IsDependentOn(aString);
  }

#if 0
PRBool
nsDependentCConcatenation::PromisesExactly( const abstract_string_type& aString ) const
  {
      // Not really like this, test for the empty string, etc
    return mStrings[kFirstString] == &aString && !mStrings[kLastString] || !mStrings[kFirstString] && mStrings[kLastString] == &aString;
  }
#endif

const nsDependentCConcatenation::char_type*
nsDependentCConcatenation::GetReadableFragment( const_fragment_type& aFragment, nsFragmentRequest aRequest, PRUint32 aPosition ) const
  {
    int whichString;

      // based on the request, pick which string we will forward the |GetReadableFragment()| call into

    switch ( aRequest )
      {
        case kPrevFragment:
        case kNextFragment:
          whichString = GetCurrentStringFromFragment(aFragment);
          break;

        case kFirstFragment:
          whichString = SetFirstStringInFragment(aFragment);
          break;

        case kLastFragment:
          whichString = SetLastStringInFragment(aFragment);
          break;

        case kFragmentAt:
          PRUint32 leftLength = mStrings[kFirstString]->Length();
          if ( aPosition < leftLength )
            whichString = SetFirstStringInFragment(aFragment);
          else
            {
              whichString = SetLastStringInFragment(aFragment);
              aPosition -= leftLength;
            }
          break;
            
      }

    const char_type* result;
    PRBool done;
    do
      {
        done = PR_TRUE;
        result = mStrings[whichString]->GetReadableFragment(aFragment, aRequest, aPosition);

        if ( !result )
          {
            done = PR_FALSE;
            if ( aRequest == kNextFragment && whichString == kFirstString )
              {
                aRequest = kFirstFragment;
                whichString = SetLastStringInFragment(aFragment);
              }
            else if ( aRequest == kPrevFragment && whichString == kLastString )
              {
                aRequest = kLastFragment;
                whichString = SetFirstStringInFragment(aFragment);
              }
            else
              done = PR_TRUE;
          }
      }
    while ( !done );
    return result;
  }
