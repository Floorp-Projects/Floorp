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

const PRUnichar*
nsAFlatString::GetReadableFragment( nsReadableFragment<PRUnichar>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          {
            const nsBufferHandle<PRUnichar>* buffer = GetBufferHandle();
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

PRUnichar*
nsAFlatString::GetWritableFragment( nsWritableFragment<PRUnichar>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset )
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          {
            nsBufferHandle<PRUnichar>* buffer = NS_CONST_CAST(nsBufferHandle<PRUnichar>*, GetBufferHandle());
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

const char*
nsAFlatCString::GetReadableFragment( nsReadableFragment<char>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          {
            const nsBufferHandle<char>* buffer = GetBufferHandle();
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

char*
nsAFlatCString::GetWritableFragment( nsWritableFragment<char>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset )
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          {
            nsBufferHandle<char>* buffer = NS_CONST_CAST(nsBufferHandle<char>*, GetBufferHandle());
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
