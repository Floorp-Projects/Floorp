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

#include "nscore.h"
#include "nsIIterator.h"
#include "nsxpfcCIID.h"
#include "nsIArray.h"
#include "nsArrayIterator.h"
#include "nsxpfcutil.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCVectorIteratorCID, NS_ARRAY_ITERATOR_CID);

nsArrayIterator :: nsArrayIterator()
{
  NS_INIT_REFCNT();
  mCurrentElement = 0 ;
}

nsArrayIterator :: ~nsArrayIterator()
{
}

NS_IMPL_QUERY_INTERFACE(nsArrayIterator, kCVectorIteratorCID)
NS_IMPL_ADDREF(nsArrayIterator)
NS_IMPL_RELEASE(nsArrayIterator)

nsresult nsArrayIterator :: Init()
{
  mCurrentElement = 0 ;
  return NS_OK ;
}

nsresult nsArrayIterator :: Init(nsIArray * aVector)
{
  mVector = aVector;
  mCurrentElement = 0 ;
  return NS_OK ;
}

nsresult nsArrayIterator :: First()
{
  mCurrentElement = 0 ;
  return NS_OK ;
}

nsresult nsArrayIterator :: Last()
{
  mCurrentElement = Count() - 1 ;
  return NS_OK ;
}

nsresult nsArrayIterator :: Next()
{
  mCurrentElement++;
  return NS_OK ;
}

nsresult nsArrayIterator :: Previous()
{
  mCurrentElement--;
  return NS_OK ;
}

PRBool nsArrayIterator :: IsDone()
{
  if (mVector == nsnull)
    return PR_TRUE;

  if ((mVector->Count()) == (mCurrentElement))
    return PR_TRUE;

  return PR_FALSE ;
}

PRBool nsArrayIterator :: IsFirst()
{
  if (mVector == nsnull)
    return PR_TRUE;

  if (0 == (mCurrentElement))
    return PR_TRUE;

  return PR_FALSE ;
}

nsComponent nsArrayIterator :: CurrentItem()
{
  if (IsDone())
    return nsnull ;
    
  return (mVector->ElementAt(mCurrentElement));     
}

PRUint32 nsArrayIterator :: Count()
{
  return (mVector->Count());     
}








