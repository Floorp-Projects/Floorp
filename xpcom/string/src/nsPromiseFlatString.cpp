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

#include "nsPromiseFlatString.h"


nsPromiseFlatString::nsPromiseFlatString( const nsPromiseFlatString& aOther )
    : mFlattenedString(aOther.mFlattenedString)
  {
    if ( aOther.mPromisedString == &aOther.mFlattenedString )
      mPromisedString = &mFlattenedString;
    else
      mPromisedString = aOther.mPromisedString;
  }

nsPromiseFlatString::nsPromiseFlatString( const nsAString& aString )
  {
    if ( aString.GetFlatBufferHandle() )
      mPromisedString = NS_STATIC_CAST(const nsAFlatString*, &aString);
    else
      {
        mFlattenedString = aString; // flatten |aString|
        mPromisedString = &mFlattenedString;
      }
  }

const nsBufferHandle<PRUnichar>*
nsPromiseFlatString::GetFlatBufferHandle() const
  {
    return mPromisedString->GetFlatBufferHandle();
  }

const nsBufferHandle<PRUnichar>*
nsPromiseFlatString::GetBufferHandle() const
  {
    return mPromisedString->GetBufferHandle();
  }

const nsSharedBufferHandle<PRUnichar>*
nsPromiseFlatString::GetSharedBufferHandle() const
  {
    return mPromisedString->GetSharedBufferHandle();
  }

PRBool
nsPromiseFlatString::Promises( const nsAString& aString ) const
  {
    return mPromisedString->Promises(aString);
  }

const PRUnichar*
nsPromiseFlatString::get() const
  {
    return mPromisedString->get();
  }

PRUint32
nsPromiseFlatString::Length() const
  {
    return mPromisedString->Length();
  }

const PRUnichar*
nsPromiseFlatString::GetReadableFragment( nsReadableFragment<PRUnichar>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    return mPromisedString->GetReadableFragment(aFragment, aRequest, aOffset);
  }



nsPromiseFlatCString::nsPromiseFlatCString( const nsPromiseFlatCString& aOther )
    : mFlattenedString(aOther.mFlattenedString)
  {
    if ( aOther.mPromisedString == &aOther.mFlattenedString )
      mPromisedString = &mFlattenedString;
    else
      mPromisedString = aOther.mPromisedString;
  }

nsPromiseFlatCString::nsPromiseFlatCString( const nsACString& aString )
  {
    if ( aString.GetFlatBufferHandle() )
      mPromisedString = NS_STATIC_CAST(const nsAFlatCString*, &aString);
    else
      {
        mFlattenedString = aString; // flatten |aString|
        mPromisedString = &mFlattenedString;
      }
  }

const nsBufferHandle<char>*
nsPromiseFlatCString::GetFlatBufferHandle() const
  {
    return mPromisedString->GetFlatBufferHandle();
  }

const nsBufferHandle<char>*
nsPromiseFlatCString::GetBufferHandle() const
  {
    return mPromisedString->GetBufferHandle();
  }

const nsSharedBufferHandle<char>*
nsPromiseFlatCString::GetSharedBufferHandle() const
  {
    return mPromisedString->GetSharedBufferHandle();
  }

PRBool
nsPromiseFlatCString::Promises( const nsACString& aString ) const
  {
    return mPromisedString->Promises(aString);
  }

const char*
nsPromiseFlatCString::get() const
  {
    return mPromisedString->get();
  }

PRUint32
nsPromiseFlatCString::Length() const
  {
    return mPromisedString->Length();
  }

const char*
nsPromiseFlatCString::GetReadableFragment( nsReadableFragment<char>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    return mPromisedString->GetReadableFragment(aFragment, aRequest, aOffset);
  }
