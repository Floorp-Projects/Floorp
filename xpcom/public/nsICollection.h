/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specifzic language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsICollection_h___
#define nsICollection_h___

#include "nsISupports.h"

// IID for the nsICollection interface
#define NS_ICOLLECTION_IID      \
{ 0xaeca730, 0x835b, 0x11d2, \
  { 0x8f, 0x30, 0x0, 0x60, 0x8, 0x31, 0x1, 0x94 } }


// IID for the nsICollection Factort interface
#define NS_ICOLLECTIONFACTORY_IID      \
{ 0xf8052641, 0x8768, 0x11d2, \
  { 0x8f, 0x39, 0x0, 0x60, 0x8, 0x31, 0x1, 0x94 } }

//----------------------------------------------------------------------

/** nsICollection Interface
 *  this may be ordered or not. a list or array, the implementation is opaque
 */
class nsICollection : public nsISupports {
public:

  static const nsIID& IID() { static nsIID iid = NS_ICOLLECTION_IID; return iid; }

  /** AddItem will take an ISupports and keep track of it 
   *  @param aItem is the Item to be added WILL BE ADDREFFED
   */
  virtual nsresult AddItem(nsISupports *aItem)=0;

  /** RemoveItem will take an nsISupports and remove it from the collection
   *  @param aItem is the item to be removed  WILL BE RELEASED
   */
  virtual nsresult RemoveItem(nsISupports *aItem)=0;

  /** Clear will clear all items from list
   */
  virtual nsresult Clear()=0;
};


#endif /* nsICollection_h___ */

