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
 * The Original Code is Mozilla strings.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
 *
 */

#ifndef nsBufferHandleUtils_h___
#define nsBufferHandleUtils_h___

#ifndef nsAReadableString_h___
#include "nsAReadableString.h"
  // for |basic_nsAReadableString|...
#endif

#include <new.h>
  // for placement |new|



template <class HandleT, class CharT>
inline
size_t
NS_AlignedHandleSize( const HandleT*, const CharT* )
  {
      // figure out the number of bytes needed the |HandleT| part, including padding to correctly align the data part
    return ((sizeof(HandleT) + sizeof(CharT) - 1) / sizeof(CharT)) * sizeof(CharT);
  }

template <class HandleT, class CharT>
inline
const CharT*
NS_DataAfterHandle( const HandleT* aHandlePtr, const CharT* aDummyCharTPtr )
  {
    typedef const CharT* CharT_ptr;
    return CharT_ptr(NS_STATIC_CAST(const unsigned char*, aHandlePtr) + NS_AlignedHandleSize(aHandlePtr, aDummyCharTPtr));
  }

template <class HandleT, class CharT>
inline
CharT*
NS_DataAfterHandle( HandleT* aHandlePtr, const CharT* aDummyCharTPtr )
  {
    typedef CharT* CharT_ptr;
    return CharT_ptr(NS_STATIC_CAST(unsigned char*, aHandlePtr) + NS_AlignedHandleSize(aHandlePtr, aDummyCharTPtr));
  }

template <class HandleT, class CharT>
HandleT*
NS_AllocateContiguousHandleWithData( const HandleT* aDummyHandlePtr, const basic_nsAReadableString<CharT>& aDataSource, PRUint32 aAdditionalCapacity )
  {
    typedef CharT* CharT_ptr;

      // figure out the number of bytes needed the |HandleT| part, including padding to correctly align the data part
    size_t handle_size    = NS_AlignedHandleSize(aDummyHandlePtr, CharT_ptr(0));

      // figure out how many |CharT|s wee need to fit in the data part
    size_t data_length    = aDataSource.Length();
    size_t buffer_length  = data_length + aAdditionalCapacity;

      // how many bytes is that (including a zero-terminator so we can claim to be flat)?
    size_t buffer_size    = buffer_length * sizeof(CharT);


    HandleT* result = 0;
    void* handle_ptr = ::operator new(handle_size + buffer_size);

    if ( handle_ptr )
      {
        CharT* data_start_ptr = CharT_ptr(NS_STATIC_CAST(unsigned char*, handle_ptr) + handle_size);
        CharT* data_end_ptr   = data_start_ptr + data_length;
        CharT* buffer_end_ptr = data_start_ptr + buffer_length;

        nsReadingIterator<CharT> fromBegin, fromEnd;
        CharT* toBegin = data_start_ptr;
        copy_string(aDataSource.BeginReading(fromBegin), aDataSource.EndReading(fromEnd), toBegin);

          // and if the caller bothered asking for a buffer bigger than their string, we'll zero-terminate
        if ( aAdditionalCapacity > 0 )
          *toBegin = CharT(0);

        result = new (handle_ptr) HandleT(data_start_ptr, data_end_ptr, data_start_ptr, buffer_end_ptr, PR_TRUE);
      }

    return result;
  }

#endif // !defined(nsBufferHandleUtils_h___)
