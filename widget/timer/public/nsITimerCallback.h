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
#ifndef nsITimerCallback_h___
#define nsITimerCallback_h___

#include "nscore.h"
#include "nsISupports.h"

class nsITimer;

/// Interface IID for nsITimerCallback
#define NS_ITIMERCALLBACK_IID         \
{ 0x5079b3a0, 0xb743, 0x11d1,         \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

/**
 * Interface implemented by users of the nsITimer class. An instance
 * of this interface is passed in when creating a timer. The Notify()
 * method of that instance is invoked after the specified delay.
 */
class nsITimerCallback : public nsISupports {
public:  
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITIMERCALLBACK_IID)

  virtual void Notify(nsITimer *timer) = 0;
};

#endif
