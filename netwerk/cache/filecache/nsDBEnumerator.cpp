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

#include "nsDBEnumerator.h"
#include "nsDiskCacheRecord.h"

nsDBEnumerator::nsDBEnumerator(nsIDBAccessor* aDB, nsNetDiskCache* aCache) :
  m_DB(aDB) ,
  m_DiskCache(aCache) ,
  m_tempEntry(0) ,
  m_tempEntry_length(0) ,
  m_CacheEntry(0) ,
  m_bReset(PR_TRUE) 
{
  NS_INIT_REFCNT();

}

nsDBEnumerator::~nsDBEnumerator()
{
  NS_IF_RELEASE(m_CacheEntry) ;
}

//
// Implement nsISupports methods
//
NS_IMPL_ISUPPORTS(nsDBEnumerator, NS_GET_IID(nsISimpleEnumerator))

/////////////////////////////////////////////////////////////////
// nsISimpleEnumerator methods

NS_IMETHODIMP
nsDBEnumerator::HasMoreElements(PRBool *_retval)
{
  *_retval = PR_FALSE ;

  nsresult rv = m_DB->EnumEntry(&m_tempEntry, &m_tempEntry_length, m_bReset) ;

  if(NS_FAILED(rv)) {
    // do some error recovery
    m_DiskCache->DBRecovery() ;
    return rv ;
  }

  m_bReset = PR_FALSE ;

  if(m_tempEntry && m_tempEntry_length != 0)
    *_retval = PR_TRUE ;

  return NS_OK ;
}

// this routine does not create a new item by itself  
// Rather it reuses the item inside the object. So if you need to use the 
// item later, you have to
// create a new item specifically, using copy constructor or some other dup 
// function. And don't forget to release it after you're done
//
NS_IMETHODIMP
nsDBEnumerator::GetNext(nsISupports **_retval)
{
  if(!m_CacheEntry) {
    m_CacheEntry = new nsDiskCacheRecord(m_DB, m_DiskCache) ;
    if(m_CacheEntry)
      NS_ADDREF(m_CacheEntry) ;
    else
      return NS_ERROR_OUT_OF_MEMORY ;
  }

  if(!_retval)
    return NS_ERROR_NULL_POINTER ;
  *_retval = nsnull ;

  nsresult rv = m_CacheEntry->RetrieveInfo(m_tempEntry, m_tempEntry_length) ;
  if(NS_FAILED(rv)) {
    // do some error recovery
    m_DiskCache->DBRecovery() ;
    return rv ;
  }

  *_retval = NS_STATIC_CAST(nsISupports*, m_CacheEntry) ;
  NS_ADDREF(*_retval) ; // all good getter addref 

  return NS_OK ;
}
