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

#ifndef nsIIterator_h___
#define nsIIterator_h___

#include "nsISupports.h"

typedef void * nsComponent ;

//0d985370-eb7c-11d1-9244-00805f8a7ab6
#define NS_IITERATOR_IID   \
{ 0x0d985370, 0xeb7c, 0x11d1,    \
{ 0x92, 0x44, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsIIterator : public nsISupports
{

public:

  NS_IMETHOD                  Init() = 0 ;

  NS_IMETHOD                  First() = 0 ;
  NS_IMETHOD                  Last() = 0 ;
  NS_IMETHOD                  Next() = 0 ;
  NS_IMETHOD                  Previous() = 0 ;
  NS_IMETHOD_(PRBool)         IsDone() = 0 ;
  NS_IMETHOD_(PRBool)         IsFirst() = 0 ;
  NS_IMETHOD_(nsComponent)    CurrentItem() = 0 ;
  NS_IMETHOD_(PRUint32)       Count() = 0 ;

};

#endif /* nsIIterator_h___ */
