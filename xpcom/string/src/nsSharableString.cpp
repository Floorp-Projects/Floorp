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

#include "nsCommonString.h"
// #include "nsBufferHandleUtils.h"

void
nsCommonString::assign( const string_type& aReadable )
  {
    const nsSharedBufferHandle<char_type>* handle = aReadable.GetSharedBufferHandle();
    if ( !handle )
      handle = NS_AllocateContiguousHandleWithData(handle, aReadable, PRUint32(1));
    mBuffer = handle;
  }

const nsSharedBufferHandle<PRUnichar>*
nsCommonString::GetSharedBufferHandle() const
  {
    return mBuffer.get();
  }

void
nsCommonCString::assign( const string_type& aReadable )
  {
    const nsSharedBufferHandle<char_type>* handle = aReadable.GetSharedBufferHandle();
    if ( !handle )
      handle = NS_AllocateContiguousHandleWithData(handle, aReadable, PRUint32(1));
    mBuffer = handle;
  }

const nsSharedBufferHandle<char>*
nsCommonCString::GetSharedBufferHandle() const
  {
    return mBuffer.get();
  }

