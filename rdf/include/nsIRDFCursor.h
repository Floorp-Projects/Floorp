/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIRDFCursor_h__
#define nsIRDFCursor_h__

#include "nsISupports.h"
class nsIRDFNode;

/**
 * A simple cursor that enumerates nodes
 */
class nsIRDFCursor : public nsISupports {
public:
  /**
   * Determine whether the cursor has more elements.
   * @return NS_OK unless a catastrophic error occurs.
   */
  NS_IMETHOD HasMoreElements(PRBool* result /* out */) = 0;

  /**
   * Fetch then next element, and possibly the truth value of the arc
   * that leads to the element.
   *
   * @param next A pointer to receive the next <tt>nsIRDFNode</tt> object.
   * @param tv A pointer to a boolean variable to receive the truth value
   * of the arc that leads to the element. You pass <tt>nsnull</tt> if 
   * you don't care to know.
   * @return NS_ERROR_UNEXPECTED if the cursor is empty; otherwise, NS_OK
   * unless a catastrophic error occurs.
   */
  NS_IMETHOD GetNext(nsIRDFNode** next /* out */,
                     PRBool* tv /* out */) = 0;
};

// 1c2abdb0-4cef-11d2-bc16-00805f912fe7
#define NS_IRDFCURSOR_IID \
{ 0x1c2abdb0, 0x4cef, 0x11d2, { 0xbc, 0x16, 0x00, 0x80, 0x5f, 0x91, 0x2f, 0xe7 } }

#endif /* nsIRDFCursor_h__ */
