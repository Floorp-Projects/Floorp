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

#ifndef _NS_IDBACCESSOR_H_
#define _NS_IDBACCESSOR_H_

#include "nsISupports.h"
#include "nsIFile.h"

// nsIDBAccessorIID {6AADD4D0-7785-11d3-87FE-000629D01344}
#define NS_IDBACCESSOR_IID \
{ 0x6aadd4d0, 0x7785, 0x11d3, \
 {0x87, 0xfe, 0x0, 0x6, 0x29, 0xd0, 0x13, 0x44}}

// nsDBAccessorCID {6AADD4D1-7785-11d3-87FE-000629D01344}
#define NS_DBACCESSOR_CID \
{ 0x6aadd4d1, 0x7785, 0x11d3, \
  { 0x87, 0xfe, 0x0, 0x6, 0x29, 0xd0, 0x13, 0x44 }} 

class nsIDBAccessor : public nsISupports
{
  public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDBACCESSOR_IID)

  NS_IMETHOD Init(nsIFile* DBFile) = 0 ;
  NS_IMETHOD Shutdown(void) = 0 ;

  NS_IMETHOD Put(PRInt32 aID, void* anEntry, PRUint32 aLength) = 0 ;

  NS_IMETHOD Get(PRInt32 aID, void** anEntry, PRUint32 *aLength) = 0 ;

  NS_IMETHOD Del(PRInt32 aID, void* anEntry, PRUint32 aLength) = 0 ;

  NS_IMETHOD GetID(const char* key, PRUint32 length, PRInt32* aID) = 0 ;

  NS_IMETHOD EnumEntry(void* *anEntry, PRUint32* aLength, PRBool bReset) = 0 ;
  
  NS_IMETHOD GetDBFilesize(PRUint64* aSize) = 0 ;
    
  NS_IMETHOD GetSizeEntry(void** anEntry, PRUint32 *aLength) = 0 ;
  NS_IMETHOD SetSizeEntry(void* anEntry, PRUint32 aLength) = 0 ;

} ;

#endif // _NS_IDBACCESSOR_H_

