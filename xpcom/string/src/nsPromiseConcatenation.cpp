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
  // remember, no one should include "nsPromiseConcatenation.h" themselves
  //  one always gets it through "nsAString.h"

PRUint32
nsPromiseConcatenation::Length() const
  {
    return mStrings[kLeftString]->Length() + mStrings[kRightString]->Length();
  }

PRBool
nsPromiseConcatenation::Promises( const string_type& aString ) const
  {
    return mStrings[0]->Promises(aString) || mStrings[1]->Promises(aString);
  }

#if 0
PRBool
nsPromiseConcatenation::PromisesExactly( const string_type& aString ) const
  {
      // Not really like this, test for the empty string, etc
    return mStrings[0] == &aString && !mStrings[1] || !mStrings[0] && mStrings[1] == &aString;
  }
#endif

const PRUnichar*
nsPromiseConcatenation::GetReadableFragment( nsReadableFragment<char_type>& aFragment, nsFragmentRequest aRequest, PRUint32 aPosition ) const
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
          whichString = SetLeftStringInFragment(aFragment);
          break;

        case kLastFragment:
          whichString = SetRightStringInFragment(aFragment);
          break;

        case kFragmentAt:
          PRUint32 leftLength = mStrings[kLeftString]->Length();
          if ( aPosition < leftLength )
            whichString = SetLeftStringInFragment(aFragment);
          else
            {
              whichString = SetRightStringInFragment(aFragment);
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
            if ( aRequest == kNextFragment && whichString == kLeftString )
              {
                aRequest = kFirstFragment;
                whichString = SetRightStringInFragment(aFragment);
              }
            else if ( aRequest == kPrevFragment && whichString == kRightString )
              {
                aRequest = kLastFragment;
                whichString = SetLeftStringInFragment(aFragment);
              }
            else
              done = PR_TRUE;
          }
      }
    while ( !done );
    return result;
  }


PRUint32
nsPromiseCConcatenation::Length() const
  {
    return mStrings[kLeftString]->Length() + mStrings[kRightString]->Length();
  }

PRBool
nsPromiseCConcatenation::Promises( const string_type& aString ) const
  {
    return mStrings[0]->Promises(aString) || mStrings[1]->Promises(aString);
  }

#if 0
PRBool
nsPromiseCConcatenation::PromisesExactly( const string_type& aString ) const
  {
      // Not really like this, test for the empty string, etc
    return mStrings[0] == &aString && !mStrings[1] || !mStrings[0] && mStrings[1] == &aString;
  }
#endif

const char*
nsPromiseCConcatenation::GetReadableFragment( nsReadableFragment<char_type>& aFragment, nsFragmentRequest aRequest, PRUint32 aPosition ) const
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
          whichString = SetLeftStringInFragment(aFragment);
          break;

        case kLastFragment:
          whichString = SetRightStringInFragment(aFragment);
          break;

        case kFragmentAt:
          PRUint32 leftLength = mStrings[kLeftString]->Length();
          if ( aPosition < leftLength )
            whichString = SetLeftStringInFragment(aFragment);
          else
            {
              whichString = SetRightStringInFragment(aFragment);
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
            if ( aRequest == kNextFragment && whichString == kLeftString )
              {
                aRequest = kFirstFragment;
                whichString = SetRightStringInFragment(aFragment);
              }
            else if ( aRequest == kPrevFragment && whichString == kRightString )
              {
                aRequest = kLastFragment;
                whichString = SetLeftStringInFragment(aFragment);
              }
            else
              done = PR_TRUE;
          }
      }
    while ( !done );
    return result;
  }
