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


nsPromiseFlatString::nsPromiseFlatString( const self_type& aOther )
    : mFlattenedString(aOther.mFlattenedString)
  {
    if ( aOther.mPromisedString == &aOther.mFlattenedString )
      mPromisedString = &mFlattenedString;
    else
      mPromisedString = aOther.mPromisedString;
  }

nsPromiseFlatString::nsPromiseFlatString( const abstract_string_type& aString )
  {
    if ( aString.GetFlatBufferHandle() )
      mPromisedString = NS_STATIC_CAST(const nsAFlatString*, &aString);
    else
      {
        mFlattenedString = aString; // flatten |aString|
        mPromisedString = &mFlattenedString;
      }
  }

const nsPromiseFlatString::buffer_handle_type*
nsPromiseFlatString::GetFlatBufferHandle() const
  {
    return mPromisedString->GetFlatBufferHandle();
  }

const nsPromiseFlatString::buffer_handle_type*
nsPromiseFlatString::GetBufferHandle() const
  {
    return mPromisedString->GetBufferHandle();
  }

const nsPromiseFlatString::shared_buffer_handle_type*
nsPromiseFlatString::GetSharedBufferHandle() const
  {
    return mPromisedString->GetSharedBufferHandle();
  }

PRBool
nsPromiseFlatString::IsDependentOn( const abstract_string_type& aString ) const
  {
    return mPromisedString->IsDependentOn(aString);
  }

const nsPromiseFlatString::char_type*
nsPromiseFlatString::get() const
  {
    return mPromisedString->get();
  }

PRUint32
nsPromiseFlatString::Length() const
  {
    return mPromisedString->Length();
  }

const nsPromiseFlatString::char_type*
nsPromiseFlatString::GetReadableFragment( const_fragment_type& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    return mPromisedString->GetReadableFragment(aFragment, aRequest, aOffset);
  }



nsPromiseFlatCString::nsPromiseFlatCString( const self_type& aOther )
    : mFlattenedString(aOther.mFlattenedString)
  {
    if ( aOther.mPromisedString == &aOther.mFlattenedString )
      mPromisedString = &mFlattenedString;
    else
      mPromisedString = aOther.mPromisedString;
  }

nsPromiseFlatCString::nsPromiseFlatCString( const abstract_string_type& aString )
  {
    if ( aString.GetFlatBufferHandle() )
      mPromisedString = NS_STATIC_CAST(const nsAFlatCString*, &aString);
    else
      {
        mFlattenedString = aString; // flatten |aString|
        mPromisedString = &mFlattenedString;
      }
  }

const nsPromiseFlatCString::buffer_handle_type*
nsPromiseFlatCString::GetFlatBufferHandle() const
  {
    return mPromisedString->GetFlatBufferHandle();
  }

const nsPromiseFlatCString::buffer_handle_type*
nsPromiseFlatCString::GetBufferHandle() const
  {
    return mPromisedString->GetBufferHandle();
  }

const nsPromiseFlatCString::shared_buffer_handle_type*
nsPromiseFlatCString::GetSharedBufferHandle() const
  {
    return mPromisedString->GetSharedBufferHandle();
  }

PRBool
nsPromiseFlatCString::IsDependentOn( const abstract_string_type& aString ) const
  {
    return mPromisedString->IsDependentOn(aString);
  }

const nsPromiseFlatCString::char_type*
nsPromiseFlatCString::get() const
  {
    return mPromisedString->get();
  }

PRUint32
nsPromiseFlatCString::Length() const
  {
    return mPromisedString->Length();
  }

const nsPromiseFlatCString::char_type*
nsPromiseFlatCString::GetReadableFragment( const_fragment_type& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    return mPromisedString->GetReadableFragment(aFragment, aRequest, aOffset);
  }
