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

#include "nsPrivateSharableString.h"

//-------1---------2---------3---------4---------5---------6---------7---------8


const nsSharedBufferHandle<PRUnichar>*
nsPrivateSharableString::GetSharedBufferHandle() const
  {
    return 0;
  }

const nsBufferHandle<PRUnichar>*
nsPrivateSharableString::GetFlatBufferHandle() const
  {
    return GetSharedBufferHandle();
  }

const nsBufferHandle<PRUnichar>*
nsPrivateSharableString::GetBufferHandle() const
  {
    return GetSharedBufferHandle();
  }

PRUint32
nsPrivateSharableString::GetImplementationFlags() const
  {
    PRUint32 flags = 0;
    const nsSharedBufferHandle<char_type>* handle = GetSharedBufferHandle();
    if ( handle )
      flags = handle->GetImplementationFlags();
    return flags;
  }



const nsSharedBufferHandle<char>*
nsPrivateSharableCString::GetSharedBufferHandle() const
  {
    return 0;
  }

const nsBufferHandle<char>*
nsPrivateSharableCString::GetFlatBufferHandle() const
  {
    return GetSharedBufferHandle();
  }

const nsBufferHandle<char>*
nsPrivateSharableCString::GetBufferHandle() const
  {
    return GetSharedBufferHandle();
  }

PRUint32
nsPrivateSharableCString::GetImplementationFlags() const
  {
    PRUint32 flags = 0;
    const nsSharedBufferHandle<char_type>* handle = GetSharedBufferHandle();
    if ( handle )
      flags = handle->GetImplementationFlags();
    return flags;
  }
