/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsArrayIterator_h___
#define nsArrayIterator_h___

#include "nsIIterator.h"
#include "nsIArray.h"

class nsArrayIterator : public nsIIterator
{
public:
  nsArrayIterator();

  NS_DECL_ISUPPORTS

  NS_IMETHOD                  Init() ;
  NS_IMETHOD                  Init(nsIArray * aVector) ;

  NS_IMETHOD                  First() ;
  NS_IMETHOD                  Last() ;
  NS_IMETHOD                  Next() ;
  NS_IMETHOD                  Previous() ;
  NS_IMETHOD_(PRBool)         IsDone() ;
  NS_IMETHOD_(PRBool)         IsFirst() ;
  NS_IMETHOD_(nsComponent)    CurrentItem() ;
  NS_IMETHOD_(PRUint32)       Count() ;

protected:
  ~nsArrayIterator();

private:
  nsIArray * mVector;
  PRUint32  mCurrentElement;

};

#endif /* nsArrayIterator_h___ */
