/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef nsIEnumerator_h___
#define nsIEnumerator_h___

#include "nsISupports.h"

// {646F4FB0-B1F2-11d1-AA29-000000000000}
#define NS_IENUMERATOR_IID \
{0x646f4fb0, 0xb1f2, 0x11d1, \
    { 0xaa, 0x29, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }


class nsIEnumerator : public nsISupports {
public:

  static const nsIID& IID() { static nsIID iid = NS_IENUMERATOR_IID; return iid; }

  /** First will reset the list. will return NS_FAILED if no items
   */
  virtual nsresult First()=0;

  /** Last will reset the list to the end. will return NS_FAILED if no items
   */
  virtual nsresult Last()=0;
  
  /** Next will advance the list. will return failed if allready at end
   */
  virtual nsresult Next()=0;

  /** Prev will decrement the list. will return failed if allready at beginning
   */
  virtual nsresult Prev()=0;

  /** CurrentItem will return the CurrentItem item it will fail if the list is empty
   *  @param aItem return value
   */
  virtual nsresult CurrentItem(nsISupports **aItem)=0;

  /** return if the collection is at the end.  that is the beginning following a call to Prev
   *  and it is the end of the list following a call to next
   *  @param aItem return value
   */
  virtual nsresult IsDone()=0;

};

#endif // __nsIEnumerator_h

