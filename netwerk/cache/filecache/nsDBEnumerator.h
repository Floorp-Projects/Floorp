/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator.
 * 
 * The Initial Developer of the Original Code is Intel Corp.
 * Portions created by Intel Corp. are
 * Copyright (C) 1999, 1999 Intel Corp.  All
 * Rights Reserved.
 * 
 * Contributor(s): Yixiong Zou <yixiong.zou@intel.com>
 *                 Carl Wong <carl.wong@intel.com>
 */

/*
 * This file is part of filecache implementation.
 *
 * It implements a simple iterator for the database, see nsDBAccessor. 
 */

#ifndef _NS_DBENUMERATOR_H_
#define _NS_DBENUMERATOR_H_

#include "nsISimpleEnumerator.h"
#include "nsINetDataCacheRecord.h"
#include "nsIDBAccessor.h"
#include "nsCOMPtr.h"
#include "nsNetDiskCache.h"
#include "nsDiskCacheRecord.h"

class nsCachedDiskData ; /* forward decl */

class nsDBEnumerator : public nsISimpleEnumerator {
public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSISIMPLEENUMERATOR 

    nsDBEnumerator(nsIDBAccessor* aDB, nsNetDiskCache* aCache) ;
    virtual ~nsDBEnumerator() ;

private:
    nsCOMPtr<nsIDBAccessor>                m_DB ;
    nsNetDiskCache*                        m_DiskCache ;
    void *                                 m_tempEntry ;
    PRUint32                               m_tempEntry_length ;
    nsDiskCacheRecord*                     m_CacheEntry ;
    PRBool                                 m_bReset ;
};

#endif // _NS_DBENUMERATOR_H_
