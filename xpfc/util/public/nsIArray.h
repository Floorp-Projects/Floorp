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

#ifndef nsIArray_h___
#define nsIArray_h___

#include "nsISupports.h"
#include "nsIIterator.h"

//9d149d10-eb7f-11d1-9244-00805f8a7ab6
#define NS_IARRAY_IID   \
{ 0x9d149d10, 0xeb7c, 0x11d1,    \
{ 0x92, 0x44, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }


typedef PRInt32 (*nsArrayCompareProc)(const nsComponent elem1, const nsComponent elem2 );

class nsIArray : public nsISupports
{

public:

  NS_IMETHOD                  Init() = 0 ;

  NS_IMETHOD_(PRUint32)       Count() = 0 ;
  NS_IMETHOD_(PRBool)         Empty() = 0 ;
  NS_IMETHOD_(PRBool)         Contains(nsComponent aComponent) = 0;
  NS_IMETHOD_(PRUint32)       IndexOf(nsComponent aComponent) = 0;
  NS_IMETHOD_(PRInt32)        InsertBinary(nsComponent aComponent, nsArrayCompareProc aCompFn, PRBool bAllowDups) = 0;
  NS_IMETHOD_(nsComponent)    ElementAt(PRUint32 aIndex) = 0 ;

  NS_IMETHOD                  Insert(PRUint32 aIndex, nsComponent aComponent) = 0 ;
  NS_IMETHOD                  Append(nsComponent aComponent) = 0 ;
  NS_IMETHOD                  Remove(nsComponent aComponent) = 0 ;
  NS_IMETHOD                  RemoveAll() = 0;
  NS_IMETHOD                  RemoveAt(PRUint32 aIndex) = 0 ;

  NS_IMETHOD                  CreateIterator(nsIIterator ** aIterator) = 0 ;

};

#endif /* nsIArray_h___ */
