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

#include "nsAFlatString.h"

const nsAFlatString::char_type*
nsAFlatString::GetReadableFragment( const_fragment_type& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          {
            const buffer_handle_type* buffer = GetBufferHandle();

            if ( !buffer )
              return 0;

            aFragment.mEnd = buffer->DataEnd();
            return (aFragment.mStart = buffer->DataStart()) + aOffset;
          }

        case kPrevFragment:
        case kNextFragment:
        default:
          return 0;
      }
  }

nsAFlatString::char_type*
nsAFlatString::GetWritableFragment( fragment_type& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset )
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          {
            buffer_handle_type* buffer = NS_CONST_CAST(buffer_handle_type*, GetBufferHandle());
            NS_ASSERTION(buffer, "trouble: no buffer!");

            aFragment.mEnd = buffer->DataEnd();
            return (aFragment.mStart = buffer->DataStart()) + aOffset;
          }

        case kPrevFragment:
        case kNextFragment:
        default:
          return 0;
      }
  }

const nsAFlatCString::char_type*
nsAFlatCString::GetReadableFragment( const_fragment_type& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          {
            const buffer_handle_type* buffer = GetBufferHandle();
            NS_ASSERTION(buffer, "trouble: no buffer!");

            aFragment.mEnd = buffer->DataEnd();
            return (aFragment.mStart = buffer->DataStart()) + aOffset;
          }

        case kPrevFragment:
        case kNextFragment:
        default:
          return 0;
      }
  }

nsAFlatCString::char_type*
nsAFlatCString::GetWritableFragment( fragment_type& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset )
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          {
            buffer_handle_type* buffer = NS_CONST_CAST(buffer_handle_type*, GetBufferHandle());
            NS_ASSERTION(buffer, "trouble: no buffer!");

            aFragment.mEnd = buffer->DataEnd();
            return (aFragment.mStart = buffer->DataStart()) + aOffset;
          }

        case kPrevFragment:
        case kNextFragment:
        default:
          return 0;
      }
  }

PRUint32
nsAFlatString::Length() const
  {
    const buffer_handle_type* handle = GetBufferHandle();
    return PRUint32(handle ? handle->DataLength() : 0); 
  }

PRUint32
nsAFlatCString::Length() const
  {
    const buffer_handle_type* handle = GetBufferHandle();
    return PRUint32(handle ? handle->DataLength() : 0); 
  }

const nsAFlatString::char_type* 
nsAFlatString::get() const
  { 
    const buffer_handle_type* handle = GetBufferHandle();
    // NS_ASSERTION(handle, "handle is null!");
    return handle ? handle->DataStart() : 0; 
  }

const nsAFlatCString::char_type* 
nsAFlatCString::get() const
  { 
    const buffer_handle_type* handle = GetBufferHandle();
    // NS_ASSERTION(handle, "handle is null!");
    return handle ? handle->DataStart() : 0; 
  }
