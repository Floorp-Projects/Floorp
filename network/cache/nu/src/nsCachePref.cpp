/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsCachePref.h"

static const PRUint32 MEM_CACHE_SIZE_DEFAULT = 1024*1024;
static const PRUint32 DISK_CACHE_SIZE_DEFAULT = 5*MEM_CACHE_SIZE_DEFAULT;

nsCachePref::nsCachePref()
{
}

nsCachePref::~nsCachePref()
{
}

PRUint32 nsCachePref::MemCacheSize() const
{
    return MEM_CACHE_SIZE_DEFAULT;
}

PRUint32 nsCachePref::DiskCacheSize() const
{
    return DISK_CACHE_SIZE_DEFAULT;
}

/*
nsrefcnt nsCachePref::AddRef(void)
{
    return ++m_RefCnt;
}
nsrefcnt nsCachePref::Release(void)
{
    if (--m_RefCnt == 0)
    {
        delete this;
        return 0;
    }
    return m_RefCnt;
}

nsresult nsCachePref::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
    return NS_OK;
}
*/

