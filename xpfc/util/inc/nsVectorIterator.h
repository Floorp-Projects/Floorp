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

#ifndef nsVectorIterator_h___
#define nsVectorIterator_h___

#include "nsIIterator.h"
#include "nsIVector.h"

class nsVectorIterator : public nsIIterator
{
public:
  nsVectorIterator();

  NS_DECL_ISUPPORTS

  NS_IMETHOD                  Init() ;
  NS_IMETHOD                  Init(nsIVector * aVector) ;

  NS_IMETHOD                  First() ;
  NS_IMETHOD                  Last() ;
  NS_IMETHOD                  Next() ;
  NS_IMETHOD                  Previous() ;
  NS_IMETHOD_(PRBool)         IsDone() ;
  NS_IMETHOD_(PRBool)         IsFirst() ;
  NS_IMETHOD_(nsComponent)    CurrentItem() ;
  NS_IMETHOD_(PRUint32)       Count() ;

protected:
  ~nsVectorIterator();

private:
  nsIVector * mVector;
  PRUint32  mCurrentElement;

};

#endif /* nsVectorIterator_h___ */
