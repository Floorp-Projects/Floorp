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
#ifndef nsISizeOfHandler_h___
#define nsISizeOfHandler_h___

#include "nscore.h"
#include "nsISupports.h"

/* c028d1f0-fc9e-11d1-89e4-006008911b81 */
#define NS_ISIZEOF_HANDLER_IID \
{ 0xc028d1f0, 0xfc9e, 0x11d1, {0x89, 0xe4, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81}}

/**
 * An API to managing a sizeof computation of an arbitrary graph.
 * The handler is responsible for remembering which objects have been
 * seen before. Note that the handler doesn't hold references to
 * nsISupport's objects; the assumption is that the objects being
 * sized are stationary and will not be modified during the sizing
 * computation and therefore do not need an extra reference count.
 */
class nsISizeOfHandler : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISIZEOF_HANDLER_IID)

  /**
   * Add in a simple size value to the running total.
   * Always returns NS_OK.
   */
  NS_IMETHOD Add(size_t aSize) = 0;

  /**
   * Update aResult with PR_TRUE if the object has been traversed
   * by the sizeof computation before. Otherwise aResult is set to
   * PR_FALSE and the object is added to the internal database
   * of objects that have been traversed. It's ok to pass a null
   * pointer in; aResult will be set to PR_TRUE so you won't accidently
   * try to traverse through null pointer.
   *
   * Note: This violates the COM API standard on purpose; so there!
   */
  virtual PRBool HaveSeen(void* anObject) = 0;

  /**
   * Return the currently computed size.
   * Always returns NS_OK.
   */
  NS_IMETHOD GetSize(PRUint32& aResult) = 0;
};

extern NS_COM nsresult
NS_NewSizeOfHandler(nsISizeOfHandler** aInstancePtrResult);

#endif /* nsISizeofHandler_h___ */
