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

#include "nsSharableString.h"
// #include "nsBufferHandleUtils.h"

void
nsSharableString::assign( const abstract_string_type& aReadable )
  {
    const shared_buffer_handle_type* handle = aReadable.GetSharedBufferHandle();
    if ( !handle )
      handle = NS_AllocateContiguousHandleWithData(handle, aReadable, PRUint32(1));
    mBuffer = handle;
  }

const nsSharableString::shared_buffer_handle_type*
nsSharableString::GetSharedBufferHandle() const
  {
    return mBuffer.get();
  }

void
nsSharableCString::assign( const abstract_string_type& aReadable )
  {
    const shared_buffer_handle_type* handle = aReadable.GetSharedBufferHandle();
    if ( !handle )
      handle = NS_AllocateContiguousHandleWithData(handle, aReadable, PRUint32(1));
    mBuffer = handle;
  }

const nsSharableCString::shared_buffer_handle_type*
nsSharableCString::GetSharedBufferHandle() const
  {
    return mBuffer.get();
  }

