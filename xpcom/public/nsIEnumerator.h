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

#if defined(XPIDL_JS_STUBS)
struct JSObject;
struct JSContext;
#endif

#define NS_IENUMERATOR_IID                           \
{ /* ad385286-cbc4-11d2-8cca-0060b0fc14a3 */         \
    0xad385286,                                      \
    0xcbc4,                                          \
    0x11d2,                                          \
    {0x8c, 0xca, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

class nsIEnumerator : public nsISupports {
public:

  static const nsIID& GetIID(void) { static nsIID iid = NS_IENUMERATOR_IID; return iid; }

  /** First will reset the list. will return NS_FAILED if no items
   */
  NS_IMETHOD First(void) = 0;
  
  /** Next will advance the list. will return failed if already at end
   */
  NS_IMETHOD Next(void) = 0;

  /** CurrentItem will return the CurrentItem item it will fail if the list is empty
   *  @param aItem return value
   */
  NS_IMETHOD CurrentItem(nsISupports **aItem) = 0;

  /** return if the collection is at the end.  that is the beginning following a call to Prev
   *  and it is the end of the list following a call to next
   *  @param aItem return value
   */
  NS_IMETHOD IsDone(void) = 0;

#if defined(XPIDL_JS_STUBS)
  // XXX Scriptability hack...
  static NS_EXPORT_(JSObject*) InitJSClass(JSContext* cx) {
    NS_NOTYETIMPLEMENTED("nsIEnumerator isn't XPIDL scriptable yet");
    return 0;
  }

  static NS_EXPORT_(JSObject*) GetJSObject(JSContext* cx, nsIEnumerator* priv) {
    NS_NOTYETIMPLEMENTED("nsIEnumerator isn't XPIDL scriptable yet");
    return 0;
  }
#endif
};

#define NS_IBIDIRECTIONALENUMERATOR_IID              \
{ /* 75f158a0-cadd-11d2-8cca-0060b0fc14a3 */         \
    0x75f158a0,                                      \
    0xcadd,                                          \
    0x11d2,                                          \
    {0x8c, 0xca, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

class nsIBidirectionalEnumerator : public nsIEnumerator {
public:

  static const nsIID& GetIID(void) { static nsIID iid = NS_IBIDIRECTIONALENUMERATOR_IID; return iid; }

  /** Last will reset the list to the end. will return NS_FAILED if no items
   */
  NS_IMETHOD Last(void) = 0;

  /** Prev will decrement the list. will return failed if already at beginning
   */
  NS_IMETHOD Prev(void) = 0;

};

// Construct and return an implementation of a "conjoining enumerator." This
// enumerator lets you string together two other enumerators into one sequence.
// The result is an nsIBidirectionalEnumerator, but if either input is not
// also bidirectional, the Last and Prev operations will fail.
extern "C" NS_COM nsresult
NS_NewConjoiningEnumerator(nsIEnumerator* first, nsIEnumerator* second,
                           nsIBidirectionalEnumerator* *aInstancePtrResult);

// Construct and return an implementation of a "union enumerator." This
// enumerator will only return elements that are found in both constituent
// enumerators.
extern "C" NS_COM nsresult
NS_NewUnionEnumerator(nsIEnumerator* first, nsIEnumerator* second,
                      nsIEnumerator* *aInstancePtrResult);

// Construct and return an implementation of an "intersection enumerator." This
// enumerator will return elements that are found in either constituent
// enumerators, eliminating duplicates.
extern "C" NS_COM nsresult
NS_NewIntersectionEnumerator(nsIEnumerator* first, nsIEnumerator* second,
                             nsIEnumerator* *aInstancePtrResult);

#endif // __nsIEnumerator_h

