/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef ilISystemServices_h___
#define ilISystemServices_h___

#include <stdio.h>
#include "nsISupports.h"

// IID for the ilISystemServices interface
#define IL_ISYSTEMSERVICES_IID    \
{ 0xc14659e0, 0xb9fe, 0x11d1,     \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

typedef void (*ilTimeoutCallbackFunction) (void * closure);

class ilISystemServices : public nsISupports {
public:

  virtual void * SetTimeout(ilTimeoutCallbackFunction aFunc, 
			  void * aClosure, PRUint32 aMsecs)=0;

  virtual void ClearTimeout(void *aTimerID)=0;
};

#endif
